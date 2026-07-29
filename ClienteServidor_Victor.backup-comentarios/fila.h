#ifndef FILA_H
#define FILA_H

#include "comum.h"

// fila compartilhada, mutex interno

#define FILA_CHEIA (-1)

// inicializa fila e mutex
int fila_init(void);

// insere no fim, devolve o indice
int fila_adiciona(int id, const char *nome);

// tamanho atual
int fila_tamanho(void);

// copia [inicio, fim), devolve quantos copiou
int fila_copia_intervalo(int inicio, int fim, Usuario *destino, int max);

#endif
