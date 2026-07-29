#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "sessoes.h"
#include "protocolo.h"

// entrada da tabela
typedef struct {
    int             em_uso;
    int             sock;
    char            ip[INET_ADDRSTRLEN];
    pthread_mutex_t envio_mutex;             // evita escritas misturadas
} Sessao;

static Sessao          tabela[MAX_SESSOES];
static int             proximo_slot     = 0;
static pthread_mutex_t tabela_mutex;

// inicializa a tabela e os mutexes
int sessoes_init(void)
{
    int i;

    memset(tabela, 0, sizeof(tabela));
    proximo_slot     = 0;

    if (pthread_mutex_init(&tabela_mutex, NULL) != 0) {
        return -1;
    }

    for (i = 0; i < MAX_SESSOES; i++) {
        if (pthread_mutex_init(&tabela[i].envio_mutex, NULL) != 0) {
            return -1;
        }
    }
    return 0;
}

// registra cliente autenticado
int sessoes_registra(int sock, const char *ip)
{
    int encontrado = SESSAO_INVALIDA;
    int tentativas;
    int i;

    pthread_mutex_lock(&tabela_mutex);

    // busca circular a partir da ultima posicao usada
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

// remove da tabela
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

// envia a todos, menos a origem
void sessoes_broadcast(int id_sessao_origem, const char *linha)
{
    int i;

    if (linha == NULL) {
        return;
    }

    pthread_mutex_lock(&tabela_mutex);      // impede remocao durante o envio

    for (i = 0; i < MAX_SESSOES; i++) {
        if (!tabela[i].em_uso || i == id_sessao_origem) {
            continue;
        }

        pthread_mutex_lock(&tabela[i].envio_mutex);
        (void) protocolo_envia_linha(tabela[i].sock, linha);
        pthread_mutex_unlock(&tabela[i].envio_mutex);
    }

    pthread_mutex_unlock(&tabela_mutex);
}

// trava o envio, pela thread da sessao
void sessoes_trava_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_lock(&tabela[id_sessao].envio_mutex);
}

// libera o envio
void sessoes_libera_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_unlock(&tabela[id_sessao].envio_mutex);
}
