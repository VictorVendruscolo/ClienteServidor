============================================================================
TRABALHO DE REDES DE COMPUTADORES
Plataforma de Monitoramento Inteligente de Filas em Tempo Real
Universidade Estadual de Mato Grosso do Sul - Ciencia da Computacao
============================================================================

ALUNO
-----
Victor Vendruscolo


COMPILACAO
----------
No diretorio onde estao os arquivos fonte, execute:

    make

Esse comando gera os dois programas: cliente e servidor.


EXECUCAO
--------
1) Na maquina servidora:

    ./servidor

2) Em cada maquina cliente (uma execucao por cliente):

    ./cliente

Nenhum dos dois programas recebe parametros. O cliente descobre
automaticamente o endereco e a porta do servidor a partir das constantes
SERVER_IP e SERVER_PORTA definidas no arquivo comum.h.


CONFIGURACAO DO ENDERECO DO SERVIDOR
------------------------------------
Para rodar o servidor e os clientes em maquinas diferentes na mesma rede:

1. Na maquina que executara o servidor, descubra o IP:

       hostname -I

2. Edite a primeira constante do arquivo comum.h:

       #define SERVER_IP "192.168.0.15"     <- coloque o IP obtido acima

3. Recompile os clientes:

       make

O valor padrao e "127.0.0.1", que permite rodar cliente e servidor na mesma
maquina sem qualquer alteracao.


TESTE DE CARGA (100, 1000 e 10000 CLIENTES)
-------------------------------------------
O gerador automatico de clientes e um programa a parte, compilado por um
alvo proprio para nao alterar o resultado do "make" sem parametros:

    make carga

Execucao (com o servidor ja rodando):

    ./carga 100
    ./carga 1000
    ./carga 10000

O programa abre a quantidade pedida de conexoes simultaneas, mantem todas
abertas e aguarda ENTER, permitindo capturar a tela do servidor com todas as
conexoes visiveis. Para que os clientes tambem se autentiquem e ocupem uma
sessao no servidor, acrescente a palavra "login":

    ./carga 10000 login

Antes do teste com 10000 clientes, eleve o limite de descritores de arquivo
do terminal:

    ulimit -n 20000


ARQUIVOS FONTE
--------------
    comum.h          constantes, tipo Usuario e textos do protocolo
    protocolo.c/.h   envio e recepcao de mensagens delimitadas por linha
    fila.c/.h        fila compartilhada de usuarios, protegida por mutex
    sessoes.c/.h     registro de clientes conectados e envio de broadcast
    persistencia.c/.h gravacao dos dados em arquivos texto
    servidor.c       programa servidor (aceita conexoes, uma thread por cliente)
    cliente.c        programa cliente (menu de operacao)
    carga.c          gerador automatico de clientes para o teste de carga
    Makefile         regras de compilacao


ARQUIVOS DE DADOS GERADOS EM EXECUCAO
-------------------------------------
O servidor cria automaticamente o subdiretorio "dados/" com:

    dados/servidor.log    eventos do servidor, com data e hora
    dados/sessoes.log     entrada e saida de cada cliente
    dados/fila.txt        retrato atual da fila
    dados/historico.txt   registro permanente de todas as insercoes


LIMPEZA
-------
    make clean

Remove os executaveis e os arquivos objeto.
