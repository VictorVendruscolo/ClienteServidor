/* ===========================================================================
 * persistencia.h - Gravacao dos dados do sistema em arquivos texto.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * O enunciado permite escolher entre banco de dados e arquivo; a escolha
 * deste trabalho foi arquivo texto. Os dados sao separados em dois grupos,
 * conforme a lista do enunciado:
 *
 *   dados de CLIENTE  (quem conectou)  -> dados/sessoes.log, dados/servidor.log
 *   dados de USUARIO  (conteudo da fila) -> dados/fila.txt, dados/historico.txt
 *
 * Todos os arquivos ficam no subdiretorio "dados/", criado automaticamente
 * na inicializacao do servidor. Cada arquivo tem seu proprio mutex, de modo
 * que threads diferentes possam gravar em arquivos diferentes ao mesmo tempo
 * sem embaralhar linhas.
 * ===========================================================================
 */

#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

/*
 * Cria o diretorio "dados/" e abre os arquivos em modo de acrescimo.
 * Retorna 0 em caso de sucesso e -1 se algum arquivo nao puder ser aberto.
 */
int persistencia_init(void);

/*
 * Registra um evento do servidor. A mensagem e impressa no terminal (e o
 * texto que aparece nas telas de funcionamento) e gravada em
 * dados/servidor.log com data e hora.
 *
 * Usa a mesma sintaxe de printf, sem '\n' no final.
 */
void persistencia_log_servidor(const char *formato, ...);

/*
 * Registra um evento de sessao em dados/sessoes.log, no formato
 * "<EVENTO> <ip> <data hora>". Usado com evento "LOGIN" e "LOGOUT".
 */
void persistencia_log_sessao(const char *evento, const char *ip);

/*
 * Acrescenta uma linha ao historico de insercoes (dados/historico.txt), no
 * formato "<data hora> ADD <id> <nome>". O arquivo nunca e reescrito: e o
 * registro permanente de tudo que entrou na fila.
 */
void persistencia_historico_add(int id, const char *nome);

/*
 * Reescreve dados/fila.txt com o conteudo atual da fila, no mesmo formato
 * usado na resposta ao comando LIST. Chamada apos cada insercao.
 */
void persistencia_salva_fila(void);

#endif /* PERSISTENCIA_H */
