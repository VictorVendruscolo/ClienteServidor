/* ===========================================================================
 * sessoes.h - Registro dos clientes autenticados e envio de broadcast.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * POR QUE ESTE MODULO EXISTE
 * --------------------------
 * O enunciado exige que, quando um cliente adiciona um usuario, o servidor
 * distribua a novidade para os demais clientes. Para isso o servidor precisa
 * saber, a qualquer instante, quais sockets estao conectados e autenticados.
 * Esse registro nao esta descrito no enunciado, mas e indispensavel para o
 * protocolo funcionar - por isso e documentado explicitamente.
 *
 * DUAS TRAVAS, COM PAPEIS DIFERENTES
 * ----------------------------------
 * 1) Um mutex protege a TABELA de sessoes (quem esta conectado). Ele e usado
 *    ao registrar, remover e percorrer a tabela. Como a remocao de uma sessao
 *    so acontece quando ninguem esta percorrendo a tabela, e impossivel
 *    enviar dados para um socket que acabou de ser fechado.
 *
 * 2) Cada sessao tem seu proprio mutex de ENVIO. Ele garante que uma resposta
 *    de varias linhas (o bloco "===== FILA =====") nao seja partida ao meio
 *    por uma mensagem de broadcast vinda de outra thread. Sem isso o cliente
 *    receberia linhas intercaladas e o protocolo se quebraria.
 *
 * REGRA PARA NAO HAVER IMPASSE (deadlock)
 * ---------------------------------------
 * Uma thread nunca deve remover a propria sessao enquanto estiver segurando
 * o mutex de envio dela. No servidor isso e respeitado naturalmente: o envio
 * de cada resposta trava e destrava o mutex antes de a conexao ser encerrada.
 * ===========================================================================
 */

#ifndef SESSOES_H
#define SESSOES_H

#include "comum.h"

/* Valor devolvido por sessoes_registra() quando nao ha espaco na tabela. */
#define SESSAO_INVALIDA (-1)

/*
 * Inicializa a tabela de sessoes. Chamar uma unica vez, antes de criar
 * qualquer thread. Retorna 0 em caso de sucesso e -1 em caso de erro.
 */
int sessoes_init(void);

/*
 * Registra um cliente autenticado na tabela.
 *
 * sock - socket ja conectado do cliente
 * ip   - endereco de origem, usado apenas para registro em log
 *
 * Retorna o identificador da sessao (>= 0), usado depois para remove-la, ou
 * SESSAO_INVALIDA se a tabela estiver cheia.
 */
int sessoes_registra(int sock, const char *ip);

/*
 * Remove uma sessao da tabela. Nao fecha o socket: quem abriu a conexao e
 * responsavel por fecha-la, depois de remover a sessao.
 */
void sessoes_remove(int id_sessao);

/*
 * Envia uma linha para todos os clientes autenticados, exceto o da sessao
 * de origem. Falhas de envio individuais sao ignoradas: um cliente que caiu
 * sera removido da tabela pela sua propria thread de atendimento.
 *
 * id_sessao_origem - sessao que provocou o evento (use SESSAO_INVALIDA para
 *                    enviar a todos, sem excecao)
 */
void sessoes_broadcast(int id_sessao_origem, const char *linha);

/*
 * Trava o envio de uma sessao especifica. Usada pela thread de atendimento
 * antes de escrever uma resposta de varias linhas no socket do proprio
 * cliente, garantindo que o bloco saia inteiro.
 */
void sessoes_trava_envio(int id_sessao);

/*
 * Libera a trava de envio obtida com sessoes_trava_envio().
 */
void sessoes_libera_envio(int id_sessao);

/*
 * Retorna quantos clientes estao autenticados neste instante.
 */
int sessoes_conectadas(void);

#endif /* SESSOES_H */
