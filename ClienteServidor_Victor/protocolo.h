#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include "comum.h"

// TCP entrega bytes: cada mensagem e uma linha
// retornos da leitura
#define LINHA_OK       1
#define LINHA_FECHADA  0
#define LINHA_ERRO    (-1)

// leitura de uma linha
int protocolo_le_linha(int sock, char *destino, size_t tam);

// envio de uma linha
int protocolo_envia_linha(int sock, const char *linha);

// limpeza de bordas
void protocolo_limpa_bordas(char *texto);

#endif
