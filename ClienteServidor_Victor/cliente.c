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

// Duas threads: a receptora le o socket e a principal le o teclado. Com um
// fluxo so, o programa ficaria parado no teclado e o broadcast so apareceria
// depois - o enunciado pede atualizacao em tempo real.

// Qual comando espera resposta
#define AGUARDA_NADA      0
#define AGUARDA_ADD       1
#define AGUARDA_LIST      2
#define AGUARDA_HEARTBEAT 3

// Ligacao entre a thread receptora e a principal
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  condicao;

    int aguardando;
    int resposta_completa;
    int conexao_ativa;
    int dentro_do_bloco;     // entre o cabecalho e o rodape da fila
} Canal;

static Canal  canal;
static int    socket_servidor = -1;

// ---------- thread receptora ----------

// Chamar com o mutex do canal ja travado.
static void marca_resposta_completa(void)
{
    canal.resposta_completa = 1;
    canal.aguardando        = AGUARDA_NADA;
    pthread_cond_signal(&canal.condicao);
}

// Imprime a linha conforme o comando que a pediu, no formato das telas.
static void imprime_resposta(int comando, const char *linha, int primeira_linha)
{
    if (strncmp(linha, RESP_ADD_OK, strlen(RESP_ADD_OK)) == 0) {
        const char *resto = linha + strlen(RESP_ADD_OK);
        int         id    = 0;
        int         deslocamento = 0;

        // "ADD_OK <id> <nome>" vira "Usuário <nome> adicionado."
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
        if (comando == AGUARDA_HEARTBEAT && primeira_linha) {
            printf("Heartbeat:\n");     // anuncia antes do bloco
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

    printf("%s\n", linha);              // linha "id - nome" do bloco
}

// Unica funcao do cliente que chama recv(): assim duas threads nunca
// disputam os mesmos bytes.
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

        // broadcast: chega sem o cliente pedir, e sem acento; acentua aqui
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

        // so mostra a resposta se algum comando estiver esperando por ela;
        // senao e copia atrasada de um comando reenviado e abandonado
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

// ---------- envio com reenvio ----------

// Manda o comando e espera a resposta. Sem resposta no tempo, reenvia o
// mesmo texto (logo, o mesmo numero de sequencia), ate MAX_TENTATIVAS.
// Devolve 1 se veio resposta, 0 se nao veio, -1 se a conexao caiu.
static int envia_comando(const char *comando_texto, int tipo)
{
    int envios = 0;
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

            // pthread_cond_timedwait quer um instante absoluto, da mesma
            // origem que o time() usa
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

    // para de esperar: copia atrasada que chegar sera ignorada
    pthread_mutex_lock(&canal.mutex);
    canal.aguardando = AGUARDA_NADA;
    pthread_mutex_unlock(&canal.mutex);

    if (caiu) {
        return -1;
    }
    return obtida ? 1 : 0;
}

// ---------- teclado ----------

// Devolve 1 se leu, 0 se a entrada acabou (Ctrl+D).
static int le_teclado(char *destino, size_t tam)
{
    if (fgets(destino, (int) tam, stdin) == NULL) {
        return 0;
    }
    protocolo_limpa_bordas(destino);
    return 1;
}

static int nome_valido(const char *nome)
{
    int i;

    if (nome[0] == '\0') {
        return 0;
    }
    for (i = 0; nome[i] != '\0'; i++) {
        unsigned char c = (unsigned char) nome[i];
        if (c < 32 || c == 127) {
            return 0;   // caractere de controle quebraria a divisao por linha
        }
    }
    return 1;
}

// ---------- opcoes do menu ----------

// Opcao 1. Devolve 0 para seguir no menu e -1 se a conexao caiu.
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

    strcpy(nome, entrada);      // tamanho ja conferido acima

    snprintf(comando, sizeof(comando), "%s %d %d %s",
             CMD_ADD, sequencia, id, nome);

    switch (envia_comando(comando, AGUARDA_ADD)) {
        case 1:  return 0;
        case 0:  printf("Servidor não respondeu, tente novamente.\n"); return 0;
        default: return -1;
    }
}

// Opcao 2
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

// Opcao 3
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

static void mostra_menu(void)
{
    printf("\n");
    printf("1 - Adicionar usuário\n");
    printf("2 - Ver fila\n");
    printf("3 - Heartbeat\n");
    printf("0 - Sair\n");
    fflush(stdout);
}

// ---------- conexao ----------

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

    // inet_pton: texto para binario; distingue erro de endereco valido
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

// Login automatico com a credencial do codigo. Acontece antes de a thread
// receptora existir, entao e sequencial: manda uma, le uma.
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
    int       sequencia = 0;   // numera os comandos desta conexao
    int       encerrar  = 0;

    signal(SIGPIPE, SIG_IGN);   // sem isso, escrever em socket fechado mata o processo

    if (conecta_ao_servidor() != 0) {
        return 1;
    }
    printf("Conectado ao servidor.\n");

    if (autentica() != 0) {
        close(socket_servidor);
        return 1;
    }

    // prepara a ligacao entre as duas threads
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

    // laco do menu
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
            break;              // Ctrl+D encerra como se fosse "Sair"
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
            continue;
        } else {
            printf("Opção inválida. Escolha 1, 2, 3 ou 0.\n");
        }

        if (resultado < 0) {
            printf("Conexão perdida com o servidor.\n");
            break;
        }
    }

    // encerramento
    (void) protocolo_envia_linha(socket_servidor, CMD_SAIR);

    shutdown(socket_servidor, SHUT_RDWR);   // faz a receptora sair do recv()
    pthread_join(receptora, NULL);
    close(socket_servidor);

    pthread_mutex_destroy(&canal.mutex);
    pthread_cond_destroy(&canal.condicao);

    printf("Cliente encerrado.\n");
    return 0;
}
