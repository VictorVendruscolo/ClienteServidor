# Fase 4 — Implementação (versão 1)

*Registro do que foi construído, das decisões tomadas durante a codificação e das
divergências em relação à especificação da Fase 3. Alimenta diretamente os itens 2 e 3
da documentação final. Data: 27/07/2026.*

---

## 1. Situação

**Versão 1 completa e testada.** Todas as funcionalidades F1–F11 de `docs/02_atividades.md`
estão implementadas. O código compila com `gcc -Wall -Wextra` **sem nenhum aviso** e foi
validado em teste funcional, teste de casos especiais, teste de retransmissão e teste de
carga com 100, 1000 e 10000 clientes.

Escopo confirmado com o Victor antes de começar: **robustez sobre o escopo já
especificado** — os prints são a base, não o teto. Isso significa validação de toda
entrada, tratamento de erro em toda chamada de sistema, casos de borda cobertos e
modularização real; e **não** significa acrescentar as funcionalidades aspiracionais da
introdução do enunciado (painel administrativo, métricas, cadastro dinâmico de operadores),
que permanecem fora de escopo.

## 2. Arquivos entregues (`src/`)

| Arquivo | Responsabilidade | Funções principais |
|---|---|---|
| `comum.h` | Constantes, tipo `Usuario`, textos do protocolo | — (somente definições) |
| `protocolo.c/.h` | Transporte de mensagens sobre TCP | `protocolo_le_linha`, `protocolo_envia_linha`, `protocolo_envia_fmt`, `protocolo_separa_comando`, `protocolo_limpa_bordas` |
| `fila.c/.h` | Fila compartilhada protegida por mutex | `fila_init`, `fila_adiciona`, `fila_tamanho`, `fila_copia_intervalo`, `fila_destroi` |
| `sessoes.c/.h` | Registro de clientes conectados e broadcast | `sessoes_init`, `sessoes_registra`, `sessoes_remove`, `sessoes_broadcast`, `sessoes_trava_envio`, `sessoes_libera_envio`, `sessoes_conectadas` |
| `persistencia.c/.h` | Gravação nos quatro arquivos `.txt` | `persistencia_init`, `persistencia_log_servidor`, `persistencia_log_sessao`, `persistencia_historico_add`, `persistencia_salva_fila`, `persistencia_fecha` |
| `servidor.c` | Aceitação de conexões e atendimento | `main`, `atende_cliente`, `autentica`, `trata_add`, `trata_list`, `trata_heartbeat`, `envia_bloco_fila`, `envia_resposta`, `verifica_sequencia`, `encerra_servidor` |
| `cliente.c` | Interface de operação | `main`, `conecta_ao_servidor`, `autentica`, `thread_receptora`, `envia_comando`, `imprime_resposta`, `opcao_adicionar_usuario`, `opcao_ver_fila`, `opcao_heartbeat` |
| `carga.c` | Gerador automático de clientes (teste de carga) | `main`, `conecta`, `autentica`, `amplia_limite_descritores` |
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
   (`MAX_SESSOES = 12000`), protegido por um `pthread_rwlock_t`.

2. **Serialização de escrita por socket.** Sem ela, uma mensagem de broadcast disparada
   por outra thread poderia ser inserida no meio de uma resposta de várias linhas (o bloco
   `===== FILA =====`), quebrando o protocolo. Cada sessão tem um mutex de envio próprio,
   mantido durante todo o bloco.

3. **Hierarquia de travas.** Para impedir impasse (*deadlock*), as travas são sempre
   adquiridas na mesma ordem: `rwlock da tabela → mutex de envio da sessão → mutex da
   fila`. Por isso `sessoes_trava_envio()` adquire as duas primeiras juntas.

4. **`SIGPIPE` ignorado** em cliente e servidor. Escrever num socket que o outro lado já
   fechou gera esse sinal, cujo comportamento padrão é encerrar o processo inteiro.
   Ignorá-lo transforma a situação em um erro de retorno, que o código já trata.

5. **Pilha reduzida das threads** (256 KB, via `pthread_attr_setstacksize`). Com o padrão
   de 8 MB, 10000 threads reservariam 80 GB de espaço de endereçamento e o teste de carga
   seria impossível.

6. **`SO_SNDTIMEO` de 2 s** nos sockets de cliente. Impede que um cliente que parou de ler
   bloqueie indefinidamente uma thread do servidor e, com ela, os broadcasts aos demais.

7. **Tolerância a `EMFILE`/`ENFILE` no `accept()`**. Atingir o limite de descritores não
   derruba o servidor: a conexão é recusada, o evento é registrado e o laço continua.

8. **Numeração das conexões nas linhas de log.** As mensagens começam exatamente com o
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
| 4.3 | **Descarte de respostas duplicadas no cliente** | Depois de reenviar um comando por tempo esgotado, as cópias da resposta ainda podem chegar. O cliente conta quantos envios fez e ignora as respostas excedentes, para que não sejam confundidas com a resposta do comando seguinte |
| 4.4 | **Cache de resposta apenas para respostas curtas** (256 bytes) | Guardar um bloco de fila inteiro por conexão custaria memória demais com 10000 clientes. `ADD_OK`, `ALIVE` e `ERRO` cabem no cache; blocos de fila são recalculados no reenvio |
| 4.5 | **`indice_visto_antes` guardado por conexão** | É o que permite recalcular exatamente o mesmo bloco quando um `HEARTBEAT` é reenviado — sem ele, o reenvio devolveria um intervalo diferente, já que o comando altera estado |
| 4.6 | **Fila lida em lotes de 32 registros ao enviar** | Mantém o mutex da fila travado por intervalos curtos: uma listagem longa não impede outros clientes de inserir |
| 4.7 | **Truncamento do nome feito uma única vez, no servidor** | Corrige defeito encontrado em teste: o nome era truncado ao entrar na fila, mas a confirmação e o histórico usavam o texto original. Agora fila, histórico, broadcast e resposta usam exatamente o mesmo texto |
| 4.8 | **Distinção entre "credencial recusada" e "encerrou antes de autenticar"** | No teste de carga sem autenticação, todas as conexões apareciam como falha de autenticação, o que sugeriria erro. São situações diferentes e agora são registradas como tais |
| 4.9 | **Socket do cliente passado à thread por ponteiro para `struct`** | Resolve, com margem, o defeito de portabilidade do `multithread.c` (converter `int` em `void*` quebra em 64 bits) e ainda permite levar o IP e o número da conexão junto |
| 4.10 | **Threads criadas já desatachadas** (`PTHREAD_CREATE_DETACHED`) | Equivalente a chamar `pthread_detach` logo após a criação, com uma chamada a menos por conexão — relevante em 10000 conexões |
| 4.11 | **`make carga` como alvo separado** | `make` sem parâmetros gera exatamente `cliente` e `servidor`, sem nenhum binário extra, eliminando risco com a correção automatizada |
| 4.12 | **Encerramento por `SIGINT` (Ctrl+C) fecha os arquivos** | Sem isso, os dados dos arquivos `.txt` poderiam ficar incompletos ao encerrar o servidor |

## 5. Consequência conhecida da regra do Heartbeat

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

## 6. Testes já realizados nesta fase

Todos executados em Linux, com servidor e clientes na mesma máquina (`127.0.0.1`).

| Teste | Resultado |
|---|---|
| Compilação com `-Wall -Wextra` | Sem nenhum aviso |
| `make` sem parâmetros | Gera exatamente `cliente` e `servidor` |
| Sequência dos prints do enunciado, com 2 clientes | Reproduzida: login, menu, `ADD`, `LIST`, broadcast e heartbeat conferem |
| Credencial errada | `LOGIN_FAIL` e conexão encerrada pelo servidor |
| Comando antes do login | Recusado |
| Comando desconhecido | `ERRO Comando desconhecido`, conexão preservada |
| `ADD` malformado (sem id, id não numérico, sem nome) | Recusado com mensagem específica, conexão preservada |
| Número de sequência inválido, zero ou fora de ordem | Recusado |
| **Reenvio de `ADD` com o mesmo `seq`** | Servidor repete a resposta e **não** insere de novo |
| **Reenvio de `HEARTBEAT` com o mesmo `seq`** | Devolve exatamente o mesmo bloco de antes |
| Nome com espaços / nome acima do limite | Aceito / truncado de forma consistente em todo o sistema |
| Heartbeat sem novidade | `ALIVE` |
| Heartbeat com novidade | Só os usuários cadastrados por outros clientes |
| Queda abrupta de cliente (sem `SAIR`) | Detectada; sessão removida; servidor segue normal |
| **Retransmissão vista do lado do cliente** | 3 tentativas a cada 3 s, sempre com o mesmo `seq`; aviso ao operador; respostas atrasadas descartadas; comando seguinte funciona normalmente |
| **Carga de 100, 1000 e 10000 clientes** | 11100 conexões aceitas, **0 falhas, 0 avisos**; cada conexão numerada de `#1` a `#11100` |
| **Carga de 10000 clientes autenticados** | 10000 sessões simultâneas, 10000 logins registrados, 0 falhas; servidor continua operando normalmente depois |
| Persistência | Os quatro arquivos gerados e consistentes com as operações |

## 7. Pendências para as fases seguintes

- **Fase 5:** repetir os testes com servidor e clientes em **máquinas diferentes**;
  capturar as telas exigidas (itens 6 e 7); conferir o tráfego no **Wireshark** contra
  `docs/03_protocolo.md`; consolidar em `docs/05_plano_testes.md`.
- **Antes da apresentação:** trocar `SERVER_IP` em `comum.h` pelo IP real da máquina do
  servidor e recompilar (`make`).
- **Fase 6:** estudo aprofundado do código, conclusão sobre desenvolvimento e dificuldades,
  referências bibliográficas.
- **Entrega:** reunir num único diretório os fontes de `src/` + `readme.txt` +
  `documentacao.pdf`, sem executáveis nem arquivos objeto, e compactar em `.zip`.

---

*Fase 4 concluída em 27/07/2026. Versão 1 pronta para validação em rede real e para a Fase 5.*
