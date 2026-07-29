/* ===========================================================================
 * protocolo.c - Implementacao do envio e da leitura de linhas.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 * ===========================================================================
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocolo.h"

/* ---------------------------------------------------------------------------
 * envia_todos
 *
 * Envia exatamente 'total' bytes pelo socket. A chamada send() pode aceitar
 * menos bytes do que o pedido quando o buffer do sistema esta cheio, por isso
 * o laco continua ate tudo sair.
 *
 * Devolve 0 se todos os bytes sairam e -1 em caso de erro.
 * ---------------------------------------------------------------------------
 */
static int envia_todos(int sock, const char *dados, size_t total)
{
    size_t enviados = 0;

    while (enviados < total) {
        ssize_t n = send(sock, dados + enviados, total - enviados, 0);

        if (n <= 0) {
            return -1;          /* conexao caiu ou erro */
        }
        enviados += (size_t) n;
    }
    return 0;
}

void protocolo_leitor_init(LeitorLinha *leitor, int sock)
{
    leitor->sock   = sock;
    leitor->usados = 0;
}

int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam)
{
    if (tam == 0) {
        return LINHA_ERRO;
    }

    for (;;) {
        size_t  i;
        ssize_t n;

        /* 1) Procura um '\n' nos bytes que ja temos guardados. */
        for (i = 0; i < leitor->usados; i++) {
            if (leitor->buffer[i] != '\n') {
                continue;
            }

            if (i >= tam) {
                return LINHA_ERRO;   /* linha maior que o buffer do chamador */
            }

            /* Copia a linha (sem o '\n') e fecha a string. */
            memcpy(destino, leitor->buffer, i);
            destino[i] = '\0';

            /* Joga o que sobrou depois do '\n' para o inicio do buffer, para
             * que a proxima leitura comece de uma posicao conhecida. */
            leitor->usados = leitor->usados - (i + 1);
            memmove(leitor->buffer, leitor->buffer + i + 1, leitor->usados);

            /* Aceita tambem linhas terminadas em "\r\n". */
            if (i > 0 && destino[i - 1] == '\r') {
                destino[i - 1] = '\0';
            }
            return LINHA_OK;
        }

        /* 2) Nao ha linha completa. Se o buffer encheu sem nenhum '\n', a
         *    mensagem esta mal formada. */
        if (leitor->usados == sizeof(leitor->buffer)) {
            return LINHA_ERRO;
        }

        /* 3) Le mais bytes do socket, no espaco que sobrou do buffer. */
        n = recv(leitor->sock,
                 leitor->buffer + leitor->usados,
                 sizeof(leitor->buffer) - leitor->usados,
                 0);

        /* Os tres retornos possiveis de recv():
         *   n > 0  - chegaram n bytes (pode ser MENOS que uma mensagem
         *            inteira, ou mais de uma mensagem de uma vez);
         *   n == 0 - o outro lado fechou a conexao de forma ordenada. E
         *            assim que o servidor descobre que um cliente saiu;
         *   n < 0  - erro (conexao perdida, socket invalido).
         *
         * Note que recv() NAO devolve "uma mensagem": ele devolve bytes do
         * fluxo. E exatamente por isso que este modulo existe. */
        if (n == 0) {
            return LINHA_FECHADA;
        }
        if (n < 0) {
            return LINHA_ERRO;
        }
        leitor->usados += (size_t) n;
    }
}

int protocolo_envia_linha(int sock, const char *linha)
{
    if (envia_todos(sock, linha, strlen(linha)) != 0) {
        return -1;
    }
    return envia_todos(sock, "\n", 1);
}

void protocolo_limpa_bordas(char *texto)
{
    size_t inicio = 0;
    size_t fim;

    if (texto == NULL) {
        return;
    }

    fim = strlen(texto);

    /* Apaga os espacos do fim, andando de tras para frente. */
    while (fim > 0 && isspace((unsigned char) texto[fim - 1])) {
        texto[--fim] = '\0';
    }

    /* Conta os espacos do inicio e desloca o texto para a esquerda. */
    while (texto[inicio] != '\0' && isspace((unsigned char) texto[inicio])) {
        inicio++;
    }
    if (inicio > 0) {
        memmove(texto, texto + inicio, fim - inicio + 1);
    }
}
