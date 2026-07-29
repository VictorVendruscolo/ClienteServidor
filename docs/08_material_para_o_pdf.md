# Material para montar o PDF da documentação

*Índice de onde está cada um dos 8 itens exigidos pelo enunciado. Escrito em 28/07/2026,
para que a redação do PDF possa começar sem depender de nenhuma conversa anterior.*

---

## Situação

O código está **pronto, testado e fechado**. Falta apenas o PDF. Nada do que é preciso
para escrevê-lo está fora dos arquivos deste repositório.

## Onde está cada item exigido

| # | Item do enunciado | Fonte principal | Situação |
|---|---|---|---|
| 1 | Sumário do problema | `03_especificacao.md` seção 1; `02_atividades.md` | ✅ pronto |
| 2 | Algoritmos, tipos de dados, funções e decisões | `04_implementacao.md` seções 2 e 4; `03_especificacao.md` seções 2–5 | ✅ pronto |
| 3 | Decisões de implementação omissas na especificação | `07_rastreabilidade.md` seção 6; `04_implementacao.md` seções 3, 4, 6.2 e 7 | ✅ pronto |
| 4 | Como foi tratada a retransmissão de mensagens | `03_protocolo.md` seção 6; `05_plano_testes.md` seção 4 | ✅ pronto |
| 5 | Testes e análise | `05_plano_testes.md` seções 2, 3 e 4 | ✅ pronto — falta anexar as capturas de tela |
| 6 | Carga de 100, 1000 e 10000 clientes | `05_plano_testes.md` seção 5 | ✅ pronto — falta anexar as capturas de tela |
| 7 | Telas do funcionamento correto | — | ⬜ **falta rodar e capturar** |
| 8 | Conclusão e referências | ver abaixo | ⬜ **falta escrever** |

## O que ainda precisa ser produzido

### Capturas de tela (itens 6 e 7)

Roteiro para gerar, já com o `SERVER_IP` ajustado e o `make` refeito:

1. Terminal do servidor logo após `./servidor` — mostra `Servidor iniciado na porta 8080`.
2. Dois clientes conectados: login, menu, cadastro de usuário nos dois.
3. A tela do cliente que **recebe** o `[Broadcast] Novo usuário: ...` sem ter pedido —
   é a prova visual do tempo real.
4. Opção `2` mostrando a fila completa.
5. Opção `3` num cliente: só os usuários de outros. Opção `3` de novo: `ALIVE`.
6. Um caso de erro, por exemplo nome longo demais ou identificador inválido.
7. Conteúdo de `dados/fila.txt` e `dados/historico.txt` — a persistência.
8. Carga: **reiniciar o servidor antes de cada uma** e capturar a tela dele com as
   conexões numeradas — três capturas, para 100, 1000 e 10000.

### Conclusão (item 8)

O professor pediu que a conclusão fale do **desenvolvimento e das dificuldades**, não só do
resultado. Material verdadeiro disponível para isso:

- **A delimitação de mensagens.** Nenhum dos códigos de aula precisa resolver isso, porque
  cada um troca uma única mensagem por conexão. Foi preciso projetar do zero a convenção de
  uma linha por mensagem e o buffer que junta os pedaços.
- **A escolha entre fork e threads.** Fork é a técnica mais detalhada em aula, mas a fila
  compartilhada exigiria comunicação entre processos, que não foi ensinada. Threads
  resolvem o compartilhamento de graça e cobram o mutex em troca.
- **O teste de 10000 clientes não funcionou de primeira.** Cada thread reserva 8 MB de
  pilha por padrão, o que daria 80 GB. A solução foi reduzir a pilha para 256 KB com
  `pthread_attr_setstacksize`.
- **Dois defeitos encontrados nos próprios testes:** o nome era cortado ao entrar na fila,
  mas a confirmação e o histórico usavam o texto original — passou a ser recusado em vez de
  cortado; e as conexões do teste de carga apareciam como "falha de autenticação" quando na
  verdade encerravam antes de tentar logar.
- **O código foi enxugado depois de funcionar.** Mecanismos que existiam por precaução, e
  não por necessidade, foram removidos — o registro está em `04_implementacao.md`, seções 5
  e 5.1. O programa saiu de 1545 para 1326 linhas sem perder nenhuma funcionalidade.
- **Sobre a retransmissão:** vale reconhecer no texto que, sobre uma conexão TCP única, o
  reenvio é em boa parte redundante, porque o TCP já retransmite segmentos perdidos. O que
  de fato agrega é o número de sequência, que impede um comando repetido de cadastrar o
  mesmo usuário duas vezes. Assumir isso é mais forte do que apresentar o mecanismo como
  indispensável.

### Referências (item 8)

Dadas pelo próprio enunciado, para o Makefile:

- http://www.gnu.org/software/make/manual/make.html
- http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

A acrescentar conforme o que for realmente citado no texto: páginas de manual do Linux
usadas (`socket`, `bind`, `listen`, `accept`, `send`, `recv`, `pthread_create`,
`pthread_mutex_init`, `pthread_cond_timedwait`, `sscanf`) e os códigos de exemplo da
disciplina, em `codigos_aula/`.

## Números para citar no texto

| Dado | Valor |
|---|---|
| Linhas de código (fora comentários) | 1326 |
| Módulos | 6 (`comum`, `protocolo`, `fila`, `sessoes`, `persistencia`, mais os dois programas) |
| Arquivos entregues | 14 (9 fontes, 3 cabeçalhos… ver `ClienteServidor_Victor/`) |
| Casos especiais testados | 30, todos aprovados |
| Conexões no maior teste de carga | 10000 simultâneas, autenticadas, 0 falhas |
| Avisos de compilação | nenhum, inclusive com o conjunto rigoroso de flags |
| Capacidade máxima da fila | 20000 usuários |
| Sessões simultâneas suportadas | 12000 |
| Porta | 8080 |

## Dois avisos para quem for escrever

1. **A pasta `ClienteServidor_Victor/` é a versão que vai no zip.** O código dela é idêntico ao de `src/`;
   só os comentários são mais curtos. O PDF deve descrever o código de `ClienteServidor_Victor/`.
2. **Nem tudo do enunciado foi implementado**, e isso é intencional e declarado: painéis
   administrativos, balanceamento de carga, papéis distintos de operador e posição numérica
   na fila ficaram fora de escopo. A lista completa, com a justificativa de cada um, está em
   `07_rastreabilidade.md`, seção 6, e **precisa** aparecer no item 3 do PDF — o enunciado
   exige que omissões sejam declaradas.

---

*A documentação deve ter nível acadêmico, no padrão dos relatórios de bolsa (anotação de
aula registrada em `GUIA_MESTRE.md`, seção 0.1).*
