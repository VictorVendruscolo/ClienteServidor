/* ===========================================================================
 * persistencia.c - Implementacao da gravacao em arquivo (ver persistencia.h).
 *
 * Trabalho de Redes de Computadores - UEMS
 * Aluno: Victor Vendruscolo
 * ===========================================================================
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "persistencia.h"
#include "comum.h"
#include "fila.h"

#define DIR_DADOS       "dados"
#define ARQ_SERVIDOR    "dados/servidor.log"
#define ARQ_SESSOES     "dados/sessoes.log"
#define ARQ_HISTORICO   "dados/historico.txt"
#define ARQ_FILA        "dados/fila.txt"

/* Quantos usuarios sao copiados da fila por vez ao regravar o snapshot.
 * Manter esse lote pequeno evita alocar um vetor grande na pilha da thread. */
#define LOTE_GRAVACAO 64

/* Arquivos mantidos abertos durante toda a execucao: abrir e fechar a cada
 * evento seria caro sob carga alta (milhares de conexoes por segundo). */
static FILE *arq_servidor  = NULL;
static FILE *arq_sessoes   = NULL;
static FILE *arq_historico = NULL;

/* Um unico mutex protege toda a gravacao. Como cada escrita e curta e seguida
 * de fflush, uma trava so e suficiente e mantem o modulo simples. */
static pthread_mutex_t mutex_dados = PTHREAD_MUTEX_INITIALIZER;

/* ---------------------------------------------------------------------------
 * agora_texto
 *
 * Preenche 'destino' com a data e hora atuais no formato
 * "AAAA-MM-DD HH:MM:SS". Usa localtime_r (versao segura para threads).
 * ---------------------------------------------------------------------------
 */
static void agora_texto(char *destino, size_t tam)
{
    time_t    agora = time(NULL);
    struct tm quebrado;

    if (localtime_r(&agora, &quebrado) == NULL) {
        snprintf(destino, tam, "0000-00-00 00:00:00");
        return;
    }
    strftime(destino, tam, "%Y-%m-%d %H:%M:%S", &quebrado);
}

int persistencia_init(void)
{
    /* mkdir falha com EEXIST se o diretorio ja existir - isso nao e erro. */
    if (mkdir(DIR_DADOS, 0755) != 0 && errno != EEXIST) {
        perror("Erro ao criar o diretorio dados/");
        return -1;
    }

    arq_servidor  = fopen(ARQ_SERVIDOR,  "a");
    arq_sessoes   = fopen(ARQ_SESSOES,   "a");
    arq_historico = fopen(ARQ_HISTORICO, "a");

    if (arq_servidor == NULL || arq_sessoes == NULL || arq_historico == NULL) {
        perror("Erro ao abrir os arquivos de dados");
        return -1;
    }
    return 0;
}

void persistencia_log_servidor(const char *formato, ...)
{
    char    mensagem[TAM_LINHA];
    char    instante[32];
    va_list argumentos;

    va_start(argumentos, formato);
    vsnprintf(mensagem, sizeof(mensagem), formato, argumentos);
    va_end(argumentos);

    agora_texto(instante, sizeof(instante));

    pthread_mutex_lock(&mutex_dados);

    /* Terminal: exatamente o texto das telas de funcionamento do enunciado. */
    printf("%s\n", mensagem);
    fflush(stdout);

    /* Arquivo: o mesmo texto, precedido de data e hora. */
    if (arq_servidor != NULL) {
        fprintf(arq_servidor, "[%s] %s\n", instante, mensagem);
        fflush(arq_servidor);
    }

    pthread_mutex_unlock(&mutex_dados);
}

void persistencia_log_sessao(const char *evento, const char *ip)
{
    char instante[32];

    agora_texto(instante, sizeof(instante));

    pthread_mutex_lock(&mutex_dados);
    if (arq_sessoes != NULL) {
        fprintf(arq_sessoes, "%s %s %s\n",
                evento,
                (ip != NULL && ip[0] != '\0') ? ip : "desconhecido",
                instante);
        fflush(arq_sessoes);
    }
    pthread_mutex_unlock(&mutex_dados);
}

void persistencia_historico_add(int id, const char *nome)
{
    char instante[32];

    agora_texto(instante, sizeof(instante));

    pthread_mutex_lock(&mutex_dados);
    if (arq_historico != NULL) {
        fprintf(arq_historico, "%s ADD %d %s\n", instante, id, nome);
        fflush(arq_historico);
    }
    pthread_mutex_unlock(&mutex_dados);
}

void persistencia_salva_fila(void)
{
    Usuario lote[LOTE_GRAVACAO];
    FILE   *arquivo;
    int     posicao = 0;
    int     copiados;

    pthread_mutex_lock(&mutex_dados);

    /* Este arquivo e um retrato do estado atual, entao e reescrito por
     * completo ("w") a cada insercao, e nao acrescentado. */
    arquivo = fopen(ARQ_FILA, "w");
    if (arquivo == NULL) {
        pthread_mutex_unlock(&mutex_dados);
        return;
    }

    fprintf(arquivo, "%s\n", FILA_CABECALHO);

    /* A fila e lida em lotes: cada chamada trava o mutex da fila por um
     * intervalo curto, em vez de mante-lo travado durante toda a gravacao. */
    do {
        int i;

        copiados = fila_copia_intervalo(posicao,
                                        posicao + LOTE_GRAVACAO,
                                        lote,
                                        LOTE_GRAVACAO);
        for (i = 0; i < copiados; i++) {
            fprintf(arquivo, "%d - %s\n", lote[i].id, lote[i].nome);
        }
        posicao += copiados;
    } while (copiados == LOTE_GRAVACAO);

    fprintf(arquivo, "%s\n", FILA_RODAPE);
    fclose(arquivo);

    pthread_mutex_unlock(&mutex_dados);
}
