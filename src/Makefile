# ============================================================================
# Makefile - Plataforma de Monitoramento Inteligente de Filas em Tempo Real
#
# Trabalho de Redes de Computadores - UEMS
# Aluno: Victor Vendruscolo
#
# Alvos disponiveis:
#
#   make            gera os dois programas exigidos: cliente e servidor
#   make carga      gera o gerador de clientes para o teste de carga
#   make tudo       gera os tres programas de uma vez
#   make clean      remove executaveis e arquivos objeto
#
# O alvo padrao (make sem parametros) produz EXATAMENTE os dois executaveis
# pedidos no enunciado, com esses nomes, e nada mais.
#
# A opcao -pthread e obrigatoria: ela liga a biblioteca de threads (pthreads),
# tecnica escolhida para atender multiplos clientes simultaneos.
# ============================================================================

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LIBS   = -pthread

# Modulos usados pelo servidor
OBJETOS_SERVIDOR = servidor.o protocolo.o fila.o sessoes.o persistencia.o

# Modulos usados pelo cliente
OBJETOS_CLIENTE  = cliente.o protocolo.o

# Modulos usados pelo gerador de carga
OBJETOS_CARGA    = carga.o protocolo.o

# ---------------------------------------------------------------------------
# Alvo padrao
# ---------------------------------------------------------------------------
all: cliente servidor

# Conveniencia para os testes: compila tambem o gerador de carga.
tudo: cliente servidor carga

# ---------------------------------------------------------------------------
# Ligacao dos executaveis
# ---------------------------------------------------------------------------
servidor: $(OBJETOS_SERVIDOR)
	$(CC) $(CFLAGS) -o servidor $(OBJETOS_SERVIDOR) $(LIBS)

cliente: $(OBJETOS_CLIENTE)
	$(CC) $(CFLAGS) -o cliente $(OBJETOS_CLIENTE) $(LIBS)

carga: $(OBJETOS_CARGA)
	$(CC) $(CFLAGS) -o carga $(OBJETOS_CARGA) $(LIBS)

# ---------------------------------------------------------------------------
# Compilacao dos modulos
#
# Cada regra lista os cabecalhos de que o modulo depende: alterar um .h faz o
# make recompilar apenas o que realmente precisa ser recompilado.
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Limpeza
# ---------------------------------------------------------------------------
clean:
	rm -f cliente servidor carga *.o

.PHONY: all tudo clean
