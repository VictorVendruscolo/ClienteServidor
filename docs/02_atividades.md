# Fase 2 — Atividades (Destrinchar o Enunciado)

*Checklist priorizada de funcionalidades. Versão revisada em 27/07 após leitura direta de
`trabalho_redes_2026.pdf` (a primeira versão deste documento foi montada sem acesso ao PDF,
só com o que já estava resumido no `GUIA_MESTRE.md` — esta revisão confirma e corrige alguns
pontos com base no texto e nos prints originais).*

---

## 1. Checklist de funcionalidades

| # | Funcionalidade | O que faz | Obrigatória/Opcional |
|---|---|---|---|
| F1 | **Login** | Cliente conecta; servidor responde `Servidor: LOGIN_OK`. O print não mostra troca de usuário/senha — só a resposta fixa. Mecanismo exato (autenticação real vs. resposta fixa) **ainda em aberto** — resolver na Fase 3. | Obrigatória (a funcionalidade); mecanismo a definir |
| F2 | **Menu principal** | Exibido após login, com **4 opções numeradas exatamente assim**: `1 - Adicionar paciente`, `2 - Ver fila`, `3 - Heartbeat`, `0 - Sair` (não é 1-2-3-4; o "Sair" é `0`). Relevante para o protocolo: o valor literal enviado pelo cliente provavelmente é esse número. | Obrigatória |
| F3 | **Adicionar paciente** | Cliente informa `ID` e `Nome` (ex.: `ID: 20`, `Nome: Carlos`); servidor confirma com `Paciente <nome> adicionado.`, guarda na fila compartilhada (protegida por `pthread_mutex_t`, técnica = threads) e dispara broadcast. | Obrigatória |
| F4 | **Ver fila** | Servidor devolve a lista formatada. Formato exato visto no print: cabeçalho `===== FILA =====`, uma linha por paciente `ID - Nome`, rodapé `================`. | Obrigatória |
| F5 | **Broadcast** | Ao adicionar paciente, o servidor distribui **diretamente para todos os clientes** conectados a lista atualizada, de forma assíncrona (chega no meio da tela, sem o cliente pedir): `[Broadcast] Novo paciente: <nome>`. | Obrigatória |
| F6 | **Heartbeat** | **Decidido (27/07):** segue a definição textual do enunciado (observação) — devolve a lista de pacientes cadastrados por **outros** clientes desde a última checagem; se não houve novidade, devolve `ALIVE`. O print do 2º cliente mostra a fila inteira (não só os de outros), mas essa divergência **não** será reproduzida — documentar a escolha como decisão de implementação (item 3 da doc). | Obrigatória |
| F7 | **Sair** | Fecha a conexão de forma limpa; servidor detecta a desconexão via `recv() <= 0`. | Obrigatória |
| F8 | **Log de conexão/desconexão no servidor** | O servidor imprime no próprio terminal `Servidor iniciado na porta 8080`, `Novo cliente conectado.` e `Cliente desconectado.` a cada evento. Aparece no print (tela do servidor) — não é interno/opcional. | Obrigatória |
| F9 | **Retransmissão de mensagens** | Tratamento de perda/reenvio — a documentação (item 4) exige explicitamente que isso seja descrito. Mecanismo a definir na Fase 3. | Obrigatória (exigida pela documentação, independente do print) |
| F10 | **Persistência (arquivo `.txt`)** | O enunciado marca a persistência inteira como não obrigatória ("Gravação em Arquivo / Banco de Dados (não obrigatório)"), mas **decidido manter (27/07)**. Grava dois tipos de dado distintos, conforme esclarecido por você: dados de **cliente** — usuários, sessões, logs ("dados do cliente que logou") — e dados de **paciente** — filas, histórico (conteúdo da fila). | Obrigatória (por decisão própria; enunciado permitiria cortar) |
| F11 | **IP/porta automáticos** | Porta fixa **8080** (confirmada no print: "Servidor iniciado na porta 8080"); cliente conecta sem parâmetros (`./cliente`, sem argumentos). | Obrigatória (regra de entrega) |

**Fora de escopo para este ciclo** (mencionado só na introdução do enunciado — cadastro de
usuários completo, notificações "em tempo real" no sentido de push além do broadcast já coberto,
painéis administrativos, balanceamento de carga, métricas): não aparecem nos prints, na
"Observação", nem nos 8 itens da documentação. Tratados como **fora de escopo** dado o prazo de
1,5 dia.

---

## 2. Mapeamento funcionalidade → item da documentação (dos 8 exigidos)

| Funcionalidade | Item(ns) da documentação que alimenta |
|---|---|
| F1, F2 (login, menu) | 1 (sumário do problema), 7 (prints de funcionamento) |
| F3, F4 (add paciente, ver fila) | 2 (algoritmos/TADs/decisões), 7 (prints) |
| F5 (broadcast) | 2, 3 (decisões de implementação) |
| F6 (heartbeat) | 2, 3 (documentar a divergência com o print), 7 |
| F7 (sair) | 2, 7 |
| F8 (log de conexão/desconexão) | 7 (prints do servidor) |
| F9 (retransmissão) | **4** (item dedicado, obrigatório) |
| F10 (persistência) | 3 (decisão de implementação — manter ou não, e por quê) |
| F11 (IP/porta automáticos) | 3 |
| Testes de carga 100/1000/10000 | 5, 6 |
| Estudo aprofundado + apresentação | 8 (conclusão + referências) |

---

## 3. Ordem de prioridade sugerida (alinhada à Fase 4 do GUIA_MESTRE)

Dado 1,5 dia restante, a ordem de implementação já definida na Fase 4 (ações 4.1 a 4.11) é a
prioridade: primeiro o esqueleto que compila e conecta (F11, F1, F2, F8), depois o núcleo de
negócio (F3, F4), depois broadcast e heartbeat (F5, F6), depois saída limpa (F7), e só então
retransmissão (F9) e persistência (F10) — nessa ordem porque F9 e F10 dependem do protocolo e
da estrutura de dados já estarem funcionando, e F10 é a única funcionalidade da lista que pode
ser cortada sem violar o enunciado, se o tempo apertar.

---

## 4. Pendências que seguem para a Fase 3

- **Mecanismo de login** (F1): autenticação real ou resposta fixa `LOGIN_OK`? Ainda em aberto.
- **Retransmissão de mensagens** (F9): mecanismo específico ainda não definido.
- **Estrutura de dados da persistência** (F10): definir o formato exato do `.txt` para os dois
  conjuntos de dados (cliente: usuários/sessões/logs; paciente: filas/histórico) na Fase 3.

**Já decididas (27/07):** Heartbeat segue o texto do enunciado (não o print); persistência
mantida em `.txt`. Ver `GUIA_MESTRE.md`, seção 6.

---

*Fase 2 revisada em 27/07/2026 após leitura direta do PDF do enunciado. Próximo: Fase 3 —
escrever `docs/03_protocolo.md` (estrutura de dados da fila + protocolo completo), já com a
técnica de concorrência decidida (threads).*
