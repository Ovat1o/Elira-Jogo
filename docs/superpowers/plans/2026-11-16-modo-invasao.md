# Modo Invasão no menu do ELIRA: plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O Modo Invasão (protótipo v2) passa a ser jogável dentro do `ELIRA.exe`, a partir de um terceiro botão no menu inicial, sem mudar o escape room.

**Architecture:** O protótipo vira o módulo `src/invasao.c` + `src/invasao.h`, com o estado `static` privado e uma interface pública `Inv*`. O `main.c` só roteia a tela nova `SCREEN_INVASION`, que é o último valor do enum `GameScreen`. O `DrawTitle()` em `funcoes.c` ganha o botão novo e perde os textos que o Victor mandou remover.

**Tech Stack:** C99, raylib 6.0 (`src/raylib/libraylib.a`), w64devkit 2.10.0 (`.deps/`), Make e CMake.

**Spec:** `docs/superpowers/specs/2026-11-16-modo-invasao-design.md`

## Global Constraints

- Tudo na branch `feature/modo-invasao` (worktree `C:\Users\Victor\Documents\Elira\wt-modo-invasao`), criada a partir da `developer`; ao final, merge `--no-ff` na `developer`. **Nada na `main`, nenhum push.**
- Mensagens de commit **sem** linha `Co-Authored-By`.
- `GameState` e `GameScreen` continuam só em `funcoes.h`. O `main.c` recebe só roteamento.
- Nenhum arquivo existente em `assets/` é alterado. Os PNGs novos vão para `assets/sprites/invasao/`.
- Compilação: `.deps\w64devkit\bin\make.exe` (C99, `-Wall -Wextra`), **sem aviso novo**.
- O Modo Invasão não toca em ranking, `GetEndingCategory` nem finais.
- Os textos de tela continuam sem acento no menu (a fonte já é usada assim no `DrawTitle`). Dentro do modo, os textos do protótipo ficam como estão.
- Build: o w64devkit de `C:\raylib` (GCC 13.2) **não** serve (`undefined reference to stat64i32`); use só o `.deps`. Na worktree nova, `.deps` não existe: chame o make e o gcc pelo caminho absoluto do clone principal `C:\Users\Victor\Documents\Elira\Elira-Jogo\.deps\w64devkit\bin\`.

## Review Focus

1. **Entrar no modo duas vezes** (menu → invasão → menu → invasão): não pode recarregar texturas nem fontes (vazamento) e precisa recomeçar em 1990 com 5 corações. Teste `inv_reentrada` na Task 1.
2. **Voltar ao menu no meio de uma partida e jogar o escape room:** o `GameState` do escape room fica intocado, porque o modo só escreve `game->screen`. Teste `inv_nao_toca_gamestate` na Task 1.
3. **A tecla que volta ao menu não pode disparar nada no menu no mesmo quadro** (ENTER na vitória x ENTER = "Iniciar missão"). O `main.c` processa uma tela por quadro e `IsKeyPressed` só vale um quadro. Conferir no roteiro manual da Task 4, passo "vitória → ENTER".
4. **Fechar a janela com o modo aberto:** `InvUnload()` antes de `CloseWindow()`, sem crash. Conferir na Task 2.
5. **Tela cheia (F11) dentro do modo:** o modo desenha no render target do `main.c`, então a escala vem de graça. A transparência dos sprites depende do blend separado do protótipo (`BLEND_CUSTOM_SEPARATE`), que o `InvDraw` mantém. Conferir na prévia da Task 1 (Elira sem borda preta).

---

### Task 1: Módulo `src/invasao.c` + `invasao.h` compilando no jogo

**Files:**
- Create: `src/invasao.h`, `src/invasao.c` (a partir de `prototipos/modo-invasao/invasao.c`)
- Create: `assets/sprites/invasao/elira_sheet.png`, `elira_fx.png`, `elira_rostos.png` (`git mv` de `prototipos/modo-invasao/`)
- Modify: `Makefile` (`SRC` e `PROJECT_HEADERS`), `CMakeLists.txt` (as duas listas `add_executable`)
- Test: `C:\Users\Victor\AppData\Local\Temp\claude\99cc82ecc4957832\ad609892-488a-42e9-9242-18b30069fb20\scratchpad\harness\inv_test.c` (fora do repo; faz `#include` de `src/invasao.c` e `src/funcoes.c`)

**Interfaces:**
- Produces (`invasao.h`, que inclui `funcoes.h`):
  ```c
  void InvEnter(GameState *game);
  void InvUpdate(GameState *game, float dt);
  void InvDraw(void);
  void InvUnload(void);
  ```
- Produces (privado, usado pelo teste via `#include`):
  ```c
  typedef struct InvKeys { bool esc, q, r, enter, space; int skipStage; } InvKeys; /* skipStage -1 = nenhum */
  static bool InvApplyKeys(InvKeys k); /* true = voltar ao menu */
  static void InvLeave(GameState *game); /* game->screen = SCREEN_TITLE; nada mais */
  ```
- Consumes: `GameState`, `SCREEN_TITLE` e `SCREEN_INVASION` (de `funcoes.h`). **O `SCREEN_INVASION` é criado nesta task** (uma linha no fim do enum em `funcoes.h`), porque o módulo precisa dele para compilar; o roteamento vem na Task 2.

- [ ] **Step 1: Worktree**
  `git worktree add ..\wt-modo-invasao -b feature/modo-invasao developer`. Trabalhar só nela daqui em diante.

- [ ] **Step 2: Escrever o teste que falha** (`inv_test.c`, assert simples com contador e `return falhas != 0`)
  - `inv_pausa`: `gs = GS_PLAY`; `InvApplyKeys({.esc=1,.skipStage=-1})` deixa `gs == GS_PAUSE` e retorna `false`; de novo com `esc` deixa `GS_PLAY`.
  - `inv_pausa_menu`: `gs = GS_PAUSE`; `InvApplyKeys({.q=1,...})` retorna `true`.
  - `inv_derrota`: `NewGame(2)`; `gs = GS_OVER`; `{.r=1}` → `gs == GS_PLAY`, `stage == 2`, `pl.hp == pl.maxhp`. Com `gs = GS_OVER`, `{.esc=1}` retorna `true` e `{.q=1}` também.
  - `inv_vitoria`: `gs = GS_WIN`; `{.r=1}` → `stage == 0`; com `gs = GS_WIN`, `{.enter=1}` retorna `true` e `{.q=1}` também.
  - `inv_reentrada`: `GameState g = {0}`; `InvEnter(&g)` duas vezes; `texElira.id` igual nas duas; `stage == 0`; `pl.hp == 10`; `g.screen == SCREEN_INVASION`.
  - `inv_nao_toca_gamestate`: preenche um `GameState` com `ResetDemo`, copia com `memcpy`, chama `InvEnter` e depois `InvLeave`; compara a cópia com `memcmp`, ignorando só o campo `screen` (zerar `screen` nas duas antes de comparar), e confere `g.screen == SCREEN_TITLE`.

  Compilar como o `menuprev.c` (mesma linha de gcc, `-w`, janela `FLAG_WINDOW_HIDDEN`, `ChangeDirectory` para a raiz da worktree).

- [ ] **Step 3: Rodar e ver falhar.** Esperado: erro de compilação (`invasao.c` / `InvApplyKeys` não existem).

- [ ] **Step 4: Montar o módulo** em `src/invasao.c`, partindo do protótipo:
  - Remover `main()`, `Shots()`/`Save()` e o `RenderTexture2D target` próprio.
  - `#define ASSETS "assets/"`; sprites por `ASSETS "sprites/invasao/elira_sheet.png"` (idem `elira_fx.png`, `elira_rostos.png`), e não mais por `GetApplicationDirectory()`.
  - Renomear qualquer símbolo de arquivo que colida com `funcoes.h`/`constantes.h` (ex.: `C_*`, `VW`/`VH`, `Clampf`) acrescentando o prefixo `Inv`. O compilador aponta quais colidem.
  - `InvApplyKeys`: as regras do loop do protótipo (linhas 2231-2237 originais) mais as saídas ao menu: `GS_PAUSE` + Q; `GS_OVER` + ESC ou Q; `GS_WIN` + ENTER ou Q. O `skipStage` (1 a 5 → `NewGame(k)`) só funciona com `#define INV_DEBUG_SKIP 1` no topo do arquivo.
  - `InvEnter`: carrega recursos só na primeira chamada (flag `static bool invLoaded`), chama `NewGame(0)` e define `game->screen = SCREEN_INVASION`.
  - `InvUpdate`: monta o `InvKeys` com `IsKeyPressed`; se `InvApplyKeys` retornar `true`, chama `InvLeave(game)`; senão faz `Update(fminf(dt, 1.0f/30), gs == GS_PLAY ? ReadInput() : (Input){0})`.
  - `InvDraw`: o miolo do antigo `Render()` sem `Begin/EndTextureMode` (mantém o `rlSetBlendFactorsSeparate` + `BLEND_CUSTOM_SEPARATE`).
  - `InvUnload`: descarrega texturas e as fontes do cache `g_font` que foram carregadas, e zera `invLoaded`.
  - Textos dos overlays: pausa `"[ESC] continuar    [Q] voltar ao menu"`; derrota `"[R] recomeçar este estágio    [Q] menu"`; vitória `"Os quatro protocolos caíram.   [R] jogar de novo   [ENTER] menu"`.
  - Acrescentar `SCREEN_INVASION` no fim do enum `GameScreen` em `funcoes.h`.
  - `git mv` dos 3 PNGs para `assets/sprites/invasao/`; `src/invasao.c` em `SRC` e `src/invasao.h` em `PROJECT_HEADERS` (Makefile); `src/invasao.c` nas duas listas do `CMakeLists.txt`.

- [ ] **Step 5: Rodar o teste e ver passar.** Esperado: `inv_test: 6/6 ok`, exit 0.

- [ ] **Step 6: Build do jogo.** O `make` na worktree gera o `ELIRA.exe` sem nenhuma linha `warning:` vinda de `src/invasao.c`, `src/funcoes.c` ou `src/main.c`.

- [ ] **Step 7: Prévia.** Harness `invprev.c` (`#include` de `invasao.c`) chama `InvEnter`, depois `NewGame(k)` para k = 0..4, e grava `InvDraw()` num `RenderTexture` em `shots/inv_<k>.png`, além de `gs = GS_PAUSE` → `shots/inv_pausa.png`. Comparar a 1990 com o `v2_10_combate_1990.png` do protótipo: mesma Elira, sem borda preta.

- [ ] **Step 8: Commit**
  ```
  git add src/invasao.c src/invasao.h src/funcoes.h Makefile CMakeLists.txt assets/sprites/invasao prototipos/modo-invasao
  git commit -m "feat: modulo invasao.c com o Modo Invasao dentro do jogo"
  ```

### Task 2: Roteamento no `main.c`

**Files:**
- Modify: `src/main.c` (includes; `ValidateRequiredAssets`; os dois `switch (game.screen)`; antes de `CloseWindow()`)

**Interfaces:**
- Consumes: `InvEnter`, `InvUpdate`, `InvDraw` e `InvUnload` (Task 1), além de `SCREEN_INVASION`.
- Produces: `case SCREEN_INVASION` no update (`InvUpdate(&game, delta)`) e no draw (`InvDraw()`). A entrada pelo menu fica para a Task 3.

- [ ] **Step 1: Teste que falha:** `ELIRA.exe --verify-assets` deve exigir `assets/sprites/invasao/elira_sheet.png`, `elira_fx.png` e `elira_rostos.png`. Renomear temporariamente `elira_fx.png` → rodar → esperado exit 1 com `[ASSETS] Recurso obrigatorio ausente: assets/sprites/invasao/elira_fx.png`. Hoje sai 0, então falha.
- [ ] **Step 2: Implementar:** `#include "invasao.h"`; os 3 caminhos na lista `required[]`; os dois `case SCREEN_INVASION`; `InvUnload();` logo antes de `CloseWindow()`. O `SCREEN_INVASION` **não** entra em `IsPhaseScreen` nem na lista de telas do clique de áudio.
- [ ] **Step 3: Verificar:** renomeado → exit 1 com a mensagem acima; restaurado → exit 0 e `[ASSETS] Todos os recursos obrigatorios foram encontrados.` O build não gera aviso novo.
- [ ] **Step 4: Commit:** `git commit -am "feat: main.c roteia SCREEN_INVASION"`

### Task 3: Menu inicial com três botões

**Files:**
- Modify: `src/funcoes.c` (`DrawTitle`, ~linha 1673), `src/main.c` (`case SCREEN_TITLE`, ~linha 154)

**Interfaces:**
- Consumes: `InvEnter(&game)` (Task 1).

- [ ] **Step 1: Teste visual que falha:** rodar o `menuprev.exe` atual contra o `DrawTitle()` real da worktree (trocar o include para a worktree) → `shots/menu_real.png`. Esperado: diferente de `shots/menu_depois_v3.png` (ainda com dois botões).
- [ ] **Step 2: Implementar em `DrawTitle`** exatamente o layout da spec 5.3:
  - `startButton {86, 326, 350, 54}`, `invasionButton {86, 392, 350, 54}`, `settingsButton {86, 458, 350, 50}`.
  - Rótulos `"INICIAR MISSAO"` (ciano), `"MODO INVASAO [I]"` (`(Color){230, 90, 200, 255}`) e `"CONFIGURACOES [C]"` (dourado).
  - Atalhos `"> ENTER: Iniciar  |  I: Invasao  |  C: Opcoes <"` em y = 524.
  - Remover os blocos "4. Subtitulos" e "7. Rodape" inteiros.
- [ ] **Step 3: Implementar no `main.c`** (`case SCREEN_TITLE`): os retângulos de clique passam para as mesmas três posições. Clique em `{86, 392, 350, 54}` ou `IsKeyPressed(KEY_I)` → `InvEnter(&game)`. ENTER/ESPAÇO continuam iniciando a missão e C continua abrindo as configurações.
- [ ] **Step 4: Verificar:** gerar `menu_real.png` de novo; visualmente igual ao `menu_depois_v3.png`, que é o aprovado.
- [ ] **Step 5: Commit:** `git commit -am "feat: menu inicial com botao Modo Invasao e textos removidos"`

### Task 4: Limpeza, documentação, regressão e merge

**Files:**
- Delete: o que restar de `prototipos/modo-invasao/` (`git rm -r`)
- Modify: `README.md` (seção "Modos de jogo")

- [ ] **Step 1: README:** seção curta "Modos de jogo" com os dois modos, os controles do Modo Invasão (WASD, setas, ESC, Q, R, ENTER) e uma linha explicando o módulo separado `src/invasao.c` (decisão do Victor, abordagem B da spec).
- [ ] **Step 2: Regressão do escape room:** recriar o `golden_test.c` (o antigo está em `...\17891eda-...\scratchpad\harness\golden_test.c`) apontando para a worktree e rodar. Esperado: 18 asserts ok.
- [ ] **Step 3: Roteiro manual no `ELIRA.exe`, com o Victor jogando:**
  - menu → I → combate em 1990 → ESC → Q → menu;
  - INICIAR MISSÃO → sala 1990 normal → menu de fase → voltar ao título;
  - MODO INVASÃO de novo → recomeça em 1990 com 5 corações;
  - tecla 5 → Núcleo → vencer (ou simular) → ENTER volta ao menu **sem** iniciar a missão;
  - F11 dentro do modo;
  - fechar a janela no meio do modo.
- [ ] **Step 4: Commit:** `git rm -r prototipos/modo-invasao` + README → `git commit -m "docs: modos de jogo no README e remove prototipo standalone"`
- [ ] **Step 5: Merge:** na `developer` do clone principal, `git merge --no-ff feature/modo-invasao -m "merge: integra feature/modo-invasao na developer"`; rebuild; `ELIRA.exe --verify-assets` com exit 0. **Sem push.**
