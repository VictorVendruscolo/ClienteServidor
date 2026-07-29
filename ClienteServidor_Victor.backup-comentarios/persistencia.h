#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

// gravacao em arquivo, em dados/
//   cliente -> sessoes.log, servidor.log
//   usuario -> fila.txt, historico.txt

// cria dados/ e abre os arquivos
int persistencia_init(void);

// evento do servidor: tela e arquivo
void persistencia_log_servidor(const char *formato, ...);

// LOGIN ou LOGOUT em sessoes.log
void persistencia_log_sessao(const char *evento, const char *ip);

// insercao em historico.txt
void persistencia_historico_add(int id, const char *nome);

// reescreve fila.txt com a fila atual
void persistencia_salva_fila(void);

#endif
