/* ===========================================================================
 * protocolo.h - Camada de transporte de mensagens sobre TCP.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * PROBLEMA QUE ESTE MODULO RESOLVE
 * --------------------------------
 * TCP e um fluxo continuo de bytes: uma chamada send() nao corresponde a uma
 * chamada recv() do outro lado. Uma mensagem pode chegar partida em varias
 * leituras, e duas mensagens podem chegar juntas na mesma leitura.
 *
 * Nosso protocolo resolve isso da forma mais simples possivel: toda mensagem
 * e uma linha de texto ASCII terminada em '\n'. Este modulo implementa os
 * dois lados dessa convencao:
 *
 *   - envio:  garante que TODOS os bytes da linha saiam (send() pode enviar
 *             menos bytes do que o pedido);
 *   - leitura: acumula bytes num buffer proprio ate encontrar um '\n',
 *             devolvendo exatamente uma linha por chamada.
 *
 * As funcoes sao usadas identicamente por cliente e servidor.
 * ===========================================================================
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include "comum.h"

/* ---------------------------------------------------------------------------
 * Leitor de linhas com buffer proprio.
 *
 * Cada socket precisa do seu proprio LeitorLinha, porque os bytes que sobram
 * de uma leitura (o comeco da proxima mensagem) ficam guardados aqui ate a
 * chamada seguinte.
 * ---------------------------------------------------------------------------
 */
typedef struct {
    int    sock;              /* descritor do socket associado              */
    char   buffer[TAM_LINHA]; /* bytes ja lidos do socket e ainda nao usados */
    size_t inicio;            /* posicao do primeiro byte valido            */
    size_t fim;               /* posicao logo apos o ultimo byte valido     */
} LeitorLinha;

/* Codigos de retorno de protocolo_le_linha(). */
#define LINHA_OK       1   /* uma linha completa foi lida                    */
#define LINHA_FECHADA  0   /* o outro lado encerrou a conexao ordenadamente  */
#define LINHA_ERRO    (-1) /* erro de leitura ou linha maior que TAM_LINHA   */

/*
 * Prepara um leitor para operar sobre um socket ja conectado.
 */
void protocolo_leitor_init(LeitorLinha *leitor, int sock);

/*
 * Le exatamente uma linha do socket, sem o '\n' final.
 *
 * destino  - buffer onde a linha sera copiada, sempre terminada em '\0'
 * tam      - tamanho de destino em bytes
 *
 * Retorna LINHA_OK, LINHA_FECHADA ou LINHA_ERRO.
 * Bloqueia ate a linha estar completa ou a conexao terminar.
 */
int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam);

/*
 * Envia uma linha de texto pelo socket, acrescentando o '\n' terminador.
 * Repete o send() ate que todos os bytes tenham sido entregues ao kernel.
 *
 * Retorna 0 em caso de sucesso e -1 em caso de erro.
 */
int protocolo_envia_linha(int sock, const char *linha);

/*
 * Versao com formatacao (mesma sintaxe de printf) de protocolo_envia_linha.
 * Nao inclua '\n' no formato: ele e acrescentado automaticamente.
 *
 * Retorna 0 em caso de sucesso e -1 em caso de erro.
 */
int protocolo_envia_fmt(int sock, const char *formato, ...);

/*
 * Remove espacos, '\r' e '\n' do inicio e do fim de uma string, no lugar.
 * Usada para tolerar entradas de teclado e clientes que terminem linha em
 * "\r\n" em vez de "\n".
 */
void protocolo_limpa_bordas(char *texto);

/*
 * Separa a primeira palavra de uma linha do resto.
 *
 * linha    - linha completa recebida (nao e modificada)
 * comando  - recebe a primeira palavra em MAIUSCULAS
 * tam_cmd  - tamanho do buffer comando
 *
 * Retorna um ponteiro para o primeiro caractere do restante da linha (os
 * argumentos), ja sem os espacos iniciais. Nunca retorna NULL: se nao houver
 * argumentos, aponta para uma string vazia.
 */
const char *protocolo_separa_comando(const char *linha, char *comando, size_t tam_cmd);

#endif /* PROTOCOLO_H */
