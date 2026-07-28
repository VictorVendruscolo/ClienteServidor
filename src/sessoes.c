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

static Sessao           tabela[MAX_SESSOES];
static int              total_conectadas = 0;
static int              proximo_slot     = 0;  /* dica de busca circular     */
static pthread_rwlock_t tabela_lock;

int sessoes_init(void)
{
    int i;

    memset(tabela, 0, sizeof(tabela));
    total_conectadas = 0;
    proximo_slot     = 0;

    if (pthread_rwlock_init(&tabela_lock, NULL) != 0) {
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

    pthread_rwlock_wrlock(&tabela_lock);

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
        total_conectadas++;
    }

    pthread_rwlock_unlock(&tabela_lock);
    return encontrado;
}

void sessoes_remove(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_rwlock_wrlock(&tabela_lock);

    if (tabela[id_sessao].em_uso) {
        tabela[id_sessao].em_uso = 0;
        tabela[id_sessao].sock   = -1;
        total_conectadas--;
    }

    pthread_rwlock_unlock(&tabela_lock);
}

void sessoes_broadcast(int id_sessao_origem, const char *linha)
{
    int i;

    if (linha == NULL) {
        return;
    }

    /* Trava de leitura: varios broadcasts podem ocorrer ao mesmo tempo, mas
     * nenhuma sessao pode ser removida enquanto este laco estiver rodando -
     * e isso que impede um send() em socket ja fechado. */
    pthread_rwlock_rdlock(&tabela_lock);

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

    pthread_rwlock_unlock(&tabela_lock);
}

void sessoes_trava_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    /* A trava de leitura e adquirida junto com o mutex de envio para manter
     * sempre a mesma ordem de aquisicao usada em sessoes_broadcast(). Ordem
     * unica de travamento e o que garante ausencia de impasse (deadlock). */
    pthread_rwlock_rdlock(&tabela_lock);
    pthread_mutex_lock(&tabela[id_sessao].envio_mutex);
}

void sessoes_libera_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_unlock(&tabela[id_sessao].envio_mutex);
    pthread_rwlock_unlock(&tabela_lock);
}

int sessoes_conectadas(void)
{
    int total;

    pthread_rwlock_rdlock(&tabela_lock);
    total = total_conectadas;
    pthread_rwlock_unlock(&tabela_lock);

    return total;
}

void sessoes_destroi(void)
{
    int i;

    for (i = 0; i < MAX_SESSOES; i++) {
        pthread_mutex_destroy(&tabela[i].envio_mutex);
    }
    pthread_rwlock_destroy(&tabela_lock);
}
