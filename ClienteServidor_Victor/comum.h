#ifndef COMUM_H
#define COMUM_H

// Endereco do servidor. Para rodar em maquinas diferentes: pegar o IP com
// "hostname -I" na maquina do servidor, trocar aqui e recompilar com make.
#define SERVER_IP "127.0.0.1"

#define SERVER_PORTA 8080

// Credencial fixa no codigo, sem validacao contra arquivo.
#define LOGIN_USUARIO "admin"
#define LOGIN_SENHA   "admin123"

#define MAX_USUARIOS_FILA 20000   // capacidade da fila
#define TAM_NOME 50               // tamanho do nome, contando o '\0'
#define MAX_SESSOES 12000         // clientes autenticados ao mesmo tempo
#define TAM_LINHA 1024            // tamanho maximo de uma linha do protocolo
#define BACKLOG 512               // conexoes pendentes no listen()

#define TIMEOUT_RESPOSTA_SEG 3    // espera pela resposta antes de reenviar
#define MAX_TENTATIVAS 3          // envios do mesmo comando (1 + 2 reenvios)

// Usuario da fila. O enunciado usa "paciente" nos prints e "usuario" no
// texto; adotei usuario. Nao confundir com "cliente", que e o programa.
typedef struct {
    int  id;
    char nome[TAM_NOME];
} Usuario;

// Comandos do cliente
#define CMD_LOGIN     "LOGIN"
#define CMD_ADD       "ADD"
#define CMD_LIST      "LIST"
#define CMD_HEARTBEAT "HEARTBEAT"
#define CMD_SAIR      "SAIR"

// Respostas do servidor
#define RESP_LOGIN_OK   "LOGIN_OK"
#define RESP_LOGIN_FAIL "LOGIN_FAIL"
#define RESP_ADD_OK     "ADD_OK"
#define RESP_ALIVE      "ALIVE"
#define RESP_ERRO       "ERRO"

#define FILA_CABECALHO "===== FILA ====="
#define FILA_RODAPE    "================"

// Broadcast trafega sem acento (ASCII puro); o cliente acentua ao exibir.
#define PREFIXO_BROADCAST  "[Broadcast]"
#define BROADCAST_NOVO_USUARIO "[Broadcast] Novo usuario: "

#endif
