/* ===========================================================================
 * protocolo.h - Envio e recebimento de mensagens sobre TCP.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * O TCP entrega um fluxo continuo de bytes: uma chamada send() nao
 * corresponde a uma chamada recv() do outro lado. Uma mensagem pode chegar
 * partida em varias leituras, e duas mensagens podem chegar juntas.
 *
 * Por isso o nosso protocolo combina que toda mensagem e uma linha de texto
 * terminada em '\n'. Este modulo cuida dos dois lados dessa combinacao:
 * o envio garante que todos os bytes saiam, e a leitura acumula bytes num
 * buffer proprio ate achar um '\n', devolvendo uma linha por chamada.
 *
 * As mesmas funcoes sao usadas pelo cliente e pelo servidor.
 * ===========================================================================
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include "comum.h"

/* ---------------------------------------------------------------------------
 * Leitor de linhas.
 *
 * Cada socket precisa do seu proprio leitor, porque os bytes que sobram de
 * uma leitura (o comeco da proxima mensagem) ficam guardados aqui ate a
 * chamada seguinte.
 * ---------------------------------------------------------------------------
 */
typedef struct {
    int    sock;              /* socket associado                          */
    char   buffer[TAM_LINHA]; /* bytes lidos do socket e ainda nao usados   */
    size_t usados;            /* quantos bytes do buffer estao ocupados     */
} LeitorLinha;

/* Valores devolvidos por protocolo_le_linha(). */
#define LINHA_OK       1   /* uma linha completa foi lida                   */
#define LINHA_FECHADA  0   /* o outro lado encerrou a conexao               */
#define LINHA_ERRO    (-1) /* erro de leitura ou linha maior que TAM_LINHA  */

/* Prepara um leitor para um socket ja conectado. */
void protocolo_leitor_init(LeitorLinha *leitor, int sock);

/*
 * Le uma linha do socket, sem o '\n' final, e copia para destino.
 * Bloqueia ate a linha estar completa ou a conexao terminar.
 * Devolve LINHA_OK, LINHA_FECHADA ou LINHA_ERRO.
 */
int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam);

/*
 * Envia uma linha pelo socket, acrescentando o '\n' no final.
 * Devolve 0 em caso de sucesso e -1 em caso de erro.
 */
int protocolo_envia_linha(int sock, const char *linha);

/*
 * Remove espacos, '\r' e '\n' do inicio e do fim de um texto, no lugar.
 * Usada para limpar o que vem do teclado.
 */
void protocolo_limpa_bordas(char *texto);

#endif /* PROTOCOLO_H */
