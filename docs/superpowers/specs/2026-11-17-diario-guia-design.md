# Diário como guia do jogador: design

Data: 2026-11-17 · Branch: `feature/diario-guia` → merge só na `developer`

## 1. Intenção

O Victor quer que o diário seja o **guia do jogador** e o **registro da história**. Hoje ele mistura textos
longos de contexto com instruções genéricas. Além disso, a aba de 2026 mostra pistas de uma sala que
não existe mais (Turing 1950, Dartmouth), e não há aba de 2048.

O diário passa a:
- registrar cada pista que o jogador encontra, com o conteúdo dela;
- dizer o **próximo passo** e mostrar o **passo a passo com as respostas** (decisão do Victor:
  mostrar a resposta para ninguém ficar preso);
- contar a história de A.R.1.3.L. pelas recompensas, que evolui a cada era.

Prévias aprovadas pelo Victor em 2026-11-17: `Documents\Elira\previas\diario_1..6_*.png`.

## 2. Objetivos, não objetivos e antiobjetivos

**Objetivos**
- 6 abas: **1990, 2008, 2026, 2048, RECOMPENSAS, OCULTOS**.
- **Aba de sala:**
  - página esquerda: **PRÓXIMO PASSO** e as **PISTAS ENCONTRADAS** (o que não foi investigado aparece cinza);
  - página direita: **COMO RESOLVER**, com os passos marcados (feito ✓ / atual ▶ / pendente ○) e as
    respostas.
- **Quando a resposta aparece:** só depois que a pista ligada a ela foi encontrada. Exemplo: a senha 118
  aparece depois que o jogador viu os dados de treino.
- **Aba RECOMPENSAS:** um cartão por artefato (Disquete 1990, CD 2008, SSD 2026, Protocolo original 2048),
  com o que ele revela, e a página "A HISTÓRIA ATÉ AQUI", que ganha um parágrafo por era concluída.
- **Aba OCULTOS:** sempre visível, com os 4 espaços. Os achados mostram o registro; os outros, uma dica de
  onde procurar.
- **Abertura:** o diário abre na aba da sala atual.
- **Disponível em 2048:** o terminal de 2048 ganha o botão do diário. A tecla D não abre o diário enquanto o
  jogador digita "EU DECIDO".
- **ESC fecha o diário** (hoje ele abre o menu da fase por cima). O rodapé já promete "Fechar [ESC/D]".

**Não objetivos**
- Mudar puzzles, salas ou finais.
- Redesenhar a sala de 2026 (a aba de 2026 segue a sala **atual**; se a sala mudar, a aba muda junto).

**Antiobjetivos**
- Diário com pistas da sala errada.
- Mostrar resposta de uma pista que o jogador ainda não encontrou.
- Alterar assets existentes (os sprites de recompensa são só desenhados em outra escala).

## 3. Restrições

- Modularização do PDF: o diário fica em `funcoes.c`/`funcoes.h`, com as constantes em `constantes.h`.
- C99 com `-Wall -Wextra`, sem aviso novo.
- As pistas usam os flags que já existem no `GameState`. Não há campo novo no estado, exceto se for indispensável.
- Os textos de recompensa e de história são rascunho meu, e o grupo revisa.

## 4. Abordagens

| | Abordagem | Prós | Contras |
|---|---|---|---|
| **A** | **Diário dirigido por dados:** tabelas de pistas e passos por sala, com funções-condição sobre o `GameState`; um único `UpdateJournal` e um único `DrawJournal` para todas as salas | **Uma lógica só; testável; acrescentar pista é uma linha** | Reescreve ~1.000 linhas do diário atual |
| B | Reescrever as páginas de cada sala à mão, como hoje | Mais próximo do código atual | Repete a lógica 4 vezes; cada sala trata abas e cliques à sua maneira (é o que gerou a aba errada de 2026) |

**Escolhida: A** (minha decisão; o Victor aprovou o resultado pelas prévias).

## 5. Como fica

- **`constantes.h`:**
  - `JOURNAL_TAB_1990 0`, `JOURNAL_TAB_2008 1`, `JOURNAL_TAB_2026 2`, `JOURNAL_TAB_2048 3`;
  - `JOURNAL_TAB_REWARDS 4`, `JOURNAL_TAB_HIDDEN 5` (era 4), `JOURNAL_TAB_COUNT 6`.
- **`funcoes.h`:**
  - `bool UpdateJournal(GameState *game)`: abre e fecha (D ou o botão do HUD) e, com o diário aberto, trata
    as abas, o ESC e o botão fechar. Retorna `true` quando o diário está aberto, e a sala para de processar
    o próprio clique.
  - `void DrawJournal(const GameState *game)`: reescrito.
- **`funcoes.c`:**
  - **Dados:** tabelas `JournalClue {título, linha1, linha2, found()}` e
    `JournalStep {texto, resposta1, resposta2, done(), answerVisible()}` por sala; cartões de recompensa
    `obtained()` + texto; frases da história por era.
  - **Funções puras testáveis:** `JournalCurrentStep(game, room)`, `JournalTabUnlocked(game, tab)`,
    `JournalTabForScreen(screen)`.
  - **Chamadas nas salas:** 1990 (`UpdateRoom2`), 2008 (`UpdateRoom2008`), 2026 (onde hoje chama
    `R26UpdateJournal`), 2048 (`UpdateTerminal2046`) e a sala antiga `UpdateRoom` passam a chamar
    `UpdateJournal`, e os blocos próprios de diário saem.
  - **Funções removidas:** `R26UpdateJournal`, `DrawJournalTabs`, `DrawJournalHiddenTab`,
    `UpdateJournalHiddenTab`, `DrawJournalHiddenPages` e o conteúdo antigo.
  - **`UpdatePhaseControls`:** com o diário aberto, o ESC fecha o diário e não abre o menu.
- **`main.c`:** `if (game.journalOpen) DrawJournal(&game);` também na tela `SCREEN_TERMINAL_2046`.
- **2048:** `DrawJournalHudButton` no rodapé do terminal. A tecla D é ignorada quando
  `ScreenWantsTextInput(game)`.

### Conteúdo das salas (o que encontra → o que responde)

- **1990**
  - Pistas: armários (`room2CabinetInspected`), porta (`room2DoorInspected`), acesso do terminal
    (`memory1990Completed`) e arquivos indexados (`puzzleFilesCompleted`).
  - Passos:
    1. Desbloquear o terminal: sequência de luzes.
    2. Organizar os arquivos (PARAM_NEURAL→PARÂMETROS, CORE_SISTEMA→SISTEMA, BASE_DADOS→BANCO DE DADOS,
       INDICE_MEMORIA→REGISTROS). A resposta aparece depois do passo 1.
    3. Sair pela porta: Disquete.
- **2008**
  - Pistas: docs 1–4, caderno do Dr. Ramos e memorando.
  - Passos:
    1. Examinar os 4 documentos (n/4).
    2. Marcar Registro de Entrada (#3) + Câmera Interna (#4). A resposta aparece com o doc 3 e o doc 4
       examinados.
    3. Suspender a ordem: CD.
    4. (Opcional) Configuração Local, quando liberada.
- **2026**
  - Pistas: manual, amostras (n/4), log de decisões, dados de treino e quadro/Anexo B.
  - Passos:
    1. Contestar com NEODÍMIO (resposta com manual lido + 4 amostras).
    2. Cadeado 118 (resposta com os dados de treino vistos).
    3. Pesos 50/25/15/10/0 (resposta com o quadro lido).
    4. Inserir o Neodímio: SSD.
- **2048**
  - Pistas: protocolo.txt, relatorio.txt, alterações e relatório falso de Ariel.
  - Passos:
    1. Abrir e comparar os arquivos.
    2. Anotar MOSTRAR FONTES / COMUNICAR INCERTEZA / HUMANO POSSUI A DECISÃO FINAL (resposta depois do
       passo 1).
    3. Restaurar: Mostrar / Admitir quando não souber / Humano decide.
    4. Digitar EU DECIDO.
- **Recompensas obtidas:**
  - Disquete: `year2008Unlocked`;
  - CD: `puzzle2008OrderSuspended`;
  - SSD: `room2026Completed`;
  - Protocolo: `term2046State == TERM2046_STATE_FIM`.

## 6. Verificação

1. **Teste de lógica** (`journal_test.c`, harness fora do repositório):
   - passo atual de cada sala em estados conhecidos;
   - visibilidade das respostas;
   - abas travadas e destravadas;
   - recompensas obtidas;
   - `JOURNAL_TAB_HIDDEN == 5`.
2. **Prévias do `DrawJournal` real** nas 6 abas, comparadas com as aprovadas.
3. **Regressão:** `golden_test` (19), `inv_test` (14), menu, textinput. Build sem aviso.
4. **Roteiro manual:**
   - D abre na sala atual e ESC fecha sem abrir o menu;
   - abas travadas mostram cadeado;
   - em 2048, o botão abre o diário e digitar "EU DECIDO" não abre.

## 7. Decisões minhas (Victor pode corrigir)

- **Abordagem A**, dirigida por dados.
- **Recompensa de 2048:** "Protocolo original".
- **Dicas dos ocultos**, a partir da posição real de cada item:
  - 1990: "olhe embaixo da mesa do terminal";
  - 2008: "o vaso de planta guarda algo";
  - 2026: "olhe o chão perto das amostras";
  - 2048: "confira o canto inferior esquerdo do terminal".
- O texto longo de contexto de 1990/2008 sai do diário; a história passa a ficar na aba RECOMPENSAS.
- `journalPage` deixa de ser usado; o campo continua no `GameState` para não mexer no struct.
