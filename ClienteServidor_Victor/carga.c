#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comum.h"
#include "protocolo.h"

// teste de carga: ./carga <quantidade>
// antes de muitos clientes: ulimit -n 20000

#define INTERVALO_RELATORIO 100

// conexao
static int conecta(void)
{
    struct sockaddr_in endereco;
    int                sock;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return -1;
    }

    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_port   = htons(SERVER_PORTA);

    if (inet_pton(AF_INET, SERVER_IP, &endereco.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *) &endereco, sizeof(endereco)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

// login
static int autentica(int sock)
{
    LeitorLinha leitor;
    char        comando[TAM_LINHA];
    char        resposta[TAM_LINHA];

    snprintf(comando, sizeof(comando), "%s %s %s",
             CMD_LOGIN, LOGIN_USUARIO, LOGIN_SENHA);

    if (protocolo_envia_linha(sock, comando) != 0) {
        return 0;
    }

    protocolo_leitor_init(&leitor, sock);
    if (protocolo_le_linha(&leitor, resposta, sizeof(resposta)) != LINHA_OK) {
        return 0;
    }

    return strcmp(resposta, RESP_LOGIN_OK) == 0;
}

int main(int argc, char *argv[])
{
    int *sockets;
    int  quantidade;
    int  abertos = 0;
    int  falhas  = 0;
    int  i;
    char tecla[8];

    signal(SIGPIPE, SIG_IGN);

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <quantidade>\n", argv[0]);
        fprintf(stderr, "Exemplos: %s 100 | %s 1000 | %s 10000\n",
                argv[0], argv[0], argv[0]);
        return 1;
    }

    quantidade = atoi(argv[1]);
    if (quantidade <= 0) {
        fprintf(stderr, "Erro: a quantidade deve ser um numero inteiro positivo.\n");
        return 1;
    }

    sockets = (int *) malloc((size_t) quantidade * sizeof(int));
    if (sockets == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para %d conexoes.\n", quantidade);
        return 1;
    }

    printf("Abrindo %d conexoes em %s:%d\n\n", quantidade, SERVER_IP, SERVER_PORTA);

    // abre e autentica cada cliente
    for (i = 0; i < quantidade; i++) {
        int sock = conecta();

        if (sock >= 0 && !autentica(sock)) {
            close(sock);
            sock = -1;
        }

        sockets[i] = sock;

        if (sock >= 0) {
            abertos++;
        } else {
            falhas++;
            if (falhas == 1) {
                // em geral: descritores ou servidor fora
                printf("Primeira falha na conexao %d: %s\n", i + 1, strerror(errno));
            }
        }

        if ((i + 1) % INTERVALO_RELATORIO == 0) {
            printf("Conexoes estabelecidas: %d de %d\n", abertos, i + 1);
            fflush(stdout);
        }
    }

    // mantem abertas: N clientes simultaneos
    printf("\nResultado: %d conexoes abertas, %d falhas.\n", abertos, falhas);
    printf("As %d conexoes permanecem ativas simultaneamente.\n", abertos);
    printf("Pressione ENTER para encerra-las...");
    fflush(stdout);

    if (fgets(tecla, sizeof(tecla), stdin) == NULL) {
        // entrada encerrada, fecha assim mesmo
    }

    for (i = 0; i < quantidade; i++) {
        if (sockets[i] >= 0) {
            close(sockets[i]);
        }
    }
    free(sockets);

    printf("\nTodas as conexoes foram encerradas.\n");
    return 0;
}
