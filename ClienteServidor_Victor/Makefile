# make          gera cliente e servidor
# make carga    gera o gerador de clientes do teste de carga
# make tudo     gera os tres
# make clean    apaga executaveis e objetos

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LIBS   = -pthread                      # liga a biblioteca de threads

OBJETOS_SERVIDOR = servidor.o protocolo.o fila.o sessoes.o persistencia.o
OBJETOS_CLIENTE  = cliente.o protocolo.o
OBJETOS_CARGA    = carga.o protocolo.o

all: cliente servidor

tudo: cliente servidor carga

servidor: $(OBJETOS_SERVIDOR)
	$(CC) $(CFLAGS) -o servidor $(OBJETOS_SERVIDOR) $(LIBS)

cliente: $(OBJETOS_CLIENTE)
	$(CC) $(CFLAGS) -o cliente $(OBJETOS_CLIENTE) $(LIBS)

carga: $(OBJETOS_CARGA)
	$(CC) $(CFLAGS) -o carga $(OBJETOS_CARGA) $(LIBS)

# cada regra lista os .h de que o modulo depende, para recompilar so o necessario
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
