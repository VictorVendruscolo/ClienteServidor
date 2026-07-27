# Estudo Teórico dos Códigos de Aula do Rubens

*Registro do estudo conduzido em 27/07/2026, cobrindo os 6 arquivos de `codigos_aula/`
mais `servidor_multiplexacao.c` (em `Redes_Computadores/`, origem não confirmada).
Objetivo: aptidão para implementar o trabalho seguindo rigorosamente o que foi ensinado
em aula, e para defender as decisões técnicas na apresentação oral.*

---

## Índice de fontes

| Arquivo | Local | Ensina | Origem confirmada |
|---|---|---|---|
| `bind.c` | `codigos_aula/` | Esqueleto do servidor (1 servidor, 1 cliente) | Sim — Rubens |
| `escrevendo.c` | `codigos_aula/` | Servidor + resposta ao cliente | Sim — Rubens |
| `redes.c` | `codigos_aula/` | Lado cliente (`connect`/`send`/`recv`) | Sim — Rubens |
| `porta.c` | `codigos_aula/` | Cliente como sonda (port scan); laço de conexões | Sim — Rubens |
| `fork.c` | `codigos_aula/` | Múltiplos clientes via `fork()` | Sim — Rubens |
| `multithread.c` | `codigos_aula/` | Múltiplos clientes via `pthreads` | Sim — Rubens |
| `servidor_multiplexacao.c` | `Redes_Computadores/` | Múltiplos clientes via `select()` | **Não confirmada** |
| `ServidorTCP-simples-IPV4.c`, `Servidor_Processos.c`, `Servidor_Threads.c`, `ClienteTCP-HTTP-IPV6.c` | `Redes_Computadores/` | Versões corrigidas dos 4 originais | **Não confirmada** — indícios de correção posterior (ver Tópico 5.3) |

---

## Tópico 1 — Fundamentos

- **Assimetria cliente-servidor**: servidor é passivo (`socket→bind→listen→accept`), cliente é ativo (`socket→connect`). Depois de conectados, ambos usam `send`/`recv`/`close` simetricamente.
- **Socket é um descritor de arquivo** (`int`), por isso aceita `close()` e até `write()`.
- **`struct sockaddr_in` (IPv4) vs `sockaddr_in6` (IPv6)**: prefixos de campo diferentes (`sin_` vs `sin6_`). O cast `(struct sockaddr *)` é conversão de tipo para a API genérica, não faz nada com os dados.
- **`htons()`**: converte de ordem de bytes do host (little-endian em x86) para ordem de rede (big-endian). Obrigatório para portas de 16 bits; `INADDR_ANY` (0) não precisa de conversão porque é igual nas duas ordens.
- **`INADDR_ANY`**: "aceitar por qualquer interface" — só faz sentido do lado servidor.
- **Terceiro parâmetro de `socket()`**: `0` e `IPPROTO_TCP` são equivalentes para `SOCK_STREAM`; quem decide o protocolo é o segundo parâmetro (`SOCK_STREAM` = TCP, fluxo contínuo de bytes sem delimitação de mensagens — conceito que retorna no Tópico 3).

## Tópico 2 — `bind.c` + `escrevendo.c`: esqueleto do servidor

Sequência: `socket()` → `bind()` → `listen()` → `accept()`.

- **`bind()`** reserva a porta para o socket; precisa do tamanho da struct porque o mesmo ponteiro genérico pode apontar para IPv4 ou IPv6.
- **`listen(sock, N)`**: `N` é o **backlog** — fila de conexões já chegadas aguardando `accept`, não o limite de clientes simultâneos.
- **`accept()`** bloqueia até uma conexão chegar e devolve um **socket novo** (`novo_socket`), distinto do socket de escuta (`meu_socket`). Essa separação é o que permite o servidor continuar aceitando enquanto atende quem já entrou — pilar de fork/threads/multiplexação.
- **Bloquear** = dormir sem gastar CPU até a condição esperada ocorrer (aplica-se a `accept()` e a `recv()`).
- `bind.c` atende **uma única conexão** e termina — é o esqueleto puro.
- `escrevendo.c` acrescenta `write(novo_socket, ...)` — confirma que socket é descritor de arquivo comum, e reforça a regra "fala-se sempre pelo `novo_socket`, nunca pelo `meu_socket`".
- Falhas de robustez notadas (a corrigir na nossa versão): nem `bind.c` nem `escrevendo.c` abortam de fato em erro; `escrevendo.c` nunca fecha os sockets.

## Tópico 3 — `redes.c` + `porta.c`: lado cliente

- **`connect()`** é o inverso do `accept()`: tentativa ativa, não espera passiva.
- **`inet_pton()`** converte endereço texto→binário; mais robusta que `inet_addr()` (usada em `porta.c`) porque cobre IPv4 e IPv6 e distingue erro de valor.
- **`recv()` não devolve "uma mensagem"** — devolve até N bytes já disponíveis no fluxo TCP. É preciso terminar a string manualmente (`buffer[n] = '\0'`) e, em protocolos reais, **delimitar mensagens por convenção própria** (nenhum dos 7 arquivos resolve isso, porque cada um troca só uma mensagem por conexão). Implicação direta: nosso protocolo (Fase 3) precisa definir essa delimitação.
- O GET HTTP de `redes.c` é o único exemplo de "protocolo" nos 7 arquivos: texto com convenção combinada (`\r\n`, linha em branco final).
- `porta.c` usa `connect()` como **sonda**: o próprio resultado da tentativa (sucesso/falha) é a informação, sem trocar dados. Um socket **novo por tentativa** é obrigatório — não há como reciclar um socket que já tentou conectar.
- `porta.c` é o modelo estrutural mais próximo do **gerador de carga da Fase 5**: trocar "variar porta" por "repetir N vezes na porta 8080".
- Erros catalogados em `porta.c`: `getservbyname` no lugar de `gethostbyname`; `fgets` não remove o `\n`; `strncpy` usado para zerar struct em vez de `memset`; includes faltando. Não impactam nosso código porque não resolvemos nome de máquina.

## Tópico 4 — `fork.c`: múltiplos clientes com processos

- **`SO_REUSEADDR`** evita erro "Address already in use" ao reiniciar o servidor rapidamente (contorna o `TIME_WAIT` da porta). Útil e a ser mantido independente da técnica escolhida.
- **`fork()`** duplica o processo inteiro, incluindo a tabela de descritores — por isso pai e filho têm referência a `meu_socket` e `novo_socket` após o fork.
- **Disciplina de `close()`**: filho fecha `meu_socket` (não usa mais a portaria), pai fecha `novo_socket` (não usa mais aquela conexão específica). Esquecer um deles vaza o descritor, porque o kernel só libera quando todas as referências fecham.
- **Zombies**: processo filho que termina fica residual até o pai colher com `waitpid`. `SIGCHLD` + `waitpid(-1, NULL, WNOHANG)` em laço resolve — o laço é necessário porque sinais POSIX não enfileiram.
- **Limitação decisiva para o trabalho**: cada processo tem memória isolada (copy-on-write). Fila compartilhada e broadcast exigiriam IPC, não ensinado em aula. Esse foi o principal motivo para não escolher fork.

## Tópico 5 — `multithread.c`: múltiplos clientes com threads

- Threads do mesmo processo **compartilham memória** — resolve nativamente o problema de isolamento do fork.
- **`pthread_create(&thread_id, NULL, thread_proc, arg)`**: inicia execução do zero na função indicada (diferente do fork, que duplica e continua de onde estava).
- **Bug catalogado e corrigido**: cast direto `(void*) novo_socket` / `(int) arg` quebra em sistemas 64 bits (`void*` tem 64 bits, `int` tem 32). Correção obrigatória: passar por `intptr_t` nos dois sentidos. A versão em `Redes_Computadores/Servidor_Threads.c` já traz essa correção — o que é o principal indício de que aquela pasta é revisão posterior, não material original duplicado do professor.
- **`pthread_detach()`** é o equivalente do `waitpid` do fork — libera recursos da thread automaticamente ao terminar, sem precisar de `pthread_join()`.
- **`sched_yield()`**: reforço didático de agendamento, não estritamente necessário para correção do programa — mantido no nosso código por fidelidade ao que foi ensinado.
- **`-lpthread`** precisa constar explicitamente no Makefile, senão o link falha.
- **Consequência para a Fase 3/4**: fila compartilhada em variável global funciona sem IPC, mas exige **mutex** (`pthread_mutex_t`) para evitar race condition — nenhum arquivo de aula mostra isso, é projeto nosso.

## Tópico 5.5 — `servidor_multiplexacao.c`: multiplexação com `select()`

*Fonte com origem não confirmada — estudado como material de apoio, não como garantidamente do Rubens.*

- Modelo radicalmente diferente: **um único processo, uma única thread**, vigiando vários sockets ao mesmo tempo via `select()`.
- `fd_set` representa um conjunto de descritores; `FD_ZERO`/`FD_SET`/`FD_ISSET` manipulam esse conjunto. O conjunto precisa ser **remontado a cada iteração**, porque `select()` o modifica.
- `select()` bloqueia até **qualquer** socket vigiado ter novidade (generalização do bloqueio de `accept()` visto no Tópico 2).
- Depois de acordar, o laço checa cada descritor com `FD_ISSET`: se foi o socket de escuta, é conexão nova (`accept()` sem bloquear, pois já se sabe que há algo pronto); se foi um cliente, é dado chegando (`recv() <= 0` = desconexão).
- **Vantagem real**: estado nativamente compartilhado (um só fluxo de execução) **sem** precisar de mutex, ao contrário de threads.
- **Desvantagens para o nosso caso**: técnica não coberta com certeza em `codigos_aula/`; `MAX_CLIENTES` é array de tamanho fixo definido em compilação; lógica de "quem está pronto" é mais difícil de defender oralmente do que o fluxo linear de fork/threads.

## Tópico 6 — Síntese comparativa e decisão

| Critério | fork | threads | multiplexação |
|---|---|---|---|
| Fila compartilhada | Precisa IPC | Nativo + mutex | Nativo, sem mutex |
| Broadcast | Complexo | Direto | Direto |
| Escala em 10000 clientes | Ruim | Média | Boa |
| Confirmado como aula do Rubens | Sim | Sim | Não |
| Facilidade de defesa oral | Alta | Média | Baixa |

**Decisão final (27/07/2026): threads.** Motivo registrado no `GUIA_MESTRE.md`, seção 6 —
prazo de 1,5 dia, técnica já dominada de disciplinas de SO, 100% confirmada como ensinada
pelo professor, resolve fila compartilhada sem IPC.

### O que nenhum dos 7 arquivos resolve (projeto nosso, Fase 3)

- Protocolo de mensagens real (tipos, formato de campos).
- Delimitação de mensagens dentro do fluxo TCP.
- Estado compartilhado de negócio (a fila de pacientes).
- Broadcast.
- Heartbeat (no sentido do enunciado).
- Retransmissão de mensagens.
- Persistência.
- Mutex para proteger a fila (necessário por termos escolhido threads).

### Mapa de herança: nosso código ← código de aula

| Nossa peça | Herda de | Adaptação necessária |
|---|---|---|
| Esqueleto do servidor | `bind.c` | Porta 8080 |
| Resposta ao cliente | `escrevendo.c` | Padronizar em `send()`, sempre no `novo_socket` |
| `SO_REUSEADDR` | `fork.c` / `multithread.c` | Usar como está |
| Laço de múltiplos clientes | `multithread.c` | `intptr_t` no cast do socket |
| Limpeza de recursos | `multithread.c` (`pthread_detach`) | Usar como está |
| Cliente | `redes.c` | IPv4, porta 8080, laço de menu de 4 opções |
| Gerador de carga | `porta.c` | "Repetir N vezes" em vez de "variar porta" |
| Compilação | `multithread.c` (comentário `-lpthread`) | Regra explícita no Makefile |
| Mutex da fila | — | Projeto próprio |
| Protocolo | — | Projeto próprio, `docs/03_protocolo.md` |

### Erros catalogados nos 7 arquivos (para não repetir)

| Arquivo | Erro | Correção |
|---|---|---|
| `bind.c` | Sem `return`/`exit` em erro fatal | Abortar de fato |
| `escrevendo.c` | Nunca fecha sockets | Sempre `close()` |
| `fork.c` | Comentário `SO_REUSEADOR` (digitação) | Constante correta é `SO_REUSEADDR` |
| `multithread.c` | `accept(listen, ...)` usa nome de função, não a variável | `accept(listensock, ...)` |
| `multithread.c` | `thread_proc` sem `return` | `return NULL;` |
| `multithread.c` | Cast `(int)`/`(void*)` sem `intptr_t` | Passar por `intptr_t` |
| `porta.c` | `getservbyname` no lugar de `gethostbyname` | Trocar função |
| `porta.c` | `fgets` deixa `\n` no buffer | Remover `\n` |
| `porta.c` | `strncpy` para zerar struct | `memset` |
| `porta.c` | Includes faltando | Adicionar `<unistd.h>`, `<arpa/inet.h>` |

---

## Pendências abertas ao fim do estudo

- Origem de `Redes_Computadores/` (arquivos corrigidos + multiplexação) segue não confirmada — afeta citação na documentação final (item 8), não afeta mais a decisão de concorrência.
- `docs/01_analise_codigos_aula.md` ainda menciona "4 arquivos" em vários pontos — desatualizado desde a inclusão de `escrevendo.c` e `porta.c`, correção pendente.

*Estudo concluído em 27/07/2026, véspera do prazo final de implementação.*
