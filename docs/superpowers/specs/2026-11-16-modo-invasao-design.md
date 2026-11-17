# Modo Invasão no ELIRA: design da integração

Data: 2026-11-16 · Branch de destino: `developer` (nada vai para a `main`)

## 1. Intenção

O Victor quer que o ELIRA tenha um **segundo modo de jogo**, de ação, sem deixar de ser
um escape room. O protótipo aprovado (`prototipos/modo-invasao/invasao.c`, v2) hoje roda
como um programa separado. Com esta mudança, o jogador escolhe o modo no **menu inicial**:

- **INICIAR MISSÃO:** o escape room de sempre (modo padrão, sem nenhuma mudança).
- **MODO INVASÃO:** ação top-down em salas. São 4 estágios (1990, 2008, 2026, 2048), cada
  um com o chefe da sua era, e depois o Núcleo com A.R.1.3.L.

## 2. Objetivos, não objetivos e antiobjetivos

**Objetivos**
- Um botão novo "MODO INVASÃO" no menu inicial, que abre o modo dentro do mesmo `ELIRA.exe`.
- O modo se comporta como no protótipo v2: 5 corações, melhorias em chip, escolha de
  1 entre 2 após cada chefe e painéis de narrativa da Elira.
- Ao morrer, a Elira volta ao início do estágio atual (decisão do Victor, 2026-11-16).
- Pela pausa dá para voltar ao menu inicial, e de lá entrar em qualquer um dos modos.

**Não objetivos (agora)**
- Revisar as falas da Elira e o nome do artefato de 2048: o texto atual entra como está e
  será ajustado depois, com o Victor.
- Ranking, pontuação ou finais para o Modo Invasão.
- Áudio próprio do modo: a música ambiente do jogo continua tocando.

**Antiobjetivos (seria falha mesmo funcionando)**
- Qualquer mudança de comportamento no escape room (salas, puzzles, diário, ranking, finais).
- Alterar um asset existente em `assets/` (o PDF proíbe).
- Usar ou alterar `GetEndingCategory` e as regras de final (o PDF proíbe regra de final nova).
- Mexer na `main` ou dar push sem pedido explícito.

## 3. Restrições

- PDF, seção 2: preservar a modularização. O `main.c` recebe só pequenas alterações de
  roteamento, e `GameState` e `GameScreen` continuam declarados em `funcoes.h`.
- Abordagem B, escolhida pelo Victor: o modo vive num **módulo próprio**
  (`src/invasao.c` + `src/invasao.h`). Esse é o único desvio consciente da lista de
  arquivos do PDF. O README do repositório vai explicar o motivo.
- Resolução virtual de 1280x720, a mesma do jogo e do protótipo. O jogo já desenha num
  `RenderTexture` e escala para a janela.
- Compilação em C99 com `-Wall -Wextra`, sem avisos novos.
- Sem push automático.

## 4. Abordagens consideradas

| | Abordagem | Prós | Contras |
|---|---|---|---|
| A | Código em `funcoes.c` (bloco `Inv*`), estado numa struct `InvasionState` dentro do `GameState` | Segue a lista de arquivos do PDF à risca | Refatoração grande das ~60 variáveis globais; `funcoes.c` vai para ~16,5 mil linhas; mais conflito com os builds que mexem em `funcoes.c` |
| **B** | **Módulo `src/invasao.c` + `invasao.h`; `main.c` só roteia** | **Código isolado, sem conflito com as salas; o estado fica privado no módulo** | **Cria um 5º arquivo-fonte fora da lista do PDF** |
| C | Menu abre o `invasao.exe` separado | Quase nenhuma mudança | Dois programas para empacotar; a janela troca; descartada |

**Escolhida: B (Victor).**

## 5. Como fica

### 5.1 Arquivos

| Arquivo | Mudança |
|---|---|
| `src/invasao.c` (novo) | O código do protótipo v2 sem o próprio `main()` nem o próprio `RenderTexture`. O estado continua `static` (privado ao módulo). |
| `src/invasao.h` (novo) | A interface pública (5.2). Inclui `funcoes.h` só para conhecer `GameState`. |
| `src/funcoes.h` | `SCREEN_INVASION` acrescentado **no fim** do enum `GameScreen`. Nada mais. |
| `src/funcoes.c` | `DrawTitle()`: botão novo, botões reposicionados, textos removidos e linha de atalhos atualizada (5.3). |
| `src/main.c` | `#include "invasao.h"`; retângulos de clique do `SCREEN_TITLE` atualizados para as novas posições; clique/tecla I leva para o modo; `case SCREEN_INVASION` no update e no draw; `InvUnload()` antes de `CloseWindow()`; os novos sprites entram na lista de `--verify-assets`. |
| `Makefile`, `CMakeLists.txt` | `src/invasao.c` adicionado à lista de fontes; `invasao.h` aos cabeçalhos. |
| `assets/sprites/invasao/` (nova pasta) | `elira_sheet.png`, `elira_fx.png`, `elira_rostos.png` (arquivos novos, nenhum existente é alterado). |
| `prototipos/modo-invasao/` | Removida depois da integração, para ficar uma única fonte da verdade (o histórico do git guarda a versão standalone). |
| `README.md` | Seção curta: os dois modos, os controles do Modo Invasão e o motivo do módulo separado. |

### 5.2 Interface do módulo (`invasao.h`)

```c
void InvEnter(GameState *game);            /* carrega recursos (1ª vez) e começa do estágio 1990 */
void InvUpdate(GameState *game, float dt); /* lê entrada e avança; pode voltar game->screen para SCREEN_TITLE */
void InvDraw(void);                        /* desenha no render target que o main.c já abriu */
void InvUnload(void);                      /* libera texturas e fontes do modo (no fechamento do jogo) */
```

- `InvEnter` carrega as texturas e as fontes na primeira entrada e reaproveita nas
  seguintes. O modo sempre começa um jogo novo no estágio 1990.
- Os recursos vêm de `assets/...` (caminho relativo). O `main.c` já põe o diretório de
  trabalho na raiz que contém `assets/`.
- O sistema de fonte do protótipo (cache de `LoadFontEx` por tamanho, com o mesmo
  `GeistPixel-Square.ttf`) **fica dentro do módulo**, para não mudar o visual testado.

### 5.3 Menu inicial

```
  ARQUIVO DE MEMORIA // TRANSMISSAO 01
  ELIRA
  ───────────
  [ INICIAR MISSAO      ]   {86, 326, 350, 54}  ciano
  [ MODO INVASAO [I]    ]   {86, 392, 350, 54}  magenta (230, 90, 200)
  [ CONFIGURACOES [C]   ]   {86, 458, 350, 50}  dourado
  > ENTER: Iniciar  |  I: Invasao  |  C: Opcoes <   y=524
```

Prévia aprovada pelo Victor (2026-11-16), com os seguintes textos **removidos** do menu a pedido dele:
"ESCAPE RUN TEMPORAL", "ENTRE NAS MEMORIAS DA MAQUINA", "Descubra o que existia antes de
A.R.1.3.L. controlar o futuro.", o rodapé "DEMO TEMPORAL // 1990 - 2048" e "Point & click narrativo".
Os botões sobem para ocupar o espaço. O título da janela continua "ELIRA - Escape Run Temporal".

- Clique no botão ou tecla **I**: `InvEnter(&game)` e `game.screen = SCREEN_INVASION`. O modo
  não passa pela tela de nome nem pela cinemática.
- Antes de qualquer mudança no menu, sai uma **prévia em PNG** para o Victor aprovar (regra
  de trabalho dele).

### 5.4 Dentro do Modo Invasão

- Controles iguais aos do protótipo: WASD move, setas atiram.
- **ESC:** pausa / continua. A pausa mostra "**[Q] voltar ao menu**" (o modo é todo no teclado; o protótipo não usa mouse). No protótipo, Q
  fechava o programa; agora volta para `SCREEN_TITLE`.
- **Derrota:** R tenta de novo o estágio (volta ao início do estágio), e ESC / Q volta ao menu.
- **Vitória:** R começa um jogo novo, e ENTER / Q volta ao menu.
- **Teclas 1 a 5** (pular estágio): continuam como atalho de teste, controladas por uma
  constante `INV_DEBUG_SKIP` em `invasao.c`, ligada por enquanto. Desligar antes de uma entrega.
- Atalhos globais do `main.c` continuam valendo: F11 / Alt+Enter (tela cheia) e M (música).
  O M não colide com nenhuma tecla do modo.
- `UpdatePhaseControls` (menu de fase do escape room) já ignora telas fora de `IsPhaseScreen`,
  então não aparece nem captura entrada em `SCREEN_INVASION`. Não incluir a tela nova nessa lista.

## 6. Verificação (o que conta como "pronto")

1. `make` compila sem erro e sem aviso novo; o CMake também lista os arquivos novos.
2. `ELIRA.exe --verify-assets` passa com os sprites novos.
3. Prévias PNG (harness, com janela oculta): menu com os 3 botões; um quadro de combate de
   cada era; um chefe; a pausa com "voltar ao menu".
4. Fluxo jogado no `ELIRA.exe`: menu → Modo Invasão → pausa → menu → INICIAR MISSÃO → sala 1990
   funcionando → de volta ao menu → Modo Invasão de novo (recomeça em 1990, sem textura quebrada).
5. Escape room sem regressão: o harness de lógica existente (`golden_test.c`, 18 asserts,
   recriado se tiver sumido) continua passando.

## 7. Fluxo git

- O trabalho é feito numa worktree `wt-modo-invasao`, branch `feature/modo-invasao` criada a partir da `developer`.
- Ao final: merge `--no-ff` na `developer`. Nada na `main`.
- Commits sem linha de coautoria. Push só quando o Victor pedir.

## 8. Decisões tomadas por mim (Victor pode corrigir)

- Sem ranking/finais no modo (antiobjetivo do PDF).
- Botão no meio, Configurações desce; tecla **I**.
- O modo sempre recomeça em 1990 ao entrar pelo menu (não guarda progresso).
- Pasta `assets/sprites/invasao/` para os PNGs novos.
- Fontes do modo continuam com o sistema próprio do protótipo, dentro do módulo.
- Atalho 1 a 5 continua ligado por enquanto (`INV_DEBUG_SKIP`).
- `prototipos/modo-invasao/` removida depois da integração.

## 9. Fica para quem implementar

- Como desacoplar `Render()`/`Draw()` do protótipo do `RenderTexture` próprio (a ideia é
  `InvDraw` só desenhar; o `BeginTextureMode` já foi chamado pelo `main.c`).
- Como mapear `gs` (`GS_PLAY`, `GS_PAUSE`, `GS_OVER`, `GS_WIN`, `GS_STORY`…) para as saídas ao menu.
- A posição exata do botão e das linhas de atalho, conforme a prévia aprovada.
- Evitar colisão de nomes: tudo que for `static` em `invasao.c` fica privado; os nomes
  públicos começam com `Inv`.
