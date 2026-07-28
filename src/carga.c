/* ===========================================================================
 * carga.c - Gerador automatico de clientes para o teste de carga.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 *
 * Compilacao:  make carga
 * Execucao:    ./carga <quantidade> [login]
 *
 * Exemplos:
 *      ./carga 100
 *      ./carga 1000
 *      ./carga 10000 login
 *
 * O enunciado exige testar o servidor com 100, 1000 e 10000 clientes, com
 * geracao automatica, e permite expressamente que isso seja feito por um
 * segundo programa cliente que receba a quantidade como parametro. Este e
 * esse programa - ele NAO substitui o ./cliente, que continua sendo iniciado
 * sem parametros.
 *
 * A estrutura segue o exemplo de laco de conexoes visto em aula (porta.c),
 * trocando "variar a porta" por "repetir N vezes na mesma porta".
 *
 * Todas as conexoes sao mantidas ABERTAS ao mesmo tempo e o programa aguarda
 * ENTER antes de encerra-las: e isso que permite capturar uma tela em que
 * todas as conexoes simultaneas estejam visiveis, como pede o enunciado.
 *
 * Sem o argumento "login", cada cliente apenas estabelece a conexao TCP
 * (mede a capacidade de aceitacao do servidor). Com "login", cada cliente
 * tambem se autentica e passa a ocupar uma sessao no servidor.
 * ===========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comum.h"
#include "protocolo.h"

/* De quantas em quantas conexoes o progresso e informado na tela. */
#define INTERVALO_RELATORIO 100

/* ---------------------------------------------------------------------------
 * amplia_limite_descritores
 *
 * Cada conexao consome um descritor de arquivo. O limite padrao costuma ser
 * 1024, insuficiente para o teste de 10000 clientes. Esta funcao eleva o
 * limite flexivel ate o limite rigido permitido ao usuario, sem exigir
 * privilegios de administrador.
 *
 * Retorna o novo limite efetivo.
 * ---------------------------------------------------------------------------
 */
static long amplia_limite_descritores(void)
{
    struct rlimit limite;

    if (getrlimit(RLIMIT_NOFILE, &limite) != 0) {
        return -1;
    }

    if (limite.rlim_cur < limite.rlim_max) {
        limite.rlim_cur = limite.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &limite) != 0) {
            /* Mantem o limite anterior; o programa avisara se faltar espaco. */
            getrlimit(RLIMIT_NOFILE, &limite);
        }
    }

    return (long) limite.rlim_cur;
}

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
    int    *sockets;
    long    limite_descritores;
    int     quantidade;
    int     com_login = 0;
    int     abertos   = 0;
    int     falhas    = 0;
    int     i;
    time_t  inicio;
    double  duracao;
    char    tecla[8];

    /* Escrever num socket que o servidor fechou nao deve derrubar o teste. */
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Uso: %s <quantidade> [login]\n", argv[0]);
        fprintf(stderr, "Exemplos: %s 100 | %s 1000 | %s 10000 login\n",
                argv[0], argv[0], argv[0]);
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

    limite_descritores = amplia_limite_descritores();

    printf("=== Teste de carga ===\n");
    printf("Servidor .................: %s:%d\n", SERVER_IP, SERVER_PORTA);
    printf("Clientes solicitados .....: %d\n", quantidade);
    printf("Autenticacao .............: %s\n", com_login ? "sim" : "nao (apenas conexao)");
    printf("Limite de descritores ....: %ld\n", limite_descritores);

    if (limite_descritores > 0 && quantidade + 16 > limite_descritores) {
        printf("AVISO: o limite de descritores pode ser insuficiente.\n");
        printf("       Execute 'ulimit -n %d' antes de rodar o teste.\n",
               quantidade + 64);
    }
    printf("\n");

    sockets = (int *) malloc((size_t) quantidade * sizeof(int));
    if (sockets == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para %d conexoes.\n", quantidade);
        return 1;
    }

    inicio = time(NULL);

    for (i = 0; i < quantidade; i++) {
        int sock = conecta();

        if (sock < 0) {
            falhas++;
            sockets[i] = -1;

            /* Na primeira falha, mostra o motivo: quase sempre e limite de
             * descritores, esgotamento de portas ou servidor fora do ar. */
            if (falhas == 1) {
                printf("Primeira falha na conexao %d: %s\n", i + 1, strerror(errno));
            }
            continue;
        }

        if (com_login && !autentica(sock)) {
            close(sock);
            sockets[i] = -1;
            falhas++;
            continue;
        }

        sockets[i] = sock;
        abertos++;

        if ((i + 1) % INTERVALO_RELATORIO == 0) {
            printf("Conexoes estabelecidas: %d de %d\n", abertos, i + 1);
            fflush(stdout);
        }
    }

    duracao = difftime(time(NULL), inicio);

    printf("\n=== Resultado ===\n");
    printf("Conexoes solicitadas .....: %d\n", quantidade);
    printf("Conexoes bem-sucedidas ...: %d\n", abertos);
    printf("Falhas ...................: %d\n", falhas);
    printf("Tempo total ..............: %.0f segundo(s)\n", duracao);
    printf("\nAs %d conexoes estao ABERTAS neste momento.\n", abertos);
    printf("Verifique a tela do servidor e capture a imagem agora.\n");
    printf("Pressione ENTER para fechar todas as conexoes...");
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
