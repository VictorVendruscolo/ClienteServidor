# Fase 3 — Protocolo de Mensagens (rascunho v1)

*Primeira versão para discussão. Pontos marcados **[A DECIDIR]** são os que faltam fechar
antes de passar para a Fase 4 (implementação). Base: `docs/02_atividades.md` (funcionalidades)
e `docs/00_estudo_codigos_aula.md` (o que os códigos de aula ensinam e o que não resolvem).*

---

## 1. Transporte e sessão

- TCP (`SOCK_STREAM`), IPv4. Servidor na porta **8080** (fixa), IP do cliente via
  `#define SERVER_IP` (decidido no `GUIA_MESTRE.md`, seção 6).
- **Uma conexão TCP persistente por cliente**, do login até o "Sair" — não é uma conexão nova
  por comando. O servidor trata cada conexão numa thread própria (`pthread_create` +
  `pthread_detach`, base `multithread.c`).
- Fluxo geral de uma sessão:
  1. Cliente conecta.
  2. Cliente envia `LOGIN`.
  3. Servidor responde `LOGIN_OK` ou `LOGIN_FAIL` (e fecha a conexão se falhar).
  4. Cliente entra no laço do menu (`1`/`2`/`3`/`0`), uma requisição por vez, aguardando resposta
     antes de mostrar o menu de novo.
  5. A qualquer momento nesse laço, o servidor pode empurrar uma mensagem de `[Broadcast]`
     sem o cliente pedir (outro cliente adicionou um usuário).
  6. Cliente envia `SAIR` (ou fecha) → servidor fecha o socket daquele cliente.

## 2. Delimitação de mensagens **[A DECIDIR — proposta abaixo]**

Nenhum dos 7 códigos de aula resolve isso (cada um troca só uma mensagem por conexão — ver
`docs/00_estudo_codigos_aula.md`, Tópico 3). TCP é um fluxo de bytes, não preserva "uma
`send()` = uma `recv()`". Proposta para a v1, pela simplicidade e pelo prazo:

- **Mensagens de uma linha**: texto ASCII terminado em `\n` (LF). Comando em maiúsculas +
  argumentos separados por espaço. Ex.: `ADD 20 Carlos\n`.
- **Mensagens de várias linhas** (resposta de `LIST` e de `HEARTBEAT` quando há novidade):
  uma linha de cabeçalho fixa, N linhas de dado, uma linha de rodapé fixa — o cliente lê linha
  a linha até bater no rodapé, sem precisar saber `N` de antemão. É exatamente o formato que
  já aparece no print (`===== FILA =====` ... `================`).
- Do lado de quem lê: `recv()` acumulando num buffer até achar `\n`, processa a linha, continua
  com o resto do buffer (padrão comum, nenhum dos códigos de aula mostra isso pronto).

## 3. Autenticação

- Confirmado em aula: usuário/senha **fixos, hardcoded** no código (não é validado contra
  arquivo/BD).
- Mensagem: `LOGIN <usuario> <senha>\n`
- Resposta: `LOGIN_OK\n` ou `LOGIN_FAIL\n`
- **Decidido (v1):** credencial de exemplo `admin` / `admin123`, como duas constantes no
  código do servidor (`#define LOGIN_USUARIO "admin"` / `#define LOGIN_SENHA "admin123"`).
  Trocar é só editar essas duas linhas e recompilar — troque quando quiser algo mais pessoal.

## 4. Tabela de mensagens

### Cliente → Servidor

Todo comando (exceto `LOGIN` e `SAIR`) carrega um número de sequência `<seq>` — inteiro que
começa em 1 e incrementa a cada novo comando **naquela conexão**, usado para o servidor
detectar reenvio por timeout e não processar duas vezes (seção 6).

| Comando | Formato | Quando |
|---|---|---|
| Login | `LOGIN <usuario> <senha>\n` | Primeira mensagem da sessão |
| Adicionar usuário | `ADD <seq> <id> <nome>\n` | Opção `1` do menu |
| Ver fila | `LIST <seq>\n` | Opção `2` do menu |
| Heartbeat | `HEARTBEAT <seq>\n` | Opção `3` do menu |
| Sair | `SAIR\n` | Opção `0` do menu |

### Servidor → Cliente (resposta direta a um pedido)

| Resposta | Formato | Quando |
|---|---|---|
| Login OK | `LOGIN_OK\n` | Credencial confere |
| Login falhou | `LOGIN_FAIL\n` | Credencial não confere (servidor fecha a conexão em seguida) |
| Usuário adicionado | `ADD_OK <id> <nome>\n` | Após guardar na fila (protegida por mutex) e disparar broadcast |
| Fila | `===== FILA =====\n` + `<id> - <nome>\n` × N + `================\n` | Resposta de `LIST` |
| Heartbeat com novidade | mesmo formato de `LIST`, mas só com os usuários adicionados por **outros** clientes desde a última checagem **deste** cliente | Resposta de `HEARTBEAT` |
| Heartbeat sem novidade | `ALIVE\n` | Resposta de `HEARTBEAT`, sem novidade |

### Servidor → Cliente (assíncrona, não solicitada)

| Mensagem | Formato | Quando |
|---|---|---|
| Broadcast | `[Broadcast] Novo usuário: <nome>\n` | Servidor manda para **todos os clientes exceto quem adicionou** logo após um `ADD` bem-sucedido |

## 5. Estrutura de dados no servidor

```c
typedef struct {
    int id;
    char nome[50];
} Usuario;
```

- **Fila global compartilhada**: **array de tamanho fixo** — `Usuario fila[20000]` — protegida
  por **um único `pthread_mutex_t`** para todas as operações de leitura/escrita (`ADD`, `LIST`,
  `HEARTBEAT` leem/escrevem a mesma estrutura). **Decidido (v1):** array em vez de lista ligada,
  pela simplicidade de implementar/testar no tempo que resta; 20000 cobre com folga o teste de
  carga de 10000 clientes mesmo que cada um adicione mais de um usuário. Documentar essa
  limitação (tamanho máximo) no item 3 da documentação.
- **ID e nome:** `id` é `int`, digitado pelo operador — **sem validação de unicidade** (nada
  no enunciado exige IDs únicos; os prints usam valores distintos, mas por escolha de quem
  testou, não por regra do sistema). `nome` limitado a **50 bytes** (`char nome[50]`).

  > **Atualização de 28/07 (o que o código faz hoje).** Duas regras foram acrescentadas
  > durante os testes, e valem tanto no cliente quanto no servidor:
  >
  > - **identificador precisa ser positivo.** Valores negativos e zero são recusados com
  >   `ERRO O identificador deve ser um numero positivo`. *Limitação conhecida:* um número
  >   acima do limite do `int` sofre estouro silencioso no `sscanf` e entra com outro valor.
  > - **nome acima do limite é recusado, não cortado**, com `ERRO Nome invalido, vazio ou
  >   longo demais`. Cortar traria dois problemas: o cliente receberia a confirmação de um
  >   nome diferente do que enviou, e o corte em 49 bytes poderia cair no meio de um
  >   caractere acentuado, que ocupa dois bytes. Nomes com acento são aceitos normalmente.
- **Por thread/cliente**: uma variável local `int ultimo_indice_visto`, guardada desde o
  `LOGIN` (= tamanho da fila naquele momento). O `HEARTBEAT` compara esse valor com o tamanho
  atual da fila: se maior, devolve os usuários entre esses índices e atualiza o valor; senão,
  `ALIVE`.
  - **Detalhe importante:** quando o próprio cliente faz um `ADD`, o `ultimo_indice_visto` dele
    precisa ser atualizado **imediatamente** (como se ele já tivesse "visto" o próprio usuário
    adicionado) — senão o `HEARTBEAT` devolveria pra ele mesmo o usuário que ele acabou de
    cadastrar, o que não bate com "usuários cadastrados por **outros** clientes".

## 6. Retransmissão de mensagens (item 4 da documentação — anotado "ack envio/recebimento")

TCP já garante entrega ordenada e sem perda **enquanto a conexão estiver de pé** — a
"retransmissão" que o professor quer documentada é sobre o nível da aplicação: garantir que
quem mandou uma mensagem sabe que ela foi **recebida e processada** do outro lado, e reagir se
não souber.

**Decidido (v1): resposta = ACK implícito.**
- Toda resposta do servidor listada na seção 4 **já funciona como ACK** do pedido do cliente
  (ex.: `ADD_OK` confirma que o `ADD` foi recebido e processado). Não existe uma mensagem `ACK`
  separada — a tabela da seção 4 perde a linha `ACK <seq>` que estava no rascunho.
- O cliente, depois de mandar um comando, espera a resposta com um **timeout de 3 segundos**.
  Se estourar sem resposta, **reenvia o mesmo comando**, até **3 tentativas**. Se as 3 falharem,
  avisa o usuário (ex.: `"Servidor não respondeu, tente novamente."`) e volta ao menu.
- Para o servidor não processar o mesmo `ADD` duas vezes se o reenvio cruzar com uma resposta
  que só demorou (e não se perdeu de verdade), cada comando carrega um número de sequência por
  conexão: `ADD <seq> <id> <nome>\n`, `LIST <seq>\n`, `HEARTBEAT <seq>\n`. O servidor guarda o
  último `seq` processado daquela conexão; se receber um `seq` repetido, reenvia a mesma
  resposta de antes sem processar de novo (idempotência).
- Essa é a parte do protocolo a explicar com mais cuidado na documentação (item 4) e na
  apresentação oral — é a resposta para "como foi tratada a retransmissão de mensagens".

## 7. Persistência (arquivo `.txt`, escolhida no lugar do banco)

Separando os dois tipos de dado esclarecidos por você (cliente vs. usuário):

| Arquivo | Conteúdo | Quando grava |
|---|---|---|
| `dados/sessoes.log` | Uma linha por evento de conexão: `LOGIN <ip> <timestamp>` / `LOGOUT <ip> <timestamp>` (dados de **cliente** — sessão de quem logou) | A cada login/saída |
| `dados/servidor.log` | Mesmas mensagens que já vão para o stdout (`Servidor iniciado...`, `Novo cliente conectado.`, `Cliente desconectado.`) | A cada evento de conexão/desconexão |
| `dados/fila.txt` | Snapshot atual da fila completa, no mesmo formato do `LIST` | Reescrito a cada `ADD` |
| `dados/historico.txt` | Log append-only: `<timestamp> ADD <id> <nome>` (dados de **usuário** — histórico da fila) | A cada `ADD` |

**Decidido:** mantém o split em 4 arquivos acima, e o servidor **sempre começa com a fila
vazia** ao iniciar (não recarrega `fila.txt`) — mais simples, e bate com o que os prints
sugerem (cada execução parece começar do zero). Os arquivos de log/histórico continuam
existindo como registro entre execuções, só a fila em memória não é restaurada.

## 8. Mapeamento com o Wireshark (para a defesa oral)

O professor vai capturar o tráfego e comparar com o que está documentado aqui. Cada linha de
comando/resposta desta seção 4 deve aparecer, em texto puro, dentro dos pacotes TCP capturados
(sem criptografia, sem serialização binária) — como fazia o `redes.c` com o GET HTTP. Isso é
uma vantagem para a apresentação: dá para abrir o Wireshark, seguir o "TCP Stream" de uma
conexão e mostrar exatamente essa tabela acontecendo byte a byte.

---

## Resumo das decisões (v1 fechada em 27/07)

Todos os pontos em aberto da primeira versão foram decididos:

1. Credencial de login: `admin` / `admin123` (constantes no código, trocáveis) — seção 3.
2. Fila: **array fixo** `Usuario fila[20000]`, sem lista ligada — seção 5.
3. Retransmissão: **resposta = ACK implícito** + timeout de 3s / 3 tentativas + número de
   sequência por conexão para evitar processar duplicado — seção 6.
4. Persistência: 4 arquivos (`sessoes.log`, `servidor.log`, `fila.txt`, `historico.txt`) —
   seção 7.
5. Servidor **sempre começa com a fila vazia**, não recarrega `fila.txt` — seção 7.
6. Delimitação de mensagens: texto ASCII, uma linha terminada em `\n`, com formato de
   cabeçalho/rodapé fixo para respostas de múltiplas linhas — seção 2.
7. `nome` limitado a 50 bytes e **recusado** se ultrapassar; `id` precisa ser positivo, mas
   sem checagem de unicidade — seção 5.

**Sem pendências bloqueantes.** Próximo passo: `docs/03_especificacao.md` (juntar isso com as
decisões de arquitetura — threads, mutex, estrutura de arquivos) e então a Fase 4
(implementação) pode começar.

---

*Protocolo v1 fechado em 27/07/2026, após discussão. Pronto para virar `docs/03_especificacao.md`
e alimentar a implementação (Fase 4).*
