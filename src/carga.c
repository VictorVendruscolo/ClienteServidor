/* ===========================================================================
 * carga.c - Gerador automatico de clientes para o teste de carga.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Compilacao:  make carga
 * Execucao:    ./carga <quantidade> [login]
 *
 * Exemplos:    ./carga 100
 *              ./carga 1000
 *              ./carga 10000 login
 *
 * O enunciado exige testar o servidor com 100, 1000 e 10000 clientes, com
 * geracao automatica, e permite expressamente que isso seja feito por um
 * segundo programa cliente que receba a quantidade como parametro. Este e
 * esse programa - ele NAO substitui o ./cliente, que continua sendo iniciado
 * sem parametros.
 *
 * A estrutura segue o laco de conexoes visto em aula (porta.c), trocando
 * "variar a porta" por "repetir N vezes na mesma porta".
 *
 * Todas as conexoes sao mantidas ABERTAS ao mesmo tempo e o programa aguarda
 * ENTER antes de encerra-las: e isso que permite capturar uma tela em que
 * todas as conexoes simultaneas estejam visiveis, como pede o enunciado.
 *
 * Sem o argumento "login", cada cliente apenas estabelece a conexao TCP.
 * Com "login", cada cliente tambem se autentica e ocupa uma sessao.
 *
 * Antes do teste com muitos clientes, eleve o limite de descritores do
 * terminal:   ulimit -n 20000
 * ===========================================================================
 */

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

/* De quantas em quantas conexoes o progresso e informado na tela. */
#define INTERVALO_RELATORIO 100

/*
 * Abre uma conexao com o servidor.
 * Retorna o socket conectado ou -1 em caso de falha.
 */
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

/*
 * Envia a credencial fixa e confere a resposta do servidor.
 * Retorna 1 se o acesso foi liberado.
 */
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
    int  com_login = 0;
    int  abertos   = 0;
    int  falhas    = 0;
    int  i;
    char tecla[8];

    /* Escrever num socket que o servidor fechou nao deve derrubar o teste. */
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Uso: %s <quantidade> [login]\n", argv[0]);
        return 1;
    }

    quantidade = atoi(argv[1]);
    if (quantidade <= 0) {
        fprintf(stderr, "Erro: a quantidade deve ser um numero inteiro positivo.\n");
        return 1;
    }

    if (argc == 3) {
        if (strcmp(argv[2], "login") != 0) {
            fprintf(stderr, "Erro: o segundo parametro, se usado, deve ser \"login\".\n");
            return 1;
        }
        com_login = 1;
    }

    sockets = (int *) malloc((size_t) quantidade * sizeof(int));
    if (sockets == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para %d conexoes.\n", quantidade);
        return 1;
    }

    printf("Abrindo %d conexoes em %s:%d%s\n\n", quantidade, SERVER_IP,
           SERVER_PORTA, com_login ? " (com autenticacao)" : "");

    for (i = 0; i < quantidade; i++) {
        int sock = conecta();

        if (sock >= 0 && com_login && !autentica(sock)) {
            close(sock);
            sock = -1;
        }

        sockets[i] = sock;

        if (sock >= 0) {
            abertos++;
        } else {
            falhas++;
            /* Na primeira falha, mostra o motivo: quase sempre e limite de
             * descritores (use ulimit -n) ou servidor fora do ar. */
            if (falhas == 1) {
                printf("Primeira falha na conexao %d: %s\n", i + 1, strerror(errno));
            }
        }

        if ((i + 1) % INTERVALO_RELATORIO == 0) {
            printf("Conexoes estabelecidas: %d de %d\n", abertos, i + 1);
            fflush(stdout);
        }
    }

    printf("\nResultado: %d conexoes abertas, %d falhas.\n", abertos, falhas);
    printf("As conexoes estao ABERTAS. Capture a tela do servidor agora.\n");
    printf("Pressione ENTER para fecha-las...");
    fflush(stdout);

    if (fgets(tecla, sizeof(tecla), stdin) == NULL) {
        /* Entrada encerrada: prossegue com o fechamento assim mesmo. */
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
