# Alterações no código e o que precisa mudar no PDF

*29/07/2026. Registro do passe de simplificação feito na pasta de entrega
`ClienteServidor_Victor/`. Serve para (a) ajustar a documentação final e (b) saber exatamente
o que ainda é complexo, específico ou obscuro e por isso precisa estar declarado no PDF.*

**Backup íntegro da versão anterior:** `ClienteServidor_Victor.backup-comentarios/`

---

## 1. Resumo

| | antes | depois |
|---|---|---|
| Linhas de código (sem vazias nem comentários) | 1326 | **1283** |
| Módulos | 6 | 6 |
| Comentários | frases explicativas | **100 rótulos curtos** |
| Avisos `-Wall -Wextra` | 0 | 0 |
| Avisos no conjunto rigoroso | 0 | 0 |
| 8 combinações de `-std` e `-O2` | limpas | limpas |
| Bateria funcional (19 verificações) | — | **saída idêntica à anterior** |
| Carga 100 / 1.000 / 10.000 | 0 falhas | **0 falhas** |

Duas alterações de código foram aplicadas. **Nenhuma muda o protocolo na rede, nenhuma muda
o comportamento observável.** Todo o resto do código está intacto.

---

## 2. Alteração 1 — leitura de linha byte a byte

**Arquivos:** `protocolo.h`, `protocolo.c`, `servidor.c`, `cliente.c`, `carga.c`

### O que era

O módulo `protocolo` mantinha um tipo `LeitorLinha` — uma struct com o socket, um buffer de
1024 bytes e um contador `usados`. Cada `recv()` trazia um punhado de bytes para esse buffer;
a função procurava o `\n`, copiava a linha para o destino e **deslocava o resto do buffer de
volta para o começo com `memmove`**, guardando essa sobra para a chamada seguinte. Havia
ainda o tratamento de `\r\n` e um caminho de erro para buffer cheio sem quebra de linha.

### O que é agora

```c
int protocolo_le_linha(int sock, char *destino, size_t tam)
{
    size_t usados = 0;
    char   c;
    ...
    for (;;) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n == 0)      return LINHA_FECHADA;
        if (n < 0)       return LINHA_ERRO;
        if (c == '\n') { destino[usados] = '\0'; return LINHA_OK; }
        if (usados + 1 >= tam) return LINHA_ERRO;
        destino[usados] = c;
        usados++;
    }
}
```

Um byte por `recv`, até achar o `\n`. **Deixaram de existir:** o tipo `LeitorLinha`, a função
`protocolo_leitor_init`, o `memmove`, o buffer intermediário, a sobra entre chamadas e o
tratamento de `\r\n`. A assinatura passou a receber o socket direto, o que apagou as
declarações `LeitorLinha leitor;` dos cinco pontos onde existiam.

### Por que

O problema que o buffer resolvia — TCP entregar bytes e não mensagens — continua resolvido:
a delimitação por `\n` é que resolve, não o buffer. O buffer era **otimização**, para fazer
menos chamadas de sistema. Era também a construção que mais fugia do nível da disciplina.

### Preço

Uma chamada de sistema por byte, em vez de aproximadamente uma por linha. Para os volumes
deste trabalho (mensagens de dezenas de bytes, poucos clientes digitando num menu) é
imperceptível. No teste de carga não muda nada: as conexões ficam abertas e paradas.

### Ganho não previsto: um defeito latente deixou de existir

Na versão anterior o `cliente.c` criava um `LeitorLinha` **local** dentro de `autentica()`
para ler o `LOGIN_OK`, e a `thread_receptora` criava **outro** para o mesmo socket. Se o
`LOGIN_OK` chegasse no mesmo segmento TCP que uma mensagem seguinte, a sobra ficaria no
struct local e **morreria junto com ele** ao fim da função — mensagem perdida.

Na prática isso não acontecia, porque o cliente ainda não está registrado na tabela de
sessões quando o `LOGIN_OK` é enviado, então não há mensagem para vir colada. Era defeito
**latente, não ativo**. Sem sobra, ele deixa de ser possível.

---

## 3. Alteração 2 — um único *mutex* de envio

**Arquivo:** `sessoes.c`

### O que era

Cada uma das 12.000 entradas da tabela de sessões tinha um `pthread_mutex_t envio_mutex`
próprio, e todos eram inicializados na partida do servidor (um laço com 12.000 chamadas de
`pthread_mutex_init`). O `broadcast` travava a tabela e, dentro do laço, travava e destravava
o *mutex* de cada sessão. Isso exigia uma **regra de não-impasse** documentada: o *mutex* de
envio era obtido sem travar a tabela, porque quem o obtinha era a própria thread dona da
sessão, e só ela removia aquela entrada.

### O que é agora

```c
static pthread_mutex_t envio_mutex = PTHREAD_MUTEX_INITIALIZER;
```

Um só, para todos os sockets. O campo saiu da struct `Sessao`, o laço de inicialização saiu
do `sessoes_init`, e o `broadcast` trava o envio **uma vez**, fora do laço. As funções
`sessoes_trava_envio` e `sessoes_libera_envio` deixaram de precisar validar o índice.

### Por que

É o mesmo raciocínio já registrado em `04_implementacao.md` §5, quando o `pthread_rwlock_t`
foi removido: com algumas dezenas de clientes a diferença é nula. E o ganho é maior do que a
economia de linhas: **a regra de não-impasse deixa de existir**. Um *mutex* só não tem ordem
de aquisição, logo não há impasse possível. Some um conceito difícil da documentação e da
apresentação.

### Preço

Dois clientes não são escritos ao mesmo tempo. Um `broadcast` espera uma listagem terminar, e
vice-versa. Com a escala deste sistema, irrelevante.

### O que continua garantido

A serialização de escrita continua existindo — é ela que impede um `broadcast` de partir o
bloco `===== FILA =====` no meio. Só passou a ser global em vez de por sessão. **Isso foi
testado especificamente** (ver seção 5).

---

## 4. O que NÃO foi alterado, e por quê

Estas foram consideradas e recusadas. Vale registrar, porque cada uma parece "excesso" e não é.

| Item | Motivo de ficar |
|---|---|
| **Envio da fila em lotes de 32** (`LOTE_ENVIO`) | Sem os lotes, o *mutex* da fila fica travado durante a listagem inteira: quem quiser cadastrar espera o tempo de enviar todas as linhas. Com fila grande isso é real. É otimização com justificativa honesta |
| **Tipo do comando aguardado** (`AGUARDA_ADD/LIST/HEARTBEAT`) | Existe para imprimir `Heartbeat:` antes do bloco — e **o enunciado exige** esse texto (telas das páginas 5 e 6) |
| **Laço de envio completo** (`envia_todos`) | `send` pode aceitar menos bytes do que foi pedido. Sem o laço, meia linha sairia e o protocolo quebraria sem erro reportado. O laço é o que torna correto |
| **Recusa de caractere de controle no nome** | Um `\n` dentro de um nome faria o cliente ler duas linhas ao receber o bloco da fila, corrompendo a resposta. A validação no servidor cobre o caso de alguém falar com ele por `telnet` ou script |
| **Encerramento ordenado no cliente** (`shutdown` + `pthread_join`) | Os dois são um par: sem o `shutdown`, a thread receptora fica presa no `recv` e o `join` nunca retorna. É robustez, e remover às pressas não vale |
| **`Canal`, `pthread_cond_timedwait`, idempotência** | São o item 4 da documentação. Remover invalidaria os testes 19–22, 25, 27 e 29 e a seção de retransmissão do PDF |
| **Reescrita acentuada no cliente** | Consequência da decisão de trafegar ASCII puro, que tem justificativa própria (bytes iguais em qualquer máquina, na comparação com o Wireshark) |
| **`protocolo_limpa_bordas`** com `isspace` e `memmove` | Simplificar mudaria comportamento: nome com espaço nas pontas passaria a ir com os espaços |
| **`memset` no `fila_init`** | Redundante (vetor `static` já nasce zerado), mas inofensivo e legível |

---

## 5. Como as duas alterações foram validadas

Servidor novo, em porta livre, para cada versão. Comparação da saída, linha a linha, com a
versão anterior.

**19 verificações funcionais — saída idêntica:** login válido e inválido, comando antes do
login, `ADD`, reenvio com o mesmo número de sequência, broadcast, `LIST`, *heartbeat* com
novidade, *heartbeat* repetido, *heartbeat* vazio, identificador negativo, nome longo demais,
comando inexistente, sequência zero, `ADD` sem argumentos, linha em branco.

**Dois testes específicos das peças alteradas:**

| Teste | Protege | Resultado |
|---|---|---|
| Duas mensagens completas num único `write` | a delimitação (alteração 1) | as duas processadas, na ordem |
| Uma mensagem partida em dois `write` com pausa | a delimitação (alteração 1) | remontada corretamente |
| 25 listagens longas com outro cliente cadastrando sem parar em paralelo | a serialização de escrita (alteração 2) | **nenhum bloco partido**; 176 broadcasts chegaram, todos fora do bloco |

**Compilação:** 0 avisos com `-Wall -Wextra`; 0 avisos com `-Wpedantic -Wshadow -Wconversion
-Wsign-conversion -Wcast-align`; 0 erros e 0 avisos nas 8 combinações de `-std=c99/c11/c17`
com e sem `-O2`.

**Carga:** 100, 1.000 e 10.000 clientes, 0 falhas, com exatamente N linhas de
`Novo cliente conectado` no servidor. Um cliente comum operou normalmente depois da carga.
Os quatro arquivos de `dados/` foram gerados corretamente.

---

## 6. Convenção de comentários adotada

Os comentários explicativos em prosa foram substituídos por **rótulos curtos** — uma palavra
ou expressão, com `//`, sem frases. A função deles é apontar o que cada parte faz; a
explicação fica no PDF. Trechos triviais não recebem rótulo.

Exemplos: `// fila compartilhada`, `// secao critica`, `// maquina de estados do bloco`,
`// espera com tempo limite`, `// deteccao de reenvio`, `// mutex unico de envio`,
`// pilha reduzida`, `// uma thread por cliente`.

São 100 rótulos nos 12 arquivos.

---

## 7. O que ainda é complexo, específico ou obscuro — **precisa estar no PDF**

O enunciado exige que qualquer elemento não especificado, mas necessário, seja descrito
explicitamente. Esta é a lista do que sobrou e precisa aparecer no item 2 ou 3.

### 7.1 Construções avançadas

| O que | Onde | O que dizer |
|---|---|---|
| `pthread_cond_timedwait` + struct `Canal` | `cliente.c` | A thread do menu precisa esperar uma resposta que **outra** thread recebe. A variável de condição faz ela dormir até ser avisada, ou até estourar o prazo de 3 s. O prazo é um **instante absoluto** (`time(NULL)+3`), exigência da função. O `while` em volta existe porque uma variável de condição pode acordar sem ninguém ter sinalizado (*acorde espúrio*), então a condição é reconferida a cada acorde |
| Idempotência: `ultima_resposta_add` + `resposta_add_valida` | `servidor.c` | Guarda a resposta do último `ADD` para devolvê-la em caso de reenvio, **sem inserir de novo** |
| Par `indice_visto` / `indice_visto_antes` | `servidor.c` | Permite recalcular exatamente o mesmo bloco quando um `HEARTBEAT` chega repetido, já que o comando altera estado |
| Máquina de estados de bloco multilinha (`dentro_do_bloco`) | `cliente.c` | Existe porque a resposta de `LIST` tem várias linhas: o cliente precisa saber se está dentro ou fora de um bloco para separar resposta de broadcast |
| Serialização global de escrita | `sessoes.c` | Impede que um broadcast de outra thread entre no meio de um bloco de várias linhas |
| Envio e gravação em lotes (32 e 64) | `servidor.c`, `persistencia.c` | Mantêm o *mutex* da fila travado por intervalos curtos, para uma listagem longa não bloquear inserções |
| `pthread_attr_setstacksize` (256 KB) | `servidor.c` | 8 MB × 10.000 threads = 80 GB de espaço reservado. Sem reduzir, o teste de carga é impossível |
| `pthread_attr_setdetachstate` | `servidor.c` | Thread que termina sem ser recolhida deixa recursos presos (equivalente ao processo zumbi do `fork`). Criar já desatachada economiza uma chamada por conexão |
| Busca circular de slot livre | `sessoes.c` | Começa do último slot usado, em vez de varrer do zero a cada conexão |

### 7.2 Funções de biblioteca fora do que a disciplina mostrou

| Função | Alternativa comum | Por que esta |
|---|---|---|
| `sscanf` com `%*s` e `%n` | aritmética de ponteiro ou `strchr` | `%*s` lê e descarta o comando, `%n` grava quantos caracteres foram consumidos, marcando onde o nome começa. **É o ponto mais obscuro do código** |
| `vsnprintf` + `va_list` (`persistencia_log_servidor`) | `snprintf` no chamador | Permite chamar o log com formato variável, como um `printf` |
| `inet_pton` / `inet_ntop` | `inet_addr` / `inet_ntoa` | `inet_addr` devolve `INADDR_NONE` no erro, que é um endereço válido (255.255.255.255) — não distingue erro de acerto. `inet_ntoa` devolve um buffer `static` compartilhado, inseguro com threads |
| `localtime_r` | `localtime` | `localtime` devolve uma struct `static` compartilhada, insegura com threads |
| `memmove` | `memcpy` | As regiões se sobrepõem; `memcpy` não garante o resultado nesse caso |
| `strncpy` | `strcpy` | Limita a cópia ao tamanho do destino. Não garante o `\0` final, que por isso é escrito à mão |
| `snprintf` | `printf` | **Não são alternativas.** `printf` escreve na tela; `snprintf` monta uma string dentro de um buffer com limite de tamanho, que é o que se envia pela rede |
| `isspace` | comparação direta | Cobre espaço, tabulação e quebra de linha de uma vez |
| `static` em escopo de arquivo | variável global | Restringe o símbolo ao próprio `.c`. É o mecanismo que torna a modularidade real: o vetor da fila é inacessível de fora, então é impossível mexer nela sem passar pelas funções que travam o *mutex* |

### 7.3 Limitações e incoerências a declarar

1. **Estouro de inteiro no identificador.** `sscanf("%d")` aceita silenciosamente um número
   acima do limite de `int` e grava outro valor (`99999999999999` entra como `276447231`).
   Negativos e zero são recusados; o estouro produz positivo e passa.
2. **`MAX_SESSOES` 12.000 contra `MAX_USUARIOS_FILA` 20.000.** O teto real de clientes
   simultâneos é 12.000, não 20.000.
3. **`fila.txt` é reescrito por inteiro a cada `ADD`.** Com a fila grande, é O(N) por
   inserção.
4. **Dois estilos de inicialização de *mutex*:** `PTHREAD_MUTEX_INITIALIZER` em `servidor.c`,
   `persistencia.c` e `sessoes.c`; `pthread_mutex_init` em `fila.c` e `cliente.c`.
5. **Três formas de converter texto em inteiro:** `atoi` em `carga.c`, `sscanf("%d")` no
   cliente e no servidor.
6. **O cliente tem encerramento ordenado, o servidor não** (decisão 4.12). No cliente é
   necessário: há uma thread para recolher com `pthread_join`, e o `shutdown` é o que a faz
   sair do `recv`. No servidor as threads são desatachadas e o processo nunca encerra
   normalmente.
7. **A fila não é recarregada** ao reiniciar o servidor.
8. **Identificadores não são verificados quanto à unicidade.**

---

## 8. O que editar na documentação

### `docs/documentacao.tex`

**Linha 360** — tirar `protocolo_leitor_init` da lista de funções do módulo:

```
\texttt{protocolo\_le\_linha}, \texttt{protocolo\_envia\_linha}, \texttt{protocolo\_limpa\_bordas} \\
```

**Seção 2.3 (Protocolo de mensagens), por volta da linha 290** — a frase *"Quem lê acumula os
bytes em um buffer próprio até encontrar a quebra de linha"* não descreve mais o código.
Substituir por:

> Quem lê consome um byte por vez até encontrar a quebra de linha, e devolve à camada de
> cima exatamente uma mensagem por chamada.

**Seção 3.1 (Elementos acrescentados), itens 2 e 3, linhas 525–534** — o item 2 muda e o
item 3 **deixa de existir**. Texto de substituição para o item 2:

> \item \textbf{Serialização de escrita.} Sem ela, uma mensagem de \textit{broadcast}
> disparada por outra thread poderia entrar no meio de uma resposta de várias linhas e
> quebrar o protocolo. Um único \textit{mutex} de envio, global, é mantido travado do
> cabeçalho ao rodapé do bloco. Como é um \textit{mutex} só, não existe ordem de aquisição
> entre travas de envio e portanto não há risco de impasse.

Renumerar os itens seguintes.

### `docs/04_implementacao.md`

- **§3, itens 2 e 3** — mesma mudança: item 2 reescrito para *mutex* único, item 3 (regra de
  não-impasse) removido.
- **§2, tabela de arquivos** — tirar `protocolo_leitor_init` da linha do `protocolo.c/.h`.
- Acrescentar uma **§5.2** registrando este passe, com o conteúdo das seções 2 e 3 deste
  documento.

### `docs/06_estudo_aprofundado.md`

- **Linha 97**, a pergunta *"Por que cada sessão tem um mutex de envio?"* → *"Por que existe
  um mutex de envio?"*, com a resposta atualizada.
- **§2**, a descrição do `protocolo.c` — trocar a menção ao buffer pela leitura byte a byte.
- **§3, passo 3** — o caminho da mensagem não passa mais por `protocolo_leitor_init`.
- Acrescentar `pthread_cond_timedwait` às perguntas prováveis, com a explicação da
  seção 7.1 deste documento.

### `docs/05_plano_testes.md`

Acrescentar os três testes novos da seção 5 (duas mensagens num `write`, uma mensagem em dois
`write`, listagens longas sob broadcast concorrente). São evidência direta para o item 5.

---

## 9. Pendências antes de zipar

1. **Apagar os executáveis e os `.o` da pasta de entrega.** O `make` que rodei para validar
   deixou `cliente`, `servidor`, `carga` e sete `.o` em `ClienteServidor_Victor/`, e eu não
   tenho permissão para removê-los daqui. Rode `make clean` nessa pasta antes de compactar —
   o enunciado é explícito em não aceitar executáveis nem arquivos objeto.
2. **Trocar `SERVER_IP`** em `comum.h` pelo IP real da máquina do servidor e rodar `make`.
3. **Recompilar o PDF** depois das edições da seção 8.
4. Confirmar o nome completo do professor na referência [4] do `.tex`.
5. Refazer, se der tempo, a captura `03-cliente-a-cadastro.png` (é de execução diferente das
   outras).
