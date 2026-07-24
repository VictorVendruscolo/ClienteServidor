// Este servidor utiliza a funcao fork()  para gerenciar multiplos clientes.
// Ao se fazer a chamada do sistema fork() cria-se uma duplicada exata do programa e, um novo processo filho e iniciado para essa copia.

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
// A bibliotca <sys/wait.h> e a biblioteca <signal.h> sao necessarias para evitar a criacao de zombies.
// Zombies sao processos filhos que aparecem quando processos pais deixam de existir sem ser feita a chamada wait() ou waitpid() nos processos filhos.

void sigchld_handler(int signo)
// Manioulador de sinais. Ele simplesmente faz a chamada waitpid para todo filho que for desconectado.
{
    while (waitpid(-1, NULL, WNOHANG) > 0) //-1 desconecxao, NULL flag, "" //maior que 0 existe processo, senao elimina//
    // A ideia de se chamar em um laco e que nao se tem certeza que ha uma correlacao um-para-um entre os filhos desconectados e
    // as chamadas ao manipulador de sinais.
    // Vale lembrar que o posix nao permite a criacao de filas nas chamadas de sinal. //Se aparecer -1 elimina//
    // Ou seja, pode acontecer de chamar o manipulador apos varios filhos ja terem sido desconecatdos.
    {
        /* code */
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in sAddr;
    int meu_socket;
    int novo_socket;
    char buffer[25];
    int resultado, leitor, pid, valor;

    meu_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    valor = 1;
    // SO_REUSEADOR significa que as regras utilizadas para a validacao de endereco feita pelo bind permite a reutilizacao de endereco locais.
    resultado = setsockopt(meu_socket, SOL_SOCKET, SO_REUSEADDR, &valor, sizeof(valor));
    if (resultado < 0)
    {
        perror("Erro: setsockopt");
        return 0;
    }
    // Uso da bind para associar a porta com tdos os enderecos.
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(1972);
    sAddr.sin_addr.s_addr = INADDR_ANY;

    resultado = bind(meu_socket, (struct sockaddr *)&sAddr, sizeof(sAddr));
    if (resultado < 0)
    {
        perror("Erro: bind");
        return 0;
    }
    // Colocando o socket para ouvir a chegada de conexoes.
    resultado = listen(meu_socket, 5);
    if (resultado < 0)
    {
        perror("Erro: listen");
        return 0;
    }
    signal(SIGCHLD, sigchld_handler);
    // Ativando o manipulador de sinais antes de entrar no laco.
    while (1)
    {
        novo_socket = accept(meu_socket, NULL, NULL); // Aceitar a conexao//
        // Antes da chamada ser aceita (retornada), chama-se o fork() para a criacao de novos processos.
        if ((pid = fork()) == 0)
        {
            // Se retornar 0 e porque estamos no processo inicial. Caso contrario, retorna o PID do novo processo filho.
            printf("Processo filho %i criado.\n", getpid());
            close(meu_socket);
            // Uma vez com o processo filho, fecha-se o processo listen.
            // Lembre-se que todos os processos filhos sao copiados do processo pai.
            leitor = recv(novo_socket, buffer, 25, 0);
            buffer[leitor] = '\0';
            printf("%s\n", buffer);
            send(novo_socket, buffer, leitor, 0); // Leitor que esta mandando//
            close(novo_socket);
            // Esta linha 79 so e alcancada no processo pai. Uma vez que o processo filho tem uma copia do socket cliente.
            // O processo pai faz a sua referencia e decrementa o contador no kernel.
            printf("Processo filho %i terminado.\n", getpid());
            exit(0);
        }
        close(novo_socket);
    }
}