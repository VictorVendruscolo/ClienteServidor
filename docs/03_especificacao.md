# Fase 3 — Especificação (arquitetura + protocolo consolidados)

*Junta as decisões de arquitetura com o protocolo (`docs/03_protocolo.md`) num documento só.
Serve de base direta para os itens 1, 2 e 3 da documentação final. Nada aqui é decisão nova —
é consolidação do que já foi fechado em `GUIA_MESTRE.md`, `docs/02_atividades.md` e
`docs/03_protocolo.md`.*

---

## 1. Sumário do problema

Plataforma cliente-servidor para monitoramento de uma fila em tempo real: um servidor central
mantém uma fila compartilhada de usuários (ID + Nome); múltiplos clientes conectam
simultaneamente, adicionam usuários à fila, consultam a fila, e recebem atualizações em tempo
real (broadcast) e sob demanda (heartbeat). Escopo: tudo do enunciado (`trabalho_redes_2026.pdf`)
é obrigatório, incluindo persistência (escolhida em arquivo `.txt`, não banco de dados) —
ver `GUIA_MESTRE.md`, seção 0.1, para o esclarecimento do professor sobre isso.

## 2. Arquitetura geral

```
                    porta 8080 (TCP)
 [Cliente 1] ───────────┐
 [Cliente 2] ───────────┼──► [Servidor] ── fila compartilhada (mutex)
 [Cliente N] ───────────┘         │
                                   └──► arquivos .txt (persistência)
```

- **Técnica de concorrência: threads (`pthreads`)** — decidido em `GUIA_MESTRE.md`, seção 6.
  Uma thread por cliente conectado (`pthread_create` + `pthread_detach`), base `multithread.c`
  com a correção do cast via `intptr_t` (ver `docs/00_estudo_codigos_aula.md`, Tópico 5).
- **Uma conexão TCP persistente por cliente**, do `LOGIN` ao `SAIR`.
- **Servidor**: `socket → setsockopt(SO_REUSEADDR) → bind (porta 8080, INADDR_ANY) → listen →
  accept` em laço `while(1)`, uma thread nova por `accept()`.
- **Cliente**: `socket → connect (SERVER_IP:8080) → LOGIN → laço de menu → SAIR → close`. IP do
  servidor fixo via `#define SERVER_IP` (decidido — servidor e clientes rodam em máquinas
  diferentes na mesma rede; ver `GUIA_MESTRE.md`, seção 6).

## 3. Estrutura de dados

```c
typedef struct {
    int  id;
    char nome[50];
} Usuario;

Usuario fila[20000];              // array fixo — decidido em docs/03_protocolo.md, seção 5
int     fila_tamanho;             // quantos slots de `fila` estão em uso
pthread_mutex_t fila_mutex;       // protege fila[] e fila_tamanho em todo acesso
```

- Todo acesso a `fila`/`fila_tamanho` (por `ADD`, `LIST` ou `HEARTBEAT`) passa por
  `pthread_mutex_lock`/`unlock`.
- Cada thread de cliente guarda localmente `int ultimo_indice_visto`, para o `HEARTBEAT`
  devolver só os usuários adicionados por outros desde a última checagem (detalhe da
  atualização própria ao fazer `ADD` — ver `docs/03_protocolo.md`, seção 5).
- `id`: `int` que precisa ser **positivo** (negativo e zero são recusados), sem checagem de
  unicidade. `nome`: até 49 caracteres — acima disso o cadastro é **recusado**, não cortado.
  Ver a atualização de 28/07 em `docs/03_protocolo.md`, seção 5.

## 4. Protocolo de mensagens

Especificado por completo em **`docs/03_protocolo.md`**. Resumo:

- Transporte: TCP, texto ASCII, uma mensagem por linha (`\n`), com número de sequência nos
  comandos pós-login para suportar retransmissão.
- Comandos do cliente: `LOGIN`, `ADD`, `LIST`, `HEARTBEAT`, `SAIR`.
- Respostas do servidor: `LOGIN_OK`/`LOGIN_FAIL`, `ADD_OK`, fila formatada
  (`===== FILA ===== ... ================`), `ALIVE` ou fila parcial (heartbeat), e a
  mensagem assíncrona `[Broadcast] Novo usuário: <nome>`.
- Retransmissão: resposta = ACK implícito; timeout de 3s / até 3 tentativas de reenvio pelo
  cliente; servidor ignora `seq` repetido (idempotência).

## 5. Autenticação

Usuário/senha **fixos no código** (`#define LOGIN_USUARIO "admin"` / `#define LOGIN_SENHA
"admin123"`), confirmado em aula ("deixar no código"). Sem validação dinâmica contra
arquivo/BD. Mensagem `LOGIN <usuario> <senha>` → `LOGIN_OK` ou `LOGIN_FAIL` (conexão encerrada
se falhar).

## 6. Persistência (arquivo `.txt`)

Escolhida no lugar do banco de dados (decisão registrada em `GUIA_MESTRE.md`, seção 6),
separando dados de **cliente** (sessão/login) e de **usuário** (fila/histórico):

| Arquivo | Conteúdo |
|---|---|
| `dados/sessoes.log` | `LOGIN <ip> <timestamp>` / `LOGOUT <ip> <timestamp>` por conexão |
| `dados/servidor.log` | Eventos do servidor (mesmo texto do stdout) |
| `dados/fila.txt` | Snapshot atual da fila, reescrito a cada `ADD` |
| `dados/historico.txt` | Log append-only de cada `ADD` |

O servidor **sempre inicia com a fila vazia em memória** — não recarrega `fila.txt` ao
iniciar (decisão registrada em `docs/03_protocolo.md`, seção 7).

## 7. Estrutura de módulos do código-fonte (proposta para a Fase 4)

```
src/
├── comum.h          // Usuario, constantes (porta, SERVER_IP, tamanhos), nomes dos comandos
├── protocolo.c/.h    // monta/parseia as linhas de texto do protocolo (comum a cliente e servidor)
├── fila.c/.h          // operações sobre a fila compartilhada (add, listar, heartbeat) + mutex
├── persistencia.c/.h  // leitura/escrita dos 4 arquivos .txt
├── servidor.c/.h      // main do servidor: accept + thread por cliente + login
├── cliente.c/.h        // main do cliente: connect + laço de menu
└── Makefile
```

Motivo da separação: a avaliação cobra "modularidade" e "quais funções são implementadas em
cada módulo" na documentação (item 2) — com essa divisão, cada arquivo tem uma responsabilidade
só, e o item 2 da doc vira quase uma tradução direta desta lista. **Sugestão, ajustável** —
não é uma decisão fechada como as anteriores, é só um ponto de partida para a Fase 4.

## 8. Limitações conhecidas e decisões de implementação (para o item 3 da documentação)

Tudo que o enunciado deixa em aberto e que decidimos por conta própria — cada linha aqui é
material direto para o item 3 ("decisões de implementação omissas na especificação"):

- Terminologia: "usuário" no lugar de "paciente" (print usa o termo antigo; o texto do
  enunciado já usa "usuário").
- Fila com tamanho máximo fixo (20000), não dinâmica.
- IDs sem checagem de unicidade.
- IP do servidor fixo no código, não descoberto dinamicamente na rede — recompilar ao mudar
  de máquina.
- Login com credencial única fixa no código, não autenticação real de múltiplos usuários.
- Heartbeat segue literalmente o texto da observação do enunciado, não o comportamento do
  print (que mostra a fila inteira).
- Retransmissão via resposta-como-ACK + timeout/reenvio, em vez de um `ACK` explícito separado.
- Servidor não recarrega a fila salva ao reiniciar.

## 9. Rastreabilidade com os 8 itens da documentação final

| # | Item | Fonte neste projeto |
|---|---|---|
| 1 | Sumário do problema | Seção 1 deste documento + `docs/02_atividades.md` |
| 2 | Algoritmos, TADs, funções, decisões | Seções 2–5 e 7 deste documento |
| 3 | Decisões de implementação omissas | Seção 8 deste documento |
| 4 | Retransmissão de mensagens (ack envio/recebimento) | Seção 4 + `docs/03_protocolo.md`, seção 6 |
| 5 | Testes + análise (logs/printscreen) | `docs/05_plano_testes.md` (Fase 5) |
| 6 | Teste de carga 100/1000/10000 | `docs/05_plano_testes.md` (Fase 5) |
| 7 | Prints de funcionamento | Fases 4 e 5 |
| 8 | Conclusão (desenvolvimento/dificuldades) + referências | `docs/06_estudo_aprofundado.md` (Fase 6) |

---

*Especificação fechada em 27/07/2026. Fase 3 completa — próximo passo: Fase 4, implementação
em C (`src/`, começando pelo `Makefile` e o esqueleto de conexão).*
