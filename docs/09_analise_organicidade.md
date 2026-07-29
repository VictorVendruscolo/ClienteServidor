# Análise de organicidade do código

*Feita em 29/07/2026, com 3h para a entrega. Critério: o código prova que o aluno
aprendeu, ou prova que alguém resolveu por ele? Marcadores `[REVISAR n]` e `[MANTER]`
estão inseridos no próprio código de `ClienteServidor_Victor/`, com os números desta lista.*

**Estado:** todos os comentários originais foram removidos dos `.c`/`.h` e substituídos
apenas por estes marcadores. Backup íntegro em `ClienteServidor_Victor.backup-comentarios/`.
Os binários gerados são **byte a byte idênticos** aos de antes do passe — nada de
funcionamento mudou.

---

## Grupo A — Sai sem prejuízo nenhum

Nenhum destes serve a uma exigência do enunciado. São enfeite puro.

| # | O que é | Por que existe | Prejuízo de tirar |
|---|---|---|---|
| 04 | `SEM_TENTATIVA` × `CREDENCIAL_NEGADA` (servidor.c) | Deixar a tela do teste de carga honesta | Nenhum. O log passa a dizer "credencial recusada" para quem só fechou a conexão |
| 05 | `AGUARDA_ADD/LIST/HEARTBEAT` (cliente.c) | Imprimir a palavra `Heartbeat:` antes do cabeçalho da fila | Sai a palavra `Heartbeat:` de uma tela. 4 `#define` + 1 parâmetro atravessando duas funções por isso |
| 08 | `seq < último` = `SEQ_INVALIDA` (servidor.c) | Rigor inventado | Nenhum. Nada no enunciado pede. Foi isso que gerou os testes 21 e 22 |
| 11 | `memset(fila, 0, ...)` + `quantidade = 0` no `fila_init` | Zerar o que já nasce zerado | Nenhum. `static` é zerado por definição da linguagem |
| 03b | Tratamento de `\r\n` (protocolo.c) | Defesa contra cliente que manda `\r` | Nenhum. Nem seu cliente nem seu servidor mandam `\r` |
| 14b | `pthread_mutex_destroy` / `cond_destroy` nas portas do `return` | Simetria | Nenhum. O processo já está morrendo. É a mesma família do `fila_destroi` que você já removeu |

**Custo de aplicar:** minutos. Não altera nenhum caminho de execução testado.

---

## Grupo B — Simplificável; o preço é só desempenho

Aqui está o "matar barata com bazuca". Cada linha tem uma alternativa que um aluno
escreveria com o que sabe, e a perda é sempre de desempenho — nunca de correção.

### 02 — Um mutex por sessão (sessoes.c) · o pior caso do código

**Hoje:** cada uma das 12000 entradas da tabela tem seu próprio `pthread_mutex_t`, todos
inicializados no boot. Existe para um broadcast não partir o bloco `===== FILA =====` no meio.

**Alternativa:** **um único mutex global de envio.** Resolve o mesmo problema em uma linha.

**Prejuízo:** dois clientes não são escritos ao mesmo tempo. Com dezenas de clientes, nulo.
**É exatamente o argumento que você já usou** para remover o `rwlock` (`04_implementacao.md` §5).

**Ganho colateral, e é o maior de todos:** a regra de não-impasse documentada em
`04_implementacao.md` §3.3 — a que você precisa explicar na defesa — **deixa de existir**.
Um mutex só não tem ordem de aquisição, então não tem impasse possível.

### 02b — Busca circular de slot (sessoes.c)

`proximo_slot`, contador de tentativas, aritmética `% MAX_SESSOES`. Varredura linear do 0
tem 4 linhas e é obviamente correta. Prejuízo: com 10000 sessões, a varredura fica O(N) por
conexão. Em milissegundos, no teste de carga inteiro.

### 03 — `LeitorLinha` (protocolo.c) · o mais interessante

**Hoje:** struct com buffer próprio, `memmove` da sobra para o começo, contador `usados`,
caminho de erro para buffer cheio sem `\n`.

**Alternativa:** `recv` de **1 byte por vez** até achar o `\n`. Não precisa de struct, de
`protocolo_leitor_init`, de `memmove`, de guardar sobra, nem do erro de buffer cheio.
Cai de ~50 linhas para ~15, e a struct sai de `protocolo.h`.

**Prejuízo:** uma chamada de sistema por byte, em vez de ~1 por linha. Para 4 clientes
digitando num menu, invisível. No teste de carga não muda nada — as 10000 conexões ficam
paradas, sem tráfego.

**O ponto importante:** a delimitação de mensagem por linha **é obrigatória** (você tem
conexão persistente com muitas mensagens). O que é opcional é o *buffer*. Ele é otimização,
não requisito. E é o buffer, não a ideia, que ninguém consegue improvisar numa arguição.

### 06 e 06b — Envio e gravação em lotes (servidor.c e persistencia.c)

`LOTE_ENVIO 32` e `LOTE_GRAVACAO 64`, para não prender o mutex da fila durante uma listagem
longa. Alternativa: copiar sob uma trava só, ou travar durante a listagem. Prejuízo: um
`LIST` longo bloqueia inserções por alguns milissegundos. Sai também a generalidade toda de
`fila_copia_intervalo` (marcador 06).

### 09 — `protocolo_limpa_bordas` (protocolo.c)

Trim das duas pontas com `isspace` + `memmove`. O que de fato precisava: tirar o `\n` que o
`fgets` deixa. Uma linha resolve: `entrada[strcspn(entrada, "\n")] = '\0';`
Prejuízo: espaço no começo/fim de um nome deixa de ser removido.

### 15 — `persistencia_log_servidor` variádica (persistencia.c)

`va_list` + `vsnprintf`. **Você removeu a `protocolo_envia_fmt` justamente por ser
variádica** (`04_implementacao.md` §5.1) e manteve esta. Alternativa: os 4 chamadores fazem
`snprintf` e passam um `char *`. Sai `<stdarg.h>` do projeto. Prejuízo: nenhum.

### 13 — `envia_todos` (protocolo.c)

Laço que repete `send` até todos os bytes saírem. Tecnicamente correto (`send` pode aceitar
menos), e na prática uma linha de até 1024 bytes em rede local nunca parte.
**Recomendo manter:** são 12 linhas e a explicação é uma frase. É barato e é certo.

---

## Grupo C — Só sai perdendo exigência

### 01 / 01b / 01c — `Canal`, máquina de estados e `pthread_cond_timedwait` (cliente.c)

É a peça mais avançada do trabalho. Existe porque a thread do menu precisa saber que a
receptora já recebeu a resposta — e isso existe porque você implementou **reenvio automático
com tempo limite**.

**A pergunta de fundo:** o item 4 exige reenvio automático? Releia o enunciado: ele pede
*"como foi tratada a retransmissão de mensagens"* — é uma pergunta de **documentação**. Sua
anotação de aula foi *"ack envio/recebimento"*. A resposta-como-ACK sozinha já atende isso.
O **timeout de 3s + 3 tentativas + número de sequência + idempotência** foi acréscimo seu.

Se o item 4 fosse respondido só com "a resposta do servidor é a confirmação; se não chegar,
o cliente avisa o operador", **desapareceriam**: `pthread_cond_timedwait`, o `Canal` inteiro,
`verifica_sequencia`, o cache do `ADD` (07), o par de índices (07b) e a regra de descartar
resposta atrasada. É a maior parte do que você não consegue defender.

**Mas o prejuízo é grande e concreto:** invalida os testes 19–22, 25, 27 e 29 do
`05_plano_testes.md`, a seção 4 inteira do plano de testes, a seção de retransmissão do
`.tex` e as figuras. **Não recomendo mexer nisso hoje.** Registro para você saber que a
complexidade não veio do enunciado — veio de uma escolha sua, e você pode dizer isso.

### 07 / 07b — Cache do `ADD` e par de índices (servidor.c)

Necessários **se** a idempotência ficar. Não são enfeite: são o que faz um reenvio não
cadastrar duas vezes. Se o Grupo C ficar, isso fica.

### 10 — Recusa de caractere de controle (servidor.c)

Só o `\n` quebra o protocolo de linha. O resto é defesa contra um cliente que você escreveu.
Mas a validação no servidor é a resposta certa para "e se alguém falar com o servidor por
`telnet`?" — o professor testa com ferramentas próprias. **Manter, sabendo o porquê.**

### 12 — Reescrita acentuada no cliente (cliente.c)

Consequência da sua decisão de trafegar ASCII puro. Não é exigência, é escolha — e ela tem
uma justificativa boa (bytes iguais em qualquer máquina, na comparação com o Wireshark).

---

## Grupo D — Parece avançado, mas é bom de defender. Não mexa

| O que | A resposta, em uma frase |
|---|---|
| `pthread_attr_setstacksize` | 8 MB × 10000 threads = 80 GB. Reduzi para 256 KB |
| `inet_ntop` | `inet_ntoa` devolve buffer `static` compartilhado, quebra com threads |
| `localtime_r` | `localtime` devolve struct `static` compartilhada, mesma razão |
| `sched_yield()` | Veio do `multithread.c` do professor. Não é otimização minha |
| `signal(SIGPIPE, SIG_IGN)` | Escrever em socket fechado mataria o processo |
| `SO_REUSEADDR` | Reiniciar sem esperar o `TIME_WAIT` |
| `htons` | Ordem de bytes da rede |
| `struct` passada à thread por ponteiro | Corrige o defeito de cast do `multithread.c` em 64 bits |

---

## Achados extras (não são over-implementation, são inconsistências)

1. **16 — `fila.txt` é reescrito inteiro a cada `ADD`.** Com a fila cheia, isso é O(N) por
   inserção. Conviver com isso e ao mesmo tempo gravar "em lotes de 64 para não prender o
   mutex" é uma contradição de prioridade: o custo real está no lugar que não foi otimizado.
2. **14 — o cliente tem encerramento ordenado (`shutdown` + `pthread_join`); o servidor
   decidiu explicitamente não ter** (decisão 4.12: "evita código que nunca é executado").
   As duas decisões se contradizem.
3. **15 — uma função variádica foi removida por ser variádica, outra ficou.**
4. **17 — `MAX_SESSOES 12000` contra `MAX_USUARIOS_FILA 20000`:** dois limites diferentes,
   sem motivo declarado em lugar nenhum. O teto real de clientes é 12000, não 20000.

---

## Leitura de conjunto

A complexidade do código não está espalhada. Ela está concentrada em **dois lugares**, e os
dois têm a mesma origem:

- **O `Canal` + `cond_timedwait` + toda a maquinaria de sequência** existe por causa do
  reenvio automático, que foi sua interpretação (generosa) do item 4.
- **Os dois níveis de trava em `sessoes.c`** existem por causa da resposta multilinha.

Tirando o Grupo A e o Grupo B, o código cai na casa de **200 linhas** (estimativa grosseira,
não medida) e perde os dois trechos que ninguém defende sem ter escrito: o `memmove` do
buffer e a regra de não-impasse. O que sobra de avançado — `cond_timedwait` e a idempotência
— é o item 4, e sobre ele você tem uma resposta honesta e forte: *"o TCP já retransmite; eu
fui além do necessário aqui, e o que de fato agrega é o número de sequência."*

**Recomendação para as 3h:** Grupo A é praticamente grátis. Grupo B vale pelos dois ganhos
de defesa (02 mata a regra de não-impasse; 03 mata o `memmove`), mas exige repetir os testes.
Grupo C, documentar em vez de remover.
