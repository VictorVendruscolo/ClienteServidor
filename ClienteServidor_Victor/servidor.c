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

// multiplos clientes por threads

#define TAM_PILHA_THREAD (256 * 1024)   // 8 MB padrao nao cabe em 10000 threads
#define LOTE_ENVIO 32
#define TAM_RESPOSTA_ADD 256

// classificacao da sequencia
#define SEQ_NOVA     0
#define SEQ_REPETIDA 1
#define SEQ_INVALIDA 2

// dados passados para a thread de atendimento
typedef struct {
    int           sock;
    char          ip[INET_ADDRSTRLEN];
    int           porta;
    unsigned long numero;
} DadosConexao;

// estado da conexao, na pilha da thread
typedef struct {
    int  sock;
    int  id_sessao;

    int  seq_ultimo;

    char ultima_resposta_add[TAM_RESPOSTA_ADD];   // ADD e o unico que muda a fila
    int  resposta_add_valida;

    int  indice_visto;                            // limites do ultimo heartbeat
    int  indice_visto_antes;
} EstadoConexao;

static unsigned long   total_conexoes  = 0;
static pthread_mutex_t mutex_contador  = PTHREAD_MUTEX_INITIALIZER;

// envio de linha unica
static int envia_resposta(EstadoConexao *estado, const char *linha)
{
    int resultado;

    sessoes_trava_envio(estado->id_sessao);
    resultado = protocolo_envia_linha(estado->sock, linha);
    sessoes_libera_envio(estado->id_sessao);

    return resultado;
}

// envio do bloco da fila
static int envia_bloco_fila(EstadoConexao *estado, int inicio, int fim)
{
    Usuario lote[LOTE_ENVIO];
    int     posicao = inicio;
    int     erro    = 0;

    sessoes_trava_envio(estado->id_sessao);     // bloco sai inteiro

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

// guarda a resposta do ADD
static void guarda_resposta_add(EstadoConexao *estado, const char *resposta)
{
    if (strlen(resposta) < TAM_RESPOSTA_ADD) {
        strcpy(estado->ultima_resposta_add, resposta);
        estado->resposta_add_valida = 1;
    } else {
        estado->resposta_add_valida = 0;
    }
}

// classifica a sequencia: igual = reenvio
static int verifica_sequencia(EstadoConexao *estado, int seq)
{
    if (seq <= 0) {
        return SEQ_INVALIDA;
    }
    if (seq == estado->seq_ultimo) {
        return SEQ_REPETIDA;
    }
    if (seq < estado->seq_ultimo) {
        return SEQ_INVALIDA;
    }
    return SEQ_NOVA;
}

// valida o nome
static int nome_valido(const char *nome)
{
    int i;

    if (nome == NULL || nome[0] == '\0') {
        return 0;
    }
    if (strlen(nome) >= TAM_NOME) {
        return 0;           // recusa em vez de cortar
    }
    for (i = 0; nome[i] != '\0'; i++) {
        unsigned char c = (unsigned char) nome[i];
        if (c < 32 || c == 127) {
            return 0;       // controle quebraria a divisao por linha
        }
    }
    return 1;
}

// ADD <seq> <id> <nome>
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

    // "%*s" pula o comando, "%n" marca onde o nome comeca
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
        if (estado->resposta_add_valida) {
            return envia_resposta(estado, estado->ultima_resposta_add);   // sem reinserir
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

    // autor ja "viu" o proprio registro
    estado->indice_visto_antes = estado->indice_visto;
    estado->indice_visto       = indice + 1;

    persistencia_historico_add(id, nome);
    persistencia_salva_fila();

    // avisa os outros
    snprintf(aviso, sizeof(aviso), "%s%s", BROADCAST_NOVO_USUARIO, nome);
    sessoes_broadcast(estado->id_sessao, aviso);

    snprintf(resposta, sizeof(resposta), "%s %d %s", RESP_ADD_OK, id, nome);
    guarda_resposta_add(estado, resposta);

    return envia_resposta(estado, resposta);
}

// LIST <seq>
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
    estado->resposta_add_valida = 0;        // vale so para o ultimo comando

    return envia_bloco_fila(estado, 0, fila_tamanho());
}

// HEARTBEAT <seq>: novidade ou ALIVE
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

    if (situacao == SEQ_NOVA) {             // repetido mantem os limites
        estado->indice_visto_antes  = estado->indice_visto;
        estado->indice_visto        = fila_tamanho();
        estado->seq_ultimo          = seq;
        estado->resposta_add_valida = 0;
    }

    inicio = estado->indice_visto_antes;
    fim    = estado->indice_visto;

    if (fim <= inicio) {
        return envia_resposta(estado, RESP_ALIVE);      // trecho vazio
    }

    return envia_bloco_fila(estado, inicio, fim);
}

// resultados do login
#define AUTENTICADO        1
#define CREDENCIAL_NEGADA  0
#define SEM_TENTATIVA    (-1)

// login: LOGIN <usuario> <senha>
static int autentica(int sock, LeitorLinha *leitor)
{
    char linha[TAM_LINHA];
    char comando[32];
    char usuario[64];
    char senha[64];

    if (protocolo_le_linha(leitor, linha, sizeof(linha)) != LINHA_OK) {
        return SEM_TENTATIVA;
    }

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

// atendimento: uma copia por cliente conectado
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
    free(dados);

    // numero e origem identificam a conexao
    persistencia_log_servidor("Novo cliente conectado. [#%lu] %s:%d",
                              numero, ip, porta);

    protocolo_leitor_init(&leitor, sock);

    memset(&estado, 0, sizeof(estado));
    estado.sock      = sock;
    estado.id_sessao = SESSAO_INVALIDA;

    // login
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

    // sessao: habilita o broadcast
    estado.id_sessao = sessoes_registra(sock, ip);
    if (estado.id_sessao == SESSAO_INVALIDA) {
        (void) protocolo_envia_linha(sock, RESP_ERRO " Servidor com lotacao maxima de sessoes");
        persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d (tabela de sessoes cheia)",
                                  numero, ip, porta);
        close(sock);
        return NULL;
    }

    persistencia_log_sessao("LOGIN", ip);

    // o que ja estava na fila conta como visto
    estado.indice_visto       = fila_tamanho();
    estado.indice_visto_antes = estado.indice_visto;

    // comandos
    for (;;) {
        int leitura = protocolo_le_linha(&leitor, linha, sizeof(linha));

        if (leitura != LINHA_OK) {
            break;                          // fechou ou linha invalida
        }

        if (sscanf(linha, "%31s", comando) != 1) {
            continue;                       // linha em branco
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

    // encerramento
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

    signal(SIGPIPE, SIG_IGN);   // escrever em socket fechado mataria o processo

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

    // socket de escuta
    socket_escuta = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_escuta < 0) {
        perror("Erro ao criar o socket");
        return 1;
    }

    // SO_REUSEADDR: reinicia sem esperar o TIME_WAIT
    if (setsockopt(socket_escuta, SOL_SOCKET, SO_REUSEADDR,
                   &valor, sizeof(valor)) < 0) {
        perror("Erro no setsockopt");
        close(socket_escuta);
        return 1;
    }

    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family      = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;      // qualquer interface
    endereco_servidor.sin_port        = htons(SERVER_PORTA);   // ordem de bytes da rede

    if (bind(socket_escuta, (struct sockaddr *) &endereco_servidor,
             sizeof(endereco_servidor)) < 0) {
        perror("Erro no bind");
        close(socket_escuta);
        return 1;
    }

    if (listen(socket_escuta, BACKLOG) < 0) {   // BACKLOG: fila de pendentes
        perror("Erro no listen");
        close(socket_escuta);
        return 1;
    }

    // atributos das threads
    if (pthread_attr_init(&atributos) != 0) {
        perror("Erro ao inicializar os atributos de thread");
        close(socket_escuta);
        return 1;
    }
    pthread_attr_setstacksize(&atributos, TAM_PILHA_THREAD);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);   // evita thread zumbi

    persistencia_log_servidor("Servidor iniciado na porta %d", SERVER_PORTA);

    // aceitacao de conexoes
    for (;;) {
        struct sockaddr_in endereco_cliente;
        socklen_t          tamanho_endereco = sizeof(endereco_cliente);
        DadosConexao      *dados;
        pthread_t          identificador_thread;
        int                novo_socket;

        novo_socket = accept(socket_escuta,     // devolve socket novo por cliente
                             (struct sockaddr *) &endereco_cliente,
                             &tamanho_endereco);

        if (novo_socket < 0) {
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

        if (pthread_create(&identificador_thread, &atributos,
                           atende_cliente, (void *) dados) != 0) {
            persistencia_log_servidor("Aviso: nao foi possivel criar a thread do cliente.");
            close(novo_socket);
            free(dados);
            continue;
        }

        sched_yield();      // da a vez para a thread nova
    }
}
