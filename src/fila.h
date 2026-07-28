/* ===========================================================================
 * fila.h - Fila compartilhada de usuarios (estado de negocio do servidor).
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * A tecnica de concorrencia escolhida foi threads (pthreads): todas as
 * threads de atendimento compartilham a mesma memoria, o que torna a fila
 * naturalmente compartilhada, mas exige exclusao mutua em todo acesso.
 *
 * Este modulo encapsula essa protecao: quem usa a fila nunca manipula o
 * mutex diretamente, apenas chama as funcoes abaixo. Nenhuma funcao deste
 * modulo faz entrada/saida de rede ou de arquivo, para que o mutex seja
 * mantido pelo menor tempo possivel.
 *
 * Estrutura de dados: array de tamanho fixo (MAX_USUARIOS_FILA), escolhido
 * no lugar de lista ligada pela simplicidade de implementacao e por cobrir
 * com folga o teste de carga previsto.
 * ===========================================================================
 */

#ifndef FILA_H
#define FILA_H

#include "comum.h"

/* Codigos de retorno de fila_adiciona(). */
#define FILA_CHEIA (-1)

/*
 * Inicializa a fila (vazia) e o mutex que a protege.
 * Deve ser chamada uma unica vez, antes de qualquer thread ser criada.
 * Retorna 0 em caso de sucesso e -1 em caso de erro.
 */
int fila_init(void);

/*
 * Acrescenta um usuario ao final da fila, de forma atomica.
 *
 * id   - identificador informado pelo operador
 * nome - nome do usuario (truncado em TAM_NOME - 1 caracteres)
 *
 * Retorna o indice em que o usuario foi inserido (>= 0), ou FILA_CHEIA se
 * a capacidade maxima ja tiver sido atingida.
 */
int fila_adiciona(int id, const char *nome);

/*
 * Retorna quantos usuarios existem na fila neste instante.
 */
int fila_tamanho(void);

/*
 * Copia para 'destino' os usuarios das posicoes [inicio, fim) da fila.
 *
 * inicio  - primeiro indice desejado (valores negativos sao tratados como 0)
 * fim     - indice logo apos o ultimo desejado; se maior que o tamanho atual,
 *           e reduzido ao tamanho atual
 * destino - vetor que recebe as copias
 * max     - capacidade de 'destino'
 *
 * Retorna quantos usuarios foram efetivamente copiados. A copia e feita sob
 * o mutex, de modo que o chamador recebe um retrato consistente e pode
 * formatar e enviar os dados sem manter a fila travada.
 */
int fila_copia_intervalo(int inicio, int fim, Usuario *destino, int max);

#endif /* FILA_H */
