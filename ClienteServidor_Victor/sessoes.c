#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "sessoes.h"
#include "protocolo.h"

typedef struct {
    int             em_uso;
    int             sock;
    char            ip[INET_ADDRSTRLEN];
    pthread_mutex_t envio_mutex;             // impede escritas misturadas
} Sessao;

static Sessao          tabela[MAX_SESSOES];
static int             proximo_slot     = 0;  // por onde comecar a procurar
static pthread_mutex_t tabela_mutex;

int sessoes_init(void)
{
    int i;

    memset(tabela, 0, sizeof(tabela));
    proximo_slot     = 0;

    if (pthread_mutex_init(&tabela_mutex, NULL) != 0) {
        return -1;
    }

    // os mutexes de envio sao criados uma vez e reaproveitados por cada
    // sessao que ocupar aquela posicao
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

    // busca circular: comeca da ultima posicao usada, em vez de varrer a
    // tabela inteira a cada nova conexao
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

    // a tabela fica travada durante todo o laco: e isso que impede uma
    // sessao de ser removida (e o socket fechado) no meio de um envio
    pthread_mutex_lock(&tabela_mutex);

    for (i = 0; i < MAX_SESSOES; i++) {
        if (!tabela[i].em_uso || i == id_sessao_origem) {
            continue;
        }

        // trava o envio daquele cliente para esta linha nao entrar no meio
        // de uma resposta que a thread dele esteja mandando
        pthread_mutex_lock(&tabela[i].envio_mutex);
        (void) protocolo_envia_linha(tabela[i].sock, linha);
        pthread_mutex_unlock(&tabela[i].envio_mutex);
    }

    pthread_mutex_unlock(&tabela_mutex);
}

// Nao trava a tabela: quem chama e a thread dona da sessao, e so ela remove
// essa sessao, entao a posicao existe.
void sessoes_trava_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_lock(&tabela[id_sessao].envio_mutex);
}

void sessoes_libera_envio(int id_sessao)
{
    if (id_sessao < 0 || id_sessao >= MAX_SESSOES) {
        return;
    }

    pthread_mutex_unlock(&tabela[id_sessao].envio_mutex);
}
