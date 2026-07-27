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
| **Fase atual** | Fase 1, estudo aprofundado dos códigos de aula e Fase 2 concluídos → técnica de concorrência decidida → seguindo para especificação/protocolo (Fase 3) |
| **Escopo definido** | **Tudo do enunciado é obrigatório.** A persistência era opcional pelo texto ("Gravação em Arquivo / Banco de Dados — **não obrigatório**"), mas **decidido manter** em arquivo `.txt` (seção 6). |
| **Técnica de concorrência** | **DECIDIDO: threads (`pthreads`)** — ver Registro de Decisões, seção 6 |
| **Trabalho individual** | Sim |

### Regras de entrega que NÃO podem falhar (do enunciado)
Estas eliminam o trabalho da correção se descumpridas. Tratar como sagradas:

1. Entregar um **.zip** com tudo num **único diretório**.
2. Dentro: `readme.txt` (nome do aluno + comando de execução), o **PDF da documentação**,
   e **todos os fontes** (`.c`, `.h`, `Makefile`). **Sem executáveis nem arquivos objeto.**
3. Um **Makefile** que, rodado sem parâmetros (`make`), gera **dois** programas chamados
   **exatamente** `cliente` e `servidor`.
4. Os programas iniciam **sem parâmetros**: `./cliente` e `./servidor`. O cliente descobre
   IP e porta **automaticamente**.
5. **O programa precisa compilar.** Se não compilar, não é corrigido.

---

## 1. Visão geral das fases

O trabalho está quebrado em 6 fases (as suas), cada uma com ações menores e um "pronto quando".
As fases seguem uma progressão: entender → especificar → construir → validar → aprofundar.

| Fase | Nome | Entregável ao fim | Status |
|---|---|---|---|
| 1 | Analisar os códigos de aula do Rubens | `docs/01_analise_codigos_aula.md` | ✅ Concluída |
| 2 | Destrinchar o enunciado em atividades | `docs/02_atividades.md` | ✅ Concluída (27/07, revisada após leitura do PDF) |
| 3 | Estudar soluções e decidir o caminho | `docs/03_especificacao.md` + `docs/03_protocolo.md` | 🟡 Próxima — técnica decidida (threads); protocolo a escrever |
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

> ✅ **Concluída em 27/07, revisada no mesmo dia após leitura direta de
> `trabalho_redes_2026.pdf`.** Resultado completo em `docs/02_atividades.md`. A primeira
> versão tinha sido montada só com o que já estava resumido neste guia; a revisão confirmou
> a maior parte e corrigiu um ponto importante de escopo (persistência) — ver seção 7.

Ações menores:
- [x] 2.1 Listar cada funcionalidade vista nos **prints**: login (`LOGIN_OK`), menu com
      exatamente `1 - Adicionar paciente`, `2 - Ver fila`, `3 - Heartbeat`, `0 - Sair`,
      Adicionar paciente (ID + Nome), Ver fila, Heartbeat, Sair, e as mensagens do servidor
      (`Servidor iniciado na porta 8080`, `Novo cliente conectado.`, `Cliente desconectado.`).
- [x] 2.2 Detalhar o comportamento do **Heartbeat** conforme a observação do enunciado:
      "devolver a lista de usuários cadastrados por **outros** clientes; se não houver novas
      inserções, devolver `ALIVE`." **Decisão confirmada por você (27/07):** seguir o texto da
      observação, não o print — o print diverge (mostra a fila inteira), mas o comportamento
      correto é só os pacientes cadastrados por outros clientes desde a última checagem.
      Documentar essa divergência como decisão de implementação (item 3 da doc).
- [x] 2.3 Detalhar o **broadcast**: quando um cliente adiciona paciente, os outros recebem
      `[Broadcast] Novo paciente: <nome>` (confirmado no print do 2º cliente).
- [x] 2.4 Marcar o que é **obrigatório** (núcleo) vs **opcional**. Confirmado no texto do
      enunciado: a introdução (cadastro completo de usuários, notificações além do broadcast,
      painéis administrativos, balanceamento de carga, métricas) é aspiracional e não aparece
      nos prints/observação/documentação — fora de escopo. Persistência era opcional pelo texto
      (seção SISTEMAS), mas **decidido manter** (arquivo `.txt`) — ver seção 6.
- [x] 2.5 Mapear cada funcionalidade para os **8 itens da documentação** exigida.
- [x] 2.6 Registrado em `docs/02_atividades.md` com ordem de implementação e prioridade.

**Pronto ✅.**

---

### FASE 3 — Estudar soluções e decidir o caminho (especificação) 🟡 Próxima

**Técnica de concorrência: DECIDIDO — threads.** Ver seção 6 (Registro de Decisões).
Funcionalidades e escopo já destrinchados em `docs/02_atividades.md` (Fase 2).

Ações menores restantes:
- [ ] 3.3 Definir a **estrutura de dados da fila** (ex.: lista/array de pacientes com ID e Nome)
      protegida por `pthread_mutex_t` (obrigatório com threads — ver `docs/00_estudo_codigos_aula.md`, 5.7).
- [ ] 3.4 Definir o **protocolo**: cada tipo de mensagem, formato exato dos bytes/campos,
      quem envia, o que espera de resposta. Ex.: `LOGIN`, `ADD ID Nome`, `LIST`, `HEARTBEAT`,
      respostas `LOGIN_OK`, `ALIVE`, etc. Usar o formato de fila confirmado no print
      (`===== FILA =====` / `ID - Nome` / `================`). Escrever em `docs/03_protocolo.md`.
- [ ] 3.5 Tratar **retransmissão de mensagens** (item 4 da documentação exige isso explicitamente).
- [ ] 3.6 Definir IP/porta fixos (o print mostra **porta 8080**) e como o cliente os obtém
      "automaticamente".
- [ ] 3.7 Implementar Heartbeat conforme **decidido**: apenas pacientes cadastrados por outros
      clientes desde a última checagem (`ALIVE` se não houver novidade) — documentar no protocolo
      a divergência com o print do enunciado (decisão de implementação, item 3 da doc).
- [ ] 3.8 Modelar a persistência (`.txt`) separando claramente dois tipos de dado — **decidido
      por você (27/07)**: dados de **cliente** (sessão/login: usuários, sessões, logs) são
      diferentes de dados de **paciente** (fila, histórico de inserções). O enunciado lista
      "usuários; filas; histórico; logs; sessões (dados do cliente que logou)" como o que
      persistir — "usuários"/"sessões"/"logs" são do **cliente** (software/quem logou), e
      "filas"/"histórico" são dos **pacientes** (conteúdo da fila).
- [ ] 3.9 Escrever `docs/03_especificacao.md` juntando decisões de arquitetura + protocolo.

**Pronto quando:** o protocolo estiver escrito por completo.

---

### FASE 4 — Implementar cliente e servidor

Ações menores (ordem de implementação):
- [ ] 4.1 `Makefile` mínimo que compila `cliente` e `servidor` com `-lpthread` (montar cedo, testar o `make`).
- [ ] 4.2 Servidor: criar socket, `SO_REUSEADDR`, `bind` na porta 8080, `listen`, `accept` em laço `while(1)`,
      logando `Servidor iniciado na porta 8080` / `Novo cliente conectado.` / `Cliente desconectado.`.
- [ ] 4.3 Cliente: `connect` (IPv4, 127.0.0.1:8080 automático), enviar `LOGIN`, receber `LOGIN_OK`, mostrar
      menu com as opções `1`/`2`/`3`/`0` exatamente como no print.
- [ ] 4.4 Servidor: `pthread_create` + `pthread_detach` por cliente conectado (base: `multithread.c`, com a correção do cast via `intptr_t`).
- [ ] 4.5 Funcionalidade "Adicionar paciente" (ID + Nome) + guardar na fila no servidor, protegida por mutex.
- [ ] 4.6 Funcionalidade "Ver fila" (servidor devolve a lista formatada: `===== FILA =====` / `ID - Nome` / `================`).
- [ ] 4.7 **Broadcast** direto para todos os clientes ao adicionar paciente, mantendo a lista
      de cada um atualizada em tempo real (`[Broadcast] Novo paciente: <nome>`).
- [ ] 4.8 **Heartbeat**: devolve os pacientes cadastrados por **outros** clientes desde a última
      checagem, ou `ALIVE` se não houver novidade (decidido — ver seção 6).
- [ ] 4.9 "Sair" limpo (fechar socket, servidor detecta desconexão via `recv() <= 0`).
- [ ] 4.10 Modularizar (`.c`/`.h`) e comentar — a avaliação cobra organização e modularidade.
- [ ] 4.11 **Persistência em arquivo `.txt`** (opcional pelo enunciado, mas **decidido manter**):
      grava dados de **cliente** (usuários/sessões/logs — quem logou) e dados de **paciente**
      (filas/histórico — conteúdo da fila), como dois conjuntos de dados distintos.

**Pronto quando:** cliente e servidor reproduzem o comportamento dos prints ponta a ponta.

---

### FASE 5 — Testar e validar

Ações menores:
- [ ] 5.1 Teste funcional manual: dois clientes, reproduzir a sequência dos prints do PDF.
- [ ] 5.2 Escrever o **gerador automático de clientes** (base: `porta.c` — mesma lógica de laço
      `socket()`/`connect()`/`close()`, trocando "variar porta" por "repetir N vezes na porta 8080").
      O enunciado permite explicitamente um 2º código cliente com parâmetro `100`/`1000`/`10000`.
- [ ] 5.3 Rodar carga de **100**, depois **1000**, depois **10000**; capturar prints mostrando
      todas as conexões.
- [ ] 5.4 Verificar com **Wireshark** que o tráfego bate com o protocolo documentado.
- [ ] 5.5 Testar casos especiais (cliente cai no meio, IDs repetidos, fila vazia no Heartbeat).
- [ ] 5.6 Registrar tudo em `docs/05_plano_testes.md` com prints e análise (itens 5, 6 e 7 da doc).

**Pronto quando:** os três testes de carga passam com prints salvos e o tráfego confere no Wireshark.

---

### FASE 6 — Estudar a fundo o código final

Ações menores:
- [ ] 6.1 Percorrer cada módulo e escrever, com suas palavras, o que cada função faz.
- [ ] 6.2 Saber explicar a técnica de concorrência escolhida (threads) e por quê — incluindo o
      motivo real (prazo) e os motivos técnicos (fila compartilhada nativa, familiaridade prévia).
- [ ] 6.3 Saber explicar o protocolo e como o Wireshark o veria.
- [ ] 6.4 Antecipar perguntas do professor e ensaiar respostas.
- [ ] 6.5 Registrar em `docs/06_estudo_aprofundado.md` (serve de cola para a apresentação).

**Pronto quando:** você conseguir explicar qualquer linha do código sem consultar.

---

## 4. Documentação final (o PDF obrigatório)

O PDF de documentação precisa conter **os 8 itens** abaixo. Vamos preenchendo ao longo das fases,
não tudo no fim. Mapeamento item → fase que gera o conteúdo:

| # | Item exigido | Fase que alimenta |
|---|---|---|
| 1 | Sumário do problema | 2 |
| 2 | Descrição dos algoritmos, TADs, funções e decisões | 3, 4 |
| 3 | Decisões de implementação omissas na especificação | 3, 4 |
| 4 | Como foi tratada a **retransmissão de mensagens** | 3 |
| 5 | Testes + análise | 5 |
| 6 | Teste de carga 100/1000/10000 (geração automática) + prints | 5 |
| 7 | Prints do funcionamento correto de cliente e servidor | 4, 5 |
| 8 | Conclusão e referências bibliográficas | 6 |

Referências dadas pelo enunciado (para o Makefile):
- http://www.gnu.org/software/make/manual/make.html
- http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

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
| 27/07 | **Correção de escopo:** o enunciado marca a persistência **inteira** (arquivo OU banco) como não obrigatória — não só o banco, como registrado em 23/07 | Leitura direta de `trabalho_redes_2026.pdf`, seção SISTEMAS: "Gravação em Arquivo / Banco de Dados (não obrigatório)" | 2 |
| **27/07** | **Persistência: mantida** (arquivo `.txt`), mesmo sendo opcional | Esforço baixo (mutex da fila já existe), cobre histórico/logs/sessões citados na introdução | 2 |
| **27/07** | **Heartbeat: segue o texto da observação**, não o print | O print do enunciado mostra o Heartbeat devolvendo a fila inteira, mas você confirmou que o comportamento correto é a lista de pacientes cadastrados por **outros** clientes desde a última checagem (`ALIVE` se não houver novidade). Divergência do print a documentar como decisão de implementação (item 3 da doc) | 3 |
| **27/07** | **Modelo de dados esclarecido: "cliente" ≠ "paciente"** | Você confirmou: os dados armazenados na persistência (usuários, sessões, logs) são do **cliente** (o software/sessão que fez login) — não dos **pacientes** (que são o conteúdo da fila, ID+Nome). O broadcast distribui a lista atualizada diretamente para todos os clientes | 3 |

---

## 7. Perguntas em aberto / pendências

- [x] ~~Adicionar os códigos de aula~~ — feito, agora 6 arquivos em `codigos_aula/` + 1 em `Redes_Computadores/`.
- [x] ~~Decidir a técnica de concorrência~~ — **decidido: threads (27/07)**.
- [x] ~~Corrigir menções a "4 arquivos" em `docs/01_analise_codigos_aula.md`~~ — corrigido em 27/07.
- [x] ~~Montar `docs/02_atividades.md` sem acesso ao PDF~~ — revisado em 27/07 com leitura direta do PDF.
- [x] ~~Persistência: manter ou cortar?~~ — **decidido manter**, em arquivo `.txt` (27/07).
- [x] ~~Heartbeat: texto ou print?~~ — **decidido seguir o texto** da observação (27/07); divergência do
      print vira decisão de implementação documentada (item 3 da doc).
- [x] ~~Modelo de dados da persistência~~ — **esclarecido (27/07):** "cliente" (usuários/sessões/logs —
      quem logou) é diferente de "paciente" (filas/histórico — conteúdo da fila).
- [ ] Confirmar detalhes do login: autenticação real (validar usuário/senha em arquivo) ou apenas
      `LOGIN_OK` fixo como no print? O print não mostra troca de credenciais — resolver ao escrever
      o protocolo (Fase 3).
- [ ] Confirmar a origem de `Redes_Computadores/` (os 4 arquivos corrigidos + `servidor_multiplexacao.c`):
      são do Rubens também, ou foram adaptados/corrigidos pelo aluno? Não bloqueia mais a decisão de
      concorrência, mas afeta como citar essas fontes na documentação final (item 8).

---

*Última atualização: 27/07/2026 — Fase 2 concluída e revisada após leitura direta do enunciado;
persistência mantida (.txt) e heartbeat decidido (segue o texto, não o print); modelo de dados
cliente vs. paciente esclarecido; `docs/01_analise_codigos_aula.md` corrigido; técnica de
concorrência decidida (threads); cronograma ajustado para 1,5 dia restante.*
