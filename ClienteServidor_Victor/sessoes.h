#ifndef SESSOES_H
#define SESSOES_H

#include "comum.h"

// Registro dos clientes conectados, necessario para o broadcast saber para
// quem enviar. Sao duas travas: um mutex protege a tabela, e cada sessao tem
// um mutex proprio de envio, que impede um broadcast de cortar ao meio uma
// resposta de varias linhas.

#define SESSAO_INVALIDA (-1)

// Prepara a tabela. Chamar antes de criar as threads.
int sessoes_init(void);

// Poe um cliente autenticado na tabela. Devolve o numero da sessao.
int sessoes_registra(int sock, const char *ip);

// Tira a sessao da tabela. Nao fecha o socket.
void sessoes_remove(int id_sessao);

// Envia uma linha a todos os autenticados, menos o da sessao de origem.
void sessoes_broadcast(int id_sessao_origem, const char *linha);

// Trava o envio de uma sessao, para uma resposta sair inteira.
void sessoes_trava_envio(int id_sessao);

// Solta a trava de sessoes_trava_envio().
void sessoes_libera_envio(int id_sessao);

#endif
