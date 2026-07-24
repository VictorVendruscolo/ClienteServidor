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
| **Data de hoje** | 23/07/2026 |
| **Entrega e apresentação** | **29/07/2026** |
| **Dias restantes** | 6 |
| **Fase atual** | Fase 1 concluída → seguindo para Fase 2 |
| **Escopo definido** | **Tudo do enunciado é obrigatório**, exceto o banco de dados — que será substituído por **arquivo .txt** |
| **Técnica de concorrência** | **A DECIDIR na Fase 3** — análise aponta para `fork()` (didático) ou threads (escala melhor + estado compartilhado mais fácil). Ver `docs/01_analise_codigos_aula.md` |
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
| 2 | Destrinchar o enunciado em atividades | `docs/02_atividades.md` | 🟡 Próxima |
| 3 | Estudar soluções e decidir o caminho | `docs/03_especificacao.md` + `docs/03_protocolo.md` | ⬜ A fazer |
| 4 | Implementar cliente e servidor | `src/` + `Makefile` | ⬜ A fazer |
| 5 | Testar e validar | `docs/05_plano_testes.md` + `testes/` | ⬜ A fazer |
| 6 | Estudar a fundo o código final | `docs/06_estudo_aprofundado.md` | ⬜ A fazer |
| — | Documentação final + entrega | `docs/documentacao.pdf` + `readme.txt` + `.zip` | ⬜ A fazer |

Legenda de status: ⬜ a fazer · 🟡 em andamento · ✅ concluída · ⛔ bloqueada

---

## 2. Cronograma sugerido (6 dias)

Ajuste conforme sua rotina. A ideia é ter o **núcleo funcionando cedo** e deixar
documentação e extras para o fim.

| Dia | Data | Foco |
|---|---|---|
| 1 | 23/07 (hoje) | Planejamento (este guia) + Fase 1 (análise dos códigos de aula) + Fase 2 (atividades) |
| 2 | 24/07 | Fase 3: estudar concorrência, decidir técnica, escrever especificação + protocolo |
| 3 | 25/07 | Fase 4: implementar servidor (aceitar conexões, adicionar paciente, ver fila) |
| 4 | 26/07 | Fase 4: cliente + Heartbeat + broadcast; primeiro teste ponta a ponta |
| 5 | 27/07 | Fase 5: testes de carga 100/1000/10000 + gerador automático + prints |
| 6 | 28/07 | Documentação PDF + readme.txt + zip + Fase 6 (estudo). **Folga de 1 dia antes de 29/07.** |

> Regra anti-perfeccionismo: **primeiro faça funcionar o mínimo dos prints, depois refine.**
> Extras da introdução só entram depois que o núcleo estiver testado.

---

## 3. As fases em detalhe

### FASE 1 — Analisar os códigos de aula do Rubens
**Objetivo:** entender a base que os códigos do projeto vão herdar. Nada é implementado
sem antes entender como o Rubens faz sockets, laço de conexões e troca de mensagens.

> ✅ **Concluída em 23/07.** Resultado completo em `docs/01_analise_codigos_aula.md`.

O Rubens deu **4 arquivos**: `bind.c` (esqueleto do servidor), `fork.c` (múltiplos clientes por
processo), `multithread.c` (múltiplos clientes por thread) e `redes.c` (lado cliente). Resumo:

- [x] 1.1 Códigos em `codigos_aula/`.
- [x] 1.2 Mecânica de socket entendida (socket→bind→listen→accept no servidor; socket→connect→send/recv no cliente).
- [x] 1.3 Técnicas disponíveis identificadas: **fork** e **threads** (ambas prontas em aula).
- [x] 1.4 Os exemplos só ecoam bytes — **não há protocolo pronto**; teremos que projetá-lo (Fase 3).
- [x] 1.5 Mapeado o que reaproveitar (ver tabela de síntese na análise).
- [x] 1.6 Registrado em `docs/01_analise_codigos_aula.md`.

**Achados importantes:** nenhum código de aula tem estado compartilhado, broadcast, heartbeat
nem protocolo — tudo isso é projeto nosso na Fase 3. Os códigos do professor têm alguns erros de
digitação (documentados na análise) que sairão corrigidos na nossa versão.

**Pronto ✅.**

---

### FASE 2 — Destrinchar o enunciado em atividades (a mais importante)
**Objetivo:** transformar o PDF numa lista ordenada de funcionalidades concretas a implementar,
separando o que é **obrigatório** (aparece nos prints/observação) do que é **opcional**
(só na introdução). Isso evita fazer de menos (perder nota) ou de mais (fugir do escopo).

Ações menores:
- [ ] 2.1 Listar cada funcionalidade vista nos **prints**: login (`LOGIN_OK`), menu de 4 opções,
      Adicionar paciente (ID + Nome), Ver fila, Heartbeat, Sair.
- [ ] 2.2 Detalhar o comportamento do **Heartbeat** conforme a observação do enunciado:
      "devolver a lista de usuários cadastrados por **outros** clientes; se não houver novas
      inserções, devolver `ALIVE`."
- [ ] 2.3 Detalhar o **broadcast**: quando um cliente adiciona paciente, os outros recebem
      `[Broadcast] Novo paciente: <nome>` (visto no print do 2º cliente).
- [ ] 2.4 Marcar o que é **obrigatório** (núcleo) vs **opcional** (autenticação real, persistência,
      métricas, painéis — da introdução).
- [ ] 2.5 Mapear cada funcionalidade para os **8 itens da documentação** exigida.
- [ ] 2.6 Registrar em `docs/02_atividades.md` com ordem de implementação e prioridade.

**Pronto quando:** existir uma checklist priorizada de funcionalidades, cada uma com
"o que faz" e "obrigatória/opcional".

---

### FASE 3 — Estudar soluções e decidir o caminho (especificação)
**Objetivo:** com base na Fase 1 (o que o Rubens ensinou) e na Fase 2 (o que é pedido),
decidir tecnicamente como o código será e **definir o protocolo de mensagens** — este último
é crítico porque o professor vai capturar o tráfego com **Wireshark** e conferir aderência.

Ações menores:
- [ ] 3.1 **Estudar concorrência** (você pediu para estudar antes de decidir): comparar
      multiplexação (`select`/`poll`), `fork()` e threads (`pthreads`). Critérios: o que o
      Rubens usou, e o teste de carga de **10000 clientes** (fork/thread pesam; select escala melhor).
- [ ] 3.2 **Decidir a técnica** e registrar o porquê na seção de Decisões deste guia.
- [ ] 3.3 Definir a **estrutura de dados da fila** (ex.: lista/array de pacientes com ID e Nome)
      e como o servidor guarda o estado compartilhado entre clientes.
- [ ] 3.4 Definir o **protocolo**: cada tipo de mensagem, formato exato dos bytes/campos,
      quem envia, o que espera de resposta. Ex.: `LOGIN`, `ADD ID Nome`, `LIST`, `HEARTBEAT`,
      respostas `LOGIN_OK`, `ALIVE`, etc. Escrever em `docs/03_protocolo.md`.
- [ ] 3.5 Tratar **retransmissão de mensagens** (item 4 da documentação exige isso explicitamente).
- [ ] 3.6 Definir IP/porta fixos (o print mostra **porta 8080**) e como o cliente os obtém
      "automaticamente".
- [ ] 3.7 Escrever `docs/03_especificacao.md` juntando decisões de arquitetura + protocolo.

**Pronto quando:** o protocolo estiver escrito por completo e a técnica de concorrência decidida
e justificada.

---

### FASE 4 — Implementar cliente e servidor
**Objetivo:** codificar exatamente conforme a especificação da Fase 3, sem fugir do estilo do
Rubens. Construir incremental: primeiro o esqueleto que conecta, depois cada funcionalidade.

Ações menores (ordem de implementação):
- [ ] 4.1 `Makefile` mínimo que compila `cliente` e `servidor` (montar cedo, testar o `make`).
- [ ] 4.2 Servidor: criar socket, `bind` na porta 8080, `listen`, `accept` (uma conexão só, por ora).
- [ ] 4.3 Cliente: `connect`, enviar `LOGIN`, receber `LOGIN_OK`, mostrar menu de 4 opções.
- [ ] 4.4 Servidor: aplicar a técnica de múltiplos clientes decidida na Fase 3.
- [ ] 4.5 Funcionalidade "Adicionar paciente" (ID + Nome) + guardar na fila no servidor.
- [ ] 4.6 Funcionalidade "Ver fila" (servidor devolve a lista formatada).
- [ ] 4.7 **Broadcast** para os outros clientes ao adicionar paciente.
- [ ] 4.8 **Heartbeat** conforme a observação (novos de outros clientes, ou `ALIVE`).
- [ ] 4.9 "Sair" limpo (fechar socket, servidor detecta desconexão).
- [ ] 4.10 Modularizar (`.c`/`.h`) e comentar — a avaliação cobra organização e modularidade.
- [ ] 4.11 **Persistência em arquivo .txt** (no lugar do banco): gravar usuários/filas/histórico/logs/sessões.

**Pronto quando:** cliente e servidor reproduzem o comportamento dos prints ponta a ponta.

---

### FASE 5 — Testar e validar
**Objetivo:** provar que funciona conforme a especificação, incluindo o **teste de carga**
obrigatório (100 / 1000 / 10000 clientes com geração automática) e prints.

Ações menores:
- [ ] 5.1 Teste funcional manual: dois clientes, reproduzir a sequência dos prints do PDF.
- [ ] 5.2 Escrever o **gerador automático de clientes** (o enunciado permite um 2º cliente que
      recebe `100`/`1000`/`10000` como parâmetro).
- [ ] 5.3 Rodar carga de **100**, depois **1000**, depois **10000**; capturar prints mostrando
      todas as conexões.
- [ ] 5.4 Verificar com **Wireshark** que o tráfego bate com o protocolo documentado.
- [ ] 5.5 Testar casos especiais (cliente cai no meio, IDs repetidos, fila vazia no Heartbeat).
- [ ] 5.6 Registrar tudo em `docs/05_plano_testes.md` com prints e análise (itens 5, 6 e 7 da doc).

**Pronto quando:** os três testes de carga passam com prints salvos e o tráfego confere no Wireshark.

---

### FASE 6 — Estudar a fundo o código final
**Objetivo:** você dominar cada parte do código para **apresentar e explicar em laboratório**
(a avaliação exige apresentação oral). Não é reescrever — é entender de verdade.

Ações menores:
- [ ] 6.1 Percorrer cada módulo e escrever, com suas palavras, o que cada função faz.
- [ ] 6.2 Saber explicar a técnica de concorrência escolhida e por quê.
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
| 8 | Conclusão + referências bibliográficas | 6 |

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
├── codigos_aula/             ← códigos do Rubens (a adicionar)
├── docs/                     ← documentos formais de cada fase
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
| — | *(decisão final de concorrência — Fase 3)* | | 3 |

---

## 7. Perguntas em aberto / pendências

- [x] ~~Adicionar os códigos de aula~~ — feito, 4 arquivos em `codigos_aula/`.
- [x] ~~Persistência~~ — decidido: **arquivo .txt** no lugar do banco.
- [ ] **Decidir a técnica de concorrência** na Fase 3 (fork vs threads). Análise favorece threads.
- [ ] Confirmar detalhes do login: autenticação real (validar usuário/senha em arquivo) ou apenas
      `LOGIN_OK` fixo como no print? — resolver no início da Fase 3.

---

*Última atualização: 23/07/2026 — Fase 1 concluída.*
