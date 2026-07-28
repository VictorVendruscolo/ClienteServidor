/* ===========================================================================
 * protocolo.c - Implementacao da camada de mensagens (ver protocolo.h).
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 * ===========================================================================
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocolo.h"

/* ---------------------------------------------------------------------------
 * envia_todos
 *
 * Envia exatamente 'total' bytes pelo socket. A chamada send() pode aceitar
 * menos bytes do que o pedido (buffer do kernel cheio), por isso o laco.
 * EINTR significa "a chamada foi interrompida por um sinal": nao e erro, e
 * so repetir.
 *
 * Retorna 0 se todos os bytes sairam, -1 em caso de erro.
 * ---------------------------------------------------------------------------
 */
static int envia_todos(int sock, const char *dados, size_t total)
{
    size_t enviados = 0;

    while (enviados < total) {
        ssize_t n = send(sock, dados + enviados, total - enviados, 0);

        if (n > 0) {
            enviados += (size_t) n;
        } else if (n < 0 && errno == EINTR) {
            continue;              /* interrompido por sinal: tenta de novo */
        } else {
            return -1;             /* conexao caiu ou erro real             */
        }
    }
    return 0;
}

void protocolo_leitor_init(LeitorLinha *leitor, int sock)
{
    leitor->sock   = sock;
    leitor->inicio = 0;
    leitor->fim    = 0;
}

int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam)
{
    if (tam == 0) {
        return LINHA_ERRO;
    }

    for (;;) {
        size_t i;

        /* 1) Ja existe uma linha completa nos bytes que temos em maos? */
        for (i = leitor->inicio; i < leitor->fim; i++) {
            if (leitor->buffer[i] == '\n') {
                size_t tamanho_linha = i - leitor->inicio;

                if (tamanho_linha >= tam) {
                    return LINHA_ERRO;   /* linha maior que o buffer do chamador */
                }

                memcpy(destino, leitor->buffer + leitor->inicio, tamanho_linha);
                destino[tamanho_linha] = '\0';

                /* Consome a linha e o proprio '\n'. */
                leitor->inicio = i + 1;
                if (leitor->inicio == leitor->fim) {
                    leitor->inicio = 0;  /* buffer vazio: recomeca do zero */
                    leitor->fim    = 0;
                }

                /* Tolera clientes que terminem a linha com "\r\n". */
                if (tamanho_linha > 0 && destino[tamanho_linha - 1] == '\r') {
                    destino[tamanho_linha - 1] = '\0';
                }
                return LINHA_OK;
            }
        }

        /* 2) Nao ha linha completa: precisamos ler mais bytes do socket. */
        if (leitor->fim == sizeof(leitor->buffer)) {
            if (leitor->inicio > 0) {
                /* Empurra os bytes pendentes para o inicio, abrindo espaco. */
                size_t restante = leitor->fim - leitor->inicio;
                memmove(leitor->buffer, leitor->buffer + leitor->inicio, restante);
                leitor->inicio = 0;
                leitor->fim    = restante;
            } else {
                /* Buffer cheio e nenhum '\n': mensagem mal formada. */
                return LINHA_ERRO;
            }
        }

        {
            ssize_t n = recv(leitor->sock,
                             leitor->buffer + leitor->fim,
                             sizeof(leitor->buffer) - leitor->fim,
                             0);

            if (n > 0) {
                leitor->fim += (size_t) n;
            } else if (n == 0) {
                return LINHA_FECHADA;          /* o outro lado fechou       */
            } else if (errno == EINTR) {
                continue;                      /* sinal: tenta de novo      */
            } else {
                return LINHA_ERRO;
            }
        }
    }
}

int protocolo_envia_linha(int sock, const char *linha)
{
    if (envia_todos(sock, linha, strlen(linha)) != 0) {
        return -1;
    }
    return envia_todos(sock, "\n", 1);
}

int protocolo_envia_fmt(int sock, const char *formato, ...)
{
    char    linha[TAM_LINHA];
    va_list argumentos;
    int     escritos;

    va_start(argumentos, formato);
    escritos = vsnprintf(linha, sizeof(linha), formato, argumentos);
    va_end(argumentos);

    if (escritos < 0) {
        return -1;
    }
    return protocolo_envia_linha(sock, linha);
}

void protocolo_limpa_bordas(char *texto)
{
    size_t inicio = 0;
    size_t fim;

    if (texto == NULL) {
        return;
    }

    fim = strlen(texto);

    while (fim > 0 && isspace((unsigned char) texto[fim - 1])) {
        texto[--fim] = '\0';
    }
    while (texto[inicio] != '\0' && isspace((unsigned char) texto[inicio])) {
        inicio++;
    }
    if (inicio > 0) {
        memmove(texto, texto + inicio, fim - inicio + 1);
    }
}

const char *protocolo_separa_comando(const char *linha, char *comando, size_t tam_cmd)
{
    size_t i = 0;

    /* Pula espacos iniciais. */
    while (*linha != '\0' && isspace((unsigned char) *linha)) {
        linha++;
    }

    /* Copia a primeira palavra, convertida para maiusculas. */
    while (*linha != '\0' && !isspace((unsigned char) *linha)) {
        if (i + 1 < tam_cmd) {
            comando[i++] = (char) toupper((unsigned char) *linha);
        }
        linha++;
    }
    comando[i] = '\0';

    /* Pula os espacos entre o comando e os argumentos. */
    while (*linha != '\0' && isspace((unsigned char) *linha)) {
        linha++;
    }
    return linha;
}
