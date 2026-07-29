/*
 * servidor.c - Servidor da Plataforma de Monitoramento de Filas.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Execucao:  ./servidor   (sem parametros)
 *
 * Tecnica de multiplos clientes: THREADS (pthreads).
 *
 * A thread principal so faz accept(): recebe a conexao, entrega o socket a
 * uma thread nova e volta a aceitar. Ela nunca atende um cliente. Cada
 * cliente e atendido por uma thread propria, criada ja desatachada para que
 * seus recursos sejam liberados sozinhos no fim (o mesmo papel que o
 * waitpid() tem no modelo com fork).
 *
 * Como as threads compartilham a memoria do processo, a fila de usuarios e
 * comum a todos os clientes sem precisar de IPC; em troca, todo acesso a ela
 * precisa de mutex, o que fica escondido dentro do fila.c.
 *
 * Modulos do programa:
 *   comum.h        - constantes, tipo Usuario e textos do protocolo
 *   protocolo.c    - envio e leitura de linhas sobre o TCP
 *   fila.c         - fila compartilhada protegida por mutex
 *   sessoes.c      - registro dos clientes conectados e broadcast
 *   persistencia.c - gravacao nos arquivos texto
 *   servidor.c     - este arquivo: aceita conexoes e atende os clientes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comum.h"
#include "protocolo.h"
#include "fila.h"
#include "sessoes.h"
#include "persistencia.h"

/*
 * Tamanho da pilha de cada thread de atendimento. O padrao do sistema e 8 MB,
 * o que inviabilizaria o teste com 10000 clientes (10000 x 8 MB = 80 GB de
 * espaco de enderecamento). Cada thread aqui usa poucos KB, entao 256 KB
 * sobra e derruba o total para cerca de 2,5 GB de memoria virtual.
 */
#define TAM_PILHA_THREAD (256 * 1024)

/* Quantos usuarios sao copiados da fila por vez ao enviar um bloco. */
#define LOTE_ENVIO 32

/* Tamanho do texto guardado da resposta do ultimo ADD. */
#define TAM_RESPOSTA_ADD 256

/* Resultado da verificacao do numero de sequencia. */
#define SEQ_NOVA     0
#define SEQ_REPETIDA 1
#define SEQ_INVALIDA 2

/* Dados que a thread principal passa para a thread de atendimento. */
typedef struct {
    int           sock;                    /* socket ja conectado          */
    char          ip[INET_ADDRSTRLEN];     /* endereco de origem           */
    int           porta;                   /* porta de origem              */
    unsigned long numero;                  /* numero da conexao            */
} DadosConexao;

/*
 * Estado de uma conexao, do login ate a saida. Fica na pilha da thread de
 * atendimento: cada cliente tem o seu, ninguem compartilha, entao nao
 * precisa de protecao.
 */
typedef struct {
    int  sock;
    int  id_sessao;

    int  seq_ultimo;                        /* ultima sequencia processada */

    /* Resposta do ultimo ADD. O ADD e o unico comando que muda a fila, entao
     * e o unico que nao pode ser executado duas vezes: se chegar repetido,
     * devolvo este texto em vez de inserir de novo. */
    char ultima_resposta_add[TAM_RESPOSTA_ADD];
    int  resposta_add_valida;

    /* Trecho da fila devolvido no ultimo HEARTBEAT. Guardar os dois limites
     * permite repetir a mesma resposta se o comando for reenviado. */
    int  indice_visto;                      /* fim do trecho    */
    int  indice_visto_antes;                /* inicio do trecho */
} EstadoConexao;

/* Conta as conexoes aceitas, para numerar as linhas do log. */
static unsigned long   total_conexoes  = 0;
static pthread_mutex_t mutex_contador  = PTHREAD_MUTEX_INITIALIZER;

/* Envia uma resposta de uma linha, sem deixar um broadcast entrar no meio. */
static int envia_resposta(EstadoConexao *estado, const char *linha)
{
    int resultado;

    sessoes_trava_envio(estado->id_sessao);
    resultado = protocolo_envia_linha(estado->sock, linha);
    sessoes_libera_envio(estado->id_sessao);

    return resultado;
}

/*
 * Envia o bloco da fila (cabecalho, linhas "id - nome" e rodape) referente ao
 * intervalo [inicio, fim).
 *
 * A trava de envio fica presa durante o bloco todo, para ele chegar inteiro.
 * Ja a fila e travada so em lotes curtos, para uma listagem grande nao
 * impedir os outros clientes de inserir.
 */
static int envia_bloco_fila(EstadoConexao *estado, int inicio, int fim)
{
    Usuario lote[LOTE_ENVIO];
    int     posicao = inicio;
    int     erro    = 0;

    sessoes_trava_envio(estado->id_sessao);

    if (protocolo_envia_linha(estado->sock, FILA_CABECALHO) != 0) {
        erro = 1;
    }

    while (!erro && posicao < fim) {
        int limite = posicao + LOTE_ENVIO;
        int copiados;
        int i;

        if (limite > fim) {
            limite = fim;
        }

        copiados = fila_copia_intervalo(posicao, limite, lote, LOTE_ENVIO);
        if (copiados == 0) {
            break;
        }

        for (i = 0; i < copiados && !erro; i++) {
            char linha[TAM_LINHA];

            snprintf(linha, sizeof(linha), "%d - %s",
                     lote[i].id, lote[i].nome);

            if (protocolo_envia_linha(estado->sock, linha) != 0) {
                erro = 1;
            }
        }
        posicao += copiados;
    }

    if (!erro && protocolo_envia_linha(estado->sock, FILA_RODAPE) != 0) {
        erro = 1;
    }

    sessoes_libera_envio(estado->id_sessao);
    return erro ? -1 : 0;
}

/* Guarda a resposta de um ADD, para devolver caso o comando chegue repetido. */
static void guarda_resposta_add(EstadoConexao *estado, const char *resposta)
{
    if (strlen(resposta) < TAM_RESPOSTA_ADD) {
        strcpy(estado->ultima_resposta_add, resposta);
        estado->resposta_add_valida = 1;
    } else {
        estado->resposta_add_valida = 0;
    }
}

/*
 * Classifica o numero de sequencia recebido.
 *
 * O cliente numera os comandos a partir de 1 e repete o MESMO numero quando
 * reenvia um comando que ficou sem resposta. Comparando com o ultimo numero
 * processado, o servidor sabe se deve executar ou so repetir a resposta - e
 * isso que impede um mesmo usuario de entrar duas vezes na fila.
 */
static int verifica_sequencia(EstadoConexao *estado, int seq)
{
    if (seq <= 0) {
        return SEQ_INVALIDA;
    }
    if (seq == estado->seq_ultimo) {
        return SEQ_REPETIDA;
    }
    if (seq < estado->seq_ultimo) {
        return SEQ_INVALIDA;   /* fora de ordem */
    }
    return SEQ_NOVA;
}

/*
 * Confere se o nome pode ser aceito. Caracteres de controle quebrariam a
 * divisao das mensagens por linha, entao sao recusados.
 */
static int nome_valido(const char *nome)
{
    int i;

    if (nome == NULL || nome[0] == '\0') {
        return 0;
    }
    /* Nome grande demais e recusado, e nao cortado: assim o cliente nunca
     * recebe a confirmacao de um nome diferente do que mandou. */
    if (strlen(nome) >= TAM_NOME) {
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
 * ADD <seq> <id> <nome>
 * Poe o usuario na fila, responde ADD_OK a quem pediu e avisa os outros
 * clientes. O nome pode ter espacos: e todo o resto da linha.
 */
static int trata_add(EstadoConexao *estado, const char *linha)
{
    char        resposta[TAM_RESPOSTA_ADD];
    char        aviso[TAM_LINHA];
    const char *nome;
    int         seq = 0;
    int         id  = 0;
    int         deslocamento = 0;
    int         indice;
    int         situacao;

    /* "%*s" pula o comando sem guardar e "%n" anota quantos caracteres ja
     * foram lidos, entao o resto da linha e o nome. */
    if (sscanf(linha, "%*s %d %d %n", &seq, &id, &deslocamento) < 2) {
        return envia_resposta(estado, RESP_ERRO " Formato esperado: ADD <seq> <id> <nome>");
    }

    if (id <= 0) {
        return envia_resposta(estado, RESP_ERRO " O identificador deve ser um numero positivo");
    }

    nome = linha + deslocamento;
    if (deslocamento == 0 || !nome_valido(nome)) {
        return envia_resposta(estado, RESP_ERRO " Nome invalido, vazio ou longo demais");
    }

    situacao = verifica_sequencia(estado, seq);

    if (situacao == SEQ_INVALIDA) {
        return envia_resposta(estado, RESP_ERRO " Numero de sequencia invalido");
    }
    if (situacao == SEQ_REPETIDA) {
        /* Reenvio: devolve a resposta de antes, sem inserir de novo. */
        if (estado->resposta_add_valida) {
            return envia_resposta(estado, estado->ultima_resposta_add);
        }
        return envia_resposta(estado, RESP_ERRO " Comando repetido sem resposta armazenada");
    }

    indice = fila_adiciona(id, nome);
    estado->seq_ultimo = seq;

    if (indice == FILA_CHEIA) {
        snprintf(resposta, sizeof(resposta), "%s Fila cheia (limite de %d usuarios)",
                 RESP_ERRO, MAX_USUARIOS_FILA);
        guarda_resposta_add(estado, resposta);
        return envia_resposta(estado, resposta);
    }

    /* Quem cadastrou ja "viu" o proprio registro. Sem isso o HEARTBEAT
     * devolveria a ele o usuario que ele mesmo acabou de inserir, e o
     * enunciado pede os cadastrados por outros clientes. */
    estado->indice_visto_antes = estado->indice_visto;
    estado->indice_visto       = indice + 1;

    persistencia_historico_add(id, nome);
    persistencia_salva_fila();

    /* Avisa os outros clientes, ja fora de qualquer trava da fila. */
    snprintf(aviso, sizeof(aviso), "%s%s", BROADCAST_NOVO_USUARIO, nome);
    sessoes_broadcast(estado->id_sessao, aviso);

    snprintf(resposta, sizeof(resposta), "%s %d %s", RESP_ADD_OK, id, nome);
    guarda_resposta_add(estado, resposta);

    return envia_resposta(estado, resposta);
}

/*
 * LIST <seq>
 * Devolve a fila inteira. So le, nao muda nada, entao um comando repetido e
 * simplesmente refeito.
 */
static int trata_list(EstadoConexao *estado, const char *linha)
{
    int seq = 0;
    int situacao;

    if (sscanf(linha, "%*s %d", &seq) != 1) {
        return envia_resposta(estado, RESP_ERRO " Formato esperado: LIST <seq>");
    }

    situacao = verifica_sequencia(estado, seq);
    if (situacao == SEQ_INVALIDA) {
        return envia_resposta(estado, RESP_ERRO " Numero de sequencia invalido");
    }

    estado->seq_ultimo = seq;

    /* A resposta guardada vale so para o ultimo comando; outro comando a
     * descarta. */
    estado->resposta_add_valida = 0;

    return envia_bloco_fila(estado, 0, fila_tamanho());
}

/*
 * HEARTBEAT <seq>
 * Devolve os usuarios cadastrados por OUTROS clientes desde a ultima
 * verificacao deste cliente; se nao houver novidade, responde ALIVE.
 *
 * O trecho fica guardado em dois marcadores. Comando novo avanca os
 * marcadores; comando repetido mantem os mesmos e por isso produz a mesma
 * resposta - inclusive o ALIVE, que e o que sai quando o trecho fica vazio.
 */
static int trata_heartbeat(EstadoConexao *estado, const char *linha)
{
    int seq = 0;
    int situacao;
    int inicio;
    int fim;

    if (sscanf(linha, "%*s %d", &seq) != 1) {
        return envia_resposta(estado, RESP_ERRO " Formato esperado: HEARTBEAT <seq>");
    }

    situacao = verifica_sequencia(estado, seq);
    if (situacao == SEQ_INVALIDA) {
        return envia_resposta(estado, RESP_ERRO " Numero de sequencia invalido");
    }

    if (situacao == SEQ_NOVA) {
        /* Comando novo: o trecho vai do ponto ja visto ate o fim da fila. */
        estado->indice_visto_antes  = estado->indice_visto;
        estado->indice_visto        = fila_tamanho();
        estado->seq_ultimo          = seq;
        estado->resposta_add_valida = 0;
    }

    inicio = estado->indice_visto_antes;
    fim    = estado->indice_visto;

    if (fim <= inicio) {
        return envia_resposta(estado, RESP_ALIVE);   /* nada novo */
    }

    return envia_bloco_fila(estado, inicio, fim);
}

/* Resultados possiveis da autenticacao. */
#define AUTENTICADO        1
#define CREDENCIAL_NEGADA  0
#define SEM_TENTATIVA    (-1)

/*
 * Le a primeira mensagem da conexao e confere a credencial.
 * Formato esperado: LOGIN <usuario> <senha>
 *
 * Devolve AUTENTICADO, CREDENCIAL_NEGADA (tentou e foi recusado) ou
 * SEM_TENTATIVA (a conexao acabou antes de mandar qualquer coisa).
 *
 * A resposta sai aqui sem trava de sessao, porque o cliente so entra na
 * tabela depois de autenticado e ainda nao pode receber broadcast.
 */
static int autentica(int sock, LeitorLinha *leitor)
{
    char linha[TAM_LINHA];
    char comando[32];
    char usuario[64];
    char senha[64];

    if (protocolo_le_linha(leitor, linha, sizeof(linha)) != LINHA_OK) {
        return SEM_TENTATIVA;
    }

    /* Precisa ter as tres partes e a credencial precisa bater. */
    if (sscanf(linha, "%31s %63s %63s", comando, usuario, senha) != 3 ||
        strcmp(comando, CMD_LOGIN)     != 0 ||
        strcmp(usuario, LOGIN_USUARIO) != 0 ||
        strcmp(senha,   LOGIN_SENHA)   != 0) {

        (void) protocolo_envia_linha(sock, RESP_LOGIN_FAIL);
        return CREDENCIAL_NEGADA;
    }

    if (protocolo_envia_linha(sock, RESP_LOGIN_OK) != 0) {
        return SEM_TENTATIVA;
    }
    return AUTENTICADO;
}

/*
 * Atende um cliente do comeco ao fim: autentica, registra a sessao, processa
 * os comandos ate o SAIR ou a queda da conexao, e limpa tudo.
 * Roda uma copia desta funcao em cada thread criada pela thread principal.
 */
static void *atende_cliente(void *argumento)
{
    DadosConexao *dados = (DadosConexao *) argumento;
    EstadoConexao estado;
    LeitorLinha   leitor;
    char          linha[TAM_LINHA];
    char          comando[32];
    int           sock          = dados->sock;
    unsigned long numero        = dados->numero;
    char          ip[INET_ADDRSTRLEN];
    int           porta         = dados->porta;
    int           autenticado   = 0;

    strncpy(ip, dados->ip, sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = '\0';
    free(dados);                    /* ja copiei o que precisava */

    /* O numero e a origem deixam cada conexao identificavel no log; com
     * milhares de clientes, linhas iguais seriam indistinguiveis. */
    persistencia_log_servidor("Novo cliente conectado. [#%lu] %s:%d",
                              numero, ip, porta);

    protocolo_leitor_init(&leitor, sock);

    memset(&estado, 0, sizeof(estado));
    estado.sock      = sock;
    estado.id_sessao = SESSAO_INVALIDA;

    /* 1) Autenticacao */
    autenticado = autentica(sock, &leitor);
    if (autenticado != AUTENTICADO) {
        persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d (%s)",
                                  numero, ip, porta,
                                  (autenticado == CREDENCIAL_NEGADA)
                                      ? "credencial recusada"
                                      : "encerrou antes de autenticar");
        close(sock);
        return NULL;
    }

    /* 2) Registro da sessao: e o que habilita receber broadcast */
    estado.id_sessao = sessoes_registra(sock, ip);
    if (estado.id_sessao == SESSAO_INVALIDA) {
        (void) protocolo_envia_linha(sock, RESP_ERRO " Servidor com lotacao maxima de sessoes");
        persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d (tabela de sessoes cheia)",
                                  numero, ip, porta);
        close(sock);
        return NULL;
    }

    persistencia_log_sessao("LOGIN", ip);

    /* Tudo que ja estava na fila quando ele entrou conta como visto: o
     * heartbeat so reporta o que chegar daqui pra frente. */
    estado.indice_visto       = fila_tamanho();
    estado.indice_visto_antes = estado.indice_visto;

    /* 3) Laco de comandos */
    for (;;) {
        int leitura = protocolo_le_linha(&leitor, linha, sizeof(linha));

        if (leitura != LINHA_OK) {
            break;    /* cliente fechou ou mandou linha invalida */
        }

        /* A primeira palavra da linha e o comando. */
        if (sscanf(linha, "%31s", comando) != 1) {
            continue;              /* linha em branco */
        }

        if (strcmp(comando, CMD_ADD) == 0) {
            if (trata_add(&estado, linha) != 0) {
                break;
            }
        } else if (strcmp(comando, CMD_LIST) == 0) {
            if (trata_list(&estado, linha) != 0) {
                break;
            }
        } else if (strcmp(comando, CMD_HEARTBEAT) == 0) {
            if (trata_heartbeat(&estado, linha) != 0) {
                break;
            }
        } else if (strcmp(comando, CMD_SAIR) == 0) {
            break;
        } else {
            if (envia_resposta(&estado, RESP_ERRO " Comando desconhecido") != 0) {
                break;
            }
        }
    }

    /* 4) Encerramento */
    sessoes_remove(estado.id_sessao);
    persistencia_log_sessao("LOGOUT", ip);
    close(sock);

    persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d", numero, ip, porta);
    return NULL;
}

int main(void)
{
    struct sockaddr_in endereco_servidor;
    pthread_attr_t     atributos;
    int                socket_escuta;
    int                valor = 1;

    /* Escrever num socket que o cliente ja fechou gera SIGPIPE, que por
     * padrao mata o processo inteiro. Ignorando o sinal, o send() so devolve
     * erro, que o codigo ja trata. */
    signal(SIGPIPE, SIG_IGN);

    if (persistencia_init() != 0) {
        return 1;
    }
    if (fila_init() != 0) {
        fprintf(stderr, "Erro ao inicializar a fila.\n");
        return 1;
    }
    if (sessoes_init() != 0) {
        fprintf(stderr, "Erro ao inicializar a tabela de sessoes.\n");
        return 1;
    }

    /* Socket de escuta */
    socket_escuta = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_escuta < 0) {
        perror("Erro ao criar o socket");
        return 1;
    }

    /* SO_REUSEADDR deixa reiniciar o servidor na hora, sem esperar a porta
     * sair do estado TIME_WAIT. */
    if (setsockopt(socket_escuta, SOL_SOCKET, SO_REUSEADDR,
                   &valor, sizeof(valor)) < 0) {
        perror("Erro no setsockopt");
        close(socket_escuta);
        return 1;
    }

    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family      = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;   /* qualquer interface */
    endereco_servidor.sin_port        = htons(SERVER_PORTA);

    if (bind(socket_escuta, (struct sockaddr *) &endereco_servidor,
             sizeof(endereco_servidor)) < 0) {
        perror("Erro no bind");
        close(socket_escuta);
        return 1;
    }

    if (listen(socket_escuta, BACKLOG) < 0) {
        perror("Erro no listen");
        close(socket_escuta);
        return 1;
    }

    /* Atributos das threads de atendimento */
    if (pthread_attr_init(&atributos) != 0) {
        perror("Erro ao inicializar os atributos de thread");
        close(socket_escuta);
        return 1;
    }
    pthread_attr_setstacksize(&atributos, TAM_PILHA_THREAD);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);

    persistencia_log_servidor("Servidor iniciado na porta %d", SERVER_PORTA);

    /* Laco de aceitacao de conexoes */
    for (;;) {
        struct sockaddr_in endereco_cliente;
        socklen_t          tamanho_endereco = sizeof(endereco_cliente);
        DadosConexao      *dados;
        pthread_t          identificador_thread;
        int                novo_socket;

        novo_socket = accept(socket_escuta,
                             (struct sockaddr *) &endereco_cliente,
                             &tamanho_endereco);

        if (novo_socket < 0) {
            /* Falhar em aceitar uma conexao nao derruba o servidor: registra
             * e volta a aceitar. */
            perror("Erro no accept");
            continue;
        }

        dados = (DadosConexao *) malloc(sizeof(DadosConexao));
        if (dados == NULL) {
            persistencia_log_servidor("Aviso: sem memoria para aceitar a conexao.");
            close(novo_socket);
            continue;
        }

        dados->sock  = novo_socket;
        dados->porta = ntohs(endereco_cliente.sin_port);
        if (inet_ntop(AF_INET, &endereco_cliente.sin_addr,
                      dados->ip, sizeof(dados->ip)) == NULL) {
            strcpy(dados->ip, "desconhecido");
        }

        pthread_mutex_lock(&mutex_contador);
        dados->numero = ++total_conexoes;
        pthread_mutex_unlock(&mutex_contador);

        /* Uma thread por cliente. Os atributos ja pedem thread desatachada,
         * o que dispensa o pthread_detach e evita thread "zumbi". */
        if (pthread_create(&identificador_thread, &atributos,
                           atende_cliente, (void *) dados) != 0) {
            persistencia_log_servidor("Aviso: nao foi possivel criar a thread do cliente.");
            close(novo_socket);
            free(dados);
            continue;
        }

        /* Da a vez para a thread recem-criada comecar a rodar. */
        sched_yield();
    }

    /* O laco so termina se o processo for encerrado (Ctrl+C). Como cada
     * gravacao e seguida de fflush, nada se perde. */
}
