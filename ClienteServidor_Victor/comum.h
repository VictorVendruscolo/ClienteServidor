#ifndef COMUM_H
#define COMUM_H

// IP do servidor (ver readme)
#define SERVER_IP "127.0.0.1"

#define SERVER_PORTA 8080

// credencial fixa no codigo
#define LOGIN_USUARIO "admin"
#define LOGIN_SENHA   "admin123"

// limites
#define MAX_USUARIOS_FILA 20000
#define TAM_NOME 50
#define MAX_SESSOES 12000
#define TAM_LINHA 1024
#define BACKLOG 512

// retransmissao
#define TIMEOUT_RESPOSTA_SEG 3
#define MAX_TENTATIVAS 3

// usuario da fila (o "paciente" dos prints)
typedef struct {
    int  id;
    char nome[TAM_NOME];
} Usuario;

// comandos
#define CMD_LOGIN     "LOGIN"
#define CMD_ADD       "ADD"
#define CMD_LIST      "LIST"
#define CMD_HEARTBEAT "HEARTBEAT"
#define CMD_SAIR      "SAIR"

// respostas
#define RESP_LOGIN_OK   "LOGIN_OK"
#define RESP_LOGIN_FAIL "LOGIN_FAIL"
#define RESP_ADD_OK     "ADD_OK"
#define RESP_ALIVE      "ALIVE"
#define RESP_ERRO       "ERRO"

// delimitadores da fila
#define FILA_CABECALHO "===== FILA ====="
#define FILA_RODAPE    "================"

// broadcast sem acento, cliente acentua
#define PREFIXO_BROADCAST  "[Broadcast]"
#define BROADCAST_NOVO_USUARIO "[Broadcast] Novo usuario: "

#endif
