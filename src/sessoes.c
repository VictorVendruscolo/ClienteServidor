/* ===========================================================================
 * sessoes.c - Implementacao do registro de clientes (ver sessoes.h).
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 * ===========================================================================
 */

#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "sessoes.h"
#include "protocolo.h"

/* ---------------------------------------------------------------------------
 * Uma entrada da tabela de sessoes.
 * ---------------------------------------------------------------------------
 */
typedef struct {
    int             em_uso;                  /* 1 se a entrada esta ocupada  */
    int             sock;                    /* socket do cliente            */
    char            ip[INET_ADDRSTRLEN];     /* endereco de origem (log)     */
    pthread_mutex_t envio_mutex;             /* serializa escritas no socket */
} Sessao;

static Sessao          tabela[MAX_SESSOES];
static int             proximo_slot     = 0;  /* dica de busca circular      */
static pthread_mutex_t tabela_mutex;

int sessoes_init(void)
{
    int i;

    memset(tabela, 0, sizeof(tabela));
    proximo_slot     = 0;

    if (pthread_mutex_init(&tabela_mutex, NULL) != 0) {
        return -1;
    }

    /* Os mutexes de envio sao criados uma unica vez, na inicializacao, e
     * reaproveitados a cada nova sessao que ocupar aquele slot. */
    for (i = 0; i < MAX_SESSOES; i++) {
        if (pthread_mutex_init(&tabela[i].envio_mutex, NULL) != 0) {
            return -1;
        }
    }
    return 0;
}

int sessoes_registra(int sock, const char *ip)
{
    int encontrado = SESSAO_INVALIDA;
    int tentativas;
    int i;

    pthread_mutex_lock(&tabela_mutex);

    /* Busca circular a partir do ultimo slot usado: evita varrer a tabela
     * inteira desde o inicio a cada nova conexao. */
    i = proximo_slot;
    for (tentativas = 0; tentativas < MAX_SESSOES; tentativas++) {
        if (!tabela[i].em_uso) {
            encontrado = i;
            break;
        }
        i = (i + 1) % MAX_SESSOES;
    }

    if (encontrado != SESSAO_INVALIDA) {
        tabela[encontrado].em_uso = 1;
        tabela[encontrado].sock   = sock;

        if (ip != NULL) {
            strncpy(tabela[encontrado].ip, ip, INET_ADDRSTRLEN - 1);
            tabela[encontrado].ip[INET_ADDRSTRLEN - 1] = '\0';
        } else {
            tabela[encontrado].ip[0] = '\0';
        }

        proximo_slot = (encontrado + 1) % MAX_SESSOES;
    }

    pthread_mutex_unlock(&tabela_mutex);
    return encontrado;
}

void sessoes_remove(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_lock(&tabela_mutex);

    if (tabela[id_sessao].em_uso) {
        tabela[id_sessao].em_uso = 0;
        tabela[id_sessao].sock   = -1;
    }

    pthread_mutex_unlock(&tabela_mutex);
}

void sessoes_broadcast(int id_sessao_origem, const char *linha)
{
    int i;

    if (linha == NULL) {
        return;
    }

    /* A tabela fica travada durante todo o laco. E isso que impede uma sessao
     * de ser removida (e o socket fechado) no meio de um envio. */
    pthread_mutex_lock(&tabela_mutex);

    for (i = 0; i < MAX_SESSOES; i++) {
        if (!tabela[i].em_uso || i == id_sessao_origem) {
            continue;
        }

        /* O mutex de envio garante que esta linha nao se intercale com uma
         * resposta de varias linhas que a thread daquele cliente esteja
         * enviando neste momento. */
        pthread_mutex_lock(&tabela[i].envio_mutex);
        (void) protocolo_envia_linha(tabela[i].sock, linha);
        pthread_mutex_unlock(&tabela[i].envio_mutex);
    }

    pthread_mutex_unlock(&tabela_mutex);
}

void sessoes_trava_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    /* Nao e preciso travar a tabela aqui: quem chama e a propria thread dona
     * da sessao, e so ela remove a sessao - portanto a entrada existe com
     * certeza enquanto esta chamada acontece. */
    pthread_mutex_lock(&tabela[id_sessao].envio_mutex);
}

void sessoes_libera_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_unlock(&tabela[id_sessao].envio_mutex);
}

