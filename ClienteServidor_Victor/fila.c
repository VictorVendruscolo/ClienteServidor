#include <string.h>
#include <pthread.h>

#include "fila.h"

// static: so este arquivo alcanca o vetor, entao todo acesso passa pelas
// funcoes abaixo, que travam o mutex.
static Usuario         fila[MAX_USUARIOS_FILA];
static int             quantidade = 0;
static pthread_mutex_t fila_mutex;

int fila_init(void)
{
    quantidade = 0;
    memset(fila, 0, sizeof(fila));

    if (pthread_mutex_init(&fila_mutex, NULL) != 0) {
        return -1;
    }
    return 0;
}

// Insere no fim da fila. Devolve o indice, ou FILA_CHEIA.
int fila_adiciona(int id, const char *nome)
{
    int indice;

    pthread_mutex_lock(&fila_mutex);            // secao critica

    if (quantidade >= MAX_USUARIOS_FILA) {
        pthread_mutex_unlock(&fila_mutex);
        return FILA_CHEIA;
    }

    indice = quantidade;
    fila[indice].id = id;
    strncpy(fila[indice].nome, nome, TAM_NOME - 1);
    fila[indice].nome[TAM_NOME - 1] = '\0';     // strncpy nao garante o '\0'
    quantidade++;

    pthread_mutex_unlock(&fila_mutex);
    return indice;
}

int fila_tamanho(void)
{
    int total;

    pthread_mutex_lock(&fila_mutex);
    total = quantidade;
    pthread_mutex_unlock(&fila_mutex);

    return total;
}

// Copia um trecho da fila. Copiar sob o mutex deixa quem chamou com um
// retrato consistente, sem precisar prender a fila para enviar.
int fila_copia_intervalo(int inicio, int fim, Usuario *destino, int max)
{
    int copiados = 0;

    if (destino == NULL || max <= 0) {
        return 0;
    }
    if (inicio < 0) {
        inicio = 0;
    }

    pthread_mutex_lock(&fila_mutex);

    if (fim > quantidade) {
        fim = quantidade;
    }
    while (inicio < fim && copiados < max) {
        destino[copiados] = fila[inicio];
        copiados++;
        inicio++;
    }

    pthread_mutex_unlock(&fila_mutex);
    return copiados;
}
