#include <stdio.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write

int main()
{

    int meu_socket, novo_socket, c;
    struct sockaddr_in servidor, cliente;
    char *mensagem;

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
    puts("Aguardando chegada de conexoes...");
    c = sizeof(struct sockaddr_in); // tamanho da estrutura sockaddr_in - entrada do endereco socket
    novo_socket = accept(meu_socket, (struct sockaddr *)&cliente, (socklen_t *)&c);
    if (novo_socket < 0)
    {
        perror("Erro ao aceitar conexão!");
        return 1;
    }

    puts("Conexão aceita!");

    // Respondendo ao cliente
    mensagem = "E ai meu chapa. Acabei de receber a sua conexao. Mas vou indo falows\n";
    write(novo_socket, mensagem, strlen(mensagem));

    return 0;
}