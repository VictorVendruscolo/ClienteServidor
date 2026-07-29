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

// Tecnica de multiplos clientes: threads. A thread principal so aceita
// conexoes e entrega cada uma a uma thread nova; nunca atende um cliente.

#define TAM_PILHA_THREAD (256 * 1024)   // 256 KB: com os 8 MB padrao, 10000 threads nao caberiam
#define LOTE_ENVIO 32                   // usuarios copiados por vez da fila
#define TAM_RESPOSTA_ADD 256

// Resultado da verificacao do numero de sequencia
#define SEQ_NOVA     0
#define SEQ_REPETIDA 1
#define SEQ_INVALIDA 2

// Dados que a thread principal passa para a thread de atendimento
typedef struct {
    int           sock;
    char          ip[INET_ADDRSTRLEN];
    int           porta;
    unsigned long numero;                  // numero da conexao
} DadosConexao;

// Estado de uma conexao, do login ate a saida. Fica na pilha da thread:
// cada cliente tem o seu, ninguem compartilha, nao precisa de protecao.
typedef struct {
    int  sock;
    int  id_sessao;

    int  seq_ultimo;

    // resposta do ultimo ADD: e o unico comando que muda a fila, entao o
    // unico que nao pode ser executado duas vezes
    char ultima_resposta_add[TAM_RESPOSTA_ADD];
    int  resposta_add_valida;

    // trecho devolvido no ultimo HEARTBEAT; guardar os dois limites permite
    // repetir a mesma resposta se o comando for reenviado
    int  indice_visto;
    int  indice_visto_antes;
} EstadoConexao;

static unsigned long   total_conexoes  = 0;
static pthread_mutex_t mutex_contador  = PTHREAD_MUTEX_INITIALIZER;

// ---------- envio ----------

// Envia uma linha sem deixar um broadcast entrar no meio.
static int envia_resposta(EstadoConexao *estado, const char *linha)
{
    int resultado;

    sessoes_trava_envio(estado->id_sessao);
    resultado = protocolo_envia_linha(estado->sock, linha);
    sessoes_libera_envio(estado->id_sessao);

    return resultado;
}

// Envia o bloco da fila referente a [inicio, fim). A trava de envio segura o
// bloco todo; a fila e travada so em lotes curtos.
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

static void guarda_resposta_add(EstadoConexao *estado, const char *resposta)
{
    if (strlen(resposta) < TAM_RESPOSTA_ADD) {
        strcpy(estado->ultima_resposta_add, resposta);
        estado->resposta_add_valida = 1;
    } else {
        estado->resposta_add_valida = 0;
    }
}

// ---------- retransmissao ----------

// O cliente repete o MESMO numero ao reenviar um comando sem resposta.
// Comparando com o ultimo processado, sabemos se executamos ou so repetimos
// a resposta - e isso que impede o mesmo usuario de entrar duas vezes.
static int verifica_sequencia(EstadoConexao *estado, int seq)
{
    if (seq <= 0) {
        return SEQ_INVALIDA;
    }
    if (seq == estado->seq_ultimo) {
        return SEQ_REPETIDA;
    }
    if (seq < estado->seq_ultimo) {
        return SEQ_INVALIDA;                 // fora de ordem
    }
    return SEQ_NOVA;
}

// ---------- validacao ----------

static int nome_valido(const char *nome)
{
    int i;

    if (nome == NULL || nome[0] == '\0') {
        return 0;
    }
    if (strlen(nome) >= TAM_NOME) {
        return 0;      // recusa em vez de cortar: o cliente nao pode receber nome diferente
    }
    for (i = 0; nome[i] != '\0'; i++) {
        unsigned char c = (unsigned char) nome[i];
        if (c < 32 || c == 127) {
            return 0;  // caractere de controle quebraria a divisao por linha
        }
    }
    return 1;
}

// ---------- comandos ----------

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

    // "%*s" pula o comando e "%n" marca onde parou: o resto da linha e o
    // nome, que pode ter espacos
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
            return envia_resposta(estado, estado->ultima_resposta_add);   // sem inserir de novo
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

    // quem cadastrou ja "viu" o proprio registro, senao o HEARTBEAT
    // devolveria a ele o usuario que ele mesmo inseriu
    estado->indice_visto_antes = estado->indice_visto;
    estado->indice_visto       = indice + 1;

    persistencia_historico_add(id, nome);
    persistencia_salva_fila();

    // avisa os outros clientes, ja fora de qualquer trava da fila
    snprintf(aviso, sizeof(aviso), "%s%s", BROADCAST_NOVO_USUARIO, nome);
    sessoes_broadcast(estado->id_sessao, aviso);

    snprintf(resposta, sizeof(resposta), "%s %d %s", RESP_ADD_OK, id, nome);
    guarda_resposta_add(estado, resposta);

    return envia_resposta(estado, resposta);
}

// LIST <seq> - devolve a fila inteira. So le, entao repetir e refazer.
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
    estado->resposta_add_valida = 0;    // a resposta guardada vale so para o ultimo comando

    return envia_bloco_fila(estado, 0, fila_tamanho());
}

// HEARTBEAT <seq> - devolve os usuarios cadastrados por OUTROS clientes
// desde a ultima verificacao; se nao houver, ALIVE.
// Comando novo avanca os marcadores; comando repetido mantem os mesmos e
// produz a mesma resposta - inclusive o ALIVE, que sai do trecho vazio.
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
        estado->indice_visto_antes  = estado->indice_visto;
        estado->indice_visto        = fila_tamanho();
        estado->seq_ultimo          = seq;
        estado->resposta_add_valida = 0;
    }

    inicio = estado->indice_visto_antes;
    fim    = estado->indice_visto;

    if (fim <= inicio) {
        return envia_resposta(estado, RESP_ALIVE);      // nada novo
    }

    return envia_bloco_fila(estado, inicio, fim);
}

// ---------- autenticacao ----------

#define AUTENTICADO        1
#define CREDENCIAL_NEGADA  0
#define SEM_TENTATIVA    (-1)   // conexao acabou antes de mandar qualquer coisa

// Le a primeira mensagem e confere a credencial: LOGIN <usuario> <senha>
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

// ---------- thread de atendimento ----------

// Roda uma copia desta funcao por cliente conectado.
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
    free(dados);                        // ja copiei o que precisava

    // numero e origem deixam cada conexao identificavel com muitos clientes
    persistencia_log_servidor("Novo cliente conectado. [#%lu] %s:%d",
                              numero, ip, porta);

    protocolo_leitor_init(&leitor, sock);

    memset(&estado, 0, sizeof(estado));
    estado.sock      = sock;
    estado.id_sessao = SESSAO_INVALIDA;

    // autenticacao
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

    // registro da sessao: e o que habilita receber broadcast
    estado.id_sessao = sessoes_registra(sock, ip);
    if (estado.id_sessao == SESSAO_INVALIDA) {
        (void) protocolo_envia_linha(sock, RESP_ERRO " Servidor com lotacao maxima de sessoes");
        persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d (tabela de sessoes cheia)",
                                  numero, ip, porta);
        close(sock);
        return NULL;
    }

    persistencia_log_sessao("LOGIN", ip);

    // o que ja estava na fila conta como visto: o heartbeat so reporta o
    // que chegar daqui pra frente
    estado.indice_visto       = fila_tamanho();
    estado.indice_visto_antes = estado.indice_visto;

    // laco de comandos
    for (;;) {
        int leitura = protocolo_le_linha(&leitor, linha, sizeof(linha));

        if (leitura != LINHA_OK) {
            break;                      // cliente fechou ou linha invalida
        }

        if (sscanf(linha, "%31s", comando) != 1) {
            continue;                   // linha em branco
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

    signal(SIGPIPE, SIG_IGN);   // sem isso, escrever em socket fechado mata o processo

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

    // SO_REUSEADDR: reinicia o servidor na hora, sem esperar o TIME_WAIT
    if (setsockopt(socket_escuta, SOL_SOCKET, SO_REUSEADDR,
                   &valor, sizeof(valor)) < 0) {
        perror("Erro no setsockopt");
        close(socket_escuta);
        return 1;
    }

    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family      = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;      // qualquer interface
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

    // atributos das threads de atendimento
    if (pthread_attr_init(&atributos) != 0) {
        perror("Erro ao inicializar os atributos de thread");
        close(socket_escuta);
        return 1;
    }
    pthread_attr_setstacksize(&atributos, TAM_PILHA_THREAD);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);  // evita thread zumbi

    persistencia_log_servidor("Servidor iniciado na porta %d", SERVER_PORTA);

    // laco de aceitacao
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
            perror("Erro no accept");   // falhar em aceitar nao derruba o servidor
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

        sched_yield();      // da a vez para a thread recem-criada
    }
}
