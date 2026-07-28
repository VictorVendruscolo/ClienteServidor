# Fase 4 — Implementação (versão 1)

*Registro do que foi construído, das decisões tomadas durante a codificação e das
divergências em relação à especificação da Fase 3. Alimenta diretamente os itens 2 e 3
da documentação final. Escrito em 27/07/2026, revisado em 28/07 após a simplificação.*

---

## 1. Situação

**Versão 1 completa e testada.** Todas as funcionalidades F1–F11 de `docs/02_atividades.md`
estão implementadas. O código compila com `gcc -Wall -Wextra` **sem nenhum aviso** e foi
validado em teste funcional, 21 casos especiais, teste de retransmissão e teste de carga
com 100, 1000 e 10000 clientes.

**Escopo:** robustez sobre as funcionalidades já especificadas — os prints são a base, não
o teto. Isso significa validação de toda entrada, tratamento de erro em toda chamada de
sistema e casos de borda cobertos; e **não** significa acrescentar as funcionalidades
aspiracionais da introdução do enunciado (painel administrativo, métricas, cadastro
dinâmico de operadores), que permanecem fora de escopo.

**Passe de simplificação (28/07).** Depois da primeira versão funcionando, o código passou
por uma revisão deliberada para reduzir a carga conceitual: 118 linhas removidas e sete
mecanismos eliminados (ver seção 5). O critério foi manter apenas o que é exigido pelo
enunciado ou indispensável para o protocolo funcionar, retirando proteções extras que
existiam por precaução e não por necessidade. Toda a bateria de testes foi repetida
depois, com os mesmos resultados.

## 2. Arquivos entregues (`src/`)

| Arquivo | Responsabilidade | Funções principais |
|---|---|---|
| `comum.h` | Constantes, tipo `Usuario`, textos do protocolo | — (somente definições) |
| `protocolo.c/.h` | Transporte de mensagens sobre TCP | `protocolo_le_linha`, `protocolo_envia_linha`, `protocolo_envia_fmt`, `protocolo_separa_comando`, `protocolo_limpa_bordas` |
| `fila.c/.h` | Fila compartilhada protegida por mutex | `fila_init`, `fila_adiciona`, `fila_tamanho`, `fila_copia_intervalo` |
| `sessoes.c/.h` | Registro de clientes conectados e broadcast | `sessoes_init`, `sessoes_registra`, `sessoes_remove`, `sessoes_broadcast`, `sessoes_trava_envio`, `sessoes_libera_envio`, `sessoes_conectadas` |
| `persistencia.c/.h` | Gravação nos quatro arquivos `.txt` | `persistencia_init`, `persistencia_log_servidor`, `persistencia_log_sessao`, `persistencia_historico_add`, `persistencia_salva_fila` |
| `servidor.c` | Aceitação de conexões e atendimento | `main`, `atende_cliente`, `autentica`, `trata_add`, `trata_list`, `trata_heartbeat`, `envia_bloco_fila`, `envia_resposta`, `verifica_sequencia`, `guarda_cache`, `nome_valido` |
| `cliente.c` | Interface de operação | `main`, `conecta_ao_servidor`, `autentica`, `thread_receptora`, `envia_comando`, `imprime_resposta`, `opcao_adicionar_usuario`, `opcao_ver_fila`, `opcao_heartbeat`, `converte_inteiro` |
| `carga.c` | Gerador automático de clientes (teste de carga) | `main`, `conecta`, `autentica` |
| `Makefile` | Compilação | alvos `all` (padrão), `carga`, `tudo`, `clean` |
| `readme.txt` | Nome do aluno e comandos de execução | — |

Diferença em relação à proposta da Fase 3 (`docs/03_especificacao.md`, seção 7): foi
acrescentado o módulo **`sessoes.c/.h`**, que não estava previsto, e não foram criados
`servidor.h` nem `cliente.h` — cabeçalho não faz sentido para um módulo que só contém o
`main` e funções privadas ao arquivo (todas declaradas `static`).

## 3. Elementos não previstos no enunciado, mas necessários

A seção "Avaliação" exige que qualquer elemento não especificado no enunciado, mas
necessário para o protocolo funcionar, seja descrito explicitamente. São estes:

1. **Tabela de sessões** (`sessoes.c`). O broadcast exige saber, a cada instante, quais
   sockets estão conectados e autenticados. Implementada como array de tamanho fixo
   (`MAX_SESSOES = 12000`), protegido por um `pthread_mutex_t`.

2. **Serialização de escrita por socket.** Sem ela, uma mensagem de broadcast disparada
   por outra thread poderia ser inserida no meio de uma resposta de várias linhas (o bloco
   `===== FILA =====`), quebrando o protocolo. Cada sessão tem um mutex de envio próprio,
   mantido durante todo o bloco.

3. **Regra para não haver impasse (*deadlock*).** O mutex de envio de uma sessão é obtido
   sem travar a tabela — quem chama é a própria thread dona da sessão, e só ela remove
   aquela entrada, então ela existe com certeza. A regra que sustenta isso: uma thread
   nunca remove a própria sessão enquanto segura o mutex de envio dela. No servidor isso é
   respeitado naturalmente, porque cada resposta trava e destrava antes de a conexão ser
   encerrada.

4. **`SIGPIPE` ignorado** em cliente e servidor. Escrever num socket que o outro lado já
   fechou gera esse sinal, cujo comportamento padrão é encerrar o processo inteiro.
   Ignorá-lo transforma a situação em um erro de retorno, que o código já trata.

5. **Pilha reduzida das threads** (256 KB, via `pthread_attr_setstacksize`). Com o padrão
   de 8 MB, 10000 threads reservariam 80 GB de espaço de endereçamento e o teste de carga
   seria impossível.

6. **Numeração das conexões nas linhas de log.** As mensagens começam exatamente com o
   texto das telas do enunciado (`Novo cliente conectado.` / `Cliente desconectado.`) e
   recebem, em seguida, o número sequencial e a origem — por exemplo,
   `Novo cliente conectado. [#4271] 192.168.0.20:51344`. Sem isso, o print do teste de
   10000 clientes seria uma parede de linhas idênticas, e o enunciado exige (item 6) que
   seja "possível identificar todas as conexões".

## 4. Decisões tomadas durante a implementação

| # | Decisão | Motivo |
|---|---|---|
| 4.1 | **Cliente também usa duas threads**: uma recebe do socket, a outra lê o teclado | Com um único fluxo, o programa ficaria preso no `fgets` e só veria o broadcast depois — o que contraria a exigência de atualização em tempo real. A thread receptora é a única que chama `recv()` |
| 4.2 | **Textos de protocolo em ASCII puro, exibição acentuada** | O enunciado especifica texto ASCII e o professor vai comparar a captura de rede com a documentação; bytes iguais em qualquer máquina evitam divergência por codificação. O cliente reescreve a mensagem com acentuação correta apenas ao exibi-la (`[Broadcast] Novo usuario:` na rede → `[Broadcast] Novo usuário:` na tela). Atende também ao critério de "uso correto da língua portuguesa" |
| 4.3 | **Respostas sem comando esperando são ignoradas** | Depois de reenviar um comando por tempo esgotado, as cópias da resposta ainda podem chegar. A regra é uma só: só se exibe uma resposta se houver um comando aguardando por ela; caso contrário é cópia atrasada e é descartada, para não ser confundida com a resposta do comando seguinte |
| 4.4 | **Cache de resposta apenas para respostas curtas** (256 bytes) | Guardar um bloco de fila inteiro por conexão custaria memória demais com 10000 clientes. `ADD_OK`, `ALIVE` e `ERRO` cabem no cache; blocos de fila são recalculados no reenvio |
| 4.5 | **`indice_visto_antes` guardado por conexão** | É o que permite recalcular exatamente o mesmo bloco quando um `HEARTBEAT` é reenviado — sem ele, o reenvio devolveria um intervalo diferente, já que o comando altera estado |
| 4.6 | **Fila lida em lotes de 32 registros ao enviar** | Mantém o mutex da fila travado por intervalos curtos: uma listagem longa não impede outros clientes de inserir |
| 4.7 | **Truncamento do nome feito uma única vez, no servidor** | Corrige defeito encontrado em teste: o nome era truncado ao entrar na fila, mas a confirmação e o histórico usavam o texto original. Agora fila, histórico, broadcast e resposta usam exatamente o mesmo texto |
| 4.8 | **Distinção entre "credencial recusada" e "encerrou antes de autenticar"** | No teste de carga sem autenticação, todas as conexões apareciam como falha de autenticação, o que sugeriria erro. São situações diferentes e agora são registradas como tais |
| 4.9 | **Socket do cliente passado à thread por ponteiro para `struct`** | Resolve, com margem, o defeito de portabilidade do `multithread.c` (converter `int` em `void*` quebra em 64 bits) e ainda permite levar o IP e o número da conexão junto |
| 4.10 | **Threads criadas já desatachadas** (`PTHREAD_CREATE_DETACHED`) | Equivalente a chamar `pthread_detach` logo após a criação, com uma chamada a menos por conexão — relevante em 10000 conexões |
| 4.11 | **`make carga` como alvo separado** | `make` sem parâmetros gera exatamente `cliente` e `servidor`, sem nenhum binário extra, eliminando risco com a correção automatizada |
| 4.12 | **Sem encerramento ordenado do servidor** | O servidor roda até ser interrompido. Como cada gravação nos arquivos é seguida de `fflush`, nada se perde ao encerrar, e o sistema operacional libera os recursos do processo. Evita código que nunca é executado |

## 5. Simplificação deliberada (28/07)

Sete mecanismos foram retirados depois da primeira versão funcionar. O motivo é explícito:
cada mecanismo a mais é um conceito a mais para justificar, e nenhum destes era exigido
pelo enunciado nem necessário para o protocolo funcionar.

| Removido | Por que existia | Por que saiu |
|---|---|---|
| `pthread_rwlock_t` na tabela de sessões | Permitir broadcasts simultâneos | Com dezenas de clientes a diferença é nula, e `pthread_mutex_t` é a primitiva vista em aula. A versão simplificada ficou até mais eficiente: o mutex de envio deixou de exigir a trava da tabela |
| `SO_SNDTIMEO` nos sockets | Evitar que um cliente que parou de ler bloqueasse uma thread | Risco teórico; nenhum teste o exercitou |
| Tratamento específico de `EMFILE`/`ENFILE` | Mensagem melhor ao esgotar descritores | Redundante: o tratamento genérico de erro do `accept()` já mantém o servidor de pé |
| `setrlimit` no gerador de carga | Elevar o limite de descritores sozinho | `ulimit -n 20000` antes do teste resolve, e já está no `readme.txt` |
| Tratador de `SIGINT` | Encerramento ordenado com Ctrl+C | Sem perda de dados, porque toda gravação é seguida de `fflush` |
| `fila_destroi`, `sessoes_destroi`, `persistencia_fecha` | Simetria com as funções de inicialização | Ficaram órfãs ao sair o tratador de `SIGINT`; código que nunca executa foi removido |
| Contador de respostas a descartar, no cliente | Contar quantas cópias atrasadas ignorar | Substituído por uma regra de três linhas, com o mesmo efeito: ignora-se qualquer resposta que chegue sem haver comando esperando |

Saldo: **1545 → 1427 linhas de código** (fora comentários). A bateria de testes foi repetida
por inteiro depois da simplificação, com resultados idênticos.

## 6. Consequência conhecida da regra do Heartbeat

A especificação (`docs/03_protocolo.md`, seção 5) determina que, ao fazer um `ADD`, o
cliente marque como "visto" tudo até o registro que ele acabou de criar — caso contrário o
`HEARTBEAT` devolveria a ele o próprio cadastro, contrariando a definição de "usuários
cadastrados por **outros** clientes".

Como o marcador é um único índice, isso tem um efeito colateral: se outro cliente inseriu
um usuário entre a última verificação e o meu `ADD`, esse registro passa a contar como
visto e não aparece no meu próximo `HEARTBEAT`. **Na prática não há perda de informação**,
porque esse mesmo registro já foi entregue ao cliente pela mensagem de broadcast, que é
imediata. A regra foi implementada exatamente como especificado; este parágrafo registra a
consequência para o item 3 da documentação final.

## 7. Testes realizados nesta fase

Todos executados em Linux, com servidor e clientes na mesma máquina (`127.0.0.1`), e
repetidos por inteiro após a simplificação da seção 5.

| Teste | Resultado |
|---|---|
| Compilação com `-Wall -Wextra` | Sem nenhum aviso |
| `make` sem parâmetros | Gera exatamente `cliente` e `servidor` |
| Sequência dos prints do enunciado, com 2 clientes | Reproduzida: login, menu, `ADD`, `LIST`, broadcast e heartbeat conferem |
| **21 casos especiais** | 21 de 21 aprovados (credencial errada, comando antes do login, comando desconhecido, `ADD` malformado, id não numérico, sem nome, sequência inválida/zero/fora de ordem, nome com espaços, nome acima do limite, fila vazia, queda abrupta de cliente) |
| **Reenvio de `ADD` com o mesmo `seq`** | Servidor repete a resposta e **não** insere de novo |
| **Reenvio de `HEARTBEAT` com o mesmo `seq`** | Devolve exatamente o mesmo bloco de antes |
| **Retransmissão vista do lado do cliente** | 3 tentativas a cada 3 s, sempre com o mesmo `seq`; aviso ao operador; respostas atrasadas descartadas; comando seguinte funciona normalmente |
| **Carga de 100, 1000 e 10000 clientes** | **0 falhas**; cada conexão numerada individualmente |
| **Carga de 10000 clientes autenticados** | 10000 sessões simultâneas, 10000 logins gravados, 0 falhas |
| Total acumulado numa só execução do servidor | **21100 conexões, 0 erros de `accept`**; servidor segue operando normalmente depois |
| Persistência | Os quatro arquivos gerados e consistentes com as operações |

## 8. Pendências para as fases seguintes

- **Fase 5:** repetir os testes com servidor e clientes em **máquinas diferentes**;
  capturar as telas exigidas (itens 6 e 7); conferir o tráfego no **Wireshark** contra
  `docs/03_protocolo.md`; consolidar em `docs/05_plano_testes.md`.
- **Antes da apresentação:** trocar `SERVER_IP` em `comum.h` pelo IP real da máquina do
  servidor e recompilar (`make`).
- **Fase 6:** conclusão sobre desenvolvimento e dificuldades, referências bibliográficas.
  O guia de defesa oral já está em `docs/06_estudo_aprofundado.md`.
- **Entrega:** reunir num único diretório os fontes de `src/` + `readme.txt` +
  `documentacao.pdf`, sem executáveis nem arquivos objeto, e compactar em `.zip`.

---

*Fase 4 concluída em 27/07/2026; simplificação e revalidação em 28/07/2026.*
