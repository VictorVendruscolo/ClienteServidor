#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{

    int meu_socket, novo_socket, c;
    struct sockaddr_in servidor, cliente;

    // Criar um socket //UM SERVIDOR UM CLIENTE
    meu_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (meu_socket == -1)
    {
        printf("Erro ao criar o socket");
    }

    // Preparando a estrutura sockaddr_in para o servidor
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(8888);

    // bind o socket ao endereço e porta do servidor
    if (bind(meu_socket, (struct sockaddr *)&servidor, sizeof(servidor)) < 0)
    {
        puts("Erro no bind");
    }

    puts("Bind execuando corretamente!\n");

    // Listen - ouvindo conexoes
    listen(meu_socket, 3); // 3 é o número máximo de conexões pendentes na fila

    // Aceitando conexoes
    puts("Aguardando chegada deconexoes...");
    c = sizeof(struct sockaddr_in); // tamanho da estrutura sockaddr_in - entrada do endereco socket
    novo_socket = accept(meu_socket, (struct sockaddr *)&cliente, (socklen_t *)&c);
    if (novo_socket < 0)
    {
        perror("Erro ao aceitar conexão!");
    }

    puts("Conexão aceita!");

    return 0;
}