# Fase 1 — Análise dos Códigos de Aula do Rubens

*Base técnica que os códigos do trabalho vão herdar. Princípio: fazer como o Rubens ensinou.*

> **Nota de atualização (27/07):** esta análise foi escrita originalmente em 23/07 com os
> **4 arquivos** então disponíveis (`bind.c`, `fork.c`, `multithread.c`, `redes.c`). O
> repositório foi depois ampliado com mais **2 arquivos** em `codigos_aula/`
> (`escrevendo.c` e `porta.c`), totalizando **6 arquivos**, mais 1 arquivo de multiplexação
> (`servidor_multiplexacao.c`) em `Redes_Computadores/`, cuja origem segue **não
> confirmada**. As correções pontuais abaixo (contagem de arquivos e tabela de síntese)
> foram feitas em 27/07; a análise completa e detalhada dos 7 arquivos, incluindo
> `escrevendo.c`, `porta.c` e `servidor_multiplexacao.c`, está em
> `docs/00_estudo_codigos_aula.md`. A decisão final de concorrência (threads) também está
> registrada lá e no `GUIA_MESTRE.md`, seção 6.

O professor disponibilizou **6 arquivos** (em `codigos_aula/`). Cada um demonstra uma peça
isolada da API de sockets em C. Juntos, formam o vocabulário completo que precisamos.

---

## 1. `bind.c` — Servidor mínimo (1 servidor, 1 cliente)

**O que ensina:** o esqueleto de um servidor TCP, do socket ao `accept`, sem laço nem concorrência.

Sequência que ele usa:
1. `socket(AF_INET, SOCK_STREAM, 0)` — cria socket TCP IPv4.
2. Preenche `struct sockaddr_in`: `sin_family = AF_INET`, `sin_addr.s_addr = INADDR_ANY`
   (aceita qualquer interface), `sin_port = htons(8888)`.
3. `bind()` — associa o socket à porta.
4. `listen(meu_socket, 3)` — abre para conexões (fila de até 3 pendentes).
5. `accept()` — bloqueia até um cliente chegar e devolve um **novo socket** para aquele cliente.

**Ponto-chave:** aceita **apenas uma conexão** e encerra. É a base — os outros arquivos
mostram como estender isso para múltiplos clientes.

**Reaproveitar:** todo o bloco socket→bind→listen→accept é a espinha do nosso servidor.

---

## 2. `fork.c` — Múltiplos clientes com `fork()`

**O que ensina:** um processo-filho por cliente. É a técnica clássica e mais didática.

O que ele acrescenta ao esqueleto do `bind.c`:
- `setsockopt(..., SO_REUSEADDR, ...)` — permite reusar a porta imediatamente após reiniciar
  o servidor (evita o erro "Address already in use"). **Vamos querer isso.**
- Laço `while(1)` em torno do `accept()` — servidor nunca para de aceitar.
- Para cada conexão aceita, chama `fork()`:
  - **filho** (`pid == 0`): fecha o socket de escuta, atende o cliente (`recv`/`send`), depois `exit(0)`.
  - **pai**: fecha o socket do cliente e volta ao `accept()`.
- **Tratamento de zombies:** instala `signal(SIGCHLD, sigchld_handler)`, e o handler chama
  `waitpid(-1, NULL, WNOHANG)` num laço para recolher todos os filhos que terminaram.

**Custo:** cada cliente é um processo inteiro. Com 10000 clientes, são 10000 processos — pesado.

> ⚠️ Observação: o `fork.c` tem um erro de digitação — `SO_REUSEADOR` deveria ser `SO_REUSEADDR`.
> Se formos usar essa base, corrigir.

---

## 3. `multithread.c` — Múltiplos clientes com threads (`pthreads`)

**O que ensina:** uma thread por cliente, em vez de um processo. Mais leve que `fork()`.

Diferenças em relação ao `fork.c`:
- Compila com `-lpthread` (importante para o **Makefile**).
- Para cada conexão: `pthread_create(&thread_id, NULL, thread_proc, (void*)novo_socket)`
  passa o socket do cliente para a função `thread_proc`, que faz `recv`/`send`/`close`.
- `pthread_detach(thread_id)` — desatacha a thread para não virar "zombie" (equivale ao waitpid do fork).
- `sched_yield()` — cede a CPU para a nova thread iniciar.

**Custo:** mais leve que processos, mas 10000 threads ainda consomem bastante memória (uma pilha por thread).

> ⚠️ Observações: há dois erros de digitação no código do professor —
> `accept(listen, ...)` deveria ser `accept(listensock, ...)`, e `thread_proc` não tem `return`.
> Também há um bug de portabilidade: o cast direto `(void*) novo_socket` / `(int) arg` quebra em
> sistemas 64 bits — a correção é passar por `intptr_t` (ver `docs/00_estudo_codigos_aula.md`,
> Tópico 5). São erros de aula; na nossa versão sairão corrigidos.

---

## 4. `redes.c` — Cliente TCP (lado que conecta)

**O que ensina:** o lado **cliente** — como conectar num servidor e trocar dados. (Aqui ele usa
IPv6 e faz um GET HTTP só como exemplo, mas a mecânica é a mesma para o nosso caso.)

Sequência do cliente:
1. `socket(...)` — cria o socket.
2. Preenche o endereço do servidor (família, porta, IP via `inet_pton`).
3. `connect()` — conecta ao servidor.
4. `send()` para enviar, `recv()` para receber a resposta.
5. `close()`.

**Reaproveitar:** este é o molde do nosso `cliente.c`. Adaptações necessárias:
- Usar **IPv4** (`AF_INET`, `sockaddr_in`, `inet_pton(AF_INET, ...)` ou `INADDR_LOOPBACK`),
  não IPv6.
- Conectar em **127.0.0.1** (ou o IP do servidor) na **porta 8080** (a dos prints), de forma
  automática, sem parâmetros — como o enunciado exige.
- Em vez de um GET HTTP único, ter o **laço de menu** (4 opções) trocando mensagens do nosso protocolo.

---

## 4.1 `escrevendo.c` e `porta.c` (adicionados em 27/07)

Estes dois arquivos entraram no repositório depois desta análise original. Resumo rápido
(análise completa em `docs/00_estudo_codigos_aula.md`, Tópicos 2 e 3):

- **`escrevendo.c`**: mesma base de `bind.c`, acrescentando `write(novo_socket, ...)` para
  responder ao cliente. Confirma que socket é um descritor de arquivo comum e reforça a
  regra "fala-se sempre pelo `novo_socket`". Nunca fecha os sockets — corrigir na nossa versão.
- **`porta.c`**: cliente usado como sonda de portas (port scan) — para cada porta testada,
  abre um socket novo e tenta `connect()`; o resultado (sucesso/falha) é a própria informação.
  É o modelo estrutural mais próximo do **gerador de carga da Fase 5** (trocar "variar porta"
  por "repetir N vezes na porta 8080"). Contém erros catalogados (`getservbyname` em vez de
  `gethostbyname`, `fgets` que não remove `\n`, `strncpy` usado para zerar struct em vez de
  `memset`) que não impactam nosso código porque não resolvemos nome de máquina.

---

## 5. Síntese: o que herdamos

| Peça do nosso código | Vem de | Adaptação |
|---|---|---|
| Esqueleto do servidor (socket→bind→listen→accept) | `bind.c` | Porta 8080, `SO_REUSEADDR` |
| Resposta ao cliente (`write`/`send`) | `escrevendo.c` | Padronizar em `send()`, sempre no `novo_socket`, sempre `close()` |
| Laço de múltiplos clientes | `fork.c` **ou** `multithread.c` | Ver decisão abaixo — **decidido: `multithread.c`** |
| Recolher filhos/threads terminados | `fork.c` (waitpid) / `multithread.c` (detach) | Threads → `pthread_detach` |
| Cliente (connect/send/recv) | `redes.c` | IPv4, porta 8080, laço de menu |
| Gerador de carga (Fase 5) | `porta.c` | "Repetir N vezes" em vez de "variar porta" |

O que **nenhum** deles tem, e teremos que projetar na Fase 3:
- Estado compartilhado da **fila** entre os clientes (com threads, protegido por mutex).
- **Broadcast** para os outros clientes quando um paciente é adicionado.
- **Heartbeat** (novos de outros clientes, ou `ALIVE`).
- Um **protocolo de mensagens** de verdade (os exemplos só ecoam bytes).

---

## 6. Decisão de concorrência

> **Atualização (27/07): decisão finalizada.** Esta seção registra o raciocínio original da
> Fase 1 (23/07), quando a técnica ainda estava em aberto. A **decisão final foi threads**,
> tomada em 27/07 após o estudo aprofundado dos 7 arquivos — ver `docs/00_estudo_codigos_aula.md`
> (Tópico 6) e `GUIA_MESTRE.md` (seção 6) para o raciocínio completo e definitivo. O texto
> abaixo é mantido como registro histórico do primeiro-passo de análise.

O Rubens ensinou **três** técnicas (o PDF permite escolher uma): `fork()`, threads e — implícito
no enunciado — multiplexação. Entre os códigos de aula, ele deu **fork** e **threads** prontos.

**Recomendação original (23/07): `fork()`.** Motivos:
- É a técnica mais **didática** e a que o professor detalhou melhor (comentou zombies, waitpid, SO_REUSEADDR).
- Facilita **explicar na apresentação oral** (avaliação exige isso).
- O código de aula é o mais completo dos dois.

**Ponto de atenção:** o teste de carga de **10000 clientes** é pesado para fork (10000 processos).
Duas saídas, a decidir na Fase 3:
- (a) usar fork e aceitar que o teste de 10000 vai forçar a máquina (documentar isso);
- (b) usar **threads**, que aguentam melhor a carga alta e ainda são "o que o Rubens ensinou".

> **Pendência (resolvida em 27/07 — ver nota no topo desta seção):** o compartilhamento de
> estado da fila entre conexões é mais simples com **threads** (memória compartilhada no mesmo
> processo) do que com **fork** (processos separados precisam de memória compartilhada / IPC).
> Esse detalhe, somado ao prazo curto e à familiaridade prévia do aluno com threads, decidiu a
> escolha final por **threads**.

---

*Fase 1 concluída em 23/07/2026. Correções de contagem de arquivos e nota de decisão final
aplicadas em 27/07/2026, após o estudo aprofundado dos 7 arquivos (`docs/00_estudo_codigos_aula.md`)
e a decisão de concorrência (threads). Próximo: Fase 2 (destrinchar o enunciado em atividades).*
