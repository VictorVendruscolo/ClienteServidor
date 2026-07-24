#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{

    int meu_socket;
    struct sockaddr_in6 servidor;
    char *ipv6_addr = "2800:3f0:4001:80b::200e";
    char *mensagem, resposta_servidor[2000];

    // criar o socket IPv6
    meu_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (meu_socket == -1)
    {
        perror("Erro ao criar o socket");
        exit(1);
    }
    // configurar o endereço do servidor
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin6_family = AF_INET6;
    servidor.sin6_port = htons(80);

    // converter o endereço IPv6 de string para formato binário
    if (inet_pton(AF_INET6, ipv6_addr, &servidor.sin6_addr) <= 0)
    {
        perror("Endereço IPv6 inválido");
        exit(1);
    }

    // conectar ao servidor remoto
    if (connect(meu_socket, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("Erro ao conectar ao servidor");
        exit(1);
    }

    // enviar mensagem para o servidor
    mensagem = "GET / HTTP/1.1\r\nHost: [2800:3f0:4001:80b::200e]\r\n\r\n";
    if (send(meu_socket, mensagem, strlen(mensagem), 0) == -1)
    {
        perror("Erro ao enviar mensagem");
        exit(1);
    }

    // receber resposta do servidor
    if (recv(meu_socket, resposta_servidor, sizeof(resposta_servidor) - 1, 0) == -1)
    {
        perror("Erro ao receber resposta");
        exit(1);
    }

    // imprimir resposta do servidor
    printf("Resposta do servidor:\n%s\n", resposta_servidor);

    // fechar o socket
    close(meu_socket);

    return 0;
}