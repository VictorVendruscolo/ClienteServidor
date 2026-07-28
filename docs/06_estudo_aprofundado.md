# Fase 6 — Guia de estudo e defesa oral

*Cola para a apresentação. Ordem de leitura do código, o que cada peça faz, o caminho
completo de uma mensagem e as perguntas prováveis já com resposta. Tempo estimado de
estudo: 2 horas.*

---

## 1. Ordem de leitura (não leia na ordem alfabética)

| # | Arquivo | Tempo | O que você deve sair sabendo |
|---|---|---|---|
| 1 | `comum.h` | 5 min | O vocabulário: portas, limites, textos que trafegam na rede |
| 2 | `protocolo.h` | 10 min | Por que TCP exige delimitar mensagens e como resolvemos |
| 3 | `fila.h` + `fila.c` | 10 min | O dado central do sistema e por que ele precisa de mutex |
| 4 | `servidor.c` — `main` | 15 min | `socket → bind → listen → accept → thread`, o esqueleto da aula |
| 5 | `servidor.c` — `atende_cliente` | 15 min | O ciclo de vida completo de um cliente |
| 6 | `servidor.c` — `trata_add`, `trata_list`, `trata_heartbeat` | 20 min | A regra de negócio e a retransmissão |
| 7 | `sessoes.c` | 10 min | Como o broadcast encontra os outros clientes |
| 8 | `cliente.c` | 25 min | As duas threads e o reenvio por tempo esgotado |
| 9 | `persistencia.c` | 5 min | Quatro arquivos, um mutex |
| 10 | `carga.c` | 5 min | O laço de conexões, herdado do `porta.c` |

Leia **de cima para baixo dentro de cada arquivo**: os cabeçalhos de cada `.h` explicam o
*porquê* do módulo antes de você ver uma linha de código.

## 2. O que cada módulo faz, em duas frases

**`comum.h`** — Só definições. Reúne num lugar só as constantes (porta 8080, limites,
credencial) e os textos exatos do protocolo, para cliente e servidor nunca divergirem.

**`protocolo.c`** — Resolve o problema de que TCP é um fluxo de bytes, não uma sequência de
mensagens. Toda mensagem é uma linha terminada em `\n`; este módulo garante que o envio
saia inteiro e que a leitura devolva exatamente uma linha por chamada.

**`fila.c`** — Guarda os usuários num array de tamanho fixo, protegido por um mutex. Todo
acesso passa pelas funções deste arquivo, o que torna impossível alguém esquecer de travar.

**`sessoes.c`** — Sabe quais clientes estão conectados e autenticados, para o broadcast
encontrá-los. Cada sessão tem um mutex próprio de escrita, para uma resposta de várias
linhas não ser partida ao meio por uma notificação.

**`persistencia.c`** — Grava em quatro arquivos texto: dois de cliente (`sessoes.log`,
`servidor.log`) e dois de usuário (`fila.txt`, `historico.txt`).

**`servidor.c`** — A thread principal só aceita conexões e cria uma thread por cliente. Cada
thread autentica, registra a sessão e processa comandos até o cliente sair.

**`cliente.c`** — Uma thread lê o teclado e envia comandos; a outra lê o socket e imprime o
que chega. É essa separação que permite receber uma notificação mesmo com o operador parado.

**`carga.c`** — Abre N conexões de uma vez e as mantém abertas, para o teste de 100, 1000 e
10000 clientes.

## 3. O caminho completo de uma mensagem

Saber contar este percurso responde metade das perguntas possíveis. Cenário: o operador do
**cliente A** cadastra o usuário `10 / Abel`, e o **cliente B** vê a novidade.

1. **`cliente.c`, `opcao_adicionar_usuario`** — lê `ID` e `Nome` do teclado, valida (o ID
   precisa ser um número inteiro completo; o nome não pode ser vazio nem ter caracteres de
   controle) e monta a linha `ADD 1 10 Abel`. O `1` é o número de sequência.
2. **`cliente.c`, `envia_comando`** — marca que está esperando resposta, envia a linha e
   dorme por até 3 segundos aguardando um sinal da thread receptora.
3. **`protocolo_envia_linha`** — acrescenta `\n` e repete o `send()` até todos os bytes
   saírem. *É este byte a byte que aparece no Wireshark.*
4. **`servidor.c`, `atende_cliente`** — a thread daquele cliente está bloqueada em
   `protocolo_le_linha`, que acorda com a linha completa.
5. **`protocolo_separa_comando`** — separa `ADD` do resto (`1 10 Abel`).
6. **`servidor.c`, `trata_add`** — confere o número de sequência: é novo, então processa.
   Trunca o nome para o limite, insere na fila.
7. **`fila.c`, `fila_adiciona`** — trava o mutex, copia o registro para a próxima posição
   livre, destrava. **Este é o único ponto onde a fila é modificada.**
8. **`persistencia.c`** — grava a linha no histórico e reescreve o retrato da fila.
9. **`sessoes.c`, `sessoes_broadcast`** — percorre a tabela e envia
   `[Broadcast] Novo usuario: Abel` para todos, menos para quem cadastrou.
10. **`servidor.c`** — responde `ADD_OK 10 Abel` ao cliente A e guarda essa resposta no
    cache, caso o comando chegue repetido.
11. **`cliente.c`, `thread_receptora` (no A)** — recebe `ADD_OK`, formata como
    `Usuário Abel adicionado.`, sinaliza a thread principal, que acorda e mostra o menu.
12. **`cliente.c`, `thread_receptora` (no B)** — recebe a linha de broadcast e a imprime na
    hora, com o acento correto, mesmo que o operador do B esteja parado no menu.

## 4. As quatro perguntas difíceis, com a resposta pronta

**"Por que o cliente tem duas threads? Um cliente não é só enviar e receber?"**

Porque ele precisa fazer duas coisas ao mesmo tempo: esperar o operador digitar e receber
notificações que o servidor manda sozinho. Com um fluxo só, o programa fica preso no
teclado e a notificação de broadcast só apareceria quando o operador digitasse alguma
coisa — o que contraria a exigência de atualização em tempo real do enunciado. A thread
receptora é a única que chama `recv()`, então não há duas threads disputando os mesmos
bytes.

**"Por que cada sessão tem um mutex de envio?"**

Porque a resposta de "ver fila" tem várias linhas. Se um broadcast de outra thread entrasse
no meio dela, o cliente receberia o cabeçalho, uma linha de broadcast e depois o resto da
fila — e o protocolo se quebraria. O mutex fica travado do cabeçalho ao rodapé, garantindo
que o bloco saia inteiro.

**"O TCP já garante a entrega. Por que você implementou retransmissão?"**

*(A pergunta mais provável de todas, e a mais importante.)*

O TCP garante entrega ordenada **enquanto a conexão está de pé**, e garante que os bytes
chegaram à outra máquina — não que a aplicação do outro lado processou o pedido. Se o
servidor estiver vivo mas travado, ou se a conexão cair no meio, o TCP não resolve. A
retransmissão do enunciado é no nível da aplicação: cada resposta do servidor funciona como
confirmação do pedido; se ela não chega em 3 segundos, o cliente reenvia o mesmo comando,
até 3 vezes. Para o servidor não executar duas vezes, cada comando leva um número de
sequência: ao receber um número repetido, o servidor devolve a resposta anterior em vez de
processar de novo. Isso é o que torna a operação idempotente.

**"10000 threads não é pesado demais?"**

O peso é a pilha de cada thread. O padrão do sistema é 8 MB, o que daria 80 GB de espaço de
endereçamento e tornaria o teste impossível. Como cada thread aqui usa poucos kilobytes,
reduzi a pilha para 256 KB com `pthread_attr_setstacksize`, o que baixa o total para cerca
de 2,5 GB de memória *virtual* — reservada, não ocupada fisicamente. Testei com 10000
clientes simultâneos, inclusive todos autenticados, sem nenhuma falha.

## 5. Perguntas prováveis por tema

### Sockets

**Diferença entre o socket de escuta e o que o `accept` devolve?** O de escuta (`bind` +
`listen`) só recebe pedidos de conexão. O `accept` devolve um socket **novo**, exclusivo
daquele cliente. É essa separação que permite continuar aceitando enquanto se atende quem
já entrou — é o pilar de fork, threads e multiplexação.

**O `listen(sock, 512)` limita a 512 clientes?** Não. O 512 é o *backlog*: quantas conexões
já chegadas podem esperar na fila até o `accept` pegá-las. O limite de clientes atendidos é
outro (descritores de arquivo e memória).

**Para que serve `SO_REUSEADDR`?** Permite reiniciar o servidor imediatamente, sem esperar
a porta sair do estado `TIME_WAIT`. Sem isso, dá "Address already in use" ao reiniciar.

**Por que `htons`?** Converte o número da porta da ordem de bytes da máquina (little-endian
no x86) para a ordem da rede (big-endian). Sem isso, a porta 8080 chegaria trocada.

**Por que `inet_pton` e não `inet_addr`?** É mais segura: cobre IPv4 e IPv6 e distingue erro
de endereço válido. O `inet_addr` do `porta.c` não faz essa distinção.

**Como o servidor sabe que o cliente caiu?** O `recv()` devolve 0 quando o outro lado fecha
ordenadamente, e valor negativo em erro. Nos dois casos a thread sai do laço, remove a
sessão e fecha o socket.

### Threads e concorrência

**Por que threads e não `fork()`?** Porque a fila é compartilhada por todos os clientes.
Com `fork()`, cada processo tem memória isolada e seria preciso IPC (memória compartilhada),
que não foi ensinado em aula. Threads compartilham a memória do processo nativamente — o
custo é ter que proteger o acesso com mutex. O broadcast também fica direto, porque todas
as threads enxergam a mesma tabela de sessões.

**O que acontece se dois clientes adicionarem ao mesmo tempo?** Sem proteção, os dois
poderiam escrever na mesma posição e um sobrescreveria o outro. O `fila_adiciona` trava o
mutex antes de ler a posição livre e só destrava depois de gravar, então as inserções
acontecem uma de cada vez.

**Por que `pthread_detach` (ou criar já desatachada)?** Uma thread que termina fica com
recursos presos até alguém recolhê-la — é o equivalente ao processo zumbi do `fork()`.
Desatachar faz o sistema liberar automaticamente. Aqui isso é feito já na criação, pelos
atributos, o que economiza uma chamada por conexão.

**Por que `sched_yield()`?** Cede a CPU para a thread recém-criada começar a executar. Veio
do `multithread.c` da aula; não é necessário para a correção do programa, foi mantido por
fidelidade ao que foi ensinado.

### Protocolo

**Como você delimita as mensagens, se TCP é um fluxo de bytes?** Toda mensagem é uma linha
de texto terminada em `\n`. Na leitura, acumulo os bytes num buffer próprio até achar um
`\n`; o que sobra fica guardado para a próxima chamada. Nenhum dos códigos de aula precisa
disso, porque cada um troca só uma mensagem por conexão. As respostas de várias linhas usam
cabeçalho e rodapé fixos (`===== FILA =====` e `================`), então o cliente lê até
bater no rodapé sem precisar saber quantas linhas virão.

**O que o Wireshark vai mostrar?** Texto puro, exatamente as linhas da tabela de mensagens
da documentação: `LOGIN admin admin123`, `ADD 1 10 Abel`, `ADD_OK 10 Abel`, `HEARTBEAT 2`,
`ALIVE`. Sem criptografia e sem formato binário. Dá para seguir o *TCP Stream* de uma
conexão e ver a sessão inteira.

**Por que as mensagens da rede não têm acento?** Para que os bytes sejam idênticos em
qualquer máquina, independentemente da codificação do terminal — o que importa quando o
tráfego é comparado com a documentação. O cliente reescreve o texto com acentuação correta
só na hora de mostrar na tela.

**Por que o heartbeat não devolve a fila inteira, como no print?** Porque o texto do
enunciado é explícito: a função devolve "a lista de usuários cadastrados por **outros**
clientes", e `ALIVE` se não houver novidade. O print diverge do texto; segui o texto e
registrei a divergência como decisão de implementação. Cada cliente guarda um marcador de
até onde já viu a fila.

### Persistência e carga

**Por que arquivo e não banco de dados?** O enunciado deixa a escolha em aberto e o arquivo
é o suficiente para os dados pedidos. São quatro: `sessoes.log` e `servidor.log` guardam
dados de cliente (quem logou e quando); `fila.txt` guarda o retrato atual da fila e
`historico.txt` o registro permanente de todas as inserções.

**O servidor recupera a fila ao reiniciar?** Não. Ele sempre começa com a fila vazia — foi
uma decisão registrada na especificação, e é o que os prints do enunciado sugerem. Os
arquivos de log e histórico continuam existindo entre execuções.

**Como você gerou 10000 clientes?** Um segundo programa (`carga.c`), que o enunciado permite
expressamente. Ele repete `socket()` + `connect()` N vezes — a mesma estrutura de laço do
`porta.c` da aula, trocando "variar a porta" por "repetir na mesma porta" — e mantém todas
as conexões abertas até eu apertar ENTER, para dar tempo de capturar a tela.

**E se a fila encher?** O `fila_adiciona` devolve um código de fila cheia e o servidor
responde com `ERRO Fila cheia`, sem derrubar nada. O limite é 20000, definido em `comum.h`.

## 6. Se ele perguntar algo que você não sabe

Não invente. Diga onde a resposta está e ofereça mostrar: *"Isso está no `protocolo.c`, na
função de leitura de linha — posso abrir aqui."* Saber navegar o próprio código conta a seu
favor; inventar um mecanismo que não existe é o pior cenário possível.

Se ele apontar uma limitação real — o marcador único do heartbeat, a fila de tamanho fixo,
o IP fixo no código —, **concorde e mostre que está documentada**. Todas estão registradas
no item 3 da documentação, como o próprio enunciado exige.

## 7. Roteiro da demonstração ao vivo

1. `make` — mostra que gera exatamente `cliente` e `servidor`.
2. `./servidor` numa janela — aparece `Servidor iniciado na porta 8080`.
3. `./cliente` em duas outras janelas — `Conectado ao servidor.` / `Servidor: LOGIN_OK`.
4. No cliente A: opção `1`, cadastra `10 / Abel`. **Aponte a notificação surgindo sozinha
   na tela do cliente B** — é a prova do tempo real.
5. No cliente B: opção `1`, cadastra `20 / Carlos`; depois opção `2` para ver a fila com os
   dois.
6. No cliente A: opção `3` (heartbeat) — devolve só o `Carlos`, cadastrado pelo outro.
   Repita a opção `3` — agora devolve `ALIVE`. **Isso demonstra a regra do enunciado.**
7. Mostre `dados/fila.txt` e `dados/historico.txt` — a persistência.
8. `./carga 1000` — mostre a tela do servidor com as conexões numeradas.
9. Se houver Wireshark, siga o *TCP Stream* de uma conexão e compare com a tabela de
   mensagens da documentação.

**Antes de tudo isso:** confirme que o `SERVER_IP` em `comum.h` é o da máquina do servidor
e rode `make` de novo.

---

*Escrito em 28/07/2026. Complementos pendentes para o item 8 da documentação final:
conclusão sobre o desenvolvimento e as dificuldades, e as referências bibliográficas.*
