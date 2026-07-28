/* ===========================================================================
 * comum.h - Constantes, tipos e textos do protocolo compartilhados por
 *           cliente e servidor.
 *
 * Trabalho de Redes de Computadores - UEMS
 * Plataforma de Monitoramento Inteligente de Filas em Tempo Real
 * Aluno: Victor Vendruscolo
 *
 * Este cabecalho e o unico ponto onde os valores "magicos" do sistema estao
 * definidos. Cliente e servidor incluem o mesmo arquivo, garantindo que os
 * dois lados falem exatamente o mesmo protocolo (a avaliacao verifica a
 * aderencia ao protocolo com captura de trafego).
 * ===========================================================================
 */

#ifndef COMUM_H
#define COMUM_H

/* ---------------------------------------------------------------------------
 * >>> ENDERECO DO SERVIDOR <<<
 *
 * ATENCAO: esta e a UNICA linha que precisa ser alterada para rodar o
 * servidor e os clientes em maquinas diferentes na mesma rede.
 *
 *   1. Na maquina que vai rodar ./servidor, execute:  hostname -I
 *   2. Troque o valor abaixo pelo IP mostrado (ex.: "192.168.0.15")
 *   3. Recompile os clientes com:  make
 *
 * O valor padrao "127.0.0.1" (loopback) permite testar cliente e servidor na
 * mesma maquina, sem nenhuma alteracao. Os programas continuam sendo
 * iniciados sem parametros (./cliente e ./servidor), como exige o enunciado.
 * ---------------------------------------------------------------------------
 */
#define SERVER_IP "127.0.0.1"

/* Porta de escuta do servidor (fixa, conforme os prints do enunciado). */
#define SERVER_PORTA 8080

/* --- Credenciais de acesso -------------------------------------------------
 * Usuario e senha ficam embutidos no codigo (decisao registrada na
 * especificacao: "deixar no codigo"). Nao ha validacao dinamica contra
 * arquivo ou banco de dados.
 */
#define LOGIN_USUARIO "admin"
#define LOGIN_SENHA   "admin123"

/* --- Limites do sistema ---------------------------------------------------- */

/* Capacidade maxima da fila de usuarios (array de tamanho fixo).
 * 20000 cobre com folga o teste de carga de 10000 clientes. */
#define MAX_USUARIOS_FILA 20000

/* Tamanho maximo do nome de um usuario, incluindo o terminador '\0'. */
#define TAM_NOME 50

/* Numero maximo de clientes autenticados simultaneamente (tabela de sessoes). */
#define MAX_SESSOES 12000

/* Tamanho maximo de uma linha do protocolo, incluindo '\n' e '\0'. */
#define TAM_LINHA 1024

/* Backlog do listen(): fila de conexoes ja chegadas aguardando accept(). */
#define BACKLOG 512

/* --- Parametros de retransmissao (item 4 da documentacao) ------------------ */

/* Tempo que o cliente espera pela resposta antes de reenviar o comando. */
#define TIMEOUT_RESPOSTA_SEG 3

/* Numero maximo de envios do mesmo comando (1 original + 2 reenvios). */
#define MAX_TENTATIVAS 3

/* --- Tipo abstrato de dado principal --------------------------------------- */

/*
 * Usuario: uma pessoa na fila de atendimento.
 * O enunciado chama esse registro de "paciente" nos prints de exemplo; o
 * texto do enunciado usa "usuario", terminologia adotada neste sistema.
 * Nao confundir com "cliente", que e a instancia do programa ./cliente.
 */
typedef struct {
    int  id;               /* identificador informado pelo operador     */
    char nome[TAM_NOME];   /* nome do usuario, terminado em '\0'        */
} Usuario;

/* --- Textos do protocolo ---------------------------------------------------
 * Definidos como constantes para que cliente e servidor nunca divirjam em
 * um caractere sequer. Estes sao os bytes que aparecem na captura de rede.
 */

/* Comandos enviados pelo cliente */
#define CMD_LOGIN     "LOGIN"
#define CMD_ADD       "ADD"
#define CMD_LIST      "LIST"
#define CMD_HEARTBEAT "HEARTBEAT"
#define CMD_SAIR      "SAIR"

/* Respostas enviadas pelo servidor */
#define RESP_LOGIN_OK   "LOGIN_OK"
#define RESP_LOGIN_FAIL "LOGIN_FAIL"
#define RESP_ADD_OK     "ADD_OK"
#define RESP_ALIVE      "ALIVE"
#define RESP_ERRO       "ERRO"

/* Delimitadores do bloco de fila (formato exato dos prints do enunciado) */
#define FILA_CABECALHO "===== FILA ====="
#define FILA_RODAPE    "================"

/* Mensagem assincrona de broadcast.
 *
 * Os textos que trafegam na rede sao mantidos em ASCII puro (sem acentos),
 * de modo que a captura de trafego mostre exatamente os mesmos bytes em
 * qualquer maquina, independentemente da codificacao do terminal. O cliente
 * reescreve a mensagem com a acentuacao correta apenas na hora de exibi-la
 * ao operador. */
#define PREFIXO_BROADCAST  "[Broadcast]"
#define BROADCAST_NOVO_USUARIO "[Broadcast] Novo usuario: "

#endif /* COMUM_H */
