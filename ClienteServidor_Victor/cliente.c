/*
 * cliente.c - Cliente da Plataforma de Monitoramento de Filas.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Execucao:  ./cliente   (sem parametros)
 *
 * O endereco e a porta do servidor saem das constantes SERVER_IP e
 * SERVER_PORTA, definidas em comum.h, sem precisar de argumento na linha de
 * comando.
 *
 * O cliente usa duas threads porque precisa fazer duas coisas ao mesmo
 * tempo: esperar o operador digitar uma opcao e receber as mensagens que o
 * servidor manda sozinho (o broadcast, quando outro cliente cadastra um
 * usuario). Com um fluxo so, o programa ficaria parado no teclado e o aviso
 * so apareceria depois - o enunciado pede atualizacao em tempo real.
 *
 *   - thread receptora: unica que le do socket. Imprime o que chega e avisa
 *     a thread principal quando a resposta de um comando terminou;
 *   - thread principal: le o teclado, manda os comandos e espera a resposta,
 *     com tempo limite.
 *
 * Retransmissao: a resposta do servidor serve de confirmacao do comando. Se
 * ela nao chegar em TIMEOUT_RESPOSTA_SEG segundos, o cliente manda de novo o
 * MESMO comando com o MESMO numero de sequencia, ate MAX_TENTATIVAS vezes. E
 * esse numero que permite ao servidor perceber o reenvio e repetir a
 * resposta em vez de executar duas vezes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comum.h"
#include "protocolo.h"

/* Qual comando esta esperando resposta. */
#define AGUARDA_NADA      0
#define AGUARDA_ADD       1
#define AGUARDA_LIST      2
#define AGUARDA_HEARTBEAT 3

/* Ligacao entre a thread receptora e a thread principal. */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  condicao;

    int aguardando;          /* comando que espera resposta (AGUARDA_*)   */
    int resposta_completa;   /* 1 quando a resposta terminou de chegar    */
    int conexao_ativa;       /* 0 quando o servidor fecha ou da erro      */
    int dentro_do_bloco;     /* 1 entre o cabecalho e o rodape da fila    */
} Canal;

static Canal  canal;
static int    socket_servidor = -1;

/*
 * Avisa a thread principal que a resposta chegou.
 * Chamar com o mutex do canal ja travado.
 */
static void marca_resposta_completa(void)
{
    canal.resposta_completa = 1;
    canal.aguardando        = AGUARDA_NADA;
    pthread_cond_signal(&canal.condicao);
}

/*
 * Imprime a linha recebida conforme o comando que a pediu, no formato das
 * telas do enunciado.
 */
static void imprime_resposta(int comando, const char *linha, int primeira_linha)
{
    if (strncmp(linha, RESP_ADD_OK, strlen(RESP_ADD_OK)) == 0) {
        /* "ADD_OK <id> <nome>" vira "Usuário <nome> adicionado." */
        const char *resto = linha + strlen(RESP_ADD_OK);
        int         id    = 0;
        int         deslocamento = 0;

        if (sscanf(resto, "%d %n", &id, &deslocamento) == 1 && deslocamento > 0) {
            printf("Usuário %s adicionado.\n", resto + deslocamento);
        } else {
            printf("Usuário adicionado.\n");
        }
        return;
    }

    if (strcmp(linha, RESP_ALIVE) == 0) {
        printf("Heartbeat: %s\n", RESP_ALIVE);
        return;
    }

    if (strncmp(linha, RESP_ERRO, strlen(RESP_ERRO)) == 0) {
        printf("Servidor: %s\n", linha);
        return;
    }

    if (strcmp(linha, FILA_CABECALHO) == 0) {
        /* No heartbeat com novidade, anuncia antes do bloco. */
        if (comando == AGUARDA_HEARTBEAT && primeira_linha) {
            printf("Heartbeat:\n");
        } else {
            printf("\n");
        }
        printf("%s\n", FILA_CABECALHO);
        return;
    }

    if (strcmp(linha, FILA_RODAPE) == 0) {
        printf("%s\n", FILA_RODAPE);
        return;
    }

    /* Linha "id - nome" do bloco, ou alguma resposta nao prevista. */
    printf("%s\n", linha);
}

/*
 * Le o socket ate a conexao acabar. E a unica funcao do cliente que chama
 * recv(): deixando a leitura num lugar so, duas threads nunca disputam os
 * mesmos bytes.
 */
static void *thread_receptora(void *argumento)
{
    LeitorLinha leitor;
    char        linha[TAM_LINHA];

    (void) argumento;
    protocolo_leitor_init(&leitor, socket_servidor);

    for (;;) {
        int resultado = protocolo_le_linha(&leitor, linha, sizeof(linha));

        if (resultado != LINHA_OK) {
            pthread_mutex_lock(&canal.mutex);
            canal.conexao_ativa = 0;
            pthread_cond_signal(&canal.condicao);
            pthread_mutex_unlock(&canal.mutex);
            break;
        }

        /* Broadcast: nao pertence a resposta nenhuma e aparece na hora, mesmo
         * no meio do menu. Vem sem acento (ASCII puro, ver comum.h) e e
         * reescrito aqui em portugues correto so para exibir. */
        if (strncmp(linha, BROADCAST_NOVO_USUARIO,
                    strlen(BROADCAST_NOVO_USUARIO)) == 0) {
            printf("[Broadcast] Novo usuário: %s\n",
                   linha + strlen(BROADCAST_NOVO_USUARIO));
            fflush(stdout);
            continue;
        }
        if (strncmp(linha, PREFIXO_BROADCAST, strlen(PREFIXO_BROADCAST)) == 0) {
            printf("%s\n", linha);
            fflush(stdout);
            continue;
        }

        pthread_mutex_lock(&canal.mutex);

        /* Regra unica: so mostro uma resposta se algum comando estiver
         * esperando por ela. Se nao houver, e a copia atrasada de um comando
         * que ja foi reenviado e abandonado, e ela e ignorada para nao ser
         * confundida com a resposta do proximo comando. */
        if (!canal.dentro_do_bloco) {
            if (strcmp(linha, FILA_CABECALHO) == 0) {
                canal.dentro_do_bloco = 1;
                if (canal.aguardando != AGUARDA_NADA) {
                    imprime_resposta(canal.aguardando, linha, 1);
                }
            } else if (canal.aguardando != AGUARDA_NADA) {
                imprime_resposta(canal.aguardando, linha, 1);
                marca_resposta_completa();
            }
        } else {
            if (canal.aguardando != AGUARDA_NADA) {
                imprime_resposta(canal.aguardando, linha, 0);
            }
            if (strcmp(linha, FILA_RODAPE) == 0) {
                canal.dentro_do_bloco = 0;
                if (canal.aguardando != AGUARDA_NADA) {
                    marca_resposta_completa();
                }
            }
        }

        fflush(stdout);
        pthread_mutex_unlock(&canal.mutex);
    }

    return NULL;
}

/*
 * Manda um comando e espera a resposta.
 *
 * comando_texto - a linha ja montada (ex.: "ADD 3 20 Carlos")
 * tipo          - AGUARDA_ADD, AGUARDA_LIST ou AGUARDA_HEARTBEAT
 *
 * Reenvia o mesmo texto (logo, o mesmo numero de sequencia) enquanto a
 * resposta nao chegar no tempo, ate MAX_TENTATIVAS vezes.
 *
 * Devolve 1 se a resposta chegou, 0 se o servidor nao respondeu e -1 se a
 * conexao caiu.
 */
static int envia_comando(const char *comando_texto, int tipo)
{
    int envios = 0;      /* quantas vezes o comando foi para a rede */
    int obtida = 0;
    int caiu   = 0;

    pthread_mutex_lock(&canal.mutex);
    canal.aguardando        = tipo;
    canal.resposta_completa = 0;
    pthread_mutex_unlock(&canal.mutex);

    while (envios < MAX_TENTATIVAS && !obtida && !caiu) {

        if (envios > 0) {
            printf("Sem resposta do servidor. Reenviando (tentativa %d de %d)...\n",
                   envios + 1, MAX_TENTATIVAS);
            fflush(stdout);
        }
        envios++;

        if (protocolo_envia_linha(socket_servidor, comando_texto) != 0) {
            caiu = 1;
            break;
        }

        {
            struct timespec limite;

            /* Hora de desistir de esperar. O pthread_cond_timedwait quer um
             * instante absoluto contado da mesma origem que o time() usa,
             * entao basta somar o limite ao horario de agora. */
            limite.tv_sec  = time(NULL) + TIMEOUT_RESPOSTA_SEG;
            limite.tv_nsec = 0;

            pthread_mutex_lock(&canal.mutex);
            while (!canal.resposta_completa && canal.conexao_ativa) {
                if (pthread_cond_timedwait(&canal.condicao, &canal.mutex,
                                           &limite) == ETIMEDOUT) {
                    break;
                }
            }
            obtida = canal.resposta_completa;
            caiu   = !canal.conexao_ativa;
            pthread_mutex_unlock(&canal.mutex);
        }
    }

    /* Para de esperar resposta: daqui pra frente, copia atrasada que chegar
     * sera ignorada pela thread receptora. */
    pthread_mutex_lock(&canal.mutex);
    canal.aguardando = AGUARDA_NADA;
    pthread_mutex_unlock(&canal.mutex);

    if (caiu) {
        return -1;
    }
    return obtida ? 1 : 0;
}

/*
 * Le uma linha do teclado, ja sem o '\n'.
 * Devolve 1 se leu e 0 se a entrada acabou (Ctrl+D).
 */
static int le_teclado(char *destino, size_t tam)
{
    if (fgets(destino, (int) tam, stdin) == NULL) {
        return 0;
    }
    protocolo_limpa_bordas(destino);
    return 1;
}

/*
 * Confere se o nome pode ser enviado: nao vazio e sem caracteres de
 * controle, que quebrariam a divisao das mensagens por linha.
 */
static int nome_valido(const char *nome)
{
    int i;

    if (nome[0] == '\0') {
        return 0;
    }
    for (i = 0; nome[i] != '\0'; i++) {
        unsigned char c = (unsigned char) nome[i];
        if (c < 32 || c == 127) {
            return 0;
        }
    }
    return 1;
}

/*
 * Opcao 1: pede o identificador e o nome, e manda o comando ADD.
 * Devolve 0 para seguir no menu e -1 se a conexao caiu.
 */
static int opcao_adicionar_usuario(int sequencia)
{
    char entrada[TAM_LINHA];
    char comando[TAM_LINHA];
    char nome[TAM_NOME];
    int  id = 0;

    printf("ID: ");
    fflush(stdout);
    if (!le_teclado(entrada, sizeof(entrada))) {
        return -1;
    }
    if (sscanf(entrada, "%d", &id) != 1 || id <= 0) {
        printf("ID inválido. Informe um número inteiro positivo.\n");
        return 0;
    }

    printf("Nome: ");
    fflush(stdout);
    if (!le_teclado(entrada, sizeof(entrada))) {
        return -1;
    }
    if (!nome_valido(entrada)) {
        printf("Nome inválido. O nome não pode ser vazio.\n");
        return 0;
    }
    if (strlen(entrada) >= TAM_NOME) {
        printf("Nome muito longo. Use até %d caracteres.\n", TAM_NOME - 1);
        return 0;
    }

    /* Copia para um buffer do tamanho do campo; o tamanho ja foi conferido
     * acima, entao cabe com certeza. */
    strcpy(nome, entrada);

    snprintf(comando, sizeof(comando), "%s %d %d %s",
             CMD_ADD, sequencia, id, nome);

    switch (envia_comando(comando, AGUARDA_ADD)) {
        case 1:  return 0;
        case 0:  printf("Servidor não respondeu, tente novamente.\n"); return 0;
        default: return -1;
    }
}

/* Opcao 2: pede a fila completa. */
static int opcao_ver_fila(int sequencia)
{
    char comando[TAM_LINHA];

    snprintf(comando, sizeof(comando), "%s %d", CMD_LIST, sequencia);

    switch (envia_comando(comando, AGUARDA_LIST)) {
        case 1:  return 0;
        case 0:  printf("Servidor não respondeu, tente novamente.\n"); return 0;
        default: return -1;
    }
}

/* Opcao 3: pergunta o que os outros clientes cadastraram. */
static int opcao_heartbeat(int sequencia)
{
    char comando[TAM_LINHA];

    snprintf(comando, sizeof(comando), "%s %d", CMD_HEARTBEAT, sequencia);

    switch (envia_comando(comando, AGUARDA_HEARTBEAT)) {
        case 1:  return 0;
        case 0:  printf("Servidor não respondeu, tente novamente.\n"); return 0;
        default: return -1;
    }
}

/* Mostra o menu principal. */
static void mostra_menu(void)
{
    printf("\n");
    printf("1 - Adicionar usuário\n");
    printf("2 - Ver fila\n");
    printf("3 - Heartbeat\n");
    printf("0 - Sair\n");
    fflush(stdout);
}

/* Abre a conexao TCP com o servidor. Devolve 0 se deu certo. */
static int conecta_ao_servidor(void)
{
    struct sockaddr_in endereco;

    socket_servidor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_servidor < 0) {
        perror("Erro ao criar o socket");
        return -1;
    }

    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_port   = htons(SERVER_PORTA);

    /* inet_pton converte o endereco de texto para binario; e melhor que
     * inet_addr porque distingue erro de endereco valido. */
    if (inet_pton(AF_INET, SERVER_IP, &endereco.sin_addr) <= 0) {
        fprintf(stderr, "Endereço IP inválido em SERVER_IP: %s\n", SERVER_IP);
        close(socket_servidor);
        return -1;
    }

    if (connect(socket_servidor, (struct sockaddr *) &endereco,
                sizeof(endereco)) < 0) {
        fprintf(stderr, "Não foi possível conectar em %s:%d - %s\n",
                SERVER_IP, SERVER_PORTA, strerror(errno));
        fprintf(stderr, "Verifique se o servidor está em execução e se o valor de "
                        "SERVER_IP em comum.h corresponde à máquina do servidor.\n");
        close(socket_servidor);
        return -1;
    }

    return 0;
}

/*
 * Manda a credencial que esta no codigo e espera a resposta.
 * Isso acontece antes de a thread receptora existir, entao a troca e
 * sequencial: manda uma mensagem, le uma. Devolve 0 se foi liberado.
 */
static int autentica(void)
{
    LeitorLinha leitor;
    char        resposta[TAM_LINHA];
    char        comando[TAM_LINHA];

    protocolo_leitor_init(&leitor, socket_servidor);

    snprintf(comando, sizeof(comando), "%s %s %s",
             CMD_LOGIN, LOGIN_USUARIO, LOGIN_SENHA);

    if (protocolo_envia_linha(socket_servidor, comando) != 0) {
        fprintf(stderr, "Erro ao enviar as credenciais.\n");
        return -1;
    }

    if (protocolo_le_linha(&leitor, resposta, sizeof(resposta)) != LINHA_OK) {
        fprintf(stderr, "O servidor encerrou a conexão durante a autenticação.\n");
        return -1;
    }

    printf("Servidor: %s\n", resposta);

    if (strcmp(resposta, RESP_LOGIN_OK) != 0) {
        fprintf(stderr, "Acesso negado pelo servidor.\n");
        return -1;
    }

    return 0;
}

int main(void)
{
    pthread_t receptora;
    int       sequencia = 0;   /* numera os comandos desta conexao */
    int       encerrar  = 0;

    /* Sem isso, escrever num socket que o servidor ja fechou mataria o
     * processo em vez de devolver um erro que da para tratar. */
    signal(SIGPIPE, SIG_IGN);

    if (conecta_ao_servidor() != 0) {
        return 1;
    }
    printf("Conectado ao servidor.\n");

    if (autentica() != 0) {
        close(socket_servidor);
        return 1;
    }

    /* Prepara a ligacao entre as duas threads. */
    memset(&canal, 0, sizeof(canal));
    canal.conexao_ativa = 1;
    canal.aguardando    = AGUARDA_NADA;
    pthread_mutex_init(&canal.mutex, NULL);
    pthread_cond_init(&canal.condicao, NULL);

    if (pthread_create(&receptora, NULL, thread_receptora, NULL) != 0) {
        fprintf(stderr, "Erro ao criar a thread de recepção.\n");
        close(socket_servidor);
        return 1;
    }

    /* Laco do menu */
    while (!encerrar) {
        char entrada[TAM_LINHA];
        int  resultado = 0;
        int  ativa;

        pthread_mutex_lock(&canal.mutex);
        ativa = canal.conexao_ativa;
        pthread_mutex_unlock(&canal.mutex);

        if (!ativa) {
            printf("Conexão encerrada pelo servidor.\n");
            break;
        }

        mostra_menu();

        if (!le_teclado(entrada, sizeof(entrada))) {
            break;   /* fim da entrada (Ctrl+D): encerra como se fosse "Sair" */
        }

        if (strcmp(entrada, "1") == 0) {
            sequencia++;
            resultado = opcao_adicionar_usuario(sequencia);
        } else if (strcmp(entrada, "2") == 0) {
            sequencia++;
            resultado = opcao_ver_fila(sequencia);
        } else if (strcmp(entrada, "3") == 0) {
            sequencia++;
            resultado = opcao_heartbeat(sequencia);
        } else if (strcmp(entrada, "0") == 0) {
            encerrar = 1;
        } else if (entrada[0] == '\0') {
            continue;   /* linha em branco: so mostra o menu de novo */
        } else {
            printf("Opção inválida. Escolha 1, 2, 3 ou 0.\n");
        }

        if (resultado < 0) {
            printf("Conexão perdida com o servidor.\n");
            break;
        }
    }

    /* Encerramento */
    (void) protocolo_envia_linha(socket_servidor, CMD_SAIR);

    /* Fechar o socket faz a thread receptora sair do recv() e terminar. */
    shutdown(socket_servidor, SHUT_RDWR);
    pthread_join(receptora, NULL);
    close(socket_servidor);

    pthread_mutex_destroy(&canal.mutex);
    pthread_cond_destroy(&canal.condicao);

    printf("Cliente encerrado.\n");
    return 0;
}
