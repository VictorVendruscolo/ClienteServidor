/*
 * carga.c - Gerador automatico de clientes para o teste de carga.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Compilacao:  make carga
 * Execucao:    ./carga <quantidade>
 *
 * Exemplos:    ./carga 100
 *              ./carga 1000
 *              ./carga 10000
 *
 * O enunciado pede o teste com 100, 1000 e 10000 clientes gerados
 * automaticamente, e permite fazer isso com um segundo programa cliente que
 * receba a quantidade como parametro. Este e esse programa; ele nao
 * substitui o ./cliente, que continua sendo iniciado sem parametros.
 *
 * A estrutura e a do laco de conexoes visto em aula (porta.c), trocando
 * "variar a porta" por "repetir N vezes na mesma porta". Cada cliente gerado
 * conecta e se autentica, ocupando uma sessao no servidor igual a um
 * ./cliente de verdade.
 *
 * As conexoes ficam abertas ate o operador apertar ENTER. Isso e essencial:
 * sem manter, o programa estaria abrindo e fechando N conexoes em sequencia
 * e o servidor nunca atenderia mais de uma por vez. Mantendo todas ativas,
 * ele precisa sustentar N clientes simultaneos, que e o que o teste mede.
 *
 * Reinicie o servidor antes de cada execucao, para a numeracao das conexoes
 * na tela dele comecar em [#1].
 *
 * Antes do teste com muitos clientes, aumente o limite de descritores do
 * terminal:   ulimit -n 20000
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

/* De quantas em quantas conexoes o progresso aparece na tela. */
#define INTERVALO_RELATORIO 100

/* Abre uma conexao. Devolve o socket ou -1 se falhar. */
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

/* Manda a credencial e confere a resposta. Devolve 1 se foi liberado. */
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

    /* Escrever num socket que o servidor fechou nao deve derrubar o teste. */
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
            /* Na primeira falha mostra o motivo: quase sempre e limite de
             * descritores (ver ulimit -n) ou servidor fora do ar. */
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
    printf("As %d conexoes permanecem ativas simultaneamente.\n", abertos);
    printf("Pressione ENTER para encerra-las...");
    fflush(stdout);

    if (fgets(tecla, sizeof(tecla), stdin) == NULL) {
        /* Entrada encerrada: fecha assim mesmo. */
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
