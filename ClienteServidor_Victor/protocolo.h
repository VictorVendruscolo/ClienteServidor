#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include "comum.h"

// TCP entrega bytes: cada mensagem e uma linha

// leitor por socket: guarda a sobra
typedef struct {
    int    sock;
    char   buffer[TAM_LINHA];
    size_t usados;
} LeitorLinha;

// retornos de protocolo_le_linha
#define LINHA_OK       1
#define LINHA_FECHADA  0
#define LINHA_ERRO    (-1)

// prepara o leitor
void protocolo_leitor_init(LeitorLinha *leitor, int sock);

// le uma linha, sem o '\n'
int protocolo_le_linha(LeitorLinha *leitor, char *destino, size_t tam);

// envia uma linha, com o '\n'
int protocolo_envia_linha(int sock, const char *linha);

// remove espacos das pontas
void protocolo_limpa_bordas(char *texto);

#endif
