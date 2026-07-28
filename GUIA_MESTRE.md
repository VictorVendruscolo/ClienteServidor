# Guia Mestre — Trabalho 1 de Redes de Computadores

**Plataforma de Monitoramento Inteligente de Filas em Tempo Real**
Cliente-Servidor em C · UEMS · Prof. Rubens · Aluno: Victor Vendruscolo

> Este é um **documento vivo**. Ele nos acompanha do começo ao fim. A cada avanço,
> atualizamos o status, registramos decisões e, se necessário, criamos outros documentos
> na pasta `docs/`. Princípio central do trabalho: **fazer o que o Rubens ensinou em
> aula, nem mais nem menos.**

---

## 0. Situação e prazos (atualizar sempre)

| Campo | Valor |
|---|---|
| **Data de hoje** | 27/07/2026 |
| **Entrega e apresentação** | **29/07/2026** |
| **Dias restantes** | 1,5 dia |
| **Fase atual** | Fases 1, 2 e 3 concluídas (protocolo e especificação fechados) → seguindo para implementação (Fase 4) |
| **Escopo definido** | **Tudo do enunciado é obrigatório.** Armazenamento: é preciso escolher banco de dados **ou** arquivo (não dá pra pular os dois) — **optamos por arquivo `.txt`**, por ser mais básico (confirmado em aula, seção 0.1). |
| **Técnica de concorrência** | **DECIDIDO: threads (`pthreads`)** — ver Registro de Decisões, seção 6 |
| **Terminologia do domínio** | O print usa "paciente"; **nosso sistema usa "usuário"** (o texto do enunciado já usa esse termo). Ver seção 0.1. |
| **Trabalho individual** | Sim |

### Regras de entrega que NÃO podem falhar (do enunciado)
Estas eliminam o trabalho da correção se descumpridas. Tratar como sagradas — **o trabalho é
corrigido por scripts de correção automatizados** (confirmado em aula), então nomes, parâmetros
e formatos exatos importam mais do que pareceria à primeira vista:

1. Entregar um **.zip** com tudo num **único diretório**.
2. Dentro: `readme.txt` (nome do aluno + comando de execução), o **PDF da documentação**,
   e **todos os fontes** (`.c`, `.h`, `Makefile`). **Sem executáveis nem arquivos objeto.**
3. Um **Makefile** que, rodado sem parâmetros (`make`), gera **dois** programas chamados
   **exatamente** `cliente` e `servidor`.
4. Os programas iniciam **sem parâmetros**: `./cliente` e `./servidor`. O cliente descobre
   IP e porta **automaticamente**. **Confirmado:** servidor roda numa máquina, clientes em
   outras, mesma rede. **Decidido: IP fixo no código** (`#define SERVER_IP`) — ver seção 6.
5. **O programa precisa compilar.** Se não compilar, não é corrigido.

---

## 0.1 Esclarecimentos do Rubens em aula (suas anotações, registradas em 27/07)

| Tema | Esclarecimento |
|---|---|
| **Terminologia** | O print usa "paciente" (`Adicionar paciente`, `Paciente Carlos adicionado`), mas o texto do enunciado já usa "usuário" (observação do Heartbeat: "lista de usuários"; introdução: "cadastro e autenticação de usuários"). Você confirmou: **trocamos "paciente" por "usuário"** no nosso sistema. Continua existindo, à parte, o "cliente" — o software/sessão que conecta e loga. São dois conceitos diferentes: **usuário** = quem entra na fila (ID + Nome, ex-"paciente"); **cliente** = a instância do programa `./cliente` que loga e opera o sistema. |
| **Cadastro e autenticação** | É para **"deixar no código"** — usuário/senha **fixos, embutidos no fonte**, não uma validação dinâmica contra arquivo/BD. Você também anotou, na tela de LOGIN_OK: "senha já está no código". **Isso fecha a pendência do mecanismo de login** (ver Fase 3 e seção 7). |
| **Armazenamento** | Rubens esclareceu verbalmente: é para **escolher** banco de dados OU arquivo — não para tratar a persistência como dispensável, como o parêntese isolado "(não obrigatório)" do PDF poderia sugerir. **Optamos por arquivo**, por ser mais básico. Isso substitui a leitura anterior deste guia (que tratava a persistência inteira como opcional/cortável). |
| **Item 4 da documentação** | Anotado como **"ack envio/recebimento"** — a retransmissão de mensagens deve ser tratada com confirmação (ACK) de envio/recebimento entre cliente e servidor. |
| **Item 5 da documentação** | Anotado como **"logs/printscreen"** — os testes devem ser evidenciados com logs e capturas de tela. |
| **Item 8 da documentação** | Anotado como **"conclusão do desenvolvimento, dificuldades..."** — a conclusão deve falar do processo de desenvolvimento e das dificuldades enfrentadas, não só resultado final. |
| **Referências bibliográficas (item 8)** | Você não tem certeza do que entra aqui. Provavelmente os dois links de Makefile já dados no enunciado, mais qualquer fonte técnica usada (man pages, documentação de `pthreads`, etc.). Baixa prioridade agora — resolver na Fase 6. |
| **Correção automatizada** | O trabalho é corrigido por **scripts de correção**, não só leitura humana — reforça por que Makefile e nomes/parâmetros exatos são inegociáveis, e sugere que os formatos de saída (texto exato dos prints, ex.: `LOGIN_OK`, `ALIVE`, `===== FILA =====`) devem ser seguidos à risca. |
| **Nível do relatório** | A documentação final deve ter **nível acadêmico**, no padrão dos relatórios que você já escreve para as bolsas. Nota para a Fase 6. |
| **Sobre os prints do enunciado** | Você anotou que a 3ª imagem (tela do servidor, página 4 do PDF) foi a **primeira execução** — ou seja, os prints não são uma única sessão sincronizada; são capturas ilustrativas separadas de execuções diferentes. |
| **Captura de pacotes** | O professor vai capturar o tráfego (Wireshark) e conferir contra o que **a documentação** descreve — reforça: documentar exatamente o protocolo implementado, não um ideal. |
| **Linguagem e ambiente** | Trabalho em **C**. **Confirmado:** servidor roda numa máquina, clientes em outras máquinas, mesma rede — não é só portabilidade de build. **Decidido:** IP do servidor fixo no código (`#define`), igual ao padrão de todos os códigos de aula do Rubens; se sobrar tempo, melhorar para ler de um arquivo texto (sem recompilar) — ver seção 6. |

---

## 1. Visão geral das fases

O trabalho está quebrado em 6 fases (as suas), cada uma com ações menores e um "pronto quando".
As fases seguem uma progressão: entender → especificar → construir → validar → aprofundar.

| Fase | Nome | Entregável ao fim | Status |
|---|---|---|---|
| 1 | Analisar os códigos de aula do Rubens | `docs/01_analise_codigos_aula.md` | ✅ Concluída |
| 2 | Destrinchar o enunciado em atividades | `docs/02_atividades.md` | ✅ Concluída (27/07, revisada 2x) |
| 3 | Estudar soluções e decidir o caminho | `docs/03_especificacao.md` + `docs/03_protocolo.md` | ✅ Concluída (27/07) |
| 4 | Implementar cliente e servidor | `src/` + `Makefile` | ⬜ A fazer |
| 5 | Testar e validar | `docs/05_plano_testes.md` + `testes/` | ⬜ A fazer |
| 6 | Estudar a fundo o código final | `docs/06_estudo_aprofundado.md` | ⬜ A fazer |
| — | Documentação final + entrega | `docs/documentacao.pdf` + `readme.txt` + `.zip` | ⬜ A fazer |
| — | Estudo teórico dos códigos de aula (7 arquivos) | `docs/00_estudo_codigos_aula.md` | ✅ Concluído (27/07) |

Legenda de status: ⬜ a fazer · 🟡 em andamento · ✅ concluída · ⛔ bloqueada

---

## 2. Cronograma sugerido (ajustado ao prazo real de 1,5 dia)

O cronograma original de 6 dias não se aplica mais. Com 1,5 dia restante, a prioridade
vira **implementar o núcleo funcionando primeiro**, protocolo mínimo viável, documentação
por último.

> Regra anti-perfeccionismo, agora ainda mais crítica: **primeiro faça funcionar o mínimo
> dos prints, depois refine.** Não há mais tempo para extras da introdução.

---

## 3. As fases em detalhe

### FASE 1 — Analisar os códigos de aula do Rubens

> ✅ **Concluída em 23/07.** Resultado completo em `docs/01_analise_codigos_aula.md`.
> Estudo teórico aprofundado dos 7 arquivos concluído em 27/07, ver `docs/00_estudo_codigos_aula.md`.

**Nota de atualização:** a análise original citava 4 arquivos do Rubens. O repositório
foi posteriormente atualizado com mais 2 arquivos em `codigos_aula/` (`escrevendo.c` e
`porta.c`), totalizando **6 arquivos de `codigos_aula/`**, mais 1 arquivo de multiplexação
(`servidor_multiplexacao.c`) presente em `Redes_Computadores/` cuja origem (professor ou
adaptação própria) segue **não confirmada**. Ver `docs/00_estudo_codigos_aula.md` para a
análise completa dos 7.

**Pronto ✅.**

---

### FASE 2 — Destrinchar o enunciado em atividades (a mais importante)

> ✅ **Concluída em 27/07, revisada duas vezes no mesmo dia:** primeiro após leitura direta de
> `trabalho_redes_2026.pdf`, depois após suas anotações de aula (terminologia, login, persistência,
> itens da documentação). Resultado completo em `docs/02_atividades.md`.

Ações menores:
- [x] 2.1 Listar cada funcionalidade vista nos **prints**: login (`LOGIN_OK`, senha fixa no
      código), menu com exatamente `1 - Adicionar usuário` (print usa "paciente", nós usamos
      "usuário"), `2 - Ver fila`, `3 - Heartbeat`, `0 - Sair`, e as mensagens do servidor
      (`Servidor iniciado na porta 8080`, `Novo cliente conectado.`, `Cliente desconectado.`).
- [x] 2.2 Detalhar o comportamento do **Heartbeat** conforme a observação do enunciado:
      "devolver a lista de usuários cadastrados por **outros** clientes; se não houver novas
      inserções, devolver `ALIVE`." **Decidido:** seguir o texto da observação, não o print — o
      print diverge (mostra a fila inteira). Documentar a divergência como decisão de
      implementação (item 3 da doc).
- [x] 2.3 Detalhar o **broadcast**: quando um cliente adiciona usuário, o servidor distribui a
      lista atualizada diretamente para todos os clientes (`[Broadcast] Novo usuário: <nome>`
      no nosso sistema; o print, com a terminologia antiga, mostra "Novo paciente").
- [x] 2.4 Marcar o que é **obrigatório** (núcleo) vs **opcional**. Confirmado no texto do
      enunciado: a introdução (cadastro completo de usuários além do login fixo, notificações
      além do broadcast, painéis administrativos, balanceamento de carga, métricas) é
      aspiracional e não aparece nos prints/observação/documentação — fora de escopo.
      Armazenamento: obrigatório escolher arquivo ou banco (esclarecido em aula) — optamos
      por arquivo `.txt`.
- [x] 2.5 Mapear cada funcionalidade para os **8 itens da documentação** exigida, já com as
      anotações de aula sobre os itens 4 ("ack envio/recebimento"), 5 ("logs/printscreen") e 8
      ("conclusão do desenvolvimento, dificuldades").
- [x] 2.6 Registrado em `docs/02_atividades.md` com ordem de implementação e prioridade.

**Pronto ✅.**

---

### FASE 3 — Estudar soluções e decidir o caminho (especificação) 🟡 Próxima

**Técnica de concorrência: DECIDIDO — threads.** Ver seção 6 (Registro de Decisões).
Funcionalidades e escopo já destrinchados em `docs/02_atividades.md` (Fase 2).

Ações menores:
- [x] 3.3 Estrutura de dados da fila **decidida**: array fixo `Usuario fila[20000]` (ID + Nome),
      protegido por `pthread_mutex_t` — ver `docs/03_protocolo.md`, seção 5.
- [x] 3.4 Protocolo completo escrito em `docs/03_protocolo.md`: cada mensagem, formato exato
      (texto ASCII terminado em `\n`, com número de sequência), quem envia, o que espera de
      resposta.
- [x] 3.4.1 Login: credencial de exemplo `admin`/`admin123`, fixa no código (trocável) —
      `docs/03_protocolo.md`, seção 3.
- [x] 3.5 Retransmissão decidida: **resposta = ACK implícito** + timeout de 3s / 3 tentativas +
      número de sequência para evitar duplicar — `docs/03_protocolo.md`, seção 6.
- [x] 3.6 IP/porta: porta **8080** fixa; IP do servidor como **constante `#define SERVER_IP`**
      no cliente (servidor e clientes rodam em máquinas diferentes na mesma rede — recompilar
      antes de cada apresentação/teste). Melhoria futura, se sobrar tempo: ler de arquivo texto.
- [x] 3.7 Heartbeat: apenas usuários cadastrados por **outros** clientes desde a última checagem
      (`ALIVE` senão) — decisão de implementação documentada (diverge do print, segue o texto).
- [x] 3.8 Persistência (`.txt`) modelada em 4 arquivos, separando dados de **cliente**
      (sessões/logs) e **usuário** (fila/histórico) — `docs/03_protocolo.md`, seção 7. Servidor
      sempre começa com a fila vazia (não recarrega).
- [x] 3.9 `docs/03_especificacao.md` escrito, juntando arquitetura + protocolo + proposta de
      módulos do código-fonte + rastreabilidade com os 8 itens da documentação.

**Pronto ✅.**

---

### FASE 4 — Implementar cliente e servidor

Ações menores (ordem de implementação):
- [ ] 4.1 `Makefile` mínimo que compila `cliente` e `servidor` com `-lpthread` (montar cedo, testar o `make`).
- [ ] 4.2 Servidor: criar socket, `SO_REUSEADDR`, `bind` na porta 8080, `listen`, `accept` em laço `while(1)`,
      logando `Servidor iniciado na porta 8080` / `Novo cliente conectado.` / `Cliente desconectado.`.
- [ ] 4.3 Cliente: `connect` (IPv4, porta 8080, IP do servidor via `#define SERVER_IP` — decidido
      na seção 6), enviar `LOGIN` com a credencial fixa, receber `LOGIN_OK`, mostrar menu com as
      opções `1`/`2`/`3`/`0` exatamente como no print (rótulos com "usuário" em vez de "paciente").
- [ ] 4.3.1 Antes de recompilar para a apresentação: descobrir o IP real da máquina que vai
      rodar o `servidor` (`ip a` / `hostname -I`) e atualizar `SERVER_IP` no cliente.
- [ ] 4.4 Servidor: `pthread_create` + `pthread_detach` por cliente conectado (base: `multithread.c`, com a correção do cast via `intptr_t`).
- [ ] 4.5 Funcionalidade "Adicionar usuário" (ID + Nome) + guardar na fila no servidor, protegida por mutex.
- [ ] 4.6 Funcionalidade "Ver fila" (servidor devolve a lista formatada: `===== FILA =====` / `ID - Nome` / `================`).
- [ ] 4.7 **Broadcast** direto para todos os clientes ao adicionar usuário, mantendo a lista
      de cada um atualizada em tempo real (`[Broadcast] Novo usuário: <nome>`).
- [ ] 4.8 **Heartbeat**: devolve os usuários cadastrados por **outros** clientes desde a última
      checagem, ou `ALIVE` se não houver novidade (decidido — ver seção 6).
- [ ] 4.9 "Sair" limpo (fechar socket, servidor detecta desconexão via `recv() <= 0`).
- [ ] 4.10 **Retransmissão de mensagens via ACK** (envio/recebimento confirmado) — item 4 da documentação.
- [ ] 4.11 Modularizar (`.c`/`.h`) e comentar — a avaliação cobra organização e modularidade.
- [ ] 4.12 **Persistência em arquivo `.txt`** (escolhida no lugar do banco): grava dados de
      **cliente** (usuários-operadores/sessões/logs — quem logou) e dados de **usuário**
      (filas/histórico — conteúdo da fila), como dois conjuntos de dados distintos.

**Pronto quando:** cliente e servidor reproduzem o comportamento dos prints ponta a ponta.

---

### FASE 5 — Testar e validar

Ações menores:
- [ ] 5.1 Teste funcional manual: dois clientes, reproduzir a sequência dos prints do PDF (adaptada
      para "usuário").
- [ ] 5.2 Escrever o **gerador automático de clientes** (base: `porta.c` — mesma lógica de laço
      `socket()`/`connect()`/`close()`, trocando "variar porta" por "repetir N vezes na porta 8080").
      O enunciado permite explicitamente um 2º código cliente com parâmetro `100`/`1000`/`10000`.
- [ ] 5.3 Rodar carga de **100**, depois **1000**, depois **10000**; capturar prints mostrando
      todas as conexões.
- [ ] 5.4 Verificar com **Wireshark** que o tráfego bate com o protocolo documentado.
- [ ] 5.5 Testar casos especiais (cliente cai no meio, IDs repetidos, fila vazia no Heartbeat).
- [ ] 5.6 Registrar tudo em `docs/05_plano_testes.md` com **logs e prints** (anotação de aula
      para o item 5) e análise (itens 5, 6 e 7 da doc).

**Pronto quando:** os três testes de carga passam com prints salvos e o tráfego confere no Wireshark.

---

### FASE 6 — Estudar a fundo o código final

Ações menores:
- [ ] 6.1 Percorrer cada módulo e escrever, com suas palavras, o que cada função faz.
- [ ] 6.2 Saber explicar a técnica de concorrência escolhida (threads) e por quê — incluindo o
      motivo real (prazo) e os motivos técnicos (fila compartilhada nativa, familiaridade prévia).
- [ ] 6.3 Saber explicar o protocolo e como o Wireshark o veria.
- [ ] 6.4 Antecipar perguntas do professor e ensaiar respostas.
- [ ] 6.5 Escrever a conclusão da documentação cobrindo **desenvolvimento e dificuldades**
      (anotação de aula, item 8), em **nível acadêmico** (padrão dos relatórios de bolsa).
- [ ] 6.6 Definir as **referências bibliográficas** do item 8 (provavelmente os 2 links de
      Makefile do enunciado + fontes técnicas usadas — confirmar).
- [ ] 6.7 Registrar em `docs/06_estudo_aprofundado.md` (serve de cola para a apresentação).

**Pronto quando:** você conseguir explicar qualquer linha do código sem consultar.

---

## 4. Documentação final (o PDF obrigatório)

O PDF de documentação precisa conter **os 8 itens** abaixo. Vamos preenchendo ao longo das fases,
não tudo no fim. Mapeamento item → fase que gera o conteúdo, já com as anotações de aula:

| # | Item exigido | Anotação de aula | Fase que alimenta |
|---|---|---|---|
| 1 | Sumário do problema | — | 2 |
| 2 | Descrição dos algoritmos, TADs, funções e decisões | — | 3, 4 |
| 3 | Decisões de implementação omissas na especificação | — | 3, 4 |
| 4 | Como foi tratada a **retransmissão de mensagens** | **ack envio/recebimento** | 3 |
| 5 | Testes + análise | **logs/printscreen** | 5 |
| 6 | Teste de carga 100/1000/10000 (geração automática) + prints | — | 5 |
| 7 | Prints do funcionamento correto de cliente e servidor | — | 4, 5 |
| 8 | Conclusão e referências bibliográficas | **conclusão do desenvolvimento, dificuldades**; referências ainda a definir | 6 |

Referências dadas pelo enunciado (para o Makefile, entram no item 8):
- http://www.gnu.org/software/make/manual/make.html
- http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

> **Nível do relatório:** acadêmico, no padrão dos relatórios de bolsa do Victor (anotação de aula).

---

## 5. Estrutura de arquivos do projeto

```
ClienteServidor/
├── GUIA_MESTRE.md            ← este arquivo (documento vivo)
├── README.md
├── trabalho_redes_2026.pdf   ← enunciado
├── codigos_aula/             ← códigos do Rubens (6 arquivos)
├── docs/                     ← documentos formais de cada fase
│   ├── 00_estudo_codigos_aula.md
│   ├── 01_analise_codigos_aula.md
│   ├── 02_atividades.md
│   ├── 03_especificacao.md
│   ├── 03_protocolo.md
│   ├── 05_plano_testes.md
│   ├── 06_estudo_aprofundado.md
│   └── documentacao.pdf      ← entregável final
├── src/                      ← código-fonte da entrega
│   ├── servidor.c / .h
│   ├── cliente.c / .h
│   └── Makefile
└── testes/                   ← gerador de carga, scripts, prints
```

> **Atenção na hora de zipar:** o enunciado pede **um único diretório** com os fontes.
> Na entrega, o conteúdo de `src/` (fontes + Makefile) + `readme.txt` + `documentacao.pdf`
> vão para uma pasta única, sem executáveis nem `.o`.

---

## 6. Registro de decisões (preencher ao longo do caminho)

Toda decisão técnica relevante fica aqui, com data e motivo. Serve para a documentação
(itens 2 e 3) e para você lembrar o porquê de cada escolha.

| Data | Decisão | Motivo | Fase |
|---|---|---|---|
| 23/07 | ~~Escopo = só núcleo~~ **revisado:** tudo do enunciado é obrigatório | Ajuste seu: o único opcional é o banco | 0 |
| 23/07 | Banco de dados → **arquivo .txt** | Enunciado marca o banco como não obrigatório; .txt cumpre o papel de persistência | 0 |
| 23/07 | Técnica de concorrência ainda em aberto, mas análise favorece **threads** | Threads escalam melhor no teste de 10000 E facilitam o estado compartilhado da fila (mesmo processo) | 1 |
| 27/07 | Repositório atualizado: mais 2 arquivos em `codigos_aula/` (`escrevendo.c`, `porta.c`) + 1 arquivo de multiplexação em `Redes_Computadores/` (`servidor_multiplexacao.c`, origem não confirmada) | Estudo teórico revisado para cobrir os 7 arquivos | 1 |
| **27/07** | **Decisão final: threads (`pthreads`)** | Restam 1,5 dia até a entrega. Threads é a técnica que o aluno já domina de disciplinas de SO, está 100% confirmada como ensinada pelo Rubens (`multithread.c`), e resolve fila compartilhada + broadcast sem IPC — só exige mutex. Multiplexação (`select`) foi descartada apesar de escalar melhor e não exigir mutex: técnica desconhecida do aluno e com origem não confirmada como material do professor — risco de cronograma e de defesa oral incompatível com o prazo. Fork foi descartado: fila compartilhada exigiria IPC, não ensinado em aula. | 3 |
| 27/07 | ~~Correção de escopo: persistência inteira seria opcional~~ **revisado de novo:** Rubens esclareceu em aula que é obrigatório escolher banco OU arquivo | Leitura isolada do PDF ("não obrigatório") sugeria que dava pra pular; anotação de aula corrigiu isso | 2 |
| **27/07** | **Persistência: arquivo `.txt`** (não banco) | Mais básico, esforço baixo com threads (mutex da fila já existe), cobre histórico/logs/sessões citados na introdução | 2 |
| **27/07** | **Heartbeat: segue o texto da observação**, não o print | O print do enunciado mostra o Heartbeat devolvendo a fila inteira, mas o comportamento correto é a lista de usuários cadastrados por **outros** clientes desde a última checagem (`ALIVE` se não houver novidade). Divergência do print a documentar como decisão de implementação (item 3 da doc) | 3 |
| **27/07** | **Terminologia: "paciente" (do print) → "usuário" (nosso sistema)** | O texto do enunciado já usa "usuário" (observação do Heartbeat, introdução); só o print de exemplo usa "paciente". Confirmado em aula. "Cliente" continua sendo um conceito à parte: a sessão/software que loga | 2 |
| **27/07** | **Login: usuário/senha fixos, hardcoded no código** | Confirmado em aula ("deixar no código"; "senha já está no código" no LOGIN_OK) — não é autenticação dinâmica contra arquivo/BD | 3 |
| **27/07** | **Retransmissão de mensagens via ACK de envio/recebimento** | Anotação de aula para o item 4 da documentação | 3 |
| **27/07** | **IP do servidor fixo no código (`#define SERVER_IP`)**, não descoberta dinâmica | Confirmado: servidor roda numa máquina, clientes em outras, mesma rede. Nenhum código de aula do Rubens faz descoberta de rede (todos usam IP literal) — implementar isso do zero seria fora do escopo ensinado e arriscado com 1,5 dia restante. Solução: recompilar o cliente com o IP certo antes de cada teste/apresentação. **Se sobrar tempo:** melhorar para ler o IP de um arquivo texto (não recompila) | 3 |
| **27/07** | **Protocolo v1 fechado**: mensagens texto/`\n`, fila = array fixo (20000), retransmissão = resposta como ACK + timeout 3s/3 tentativas + nº de sequência, persistência em 4 arquivos, fila sempre começa vazia, credencial de login `admin`/`admin123` | Discutido item a item com você; ver `docs/03_protocolo.md` para o racional completo de cada escolha | 3 |

---

## 7. Perguntas em aberto / pendências

- [x] ~~Adicionar os códigos de aula~~ — feito, agora 6 arquivos em `codigos_aula/` + 1 em `Redes_Computadores/`.
- [x] ~~Decidir a técnica de concorrência~~ — **decidido: threads (27/07)**.
- [x] ~~Corrigir menções a "4 arquivos" em `docs/01_analise_codigos_aula.md`~~ — corrigido em 27/07.
- [x] ~~Montar `docs/02_atividades.md` sem acesso ao PDF~~ — revisado em 27/07 com leitura direta do PDF.
- [x] ~~Persistência: manter ou cortar?~~ — **decidido manter, em arquivo `.txt`** (esclarecido em aula: não é opcional escolher nenhum dos dois).
- [x] ~~Heartbeat: texto ou print?~~ — **decidido seguir o texto** da observação; divergência do
      print vira decisão de implementação documentada (item 3 da doc).
- [x] ~~Modelo de dados da persistência~~ — **esclarecido:** "cliente" (usuários-operadores/sessões/logs)
      é diferente de "usuário" (ex-"paciente": filas/histórico — conteúdo da fila).
- [x] ~~Mecanismo de login~~ — **decidido: usuário/senha fixos, hardcoded no código** (anotação de aula).
- [x] ~~Terminologia "paciente" vs. "usuário"~~ — **decidido: usar "usuário"** no nosso sistema.
- [x] ~~Mecanismo de retransmissão~~ — **decidido: ACK de envio/recebimento** (anotação de aula,
      detalhamento técnico ainda a fazer na Fase 3).
- [x] ~~"Testado em máquinas diferentes": muda o requisito de IP/porta?~~ — **confirmado por você:**
      servidor num PC, clientes em outros PCs, mesma rede. **Decidido: IP fixo no código**
      (`#define SERVER_IP`), recompilando antes de cada apresentação/teste em rede nova. Melhoria
      futura (se sobrar tempo): ler o IP de um arquivo texto.
- [ ] Referências bibliográficas do item 8: prováveis (links de Makefile do enunciado + fontes
      técnicas), a confirmar na Fase 6. Baixa prioridade agora.
- [ ] Confirmar a origem de `Redes_Computadores/` (os 4 arquivos corrigidos + `servidor_multiplexacao.c`):
      são do Rubens também, ou foram adaptados/corrigidos pelo aluno? Não bloqueia mais a decisão de
      concorrência, mas afeta como citar essas fontes na documentação final (item 8).

---

*Última atualização: 27/07/2026 — Fase 3 concluída: `docs/03_protocolo.md` (mensagens,
estrutura de dados, retransmissão via ACK implícito, persistência em 4 arquivos, credencial de
login) e `docs/03_especificacao.md` (arquitetura + protocolo + proposta de módulos +
rastreabilidade com os 8 itens da documentação). Próximo: Fase 4 — implementação em C,
começando pelo Makefile e o esqueleto de conexão.*
