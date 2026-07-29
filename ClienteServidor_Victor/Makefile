# ---------------------------------------------------------------------------
# Makefile - Plataforma de Monitoramento Inteligente de Filas em Tempo Real
#
# Trabalho de Redes de Computadores - UEMS
# Aluno: Victor Vendruscolo
#
#   make          gera os dois programas: cliente e servidor
#   make carga    gera o gerador de clientes do teste de carga
#   make tudo     gera os tres de uma vez
#   make clean    apaga executaveis e arquivos objeto
#
# O alvo padrao (make sem parametros) gera exatamente cliente e servidor.
# A opcao -pthread e obrigatoria: e ela que liga a biblioteca de threads.
# ---------------------------------------------------------------------------

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LIBS   = -pthread

OBJETOS_SERVIDOR = servidor.o protocolo.o fila.o sessoes.o persistencia.o
OBJETOS_CLIENTE  = cliente.o protocolo.o
OBJETOS_CARGA    = carga.o protocolo.o

all: cliente servidor

tudo: cliente servidor carga

# --- Ligacao dos executaveis ---
servidor: $(OBJETOS_SERVIDOR)
	$(CC) $(CFLAGS) -o servidor $(OBJETOS_SERVIDOR) $(LIBS)

cliente: $(OBJETOS_CLIENTE)
	$(CC) $(CFLAGS) -o cliente $(OBJETOS_CLIENTE) $(LIBS)

carga: $(OBJETOS_CARGA)
	$(CC) $(CFLAGS) -o carga $(OBJETOS_CARGA) $(LIBS)

# --- Compilacao dos modulos ---
# Cada regra lista os cabecalhos de que o modulo depende, para o make
# recompilar so o necessario quando um .h muda.
servidor.o: servidor.c comum.h protocolo.h fila.h sessoes.h persistencia.h
	$(CC) $(CFLAGS) -c servidor.c

cliente.o: cliente.c comum.h protocolo.h
	$(CC) $(CFLAGS) -c cliente.c

carga.o: carga.c comum.h protocolo.h
	$(CC) $(CFLAGS) -c carga.c

protocolo.o: protocolo.c protocolo.h comum.h
	$(CC) $(CFLAGS) -c protocolo.c

fila.o: fila.c fila.h comum.h
	$(CC) $(CFLAGS) -c fila.c

sessoes.o: sessoes.c sessoes.h comum.h protocolo.h
	$(CC) $(CFLAGS) -c sessoes.c

persistencia.o: persistencia.c persistencia.h comum.h fila.h
	$(CC) $(CFLAGS) -c persistencia.c

clean:
	rm -f cliente servidor carga *.o

.PHONY: all tudo clean
