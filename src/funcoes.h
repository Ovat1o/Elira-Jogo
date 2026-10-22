#ifndef FUNCOES_H
#define FUNCOES_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdbool.h>
#include "constantes.h"

/* ========================================================================= */
/* ENUMERACOES E ESTRUTURAS DE DADOS DO JOGO                                 */
/* ========================================================================= */

typedef enum GameScreen
{
    SCREEN_TITLE,
    SCREEN_SETTINGS,
    SCREEN_PLAYER_NAME,
    SCREEN_CINEMATIC_INTRO,
    SCREEN_INTRO,
    SCREEN_ROOM,
    SCREEN_TERMINAL,
    SCREEN_RESULT,
    SCREEN_ROOM_2,
    SCREEN_MEMORY_1990,
    SCREEN_PUZZLE_FILES,
    SCREEN_RESULT_2,
    SCREEN_ROOM_2008,
    SCREEN_PUZZLE_2008,
    SCREEN_RESULT_2008,
    SCREEN_ROOM_2026,
    SCREEN_RESULT_2026,
    SCREEN_FINAL_DIALOGUE,
    SCREEN_FINAL_ENDING,
    SCREEN_FAILURE
} GameScreen;

typedef struct GameState
{
    GameScreen screen;
    int lives;
    bool foundCalendar;
    bool foundBooks;
    bool foundDrawer;
    bool foundBlueprint;
    bool foundTape;
    bool journalOpen;
    int journalPage;
    int journalYearTab;
    bool musicMuted;
    char playerName[PLAYER_NAME_MAX + 1];
    int playerNameLength;
    char code[5];
    int codeLength;
    double startTime;
    double elapsedTime;
    char message[160];
    float messageTimer;

    /* --- QUARTO 2 (1990): MINIGAME DE MEMORIA & PUZZLE DE ARQUIVOS/PASTAS --- */
    bool room2CabinetInspected;
    bool room2DoorInspected;
    bool memory1990Completed;
    int memorySequence[5];
    int memoryRound;                /* Rodada atual: 1 a 5 */
    int memoryInputIndex;           /* Indice de entrada do jogador na rodada */
    int memoryState;                /* 0=demonstrando, 1=entrada, 2=etapa ok, 3=erro, 4=concluido */
    int memoryShowIndex;            /* Posicao sendo demonstrada na sequencia */
    float memoryTimer;              /* Temporizador de animacao da sequencia/feedback */
    bool memoryLightOn;             /* Estado aceso/apagado da demonstracao */
    int memoryLastClicked;          /* Ultimo botao clicado na grade 3x3 (0..8) */
    float memoryClickTimer;         /* Duracao do brilho do clique */
    bool puzzleFilesCompleted;
    int draggingFileIndex;
    int fileConnections[4];
    int rightFolderColors[4];
    float puzzleCompleteTimer;
    int puzzleFilesErrorFolder;     /* Pasta que recebeu tentativa incorreta (-1 = nenhuma) */
    float puzzleFilesFeedbackTimer; /* Temporizador de feedback no painel de arquivos */
    bool puzzleFilesFeedbackIsError;
    char puzzleFilesFeedback[128];

    /* --- CONTROLE TEMPORAL (1990 -> 2008 -> 2026 -> 2048) --- */
    bool year2008Unlocked;
    bool year2026Unlocked;
    bool year2048Unlocked;

    /* --- FASE 2008: ESCRITORIO DE VIGILANCIA & PUZZLE DE RECONHECIMENTO FACIAL --- */
    bool doc2008FaceExamined;          /* Doc 1: Comparacao Facial (Drone) */
    bool doc2008FlyerExamined;         /* Doc 2: Panfleto da Revolta */
    bool doc2008EntryExamined;         /* Doc 3: Registro de Entrada da Empresa */
    bool doc2008CameraExamined;        /* Doc 4: Camera Interna (CFTV Servidores) */
    bool doc2008FatherOriginExamined;  /* Doc 5: Notas Humanitarias do Dr. Ramos */
    bool doc2008GovContractExamined;   /* Doc 6: Memorando Governamental 2008 */
    bool room2008OptionalUnlocked;     /* Pista compreendida -> Libera aba de Configuracao Local */
    bool room2008OptionalCompleted;    /* Despacho automatico local desativado mantendo provas */
    bool puzzle2008Selected[4];        /* Checkbox de selecao dos 4 registros */
    int puzzle2008SelectionCount;      /* Quantidade selecionada (0 a 2) */
    bool puzzle2008Contested;          /* Falso positivo comprovado no terminal */
    bool puzzle2008OrderSuspended;     /* Ordem de interceptacao suspensa com sucesso */
    char puzzle2008Feedback[160];      /* Feedback da combinacao selecionada */
    float puzzle2008FeedbackTimer;     /* Temporizador de feedback */
    int activeDocument2008;            /* -1 = nenhum, 0..3 = registros, 4 = pai, 5 = memorando */
    int terminal2008ActiveTab;         /* 0 = Ordem/Evidencias, 1 = Configuracao Local */

    /* --- FASE 2026: SALA DE HIDROGENIO (ALUCINACAO IA & TERRAS RARAS) --- */
    bool manual2026Read;               /* Jogador leu o manual tecnico */
    bool manual2026Open;               /* Modal de leitura do manual tecnico aberto */
    bool terminal2026Open;             /* Modal/visao do terminal da Ariel */
    int selectedMaterial2026;          /* -1 = nenhum, 0 = A-17, 1 = B-04, 2 = C-22, 3 = D-09 */
    bool confirmModal2026Open;         /* Caixa de confirmacao aberta */
    int pendingConfirmMaterial2026;    /* Material pendente de confirmacao (0..3) */
    bool feedbackModal2026Open;        /* Modal de alerta de erro ativo */
    int lastErrorMaterial2026;         /* Material do ultimo erro (0, 1, 3) */
    bool room2026Processing;           /* Reator processando a liga metalica */
    float room2026ProcessTimer;        /* Temporizador da barra de progresso */
    bool room2026Completed;            /* Reator estabilizado / SSD obtido */
    bool room2026FirstTrySuccess;      /* Bonus: acertou na primeira tentativa sem errar */
    int room2026MistakesCount;         /* Contagem de erros cometidos */
    bool room2026ThermalCollapse;      /* Colapso termico ao zerar vidas */
    bool sample2026Analyzed[4];        /* Amostras A-17, B-04, C-22, D-09 analisadas */
    bool analyzingSample2026;          /* Espectro em processo de analise (~2s) */
    float analyzingTimer2026;          /* Timer da animacao do espectro */
    int analyzingSampleIndex2026;      /* Indice da amostra sendo analisada */
    bool reactorCountdownActive2026;   /* Countdown local do reator ativo (inicia no terminal) */
    float reactorTimer2026;            /* Contador local de estabilidade (50s) */
    bool room2026TimeOut;              /* Colapso por tempo esgotado no reator */

    /* --- CONFRONTO FINAL: SALA DE DIALOGO ELIRA x ARIEL --- */
    int finalStage;                    /* Etapa atual do roteiro (FinalStage) */
    int finalLine;                     /* Fala atual dentro da etapa */
    float finalTypeTimer;              /* Tempo acumulado do efeito maquina de escrever */
    int finalPoints;                   /* Pontos de compreensao (0 a 2) */
    int finalChoice[3];                /* Escolhas: P1, P2, Decisao final (-1 = pendente) */

    /* --- INTRODUCAO CINEMATOGRAFICA NARRATIVA --- */
    float crawlScroll;
    float cinematicTimer;
    int cinematicPhase;
} GameState;

typedef struct CrawlEntry
{
    const char *text;
    int type; /* 0: Titulo ELIRA, 1: Corpo, 2: Destaque/Citacao, 3: Ano 1990, 4: Espaco vazio */
} CrawlEntry;

typedef struct Material2026Def
{
    const char *idLetter;       /* "A-17", "B-04", "C-22", "D-09" */
    const char *code;           /* "AMOSTRA A-17", etc. */
    const char *realName;       /* "Magnetita", "Gas Hidrogenio", "Mineral de Terras Raras (Nd)", "Acido Cloridrico" */
    const char *state;          /* "solido", "gasoso", "solido", "liquido" */
    const char *origin;         /* "mineral", "gasoso", "mineral", "solucao" */
    const char *elements;       /* "Fe, O", "H2", "Nd, Ce, La", "HCl" */
    const char *classification; /* "Magnetita", "Hidrogenio", "Mineral portador de Terras Raras", "Solucao corrosiva" */
    bool hasRareEarth;          /* false, false, true, false */
    bool isCorrect;             /* false, false, true, false */
    Color accentColor;
} Material2026Def;

typedef enum FinalStage
{
    FINAL_INTRO = 0,
    FINAL_Q1,
    FINAL_Q1_REACT,
    FINAL_Q2,
    FINAL_Q2_REACT,
    FINAL_DECISION,
    FINAL_DECISION_REACT,
    FINAL_STAGE_COUNT
} FinalStage;

typedef enum FinalSpeaker
{
    SPEAKER_NARRATOR = 0,
    SPEAKER_ARIEL,
    SPEAKER_ELIRA
} FinalSpeaker;

typedef enum FinalEnding
{
    ENDING_HUMAN_HANDS = 0, /* Ferramenta + compreensao total */
    ENDING_FRAGILE_TRUCE,   /* Ferramenta sem compreensao total */
    ENDING_SILENCE,         /* Desligar + compreensao total */
    ENDING_BLACKOUT,        /* Desligar sem compreensao total */
    ENDING_AUTOPILOT        /* Ariel continua decidindo */
} FinalEnding;

typedef struct DialogueLine
{
    int speaker;
    const char *text;
} DialogueLine;

typedef struct AmbientChordDef
{
    float bass_freq;
    int note_count;
    float freqs[6];
    int arp_count;
    float arp_freqs[8];
} AmbientChordDef;

/* ========================================================================= */
/* VARIAVEIS E CORES GLOBAIS COMPARTILHADAS                                  */
/* ========================================================================= */

extern const Color VOID_COLOR;
extern const Color PANEL_COLOR;
extern const Color PANEL_LIGHT;
extern const Color CYAN_COLOR;
extern const Color GOLD_COLOR;
extern const Color RED_COLOR;
extern const Color PAPER_COLOR;
extern const Color INK_COLOR;
extern const Color WIRE_YELLOW;
extern const Color WIRE_RED;
extern const Color WIRE_BLUE;
extern const Color WIRE_PINK;
extern const Color REWARD_BODY_COLOR;

extern int g_currentRes;
extern float g_musicVolume;
extern Texture2D g_eliraPortrait;
extern bool g_eliraPortraitLoaded;
extern Texture2D g_rewardFloppyTex;
extern Texture2D g_rewardCdTex;
extern Texture2D g_rewardSsdTex;
extern bool g_rewardSpritesLoaded;
extern Texture2D g_heartFullTex;
extern Texture2D g_heartEmptyTex;
extern bool g_heartTexturesLoaded;

/* ========================================================================= */
/* HEADERS DE TODAS AS FUNCOES USADAS NO CODIGO                              */
/* ========================================================================= */

/* Compatibilidade entre versoes da raylib */
#if RAYLIB_VERSION_MAJOR < 5 || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR < 5)
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color);
#endif
void DrawCircleGradientCompat(int centerX, int centerY, float radius, Color inner, Color outer);

/* Fontes e Utilitarios de Texto / Interface */
Color GetFileWireColor(int colorIndex);
bool GameFontAvailable(void);
Font GetGameFont(float pixelSize);
void UnloadGameFonts(void);
void DrawGameTextEx(const char *text, Vector2 position, float size, float spacing, Color color);
Vector2 MeasureGameTextEx(const char *text, float size, float spacing);
void DrawGameText(const char *text, int x, int y, int size, Color color);
int MeasureGameText(const char *text, int size);
Vector2 GetVirtualMouse(void);
void ApplyResolution(int resIndex);
void CenterText(const char *text, int y, int size, Color color);
void DrawButton(Rectangle bounds, const char *label, Color accent);
bool Clicked(Rectangle bounds);
void DrawBackground(void);
void DrawTitleSpaceBackground(void);
void SetMessage(GameState *game, const char *message);
int ClueCount(const GameState *game);
int ExaminedDocs2008Count(const GameState *game);

/* Inicializacao e Controle de Estado */
void GenerateMemory1990Sequence(GameState *game);
void StartMemory1990Round(GameState *game, int round);
void InitMemory1990(GameState *game);
void InitRoom2Puzzle(GameState *game);
void InitRoom2008(GameState *game);
void InitFinalDialogue(GameState *game);
void InitRoom2026(GameState *game);
void ResetDemo(GameState *game);
void UpdatePlayerName(GameState *game);

/* Telas de Menu, Configuracoes, Identificacao e Introducao */
void DrawTitleStartButton(Rectangle bounds, const char *label, Color accent);
void DrawTitle(void);
void DrawSettings(const GameState *game);
void UpdateSettings(GameState *game, Music *bgm, bool bgmLoaded);
void DrawPlayerName(const GameState *game);
void DrawIntro(const GameState *game);
void InitCinematicIntro(GameState *game);
void UpdateCinematicIntro(GameState *game);
void DrawCinematicIntro(const GameState *game);

/* Sala 2026 (O Prototipo), HUD, Diario e Terminal 2026 */
void DrawHotspot(Rectangle bounds, const char *label);
void DrawRoomWindow(void);
void DrawRoom(const GameState *game);
void DrawJournalHudButton(const GameState *game);
void DrawHud(const GameState *game);
void DrawPadlockIcon(int centerX, int centerY, float scale, Color bodyColor, Color shackleColor);
void DrawJournalTabs(const GameState *game, int bookX, int bookY);
void DrawJournal(const GameState *game);
void UpdateRoom(GameState *game);
void AddDigit(GameState *game, char digit);
void CheckCode(GameState *game);
void DrawTerminal(const GameState *game);
void UpdateTerminal(GameState *game);
void DrawFloppy(Vector2 position, float scale);
void DrawSsd(Vector2 position, float scale);
void DrawResult(const GameState *game);
void DrawFailure(void);

/* Recompensas Padronizadas (1990, 2008, 2026) e Vidas (Sprite Heart) */
void LoadRewardSprites(void);
void UnloadRewardSprites(void);
void LoadHeartSprites(void);
void UnloadHeartSprites(void);
void DrawLivesIcons(Vector2 position, int lives, float heartSize);
void GetRewardScreenButtons(Rectangle *outNext, Rectangle *outRestart, Rectangle *outMenu);
void DrawRewardHudHeader(const GameState *game);
void DrawRewardSpriteItem(int year, Vector2 center, float maxSize);

/* Quarto 2 (Ano 1990) - Sala de Arquivos, Minigame de Memoria e Puzzle de Pastas */
void DrawTapeCassette(Vector2 position, float scale);
void DrawRoom2(const GameState *game);
void DrawHudRoom2(const GameState *game);
void UpdateRoom2(GameState *game);
void DrawMemoryProgressDots(int startX, int y, int completedStages, int activeRound, bool isError);
void DrawMemory1990(const GameState *game);
void UpdateMemory1990(GameState *game);
void DrawDigitalFileIcon(float x, float y, Color accentColor);
void DrawFolderDirectoryIcon(float x, float y, Color folderColor, bool hasFile);
void DrawPuzzleFiles(const GameState *game);
void UpdatePuzzleFiles(GameState *game);
void DrawResult2(const GameState *game);
void UpdateResult2(GameState *game);

/* Fase 2 (Ano 2008) - Escritorio de Vigilancia & Reconhecimento Facial */
void DrawMemoryCd(Vector2 position, float scale);
void DrawDocumentViewer2008(const GameState *game);
void UpdateDocumentViewer2008(GameState *game);
void DrawRoom2008(const GameState *game);
void DrawHudRoom2008(const GameState *game);
void UpdateRoom2008(GameState *game);
void DrawPuzzle2008(const GameState *game);
void UpdatePuzzle2008(GameState *game);
void DrawResult2008(const GameState *game);
void UpdateResult2008(GameState *game);

/* Fase 3 (Ano 2026) - Sala de Hidrogenio & Sintese de Terras Raras (SSD) */
void DrawHudRoom2026(const GameState *game);
void DrawRoom2026(const GameState *game);
void UpdateRoom2026(GameState *game);
void DrawResult2026(const GameState *game);
void UpdateResult2026(GameState *game);

/* Confronto Final (2048) - Sala de Dialogo Elira x Ariel */
bool IsPortraitBackground(Color c);
void LoadEliraPortrait(void);
int DrawTextWrappedPartial(const char *text, int visibleBytes, int x, int y, int size,
                           int maxWidth, int lineHeight, Color color);
const char **GetFinalOptions(int stage);
const DialogueLine *GetFinalLines(const GameState *game, int *count);
int GetFinalEnding(const GameState *game);
bool FinalIsChoiceMode(const GameState *game);
Rectangle FinalBoxRect(bool choiceMode);
Rectangle FinalOptionRect(int index);
void DrawArielCore(Vector2 c, float s, Color core, float energy, float lookX);
void DrawEliraPortrait(Rectangle dest, bool speaking);
void DrawFinalDialogue(const GameState *game);
void AdvanceFinalStage(GameState *game);
void UpdateFinalDialogue(GameState *game);
void DrawFinalEnding(const GameState *game);
void UpdateFinalEnding(GameState *game);

/* Geracao de Audio Ambiente */
float MidiToFreq(float m);
void EnsureAmbientMusicFile(void);

#endif /* FUNCOES_H */
