# Rastreabilidade: enunciado → código

*Auditoria de 28/07/2026. Percorre o enunciado (`trabalho_redes_2026.pdf`) do início ao fim e
aponta, para cada exigência, onde ela está atendida no código — ou por que ficou fora de
escopo. Serve para conferência antes da entrega, para a defesa oral e como base dos itens 2
e 3 da documentação final.*

---

## 1. Técnicas de múltiplos clientes

| Exigência do enunciado | Onde está | Situação |
|---|---|---|
| Suportar múltiplas conexões por multiplexação, `fork()` **ou** multithreads | `servidor.c`, `main` → `pthread_create` com `TAM_PILHA_THREAD` | ✅ threads |

### O "apenas" do servidor principal

O enunciado determina que o servidor principal fique responsável **apenas** por aceitar
conexões, distribuir eventos e monitorar sockets. Correspondência na nossa arquitetura:

| Responsabilidade | Onde |
|---|---|
| **Aceitar conexões** | Laço `for(;;)` do `main`, em `servidor.c`: só faz `accept()`, cria a thread e volta a aceitar. Não processa nenhum comando |
| **Monitorar sockets** | Cada thread de atendimento, bloqueada em `protocolo_le_linha` (que chama `recv`), detectando dados novos ou o fim da conexão |
| **Distribuir eventos** | `sessoes_broadcast`, em `sessoes.c`, disparado pela thread que processou o `ADD` |

O ponto a defender: a thread principal **nunca atende um cliente**. Ela recebe a conexão e
delega. Todo o trabalho de negócio acontece nas threads criadas por ela.

## 2. Arquitetura do sistema

O diagrama do enunciado (Cliente → Socket → Servidor → Gravação em arquivo/banco) está
implementado assim:

```
[cliente.c] --socket TCP 8080--> [servidor.c] --> [persistencia.c] --> dados/*.txt
```

### Responsabilidades dos clientes

| Exigência | Onde está | Situação |
|---|---|---|
| Enviar solicitações | `cliente.c`: `opcao_adicionar_usuario`, `opcao_ver_fila`, `opcao_heartbeat` → `envia_comando` | ✅ |
| Receber notificações | `cliente.c`: `thread_receptora` imprime o broadcast assim que chega | ✅ |
| Atualizar filas em tempo real | Implementado como **notificação**, não como réplica local — ver seção 6 | ✅ com ressalva documentada |
| Autenticar usuários | `cliente.c`: `autentica`, envia `LOGIN` com a credencial fixa | ✅ |

### Responsabilidades do servidor

| Exigência | Onde está | Situação |
|---|---|---|
| Autenticação | `servidor.c`: `autentica` | ✅ |
| Gerenciamento de filas | `fila.c`: `fila_adiciona`, `fila_tamanho`, `fila_copia_intervalo` | ✅ |
| Sincronizar os clientes | `sessoes.c`: `sessoes_broadcast` + `servidor.c`: `trata_heartbeat` | ✅ |
| Envio de notificações | `sessoes.c`: `sessoes_broadcast` | ✅ |

### Gravação em arquivo (escolhida no lugar do banco)

| Dado exigido | Arquivo | Função responsável |
|---|---|---|
| usuários | `dados/historico.txt` | `persistencia_historico_add` |
| filas | `dados/fila.txt` | `persistencia_salva_fila` |
| histórico | `dados/historico.txt` | `persistencia_historico_add` |
| logs | `dados/servidor.log` | `persistencia_log_servidor` |
| sessões (dados do cliente que logou) | `dados/sessoes.log` | `persistencia_log_sessao` |

"Usuários" e "histórico" compartilham o mesmo arquivo: cada linha de `historico.txt` registra
um usuário cadastrado com data e hora, atendendo aos dois itens de uma vez.

## 3. Sistemas

| Exigência | Situação |
|---|---|
| Inicialização como `./cliente` e `./servidor` | ✅ nenhum dos dois lê argumentos |
| Cliente conecta por IP e porta **automaticamente**, sem parâmetros | ✅ constantes `SERVER_IP` e `SERVER_PORTA` em `comum.h` |

## 4. Telas de funcionamento e a Observação

| Elemento do print | Onde é gerado |
|---|---|
| `Servidor iniciado na porta 8080` | `servidor.c`, `main` |
| `Novo cliente conectado.` | `servidor.c`, `atende_cliente` (com número da conexão e origem no fim da linha) |
| `Cliente desconectado.` | `servidor.c`, `atende_cliente` |
| `Conectado ao servidor.` / `Servidor: LOGIN_OK` | `cliente.c`, `main` e `autentica` |
| Menu `1`/`2`/`3`/`0` | `cliente.c`, `mostra_menu` |
| `Usuário <nome> adicionado.` | `cliente.c`, `imprime_resposta` |
| `===== FILA =====` … `================` | `servidor.c`, `envia_bloco_fila` |
| `[Broadcast] Novo usuário: <nome>` | `sessoes.c` (envio) + `cliente.c` (exibição acentuada) |
| `Heartbeat: ALIVE` | `cliente.c`, `imprime_resposta` |

**Observação do enunciado** (heartbeat devolve os usuários cadastrados por outros clientes, ou
`ALIVE`): implementada em `servidor.c`, `trata_heartbeat`. O print do enunciado mostra a fila
inteira, o que diverge do texto da Observação; seguimos o texto — decisão registrada na
seção 6.

## 5. Entrega e avaliação

| Exigência | Situação |
|---|---|
| `make` sem parâmetros gera **exatamente** `cliente` e `servidor` | ✅ verificado |
| Todos os fontes (`.c`, `.h`, `Makefile`) | ✅ 9 arquivos |
| **Sem** executáveis e arquivos objeto no zip | ✅ `.gitignore` + `make clean` antes de compactar |
| `readme.txt` com nome do aluno e comando de execução | ✅ |
| Tudo em um único diretório | ⬜ montar na hora da entrega |
| PDF da documentação com os 8 itens | ⬜ **pendente** |
| Qualidade do código (organização, comentários, nomes, modularidade) | ✅ 6 módulos, sem código morto |
| Aderência ao protocolo (verificada por captura de tráfego) | ✅ texto ASCII, uma mensagem por linha, conforme `03_protocolo.md` |
| Elementos não especificados descritos explicitamente | ✅ `04_implementacao.md`, seção 3 |

## 6. Escopo declarado: o que NÃO foi implementado, e por quê

O enunciado exige que qualquer elemento omitido ou acrescentado seja declarado. Esta seção é
essa declaração, e deve ser reproduzida no item 3 da documentação final.

### Itens citados apenas na introdução, fora de escopo

| Item | Por que ficou fora |
|---|---|
| **Painéis administrativos** | Citado apenas na introdução; não aparece nos prints, na Observação nem nos 8 itens da documentação. Exigiria um tipo de cliente com interface própria |
| **Balanceamento de carga** | Pressupõe mais de um servidor. A arquitetura do enunciado mostra um servidor central único |
| **Administradores monitoram métricas em tempo real** | Depende do painel administrativo |
| **Papéis distintos** (recepcionista, coordenador, administrador) | O sistema tem um único tipo de operador, coerente com a credencial única prevista para o login |
| **Posição do usuário na fila** | A resposta de `LIST` segue o formato exato do print (`id - nome`), sem número de posição. A ordem das linhas já é a ordem de atendimento |
| **Acompanhamento via aplicativo/web** | O enunciado determina cliente e servidor em C, iniciados por linha de comando |

### Decisões de implementação sobre pontos ambíguos

| Ponto | Decisão |
|---|---|
| "Atualizar filas em tempo real" (cliente) | Implementado como notificação assíncrona do novo usuário, não como réplica local da fila — é o que o print mostra. O cliente pede a fila completa com a opção `2` quando quiser |
| Heartbeat: texto da Observação × print | Seguimos o texto: apenas os usuários cadastrados por **outros** clientes desde a última checagem |
| Terminologia "paciente" × "usuário" | Adotado "usuário", termo usado no texto do enunciado |
| Persistência: banco × arquivo | Arquivo texto, opção permitida pelo enunciado |
| Endereço do servidor | Constante `SERVER_IP` no código, alterada e recompilada ao mudar de máquina |
| Unicidade de identificador | Não há verificação: dois usuários podem ter o mesmo `id` |
| Capacidade da fila | Limite fixo de 20000 registros |
| Tamanho do nome | Até 49 caracteres; acima disso o cadastro é recusado, não truncado |
| Identificador inválido | Recusado se for menor ou igual a zero. **Limitação conhecida:** um número acima do limite do tipo `int` sofre estouro silencioso no `sscanf` e é gravado com outro valor |
| Recuperação da fila | O servidor sempre inicia com a fila vazia; não recarrega `fila.txt` |

### Elementos acrescentados por necessidade do protocolo

Detalhados em `04_implementacao.md`, seção 3: tabela de sessões, serialização de escrita por
socket, `SIGPIPE` ignorado, pilha reduzida das threads e numeração das conexões no log.

---

*Auditoria concluída em 28/07/2026. Nenhuma exigência do enunciado ficou sem correspondência
no código, e todas as omissões estão declaradas acima.*
