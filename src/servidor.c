/* ===========================================================================
 * servidor.c - Servidor da Plataforma de Monitoramento de Filas.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Execucao:  ./servidor          (sem parametros)
 *
 * RESPONSABILIDADES (conforme o enunciado)
 * ----------------------------------------
 *   - aceitar conexoes;
 *   - autenticar os clientes;
 *   - gerenciar a fila compartilhada;
 *   - sincronizar os clientes (broadcast e heartbeat);
 *   - distribuir eventos e monitorar os sockets.
 *
 * TECNICA DE MULTIPLOS CLIENTES: THREADS (pthreads)
 * -------------------------------------------------
 * A thread principal faz apenas accept(): recebe a conexao e entrega o
 * socket a uma thread nova, voltando imediatamente a aceitar. Cada cliente e
 * atendido por uma thread propria, criada com pthread_create() e desatachada
 * com pthread_detach() para que seus recursos sejam liberados sozinhos ao
 * terminar (equivalente ao waitpid() do modelo com fork).
 *
 * Como todas as threads compartilham a memoria do processo, a fila de
 * usuarios e naturalmente comum a todos os clientes; a exclusao mutua fica
 * encapsulada no modulo fila.c.
 *
 * ORGANIZACAO EM MODULOS
 * ----------------------
 *   comum.h        - constantes, tipos e textos do protocolo
 *   protocolo.c    - envio/recepcao de linhas sobre o fluxo TCP
 *   fila.c         - fila compartilhada protegida por mutex
 *   sessoes.c      - registro de clientes conectados e broadcast
 *   persistencia.c - gravacao em arquivos texto
 *   servidor.c     - este arquivo: aceitacao de conexoes e atendimento
 * ===========================================================================
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

/* Tamanho da pilha de cada thread de atendimento.
 * O padrao do sistema (8 MB) tornaria invivavel o teste com 10000 clientes
 * simultaneos: 10000 x 8 MB = 80 GB de espaco de enderecamento. Como cada
 * thread usa poucos kilobytes, 256 KB e folgado e reduz o total para ~2,5 GB
 * de memoria virtual (reservada, nao ocupada fisicamente). */
#define TAM_PILHA_THREAD (256 * 1024)

/* Quantos usuarios sao copiados da fila por vez ao enviar um bloco. */
#define LOTE_ENVIO 32

/* Tamanho do texto guardado da resposta do ultimo ADD. */
#define TAM_RESPOSTA_ADD 256

/* Resultado da verificacao do numero de sequencia de um comando. */
#define SEQ_NOVA     0
#define SEQ_REPETIDA 1
#define SEQ_INVALIDA 2

/* ---------------------------------------------------------------------------
 * Dados passados da thread principal para a thread de atendimento.
 * ---------------------------------------------------------------------------
 */
typedef struct {
    int           sock;                    /* socket ja conectado           */
    char          ip[INET_ADDRSTRLEN];     /* endereco de origem            */
    int           porta;                   /* porta de origem               */
    unsigned long numero;                  /* numero sequencial da conexao  */
} DadosConexao;

/* ---------------------------------------------------------------------------
 * Estado mantido por conexao durante toda a sessao.
 *
 * Fica na pilha da thread de atendimento: cada cliente tem o seu, sem
 * qualquer compartilhamento e, portanto, sem necessidade de protecao.
 * ---------------------------------------------------------------------------
 */
typedef struct {
    int  sock;
    int  id_sessao;

    int  seq_ultimo;                        /* ultima sequencia processada  */

    /* Guarda a resposta do ultimo ADD. O ADD e o unico comando que altera a
     * fila, entao e o unico que nao pode ser executado duas vezes: se ele
     * chegar repetido, devolvemos este texto em vez de inserir de novo. */
    char ultima_resposta_add[TAM_RESPOSTA_ADD];
    int  resposta_add_valida;

    /* Trecho da fila devolvido no ultimo HEARTBEAT. Guardar os dois limites
     * permite repetir exatamente a mesma resposta se o comando for reenviado. */
    int  indice_visto;                      /* fim do trecho (ja visto)     */
    int  indice_visto_antes;                /* inicio do trecho             */
} EstadoConexao;

/* Contador de conexoes aceitas, usado para numerar as linhas de log e tornar
 * cada conexao identificavel nos testes de carga. */
static unsigned long   total_conexoes  = 0;
static pthread_mutex_t mutex_contador  = PTHREAD_MUTEX_INITIALIZER;

/* ===========================================================================
 * Funcoes auxiliares de envio
 * ===========================================================================
 */

/*
 * Envia uma resposta de uma unica linha, garantindo que ela nao seja partida
 * por uma mensagem de broadcast vinda de outra thread.
 */
static int envia_resposta(EstadoConexao *estado, const char *linha)
{
    int resultado;

    sessoes_trava_envio(estado->id_sessao);
    resultado = protocolo_envia_linha(estado->sock, linha);
    sessoes_libera_envio(estado->id_sessao);

    return resultado;
}

/*
 * Envia um bloco de fila (cabecalho, linhas "id - nome" e rodape) referente
 * ao intervalo [inicio, fim) da fila.
 *
 * A trava de envio e mantida durante todo o bloco, de modo que o conjunto
 * chegue inteiro ao cliente. A fila, em contrapartida, e travada apenas em
 * lotes curtos: assim uma listagem longa nao impede outros clientes de
 * inserir usuarios.
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
            break;   /* a fila encolheu (nao ocorre nesta versao) */
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

/*
 * Guarda o texto da resposta de um ADD, para poder devolve-lo caso o mesmo
 * comando chegue de novo por causa de um reenvio.
 */
static void guarda_resposta_add(EstadoConexao *estado, const char *resposta)
{
    if (strlen(resposta) < TAM_RESPOSTA_ADD) {
        strcpy(estado->ultima_resposta_add, resposta);
        estado->resposta_add_valida = 1;
    } else {
        estado->resposta_add_valida = 0;
    }
}

/* ===========================================================================
 * Retransmissao: controle de numero de sequencia
 * ===========================================================================
 */

/*
 * Classifica o numero de sequencia recebido.
 *
 * O cliente numera seus comandos a partir de 1, em ordem crescente, e usa o
 * MESMO numero ao reenviar um comando que ficou sem resposta. Comparando com
 * o ultimo numero processado, o servidor sabe se deve executar o comando ou
 * apenas repetir a resposta anterior - o que impede, por exemplo, que um
 * mesmo usuario entre duas vezes na fila por causa de um reenvio.
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
        return SEQ_INVALIDA;   /* sequencia fora de ordem */
    }
    return SEQ_NOVA;
}

/* ===========================================================================
 * Validacao de entrada
 * ===========================================================================
 */

/*
 * Verifica se um nome pode ser aceito: nao pode ser vazio nem conter
 * caracteres de controle, que quebrariam a delimitacao de mensagens por
 * linha do protocolo.
 */
static int nome_valido(const char *nome)
{
    int i;

    if (nome == NULL || nome[0] == '\0') {
        return 0;
    }
    /* Nomes acima do limite sao recusados, e nao cortados: assim o cliente
     * nunca recebe a confirmacao de um nome diferente do que enviou. */
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

/* ===========================================================================
 * Tratamento dos comandos
 * ===========================================================================
 */

/*
 * ADD <seq> <id> <nome>
 *
 * Insere um usuario na fila, responde ADD_OK ao autor e avisa os demais
 * clientes por broadcast. O nome pode conter espacos: e todo o restante da
 * linha depois do identificador.
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

    /* "%*s" pula o comando ADD sem guardar nada, e "%n" anota quantos
     * caracteres ja foram lidos - assim o resto da linha e o nome, que pode
     * conter espacos. */
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
        /* Reenvio: devolve a resposta anterior sem inserir de novo. */
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

    /* O proprio autor ja "viu" o usuario que acabou de cadastrar: sem isso o
     * HEARTBEAT devolveria a ele mesmo o registro que ele criou, contrariando
     * a definicao de "usuarios cadastrados por outros clientes". */
    estado->indice_visto_antes = estado->indice_visto;
    estado->indice_visto       = indice + 1;

    /* Persistencia: historico permanente e retrato atual da fila. */
    persistencia_historico_add(id, nome);
    persistencia_salva_fila();

    /* Aviso assincrono aos demais clientes (fora de qualquer trava da fila). */
    snprintf(aviso, sizeof(aviso), "%s%s", BROADCAST_NOVO_USUARIO, nome);
    sessoes_broadcast(estado->id_sessao, aviso);

    snprintf(resposta, sizeof(resposta), "%s %d %s", RESP_ADD_OK, id, nome);
    guarda_resposta_add(estado, resposta);

    return envia_resposta(estado, resposta);
}

/*
 * LIST <seq>
 *
 * Devolve a fila inteira. E uma operacao somente de leitura: reenvia-la
 * produz o mesmo efeito, por isso um comando repetido e simplesmente
 * recalculado.
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

    /* A resposta guardada vale apenas para o ultimo comando; qualquer outro
     * comando a descarta. */
    estado->resposta_add_valida = 0;

    return envia_bloco_fila(estado, 0, fila_tamanho());
}

/*
 * HEARTBEAT <seq>
 *
 * Devolve os usuarios cadastrados por OUTROS clientes desde a ultima
 * verificacao deste cliente. Nao havendo novidade, responde ALIVE.
 *
 * O trecho a devolver e guardado em dois marcadores. Um comando novo avanca
 * esses marcadores; um comando repetido mantem os mesmos e, por isso, produz
 * exatamente a mesma resposta de antes - inclusive ALIVE, que e o que sai
 * quando o trecho fica vazio.
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
        /* Comando novo: o trecho vai do ponto ja visto ate o fim atual da fila. */
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

/* ===========================================================================
 * Autenticacao
 * ===========================================================================
 */

/* Resultados possiveis da autenticacao. */
#define AUTENTICADO        1
#define CREDENCIAL_NEGADA  0
#define SEM_TENTATIVA    (-1)

/*
 * Le a primeira mensagem da conexao e verifica a credencial.
 * Formato esperado: LOGIN <usuario> <senha>
 *
 * Retorna AUTENTICADO, CREDENCIAL_NEGADA (o cliente tentou e foi recusado) ou
 * SEM_TENTATIVA (a conexao terminou antes de qualquer mensagem chegar - o que
 * acontece, por exemplo, no teste de carga que apenas abre conexoes).
 *
 * A resposta (LOGIN_OK ou LOGIN_FAIL) e enviada aqui, ainda sem trava de
 * sessao, porque o cliente so entra na tabela de sessoes depois de
 * autenticado - e portanto ainda nao pode receber broadcast.
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

    /* A linha precisa ter as tres partes e a credencial precisa conferir. */
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

/* ===========================================================================
 * Thread de atendimento
 * ===========================================================================
 */

/*
 * Atende um cliente do inicio ao fim da conexao: autentica, registra a
 * sessao, processa comandos ate SAIR ou desconexao, e limpa os recursos.
 *
 * Uma instancia desta funcao roda em cada thread criada pela thread
 * principal.
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
    free(dados);                    /* os dados ja foram copiados para a pilha */

    /* A linha comeca exatamente com o texto da tela do enunciado; o numero da
     * conexao e a origem sao acrescentados para que, nos testes de carga, seja
     * possivel identificar cada uma das conexoes na captura de tela. */
    persistencia_log_servidor("Novo cliente conectado. [#%lu] %s:%d",
                              numero, ip, porta);

    protocolo_leitor_init(&leitor, sock);

    memset(&estado, 0, sizeof(estado));
    estado.sock      = sock;
    estado.id_sessao = SESSAO_INVALIDA;

    /* --- 1) Autenticacao -------------------------------------------------- */
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

    /* --- 2) Registro da sessao (habilita o recebimento de broadcast) ------- */
    estado.id_sessao = sessoes_registra(sock, ip);
    if (estado.id_sessao == SESSAO_INVALIDA) {
        (void) protocolo_envia_linha(sock, RESP_ERRO " Servidor com lotacao maxima de sessoes");
        persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d (tabela de sessoes cheia)",
                                  numero, ip, porta);
        close(sock);
        return NULL;
    }

    persistencia_log_sessao("LOGIN", ip);

    /* O cliente considera "ja vistos" todos os usuarios que estavam na fila
     * quando ele entrou: o heartbeat so reporta o que chegar dai em diante. */
    estado.indice_visto       = fila_tamanho();
    estado.indice_visto_antes = estado.indice_visto;

    /* --- 3) Laco de comandos ---------------------------------------------- */
    for (;;) {
        int leitura = protocolo_le_linha(&leitor, linha, sizeof(linha));

        if (leitura != LINHA_OK) {
            break;    /* cliente fechou a conexao ou enviou linha invalida */
        }

        /* A primeira palavra da linha e o comando. */
        if (sscanf(linha, "%31s", comando) != 1) {
            continue;              /* linha em branco: ignorada */
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

    /* --- 4) Encerramento -------------------------------------------------- */
    sessoes_remove(estado.id_sessao);
    persistencia_log_sessao("LOGOUT", ip);
    close(sock);

    persistencia_log_servidor("Cliente desconectado. [#%lu] %s:%d", numero, ip, porta);
    return NULL;
}

/* ===========================================================================
 * Programa principal
 * ===========================================================================
 */

int main(void)
{
    struct sockaddr_in endereco_servidor;
    pthread_attr_t     atributos;
    int                socket_escuta;
    int                valor = 1;

    /* Um send() para um socket que o cliente ja fechou gera SIGPIPE, cujo
     * comportamento padrao e encerrar o processo inteiro. Ignorar o sinal faz
     * o send() apenas devolver erro, que o codigo ja trata - indispensavel
     * num servidor com muitos clientes entrando e saindo. */
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

    /* --- Socket de escuta -------------------------------------------------- */
    socket_escuta = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_escuta < 0) {
        perror("Erro ao criar o socket");
        return 1;
    }

    /* SO_REUSEADDR permite reiniciar o servidor imediatamente, sem esperar o
     * estado TIME_WAIT da porta expirar. */
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

    /* --- Atributos das threads de atendimento ------------------------------ */
    if (pthread_attr_init(&atributos) != 0) {
        perror("Erro ao inicializar os atributos de thread");
        close(socket_escuta);
        return 1;
    }
    pthread_attr_setstacksize(&atributos, TAM_PILHA_THREAD);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);

    persistencia_log_servidor("Servidor iniciado na porta %d", SERVER_PORTA);

    /* --- Laco de aceitacao de conexoes ------------------------------------- */
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
            /* Uma falha ao aceitar (inclusive falta de descritores livres) nao
             * derruba o servidor: registra e volta a aceitar conexoes. */
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
         * o que dispensa pthread_detach() e evita threads "zumbis". */
        if (pthread_create(&identificador_thread, &atributos,
                           atende_cliente, (void *) dados) != 0) {
            persistencia_log_servidor("Aviso: nao foi possivel criar a thread do cliente.");
            close(novo_socket);
            free(dados);
            continue;
        }

        /* Cede a CPU para que a thread recem-criada comece a executar. */
        sched_yield();
    }

    /* O laco acima so termina se o processo for encerrado (Ctrl+C). Como cada
     * gravacao nos arquivos de dados e seguida de fflush, nada se perde. */
}
