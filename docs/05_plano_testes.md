# Fase 5 — Testes executados e resultados

*Registro dos testes rodados em ambiente Linux, com servidor e clientes na mesma máquina
(`SERVER_IP = 127.0.0.1`). Base direta dos itens 5, 6 e 7 da documentação final. Última
execução: 28/07/2026, a partir da pasta `ClienteServidor_Victor/`.*

---

## 1. Como os testes foram feitos

Três frentes, todas repetidas a cada alteração do código:

1. **Teste funcional** — dois clientes reproduzindo a sequência das telas do enunciado.
2. **Casos especiais** — um programa em Python conversando direto com o servidor pelo
   socket, mandando comandos válidos e inválidos e conferindo cada resposta. Falar o
   protocolo em texto permite testar situações que o `./cliente` não deixaria acontecer
   (comando malformado, sequência fora de ordem, comando antes do login).
3. **Carga** — o programa `carga.c`, com 100, 1000 e 10000 clientes.

## 2. Teste funcional (item 7 da documentação)

Dois clientes conectados ao mesmo servidor:

| Passo | Ação | Resultado esperado | Obtido |
|---|---|---|---|
| 1 | Cliente A e B conectam | `Conectado ao servidor.` / `Servidor: LOGIN_OK` | ✅ |
| 2 | A cadastra `10 / Abel` e `30 / Breno` | `Usuário Abel adicionado.` | ✅ |
| 3 | B recebe as notificações | `[Broadcast] Novo usuário: Abel` e `Breno` | ✅ |
| 4 | B cadastra `20 / Carlos` e `40 / Dario` | confirmação em B, broadcast em A | ✅ |
| 5 | A escolhe `2` (ver fila) | os quatro usuários, entre `===== FILA =====` e `================` | ✅ |
| 6 | A escolhe `3` (heartbeat) | só `20 - Carlos` e `40 - Dario`, cadastrados por B | ✅ |
| 7 | B escolhe `3` (heartbeat) | `Heartbeat: ALIVE` — B só cadastrou os próprios | ✅ |
| 8 | Ambos escolhem `0` | `Cliente desconectado.` na tela do servidor | ✅ |

O passo 7 confere com o print do enunciado, que também mostra `ALIVE` para o cliente que
acabou de cadastrar. O passo 6 é onde seguimos o texto da Observação em vez do print: o
print mostra a fila inteira, e nós devolvemos apenas os usuários de outros clientes.

## 3. Casos especiais — 30 de 30 aprovados (item 5)

### Autenticação

| # | Situação testada | Resposta obtida |
|---|---|---|
| 1 | Senha errada (`LOGIN admin errada`) | `LOGIN_FAIL` |
| 2 | Conexão após falha de login | encerrada pelo servidor |
| 3 | `LOGIN` incompleto (sem senha) | `LOGIN_FAIL` |
| 4 | Comando antes do login (`LIST 1`) | `LOGIN_FAIL` |
| 5 | Credencial correta | `LOGIN_OK` |

### Comandos malformados

| # | Situação testada | Resposta obtida |
|---|---|---|
| 6 | Comando inexistente (`FAZALGO 1`) | `ERRO Comando desconhecido` |
| 7 | `ADD` sem nenhum argumento | `ERRO Formato esperado...` |
| 8 | `ADD 1` (só a sequência) | `ERRO Formato esperado...` |
| 9 | `ADD 1 abc Ana` (id não numérico) | `ERRO Formato esperado...` |
| 10 | `ADD 1 5` (sem nome) | `ERRO Nome invalido...` |
| 11 | `LIST` sem número de sequência | `ERRO Formato esperado...` |
| 12 | `HEARTBEAT` sem número de sequência | `ERRO Formato esperado...` |
| 13 | Linha em branco | ignorada, conexão segue |

Em todos, **a conexão permanece aberta** — um comando errado não derruba o cliente.

### Validação de dados

| # | Situação testada | Resposta obtida |
|---|---|---|
| 14 | Identificador negativo (`-7`) | `ERRO O identificador deve ser um numero positivo` |
| 15 | Identificador zero | `ERRO ...numero positivo` |
| 16 | Nome com 60 caracteres (limite é 49) | `ERRO Nome invalido, vazio ou longo demais` |
| 17 | Nome com espaços (`Maria Clara`) | aceito |
| 18 | Nome com acentos (`José da Silva`) | aceito |

O nome longo é **recusado**, não cortado. Isso evita dois problemas: o cliente receber a
confirmação de um nome diferente do que enviou, e o corte cair no meio de um caractere
acentuado, que ocupa dois bytes.

### Retransmissão — lado do servidor (item 4)

| # | Situação testada | Resposta obtida |
|---|---|---|
| 19 | `ADD` repetido com o mesmo número de sequência | mesma resposta `ADD_OK 10 Abel` |
| 20 | Fila após o `ADD` repetido | **um único** registro — não duplicou |
| 21 | Sequência menor que a última processada | `ERRO Numero de sequencia invalido` |
| 22 | Sequência zero | `ERRO Numero de sequencia invalido` |

### Heartbeat — os seis cenários

| # | Situação testada | Resposta obtida |
|---|---|---|
| 23 | Broadcast chega ao outro cliente | `[Broadcast] Novo usuario: Breno` |
| 24 | Heartbeat com novidade | só `30 - Breno`, cadastrado por outro |
| 25 | O mesmo heartbeat reenviado | bloco **idêntico** ao anterior |
| 26 | Heartbeat seguinte, sem novidade | `ALIVE` |
| 27 | Esse heartbeat reenviado | `ALIVE` de novo |
| 28 | Novo cadastro e novo heartbeat | só `40 - Dario`, o registro novo |
| 29 | Esse heartbeat reenviado | mesmo bloco |

Os casos 25, 27 e 29 comprovam a idempotência: reenviar um comando devolve exatamente a
mesma resposta, sem avançar o marcador do que já foi visto.

### Falha de conexão

| # | Situação testada | Resposta obtida |
|---|---|---|
| 30 | Cliente cai sem enviar `SAIR` | servidor detecta, remove a sessão e continua atendendo os demais |

## 4. Retransmissão — lado do cliente (item 4)

Testado com um servidor de mentira que aceita o login e depois **não responde a nada**,
para forçar o tempo limite:

```
t = 0,0s   servidor recebeu: ADD 1 10 Abel
t = 3,0s   servidor recebeu: ADD 1 10 Abel
t = 6,0s   servidor recebeu: ADD 1 10 Abel
```

Resultados:

- exatamente **3 envios**, um a cada **3 segundos**, como configurado;
- os três com o **mesmo número de sequência (1)** — é isso que permite ao servidor
  reconhecer o reenvio;
- o cliente avisou o operador: `Servidor não respondeu, tente novamente.`;
- em seguida o servidor de mentira mandou **3 cópias atrasadas** da resposta: o cliente
  **ignorou todas**, porque já não havia comando esperando por elas;
- o comando seguinte (`HEARTBEAT 2`) funcionou normalmente, sem confusão.

## 5. Teste de carga — 100, 1000 e 10000 clientes (item 6)

Gerados automaticamente por `./carga N`. **O servidor foi reiniciado antes de cada teste**,
para a numeração das conexões começar em `[#1]` e deixar claro quantas conexões aquele
teste abriu. Antes do teste de 10000: `ulimit -n 20000`.

| Clientes | Conexões abertas | Falhas | Numeração no servidor | Sessões autenticadas |
|---|---|---|---|---|
| 100 | 100 | 0 | `[#1]` a `[#100]` | 100 |
| 1000 | 1000 | 0 | `[#1]` a `[#1000]` | 1000 |
| 10000 | 10000 | 0 | `[#1]` a `[#10000]` | 10000 |

Cada cliente gerado **conecta e se autentica**, ocupando uma sessão no servidor igual a um
`./cliente`. As conexões ficam todas abertas ao mesmo tempo até o operador apertar ENTER —
sem isso o programa estaria abrindo e fechando conexões em sequência, e o servidor nunca
precisaria sustentar mais de uma por vez.

Depois de cada teste de carga, um cliente comum foi executado e operou normalmente,
mostrando que o servidor não ficou em estado ruim.

### Observação sobre a ordem das linhas no log

As linhas não saem em ordem numérica perfeita — no teste de 100, a `[#99]` apareceu depois
da `[#100]`. Isso não é defeito: cada conexão é atendida por uma thread diferente, e a
ordem em que elas escrevem no terminal depende de qual thread o sistema escalona primeiro.
O número é atribuído no momento da aceitação, então a contagem está correta. É, na
prática, uma evidência visual de que o servidor é concorrente.

### Por que 10000 threads couberam

Cada thread é criada com pilha de 256 KB, definida por `pthread_attr_setstacksize`. Com o
padrão do sistema (8 MB), 10000 threads reservariam 80 GB de espaço de endereçamento e o
teste seria impossível.

## 6. Compilação

| Verificação | Resultado |
|---|---|
| `make` sem parâmetros | gera exatamente `cliente` e `servidor` |
| Avisos com `-Wall -Wextra` | nenhum |
| Avisos com `-Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wnull-dereference` | nenhum |
| Compilação com `-std=c99`, `-std=c11`, `-std=c17`, com e sem `-O2` | sem erro e sem aviso nas oito combinações |

## 7. Testes que ainda faltam

- [ ] Repetir o teste funcional com **servidor e clientes em máquinas diferentes**, após
      trocar `SERVER_IP` em `comum.h` e recompilar.
- [ ] Capturar as telas do funcionamento (item 7) e da carga (item 6).
- [ ] Conferir o tráfego no **Wireshark** contra a tabela de mensagens de
      `docs/03_protocolo.md`.

---

*Testes registrados em 28/07/2026, executados a partir da pasta `ClienteServidor_Victor/`.*
