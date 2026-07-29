/*
 * persistencia.h - Gravacao dos dados em arquivos texto.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * O enunciado deixa escolher entre banco de dados e arquivo; escolhi
 * arquivo texto. Os dados sao separados nos dois grupos que o enunciado
 * cita:
 *
 *   dados de cliente (quem conectou)  -> dados/sessoes.log, dados/servidor.log
 *   dados de usuario (conteudo da fila) -> dados/fila.txt, dados/historico.txt
 *
 * O subdiretorio "dados/" e criado sozinho quando o servidor inicia.
 */

#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

/* Cria o diretorio dados/ e abre os arquivos. Devolve 0 se deu certo. */
int persistencia_init(void);

/*
 * Registra um evento do servidor: imprime no terminal (e o texto que aparece
 * na tela) e grava em dados/servidor.log com data e hora.
 * Usa a mesma sintaxe do printf, sem '\n' no fim.
 */
void persistencia_log_servidor(const char *formato, ...);

/*
 * Grava em dados/sessoes.log uma linha "<EVENTO> <ip> <data hora>",
 * com evento "LOGIN" ou "LOGOUT".
 */
void persistencia_log_sessao(const char *evento, const char *ip);

/*
 * Acrescenta em dados/historico.txt a linha "<data hora> ADD <id> <nome>".
 * Esse arquivo nunca e reescrito: guarda tudo que ja entrou na fila.
 */
void persistencia_historico_add(int id, const char *nome);

/*
 * Reescreve dados/fila.txt com a fila atual, no mesmo formato da resposta do
 * comando LIST. Chamada depois de cada insercao.
 */
void persistencia_salva_fila(void);

#endif /* PERSISTENCIA_H */
