// Servidor multithreading
// Compilar usando gcc servidor.c -o servidor -lpthread

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

void *thread_proc(void *arg);

int main(int argc, char *argv[])
{
    struct sockaddr_in sAddr;
    int listensock;
    int novo_socket;
    int resultado;
    pthread_t thread_id;
    int valor;

    listensock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    valor = 1;
    resultado = setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &valor, sizeof(valor));
    if (resultado < 0)
    {
        perror("Erro: setsockopt");
        return 0;
    }
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(1972);
    sAddr.sin_addr.s_addr = INADDR_ANY;

    resultado = bind(listensock, (struct sockaddr *)&sAddr, sizeof(sAddr));
    resultado = listen(listensock, 5);
    if (resultado < 0)
    {
        perror("Erro: listen");
        return 0;
    }

    while (1)
    {
        novo_socket = accept(listen, NULL, NULL);
        // Uma vez que o cliente conecta, uma nova thread e iniciada.
        // O descritor para esse novo cliente socket e passada para a funcao thread.
        // Apos passado para a funcao, nao ha necessidade da thread pai chegar o descritor.
        resultado = pthread_create(&thread_id, NULL, thread_proc, (void *)novo_socket);
        if (resultado != 0)
        {
            printf("Erro ao criar a thread!\n");
            return 0;
        }
        pthread_detach(thread_id); // Desatacha a thread, tira a fixacao da theread//
        // Usado para evitar a criacao de threads zombies.
        sched_yield();
        // Usada para dar a thread a chance de inicializar a execucao, desistindo do restante do restante do intervalo de tempo alocado dos pais.
    }
}

void *thread_proc(void *arg)
{
    int sock;
    char buffer[25];
    int n_lidos;
    printf("Thread filha %i com pid %i criada.\n", pthread_self(), getpid());
    sock = (int) arg;
    n_lidos = recv(sock, buffer, 25, 0);
    buffer[n_lidos] = '\0';
    printf("%s\n", buffer);
    send(sock, buffer, n_lidos, 0);
    close(sock);
    printf("Thread filha %i com pid %i finalizada.\n", pthread_self(), getpid());
}