/*
 * fila.h - Fila compartilhada de usuarios.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Como usei threads, todas as threads de atendimento enxergam a mesma
 * memoria e, portanto, a mesma fila. Isso resolve o compartilhamento, mas
 * obriga a proteger todo acesso com um mutex.
 *
 * Quem usa a fila nunca mexe no mutex: so chama as funcoes abaixo, que
 * travam e destravam por dentro. Nenhuma delas usa a rede ou grava em
 * arquivo, para o mutex ficar travado o menor tempo possivel.
 *
 * A fila e um vetor de tamanho fixo, mais simples que uma lista ligada e
 * suficiente para o teste de carga previsto.
 */

#ifndef FILA_H
#define FILA_H

#include "comum.h"

/* Devolvido por fila_adiciona() quando a fila esta cheia. */
#define FILA_CHEIA (-1)

/* Deixa a fila vazia e cria o mutex. Chamar uma vez, antes das threads. */
int fila_init(void);

/*
 * Acrescenta um usuario no fim da fila.
 * Devolve o indice onde entrou, ou FILA_CHEIA se nao couber mais.
 */
int fila_adiciona(int id, const char *nome);

/* Quantos usuarios existem na fila agora. */
int fila_tamanho(void);

/*
 * Copia para destino os usuarios das posicoes [inicio, fim) e devolve
 * quantos copiou. A copia e feita com o mutex travado, entao quem chamou
 * fica com um retrato consistente e pode envia-lo sem prender a fila.
 */
int fila_copia_intervalo(int inicio, int fim, Usuario *destino, int max);

#endif /* FILA_H */
