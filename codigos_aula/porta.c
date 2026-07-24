// Programando com port scan

#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int main(int argc, char **argv)
{

    struct hostent *host;
    int busca, i, sock, inicio, fim;
    char nome_maquina[100];
    struct sockaddr_in servidor;

    // Nome da maquina para escanear
    printf("Entre com o nome da maquina ou seu IP: ");
    fgets(nome_maquina, 100, stdin);

    // Porta inicio de escaneamento
    printf("\nEntre com o numero inicial da porta: ");
    scanf("%d", &inicio);

    // Porta final de escaneamento
    printf("\nEntre com o numero final da porta: ");
    scanf("%d", &fim);

    // Inicializando a estrutura sockaddr_in
    strncpy((char *)&servidor, "", sizeof(servidor));
    servidor.sin_family = AF_INET;

    // Uso do endereco IP direto
    if (isdigit(nome_maquina[0]))
    {
        printf("Executando o inet_addre...");
        servidor.sin_addr.s_addr = inet_addr(nome_maquina);
        printf("Concluido!\n");
    }
    // Resolvendo o nome damaquina (hotname) para endereco IP
    else if ((host = getservbyname(nome_maquina)) != 0)
    {
        printf("Executando o getservbyname...");
        strncpy((char *)&servidor.sin_addr, (char *)host->h_addr, sizeof(servidor.sin_addr.s_addr));
        printf("\nConcluido!");
    }
    else
    {
        herror(nome_maquina);
        exit(2);
    }

    // Comecando a escanear as portas. Fazer loop
    printf("Inicio do escaneamento de portas: \n");
    for (i = inicio; i <= fim; i++)
    {
        // Preencher o numero da porta
        servidor.sin_port = htons(i); // Numero da porta: 1

        // Criar uma conexao socket do tipo IP
        sock = socket(AF_INET, SOCK_STREAM, 0);

        // Testar se a conexao funcionou
        if (sock < 0)
        {
            perror("\nFalha na conexao socket!");
            exit(1);
        }

        // Conectar usando socket e a estrutura sockaddr_in
        busca = connect(sock, (struct sockaddr *)&servidor, sizeof(servidor));
        if (busca < 0)
        {
            printf("Falha na funcao connect! - Porta fechada %d\n", i);
            fflush(stdout);
        }
        else
        {
            printf("Porta aberta %d\n", i);
        }
        close(sock);
    }
    printf("\r");
    fflush(stdout);
    return 0;
}