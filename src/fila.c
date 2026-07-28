/* ===========================================================================
 * fila.c - Implementacao da fila compartilhada (ver fila.h).
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 * ===========================================================================
 */

#include <string.h>
#include <pthread.h>

#include "fila.h"

/* ---------------------------------------------------------------------------
 * Estado global do modulo.
 *
 * O array e declarado 'static' para que so este arquivo possa toca-lo: todo
 * acesso de fora obrigatoriamente passa pelas funcoes publicas, que travam
 * o mutex. Esse encapsulamento e o que impede condicoes de corrida.
 * ---------------------------------------------------------------------------
 */
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

int fila_adiciona(int id, const char *nome)
{
    int indice;

    pthread_mutex_lock(&fila_mutex);

    if (quantidade >= MAX_USUARIOS_FILA) {
        pthread_mutex_unlock(&fila_mutex);
        return FILA_CHEIA;
    }

    indice = quantidade;
    fila[indice].id = id;

    /* strncpy nao garante terminador quando a origem e maior que o destino;
     * por isso o '\0' e escrito explicitamente na ultima posicao. */
    strncpy(fila[indice].nome, nome, TAM_NOME - 1);
    fila[indice].nome[TAM_NOME - 1] = '\0';

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
