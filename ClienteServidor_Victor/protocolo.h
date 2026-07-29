#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include "comum.h"

// O TCP entrega bytes, nao mensagens. Por isso toda mensagem do protocolo e
// uma linha de texto terminada em '\n', e este modulo cuida disso.

// Leitor de um socket. Cada socket precisa do seu, porque os bytes que
// sobram de uma leitura ficam guardados aqui ate a proxima chamada.
typedef struct {
    int    sock;
    char   buffer[TAM_LINHA];
    size_t usados;            // bytes ocupados no buffer
} LeitorLinha;

#define LINHA_OK       1
#define LINHA_FECHADA  0    // o outro lado fechou a conexao
#define LINHA_ERRO    (-1)

// Prepara o leitor para um socket ja conectado.
void protocolo_leitor_init(LeitorLinha *leitor, int sock);

// Le uma linha, sem o '\n'. Bloqueia ate a linha chegar inteira.
int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam);

// Envia uma linha, acrescentando o '\n'. Devolve 0 se deu certo.
int protocolo_envia_linha(int sock, const char *linha);

// Tira espacos, '\r' e '\n' das pontas de um texto lido do teclado.
void protocolo_limpa_bordas(char *texto);

#endif
