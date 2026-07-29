#ifndef SESSOES_H
#define SESSOES_H

#include "comum.h"

// registro dos conectados, para o broadcast
// duas travas: tabela e envio por sessao

#define SESSAO_INVALIDA (-1)

// inicializa a tabela
int sessoes_init(void);

// registra cliente, devolve a sessao
int sessoes_registra(int sock, const char *ip);

// remove da tabela, sem fechar o socket
void sessoes_remove(int id_sessao);

// envia a todos, menos a sessao de origem
void sessoes_broadcast(int id_sessao_origem, const char *linha);

// trava o envio da sessao
void sessoes_trava_envio(int id_sessao);

// libera o envio da sessao
void sessoes_libera_envio(int id_sessao);

#endif
