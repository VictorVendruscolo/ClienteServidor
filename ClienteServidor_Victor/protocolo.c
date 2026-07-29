#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocolo.h"

// envio completo: send pode aceitar menos
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

// prepara o leitor
void protocolo_leitor_init(LeitorLinha *leitor, int sock)
{
    leitor->sock   = sock;
    leitor->usados = 0;
}

// le ate o '\n', guardando o que sobrar
int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam)
{
    if (tam == 0) {
        return LINHA_ERRO;
    }

    for (;;) {
        size_t  i;
        ssize_t n;

        // procura '\n' no que ja esta no buffer
        for (i = 0; i < leitor->usados; i++) {
            if (leitor->buffer[i] != '\n') {
                continue;
            }

            if (i >= tam) {
                return LINHA_ERRO;
            }

            memcpy(destino, leitor->buffer, i);
            destino[i] = '\0';

            // sobra volta para o inicio do buffer
            leitor->usados = leitor->usados - (i + 1);
            memmove(leitor->buffer, leitor->buffer + i + 1, leitor->usados);

            if (i > 0 && destino[i - 1] == '\r') {
                destino[i - 1] = '\0';          // aceita "\r\n"
            }
            return LINHA_OK;
        }

        if (leitor->usados == sizeof(leitor->buffer)) {
            return LINHA_ERRO;                  // buffer cheio sem '\n'
        }

        // le mais no espaco que sobrou
        n = recv(leitor->sock,
                 leitor->buffer + leitor->usados,
                 sizeof(leitor->buffer) - leitor->usados,
                 0);

        if (n == 0) {
            return LINHA_FECHADA;               // outro lado fechou
        }
        if (n < 0) {
            return LINHA_ERRO;
        }
        leitor->usados += (size_t) n;
    }
}

// envia com '\n'
int protocolo_envia_linha(int sock, const char *linha)
{
    if (envia_todos(sock, linha, strlen(linha)) != 0) {
        return -1;
    }
    return envia_todos(sock, "\n", 1);
}

// remove espacos das pontas
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
