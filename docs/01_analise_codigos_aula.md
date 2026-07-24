# Fase 1 — Análise dos Códigos de Aula do Rubens

*Base técnica que os códigos do trabalho vão herdar. Princípio: fazer como o Rubens ensinou.*

O professor disponibilizou **4 arquivos** (em `codigos_aula/`). Cada um demonstra uma peça
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

**Ponto-chave:** aceita **apenas uma conexão** e encerra. É a base — os outros três arquivos
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
> São erros de aula; na nossa versão sairão corrigidos.

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

## 5. Síntese: o que herdamos

| Peça do nosso código | Vem de | Adaptação |
|---|---|---|
| Esqueleto do servidor (socket→bind→listen→accept) | `bind.c` | Porta 8080, `SO_REUSEADDR` |
| Laço de múltiplos clientes | `fork.c` **ou** `multithread.c` | Ver decisão abaixo |
| Recolher filhos/threads terminados | `fork.c` (waitpid) / `multithread.c` (detach) | Conforme a técnica escolhida |
| Cliente (connect/send/recv) | `redes.c` | IPv4, porta 8080, laço de menu |

O que **nenhum** deles tem, e teremos que projetar na Fase 3:
- Estado compartilhado da **fila** entre os clientes.
- **Broadcast** para os outros clientes quando um paciente é adicionado.
- **Heartbeat** (novos de outros clientes, ou `ALIVE`).
- Um **protocolo de mensagens** de verdade (os exemplos só ecoam bytes).

---

## 6. Decisão de concorrência

O Rubens ensinou **três** técnicas (o PDF permite escolher uma): `fork()`, threads e — implícito
no enunciado — multiplexação. Entre os códigos de aula, ele deu **fork** e **threads** prontos.

**Recomendação: `fork()`.** Motivos:
- É a técnica mais **didática** e a que o professor detalhou melhor (comentou zombies, waitpid, SO_REUSEADDR).
- Facilita **explicar na apresentação oral** (avaliação exige isso).
- O código de aula é o mais completo dos dois.

**Ponto de atenção:** o teste de carga de **10000 clientes** é pesado para fork (10000 processos).
Duas saídas, a decidir na Fase 3:
- (a) usar fork e aceitar que o teste de 10000 vai forçar a máquina (documentar isso);
- (b) usar **threads**, que aguentam melhor a carga alta e ainda são "o que o Rubens ensinou".

> **Pendência para você decidir na Fase 3:** fork (mais didático) ou threads (escala melhor no
> teste de 10000). Precisa do compartilhamento de estado da fila entre conexões — isso é mais
> simples com **threads** (memória compartilhada no mesmo processo) do que com **fork** (processos
> separados precisam de memória compartilhada / IPC). **Esse detalhe pesa a favor de threads.**

---

*Fase 1 concluída em 23/07/2026. Próximo: Fase 2 (destrinchar o enunciado em atividades).*
