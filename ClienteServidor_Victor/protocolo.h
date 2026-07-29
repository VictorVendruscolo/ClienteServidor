/*
 * protocolo.h - Envio e recebimento de mensagens sobre TCP.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * O TCP entrega um fluxo de bytes, nao mensagens: uma send() nao corresponde
 * a uma recv() do outro lado. Por isso combinei que toda mensagem do
 * protocolo e uma linha de texto terminada em '\n'.
 *
 * Este modulo trata os dois lados dessa combinacao: o envio repete send()
 * ate todos os bytes sairem, e a leitura junta os bytes num buffer proprio
 * ate achar um '\n'. Cliente e servidor usam as mesmas funcoes.
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include "comum.h"

/*
 * Leitor de linhas de um socket.
 * Cada socket precisa do seu, porque os bytes que sobram de uma leitura
 * ficam guardados aqui ate a proxima chamada.
 */
typedef struct {
    int    sock;              /* socket associado                        */
    char   buffer[TAM_LINHA]; /* bytes lidos e ainda nao usados          */
    size_t usados;            /* quantos bytes do buffer estao ocupados  */
} LeitorLinha;

/* Valores devolvidos por protocolo_le_linha(). */
#define LINHA_OK       1   /* leu uma linha completa           */
#define LINHA_FECHADA  0   /* o outro lado fechou a conexao    */
#define LINHA_ERRO    (-1) /* erro de leitura ou linha grande demais */

/* Prepara o leitor para um socket ja conectado. */
void protocolo_leitor_init(LeitorLinha *leitor, int sock);

/*
 * Le uma linha do socket, sem o '\n', e copia para destino.
 * Fica bloqueado ate a linha chegar inteira ou a conexao terminar.
 */
int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam);

/*
 * Envia uma linha pelo socket, acrescentando o '\n' no final.
 * Devolve 0 se deu certo e -1 em caso de erro.
 */
int protocolo_envia_linha(int sock, const char *linha);

/* Tira espacos, '\r' e '\n' das pontas de um texto lido do teclado. */
void protocolo_limpa_bordas(char *texto);

#endif /* PROTOCOLO_H */
