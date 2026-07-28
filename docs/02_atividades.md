# Fase 2 — Atividades (Destrinchar o Enunciado)

*Checklist priorizada de funcionalidades. Revisada duas vezes em 27/07: primeiro após leitura
direta de `trabalho_redes_2026.pdf`, depois após suas anotações de aula (terminologia, login,
persistência, itens da documentação). Ver `GUIA_MESTRE.md`, seção 0.1, para o texto completo
das anotações.*

---

## 1. Checklist de funcionalidades

| # | Funcionalidade | O que faz | Obrigatória/Opcional |
|---|---|---|---|
| F1 | **Login** | Cliente conecta; servidor responde `Servidor: LOGIN_OK`. **Decidido:** usuário/senha **fixos, hardcoded no código** (confirmado em aula: "deixar no código"; "senha já está no código"). Não é autenticação dinâmica contra arquivo/BD. | Obrigatória — mecanismo definido |
| F2 | **Menu principal** | Exibido após login, com **4 opções numeradas exatamente assim**: `1 - Adicionar usuário`, `2 - Ver fila`, `3 - Heartbeat`, `0 - Sair` (o print usa "paciente" e numeração 1/2/3/0 — nosso sistema usa "usuário", mesma numeração). | Obrigatória |
| F3 | **Adicionar usuário** | Cliente informa `ID` e `Nome` (ex.: `ID: 20`, `Nome: Carlos`); servidor confirma com `Usuário <nome> adicionado.`, guarda na fila compartilhada (protegida por `pthread_mutex_t`, técnica = threads) e dispara broadcast. (Print usa "paciente" — nosso sistema usa "usuário".) | Obrigatória |
| F4 | **Ver fila** | Servidor devolve a lista formatada. Formato exato visto no print: cabeçalho `===== FILA =====`, uma linha por usuário `ID - Nome`, rodapé `================`. | Obrigatória |
| F5 | **Broadcast** | Ao adicionar usuário, o servidor envia **diretamente a todos os clientes conectados, exceto o autor**, uma notificação assíncrona com o nome do novo usuário (chega no meio da tela, sem o cliente pedir): `[Broadcast] Novo usuário: <nome>` (print usa "paciente"). **Correção de 28/07:** a redação anterior dizia "a lista atualizada", o que contradizia o próprio formato da mensagem e o print do enunciado — o que trafega é a notificação do novo usuário, não a fila inteira. Ver nota de terminologia abaixo. | Obrigatória |
| F6 | **Heartbeat** | **Decidido:** segue a definição textual do enunciado (observação) — devolve a lista de usuários cadastrados por **outros** clientes desde a última checagem; se não houve novidade, devolve `ALIVE`. O print do 2º cliente mostra a fila inteira (não só os de outros), mas essa divergência **não** será reproduzida — documentar a escolha como decisão de implementação (item 3 da doc). | Obrigatória |
| F7 | **Sair** | Fecha a conexão de forma limpa; servidor detecta a desconexão via `recv() <= 0`. | Obrigatória |
| F8 | **Log de conexão/desconexão no servidor** | O servidor imprime no próprio terminal `Servidor iniciado na porta 8080`, `Novo cliente conectado.` e `Cliente desconectado.` a cada evento. Aparece no print (tela do servidor) — não é interno/opcional. | Obrigatória |
| F9 | **Retransmissão de mensagens** | **Decidido:** tratamento via **ACK de envio/recebimento** (anotação de aula para o item 4 da documentação, literalmente exigido). Detalhamento do mecanismo (timeout, número de sequência) — Fase 3. | Obrigatória (exigida pela documentação) |
| F10 | **Persistência (arquivo `.txt`)** | Rubens esclareceu em aula: é obrigatório escolher banco de dados **ou** arquivo (o "(não obrigatório)" do PDF se refere a poder trocar um pelo outro, não a pular os dois). **Decidido: arquivo `.txt`**, por ser mais básico. Grava dois tipos de dado distintos: dados de **cliente** — usuários-operadores, sessões, logs ("dados do cliente que logou") — e dados de **usuário** (ex-"paciente") — filas, histórico (conteúdo da fila). | Obrigatória |
| F11 | **IP/porta automáticos** | Porta fixa **8080** (confirmada no print: "Servidor iniciado na porta 8080"); cliente conecta sem parâmetros (`./cliente`, sem argumentos). **Confirmado:** servidor roda numa máquina, clientes em outras, mesma rede. **Decidido:** IP do servidor como constante `#define SERVER_IP` no código do cliente — recompilar antes de cada teste/apresentação em rede nova. Nenhum código de aula faz descoberta dinâmica, então fica fora de escopo por ora. Melhoria futura (se sobrar tempo): ler o IP de um arquivo texto, sem recompilar. | Obrigatória — mecanismo definido |

**Fora de escopo para este ciclo** (mencionado só na introdução do enunciado — cadastro de
usuários completo além do login fixo, notificações "em tempo real" no sentido de push além do
broadcast já coberto, painéis administrativos, balanceamento de carga, métricas): não aparecem
nos prints, na "Observação", nem nos 8 itens da documentação. Tratados como **fora de escopo**
dado o prazo de 1,5 dia.

**Nota sobre "atualizar filas em tempo real" (responsabilidade do cliente no enunciado).**
O enunciado lista, entre as responsabilidades do cliente, "atualizar filas em tempo real".
Isso foi implementado como **notificação**, não como réplica da fila: o cliente não mantém
uma cópia local da fila; ele é avisado imediatamente de cada inserção pelo broadcast e pode
pedir a fila completa a qualquer momento com a opção `2`. A decisão segue o print do
enunciado, que mostra exatamente `[Broadcast] Novo paciente: Carlos` — o nome do novo
registro, e não a lista inteira. Registrar no item 3 da documentação final.

**Nota de terminologia:** este documento usa "usuário" onde o print original do enunciado usa
"paciente" (ex.: "Adicionar paciente" → "Adicionar usuário"). Decisão confirmada em aula — ver
`GUIA_MESTRE.md`, seção 0.1. "Cliente" continua sendo um conceito diferente: a sessão/instância
do programa `./cliente` que faz login (com credencial fixa) e opera o sistema.

---

## 2. Mapeamento funcionalidade → item da documentação (dos 8 exigidos)

| Funcionalidade | Item(ns) da documentação que alimenta |
|---|---|
| F1, F2 (login, menu) | 1 (sumário do problema), 7 (prints de funcionamento) |
| F3, F4 (add usuário, ver fila) | 2 (algoritmos/TADs/decisões), 7 (prints) |
| F5 (broadcast) | 2, 3 (decisões de implementação) |
| F6 (heartbeat) | 2, 3 (documentar a divergência com o print), 7 |
| F7 (sair) | 2, 7 |
| F8 (log de conexão/desconexão) | 7 (prints do servidor) |
| F9 (retransmissão via ACK) | **4** (item dedicado, obrigatório — anotado "ack envio/recebimento") |
| F10 (persistência) | 3 (decisão de implementação — arquivo em vez de banco, e por quê) |
| F11 (IP/porta automáticos) | 3 |
| Testes de carga 100/1000/10000 | 5 (anotado "logs/printscreen"), 6 |
| Estudo aprofundado + apresentação | 8 (conclusão sobre desenvolvimento/dificuldades + referências) |

---

## 3. Ordem de prioridade sugerida (alinhada à Fase 4 do GUIA_MESTRE)

Dado 1,5 dia restante, a ordem de implementação já definida na Fase 4 é a prioridade: primeiro
o esqueleto que compila e conecta (F11, F1, F2, F8), depois o núcleo de negócio (F3, F4), depois
broadcast e heartbeat (F5, F6), depois saída limpa (F7), e só então retransmissão via ACK (F9) e
persistência (F10) — nessa ordem porque F9 e F10 dependem do protocolo e da estrutura de dados
já estarem funcionando. Diferente da revisão anterior: **persistência (F10) não é mais cortável**
— Rubens esclareceu que é obrigatório escolher arquivo ou banco.

---

## 4. Pendências que seguem para a Fase 3

- **Retransmissão via ACK (F9):** desenhar o mecanismo exato (timeout, número de sequência,
  quantas tentativas).
- **Estrutura de dados da persistência (F10):** definir o formato exato do `.txt` para os dois
  conjuntos de dados (cliente: usuários-operadores/sessões/logs; usuário: filas/histórico).
- **Credencial de login (F1):** definir o par usuário/senha fixo exato e como a mensagem `LOGIN`
  o carrega no protocolo.
- **IP do servidor (F11):** descobrir o IP real da máquina que vai rodar o servidor no dia do
  teste e definir o valor de `SERVER_IP` no código do cliente.

**Já decididas:** terminologia (usuário no lugar de paciente); heartbeat segue o texto do
enunciado; persistência em `.txt` (obrigatória, não cortável); login com credencial fixa no
código; IP do servidor fixo no código (`#define`). Ver `GUIA_MESTRE.md`, seções 0.1 e 6. Sem
pendências bloqueantes para começar a Fase 3.

---

*Fase 2 revisada em 27/07/2026 após leitura do PDF e anotações de aula do Rubens. Próximo:
Fase 3 — escrever `docs/03_protocolo.md` (estrutura de dados da fila + protocolo completo),
já com a técnica de concorrência decidida (threads) e as pendências acima resolvidas.*
