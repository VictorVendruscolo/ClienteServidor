/*
 * sessoes.h - Registro dos clientes conectados e envio de broadcast.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * O enunciado pede que, ao adicionar um usuario, o servidor avise os outros
 * clientes. Para isso ele precisa saber quais sockets estao conectados e
 * autenticados a cada momento. Esse registro nao esta no enunciado, mas sem
 * ele o broadcast nao teria como funcionar.
 *
 * Sao duas travas, com papeis diferentes:
 *
 * 1) um mutex protege a tabela de sessoes. Como uma sessao so pode ser
 *    removida quando ninguem esta percorrendo a tabela, nunca acontece de
 *    enviar dados para um socket que acabou de ser fechado;
 *
 * 2) cada sessao tem um mutex proprio de envio, que impede uma resposta de
 *    varias linhas (o bloco "===== FILA =====") de ser cortada no meio por
 *    um broadcast vindo de outra thread.
 *
 * Regra para nao travar tudo (deadlock): uma thread nunca remove a propria
 * sessao enquanto segura o mutex de envio dela. No servidor isso acontece
 * naturalmente, porque cada resposta trava e destrava antes do fim da
 * conexao.
 */

#ifndef SESSOES_H
#define SESSOES_H

#include "comum.h"

/* Devolvido por sessoes_registra() quando a tabela esta cheia. */
#define SESSAO_INVALIDA (-1)

/* Prepara a tabela. Chamar uma vez, antes de criar qualquer thread. */
int sessoes_init(void);

/*
 * Coloca um cliente ja autenticado na tabela e devolve o numero da sessao,
 * usado depois para remove-la.
 */
int sessoes_registra(int sock, const char *ip);

/*
 * Tira a sessao da tabela. Nao fecha o socket: quem abriu a conexao fecha,
 * depois de remover a sessao.
 */
void sessoes_remove(int id_sessao);

/*
 * Manda uma linha para todos os clientes autenticados, menos o da sessao de
 * origem. Se o envio para algum falhar, ignora: a thread daquele cliente vai
 * perceber a queda e remover a sessao.
 */
void sessoes_broadcast(int id_sessao_origem, const char *linha);

/*
 * Trava o envio de uma sessao. A thread de atendimento usa antes de escrever
 * uma resposta de varias linhas, para o bloco sair inteiro.
 */
void sessoes_trava_envio(int id_sessao);

/* Solta a trava obtida com sessoes_trava_envio(). */
void sessoes_libera_envio(int id_sessao);

#endif /* SESSOES_H */
