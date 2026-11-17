# Diário como guia: plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reescrever o diário com 6 abas (4 salas, RECOMPENSAS, OCULTOS): cada aba de sala mostra as pistas encontradas, o próximo passo e o passo a passo com as respostas.

**Architecture:** O diário passa a ser dirigido por dados em `funcoes.c`: tabelas de pistas e passos por sala, com funções-condição sobre os flags existentes do `GameState`. Um único `UpdateJournal` e um único `DrawJournal` atendem a todas as salas.

**Tech Stack:** C99, raylib 6.0, w64devkit (`.deps`).

**Spec:** `docs/superpowers/specs/2026-11-17-diario-guia-design.md`

## Global Constraints

- O trabalho fica na worktree `C:\Users\Victor\Documents\Elira\wt-diario`, branch `feature/diario-guia`, e o merge vai só na `developer`. Commits sem linha de coautoria, e nada de push sem pedido.
- O diário fica em `funcoes.c`/`funcoes.h` e as constantes em `constantes.h`. O `main.c` recebe só roteamento.
- A compilação usa `.deps/w64devkit/bin/make.exe` e não pode gerar nenhum `warning:`.
- Nenhum asset existente é alterado e nenhum campo novo entra no `GameState`.
- Os testes ficam em `C:\Users\Victor\AppData\Local\Temp\claude\99cc82ecc4957832\ad609892-488a-42e9-9242-18b30069fb20\scratchpad\harness\` (harness com `#include` de `funcoes.c`, estilo `textinput_test.c`).

## Review Focus

1. **ESC com o diário aberto em cada sala:** o diário fecha e o menu da fase não abre. Coberto pelo teste `journal_esc_fecha` (Task 3).
2. **Tecla D no terminal de 2048 enquanto o jogador digita "EU DECIDO":** o diário não abre e o D vira texto. Coberto pelo teste `journal_d_bloqueado_digitando` (Task 3).
3. **Abrir o diário numa sala sem pistas ainda:** nenhuma resposta aparece, e o próximo passo é o primeiro. Coberto por `journal_inicio_sem_respostas` (Task 1).
4. **Pegar o item oculto com o diário aberto ou fechado:** a aba OCULTOS continua consistente. Verificar nas prévias da Task 2.
5. **Admin pulando a fase:** os passos aparecem como feitos se os flags forem marcados, ou como pendentes se não forem. O diário não pode travar nem mostrar lixo. Verificar no roteiro manual da Task 4.

---

### Task 1: Dados e lógica do diário (sem desenho)

**Files:** `src/constantes.h`, `src/funcoes.c` (bloco novo "DIARIO GUIA"), test `journal_test.c`

**Interfaces (produz):**
```c
/* constantes.h */
#define JOURNAL_TAB_1990 0 /* ... */ #define JOURNAL_TAB_2048 3
#define JOURNAL_TAB_REWARDS 4
#define JOURNAL_TAB_HIDDEN 5
#define JOURNAL_TAB_COUNT 6
/* funcoes.c (static) */
typedef struct { const char *title, *l1, *l2; bool (*found)(const GameState *); } JournalClue;
typedef struct { const char *text, *ans1, *ans2; bool (*done)(const GameState *); bool (*answerVisible)(const GameState *); } JournalStep;
static int JournalCurrentStep(const GameState *g, int room);   /* 1o passo nao feito; -1 = sala concluida */
static bool JournalTabUnlocked(const GameState *g, int tab);   /* salas: ano liberado; RECOMPENSAS/OCULTOS: sempre */
static int JournalTabForScreen(GameScreen s);                  /* aba da sala atual (1990 por padrao) */
static bool JournalRewardObtained(const GameState *g, int r);  /* 0 disquete, 1 CD, 2 SSD, 3 protocolo */
```

- [ ] **Step 1: escrever `journal_test.c` com os testes abaixo e rodar → RED** (as funções não existem).
  - `journal_inicio_sem_respostas`: com o `GameState` zerado, o passo atual das 4 salas é 0 e nenhum `answerVisible` dos passos ≥ 1 está ligado.
  - `journal_1990_passos`: com `memory1990Completed`, o passo atual é 1; com `puzzleFilesCompleted`, é 2; com `year2008Unlocked`, é -1.
  - `journal_2026_senha`: o `answerVisible` do passo 2 de 2026 é false antes de `dataset2026Seen` e true depois.
  - `journal_abas`: `JOURNAL_TAB_HIDDEN == 5`; a aba 2048 fica travada até `year2048Unlocked`; RECOMPENSAS e OCULTOS ficam sempre destravadas.
  - `journal_recompensas`: o disquete aparece com `year2008Unlocked`; o SSD só com `room2026Completed`.
  - `journal_aba_da_tela`: `SCREEN_ROOM_2008` dá 1, `SCREEN_TERMINAL_2046` dá 3 e `SCREEN_ROOM_2` dá 0.
- [ ] **Step 2: implementar as tabelas das 4 salas** (o conteúdo está na spec §5), mais as 4 funções e as constantes. O `JOURNAL_TAB_HIDDEN` vira 5.
- [ ] **Step 3: rodar os testes → GREEN** e rodar o make, que deve sair sem nenhum aviso.
- [ ] **Step 4: commit** com a mensagem `feat: dados e logica do diario guia`.

### Task 2: Novo `DrawJournal` (6 abas)

**Files:** `src/funcoes.c` (substitui `DrawJournal`, `DrawJournalTabs` e as funções `DrawJournalHidden*`)

**Interfaces (consome):** as da Task 1 e o `HIDDEN_ITEMS`. **Produz:** `void DrawJournal(const GameState *game)`, com o protótipo de sempre.

- [ ] **Step 1: o teste visual tem que falhar (RED).** `journalprev.c` renderiza o `DrawJournal` real em 6 estados que imitam as prévias aprovadas e confere se a página direita de 2026 tem o texto "COMO RESOLVER". Para isso, compara os pixels da faixa do título com uma renderização de referência do próprio título. Antes da mudança deve falhar, porque hoje aparece outro conteúdo.
- [ ] **Step 2: implementar o desenho** de acordo com as prévias:
  - livro, 6 abas à direita e cadeado nas abas travadas;
  - página esquerda com o próximo passo e as pistas;
  - página direita com o passo a passo;
  - aba RECOMPENSAS: cartões, história e cadeados;
  - aba OCULTOS: grade 2x2, com as dicas;
  - página de aba travada: "Arquivo ainda não alcançado";
  - texto que não cabe na página quebra de linha automaticamente, e nenhum texto pode passar da borda.
- [ ] **Step 3: GREEN e conferência visual** das 6 PNGs contra `previas/diario_*.png`.
- [ ] **Step 4: commit** com a mensagem `feat: novo desenho do diario com 6 abas`.

### Task 3: `UpdateJournal` único, ESC e 2048

**Files:** `src/funcoes.c` (salas 1990, 2008, 2026, 2048, `UpdateRoom` antiga e `UpdatePhaseControls`), `src/funcoes.h`, `src/main.c`

- [ ] **Step 1: RED.** O teste usa uma função pura `static bool JournalWantsToggle(const GameState *g, bool keyD, bool hudClick)`:
  - `journal_d_bloqueado_digitando`: na tela `SCREEN_TERMINAL_2046`, no estado EU_DECIDO, `keyD` não abre o diário;
  - `journal_esc_fecha`: `PhaseEscClosesJournal(game)` fecha o diário quando ele está aberto e devolve true, e o menu da fase não abre.
- [ ] **Step 2: implementar.**
  - `UpdateJournal` assume o controle do diário. Ao abrir, a aba vai para a da sala atual (`JournalTabForScreen`). Com o diário aberto, ele trata cliques nas abas (só nas destravadas), o botão fechar e o ESC.
  - Nas salas, os blocos próprios de diário dão lugar a `if (UpdateJournal(game)) return;`.
  - `R26UpdateJournal` e o código antigo saem.
  - O `UpdatePhaseControls` passa a fechar o diário no ESC.
  - Em 2048, o rodapé ganha o `DrawJournalHudButton`, e o `main.c` desenha o diário quando ele está aberto.
- [ ] **Step 3: GREEN.** A suíte inteira tem que passar: golden 19, inv 14, menu, textinput e journal. O build não pode gerar nenhum aviso.
- [ ] **Step 4: commit** com a mensagem `feat: diario unico em todas as salas, ESC fecha e diario em 2048`.

### Task 4: Verificação final e merge

- [ ] **Step 1:** prévias finais das 6 abas copiadas para `Documents\Elira\previas\diario_real_*.png`.
- [ ] **Step 2:** revisão independente da branch.
- [ ] **Step 3:** roteiro manual com o Victor:
  - D e ESC em cada sala;
  - o diário em 2048 enquanto digita;
  - admin pulando a fase;
  - pegar um oculto.
- [ ] **Step 4:** merge `--no-ff` na `developer`, sem push.
