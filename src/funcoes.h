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
    SCREEN_RANKING,
    SCREEN_FAILURE,
    SCREEN_INTRO_2008,
    SCREEN_INTRO_2026,
    SCREEN_INTRO_2048,
    SCREEN_INTRO_2046,
    SCREEN_TERMINAL_2046,
    SCREEN_RESULT_2046,
    SCREEN_INVASION,    /* Modo Invasao (src/invasao.c) */
    SCREEN_IAFIGHT      /* Modo IA FIGHT (src/iafight.c); manter os modos extras sempre por ultimo */
} GameScreen;

/* --- SALA 2048 (identificadores com sufixo 2046 por historico): subestados, arquivos e desfechos do terminal --- */
typedef enum Term2046State
{
    TERM2046_STATE_TERMINAL = 0,    /* menu de acoes da investigacao */
    TERM2046_STATE_ESCOLHE_ARQUIVO, /* lista de arquivos (abrir, comparar, verificar) */
    TERM2046_STATE_ANOTAR,          /* lista de 7 principios */
    TERM2046_STATE_NOTAS,           /* ver/descartar anotacoes */
    TERM2046_STATE_AVISO_ARIEL,     /* Ariel interfere */
    TERM2046_STATE_CONFIGURACAO,    /* 3 etapas: fontes, incerteza, decisoes */
    TERM2046_STATE_CONFIRMA_CONFIG, /* aplicar ou refazer */
    TERM2046_STATE_CORRECAO_ARIEL,  /* "Deseja que eu corrija automaticamente?" */
    TERM2046_STATE_EU_DECIDO,       /* digitar a frase final */
    TERM2046_STATE_FIM              /* barra de reconstrucao e desfecho */
} Term2046State;

typedef enum Term2046File
{
    TERM2046_FILE_NONE = 0,
    TERM2046_FILE_PROTOCOLO,
    TERM2046_FILE_DOCUMENTOS,
    TERM2046_FILE_SISTEMA,
    TERM2046_FILE_ALTERACOES,
    TERM2046_FILE_RELATORIO,
    TERM2046_FILE_RELATORIO_ARIEL,
    TERM2046_FILE_COUNT
} Term2046File;

typedef enum Term2046ListUse
{
    TERM2046_LIST_OPEN = 0,
    TERM2046_LIST_COMPARE1,
    TERM2046_LIST_COMPARE2,
    TERM2046_LIST_VERIFY
} Term2046ListUse;

typedef enum Term2046Warning
{
    TERM2046_WARN_CORROMPIDO = 0,
    TERM2046_WARN_COMPARACAO,
    TERM2046_WARN_SUGESTAO
} Term2046Warning;

typedef enum Term2046Pending
{
    TERM2046_PENDING_FILE = 0,
    TERM2046_PENDING_COMPARE
} Term2046Pending;

typedef enum Term2046ViewKind
{
    TERM2046_VIEW_INTRO = 0,
    TERM2046_VIEW_FILE,
    TERM2046_VIEW_COMPARE,
    TERM2046_VIEW_VERIFY,
    TERM2046_VIEW_VERIFY_NOTES,
    TERM2046_VIEW_CRACHA            /* arquivo oculto do cracha de M. Okafor (colecionavel H) */
} Term2046ViewKind;

typedef enum Term2046Sabotage
{
    TERM2046_SABOTAGE_TAMPER = 0,   /* adulterar protocolo */
    TERM2046_SABOTAGE_ERASE,        /* apagar nota */
    TERM2046_SABOTAGE_FAKE_NOTE     /* nota falsa */
} Term2046Sabotage;

typedef enum Term2046Ending
{
    TERM2046_ENDING_NONE = 0,
    TERM2046_ENDING_RUIM,
    TERM2046_ENDING_INCOMPLETO,
    TERM2046_ENDING_BOM,
    TERM2046_ENDING_DOMINIO,
    TERM2046_ENDING_ABANDONO        /* jogador escolheu [0] Sair */
} Term2046Ending;

/* --- COLECIONAVEIS OCULTOS (um por epoca; logica formal E, F, G, H) --- */
typedef enum HiddenItem
{
    HIDDEN_1990 = 0,   /* E - Radio portatil */
    HIDDEN_2008,       /* F - Celular de flip */
    HIDDEN_2026,       /* G - Smartphone */
    HIDDEN_2048,       /* H - Cracha digital */
    HIDDEN_COUNT
} HiddenItem;

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
    bool phaseMenuOpen;
    bool exitRequested;
    char playerName[PLAYER_NAME_MAX + 1];
    int playerNameLength;
    char code[5];
    int codeLength;
    double startTime;
    double elapsedTime;
    bool runTimeRecorded;              /* UH18: tempo total da partida ja gravado no desfecho */
    int finalScore;                    /* UH19: pontuacao final da partida */
    int rankingPlace;                  /* UH19: posicao (0-based) desta partida no ranking, -1 = fora/erro */
    bool rankingSaveOk;                /* UH19: gravacao no TXT funcionou */
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

    /* --- SALA 2026: LABORATORIO DE SINTESE (VIES ALGORITMICO) --- */
    int modal2026;                     /* Room2026Modal: painel aberto na sala */
    bool manual2026Read;               /* Etapa 1: manual tecnico consultado */
    bool sample2026Analyzed[R2026_MATERIAL_COUNT]; /* Amostras varridas no analisador */
    int sample2026Viewing;             /* Amostra exibida no analisador (-1 = nenhuma) */
    float sample2026ScanTimer;         /* Animacao da varredura espectral */
    bool contest2026Picking;           /* Terminal: escolhendo o material do laudo */
    bool laudo2026Done;                /* Etapa 1: laudo correto (Neodimio) registrado */
    bool decisionLog2026Read;          /* Gaveta 1: log de decisoes consultado */
    bool dataset2026Seen;              /* Etapa 2: dados de treinamento auditados */
    bool board2026Read;                /* Quadro de avisos (memorando + Anexo B) lido */
    char padlock2026Input[R2026_CODE_LEN + 1]; /* Digitos do cadeado do painel */
    int padlock2026Length;
    int padlock2026Attempts;           /* Tentativas erradas no cadeado */
    bool weights2026Unlocked;          /* Cadeado do painel de pesos aberto */
    int weights2026Draft[R2026_CRITERIA_COUNT];   /* Pesos em edicao no painel (%) */
    int weights2026Applied[R2026_CRITERIA_COUNT]; /* Pesos ativos no algoritmo de Ariel (%) */
    bool weights2026Calibrated;        /* Etapa 3: norma aplicada, Ariel recomenda Neodimio */
    bool autoExec2026Active;           /* Ariel ameaca executar a recomendacao sozinha */
    float autoExec2026Timer;           /* Contagem local ate a execucao automatica */
    int feedback2026Tone;              /* 0 = aviso, 1 = erro, 2 = sucesso */
    bool feedback2026Fatal;            /* Vidas esgotadas: o modal leva a SCREEN_FAILURE */
    char feedback2026Title[64];
    char feedback2026Lines[4][160];
    bool room2026Processing;           /* Camara sintetizando a liga do SSD */
    float room2026ProcessTimer;        /* Temporizador da barra de progresso */
    bool room2026Completed;            /* SSD obtido */
    bool room2026FirstTrySuccess;      /* Bonus: sala concluida sem nenhum erro */
    int room2026MistakesCount;         /* Erros cometidos (laudo errado, sintese falha) */

    /* --- SALA 2048: TERMINAL DE RECUPERACAO DA A.R.1.3.L --- */
    int term2046State;                 /* Term2046State */
    int term2046Influence;             /* 0 a T2046_INFLUENCE_MAX */
    bool term2046ReadProtocol;         /* leu_protocolo */
    bool term2046ReadDocuments;        /* leu_documentos */
    bool term2046ReadChanges;          /* leu_alteracoes */
    bool term2046ReadFakeReport;       /* leu_relatorio_ariel */
    bool term2046ProtocolTampered;     /* protocolo_adulterado */
    bool term2046FakeNote;             /* nota_falsa_ariel */
    bool term2046VerifyUnlocked;       /* verify_liberado */
    bool term2046FoundSources;         /* achou_fontes */
    bool term2046FoundUncertainty;     /* achou_incerteza */
    bool term2046FoundDecision;        /* achou_decisao */
    int term2046Principles;            /* principios_encontrados (0 a 3) */
    bool term2046InterruptedCompare;   /* ariel_interrompeu_comparacao */
    bool term2046DiscoveredFakeReport; /* descobriu_relatorio_falso */
    int term2046ActionsSinceSabotage;  /* acoes_desde_sabotagem */
    int term2046NextSabotage;          /* proxima_sabotagem */
    int term2046ListUse;               /* Term2046ListUse */
    int term2046FirstFile;             /* primeiro arquivo da comparacao */
    int term2046WarningType;           /* Term2046Warning */
    int term2046PendingKind;           /* Term2046Pending: conteudo bloqueado pelo aviso */
    int term2046PendingA;
    int term2046PendingB;
    int term2046ViewKind;              /* Term2046ViewKind: conteudo exibido no painel */
    int term2046ViewA;
    int term2046ViewB;
    bool term2046ViewFlag;
    int term2046Scroll;                /* rolagem do conteudo (em linhas) */
    int term2046Config[3];             /* fontes, incerteza, decisoes (0 = nao escolhido, 1 ou 2) */
    int term2046ConfigStep;            /* etapa 0 a 2 */
    char terminal2046Input[16];        /* texto digitado no estado EU_DECIDO */
    int terminal2046InputLength;
    int term2046Ending;                /* Term2046Ending */
    float term2046BarTimer;            /* acumulador da barra de reconstrucao */
    bool term2046BarDone;
    char term2046Log[T2046_LOG_MAX][T2046_LOG_LEN];
    int term2046LogKind[T2046_LOG_MAX];
    int term2046LogCount;

    /* --- CONFRONTO FINAL: SALA DE DIALOGO ELIRA x ARIEL --- */
    int finalStage;                    /* Etapa atual do roteiro (FinalStage) */
    int finalLine;                     /* Fala atual dentro da etapa */
    float finalTypeTimer;              /* Tempo acumulado do efeito maquina de escrever */
    int finalPoints;                   /* Pontos de compreensao (0 a 2) */
    int finalChoice[3];                /* Escolhas: P1, P2, Decisao final (-1 = pendente) */

    /* --- COLECIONAVEIS OCULTOS: hiddenFound[i] = E, F, G, H --- */
    bool hiddenFound[HIDDEN_COUNT];

    /* --- INTRODUCAO CINEMATOGRAFICA NARRATIVA --- */
    float crawlScroll;
    float cinematicTimer;
    int cinematicPhase;
} GameState;

typedef struct RankingEntry
{
    char name[PLAYER_NAME_MAX + 1];
    int score;
    int seconds;        /* tempo de conclusao em segundos */
    char date[20];      /* "DD/MM/AAAA HH:MM" */
    char ending[48];    /* US03: final alcancado (ex.: "Final Padrão - Trégua Frágil") */
} RankingEntry;

typedef struct CrawlEntry
{
    const char *text;
    int type; /* 0: Titulo ELIRA, 1: Corpo, 2: Destaque/Citacao, 3: Ano 1990, 4: Espaco vazio */
} CrawlEntry;

/* Sala 2026: paineis/modais do laboratorio (apenas um aberto por vez) */
typedef enum Room2026Modal
{
    R2026_MODAL_NONE = 0,
    R2026_MODAL_MANUAL,      /* manual tecnico: 4 requisitos da liga */
    R2026_MODAL_SAMPLE,      /* analisador espectral de uma amostra */
    R2026_MODAL_TERMINAL,    /* terminal de Ariel: ranking, laudo/contestacao */
    R2026_MODAL_ARCHIVE,     /* arquivo: log de decisoes + dados de treinamento */
    R2026_MODAL_BOARD,       /* quadro de avisos: memorando + Anexo B */
    R2026_MODAL_PADLOCK,     /* cadeado do painel de pesos */
    R2026_MODAL_WEIGHTS,     /* painel de pesos do algoritmo */
    R2026_MODAL_REACTOR,     /* escolha da amostra para a camara */
    R2026_MODAL_FEEDBACK,    /* resultado de uma acao (acerto/erro) */
    R2026_MODAL_PROCESSING   /* sintese final em andamento */
} Room2026Modal;

typedef struct Material2026Def
{
    const char *name;           /* "Ferro", "Cobalto", "Neodímio", "Níquel" */
    const char *upper;          /* nome em maiusculas (UTF-8) */
    const char *symbol;         /* "Fe", "Co", "Nd", "Ni" */
    const char *state;          /* estado fisico */
    const char *origin;         /* origem mineral */
    const char *magnetism;      /* propriedade magnetica */
    bool rareEarth;             /* apenas o Neodimio */
    int trainingRecords;        /* registros nos dados de treinamento de Ariel */
    const char *verdict;        /* comparacao com os 4 requisitos do manual */
    int criteria[R2026_CRITERIA_COUNT]; /* notas 0-100 por criterio do algoritmo */
    Color accent;
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
    ENDING_AUTOPILOT,       /* Ariel continua decidindo */
    ENDING_GOLDEN_PACT      /* Rota Dourada: humanos e Ariel trabalham juntos (G && K) */
} FinalEnding;

/* Categoria formal do final (secao 25 do documento do projeto) */
typedef enum EndingCategory
{
    ENDING_CAT_RUIM = 0,    /* R = !V || !T */
    ENDING_CAT_PADRAO,      /* V && T && !(G && K) */
    ENDING_CAT_OTIMO        /* G && K, com G = V && M && O */
} EndingCategory;

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
extern Texture2D g_menuCoverElira;
extern Texture2D g_menuCoverLogo;
extern bool g_menuCoverLoaded;
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
bool IsAdminPlayer(const GameState *game);
bool IsPhaseScreen(GameScreen screen);
bool UpdatePhaseControls(GameState *game);
void DrawPhaseControls(const GameState *game);

/* UH18 - Medicao de tempo de conclusao */
void RecordRunTime(GameState *game);
const char *FormatRunTime(double seconds);

/* UH19 - Ranking (arquivo TXT: Nome | Pontuacao | MM:SS - DD/MM/AAAA HH:MM) */
int CalculateScore(const GameState *game);
bool SaveRankingEntry(const char *name, int score, int seconds, const char *ending);
int LoadRanking(void);
const RankingEntry *GetRankingEntries(int *count);
void FinalizeRun(GameState *game);
void FinalizeFailedRun(GameState *game);   /* Final Ruim: vidas esgotadas */
void DrawRanking(const GameState *game);
void UpdateRanking(GameState *game);

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
void DrawMissionBriefing(const GameState *game, int missionIndex);
void DrawIntro(const GameState *game);
void DrawIntro2008(const GameState *game);
void DrawIntro2026(const GameState *game);
void DrawIntro2048(const GameState *game);
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
void DrawJournal(const GameState *game);

/* Colecionaveis ocultos (Rota Dourada) - logica centralizada em funcoes.c */
void CollectHidden(GameState *game, int item);          /* marca o oculto e avisa o jogador */
int HiddenFoundCount(const GameState *game);
bool HasAllHidden(const GameState *game);               /* O = E && F && G && H */
const char *HiddenItemName(int item);
void DrawHiddenGlint(Rectangle area, float phase);      /* brilho discreto sobre o objeto escondido */
bool UpdateJournal(GameState *game);  /* diario guia: abre/fecha e trata abas; true = entrada consumida */
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
void LoadMenuCover(void);   /* capa do menu inicial; sem ela o menu usa o visual antigo */
void UnloadMenuCover(void);
void LoadHeartSprites(void);
void UnloadHeartSprites(void);
void DrawLivesIcons(Vector2 position, int lives, float heartSize);
void DrawHudYearOperator(const char *year, const char *playerName, int y);  /* topo das salas: ano na ponta + operador */
Rectangle DrawHudLivesTimer(int lives, int totalSeconds);                   /* topo das salas: vida | tempo centralizados; devolve a area ocupada */
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

/* Fase 3 (Ano 2026) - Laboratorio de Sintese: Vies Algoritmico (SSD) */
void DrawHudRoom2026(const GameState *game);
void DrawRoom2026(const GameState *game);
void UpdateRoom2026(GameState *game);
void DrawResult2026(const GameState *game);
void UpdateResult2026(GameState *game);

/* Fase 4 (Ano 2048) - Terminal de Recuperacao da A.R.1.3.L */
void InitTerminal2046(GameState *game);
void DrawIntro2046(const GameState *game);
void DrawTerminal2046(const GameState *game);
void UpdateTerminal2046(GameState *game);
void DrawResult2046(const GameState *game);
void UpdateResult2046(GameState *game);
bool Terminal2046WantsTextInput(const GameState *game);   /* true no estado EU_DECIDO */
bool ScreenWantsTextInput(const GameState *game);         /* true quando o teclado esta digitando texto (atalhos de letra ficam desligados) */

/* Confronto Final (2048) - Sala de Dialogo Elira x Ariel */
bool IsPortraitBackground(Color c);
void LoadEliraPortrait(void);
int DrawTextWrappedPartial(const char *text, int visibleBytes, int x, int y, int size,
                           int maxWidth, int lineHeight, Color color);
const char **GetFinalOptions(int stage);
const DialogueLine *GetFinalLines(const GameState *game, int *count);
int GetFinalEnding(const GameState *game);
/* Logica central dos finais (V, T, M, O, K) */
bool HasMainArtifact(const GameState *game, int index);  /* 0..3 = A (1990), B (2008), C (2026), D (2048) */
int MainArtifactCount(const GameState *game);
bool UsedGoldenDialogue(const GameState *game);         /* K */
int GetEndingCategory(const GameState *game);            /* EndingCategory */
const char *GetEndingResultName(const GameState *game);  /* texto gravado no TXT/ranking */
int FinalOptionCount(const GameState *game);             /* 3, ou 4 com os ocultos (O) */
bool FinalIsChoiceMode(const GameState *game);
Rectangle FinalBoxRect(bool choiceMode, int optionCount);
Rectangle FinalOptionRect(int index, int optionCount);
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
