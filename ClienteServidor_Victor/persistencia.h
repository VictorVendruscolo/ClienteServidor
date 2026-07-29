#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

// Gravacao em arquivo texto, escolhida no lugar do banco de dados. Os dados
// ficam em dados/, criado sozinho quando o servidor inicia:
//   dados de cliente -> sessoes.log, servidor.log
//   dados de usuario -> fila.txt, historico.txt

// Cria o diretorio dados/ e abre os arquivos. Devolve 0 se deu certo.
int persistencia_init(void);

// Evento do servidor: imprime na tela e grava com data e hora.
// Mesma sintaxe do printf, sem '\n' no fim.
void persistencia_log_servidor(const char *formato, ...);

// Grava "<EVENTO> <ip> <data hora>" em sessoes.log. Evento: LOGIN ou LOGOUT.
void persistencia_log_sessao(const char *evento, const char *ip);

// Acrescenta "<data hora> ADD <id> <nome>" em historico.txt.
void persistencia_historico_add(int id, const char *nome);

// Reescreve fila.txt com a fila atual, no formato da resposta do LIST.
void persistencia_salva_fila(void);

#endif
