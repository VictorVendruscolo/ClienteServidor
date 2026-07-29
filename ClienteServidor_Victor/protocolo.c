#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocolo.h"

// envio completo
static int envia_todos(int sock, const char *dados, size_t total)
{
    size_t enviados = 0;

    while (enviados < total) {
        ssize_t n = send(sock, dados + enviados, total - enviados, 0);

        if (n <= 0) {
            return -1;
        }
        enviados += (size_t) n;
    }
    return 0;
}

// leitura byte a byte
int protocolo_le_linha(int sock, char *destino, size_t tam)
{
    size_t usados = 0;
    char   c;

    if (tam == 0) {
        return LINHA_ERRO;
    }

    for (;;) {
        ssize_t n = recv(sock, &c, 1, 0);

        if (n == 0) {
            return LINHA_FECHADA;
        }
        if (n < 0) {
            return LINHA_ERRO;
        }
        if (c == '\n') {
            destino[usados] = '\0';
            return LINHA_OK;
        }
        if (usados + 1 >= tam) {
            return LINHA_ERRO;
        }
        destino[usados] = c;
        usados++;
    }
}

// envio com terminador
int protocolo_envia_linha(int sock, const char *linha)
{
    if (envia_todos(sock, linha, strlen(linha)) != 0) {
        return -1;
    }
    return envia_todos(sock, "\n", 1);
}

// espacos das pontas
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
