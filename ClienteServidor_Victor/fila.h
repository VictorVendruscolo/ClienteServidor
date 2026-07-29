#ifndef FILA_H
#define FILA_H

#include "comum.h"

// Fila compartilhada por todas as threads de atendimento. O mutex fica
// dentro do modulo: quem usa a fila so chama as funcoes abaixo.

#define FILA_CHEIA (-1)

// Deixa a fila vazia e cria o mutex. Chamar antes de criar as threads.
int fila_init(void);

// Insere no fim. Devolve o indice onde entrou, ou FILA_CHEIA.
int fila_adiciona(int id, const char *nome);

// Quantos usuarios existem na fila agora.
int fila_tamanho(void);

// Copia [inicio, fim) para destino. Devolve quantos copiou.
int fila_copia_intervalo(int inicio, int fim, Usuario *destino, int max);

#endif
