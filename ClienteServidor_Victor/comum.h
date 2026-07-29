/*
 * comum.h - Constantes, tipo Usuario e textos do protocolo.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Plataforma de Monitoramento Inteligente de Filas em Tempo Real
 * Aluno: Victor Vendruscolo
 *
 * Cliente e servidor incluem este mesmo arquivo, para os dois lados usarem
 * exatamente os mesmos valores e os mesmos textos de mensagem.
 */

#ifndef COMUM_H
#define COMUM_H

/*
 * ENDERECO DO SERVIDOR
 *
 * Esta e a unica linha que precisa ser alterada para rodar o servidor e os
 * clientes em maquinas diferentes na mesma rede:
 *
 *   1. na maquina do servidor, rode:  hostname -I
 *   2. troque o valor abaixo pelo IP mostrado
 *   3. recompile com:  make
 *
 * O valor 127.0.0.1 permite testar tudo em uma maquina so.
 */
#define SERVER_IP "127.0.0.1"

/* Porta do servidor. */
#define SERVER_PORTA 8080

/* Usuario e senha ficam no proprio codigo, sem validacao contra arquivo. */
#define LOGIN_USUARIO "admin"
#define LOGIN_SENHA   "admin123"

/* Capacidade maxima da fila. */
#define MAX_USUARIOS_FILA 20000

/* Tamanho maximo do nome, contando o '\0'. */
#define TAM_NOME 50

/* Maximo de clientes autenticados ao mesmo tempo. */
#define MAX_SESSOES 12000

/* Tamanho maximo de uma linha do protocolo. */
#define TAM_LINHA 1024

/* Fila de conexoes pendentes do listen(). */
#define BACKLOG 512

/* Segundos que o cliente espera a resposta antes de reenviar o comando. */
#define TIMEOUT_RESPOSTA_SEG 3

/* Quantas vezes o mesmo comando pode ser enviado (1 original + 2 reenvios). */
#define MAX_TENTATIVAS 3

/*
 * Um usuario da fila de atendimento.
 * O enunciado usa "paciente" nos prints e "usuario" no texto; adotei
 * "usuario". Nao confundir com "cliente", que e o programa ./cliente.
 */
typedef struct {
    int  id;               /* identificador digitado pelo operador */
    char nome[TAM_NOME];   /* nome, terminado em '\0'              */
} Usuario;

/* Comandos que o cliente envia. */
#define CMD_LOGIN     "LOGIN"
#define CMD_ADD       "ADD"
#define CMD_LIST      "LIST"
#define CMD_HEARTBEAT "HEARTBEAT"
#define CMD_SAIR      "SAIR"

/* Respostas que o servidor envia. */
#define RESP_LOGIN_OK   "LOGIN_OK"
#define RESP_LOGIN_FAIL "LOGIN_FAIL"
#define RESP_ADD_OK     "ADD_OK"
#define RESP_ALIVE      "ALIVE"
#define RESP_ERRO       "ERRO"

/* Cabecalho e rodape do bloco de fila. */
#define FILA_CABECALHO "===== FILA ====="
#define FILA_RODAPE    "================"

/*
 * Mensagem de broadcast. O texto trafega sem acento, em ASCII puro, para os
 * bytes serem iguais em qualquer maquina; o cliente acentua ao exibir.
 */
#define PREFIXO_BROADCAST  "[Broadcast]"
#define BROADCAST_NOVO_USUARIO "[Broadcast] Novo usuario: "

#endif /* COMUM_H */
