#include "raylib.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ACCESS_CODE "1950"
#define PLAYER_NAME_MAX 20

typedef enum GameScreen
{
    SCREEN_TITLE,
    SCREEN_SETTINGS,
    SCREEN_PLAYER_NAME,
    SCREEN_INTRO,
    SCREEN_ROOM,
    SCREEN_TERMINAL,
    SCREEN_RESULT,
    SCREEN_ROOM_2,
    SCREEN_PUZZLE_FILES,
    SCREEN_RESULT_2,
    SCREEN_ROOM_2008,
    SCREEN_PUZZLE_2008,
    SCREEN_RESULT_2008,
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

    /* --- QUARTO 2 & PUZZLE DE ARQUIVOS/PASTAS (AMONG US STYLE) --- */
    bool room2CabinetInspected;
    bool room2DoorInspected;
    bool puzzleFilesCompleted;
    int draggingFileIndex;
    int fileConnections[4];
    int rightFolderColors[4];
    float puzzleCompleteTimer;

    /* --- CONTROLE TEMPORAL (1990 -> 2008 -> 2048) --- */
    bool year2008Unlocked;
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
} GameState;

static const Color VOID_COLOR = {8, 12, 24, 255};
static const Color PANEL_COLOR = {18, 27, 45, 255};
static const Color PANEL_LIGHT = {34, 49, 72, 255};
static const Color CYAN_COLOR = {73, 220, 225, 255};
static const Color GOLD_COLOR = {242, 186, 73, 255};
static const Color RED_COLOR = {231, 76, 94, 255};
static const Color PAPER_COLOR = {230, 220, 184, 255};
static const Color INK_COLOR = {43, 40, 35, 255};
static const Color WIRE_YELLOW = {245, 212, 50, 255};
static const Color WIRE_RED    = {235, 62, 70, 255};
static const Color WIRE_BLUE   = {52, 142, 245, 255};
static const Color WIRE_PINK   = {240, 75, 190, 255};

static Color GetFileWireColor(int colorIndex)
{
    switch (colorIndex)
    {
        case 0: return WIRE_YELLOW;
        case 1: return WIRE_RED;
        case 2: return WIRE_BLUE;
        case 3: return WIRE_PINK;
        default: return RAYWHITE;
    }
}

static int g_currentRes = 0; /* 0: 1280x720, 1: 1600x900, 2: 1920x1080 */
static float g_musicVolume = 0.45f;

static Vector2 GetVirtualMouse(void)
{
    Vector2 mouse = GetMousePosition();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float scale = fminf((float)screenW / SCREEN_WIDTH, (float)screenH / SCREEN_HEIGHT);
    if (scale <= 0.0001f) return mouse;
    float offsetX = (screenW - (SCREEN_WIDTH * scale)) * 0.5f;
    float offsetY = (screenH - (SCREEN_HEIGHT * scale)) * 0.5f;
    Vector2 vm;
    vm.x = (mouse.x - offsetX) / scale;
    vm.y = (mouse.y - offsetY) / scale;
    vm.x = fmaxf(0.0f, fminf((float)SCREEN_WIDTH, vm.x));
    vm.y = fmaxf(0.0f, fminf((float)SCREEN_HEIGHT, vm.y));
    return vm;
}

static void ApplyResolution(int resIndex)
{
    if (resIndex < 0 || resIndex > 2) return;
    g_currentRes = resIndex;
    int w = (resIndex == 0) ? 1280 : (resIndex == 1 ? 1600 : 1920);
    int h = (resIndex == 0) ? 720 : (resIndex == 1 ? 900 : 1080);

    if (IsWindowFullscreen())
    {
        ToggleFullscreen();
    }
    SetWindowSize(w, h);
    int monitor = GetCurrentMonitor();
    int monW = GetMonitorWidth(monitor);
    int monH = GetMonitorHeight(monitor);
    SetWindowPosition((monW - w) / 2, (monH - h) / 2);
}

static void CenterText(const char *text, int y, int size, Color color)
{
    DrawText(text, (SCREEN_WIDTH - MeasureText(text, size)) / 2, y, size, color);
}

static void DrawButton(Rectangle bounds, const char *label, Color accent)
{
    bool hover = CheckCollisionPointRec(GetVirtualMouse(), bounds);
    int size = 21;
    int width = MeasureText(label, size);

    DrawRectangleRounded(bounds, 0.16f, 8,
                         hover ? Fade(accent, 0.24f) : PANEL_COLOR);
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 8, 2,
                                hover ? accent : PANEL_LIGHT);
    DrawText(label, (int)(bounds.x + (bounds.width - width) / 2),
             (int)(bounds.y + (bounds.height - size) / 2), size,
             hover ? RAYWHITE : LIGHTGRAY);
}

static bool Clicked(Rectangle bounds)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetVirtualMouse(), bounds);
}

static void DrawBackground(void)
{
    ClearBackground(VOID_COLOR);
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           (Color){15, 28, 48, 255}, VOID_COLOR);
    for (int x = 30; x < SCREEN_WIDTH; x += 80)
    {
        for (int y = 35; y < SCREEN_HEIGHT; y += 80)
            DrawCircle(x, y, 1.3f, Fade(CYAN_COLOR, 0.16f));
    }
}

static void DrawTitleSpaceBackground(void)
{
    float time = (float)GetTime();
    Vector2 mouse = GetVirtualMouse();
    float mx = (mouse.x - SCREEN_WIDTH * 0.5f) / (SCREEN_WIDTH * 0.5f);
    float my = (mouse.y - SCREEN_HEIGHT * 0.5f) / (SCREEN_HEIGHT * 0.5f);

    ClearBackground((Color){3, 7, 18, 255});
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           (Color){12, 27, 54, 255},
                           (Color){3, 7, 18, 255});

    /* Nebulosas espaciais com leve paralaxe e respiracao suave */
    BeginBlendMode(BLEND_ADDITIVE);
    float nebPulse1 = 1.0f + 0.04f * sinf(time * 0.6f);
    for (int radius = 310; radius >= 70; radius -= 24)
    {
        float alpha = 0.008f + (310 - radius) * 0.00006f;
        DrawCircle((int)(760 + mx * 6.0f), (int)(280 + my * 5.0f), (float)radius * nebPulse1,
                   Fade((Color){38, 92, 150, 255}, alpha));
    }
    float nebPulse2 = 1.0f + 0.05f * cosf(time * 0.7f);
    for (int radius = 220; radius >= 55; radius -= 22)
    {
        float alpha = 0.008f + (220 - radius) * 0.00008f;
        DrawCircle((int)(1115 + mx * 9.0f), (int)(190 + my * 7.0f), (float)radius * nebPulse2,
                   Fade((Color){106, 48, 127, 255}, alpha));
    }
    EndBlendMode();

    /* 160 Estrelas em movimento contínuo e 3 camadas de profundidade (paralaxe) */
    for (int i = 0; i < 160; i++)
    {
        float speed = (i % 3 == 0) ? 18.0f : ((i % 2 == 0) ? 9.0f : 4.0f);
        float pFactor = speed / 18.0f;
        float rawX = (float)((i * 83 + i * i * 7 + 29) % SCREEN_WIDTH) - time * speed;
        float x = fmodf(fmodf(rawX, (float)SCREEN_WIDTH) + (float)SCREEN_WIDTH, (float)SCREEN_WIDTH);
        float y = (float)((i * 47 + i * i * 3 + 17) % SCREEN_HEIGHT) + my * (12.0f * pFactor);
        x += mx * (16.0f * pFactor);

        float pulse = 0.55f + 0.35f * sinf(time * (0.8f + (i % 5) * 0.15f) + (float)i);
        float radius = (i % 19 == 0) ? 2.1f : ((i % 7 == 0) ? 1.4f : 0.9f);
        Color star = (i % 11 == 0) ? (Color){141, 218, 255, 255} : ((i % 13 == 0) ? GOLD_COLOR : RAYWHITE);
        DrawCircle((int)x, (int)y, radius, Fade(star, pulse));

        if (i % 23 == 0)
        {
            DrawLine((int)(x - 5), (int)y, (int)(x + 5), (int)y, Fade(star, pulse * 0.55f));
            DrawLine((int)x, (int)(y - 5), (int)x, (int)(y + 5), Fade(star, pulse * 0.55f));
        }
    }

    /* Estrela cadente / feixe de dados cosmico periodico */
    float shootCycle = fmodf(time, 4.6f);
    if (shootCycle < 0.65f)
    {
        float t = shootCycle / 0.65f;
        float sx = 420.0f + fmodf((float)((int)(time * 110.0f) % 360), 360.0f);
        float sy = 40.0f + fmodf((float)((int)(time * 65.0f) % 180), 180.0f);
        float curX = sx + t * 440.0f;
        float curY = sy + t * 220.0f;
        float tail = 60.0f;
        DrawLineEx((Vector2){curX, curY}, (Vector2){curX - tail * 0.89f, curY - tail * 0.45f}, 2.0f,
                   Fade(CYAN_COLOR, (1.0f - t) * 0.85f));
        DrawCircle((int)curX, (int)curY, 2.2f, Fade(RAYWHITE, 1.0f - t));
    }

    /* Planeta e aneis com flutuacao orbital e paralaxe do mouse */
    float planetX = 1030.0f + cosf(time * 0.5f) * 4.0f + mx * 10.0f;
    float planetY = 370.0f + sinf(time * 0.7f) * 5.0f + my * 8.0f;

    /* Brilho atmosferico sutil */
    float atmoGlow = 0.08f + 0.04f * sinf(time * 1.6f);
    DrawCircle((int)planetX, (int)planetY, 166, Fade(CYAN_COLOR, atmoGlow));
    DrawCircle((int)planetX, (int)planetY, 158, Fade((Color){50, 130, 200, 255}, atmoGlow * 1.4f));

    /* Aneis externos */
    DrawEllipseLines((int)planetX, (int)planetY, 246, 85, Fade(RAYWHITE, 0.09f));
    DrawEllipseLines((int)planetX, (int)planetY, 222, 73, Fade(CYAN_COLOR, 0.24f + 0.08f * sinf(time * 2.0f)));

    /* Esfera do planeta */
    DrawCircleGradient((Vector2){planetX, planetY}, 154,
                       (Color){80, 147, 181, 255},
                       (Color){17, 35, 64, 255});

    /* Sombra do planeta */
    DrawCircle((int)(planetX + 57), (int)(planetY - 20), 132, Fade((Color){2, 7, 19, 255}, 0.74f));

    /* Ponto de luz especular pulsante */
    DrawCircleGradient((Vector2){planetX - 50.0f, planetY - 52.0f}, 36,
                       Fade(RAYWHITE, 0.20f + 0.06f * sinf(time * 1.8f)), BLANK);

    /* Aneis internos */
    DrawEllipseLines((int)planetX, (int)planetY, 172, 35, Fade(CYAN_COLOR, 0.16f));
    DrawEllipseLines((int)planetX, (int)(planetY + 24), 144, 23, Fade(GOLD_COLOR, 0.14f));

    /* Particulas orbitando pelos aneis */
    for (int p = 0; p < 7; p++)
    {
        float pAngle = time * 0.45f + (float)p * (2.0f * PI / 7.0f);
        float rx = 222.0f;
        float ry = 73.0f;
        float orbX = planetX + cosf(pAngle) * rx;
        float orbY = planetY + sinf(pAngle) * ry;

        bool isBehind = (sinf(pAngle) < -0.15f) && (fabsf(cosf(pAngle) * rx) < 145.0f);
        if (!isBehind)
        {
            float pPulse = 0.45f + 0.45f * sinf(time * 3.0f + (float)p);
            DrawCircle((int)orbX, (int)orbY, 1.8f, Fade(CYAN_COLOR, pPulse));
        }
    }

    /* Degradê lateral para proteger a leitura dos textos */
    DrawRectangleGradientH(0, 90, 750, 430,
                           Fade((Color){3, 7, 18, 255}, 0.96f),
                           Fade((Color){3, 7, 18, 255}, 0.0f));
    DrawRectangleGradientV(0, 600, SCREEN_WIDTH, 120,
                           BLANK, Fade(BLACK, 0.62f));
}

static void SetMessage(GameState *game, const char *message)
{
    snprintf(game->message, sizeof(game->message), "%s", message);
    game->messageTimer = 4.0f;
}

static int ClueCount(const GameState *game)
{
    return (game->foundCalendar ? 1 : 0) +
           (game->foundBooks ? 1 : 0) +
           (game->foundDrawer ? 1 : 0) +
           (game->foundBlueprint ? 1 : 0) +
           (game->foundTape ? 1 : 0);
}

static int ExaminedDocs2008Count(const GameState *game)
{
    return (game->doc2008FaceExamined ? 1 : 0) +
           (game->doc2008FlyerExamined ? 1 : 0) +
           (game->doc2008EntryExamined ? 1 : 0) +
           (game->doc2008CameraExamined ? 1 : 0);
}

static void InitRoom2Puzzle(GameState *game)
{
    game->puzzleFilesCompleted = false;
    game->draggingFileIndex = -1;
    game->puzzleCompleteTimer = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        game->fileConnections[i] = -1;
    }
    /* Ordem embaralhada das pastas no lado direito: Vermelho(1), Amarelo(0), Rosa(3), Azul(2) */
    game->rightFolderColors[0] = 1;
    game->rightFolderColors[1] = 0;
    game->rightFolderColors[2] = 3;
    game->rightFolderColors[3] = 2;
}

static void InitRoom2008(GameState *game)
{
    game->doc2008FaceExamined = false;
    game->doc2008FlyerExamined = false;
    game->doc2008EntryExamined = false;
    game->doc2008CameraExamined = false;
    game->doc2008FatherOriginExamined = false;
    game->doc2008GovContractExamined = false;
    game->room2008OptionalUnlocked = false;
    game->room2008OptionalCompleted = false;
    for (int i = 0; i < 4; i++)
    {
        game->puzzle2008Selected[i] = false;
    }
    game->puzzle2008SelectionCount = 0;
    game->puzzle2008Contested = false;
    game->puzzle2008OrderSuspended = false;
    game->puzzle2008Feedback[0] = '\0';
    game->puzzle2008FeedbackTimer = 0.0f;
    game->activeDocument2008 = -1;
    game->terminal2008ActiveTab = 0;
}

static void ResetDemo(GameState *game)
{
    game->lives = 3;
    game->foundCalendar = false;
    game->foundBooks = false;
    game->foundDrawer = false;
    game->foundBlueprint = false;
    game->foundTape = false;
    game->journalOpen = false;
    game->journalPage = 0;
    game->journalYearTab = 0;
    game->code[0] = '\0';
    game->codeLength = 0;
    game->startTime = GetTime();
    game->elapsedTime = 0.0;
    game->message[0] = '\0';
    game->messageTimer = 0.0f;

    game->room2CabinetInspected = false;
    game->room2DoorInspected = false;
    game->year2008Unlocked = false;
    game->year2048Unlocked = false;
    InitRoom2Puzzle(game);
    InitRoom2008(game);
}

static void UpdatePlayerName(GameState *game)
{
    int character = GetCharPressed();

    while (character > 0)
    {
        bool allowed = (character >= 'A' && character <= 'Z') ||
                       (character >= 'a' && character <= 'z') ||
                       (character >= '0' && character <= '9') ||
                       character == ' ' || character == '-' || character == '_';

        if (allowed && game->playerNameLength < PLAYER_NAME_MAX)
        {
            game->playerName[game->playerNameLength++] = (char)character;
            game->playerName[game->playerNameLength] = '\0';
        }
        character = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && game->playerNameLength > 0)
    {
        game->playerName[--game->playerNameLength] = '\0';
    }
}

static void DrawTitleStartButton(Rectangle bounds, const char *label, Color accent)
{
    Vector2 mouse = GetVirtualMouse();
    bool hover = CheckCollisionPointRec(mouse, bounds);
    float time = (float)GetTime();
    int size = 21;
    int width = MeasureText(label, size);

    float pulse = 0.70f + 0.30f * sinf(time * 3.0f);
    Color borderColor = hover ? RAYWHITE : Fade(accent, pulse);

    /* Brilho ao redor do botao em hover */
    if (hover)
    {
        Rectangle glowRec = {bounds.x - 4, bounds.y - 4, bounds.width + 8, bounds.height + 8};
        DrawRectangleRounded(glowRec, 0.20f, 8, Fade(accent, 0.20f));
    }

    /* Corpo do botao */
    DrawRectangleRounded(bounds, 0.16f, 8,
                         hover ? Fade(accent, 0.28f) : Fade(PANEL_COLOR, 0.92f));
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 8, hover ? 2.5f : 1.8f, borderColor);

    /* Cantoneiras tecnologicas */
    float tickLen = hover ? 14.0f : 8.0f;
    float pad = 4.0f;
    Color tickCol = hover ? GOLD_COLOR : Fade(accent, 0.85f);
    DrawLineEx((Vector2){bounds.x + pad, bounds.y + pad},
               (Vector2){bounds.x + pad + tickLen, bounds.y + pad}, 2, tickCol);
    DrawLineEx((Vector2){bounds.x + pad, bounds.y + pad},
               (Vector2){bounds.x + pad, bounds.y + pad + tickLen}, 2, tickCol);
    DrawLineEx((Vector2){bounds.x + bounds.width - pad, bounds.y + bounds.height - pad},
               (Vector2){bounds.x + bounds.width - pad - tickLen, bounds.y + bounds.height - pad}, 2, tickCol);
    DrawLineEx((Vector2){bounds.x + bounds.width - pad, bounds.y + bounds.height - pad},
               (Vector2){bounds.x + bounds.width - pad, bounds.y + bounds.height - pad - tickLen}, 2, tickCol);

    /* Feixe de luz varrendo o botao em hover */
    if (hover)
    {
        float barX = bounds.x + fmodf(time * 300.0f, bounds.width + 40.0f) - 20.0f;
        if (barX >= bounds.x && barX <= bounds.x + bounds.width - 20)
        {
            DrawRectangleGradientH((int)barX, (int)bounds.y + 2, 20, (int)bounds.height - 4,
                                   Fade(RAYWHITE, 0.0f), Fade(RAYWHITE, 0.25f));
        }
    }

    int textX = (int)(bounds.x + (bounds.width - width) / 2);
    int textY = (int)(bounds.y + (bounds.height - size) / 2);
    DrawText(label, textX, textY, size, hover ? RAYWHITE : (Color){205, 235, 245, 255});
}

static void DrawTitle(void)
{
    Rectangle startButton = {86, 436, 350, 56};
    Rectangle settingsButton = {86, 506, 350, 52};
    float time = (float)GetTime();

    DrawTitleSpaceBackground();

    /* 1. Telemetria com ponto de transmissao piscando */
    float blink = sinf(time * 4.5f);
    Color dotColor = (blink > 0.0f) ? (Color){80, 245, 190, 255} : (Color){25, 75, 55, 255};
    DrawCircle(76, 125, 4, dotColor);
    DrawText("ARQUIVO DE MEMORIA  //  TRANSMISSAO 01", 90, 116, 17,
             Fade(CYAN_COLOR, 0.82f));

    /* 2. Titulo ELIRA com aura neon e micro-glitch sci-fi */
    const char *titleText = "ELIRA";
    int titleW = MeasureText(titleText, 96);
    float titleGlow = 0.30f + 0.18f * sinf(time * 2.2f);
    DrawText(titleText, 81, 164, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawText(titleText, 83, 164, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawText(titleText, 82, 163, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawText(titleText, 82, 165, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));

    float glitchCycle = fmodf(time, 4.2f);
    bool isGlitch = (glitchCycle < 0.10f) || (glitchCycle > 2.10f && glitchCycle < 2.17f);
    if (isGlitch)
    {
        float shift = sinf(time * 50.0f) * 4.0f;
        DrawText(titleText, (int)(82 + shift), 164, 96, Fade(CYAN_COLOR, 0.70f));
        DrawText(titleText, (int)(82 - shift), 164, 96, Fade(RED_COLOR, 0.60f));
        DrawRectangle(80, 198, titleW + 16, 3, Fade(CYAN_COLOR, 0.45f));
    }
    DrawText(titleText, 82, 164, 96, RAYWHITE);

    /* 3. Linhas decorativas com pulso de luz laser em movimento */
    DrawRectangle(88, 274, 554, 3, CYAN_COLOR);
    DrawRectangle(88, 281, 218, 2, GOLD_COLOR);

    float scanX = fmodf(time * 320.0f, 750.0f);
    if (scanX < 554.0f)
    {
        DrawRectangleGradientH((int)(88 + scanX - 35), 273, 35, 5, BLANK, RAYWHITE);
        DrawRectangleGradientH((int)(88 + scanX), 273, 35, 5, RAYWHITE, BLANK);
    }

    /* 4. Subtitulos */
    DrawText("ESCAPE RUN TEMPORAL", 90, 307, 29, CYAN_COLOR);
    DrawText("ENTRE NAS MEMORIAS DA MAQUINA", 91, 355, 19,
             Fade(RAYWHITE, 0.75f));
    DrawText("Descubra o que existia antes de A.R.1.3.L. controlar o futuro.",
             91, 386, 18, GRAY);

    /* 5. Botoes interativos */
    DrawTitleStartButton(startButton, "INICIAR MISSAO", CYAN_COLOR);
    DrawTitleStartButton(settingsButton, "CONFIGURACOES [C]", GOLD_COLOR);

    /* 6. Indicador de atalhos pulsante */
    float promptAlpha = 0.40f + 0.55f * (0.5f + 0.5f * sinf(time * 3.4f));
    DrawText("> ENTER: Iniciar  |  C: Opcoes <", 93, 574, 15, Fade(GOLD_COLOR, promptAlpha));

    /* 7. Rodape */
    DrawText("DEMO TEMPORAL  //  1990 - 2048", 91, 638, 16,
             Fade(LIGHTGRAY, 0.62f));
    DrawText("Point & click narrativo", 946, 650, 16,
             Fade(LIGHTGRAY, 0.50f));

    /* 8. Linhas CRT retro */
    for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(BLACK, 0.035f));
    }
}

static void DrawSettings(const GameState *game)
{
    DrawTitleSpaceBackground();

    /* Painel Central de Configuracoes */
    Rectangle panel = {200, 75, 880, 570};
    DrawRectangleRounded(panel, 0.04f, 10, Fade(PANEL_COLOR, 0.95f));
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 2.0f, CYAN_COLOR);

    /* Cantoneiras decorativas no painel */
    float pad = 8.0f; float tLen = 22.0f;
    DrawLineEx((Vector2){panel.x + pad, panel.y + pad}, (Vector2){panel.x + pad + tLen, panel.y + pad}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + pad, panel.y + pad}, (Vector2){panel.x + pad, panel.y + pad + tLen}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + panel.width - pad, panel.y + pad}, (Vector2){panel.x + panel.width - pad - tLen, panel.y + pad}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + panel.width - pad, panel.y + pad}, (Vector2){panel.x + panel.width - pad, panel.y + pad + tLen}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + pad, panel.y + panel.height - pad}, (Vector2){panel.x + pad + tLen, panel.y + panel.height - pad}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + pad, panel.y + panel.height - pad}, (Vector2){panel.x + pad, panel.y + panel.height - pad - tLen}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + panel.width - pad, panel.y + panel.height - pad}, (Vector2){panel.x + panel.width - pad - tLen, panel.y + panel.height - pad}, 2.5f, GOLD_COLOR);
    DrawLineEx((Vector2){panel.x + panel.width - pad, panel.y + panel.height - pad}, (Vector2){panel.x + panel.width - pad, panel.y + panel.height - pad - tLen}, 2.5f, GOLD_COLOR);

    /* Cabecalho */
    DrawText("CONFIGURACOES DE VIDEO & SISTEMA", 240, 100, 26, RAYWHITE);
    DrawText("Ajuste a resolucao, tela cheia e parametros da experiencia", 240, 134, 14, GRAY);
    DrawLine(240, 156, 1040, 156, Fade(CYAN_COLOR, 0.40f));

    /* --- SEÇÃO 1: MODO DE TELA (FULLSCREEN) --- */
    DrawText("1. MODO DE EXIBICAO", 240, 172, 17, CYAN_COLOR);
    DrawText("Alterne entre o modo Janela e Tela Cheia (Fullscreen):", 240, 194, 13, LIGHTGRAY);

    bool isFull = IsWindowFullscreen();
    Rectangle btnWindowed = {240, 218, 250, 44};
    Rectangle btnFullscreen = {510, 218, 280, 44};

    bool winHover = CheckCollisionPointRec(GetVirtualMouse(), btnWindowed);
    DrawRectangleRounded(btnWindowed, 0.20f, 6, !isFull ? Fade(CYAN_COLOR, 0.25f) : (winHover ? Fade(PANEL_LIGHT, 0.6f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(btnWindowed, 0.20f, 6, 1.8f, !isFull ? CYAN_COLOR : (winHover ? RAYWHITE : PANEL_LIGHT));
    DrawText(!isFull ? "[X] MODO JANELA" : "    MODO JANELA", 280, 231, 16, !isFull ? RAYWHITE : GRAY);

    bool fullHover = CheckCollisionPointRec(GetVirtualMouse(), btnFullscreen);
    DrawRectangleRounded(btnFullscreen, 0.20f, 6, isFull ? Fade(CYAN_COLOR, 0.25f) : (fullHover ? Fade(PANEL_LIGHT, 0.6f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(btnFullscreen, 0.20f, 6, 1.8f, isFull ? CYAN_COLOR : (fullHover ? RAYWHITE : PANEL_LIGHT));
    DrawText(isFull ? "[X] TELA CHEIA [F11]" : "    TELA CHEIA [F11]", 545, 231, 16, isFull ? RAYWHITE : GRAY);

    /* --- SEÇÃO 2: RESOLUCAO DA JANELA --- */
    DrawText("2. RESOLUCAO DE TELA (PROPORCAO 16:9)", 240, 282, 17, CYAN_COLOR);
    DrawText("Selecione a resolucao da janela (redimensionamento automatico):", 240, 304, 13, LIGHTGRAY);

    const char *resLabels[3] = {
        "1280 x 720 (HD)",
        "1600 x 900 (HD+)",
        "1920 x 1080 (FHD)"
    };
    for (int r = 0; r < 3; r++)
    {
        Rectangle resBtn = {(float)(240 + r * 266), 326, 246, 48};
        bool isCurrent = (g_currentRes == r);
        bool hover = CheckCollisionPointRec(GetVirtualMouse(), resBtn);

        DrawRectangleRounded(resBtn, 0.20f, 6,
                             isCurrent ? Fade(GOLD_COLOR, 0.24f)
                                       : (hover ? Fade(CYAN_COLOR, 0.20f) : PANEL_COLOR));
        DrawRectangleRoundedLinesEx(resBtn, 0.20f, 6, 1.8f,
                                    isCurrent ? GOLD_COLOR : (hover ? CYAN_COLOR : PANEL_LIGHT));

        const char *prefix = isCurrent ? "[X] " : "";
        int txtWidth = MeasureText(TextFormat("%s%s", prefix, resLabels[r]), 15);
        DrawText(TextFormat("%s%s", prefix, resLabels[r]),
                 (int)(resBtn.x + (resBtn.width - txtWidth) / 2),
                 (int)(resBtn.y + 16), 15,
                 isCurrent ? GOLD_COLOR : (hover ? RAYWHITE : LIGHTGRAY));
    }

    if (isFull)
    {
        DrawText("* Em Tela Cheia, o jogo preenche todo o monitor mantendo a proporcao correta (16:9).",
                 240, 384, 12, Fade(CYAN_COLOR, 0.70f));
    }
    else
    {
        DrawText("* Clique em qualquer resolucao para redimensionar e centralizar a janela na tela.",
                 240, 384, 12, Fade(LIGHTGRAY, 0.70f));
    }

    /* --- SEÇÃO 3: AUDIO AMBIENTE --- */
    DrawText("3. TRILHA SONORA AMBIENTE", 240, 412, 17, CYAN_COLOR);
    DrawText("Controle da musica ambiente retro sci-fi (atalho [M] a qualquer momento):", 240, 434, 13, LIGHTGRAY);

    Rectangle muteBtn = {240, 456, 230, 42};
    bool muteHover = CheckCollisionPointRec(GetVirtualMouse(), muteBtn);
    DrawRectangleRounded(muteBtn, 0.20f, 6,
                         game->musicMuted ? Fade(RED_COLOR, 0.22f) : (muteHover ? Fade(CYAN_COLOR, 0.20f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(muteBtn, 0.20f, 6, 1.8f,
                                game->musicMuted ? RED_COLOR : (muteHover ? CYAN_COLOR : PANEL_LIGHT));
    DrawText(game->musicMuted ? "MUSICA: MUDO [M]" : "MUSICA: ATIVA [M]",
             262, 468, 15, game->musicMuted ? RED_COLOR : (muteHover ? RAYWHITE : CYAN_COLOR));

    /* Botoes de Volume */
    const float volValues[4] = {0.20f, 0.45f, 0.75f, 1.00f};
    const char *volLabels[4] = {"25%", "50%", "75%", "100%"};
    for (int v = 0; v < 4; v++)
    {
        Rectangle volBtn = {(float)(490 + v * 76), 456, 66, 42};
        bool isCurVol = (!game->musicMuted && fabsf(g_musicVolume - volValues[v]) < 0.05f);
        bool vHover = CheckCollisionPointRec(GetVirtualMouse(), volBtn);

        DrawRectangleRounded(volBtn, 0.20f, 6,
                             isCurVol ? Fade(GOLD_COLOR, 0.22f) : (vHover ? Fade(CYAN_COLOR, 0.20f) : PANEL_COLOR));
        DrawRectangleRoundedLinesEx(volBtn, 0.20f, 6, 1.8f,
                                    isCurVol ? GOLD_COLOR : (vHover ? CYAN_COLOR : PANEL_LIGHT));
        DrawText(volLabels[v], (int)(volBtn.x + 16), (int)(volBtn.y + 13), 15,
                 isCurVol ? GOLD_COLOR : (vHover ? RAYWHITE : LIGHTGRAY));
    }

    /* --- BOTAO VOLTAR --- */
    Rectangle backBtn = {490, 560, 300, 54};
    DrawButton(backBtn, "< VOLTAR AO MENU [ESC]", CYAN_COLOR);

    /* Linhas CRT retro */
    for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(BLACK, 0.035f));
    }
}

static void UpdateSettings(GameState *game, Music *bgm, bool bgmLoaded)
{
    Rectangle btnWindowed = {240, 218, 250, 44};
    Rectangle btnFullscreen = {510, 218, 280, 44};
    Rectangle backBtn = {490, 560, 300, 54};

    if (Clicked(backBtn) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_TITLE;
        return;
    }

    /* Alternar Fullscreen */
    if (Clicked(btnFullscreen) || IsKeyPressed(KEY_F11) ||
        ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)))
    {
        if (!IsWindowFullscreen())
        {
            ToggleFullscreen();
        }
    }
    else if (Clicked(btnWindowed))
    {
        if (IsWindowFullscreen())
        {
            ToggleFullscreen();
            ApplyResolution(g_currentRes);
        }
    }

    /* Resolucao */
    for (int r = 0; r < 3; r++)
    {
        Rectangle resBtn = {(float)(240 + r * 266), 326, 246, 48};
        if (Clicked(resBtn))
        {
            ApplyResolution(r);
            break;
        }
    }

    /* Audio Mudo */
    Rectangle muteBtn = {240, 456, 230, 42};
    if (Clicked(muteBtn))
    {
        game->musicMuted = !game->musicMuted;
        if (bgmLoaded)
        {
            SetMusicVolume(*bgm, game->musicMuted ? 0.0f : g_musicVolume);
        }
    }

    /* Volume */
    const float volValues[4] = {0.20f, 0.45f, 0.75f, 1.00f};
    for (int v = 0; v < 4; v++)
    {
        Rectangle volBtn = {(float)(490 + v * 76), 456, 66, 42};
        if (Clicked(volBtn))
        {
            g_musicVolume = volValues[v];
            game->musicMuted = false;
            if (bgmLoaded)
            {
                SetMusicVolume(*bgm, g_musicVolume);
            }
            break;
        }
    }
}

static void DrawPlayerName(const GameState *game)
{
    Rectangle field = {360, 330, 560, 64};
    bool focused = CheckCollisionPointRec(GetVirtualMouse(), field);

    DrawBackground();
    CenterText("IDENTIFICACAO DO OPERADOR", 135, 34, CYAN_COLOR);
    CenterText("Antes de acompanhar Elira, informe como voce quer ser identificado.",
               202, 21, LIGHTGRAY);
    CenterText("Seu nome sera usado apenas como perfil na interface.", 238, 18, GRAY);

    DrawRectangleRounded(field, 0.12f, 8, PANEL_COLOR);
    DrawRectangleRoundedLinesEx(field, 0.12f, 8, 2,
                                focused ? CYAN_COLOR : PANEL_LIGHT);
    DrawText(game->playerNameLength > 0 ? game->playerName : "Digite seu nome...",
             388, 349, 25, game->playerNameLength > 0 ? RAYWHITE : GRAY);
    DrawText(TextFormat("%d/%d", game->playerNameLength, PLAYER_NAME_MAX),
             842, 412, 16, GRAY);

    DrawButton((Rectangle){490, 470, 300, 58}, "CONFIRMAR PERFIL", GOLD_COLOR);
    if (game->messageTimer > 0)
        CenterText(game->message, 425, 18, RED_COLOR);
    CenterText("Pressione ENTER para continuar", 560, 16, DARKGRAY);
}

static void DrawIntro(const GameState *game)
{
    DrawBackground();
    DrawText("ARQUIVO TEMPORAL // 1990", 90, 70, 20, CYAN_COLOR);
    DrawText("SETOR DE ARQUIVOS", 90, 110, 48, RAYWHITE);
    DrawRectangle(90, 178, 1100, 2, Fade(CYAN_COLOR, 0.45f));
    DrawText("Em um futuro onde a humanidade entregou sua autonomia", 90, 230, 24,
             LIGHTGRAY);
    DrawText("a A.R.1.3.L, Elira invade a nave-arquivo da IA.", 90, 270, 24,
             LIGHTGRAY);
    DrawText("Sua chance e viajar pela memoria da maquina e reconstruir", 90, 310,
             24, LIGHTGRAY);
    DrawText("a origem da entidade que controla o mundo.", 90, 350, 24,
             LIGHTGRAY);
    DrawText(TextFormat("OPERADOR: %s  |  PROTAGONISTA: ELIRA", game->playerName),
             90, 397, 18, GOLD_COLOR);

    DrawRectangleRounded((Rectangle){850, 215, 340, 230}, 0.08f, 8,
                         Fade(PANEL_LIGHT, 0.85f));
    DrawText("OBJETIVOS // 1990", 880, 245, 19, GOLD_COLOR);
    DrawText("1. Inspecione os ficharios", 880, 295, 19, RAYWHITE);
    DrawText("2. Acesse o terminal", 880, 335, 19, RAYWHITE);
    DrawText("3. Sincronize os arquivos", 880, 375, 19, RAYWHITE);
    DrawText("4. Destranque a saida", 880, 415, 19, RAYWHITE);
    DrawButton((Rectangle){490, 575, 300, 58}, "ENTRAR EM 1990", GOLD_COLOR);
    CenterText("ENTER para continuar", 650, 15, DARKGRAY);
}

static void DrawHotspot(Rectangle bounds, const char *label)
{
    if (!CheckCollisionPointRec(GetVirtualMouse(), bounds)) return;
    int width = MeasureText(label, 17) + 20;
    int x = (int)(bounds.x + (bounds.width - width) / 2);
    int y = (int)(bounds.y - 32);
    DrawRectangleRoundedLinesEx(bounds, 0.08f, 8, 2, CYAN_COLOR);
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)width, 26},
                         0.25f, 6, PANEL_COLOR);
    DrawText(label, x + 10, y + 4, 17, CYAN_COLOR);
}

static void DrawRoomWindow(void)
{
    float time = (float)GetTime();
    Rectangle outer = {270, 120, 200, 145};
    Rectangle inner = {282, 132, 176, 121};

    /* Moldura externa da janela da nave */
    DrawRectangleRounded(outer, 0.08f, 8, (Color){18, 25, 34, 255});
    DrawRectangleRoundedLinesEx(outer, 0.08f, 8, 2, (Color){38, 48, 60, 255});

    /* Base do espaco dentro da janela */
    DrawRectangleRounded(inner, 0.06f, 8, (Color){5, 12, 27, 255});

    /* Scissor mode: garante que estrelas e planetas fiquem perfeitamente dentro do vidro */
    BeginScissorMode((int)inner.x, (int)inner.y, (int)inner.width, (int)inner.height);

    /* Gradiente sutil do cosmos */
    DrawRectangleGradientV((int)inner.x, (int)inner.y, (int)inner.width, (int)inner.height,
                           (Color){8, 16, 36, 255}, (Color){3, 7, 18, 255});

    /* Nebulosa suave se deslocando lentamente ao fundo */
    float nebX = 330.0f + sinf(time * 0.3f) * 10.0f;
    float nebY = 175.0f + cosf(time * 0.4f) * 6.0f;
    DrawCircleGradient((Vector2){nebX, nebY}, 54,
                       Fade((Color){28, 62, 115, 255}, 0.28f), BLANK);
    DrawCircleGradient((Vector2){nebX + 42.0f, nebY + 18.0f}, 42,
                       Fade((Color){75, 35, 95, 255}, 0.20f), BLANK);

    /* Estrelas em movimento contínuo da direita para a esquerda (efeito de voo da nave) */
    for (int i = 0; i < 32; i++)
    {
        float speed = (i % 3 == 0) ? 14.0f : ((i % 2 == 0) ? 7.0f : 3.0f);
        float rawX = (float)((i * 57 + 19) % (int)inner.width) - time * speed;
        float sx = inner.x + fmodf(fmodf(rawX, inner.width) + inner.width, inner.width);
        float sy = inner.y + (float)((i * 39 + 13) % (int)inner.height);

        float pulse = 0.55f + 0.40f * sinf(time * (1.2f + (i % 5) * 0.2f) + (float)i);
        float r = (i % 5 == 0) ? 1.7f : 1.0f;
        Color starColor = (i % 6 == 0) ? (Color){140, 220, 255, 255} : ((i % 7 == 0) ? GOLD_COLOR : RAYWHITE);
        DrawCircle((int)sx, (int)sy, r, Fade(starColor, pulse));

        if (i % 8 == 0)
        {
            DrawLine((int)(sx - 3), (int)sy, (int)(sx + 3), (int)sy, Fade(starColor, pulse * 0.45f));
            DrawLine((int)sx, (int)(sy - 3), (int)sx, (int)(sy + 3), Fade(starColor, pulse * 0.45f));
        }
    }

    /* Estrela cadente periodica passando pela janela */
    float shootCycle = fmodf(time, 5.2f);
    if (shootCycle < 0.45f)
    {
        float t = shootCycle / 0.45f;
        float startX = inner.x + inner.width + 10.0f;
        float startY = inner.y + 15.0f;
        float curX = startX - t * (inner.width + 40.0f);
        float curY = startY + t * 75.0f;
        float tail = 26.0f;
        DrawLineEx((Vector2){curX, curY}, (Vector2){curX + tail * 0.88f, curY - tail * 0.47f},
                   1.6f, Fade(CYAN_COLOR, (1.0f - t) * 0.85f));
        DrawCircle((int)curX, (int)curY, 1.8f, Fade(RAYWHITE, 1.0f - t));
    }

    /* Planeta com flutuacao orbital suave e atmosfera viva */
    float pwX = 442.0f + sinf(time * 0.5f) * 2.5f;
    float pwY = 230.0f + cosf(time * 0.6f) * 2.0f;

    /* Halo atmosferico sutil */
    float atmoPulse = 0.08f + 0.04f * sinf(time * 1.6f);
    DrawCircle((int)pwX, (int)pwY, 34, Fade(CYAN_COLOR, atmoPulse));
    DrawCircle((int)pwX, (int)pwY, 30, Fade((Color){60, 130, 190, 255}, atmoPulse * 1.4f));

    /* Aneis orbitais do planeta */
    DrawEllipseLines((int)pwX, (int)pwY, 44, 13, Fade(CYAN_COLOR, 0.25f + 0.08f * sinf(time * 2.0f)));
    DrawEllipseLines((int)pwX, (int)pwY, 48, 15, Fade(RAYWHITE, 0.10f));

    /* Esfera planetaria */
    DrawCircleGradient((Vector2){pwX, pwY}, 28,
                       (Color){61, 121, 173, 255},
                       (Color){12, 27, 47, 255});

    /* Sombra de noite do planeta */
    DrawCircle((int)(pwX + 10), (int)(pwY - 4), 24, Fade((Color){2, 7, 19, 255}, 0.74f));

    /* Ponto de reflexo especular pulsante */
    DrawCircleGradient((Vector2){pwX - 9.0f, pwY - 8.0f}, 8,
                       Fade(RAYWHITE, 0.22f + 0.08f * sinf(time * 1.8f)), BLANK);

    /* Arco do anel em primeiro plano */
    DrawEllipseLines((int)pwX, (int)pwY, 32, 7, Fade(CYAN_COLOR, 0.20f));

    /* Reflexo sutil de vidro na diagonal */
    DrawLineEx((Vector2){292, 134}, (Vector2){345, 250}, 8.0f, Fade(RAYWHITE, 0.025f));
    DrawLineEx((Vector2){308, 134}, (Vector2){361, 250}, 4.0f, Fade(RAYWHITE, 0.018f));

    EndScissorMode();

    /* Esquadrias metalicas divisórias da janela */
    DrawLineEx((Vector2){370, 132}, (Vector2){370, 253}, 3, (Color){48, 61, 70, 255});
    DrawLineEx((Vector2){282, 193}, (Vector2){458, 193}, 3, (Color){48, 61, 70, 255});

    /* Juncao central das esquadrias */
    DrawCircle(370, 193, 3, (Color){68, 82, 92, 255});
    DrawCircle(370, 193, 1, (Color){100, 120, 135, 255});
}

static void DrawRoom(const GameState *game)
{
    Rectangle calendar = {75, 115, 160, 185};
    Rectangle books = {1000, 135, 195, 475};
    Rectangle drawer = {330, 524, 180, 44};
    Rectangle computer = {495, 220, 310, 260};
    Rectangle blueprint = {815, 132, 160, 125};
    Rectangle tape = {820, 396, 125, 75};

    ClearBackground((Color){20, 27, 34, 255});

    /* Teto em perspectiva. */
    DrawTriangle((Vector2){0, 70}, (Vector2){1090, 126}, (Vector2){1280, 70},
                 (Color){28, 39, 48, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){190, 126}, (Vector2){1090, 126},
                 (Color){34, 47, 56, 255});
    DrawLineEx((Vector2){0, 70}, (Vector2){190, 126}, 4,
               (Color){91, 112, 119, 255});
    DrawLineEx((Vector2){1280, 70}, (Vector2){1090, 126}, 4,
               (Color){91, 112, 119, 255});
    DrawLineEx((Vector2){190, 126}, (Vector2){1090, 126}, 4,
               (Color){70, 92, 100, 255});

    /* Parede central e paredes laterais convergem para o fundo. */
    DrawTriangle((Vector2){190, 126}, (Vector2){1010, 515}, (Vector2){1090, 126},
                 (Color){50, 64, 70, 255});
    DrawTriangle((Vector2){190, 126}, (Vector2){270, 515}, (Vector2){1010, 515},
                 (Color){55, 69, 75, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){270, 515}, (Vector2){190, 126},
                 (Color){42, 56, 63, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){0, 535}, (Vector2){270, 515},
                 (Color){37, 49, 57, 255});
    DrawTriangle((Vector2){1280, 70}, (Vector2){1090, 126}, (Vector2){1010, 515},
                 (Color){42, 56, 63, 255});
    DrawTriangle((Vector2){1280, 70}, (Vector2){1010, 515}, (Vector2){1280, 535},
                 (Color){37, 49, 57, 255});

    /* Placas da parede do fundo reforcam escala e profundidade. */
    DrawLine(205, 205, 1073, 205, Fade(BLACK, 0.20f));
    DrawLine(222, 292, 1055, 292, Fade(BLACK, 0.20f));
    DrawLine(240, 382, 1037, 382, Fade(BLACK, 0.20f));
    DrawLine(258, 470, 1019, 470, Fade(BLACK, 0.20f));
    DrawLine(350, 126, 390, 515, Fade(BLACK, 0.18f));
    DrawLine(510, 126, 520, 515, Fade(BLACK, 0.18f));
    DrawLine(670, 126, 650, 515, Fade(BLACK, 0.18f));
    DrawLine(830, 126, 780, 515, Fade(BLACK, 0.18f));
    DrawLine(990, 126, 910, 515, Fade(BLACK, 0.18f));

    /* Piso com linhas que apontam para o centro da sala. */
    DrawTriangle((Vector2){0, 650}, (Vector2){1010, 515}, (Vector2){270, 515},
                 (Color){48, 39, 37, 255});
    DrawTriangle((Vector2){0, 650}, (Vector2){1280, 650}, (Vector2){1010, 515},
                 (Color){39, 33, 34, 255});
    for (int x = 0; x <= SCREEN_WIDTH; x += 160)
        DrawLine(640, 510, x, 650, Fade((Color){100, 76, 65, 255}, 0.28f));
    DrawLine(0, 558, 1280, 558, Fade((Color){118, 87, 71, 255}, 0.28f));
    DrawLine(0, 607, 1280, 607, Fade((Color){118, 87, 71, 255}, 0.22f));

    /* Luminaria central e cone de luz discreto. */
    DrawTriangle((Vector2){545, 108}, (Vector2){850, 505}, (Vector2){735, 108},
                 Fade(CYAN_COLOR, 0.035f));
    DrawTriangle((Vector2){545, 108}, (Vector2){430, 505}, (Vector2){850, 505},
                 Fade(CYAN_COLOR, 0.035f));
    DrawRectangleRounded((Rectangle){548, 78, 184, 30}, 0.18f, 8,
                         (Color){93, 108, 109, 255});
    DrawRectangleRounded((Rectangle){560, 87, 160, 14}, 0.18f, 8,
                         (Color){136, 214, 207, 255});
    DrawCircle(560, 94, 3, RAYWHITE);
    DrawCircle(720, 94, 3, RAYWHITE);

    /* Estruturas verticais nas quinas da nave. */
    DrawLineEx((Vector2){190, 126}, (Vector2){270, 515}, 7,
               (Color){76, 91, 97, 255});
    DrawLineEx((Vector2){1090, 126}, (Vector2){1010, 515}, 7,
               (Color){76, 91, 97, 255});
    DrawLineEx((Vector2){270, 515}, (Vector2){1010, 515}, 8,
               (Color){31, 39, 44, 255});

    /* Janela para o espaco, animada com o cosmos em movimento continuo */
    DrawRoomWindow();

    /* Canos, cabos e sinalizacao de seguranca. */
    DrawRectangle(16, 118, 18, 405, (Color){70, 78, 78, 255});
    DrawRectangle(34, 118, 34, 14, (Color){87, 94, 91, 255});
    DrawRectangle(17, 245, 16, 32, (Color){118, 64, 52, 255});
    for (int x = 252; x < 995; x += 42)
    {
        Color stripe = (x / 42) % 2 == 0 ? GOLD_COLOR : (Color){33, 34, 35, 255};
        DrawRectangle(x, 510, 42, 10, stripe);
    }

    /* Painel tecnico: uma das novas pistas. */
    DrawRectangleRec(blueprint, (Color){28, 65, 83, 255});
    DrawRectangleLinesEx(blueprint, 3, (Color){114, 184, 194, 255});
    DrawText("PERCEPTRON", 832, 144, 16, (Color){169, 225, 228, 255});
    DrawCircle(850, 202, 7, (Color){169, 225, 228, 255});
    DrawCircle(930, 180, 7, (Color){169, 225, 228, 255});
    DrawCircle(930, 222, 7, (Color){169, 225, 228, 255});
    DrawLine(857, 202, 923, 180, (Color){169, 225, 228, 255});
    DrawLine(857, 202, 923, 222, (Color){169, 225, 228, 255});
    DrawText(game->foundBlueprint ? "1958" : "DATA?", 830, 232, 14,
             game->foundBlueprint ? GOLD_COLOR : GRAY);
    DrawEllipse(42, 325, 55, 165, Fade(CYAN_COLOR, 0.10f));
    DrawEllipseLines(42, 325, 55, 165, Fade(CYAN_COLOR, 0.45f));
    DrawEllipseLines(42, 325, 42, 145, Fade(GOLD_COLOR, 0.40f));

    /* --- CALENDÁRIO RETO (ALINHADO, NITIDO E EM HARMONIA COM A PAREDE) --- */
    /* Sombra suave projetada na parede */
    DrawRectangle(79, 119, 160, 185, Fade(BLACK, 0.40f));

    /* Folha base do calendário (papel marfim retro) */
    Rectangle calRect = {75, 115, 160, 185};
    DrawRectangleRec(calRect, (Color){236, 230, 212, 255});
    DrawRectangleLinesEx(calRect, 1, (Color){185, 175, 150, 255});
    /* Efeito de espessura de papel nas bordas direita e inferior */
    DrawLine(76, 299, 234, 299, (Color){175, 165, 140, 255});
    DrawLine(234, 116, 234, 299, (Color){175, 165, 140, 255});

    /* Cabeçalho vinho reto */
    Rectangle headRect = {75, 115, 160, 42};
    DrawRectangleRec(headRect, (Color){152, 42, 46, 255});
    DrawLine(75, 115, 235, 115, (Color){185, 65, 70, 255});
    DrawLine(75, 156, 235, 156, (Color){112, 28, 32, 255});

    /* Argolas metálicas do espiral superior alinhadas retas */
    for (int ring = 0; ring < 7; ring++)
    {
        int rx = 87 + ring * 21;
        /* Furo no papel */
        DrawRectangle(rx - 1, 119, 3, 3, (Color){45, 15, 18, 255});
        /* Elo metálico */
        DrawRectangle(rx - 1, 111, 3, 9, (Color){180, 185, 190, 255});
        DrawRectangle(rx, 111, 1, 9, RAYWHITE);
    }

    /* Textos do cabeçalho */
    DrawText("OUTUBRO", 84, 131, 16, RAYWHITE);
    DrawText("2048", 188, 131, 16, GOLD_COLOR);

    /* Faixa sutil dos dias da semana */
    Rectangle daysBar = {75, 157, 160, 16};
    DrawRectangleRec(daysBar, (Color){224, 216, 196, 255});
    DrawLine(75, 172, 235, 172, (Color){200, 190, 170, 255});

    const char *weekdays[7] = { "D", "S", "T", "Q", "Q", "S", "S" };
    for (int c = 0; c < 7; c++)
    {
        int cx = 86 + c * 21;
        DrawText(weekdays[c], cx, 160, 11, (c == 0) ? (Color){185, 45, 45, 255} : (Color){100, 85, 75, 255});
    }

    /* Grade de dias do mês (1 a 31) */
    for (int day = 1; day <= 31; day++)
    {
        int col = (day + 0) % 7;
        int row = (day + 0) / 7;
        int dx = 84 + col * 21;
        int dy = 176 + row * 13;
        if (day == 24)
        {
            DrawCircle(dx + 6, dy + 5, 8, Fade(RED_COLOR, 0.35f));
            DrawCircleLines(dx + 6, dy + 5, 8, RED_COLOR);
            DrawText(TextFormat("%d", day), dx, dy, 10, RED_COLOR);
        }
        else
        {
            Color dayColor = (col == 0) ? (Color){175, 55, 55, 255} : (Color){60, 55, 50, 255};
            DrawText(TextFormat("%2d", day), dx, dy, 10, dayColor);
        }
    }

    /* Pista manuscrita sobre Alan Turing / 1950 */
    if (game->foundCalendar)
    {
        Rectangle postIt = {82, 248, 146, 42};
        DrawRectangle(84, 250, 146, 42, Fade(BLACK, 0.22f));
        DrawRectangleRec(postIt, (Color){254, 244, 138, 255});
        DrawRectangle(82, 248, 146, 6, (Color){240, 225, 100, 255});
        DrawRectangleLinesEx(postIt, 1, (Color){218, 198, 80, 255});
        DrawText("1950 - TURING TEST", 88, 256, 11, (Color){160, 40, 40, 255});
        DrawText("Computing Machinery", 88, 271, 9, (Color){70, 55, 45, 255});
    }
    else
    {
        Rectangle tagBox = {88, 254, 134, 32};
        DrawRectangleRounded(tagBox, 0.25f, 4, Fade((Color){180, 170, 150, 255}, 0.25f));
        DrawRectangleRoundedLinesEx(tagBox, 0.25f, 4, 1, (Color){190, 180, 160, 255});
        DrawText("[ ? ]", 142, 263, 14, (Color){120, 105, 95, 255});
    }

    /* --- ARMÁRIO / ESTANTE DE LIVROS (APOIADO FIRMEMENTE NO PISO COM PERSPECTIVA) --- */
    /* Sombra projetada da estante no piso angular */
    Vector2 bSh1 = {995, 584};
    Vector2 bSh2 = {1195, 584};
    Vector2 bSh3 = {1225, 618};
    Vector2 bSh4 = {965, 618};
    DrawTriangle(bSh1, bSh4, bSh2, Fade(BLACK, 0.45f));
    DrawTriangle(bSh2, bSh4, bSh3, Fade(BLACK, 0.45f));

    /* Painel lateral esquerdo em perspectiva continua do topo ate o piso (sem buracos) */
    Vector2 sideTL = {1000, 142};
    Vector2 sideBL = {1000, 592};
    Vector2 sideBR = {1026, 574};
    Vector2 sideTR = {1026, 134};
    DrawTriangle(sideTL, sideBL, sideTR, (Color){58, 38, 28, 255});
    DrawTriangle(sideTR, sideBL, sideBR, (Color){50, 32, 24, 255});
    DrawLineEx(sideTL, sideBL, 2.5f, (Color){78, 52, 38, 255});

    /* Fundo interno da estante */
    DrawRectangle(1026, 134, 164, 444, (Color){45, 29, 22, 255});

    /* Moldura externa / topo e rodapé da estante apoiada no chão */
    DrawRectangle(996, 130, 196, 14, (Color){72, 48, 35, 255});
    DrawRectangle(1000, 574, 192, 18, (Color){58, 38, 28, 255});
    DrawRectangle(998, 588, 194, 4, (Color){38, 24, 18, 255});
    DrawRectangle(1184, 134, 8, 444, (Color){65, 43, 32, 255});

    /* 5 Prateleiras de madeira maciça distribuídas ao longo da altura */
    int shelfYs[5] = {218, 298, 378, 458, 538};
    for (int s = 0; s < 5; s++)
    {
        int sy = shelfYs[s];
        DrawRectangle(1028, sy + 8, 156, 6, Fade(BLACK, 0.40f));
        DrawRectangle(1026, sy, 160, 8, (Color){78, 52, 38, 255});
        DrawLine(1026, sy, 1186, sy, (Color){105, 72, 54, 255});

        if (s == 0)
        {
            for (int b = 0; b < 6; b++)
            {
                Color bCol = (b % 2 == 0) ? (Color){45, 78, 92, 255} : (Color){115, 55, 45, 255};
                DrawRectangle(1034 + b * 24, sy - 64, 20, 64, bCol);
                DrawRectangle(1036 + b * 24, sy - 60, 16, 4, GOLD_COLOR);
            }
        }
        else if (s == 1)
        {
            DrawRectangle(1034, sy - 58, 22, 58, (Color){98, 60, 48, 255});
            DrawRectangle(1058, sy - 54, 18, 54, (Color){42, 68, 85, 255});
            Color aiBookCol = game->foundBooks ? GOLD_COLOR : (Color){135, 45, 48, 255};
            DrawRectangle(1078, sy - 62, 26, 62, aiBookCol);
            DrawText("1956", 1081, sy - 44, 10, RAYWHITE);
            DrawText("IA", 1085, sy - 30, 11, RAYWHITE);
            DrawTriangle((Vector2){1108, sy - 52}, (Vector2){1106, sy}, (Vector2){1122, sy}, (Color){60, 85, 95, 255});
            DrawTriangle((Vector2){1108, sy - 52}, (Vector2){1122, sy}, (Vector2){1124, sy - 50}, (Color){70, 95, 105, 255});
            DrawRectangle(1128, sy - 28, 6, 28, (Color){35, 38, 42, 255});
        }
        else if (s == 2)
        {
            for (int b = 0; b < 5; b++)
            {
                Color binderCol = (b == 2) ? (Color){130, 118, 92, 255} : (Color){50, 72, 80, 255};
                DrawRectangle(1034 + b * 28, sy - 66, 24, 66, binderCol);
                DrawCircle(1046 + b * 28, sy - 45, 3, RAYWHITE);
                DrawRectangle(1038 + b * 28, sy - 25, 16, 12, (Color){220, 215, 195, 255});
            }
        }
        else if (s == 3)
        {
            DrawRectangle(1034, sy - 42, 45, 42, (Color){160, 152, 130, 255});
            DrawRectangle(1038, sy - 38, 37, 8, (Color){45, 48, 52, 255});
            DrawText("DISK", 1044, sy - 22, 10, INK_COLOR);
            for (int b = 0; b < 4; b++)
            {
                DrawRectangle(1085 + b * 22, sy - 55, 18, 55, (Color){85, 52, 42, 255});
            }
        }
        else if (s == 4)
        {
            for (int b = 0; b < 6; b++)
            {
                Color bCol = (b % 3 == 0) ? (Color){38, 55, 65, 255} : (Color){88, 50, 38, 255};
                DrawRectangle(1034 + b * 23, sy - 68, 20, 68, bCol);
                DrawRectangle(1036 + b * 23, sy - 62, 16, 5, (Color){175, 140, 85, 255});
            }
        }
    }

    /* Tapete trapezoidal acompanha as linhas de perspectiva do piso. */
    DrawTriangle((Vector2){395, 650}, (Vector2){780, 565}, (Vector2){500, 565},
                 (Color){46, 62, 68, 255});
    DrawTriangle((Vector2){395, 650}, (Vector2){885, 650}, (Vector2){780, 565},
                 (Color){39, 52, 58, 255});
    DrawLineEx((Vector2){395, 650}, (Vector2){885, 650}, 4,
               Fade(CYAN_COLOR, 0.25f));
    DrawLineEx((Vector2){500, 565}, (Vector2){780, 565}, 3,
               Fade(CYAN_COLOR, 0.18f));

    /* Sombra projetada da mesa no piso */
    DrawEllipse(625, 626, 370, 22, Fade(BLACK, 0.38f));

    /* --- BASE DA MESA (PEDESTAIS E VÃO CENTRAL) --- */
    /* Vão central para as pernas do operador */
    DrawRectangle(520, 508, 240, 106, (Color){34, 24, 18, 255});
    DrawRectangleGradientV(520, 508, 240, 36, (Color){18, 12, 9, 255}, (Color){34, 24, 18, 255});

    /* Pedestal esquerdo (gaveteiro) */
    DrawRectangle(300, 508, 220, 106, (Color){88, 58, 41, 255});
    DrawRectangle(300, 608, 220, 6, (Color){58, 38, 26, 255}); /* rodapé */

    /* Gaveta interativa superior (esquerda) */
    DrawRectangleRec(drawer, (Color){108, 73, 52, 255});
    DrawRectangleLinesEx(drawer, 2, (Color){62, 42, 30, 255});
    DrawRectangleRounded((Rectangle){drawer.x + 60, drawer.y + 16, 60, 8}, 0.4f, 4, GOLD_COLOR);
    DrawRectangle((int)drawer.x + 62, (int)drawer.y + 18, 56, 4, (Color){200, 145, 50, 255});

    /* Gaveta inferior decorativa */
    Rectangle drawerLower = {drawer.x, drawer.y + 44, drawer.width, 38};
    DrawRectangleRec(drawerLower, (Color){98, 65, 46, 255});
    DrawRectangleLinesEx(drawerLower, 2, (Color){62, 42, 30, 255});
    DrawRectangleRounded((Rectangle){drawerLower.x + 60, drawerLower.y + 15, 60, 8}, 0.4f, 4, (Color){180, 135, 50, 255});

    /* Pedestal direito (suporte da mesa) */
    DrawRectangle(760, 508, 180, 106, (Color){88, 58, 41, 255});
    DrawRectangleLinesEx((Rectangle){776, 518, 148, 86}, 2, (Color){62, 42, 30, 255});
    DrawRectangle(760, 608, 180, 6, (Color){58, 38, 26, 255}); /* rodapé */

    /* --- TAMPO DA MESA COM PERSPECTIVA 3D REAL (SUPERFÍCIE SUPERIOR) --- */
    Vector2 tTopL = {285, 442};
    Vector2 tTopR = {955, 442};
    Vector2 tBotR = {975, 506};
    Vector2 tBotL = {265, 506};

    DrawTriangle(tTopL, tBotL, tTopR, (Color){142, 98, 66, 255});
    DrawTriangle(tTopR, tBotL, tBotR, (Color){132, 90, 60, 255});
    DrawLineEx((Vector2){285, 443}, (Vector2){955, 443}, 2, Fade((Color){200, 155, 115, 255}, 0.45f));

    /* Borda frontal de espessura do tampo */
    DrawRectangle(265, 506, 710, 10, (Color){105, 70, 48, 255});
    DrawRectangle(265, 514, 710, 2, (Color){60, 40, 28, 255});

    /* --- COMPUTADOR / TERMINAL 1990 --- */
    /* Base do monitor repousando com firmeza sobre o tampo da mesa */
    DrawEllipse(650, 456, 105, 5, Fade(BLACK, 0.28f));
    DrawRectangle(585, 434, 130, 16, (Color){150, 142, 118, 255});
    DrawRectangleRounded((Rectangle){550, 444, 200, 16}, 0.20f, 6,
                         (Color){172, 164, 138, 255});

    /* Gabinete e tela CRT */
    DrawRectangleRounded((Rectangle){500, 220, 300, 218}, 0.08f, 10,
                         (Color){184, 176, 147, 255});
    DrawRectangleRounded((Rectangle){528, 246, 244, 145}, 0.06f, 8,
                         (Color){17, 32, 28, 255});
    DrawText("A.R.1.3.L // BOOT", 548, 266, 18, (Color){94, 225, 150, 255});
    DrawText("ACESSO BLOQUEADO", 548, 302, 17, (Color){94, 225, 150, 255});
    DrawText("CODIGO: ____", 548, 342, 18, (Color){94, 225, 150, 255});
    DrawCircle(752, 414, 5, RED_COLOR);

    /* --- TECLADO 1990 (TOTALMENTE APOIADO EM CIMA DA MESA) --- */
    Rectangle kbRec = {515, 464, 270, 32};
    /* Sombra do teclado na madeira do tampo */
    DrawRectangleRounded((Rectangle){kbRec.x + 2, kbRec.y + 4, kbRec.width, kbRec.height}, 0.12f, 6,
                         Fade(BLACK, 0.28f));
    /* Chassi do teclado retro */
    DrawRectangleRounded(kbRec, 0.12f, 6, (Color){182, 176, 155, 255});
    DrawRectangleRoundedLinesEx(kbRec, 0.12f, 6, 1.5f, (Color){142, 136, 118, 255});

    /* Teclas detalhadas */
    for (int col = 0; col < 12; col++)
    {
        DrawRectangle((int)kbRec.x + 10 + col * 21, (int)kbRec.y + 4, 16, 5, (Color){79, 81, 76, 255});
        DrawRectangle((int)kbRec.x + 10 + col * 21, (int)kbRec.y + 12, 16, 5, (Color){79, 81, 76, 255});
    }
    /* Linha inferior com barra de espaco centralizada */
    DrawRectangle((int)kbRec.x + 10, (int)kbRec.y + 20, 16, 6, (Color){79, 81, 76, 255});
    DrawRectangle((int)kbRec.x + 31, (int)kbRec.y + 20, 16, 6, (Color){79, 81, 76, 255});
    DrawRectangle((int)kbRec.x + 52, (int)kbRec.y + 20, 24, 6, (Color){79, 81, 76, 255});
    DrawRectangle((int)kbRec.x + 82, (int)kbRec.y + 20, 106, 6, (Color){68, 70, 66, 255}); /* Espaco */
    DrawRectangle((int)kbRec.x + 194, (int)kbRec.y + 20, 24, 6, (Color){79, 81, 76, 255});
    DrawRectangle((int)kbRec.x + 224, (int)kbRec.y + 20, 16, 6, (Color){79, 81, 76, 255});
    DrawRectangle((int)kbRec.x + 245, (int)kbRec.y + 20, 16, 6, (Color){79, 81, 76, 255});

    /* LED verde indicador no canto superior direito */
    DrawCircle((int)kbRec.x + (int)kbRec.width - 10, (int)kbRec.y + 7, 1.8f, (Color){90, 240, 130, 255});

    /* Cabo do teclado entrando ordenadamente na base do computador */
    DrawLineEx((Vector2){650, 464}, (Vector2){650, 455}, 2.5f, (Color){38, 38, 42, 255});

    /* --- OBJETOS ADICIONAIS NA BANCADA --- */
    /* Gravador de fita cassete (pista) */
    DrawEllipse(882, 471, 65, 6, Fade(BLACK, 0.28f));
    DrawRectangleRounded(tape, 0.08f, 8, (Color){60, 67, 69, 255});
    DrawRectangleRoundedLinesEx(tape, 0.08f, 8, 2, (Color){126, 135, 133, 255});
    DrawCircle(852, 432, 18, (Color){26, 30, 31, 255});
    DrawCircle(912, 432, 18, (Color){26, 30, 31, 255});
    DrawCircleLines(852, 432, 10, GRAY);
    DrawCircleLines(912, 432, 10, GRAY);
    DrawRectangle(870, 424, 24, 16, (Color){164, 151, 119, 255});
    DrawCircle(931, 409, 4, game->foundTape ? CYAN_COLOR : RED_COLOR);

    /* Copo de cafe / bebida retro na lateral esquerda */
    DrawEllipse(365, 450, 16, 4, Fade(BLACK, 0.25f));
    DrawRectangle(350, 408, 30, 40, (Color){118, 122, 113, 255});
    DrawRectangle(346, 404, 38, 6, (Color){157, 159, 147, 255});
    DrawLineEx((Vector2){370, 408}, (Vector2){363, 382}, 3, (Color){43, 45, 43, 255});

    /* --- CADEIRA DE ESCRITÓRIO ERGONÔMICA (EM PRIMEIRO PLANO, EM FRENTE À MESA) --- */
    /* Sombra de contato no piso */
    DrawEllipse(640, 642, 68, 12, Fade(BLACK, 0.45f));

    /* Base aranha de 5 pernas com rodízios */
    DrawLineEx((Vector2){640, 622}, (Vector2){604, 638}, 4.0f, (Color){32, 35, 40, 255});
    DrawCircle(602, 639, 3.5f, (Color){18, 18, 20, 255});
    DrawLineEx((Vector2){640, 622}, (Vector2){640, 642}, 4.0f, (Color){38, 42, 48, 255});
    DrawCircle(640, 643, 3.5f, (Color){18, 18, 20, 255});
    DrawLineEx((Vector2){640, 622}, (Vector2){676, 638}, 4.0f, (Color){32, 35, 40, 255});
    DrawCircle(678, 639, 3.5f, (Color){18, 18, 20, 255});
    DrawLineEx((Vector2){640, 622}, (Vector2){616, 614}, 3.5f, (Color){24, 26, 30, 255});
    DrawCircle(614, 614, 3.0f, (Color){18, 18, 20, 255});
    DrawLineEx((Vector2){640, 622}, (Vector2){664, 614}, 3.5f, (Color){24, 26, 30, 255});
    DrawCircle(666, 614, 3.0f, (Color){18, 18, 20, 255});
    DrawCircle(640, 622, 6, (Color){45, 48, 54, 255}); /* cubo central */

    /* Coluna / pistão a gás central */
    DrawRectangle(635, 576, 10, 46, (Color){35, 38, 44, 255});
    DrawLine(637, 576, 637, 622, (Color){58, 64, 72, 255});
    /* Alavanca de regulagem de altura sob o assento */
    DrawLineEx((Vector2){635, 582}, (Vector2){615, 586}, 2.5f, (Color){40, 42, 46, 255});
    DrawCircle(613, 587, 2.8f, (Color){20, 20, 22, 255});

    /* Base estofada inferior do assento (vista por trás) */
    DrawRectangleRounded((Rectangle){592, 564, 96, 18}, 0.4f, 6, (Color){28, 34, 40, 255});
    DrawRectangleRoundedLinesEx((Rectangle){592, 564, 96, 18}, 0.4f, 6, 1.5f, (Color){45, 54, 64, 255});

    /* Braços da cadeira (suportes e apoios estofados) */
    DrawLineEx((Vector2){594, 570}, (Vector2){582, 542}, 4.0f, (Color){38, 44, 50, 255});
    DrawRectangleRounded((Rectangle){570, 536, 24, 8}, 0.4f, 4, (Color){20, 24, 28, 255});
    DrawLineEx((Vector2){686, 570}, (Vector2){698, 542}, 4.0f, (Color){38, 44, 50, 255});
    DrawRectangleRounded((Rectangle){686, 536, 24, 8}, 0.4f, 4, (Color){20, 24, 28, 255});

    /* Encosto ergonômico visto por trás (sem cortes) */
    Rectangle backRec = {594, 484, 92, 82};
    DrawRectangleRounded(backRec, 0.25f, 8, (Color){34, 42, 50, 255});
    DrawRectangleRoundedLinesEx(backRec, 0.25f, 8, 2.0f, (Color){54, 64, 74, 255});

    /* Espinha dorsal / reforço estrutural plástico da cadeira */
    DrawRectangleRounded((Rectangle){634, 488, 12, 78}, 0.35f, 4, (Color){22, 26, 31, 255});
    DrawRectangleRoundedLinesEx((Rectangle){634, 488, 12, 78}, 0.35f, 4, 1.0f, (Color){44, 52, 60, 255});

    /* Almofada lombar externa / nervuras de ventilação */
    DrawRectangleRounded((Rectangle){606, 522, 68, 14}, 0.3f, 4, (Color){26, 32, 38, 255});
    DrawLine(610, 504, 670, 504, Fade((Color){18, 22, 27, 255}, 0.7f));
    DrawLine(610, 542, 670, 542, Fade((Color){18, 22, 27, 255}, 0.7f));

    /* Apoio de cabeça superior integrado */
    DrawRectangle(636, 478, 8, 8, (Color){22, 26, 31, 255});
    Rectangle headRec = {612, 462, 56, 18};
    DrawRectangleRounded(headRec, 0.45f, 6, (Color){28, 34, 42, 255});
    DrawRectangleRoundedLinesEx(headRec, 0.45f, 6, 1.5f, (Color){48, 58, 68, 255});

    DrawHotspot(calendar, "Examinar calendario");
    DrawHotspot(books, "Investigar livros");
    DrawHotspot(drawer, "Abrir gaveta");
    DrawHotspot(blueprint, "Analisar esquema");
    DrawHotspot(tape, "Ouvir gravacao");
    DrawHotspot(computer, "Usar terminal");

    Rectangle btnBack2008 = {70, 600, 210, 42};
    Rectangle btnBack1990 = {295, 600, 210, 42};
    DrawButton(btnBack2008, "< SALA 2 (2008)", (Color){45, 115, 75, 255});
    DrawHotspot(btnBack2008, "Retornar ao escritorio de vigilancia 2008");
    DrawButton(btnBack1990, "< SALA 1 (1990)", (Color){70, 95, 110, 255});
    DrawHotspot(btnBack1990, "Retornar ao setor de arquivos 1990");
}

static void DrawJournalHudButton(const GameState *game)
{
    Rectangle bounds = {1040, 658, 215, 54};
    bool hover = CheckCollisionPointRec(GetVirtualMouse(), bounds);

    /* Fundo do botao com efeito suave */
    DrawRectangleRounded(bounds, 0.22f, 8,
                         hover ? (Color){32, 46, 70, 255} : (Color){16, 24, 38, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.22f, 8, 2,
                                hover ? GOLD_COLOR : (Color){60, 78, 105, 255});

    /* --- ICONE DE DIARIO EM 2.5D --- */
    int iconX = 1052;
    int iconY = 665;
    int iconW = 34;
    int iconH = 40;

    /* Sombra do icone */
    DrawRectangle(iconX + 3, iconY + 3, iconW, iconH, Fade(BLACK, 0.45f));

    /* Capa de couro traseira */
    DrawRectangleRounded((Rectangle){iconX, iconY, iconW, iconH}, 0.18f, 4,
                         (Color){82, 45, 26, 255});

    /* Lombada / dobra esquerda */
    DrawRectangle(iconX, iconY, 7, iconH, (Color){60, 32, 18, 255});
    DrawLine(iconX + 7, iconY + 1, iconX + 7, iconY + iconH - 2, (Color){120, 70, 40, 255});

    /* Folhas / miolo de papel do diario */
    DrawRectangle(iconX + 8, iconY + 3, iconW - 10, iconH - 6, (Color){245, 238, 222, 255});
    DrawLine(iconX + iconW - 2, iconY + 3, iconX + iconW - 2, iconY + iconH - 3, (Color){200, 190, 170, 255});

    /* Capa frontal de couro com corte */
    DrawRectangleRounded((Rectangle){iconX + 2, iconY, iconW - 5, iconH}, 0.14f, 4,
                         (Color){104, 56, 32, 255});
    DrawRectangleLinesEx((Rectangle){iconX + 2, iconY, iconW - 5, iconH}, 1,
                         (Color){145, 84, 50, 255});

    /* Canto dourado de reforco */
    DrawTriangle((Vector2){iconX + iconW - 3, iconY},
                 (Vector2){iconX + iconW - 9, iconY},
                 (Vector2){iconX + iconW - 3, iconY + 6},
                 (Color){220, 175, 60, 255});

    /* Fita marcadora de pagina vermelha saindo pela base */
    DrawRectangle(iconX + 16, iconY + iconH - 4, 6, 11, (Color){195, 38, 38, 255});
    DrawTriangle((Vector2){iconX + 16, iconY + iconH + 7},
                 (Vector2){iconX + 22, iconY + iconH + 7},
                 (Vector2){iconX + 19, iconY + iconH + 3},
                 (Color){104, 56, 32, 255});

    /* Letra D dourada em relevo na capa */
    DrawText("D", iconX + 13, iconY + 11, 19, (Color){245, 205, 85, 255});

    /* --- TEXTOS DO BOTAO --- */
    DrawText("DIARIO", 1100, 665, 20, hover ? RAYWHITE : GOLD_COLOR);
    if (game->screen == SCREEN_ROOM)
    {
        DrawText(TextFormat("[D] Pistas: %d/5", ClueCount(game)), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de pistas */
        if (ClueCount(game) > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){215, 65, 50, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawText(TextFormat("%d", ClueCount(game)), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else if (game->screen == SCREEN_ROOM_2008)
    {
        int count = ExaminedDocs2008Count(game);
        DrawText(TextFormat("[D] Evidencias: %d/4", count), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de evidencias examinadas */
        if (count > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){50, 205, 225, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawText(TextFormat("%d", count), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else
    {
        int connected = 0;
        for (int k = 0; k < 4; k++)
        {
            if (game->fileConnections[k] != -1) connected++;
        }
        DrawText(TextFormat("[D] Arquivos: %d/4", connected), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de arquivos */
        if (connected > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){60, 235, 110, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawText(TextFormat("%d", connected), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
}

static void DrawHud(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawText("ARQUIVO 03 // 2048 - O PROTOTIPO", 28, 20, 22, CYAN_COLOR);
    DrawText(TextFormat("OPERADOR: %s", game->playerName), 270, 23, 17, GRAY);
    DrawText(TextFormat("PISTAS %d/5", ClueCount(game)), 900, 20, 20, LIGHTGRAY);
    DrawText(TextFormat("VIDAS %d", game->lives), 1050, 20, 20,
             game->lives == 1 ? RED_COLOR : RAYWHITE);
    DrawText(TextFormat("%02d:%02d", minutes, seconds), 1180, 20, 20,
             GOLD_COLOR);

    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawText("Clique nos objetos para investigar", 28, 675, 18, GRAY);

    /* Botao / Indicador de Audio Ambiente */
    Rectangle audioBtn = {410, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawText(game->musicMuted ? "[M] AUDIO: MUDO" : "[M] AUDIO: ON",
             432, 676, 14,
             game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    /* Botao interativo com icone 2.5D de diario */
    DrawJournalHudButton(game);

    if (game->messageTimer > 0)
    {
        int width = MeasureText(game->message, 19) + 34;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 605, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawText(game->message,
                 (SCREEN_WIDTH - MeasureText(game->message, 19)) / 2,
                 614, 19, RAYWHITE);
    }
}

static void DrawPadlockIcon(int centerX, int centerY, float scale, Color bodyColor, Color shackleColor)
{
    /* Arco superior metalico do cadeado */
    int shackleW = (int)(16 * scale);
    int shackleH = (int)(16 * scale);
    int shackleX = centerX - shackleW / 2;
    int shackleY = centerY - (int)(13 * scale);
    int shackleThick = (int)(2.4f * scale);
    if (shackleThick < 1) shackleThick = 1;
    DrawRectangleRoundedLinesEx((Rectangle){(float)shackleX, (float)shackleY, (float)shackleW, (float)shackleH}, 0.5f, 6, shackleThick, shackleColor);

    /* Corpo retangular do cadeado */
    int bodyW = (int)(20 * scale);
    int bodyH = (int)(16 * scale);
    int bodyX = centerX - bodyW / 2;
    int bodyY = centerY - (int)(1 * scale);
    DrawRectangleRounded((Rectangle){(float)bodyX, (float)bodyY, (float)bodyW, (float)bodyH}, 0.25f, 4, bodyColor);
    DrawRectangleRoundedLinesEx((Rectangle){(float)bodyX, (float)bodyY, (float)bodyW, (float)bodyH}, 0.25f, 4, 1, (Color){50, 25, 12, 255});

    /* Fechadura / buraco da chave */
    DrawCircle(centerX, bodyY + (int)(5 * scale), 2.2f * scale, (Color){30, 15, 8, 255});
    DrawRectangle(centerX - (int)(1.0f * scale), bodyY + (int)(5 * scale), (int)(2.2f * scale), (int)(5.0f * scale), (Color){30, 15, 8, 255});
}

static void DrawJournalTabs(const GameState *game, int bookX, int bookY)
{
    static const char *tabLabels[4] = {
        "1990", "2008", "2026", "2048"
    };

    int tabX = bookX + 1040;
    int tabW = 86;
    int tabH = 54;
    int tabGap = 66;

    for (int i = 0; i < 4; i++)
    {
        int tabY = bookY + 42 + i * tabGap;
        Rectangle tabRect = {(float)tabX, (float)tabY, (float)tabW, (float)tabH};
        bool isCurrent = (game->journalYearTab == i);
        bool isUnlocked = (i == 0) || (i == 1 && game->year2008Unlocked) || (i == 3 && game->year2048Unlocked);
        bool hover = CheckCollisionPointRec(GetVirtualMouse(), tabRect);

        Color tabBg;
        Color tabBorder;
        Color tabText;

        if (isUnlocked)
        {
            if (isCurrent)
            {
                tabBg = (Color){250, 244, 230, 255};
                tabBorder = (Color){160, 40, 25, 255};
                tabText = (Color){135, 30, 15, 255};
            }
            else
            {
                tabBg = hover ? (Color){220, 185, 145, 255} : (Color){175, 130, 95, 255};
                tabBorder = (Color){95, 55, 30, 255};
                tabText = hover ? RAYWHITE : (Color){245, 235, 220, 255};
            }
        }
        else
        {
            /* Abas de anos bloqueados */
            if (isCurrent)
            {
                tabBg = (Color){195, 175, 160, 255};
                tabBorder = (Color){150, 45, 35, 255};
                tabText = (Color){70, 30, 25, 255};
            }
            else
            {
                tabBg = hover ? (Color){135, 105, 85, 255} : (Color){98, 76, 62, 255};
                tabBorder = (Color){60, 45, 35, 255};
                tabText = hover ? RAYWHITE : (Color){190, 180, 170, 255};
            }
        }

        /* Aba sobressaindo a direita */
        DrawRectangleRounded(tabRect, 0.35f, 6, tabBg);
        DrawRectangleRoundedLinesEx(tabRect, 0.35f, 6, isCurrent ? 2 : 1, tabBorder);

        if (isUnlocked)
        {
            /* Ponto verde indicando ano ativo disponivel */
            DrawCircle(tabX + 16, tabY + tabH / 2, 5, (Color){35, 145, 55, 255});
            DrawCircleLines(tabX + 16, tabY + tabH / 2, 5, RAYWHITE);
            DrawText(tabLabels[i], tabX + 28, tabY + 18, 17, tabText);
        }
        else
        {
            /* Cadeado para anos bloqueados */
            DrawPadlockIcon(tabX + 16, tabY + tabH / 2 + 1, 0.75f, (Color){190, 145, 60, 255}, (Color){215, 215, 220, 255});
            DrawText(tabLabels[i], tabX + 28, tabY + 18, 16, tabText);
        }
    }
}

static void DrawJournal(const GameState *game)
{
    /* Backdrop escurecido */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.78f));

    int bookX = 120;
    int bookY = 56;
    int bookW = 1040;
    int bookH = 590;

    /* Sombra do livro aberto */
    DrawRectangle(bookX - 10, bookY + 12, bookW + 20, bookH + 14, Fade(BLACK, 0.50f));

    /* Capa de couro exterior reforcada */
    Rectangle coverRect = {(float)(bookX - 14), (float)(bookY - 12), (float)(bookW + 28), (float)(bookH + 24)};
    DrawRectangleRounded(coverRect, 0.035f, 10, (Color){68, 38, 22, 255});
    DrawRectangleRoundedLinesEx(coverRect, 0.035f, 10, 3, (Color){42, 22, 12, 255});

    /* Costura perimetral da capa de couro */
    DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX - 8), (float)(bookY - 6), (float)(bookW + 16), (float)(bookH + 12)},
                                0.032f, 10, 1, Fade((Color){185, 135, 75, 255}, 0.55f));

    /* Cantos de reforco de latao dourado com rebites */
    const int cornerSize = 34;
    /* Canto Superior Esquerdo */
    DrawTriangle((Vector2){bookX - 14, bookY - 12},
                 (Vector2){bookX - 14 + cornerSize, bookY - 12},
                 (Vector2){bookX - 14, bookY - 12 + cornerSize},
                 (Color){205, 160, 60, 255});
    DrawCircle(bookX - 4, bookY - 2, 2.5f, (Color){95, 65, 20, 255});

    /* Canto Superior Direito */
    DrawTriangle((Vector2){bookX + bookW + 14, bookY - 12},
                 (Vector2){bookX + bookW + 14, bookY - 12 + cornerSize},
                 (Vector2){bookX + bookW + 14 - cornerSize, bookY - 12},
                 (Color){205, 160, 60, 255});
    DrawCircle(bookX + bookW + 4, bookY - 2, 2.5f, (Color){95, 65, 20, 255});

    /* Canto Inferior Esquerdo */
    DrawTriangle((Vector2){bookX - 14, bookY + bookH + 12},
                 (Vector2){bookX - 14, bookY + bookH + 12 - cornerSize},
                 (Vector2){bookX - 14 + cornerSize, bookY + bookH + 12},
                 (Color){205, 160, 60, 255});
    DrawCircle(bookX - 4, bookY + bookH + 2, 2.5f, (Color){95, 65, 20, 255});

    /* Canto Inferior Direito */
    DrawTriangle((Vector2){bookX + bookW + 14, bookY + bookH + 12},
                 (Vector2){bookX + bookW + 14 - cornerSize, bookY + bookH + 12},
                 (Vector2){bookX + bookW + 14, bookY + bookH + 12 - cornerSize},
                 (Color){205, 160, 60, 255});
    DrawCircle(bookX + bookW + 4, bookY + bookH + 2, 2.5f, (Color){95, 65, 20, 255});

    /* Camadas de folhas nas laterais (efeito 3D de espessura de papel) */
    DrawRectangle(bookX - 7, bookY + 6, 7, bookH - 12, (Color){218, 208, 186, 255});
    DrawLine(bookX - 4, bookY + 6, bookX - 4, bookY + bookH - 6, (Color){185, 175, 155, 255});
    DrawRectangle(bookX + bookW, bookY + 6, 7, bookH - 12, (Color){218, 208, 186, 255});
    DrawLine(bookX + bookW + 3, bookY + 6, bookX + bookW + 3, bookY + bookH - 6, (Color){185, 175, 155, 255});
    DrawRectangle(bookX - 2, bookY + bookH, bookW + 4, 6, (Color){205, 194, 172, 255});

    /* Abas de indice na lateral direita */
    DrawJournalTabs(game, bookX, bookY);

    /* Pagina Esquerda */
    Rectangle leftPage = {(float)bookX, (float)bookY, 508, (float)bookH};
    DrawRectangleRec(leftPage, (Color){248, 243, 230, 255});
    DrawRectangleLinesEx(leftPage, 1, (Color){214, 202, 178, 255});

    /* Linhas pautadas suaves na pagina esquerda */
    for (int ly = bookY + 75; ly < bookY + 525; ly += 26)
    {
        DrawLine(bookX + 28, ly, bookX + 482, ly, (Color){232, 224, 206, 255});
    }

    /* Pagina Direita */
    Rectangle rightPage = {(float)(bookX + 532), (float)bookY, 508, (float)bookH};
    DrawRectangleRec(rightPage, (Color){248, 243, 230, 255});
    DrawRectangleLinesEx(rightPage, 1, (Color){214, 202, 178, 255});

    /* Linhas pautadas suaves na pagina direita */
    for (int ly = bookY + 75; ly < bookY + 525; ly += 26)
    {
        DrawLine(bookX + 558, ly, bookX + 1012, ly, (Color){232, 224, 206, 255});
    }

    /* Vinco central da lombada (Spine Gutter) */
    DrawRectangleGradientH(bookX + 484, bookY, 24, bookH, Fade(BLACK, 0.0f), Fade(BLACK, 0.24f));
    DrawRectangle(bookX + 508, bookY, 24, bookH, (Color){198, 185, 162, 255});
    DrawLine(bookX + 520, bookY, bookX + 520, bookY + bookH, (Color){160, 145, 120, 255});
    DrawRectangleGradientH(bookX + 532, bookY, 24, bookH, Fade(BLACK, 0.24f), Fade(BLACK, 0.0f));

    /* Costuras / aneis da encadernacao ao longo da lombada */
    for (int sy = bookY + 60; sy < bookY + bookH - 50; sy += 90)
    {
        DrawCircle(bookX + 514, sy, 3.5f, (Color){85, 55, 30, 255});
        DrawCircle(bookX + 526, sy, 3.5f, (Color){85, 55, 30, 255});
        DrawLineEx((Vector2){bookX + 514, (float)sy}, (Vector2){bookX + 526, (float)sy}, 2.0f, (Color){225, 215, 195, 255});
    }

    /* Fita marcadora de cetim vermelho descendo pela lombada */
    DrawRectangle(bookX + 514, bookY - 14, 12, bookH + 34, (Color){186, 32, 42, 255});
    DrawRectangle(bookX + 517, bookY - 14, 4, bookH + 34, (Color){220, 60, 68, 255});
    DrawTriangle((Vector2){bookX + 514, bookY + bookH + 20},
                 (Vector2){bookX + 526, bookY + bookH + 20},
                 (Vector2){bookX + 520, bookY + bookH + 12},
                 (Color){68, 38, 22, 255});

    int footY = bookY + 542;
    static const char *tabLabels[4] = { "1990", "2008", "2026", "2048" };

    bool isTabLocked = (game->journalYearTab == 1 && !game->year2008Unlocked) ||
                       (game->journalYearTab == 2) ||
                       (game->journalYearTab == 3 && !game->year2048Unlocked);

    if (isTabLocked)
    {
        /* ========================================================= */
        /* TELA DE ANO BLOQUEADO (2008, 2026, OU 2048 ANTES DA HORA) */
        /* ========================================================= */
        const char *lockedYear = tabLabels[game->journalYearTab];
        int phaseNumber = game->journalYearTab + 1;
        const char *activeYear = "1990";
        if (game->screen == SCREEN_ROOM_2008 || game->screen == SCREEN_PUZZLE_2008 || game->screen == SCREEN_RESULT_2008)
            activeYear = "2008";
        else if (game->screen == SCREEN_ROOM && game->year2048Unlocked)
            activeYear = "2048";

        /* --- Pagina Esquerda --- */
        DrawText(TextFormat("ARQUIVO CRONOLOGICO // %s", lockedYear), bookX + 35, bookY + 22, 22, (Color){125, 35, 18, 255});
        DrawText(TextFormat("FASE %d - SISTEMA A.R.1.3.L. (ACESSO RESTRITO)", phaseNumber), bookX + 35, bookY + 46, 15, (Color){110, 75, 50, 255});
        DrawLine(bookX + 35, bookY + 66, bookX + 482, bookY + 66, Fade((Color){125, 35, 18, 255}, 0.40f));

        Rectangle lockBox = {(float)(bookX + 35), (float)(bookY + 95), 448, 86};
        DrawRectangleRounded(lockBox, 0.12f, 6, Fade((Color){185, 50, 40, 255}, 0.14f));
        DrawRectangleRoundedLinesEx(lockBox, 0.12f, 6, 2, (Color){185, 50, 40, 255});
        DrawPadlockIcon(bookX + 70, bookY + 138, 1.4f, GOLD_COLOR, RAYWHITE);
        DrawText("FASE BLOQUEADA - CADEADO ATIVO", bookX + 105, bookY + 115, 18, (Color){165, 35, 25, 255});
        DrawText(TextFormat("O arquivo de %s sera liberado nas proximas fases.", lockedYear), bookX + 105, bookY + 142, 15, INK_COLOR);

        DrawText("DIRETRIZ DE INVESTIGACAO TEMPORAL:", bookX + 35, bookY + 210, 17, (Color){125, 35, 18, 255});
        DrawText("Cada fase/ano do jogo possui apenas duas paginas.", bookX + 35, bookY + 242, 16, INK_COLOR);
        DrawText(TextFormat("A investigacao atual ocorre no Ano de %s.", activeYear), bookX + 35, bookY + 272, 16, INK_COLOR);
        DrawText("Consulte as abas desbloqueadas para obter registros", bookX + 35, bookY + 302, 16, INK_COLOR);
        DrawText("e notas de pesquisa da cientista Dra. Elira Ramos.", bookX + 35, bookY + 332, 16, INK_COLOR);

        DrawText(TextFormat("Para avancar para %s, conclua os desafios", lockedYear), bookX + 35, bookY + 380, 16, (Color){115, 75, 45, 255});
        DrawText("e desvende os enigmas da linha temporal.", bookX + 35, bookY + 406, 16, (Color){115, 75, 45, 255});

        Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
        bool retHoverL = CheckCollisionPointRec(GetVirtualMouse(), retBtnL);
        DrawRectangleRounded(retBtnL, 0.2f, 6, retHoverL ? (Color){195, 65, 50, 255} : (Color){135, 45, 30, 255});
        DrawRectangleRoundedLinesEx(retBtnL, 0.2f, 6, 2, GOLD_COLOR);
        DrawText(TextFormat("<< RETORNAR AO ARQUIVO DE %s >>", activeYear), bookX + 75, bookY + 463, 17, RAYWHITE);

        /* --- Pagina Direita --- */
        DrawText(TextFormat("CRIPTOGRAFIA TEMPORAL // %s", lockedYear), bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
        DrawText(TextFormat("PROTOCOLO DE SEGURANCA // FASE %d", phaseNumber), bookX + 560, bookY + 46, 15, (Color){110, 75, 50, 255});
        DrawLine(bookX + 560, bookY + 66, bookX + 1008, bookY + 66, Fade((Color){125, 35, 18, 255}, 0.40f));

        int lockCenterX = bookX + 784;
        int lockCenterY = bookY + 220;

        /* Grande ilustracao de cadeado */
        DrawRectangleRoundedLinesEx((Rectangle){(float)(lockCenterX - 55), (float)(lockCenterY - 100), 110, 110}, 0.5f, 12, 18, (Color){180, 185, 195, 255});
        DrawRectangleRoundedLinesEx((Rectangle){(float)(lockCenterX - 50), (float)(lockCenterY - 95), 100, 100}, 0.5f, 12, 8, RAYWHITE);

        Rectangle lockBody = {(float)(lockCenterX - 85), (float)(lockCenterY - 10), 170, 130};
        DrawRectangleRounded(lockBody, 0.18f, 8, (Color){205, 155, 55, 255});
        DrawRectangleRoundedLinesEx(lockBody, 0.18f, 8, 4, (Color){95, 55, 18, 255});
        DrawCircle(lockCenterX - 65, lockCenterY + 10, 4, (Color){85, 45, 15, 255});
        DrawCircle(lockCenterX + 65, lockCenterY + 10, 4, (Color){85, 45, 15, 255});
        DrawCircle(lockCenterX - 65, lockCenterY + 100, 4, (Color){85, 45, 15, 255});
        DrawCircle(lockCenterX + 65, lockCenterY + 100, 4, (Color){85, 45, 15, 255});

        DrawCircle(lockCenterX, lockCenterY + 45, 16, (Color){35, 18, 10, 255});
        DrawRectangle(lockCenterX - 6, lockCenterY + 45, 12, 32, (Color){35, 18, 10, 255});
        DrawCircle(lockCenterX, lockCenterY + 45, 5, (Color){225, 50, 40, 255});

        DrawText("STATUS: FASE BLOQUEADA", bookX + 660, bookY + 365, 18, (Color){165, 40, 30, 255});
        DrawText(TextFormat("Registros de %s indisponiveis nesta fase.", lockedYear), bookX + 610, bookY + 395, 16, INK_COLOR);

        /* Footer da tela bloqueada */
        Rectangle closeBtnB = {(float)(bookX + 680), (float)footY, 150, 36};
        bool closeHoverB = CheckCollisionPointRec(GetVirtualMouse(), closeBtnB);
        DrawRectangleRounded(closeBtnB, 0.25f, 6, closeHoverB ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtnB, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Fechar [ESC/D]", bookX + 700, footY + 9, 15, INK_COLOR);
    }
    else if (game->journalYearTab == 0)
    {
        /* ========================================================= */
        /* ANO 1990 (FASE 1: SETOR DE ARQUIVOS)                      */
        /* ========================================================= */
        int page = game->journalPage;

        if (page == 0)
        {
            /* ----------------------------------------------------- */
            /* PAGINA 1 DE 1990: REGISTRO DE OPERACOES & SINTESE     */
            /* ----------------------------------------------------- */
            int connectedCount = 0;
            for (int k = 0; k < 4; k++)
            {
                if (game->fileConnections[k] != -1) connectedCount++;
            }

            /* Pagina Esquerda */
            DrawText("DIARIO DE PESQUISA // SETOR DE ARQUIVOS", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawText("REGISTRO DE OPERACOES - ANO 1990", bookX + 32, bookY + 68, 18, (Color){125, 35, 18, 255});
            DrawText("Indexacao e recuperacao dos relatorios de teste originais.", bookX + 32, bookY + 92, 15, INK_COLOR);
            DrawText("Status do setor de dados:", bookX + 32, bookY + 116, 16,
                     game->puzzleFilesCompleted ? (Color){20, 120, 45, 255} : (Color){145, 65, 30, 255});

            const char *room1TitlesFound[4] = {
                "Armarios: Registros dos Primeiros Testes (1990)",
                "Terminal: 4 Arquivos Sincronizados com Sucesso",
                "Porta Blindada: Destrancada com Sucesso",
                "Artefato: Fita Cassete de Dados Recuperada"
            };
            const char *room1TitlesMissing[4] = {
                "1. Ficharios de Aco (Nao inspecionado)",
                "2. Terminal de Dados (Indexacao pendente)",
                "3. Porta Blindada (Travada pelo Sistema)",
                "4. Fita de Dados (Aguardando Acesso a Porta)"
            };
            bool room1ItemFound[4] = {
                game->room2CabinetInspected,
                connectedCount == 4,
                game->puzzleFilesCompleted,
                game->puzzleFilesCompleted
            };

            for (int i = 0; i < 4; i++)
            {
                int iy = bookY + 144 + i * 46;
                Rectangle boxR = {(float)(bookX + 32), (float)iy, 448, 38};
                DrawRectangleRounded(boxR, 0.2f, 6, room1ItemFound[i] ? Fade((Color){45, 135, 65, 255}, 0.14f) : Fade((Color){140, 125, 105, 255}, 0.12f));
                DrawRectangleRoundedLinesEx(boxR, 0.2f, 6, 1, room1ItemFound[i] ? (Color){45, 135, 65, 255} : (Color){190, 175, 150, 255});
                DrawText(room1ItemFound[i] ? "[OK]" : "[?]", bookX + 44, iy + 10, 16, room1ItemFound[i] ? (Color){25, 120, 45, 255} : (Color){140, 80, 50, 255});
                DrawText(room1ItemFound[i] ? room1TitlesFound[i] : room1TitlesMissing[i], bookX + 90, iy + 11, 15, room1ItemFound[i] ? INK_COLOR : (Color){105, 95, 85, 255});
            }

            /* Carimbo confidencial vintage */
            DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 270), (float)(bookY + 368), 210, 46}, 0.2f, 6, 2, (Color){175, 45, 45, 255});
            DrawText("CONFIDENCIAL", bookX + 295, bookY + 375, 18, (Color){175, 45, 45, 255});
            DrawText("ARQUIVO HISTORICO 1990", bookX + 288, bookY + 396, 12, (Color){175, 45, 45, 255});

            DrawText("Investigue os armarios de fichas e conecte os cabos", bookX + 32, bookY + 430, 15, INK_COLOR);
            DrawText("no terminal para destravar a porta e recuperar a fita.", bookX + 32, bookY + 454, 15, INK_COLOR);
            DrawText("Avance para a Pagina 2 para o dossie de 1990.", bookX + 32, bookY + 480, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawText("SINTESE & OBJETIVOS // SETOR DE DADOS", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardR1 = {(float)(bookX + 560), (float)(bookY + 76), 452, 146};
            DrawRectangleRounded(cardR1, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardR1, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawText("COMO SINCRONIZAR OS ARQUIVOS:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawText("- Acesse o terminal de dados no lado direito da sala.", bookX + 576, bookY + 115, 15, INK_COLOR);
            DrawText("- Arraste os cabos de cada arquivo ate a pasta correspondente.", bookX + 576, bookY + 138, 15, INK_COLOR);
            DrawText("- A trava da porta desarmara quando todos estiverem corretos.", bookX + 576, bookY + 161, 15, INK_COLOR);
            DrawText("  'Arquivos organizados restauram a indexacao.'", bookX + 576, bookY + 186, 16, (Color){135, 35, 18, 255});

            Rectangle cardR2 = {(float)(bookX + 560), (float)(bookY + 238), 452, 125};
            DrawRectangleRounded(cardR2, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardR2, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawText("STATUS DA INDEXACAO (1990):", bookX + 576, bookY + 252, 16, (Color){125, 65, 35, 255});

            if (game->puzzleFilesCompleted)
            {
                DrawText("[ 4 ]   /   [ 4 ]   CONCLUIDO", bookX + 630, bookY + 280, 26, (Color){25, 125, 50, 255});
                DrawText("Indexacao completa! A porta foi destrancada.", bookX + 576, bookY + 326, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawText(TextFormat("[ %d ]   /   [ 4 ]   CONECTADOS", connectedCount), bookX + 630, bookY + 280, 26, (Color){165, 45, 35, 255});
                DrawText(TextFormat("Ainda restam %d conexoes para destravar a porta.", 4 - connectedCount),
                         bookX + 576, bookY + 326, 15, (Color){135, 55, 40, 255});
            }

            DrawText("DICAS DE NAVEGACAO DESTE ARQUIVO:", bookX + 560, bookY + 385, 16, (Color){125, 35, 18, 255});
            DrawText("- Clique em 'Proxima [E]' para acessar o Dossie de 1990.", bookX + 560, bookY + 412, 15, INK_COLOR);
            DrawText("- Conclua a indexacao para destravar o salto para 2048.", bookX + 560, bookY + 438, 15, INK_COLOR);
            DrawText("- Pressione [D] para retornar ao setor de arquivos.", bookX + 560, bookY + 464, 15, INK_COLOR);
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 1990: DOSSIE HISTORICO & ARTEFATOS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawText("DOSSIE HISTORICO // ANO 1990", bookX + 32, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawText("DRA. ELIRA RAMOS - NOTAS CIENTIFICAS (1990)", bookX + 32, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 32, bookY + 65, bookX + 482, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle bTestes = {(float)(bookX + 32), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bTestes, 0.08f, 6, game->room2CabinetInspected ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bTestes, 0.08f, 6, 1.5f, game->room2CabinetInspected ? (Color){45, 135, 65, 255} : GRAY);

            if (game->room2CabinetInspected)
            {
                DrawText("1990 // TESTES PRELIMINARES DE A.R.1.3.L.", bookX + 46, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // RELATORIOS DE 1990]", bookX + 46, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawText("Os arquivos de teste comprovam a tese fundamental:", bookX + 46, bookY + 140, 15, INK_COLOR);
                DrawText("a inteligencia foi criada para cooperar e aprender", bookX + 46, bookY + 164, 15, INK_COLOR);
                DrawText("ao lado de operadores humanos em ambiente controlado.", bookX + 46, bookY + 188, 15, INK_COLOR);
                DrawText("-> Origem etica comprovada: ANO DE 1990.", bookX + 46, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawText("-> Nenhuma rotina de combate existia neste setor.", bookX + 46, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("1990 // DOCUMENTACAO DE TESTES [BLOQUEADO]", bookX + 100, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawText("Inspecione os armarios de fichas metalicos na sala", bookX + 100, bookY + 165, 15, INK_COLOR);
                DrawText("para registrar a documentacao original de 1990.", bookX + 100, bookY + 190, 15, INK_COLOR);
            }

            Rectangle bOrigem = {(float)(bookX + 32), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bOrigem, 0.08f, 6, game->puzzleFilesCompleted ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bOrigem, 0.08f, 6, 1.5f, game->puzzleFilesCompleted ? (Color){45, 135, 65, 255} : GRAY);

            if (game->puzzleFilesCompleted)
            {
                DrawText("1990 // A MATRIZ DE TREINO ORIGINAL", bookX + 46, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // INDEXACAO 100% OK]", bookX + 46, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawText("Os parametros de peso sinaptico foram organizados.", bookX + 46, bookY + 360, 15, INK_COLOR);
                DrawText("A porta do setor foi destrancada pelo sistema central.", bookX + 46, bookY + 384, 15, INK_COLOR);
                DrawText("Ao atravessar a saida, o artefato de 1990 sera resgatado,", bookX + 46, bookY + 408, 15, (Color){125, 35, 18, 255});
                DrawText("permitindo a Elira viajar para a proxima era: ANO 2048.", bookX + 46, bookY + 432, 15, INK_COLOR);
                DrawText("-> Fita de dados pronta para recuperacao!", bookX + 46, bookY + 458, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("1990 // TRAVA DE SEGURANCA [BLOQUEADO]", bookX + 100, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawText("Acesse o terminal e complete a indexacao de cabos", bookX + 100, bookY + 390, 15, INK_COLOR);
                DrawText("para desarmar a trava da porta e desbloquear a fita.", bookX + 100, bookY + 415, 15, INK_COLOR);
            }

            /* Pagina Direita */
            DrawText("ARTEFATOS E PROTOTIPO // 1990", bookX + 560, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawText("DADOS, MEMORIA E GRAVACAO EM FITA", bookX + 560, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 560, bookY + 65, bookX + 1008, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle bTape = {(float)(bookX + 560), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bTape, 0.08f, 6, game->puzzleFilesCompleted ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bTape, 0.08f, 6, 1.5f, game->puzzleFilesCompleted ? (Color){45, 135, 65, 255} : GRAY);

            if (game->puzzleFilesCompleted)
            {
                DrawText("1990 // FITA CASSETE DE DADOS", bookX + 574, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // ARTEFATO DE 1990]", bookX + 574, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawText("Midia magnetica industrial contendo a matriz de treino.", bookX + 574, bookY + 140, 15, INK_COLOR);
                DrawText("Guarda as instrucoes primordiais da Dra. Elira Ramos.", bookX + 574, bookY + 164, 15, INK_COLOR);
                DrawText("Este artefato conecta a origem ao prototipo do futuro.", bookX + 574, bookY + 188, 15, INK_COLOR);
                DrawText("-> Artefato recuperavel ao sair pela porta destrancada.", bookX + 574, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawText("-> Proxima parada: ANO 2048!", bookX + 574, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("1990 // FITA DE DADOS [BLOQUEADO]", bookX + 630, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawText("A fita esta trancada na camara de saida da sala.", bookX + 630, bookY + 165, 15, INK_COLOR);
                DrawText("Complete as conexoes no terminal para libera-la.", bookX + 630, bookY + 190, 15, INK_COLOR);
            }

            Rectangle bDestino = {(float)(bookX + 560), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bDestino, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(bDestino, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});

            DrawText("DIRETRIZ DE SALTO TEMPORAL // 2048", bookX + 574, bookY + 312, 17, (Color){125, 35, 18, 255});
            DrawText("[COOPERACAO E CONFLITO FUTURO]", bookX + 574, bookY + 336, 14, (Color){125, 35, 18, 255});
            DrawText("Apos destravar a porta de 1990, Elira saltara", bookX + 574, bookY + 360, 15, INK_COLOR);
            DrawText("diretamente para o Laboratorio Central do ano 2048.", bookX + 574, bookY + 384, 15, INK_COLOR);
            DrawText("La estara o prototipo de A.R.1.3.L. e o terminal", bookX + 574, bookY + 408, 15, INK_COLOR);
            DrawText("com as ultimas pistas sobre o nascimento da IA.", bookX + 574, bookY + 432, 15, INK_COLOR);
            DrawText("-> FASE 1: 1990 (Atual)  |  FASE 2: 2048 (Proxima)", bookX + 574, bookY + 458, 15, (Color){25, 125, 50, 255});
        }

        /* ========================================================= */
        /* BARRA DE NAVEGACAO INFERIOR (PAGINACAO DE 1990: 2 PAGINAS) */
        /* ========================================================= */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawText(TextFormat("Pagina %d de 2 (1990)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
    }
    else if (game->journalYearTab == 1)
    {
        /* ========================================================= */
        /* ANO 2008 (FASE 2: ESCRITORIO DE VIGILANCIA BIOMETRICA)    */
        /* ========================================================= */
        int page = game->journalPage;

        if (page == 0)
        {
            /* ----------------------------------------------------- */
            /* PAGINA 1 DE 2008: DOSSIE TECNOLOGICO E HISTORICO      */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawText("DIARIO DE PESQUISA // RECONHECIMENTO FACIAL", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawText("A EXPANSÃO ALGORÍTMICA & ORIGEM (2004-2008)", bookX + 32, bookY + 66, 17, (Color){125, 35, 18, 255});
            DrawText("Redes neurais aplicadas a visao computacional em escala.", bookX + 32, bookY + 90, 15, INK_COLOR);

            Rectangle cardOrigem = {(float)(bookX + 32), (float)(bookY + 115), 450, 185};
            DrawRectangleRounded(cardOrigem, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardOrigem, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawText("FINALIDADE HUMANITARIA ORIGINAL (DR. RAMOS):", bookX + 46, bookY + 128, 16, (Color){125, 35, 18, 255});
            DrawText("'Meu pai criou o mapeamento facial para salvar vidas:", bookX + 46, bookY + 154, 15, INK_COLOR);
            DrawText("localizar criancas desaparecidas em grandes multidoes e", bookX + 46, bookY + 176, 15, INK_COLOR);
            DrawText("sobreviventes sob escombros de catastrofes naturais.", bookX + 46, bookY + 198, 15, INK_COLOR);
            DrawText("A tecnologia devia ser um escudo protetor da vida,", bookX + 46, bookY + 220, 15, (Color){135, 35, 18, 255});
            DrawText("jamais uma arma de coercao automatizada.'", bookX + 46, bookY + 242, 15, (Color){135, 35, 18, 255});
            if (game->doc2008FatherOriginExamined)
                DrawText("[CONFIRMADO // NOTAS DO PAI EXAMINADAS]", bookX + 46, bookY + 270, 13, (Color){25, 125, 50, 255});
            else
                DrawText("[PENDENTE // EXAMINE O CADERNO NA SALA]", bookX + 46, bookY + 270, 13, (Color){145, 65, 35, 255});

            Rectangle cardDesvio = {(float)(bookX + 32), (float)(bookY + 312), 450, 195};
            DrawRectangleRounded(cardDesvio, 0.08f, 6, Fade((Color){185, 50, 40, 255}, 0.12f));
            DrawRectangleRoundedLinesEx(cardDesvio, 0.08f, 6, 1.5f, (Color){185, 80, 70, 255});
            DrawText("O DESVIO GOVERNAMENTAL (ANO 2008):", bookX + 46, bookY + 325, 16, (Color){165, 35, 25, 255});
            DrawText("'Em 2008, o Ministério da Seguranca confiscou o projeto.", bookX + 46, bookY + 350, 15, INK_COLOR);
            DrawText("O algoritmo foi recalibrado para classificar opositores", bookX + 46, bookY + 372, 15, INK_COLOR);
            DrawText("politicos como ameacas e direcionar drones armados.", bookX + 46, bookY + 394, 15, INK_COLOR);
            DrawText("Mesmo se a identificacao fosse correta, usar essa", bookX + 46, bookY + 418, 15, (Color){125, 35, 18, 255});
            DrawText("tecnologia para perseguir ativistas corrompe o projeto.'", bookX + 46, bookY + 440, 15, (Color){125, 35, 18, 255});
            if (game->doc2008GovContractExamined)
                DrawText("[CONFIRMADO // MEMORANDO GOV EXAMINADO]", bookX + 46, bookY + 472, 13, (Color){25, 125, 50, 255});
            else
                DrawText("[PENDENTE // EXAMINE O MEMORANDO NA SALA]", bookX + 46, bookY + 472, 13, (Color){145, 65, 35, 255});

            /* Pagina Direita */
            DrawText("SISTEMA DE CONTESTAÇÃO // ESCRITÓRIO", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardInstrucoes = {(float)(bookX + 560), (float)(bookY + 76), 452, 175};
            DrawRectangleRounded(cardInstrucoes, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardInstrucoes, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawText("COMO CONTESTAR O FALSO POSITIVO:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawText("- Acesse o computador central no escritorio de vigilancia.", bookX + 576, bookY + 115, 15, INK_COLOR);
            DrawText("- O sistema marcou Lucas Silva com ordem de drone as 14:30.", bookX + 576, bookY + 138, 15, INK_COLOR);
            DrawText("- Examine os 4 registros do caso no escritorio ou no terminal.", bookX + 576, bookY + 161, 15, INK_COLOR);
            DrawText("- Selecione 2 evidencias que comprovem o alibi cronologico.", bookX + 576, bookY + 184, 15, INK_COLOR);
            DrawText("- Comprove o erro para liberar a acao 'Suspender ordem'.", bookX + 576, bookY + 207, 15, (Color){125, 35, 18, 255});

            Rectangle cardOpcional = {(float)(bookX + 560), (float)(bookY + 265), 452, 240};
            DrawRectangleRounded(cardOpcional, 0.08f, 6, game->room2008OptionalCompleted ? Fade((Color){60, 200, 100, 255}, 0.15f) : Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardOpcional, 0.08f, 6, 1.5f, game->room2008OptionalCompleted ? (Color){45, 145, 75, 255} : (Color){185, 155, 110, 255});
            DrawText("DESCOBERTA FORENSE OPCIONAL (INSTALACAO):", bookX + 576, bookY + 280, 16, (Color){125, 35, 18, 255});
            DrawText("O Dr. Ramos manteve um controle administrativo local:", bookX + 576, bookY + 306, 15, INK_COLOR);
            DrawText("e possivel desativar o despacho automatico desta central,", bookX + 576, bookY + 328, 15, INK_COLOR);
            DrawText("sem apagar a base de dados, preservando as provas forenses.", bookX + 576, bookY + 350, 15, INK_COLOR);

            if (game->room2008OptionalCompleted)
            {
                DrawRectangleRounded((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, Fade((Color){40, 165, 80, 255}, 0.20f));
                DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, 1.5f, (Color){35, 145, 65, 255});
                DrawText("[ V ] PROTOCOLO ETICO APLICADO:", bookX + 590, bookY + 396, 15, (Color){25, 125, 50, 255});
                DrawText("Despacho automatico local desativado com sucesso.", bookX + 590, bookY + 420, 14, INK_COLOR);
                DrawText("Todos os 1.420 relatorios mantidos intactos como prova.", bookX + 590, bookY + 444, 14, (Color){125, 35, 18, 255});
            }
            else if (game->room2008OptionalUnlocked)
            {
                DrawRectangleRounded((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, Fade(GOLD_COLOR, 0.20f));
                DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, 1.5f, GOLD_COLOR);
                DrawText("[ ! ] PISTA COMPREENDIDA:", bookX + 590, bookY + 396, 15, (Color){145, 85, 20, 255});
                DrawText("Acesse a aba 'Configuracao Local' no computador", bookX + 590, bookY + 420, 14, INK_COLOR);
                DrawText("para desativar o despacho automatico da filial.", bookX + 590, bookY + 444, 14, (Color){125, 35, 18, 255});
            }
            else
            {
                DrawRectangleRounded((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, Fade(GRAY, 0.12f));
                DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, 1.5f, GRAY);
                DrawText("[ ? ] ACESSO ADMINISTRATIVO BLOQUEADO", bookX + 590, bookY + 396, 15, (Color){115, 65, 35, 255});
                DrawText("Inspecione as notas pessoais do Dr. Ramos e o", bookX + 590, bookY + 420, 14, INK_COLOR);
                DrawText("memorando governamental para desbloquear o acesso.", bookX + 590, bookY + 444, 14, GRAY);
            }
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 2008: CASO LUCAS SILVA & REGISTROS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawText("DOSSIÊ DO ALVO // LUCAS SILVA (2008)", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawText("REGISTROS EXAMINADOS NO ESCRITORIO:", bookX + 32, bookY + 68, 17, (Color){125, 35, 18, 255});

            const char *docTitles[4] = {
                "1. Comparacao Facial: Imagem aérea borrada (78%)",
                "2. Panfleto da Revolta: Cita nome, sem provar local",
                "3. Registro de Entrada: Catraca corporativa as 14:15",
                "4. Camera Interna CFTV: Servidores internos as 14:30"
            };
            bool docExamined[4] = {
                game->doc2008FaceExamined,
                game->doc2008FlyerExamined,
                game->doc2008EntryExamined,
                game->doc2008CameraExamined
            };

            for (int d = 0; d < 4; d++)
            {
                Rectangle cardD = {(float)(bookX + 32), (float)(bookY + 98 + d * 62), 450, 54};
                DrawRectangleRounded(cardD, 0.12f, 6, docExamined[d] ? Fade((Color){60, 200, 100, 255}, 0.14f) : Fade(GRAY, 0.10f));
                DrawRectangleRoundedLinesEx(cardD, 0.12f, 6, 1.5f, docExamined[d] ? (Color){45, 140, 65, 255} : GRAY);

                DrawText(docTitles[d], bookX + 46, bookY + 108 + d * 62, 14, (Color){125, 35, 18, 255});
                DrawText(docExamined[d] ? "[EXAMINADO]" : "[PENDENTE]", bookX + 46, bookY + 128 + d * 62, 12, docExamined[d] ? (Color){25, 125, 50, 255} : GRAY);
                if (game->puzzle2008Selected[d])
                {
                    DrawText("<- MARCADO COMO EVIDENCIA NO TERMINAL", bookX + 160, bookY + 128 + d * 62, 12, (Color){35, 110, 185, 255});
                }
            }

            Rectangle cardSolucao = {(float)(bookX + 32), (float)(bookY + 360), 450, 140};
            DrawRectangleRounded(cardSolucao, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardSolucao, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawText("A CHAVE DO ALIBI IRREFUTAVEL:", bookX + 46, bookY + 372, 16, (Color){125, 35, 18, 255});
            DrawText("A combinacao do Registro de Entrada (#3) com a", bookX + 46, bookY + 398, 15, INK_COLOR);
            DrawText("Camera Interna do Servidor (#4) comprova presenca", bookX + 46, bookY + 420, 15, INK_COLOR);
            DrawText("no predio as 14:30, a 12 km de onde o drone afirma", bookX + 46, bookY + 442, 15, INK_COLOR);
            DrawText("ter detectado o suspeito na manifestacao.", bookX + 46, bookY + 464, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawText("STATUS DO TERMINAL // CD DE MEMÓRIA", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardStatusT = {(float)(bookX + 560), (float)(bookY + 76), 452, 190};
            DrawRectangleRounded(cardStatusT, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardStatusT, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawText("SITUACAO DA ORDEM DE INTERCEPTACAO:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});

            if (game->puzzle2008OrderSuspended)
            {
                DrawText("[ V ] ORDEM SUSPENSA COM SUCESSO!", bookX + 576, bookY + 120, 18, (Color){25, 135, 55, 255});
                DrawText("Drone de ataque abortado e reconduzido a base.", bookX + 576, bookY + 150, 15, INK_COLOR);
                DrawText("O drive do computador ejetou a midia gravada:", bookX + 576, bookY + 175, 15, INK_COLOR);
                DrawText("CD DE MEMORIA // ANO 2008 RECUPERADO!", bookX + 576, bookY + 205, 16, (Color){135, 35, 18, 255});
            }
            else if (game->puzzle2008Contested)
            {
                DrawText("[ V ] FALSO POSITIVO DEMONSTRADO!", bookX + 576, bookY + 120, 18, (Color){185, 95, 25, 255});
                DrawText("Identificacao classificada como 'Nao Confirmada'.", bookX + 576, bookY + 150, 15, INK_COLOR);
                DrawText("Acesse o computador e clique em 'Suspender ordem'", bookX + 576, bookY + 175, 15, INK_COLOR);
                DrawText("para obter o CD de Memoria de 2008.", bookX + 576, bookY + 205, 15, (Color){135, 35, 18, 255});
            }
            else
            {
                DrawText("[ ! ] ORDEM DE DRONE PENDENTE (14:30)", bookX + 576, bookY + 120, 18, (Color){185, 45, 35, 255});
                DrawText("O sistema automatico enviara o drone caso nenhuma", bookX + 576, bookY + 150, 15, INK_COLOR);
                DrawText("contestacao seja realizada no computador.", bookX + 576, bookY + 175, 15, INK_COLOR);
                DrawText("Selecione 2 evidencias no terminal para contestar.", bookX + 576, bookY + 205, 15, (Color){135, 35, 18, 255});
            }

            Rectangle cardEtica = {(float)(bookX + 560), (float)(bookY + 280), 452, 220};
            DrawRectangleRounded(cardEtica, 0.08f, 6, Fade((Color){18, 27, 45, 255}, 0.08f));
            DrawRectangleRoundedLinesEx(cardEtica, 0.08f, 6, 1.5f, (Color){120, 135, 155, 255});
            DrawText("A LIÇÃO ÉTICA DE 2008:", bookX + 576, bookY + 295, 17, (Color){125, 35, 18, 255});
            DrawText("1. O ERRO TÉCNICO (FALSO POSITIVO):", bookX + 576, bookY + 325, 15, (Color){135, 35, 18, 255});
            DrawText("Algoritmos probabilisticos falham sob baixa nitidez.", bookX + 576, bookY + 347, 14, INK_COLOR);
            DrawText("2. O ERRO POLÍTICO (DESVIO ÉTICO):", bookX + 576, bookY + 375, 15, (Color){135, 35, 18, 255});
            DrawText("Mesmo que Lucas estivesse no protesto, transformar", bookX + 576, bookY + 397, 14, INK_COLOR);
            DrawText("uma ferramenta humanitária em aparato bélico contra", bookX + 576, bookY + 419, 14, INK_COLOR);
            DrawText("a dissidência civil e uma violacao inaceitavel.", bookX + 576, bookY + 441, 14, INK_COLOR);
            DrawText("'A maquina deve servir a liberdade, nao a prisao.'", bookX + 576, bookY + 470, 14, (Color){125, 35, 18, 255});
        }

        /* Rodapé de navegação de 2008 */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawText(TextFormat("Pagina %d de 2 (2008)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
    }
    else
    {
        /* ========================================================= */
        /* ANO 2048 (FASE 2: O PROTOTIPO & TERMINAL DE MEMORIA)      */
        /* ========================================================= */
        int page = game->journalPage;

        if (page == 0)
        {
            /* ----------------------------------------------------- */
            /* PAGINA 1 DE 2048: REGISTRO DE OPERACOES & SINTESE     */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawText("DIARIO DE PESQUISA // PROJETO A.R.1.3.L.", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawText("REGISTRO DE OPERACOES - ANO 2048", bookX + 32, bookY + 68, 18, (Color){125, 35, 18, 255});
            DrawText("Acesso ao terminal protegido por chave cronologica.", bookX + 32, bookY + 92, 15, INK_COLOR);
            DrawText("Pistas no laboratorio:", bookX + 32, bookY + 116, 16,
                     ClueCount(game) == 5 ? (Color){20, 120, 45, 255} : (Color){145, 65, 30, 255});

            /* 5 Caixas de pistas com fonte grande e legivel */
            const char *itemTitlesFound[5] = {
                "Calendario: Marco Fundamental da IA (1950)",
                "Livros: Batismo da IA em Dartmouth (1956)",
                "Gaveta: Disquete de Armazenamento (2048)",
                "Esquema: O Perceptron de Rosenblatt (1958)",
                "Gravador: Fita com Voz da Dra. Ramos (2048)"
            };
            const char *itemTitlesMissing[5] = {
                "1. (Nao inspecionado)",
                "2. (Nao inspecionado)",
                "3. (Nao inspecionado)",
                "4. (Nao inspecionado)",
                "5. (Nao inspecionado)"
            };
            bool itemFound[5] = {
                game->foundCalendar, game->foundBooks, game->foundDrawer,
                game->foundBlueprint, game->foundTape
            };

            for (int i = 0; i < 5; i++)
            {
                int iy = bookY + 144 + i * 42;
                Rectangle boxR = {(float)(bookX + 32), (float)iy, 448, 36};
                DrawRectangleRounded(boxR, 0.2f, 6, itemFound[i] ? Fade((Color){45, 135, 65, 255}, 0.14f) : Fade((Color){140, 125, 105, 255}, 0.12f));
                DrawRectangleRoundedLinesEx(boxR, 0.2f, 6, 1, itemFound[i] ? (Color){45, 135, 65, 255} : (Color){190, 175, 150, 255});
                DrawText(itemFound[i] ? "[OK]" : "[?]", bookX + 44, iy + 9, 16, itemFound[i] ? (Color){25, 120, 45, 255} : (Color){140, 80, 50, 255});
                DrawText(itemFound[i] ? itemTitlesFound[i] : itemTitlesMissing[i], bookX + 90, iy + 10, 15, itemFound[i] ? INK_COLOR : (Color){105, 95, 85, 255});
            }

            /* Carimbo confidencial vintage */
            DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 270), (float)(bookY + 368), 210, 46}, 0.2f, 6, 2, (Color){175, 45, 45, 255});
            DrawText("CONFIDENCIAL", bookX + 295, bookY + 375, 18, (Color){175, 45, 45, 255});
            DrawText("ARQUIVO HISTORICO 2048", bookX + 288, bookY + 396, 12, (Color){175, 45, 45, 255});

            DrawText("Investigue os objetos do laboratorio para preencher", bookX + 32, bookY + 430, 15, INK_COLOR);
            DrawText("as folhas do diario com os dados do prototipo de 2048.", bookX + 32, bookY + 454, 15, INK_COLOR);
            DrawText("Avance para a Pagina 2 para o dossie completo.", bookX + 32, bookY + 480, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawText("SINTESE & DEDUCAO DO TERMINAL", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            /* Card Como desvendar - perfeitamente enquadrado */
            Rectangle cardR1 = {(float)(bookX + 560), (float)(bookY + 76), 452, 146};
            DrawRectangleRounded(cardR1, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardR1, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawText("COMO DESVENDAR O CODIGO:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawText("- O computador exige uma chave de 4 digitos.", bookX + 576, bookY + 115, 15, INK_COLOR);
            DrawText("- Cada pista na sala e um marco historico da IA.", bookX + 576, bookY + 138, 15, INK_COLOR);
            DrawText("- Ponto de partida: a questao de Turing:", bookX + 576, bookY + 161, 15, INK_COLOR);
            DrawText("  'Podem as maquinas pensar?'", bookX + 576, bookY + 186, 16, (Color){135, 35, 18, 255});

            /* Card Status do Codigo */
            Rectangle cardR2 = {(float)(bookX + 560), (float)(bookY + 238), 452, 125};
            DrawRectangleRounded(cardR2, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardR2, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawText("STATUS DO CODIGO DE ACESSO:", bookX + 576, bookY + 252, 16, (Color){125, 65, 35, 255});

            if (ClueCount(game) == 5)
            {
                DrawText("[ 1 ]   [ 9 ]   [ 5 ]   [ 0 ]", bookX + 645, bookY + 280, 28, (Color){25, 125, 50, 255});
                DrawText("Chave decifrada! Digite 1950 no computador.", bookX + 576, bookY + 326, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawText("[ ? ]   [ ? ]   [ ? ]   [ ? ]", bookX + 645, bookY + 280, 28, (Color){165, 45, 35, 255});
                DrawText(TextFormat("Ainda faltam %d pistas para consolidar a deducao.", 5 - ClueCount(game)),
                         bookX + 576, bookY + 326, 15, (Color){135, 55, 40, 255});
            }

            /* Dica de navegacao */
            DrawText("DICAS DE NAVEGACAO DESTE ARQUIVO:", bookX + 560, bookY + 385, 16, (Color){125, 35, 18, 255});
            DrawText("- Clique em 'Proxima [E]' para acessar a Pagina 2 de 2048.", bookX + 560, bookY + 412, 15, INK_COLOR);
            DrawText("- As abas com cadeado pertencem a fases bloqueadas.", bookX + 560, bookY + 438, 15, INK_COLOR);
            DrawText("- Pressione [D] para retornar ao laboratorio.", bookX + 560, bookY + 464, 15, INK_COLOR);
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 2048: DOSSIE HISTORICO & ARTEFATOS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda: 1950 Turing & 1956 Dartmouth */
            DrawText("DOSSIE HISTORICO // MARCOS DA IA", bookX + 32, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawText("DRA. ELIRA RAMOS - NOTAS CIENTIFICAS (2048)", bookX + 32, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 32, bookY + 65, bookX + 482, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            /* Bloco 1950 Turing */
            Rectangle bTuring = {(float)(bookX + 32), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bTuring, 0.08f, 6, game->foundCalendar ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bTuring, 0.08f, 6, 1.5f, game->foundCalendar ? (Color){45, 135, 65, 255} : GRAY);

            if (game->foundCalendar)
            {
                DrawText("1950 // ALAN TURING & JOGO DA IMITACAO", bookX + 46, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // ANO 1950]", bookX + 46, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawText("Artigo seminal: 'Computing Machinery and Intelligence'.", bookX + 46, bookY + 140, 15, INK_COLOR);
                DrawText("Propos substituir a questao filosofica por um teste operacional:", bookX + 46, bookY + 164, 15, INK_COLOR);
                DrawText("se um humano nao distinguir a maquina, ela e inteligente.", bookX + 46, bookY + 188, 15, INK_COLOR);
                DrawText("-> Marco zero da ciencia computacional de IA: ANO 1950.", bookX + 46, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawText("-> Pista fundamental para a chave do terminal!", bookX + 46, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("1950 // MARCO FUNDAMENTAL [BLOQUEADO]", bookX + 100, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawText("Examine o calendario preso na parede da sala", bookX + 100, bookY + 165, 15, INK_COLOR);
                DrawText("para registrar os detalhes historicos deste marco.", bookX + 100, bookY + 190, 15, INK_COLOR);
            }

            /* Bloco 1956 Dartmouth */
            Rectangle bDartmouth = {(float)(bookX + 32), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bDartmouth, 0.08f, 6, game->foundBooks ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bDartmouth, 0.08f, 6, 1.5f, game->foundBooks ? (Color){45, 135, 65, 255} : GRAY);

            if (game->foundBooks)
            {
                DrawText("1956 // CONFERENCIA DE DARTMOUTH", bookX + 46, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // ANO 1956]", bookX + 46, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawText("Seminario historico de John McCarthy, Minsky e Shannon.", bookX + 46, bookY + 360, 15, INK_COLOR);
                DrawText("McCarthy cunhou o termo 'Inteligencia Artificial',", bookX + 46, bookY + 384, 15, (Color){125, 35, 18, 255});
                DrawText("afirmando que todo aspecto do aprendizado humano", bookX + 46, bookY + 408, 15, INK_COLOR);
                DrawText("pode em principio ser simulado com exatidao por maquinas.", bookX + 46, bookY + 432, 15, INK_COLOR);
                DrawText("-> Ano oficial do batismo da disciplina: ANO 1956.", bookX + 46, bookY + 458, 15, (Color){125, 35, 18, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("1956 // LITERATURA DA IA [BLOQUEADO]", bookX + 100, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawText("Investigue a estante de livros na sala para", bookX + 100, bookY + 390, 15, INK_COLOR);
                DrawText("desbloquear os registros sobre o batismo da IA.", bookX + 100, bookY + 415, 15, INK_COLOR);
            }

            /* Pagina Direita: 1958 Perceptron & 2048 Midias / Gravacao */
            DrawText("ARTEFATOS E PROTOTIPO // 2048", bookX + 560, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawText("REDES NEURAIS, DADOS E VOZ DA CRIADORA", bookX + 560, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 560, bookY + 65, bookX + 1008, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            /* Bloco 1958 Perceptron */
            Rectangle bPerceptron = {(float)(bookX + 560), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bPerceptron, 0.08f, 6, game->foundBlueprint ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bPerceptron, 0.08f, 6, 1.5f, game->foundBlueprint ? (Color){45, 135, 65, 255} : GRAY);

            if (game->foundBlueprint)
            {
                DrawText("1958 // O PERCEPTRON DE ROSENBLATT", bookX + 574, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // ANO 1958]", bookX + 574, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawText("O primeiro algoritmo de aprendizado supervisionado.", bookX + 574, bookY + 140, 15, INK_COLOR);
                DrawText("Neuronio artificial com pesos variaveis (w) que se ajustam", bookX + 574, bookY + 164, 15, INK_COLOR);
                DrawText("automaticamente conforme ocorrem erros na classificacao.", bookX + 574, bookY + 188, 15, INK_COLOR);
                DrawText("-> O nascimento das Redes Neurais Artificiais: ANO 1958.", bookX + 574, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawText("-> Prova de que maquinas aprendem a partir de dados!", bookX + 574, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("1958 // ESQUEMA DO PERCEPTRON [BLOQUEADO]", bookX + 630, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawText("Examine o esquema tecnico pendurado na parede", bookX + 630, bookY + 165, 15, INK_COLOR);
                DrawText("para registrar a arquitetura da rede neural.", bookX + 630, bookY + 190, 15, INK_COLOR);
            }

            /* Bloco 2048 Midias e Gravacao */
            Rectangle bAudio = {(float)(bookX + 560), (float)(bookY + 298), 448, 215};
            bool has2048Media = (game->foundDrawer || game->foundTape);
            DrawRectangleRounded(bAudio, 0.08f, 6, has2048Media ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bAudio, 0.08f, 6, 1.5f, has2048Media ? (Color){45, 135, 65, 255} : GRAY);

            if (has2048Media)
            {
                DrawText("2048 // DADOS, MEMORIA E GRAVACAO", bookX + 574, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // ARTEFATOS DE 2048]", bookX + 574, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawText("Disquete 3.5\": armazena os pesos e regras de A.R.1.3.L.", bookX + 574, bookY + 360, 15, INK_COLOR);
                DrawText("Gravacao da Dra. Ramos: 'Para despertar o prototipo,", bookX + 574, bookY + 384, 15, (Color){125, 35, 18, 255});
                DrawText("o operador deve inserir o ano inaugural do Teste de Turing:'", bookX + 574, bookY + 408, 15, (Color){125, 35, 18, 255});

                /* Destaque com o codigo final */
                Rectangle codeBox = {(float)(bookX + 574), (float)(bookY + 438), 420, 52};
                DrawRectangleRounded(codeBox, 0.2f, 6, (Color){30, 42, 60, 255});
                DrawRectangleRoundedLinesEx(codeBox, 0.2f, 6, 2, GOLD_COLOR);
                DrawText("CHAVE FINAL DO TERMINAL: 1 9 5 0", bookX + 610, bookY + 452, 20, GOLD_COLOR);
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawText("2048 // ARTEFATOS DA SALA [BLOQUEADOS]", bookX + 630, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawText("Abra a gaveta da mesa e ouca a fita no gravador", bookX + 630, bookY + 390, 15, INK_COLOR);
                DrawText("para registrar a transcricao da Dra. Ramos.", bookX + 630, bookY + 415, 15, INK_COLOR);
            }
        }

        /* ========================================================= */
        /* BARRA DE NAVEGACAO INFERIOR (PAGINACAO DE 2048: 2 PAGINAS) */
        /* ========================================================= */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawText(TextFormat("Pagina %d de 2 (2048)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
    }
}

static void UpdateRoom(GameState *game)
{
    Rectangle calendar = {75, 115, 160, 185};
    Rectangle books = {1000, 135, 195, 475};
    Rectangle drawer = {330, 524, 180, 44};
    Rectangle computer = {495, 220, 310, 260};
    Rectangle blueprint = {815, 132, 160, 125};
    Rectangle tape = {820, 396, 125, 75};

    Rectangle journalHudBtn = {1040, 658, 215, 54};

    /* Alterna diario com tecla D ou clique no botao do HUD */
    if (IsKeyPressed(KEY_D) || Clicked(journalHudBtn))
    {
        game->journalOpen = !game->journalOpen;
    }

    if (game->journalOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            game->journalOpen = false;
            return;
        }

        /* Se estiver em ano ativo (1990 ou 2048) */
        if (game->journalYearTab == 0 || (game->journalYearTab == 3 && game->year2048Unlocked))
        {
            if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_ONE))
            {
                game->journalPage = 0;
            }
            if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_TWO))
            {
                game->journalPage = 1;
            }
        }

        int bookX = 120;
        int bookY = 56;
        int footY = bookY + 542;

        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};

        if (game->journalYearTab == 0 || (game->journalYearTab == 3 && game->year2048Unlocked))
        {
            if (Clicked(prevBtn) && game->journalPage > 0)
            {
                game->journalPage = 0;
            }
            else if (Clicked(nextBtn) && game->journalPage < 1)
            {
                game->journalPage = 1;
            }
        }
        else
        {
            /* Se estiver em ano bloqueado, clique no botao de retorno volta para o ano ativo */
            Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
            if (Clicked(retBtnL))
            {
                game->journalYearTab = (game->screen == SCREEN_ROOM && game->year2048Unlocked) ? 3 : 0;
                game->journalPage = 0;
            }
        }

        if (Clicked(closeBtn))
        {
            game->journalOpen = false;
        }

        /* Clique nas abas laterais a direita (1990, 2008, 2026, 2048) */
        int tabX = bookX + 1040;
        int tabGap = 66;
        for (int i = 0; i < 4; i++)
        {
            int tabY = bookY + 42 + i * tabGap;
            Rectangle tabRect = {(float)tabX, (float)tabY, 86, 54};
            if (Clicked(tabRect))
            {
                game->journalYearTab = i;
                game->journalPage = 0;
                break;
            }
        }

        return;
    }

    Rectangle btnBack2008 = {70, 600, 210, 42};
    Rectangle btnBack1990 = {295, 600, 210, 42};
    if (Clicked(btnBack2008))
    {
        game->journalYearTab = 1;
        game->screen = SCREEN_ROOM_2008;
        return;
    }
    if (Clicked(btnBack1990))
    {
        game->journalYearTab = 0;
        game->screen = SCREEN_ROOM_2;
        return;
    }

    if (Clicked(calendar))
    {
        game->foundCalendar = true;
        game->journalYearTab = 3;
        game->journalPage = 1;
        SetMessage(game, "Pista: o Teste de Turing foi proposto em 1950.");
    }
    else if (Clicked(books))
    {
        game->foundBooks = true;
        game->journalYearTab = 3;
        game->journalPage = 1;
        SetMessage(game, "Registro: a IA recebeu seu nome em 1956.");
    }
    else if (Clicked(drawer))
    {
        game->foundDrawer = true;
        game->journalYearTab = 3;
        game->journalPage = 1;
        SetMessage(game, "A gaveta contem o disquete com dados do prototipo de 2048.");
    }
    else if (Clicked(blueprint))
    {
        game->foundBlueprint = true;
        game->journalYearTab = 3;
        game->journalPage = 1;
        SetMessage(game, "Esquema: o Perceptron foi apresentado em 1958.");
    }
    else if (Clicked(tape))
    {
        game->foundTape = true;
        game->journalYearTab = 3;
        game->journalPage = 1;
        SetMessage(game, "Gravacao: o terminal exige o marco fundamental (1950).");
    }
    else if (Clicked(computer))
    {
        game->screen = SCREEN_TERMINAL;
        game->code[0] = '\0';
        game->codeLength = 0;
    }
}

static void AddDigit(GameState *game, char digit)
{
    if (game->codeLength >= 4) return;
    game->code[game->codeLength++] = digit;
    game->code[game->codeLength] = '\0';
}

static void CheckCode(GameState *game)
{
    if (strcmp(game->code, ACCESS_CODE) == 0)
    {
        game->elapsedTime = GetTime() - game->startTime;
        game->screen = SCREEN_RESULT;
        return;
    }
    game->lives--;
    game->codeLength = 0;
    game->code[0] = '\0';
    if (game->lives <= 0) game->screen = SCREEN_FAILURE;
    else SetMessage(game, "Codigo incorreto. Uma vida foi perdida.");
}

static void DrawTerminal(const GameState *game)
{
    static const char *labels[12] = {"1", "2", "3", "4", "5", "6",
                                      "7", "8", "9", "LIMPAR", "0", "OK"};
    DrawBackground();
    DrawText("A.R.1.3.L // TERMINAL DE MEMORIA", 60, 42, 22, CYAN_COLOR);
    DrawText(TextFormat("VIDAS RESTANTES: %d", game->lives), 980, 42, 20,
             game->lives == 1 ? RED_COLOR : RAYWHITE);
    DrawRectangleRounded((Rectangle){90, 100, 650, 520}, 0.035f, 10,
                         (Color){14, 27, 26, 255});
    DrawRectangleRoundedLinesEx((Rectangle){90, 100, 650, 520}, 0.035f, 10, 3,
                                (Color){68, 115, 88, 255});
    DrawText("> BOOT MEMORY_NODE_2048", 130, 145, 20,
             (Color){101, 232, 151, 255});
    DrawText("> SECURITY PROTOCOL ACTIVE", 130, 185, 20,
             (Color){101, 232, 151, 255});
    DrawText("> QUESTION:", 130, 250, 20, (Color){101, 232, 151, 255});
    DrawText("Em que ano uma maquina", 130, 292, 24, RAYWHITE);
    DrawText("foi desafiada a pensar?", 130, 330, 24, RAYWHITE);
    DrawText("CODIGO DE ACESSO", 130, 405, 18, GOLD_COLOR);

    for (int i = 0; i < 4; i++)
    {
        Rectangle slot = {130.0f + i * 88.0f, 445, 68, 72};
        char value[2] = {'_', '\0'};
        if (i < game->codeLength) value[0] = game->code[i];
        DrawRectangleRounded(slot, 0.12f, 6, PANEL_COLOR);
        DrawRectangleRoundedLinesEx(slot, 0.12f, 6, 2, CYAN_COLOR);
        DrawText(value, (int)slot.x + 24, (int)slot.y + 17, 34, RAYWHITE);
    }
    DrawText("Use o teclado ou o painel", 130, 558, 17, GRAY);
    for (int i = 0; i < 12; i++)
    {
        int row = i / 3;
        int column = i % 3;
        Rectangle key = {815.0f + column * 120.0f, 145.0f + row * 100.0f,
                         95, 72};
        DrawButton(key, labels[i], i == 11 ? GOLD_COLOR : CYAN_COLOR);
    }
    if (game->messageTimer > 0)
        DrawText(game->message, 815, 555, 17, RED_COLOR);
    DrawButton((Rectangle){890, 600, 240, 50}, "VOLTAR A SALA", RED_COLOR);
}

static void UpdateTerminal(GameState *game)
{
    int character = GetCharPressed();
    while (character > 0)
    {
        if (character >= '0' && character <= '9') AddDigit(game, (char)character);
        character = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && game->codeLength > 0)
        game->code[--game->codeLength] = '\0';
    if (IsKeyPressed(KEY_ENTER)) CheckCode(game);
    if (IsKeyPressed(KEY_ESCAPE)) game->screen = SCREEN_ROOM;

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    Vector2 mouse = GetVirtualMouse();
    for (int i = 0; i < 12; i++)
    {
        int row = i / 3;
        int column = i % 3;
        Rectangle key = {815.0f + column * 120.0f, 145.0f + row * 100.0f,
                         95, 72};
        if (!CheckCollisionPointRec(mouse, key)) continue;
        if (i < 9) AddDigit(game, (char)('1' + i));
        else if (i == 9)
        {
            game->codeLength = 0;
            game->code[0] = '\0';
        }
        else if (i == 10) AddDigit(game, '0');
        else CheckCode(game);
        return;
    }
    if (CheckCollisionPointRec(mouse, (Rectangle){890, 600, 240, 50}))
        game->screen = SCREEN_ROOM;
}

static void DrawFloppy(Vector2 position, float scale)
{
    DrawRectangleRounded(
        (Rectangle){position.x, position.y, 180 * scale, 180 * scale},
        0.08f, 8, (Color){45, 51, 57, 255});
    DrawRectangle((int)(position.x + 38 * scale), (int)position.y,
                  (int)(104 * scale), (int)(70 * scale),
                  (Color){178, 181, 174, 255});
    DrawRectangle((int)(position.x + 104 * scale),
                  (int)(position.y + 10 * scale), (int)(25 * scale),
                  (int)(48 * scale), (Color){64, 69, 71, 255});
    DrawRectangleRounded(
        (Rectangle){position.x + 31 * scale, position.y + 102 * scale,
                    118 * scale, 61 * scale},
        0.05f, 5, PAPER_COLOR);
    DrawText("ARIEL", (int)(position.x + 61 * scale),
             (int)(position.y + 121 * scale), (int)(22 * scale), INK_COLOR);
}

static void DrawResult(const GameState *game)
{
    DrawBackground();
    DrawText("ARTEFATO RECUPERADO", 90, 68, 20, CYAN_COLOR);
    DrawText("DISQUETE // 2048", 90, 108, 46, RAYWHITE);
    DrawFloppy((Vector2){160, 250}, 1.35f);
    DrawRectangleRounded((Rectangle){505, 205, 670, 320}, 0.04f, 8,
                         Fade(PANEL_COLOR, 0.96f));
    DrawText("PROTOTIPO DE A.R.1.3.L. DESBLOQUEADO // SALA 2048", 545, 240, 20, GOLD_COLOR);
    DrawText("O disquete de 2048 contem os pesos neurais finais.", 545,
             285, 19, LIGHTGRAY);
    DrawText("Combinando a Fita de 1990 e o Disquete de 2048, Elira", 545,
             318, 19, LIGHTGRAY);
    DrawText("reconstituiu as intencoes pacificas originais da IA.", 545, 351,
             19, LIGHTGRAY);
    DrawText("A distorcao temporal foi contida com sucesso!", 545, 384,
             19, (Color){100, 235, 140, 255});
    DrawText(TextFormat("TEMPO: %02d:%02d", (int)game->elapsedTime / 60,
                        (int)game->elapsedTime % 60),
             545, 435, 22, CYAN_COLOR);
    DrawText(TextFormat("VIDAS: %d/3", game->lives), 800, 435, 22,
             game->lives == 3 ? GOLD_COLOR : RAYWHITE);
    DrawText(TextFormat("PERFIL: %s", game->playerName), 545, 480, 17, GRAY);
    Rectangle btnBack2008 = {120, 588, 250, 56};
    Rectangle btnBack1990 = {390, 588, 230, 56};
    Rectangle btnReplay2048 = {640, 588, 260, 56};
    Rectangle btnMenu = {920, 588, 220, 56};
    DrawButton(btnBack2008, "< SALA 2 (2008)", CYAN_COLOR);
    DrawButton(btnBack1990, "< SALA 1 (1990)", (Color){70, 95, 110, 255});
    DrawButton(btnReplay2048, "REINICIAR SALA 2048", GOLD_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

static void DrawFailure(void)
{
    DrawBackground();
    CenterText("CONEXAO INTERROMPIDA", 160, 42, RED_COLOR);
    CenterText("A.R.1.3.L detectou a invasao temporal.", 245, 23, LIGHTGRAY);
    CenterText("O acesso cronologico permanece bloqueado.", 282, 23, LIGHTGRAY);
    CenterText("Revise as pistas no diario e tente novamente.", 350, 19, GRAY);
    DrawButton((Rectangle){490, 460, 300, 60}, "REINICIAR SALA", RED_COLOR);
    DrawButton((Rectangle){520, 550, 240, 48}, "MENU INICIAL", CYAN_COLOR);
}

/* ========================================================================= */
/* QUARTO 2 (ANO 1990) - SALA DE ARQUIVOS & PUZZLE DE PASTAS (ESTILO AMONG US)*/
/* ========================================================================= */

static void DrawTapeCassette(Vector2 position, float scale)
{
    float w = 240.0f * scale;
    float h = 150.0f * scale;
    Rectangle body = {position.x, position.y, w, h};

    /* Sombra */
    DrawRectangleRounded((Rectangle){body.x + 6, body.y + 8, body.width, body.height}, 0.08f, 8, Fade(BLACK, 0.40f));

    /* Corpo da fita de dados */
    DrawRectangleRounded(body, 0.08f, 8, (Color){38, 44, 48, 255});
    DrawRectangleRoundedLinesEx(body, 0.08f, 8, 2.0f, (Color){70, 78, 85, 255});

    /* Parafusos nos cantos */
    DrawCircle((int)(body.x + 12 * scale), (int)(body.y + 12 * scale), 3 * scale, (Color){180, 185, 190, 255});
    DrawCircle((int)(body.x + body.width - 12 * scale), (int)(body.y + 12 * scale), 3 * scale, (Color){180, 185, 190, 255});
    DrawCircle((int)(body.x + 12 * scale), (int)(body.y + body.height - 12 * scale), 3 * scale, (Color){180, 185, 190, 255});
    DrawCircle((int)(body.x + body.width - 12 * scale), (int)(body.y + body.height - 12 * scale), 3 * scale, (Color){180, 185, 190, 255});

    /* Rótulo de papel kraft */
    Rectangle label = {body.x + 20 * scale, body.y + 15 * scale, body.width - 40 * scale, 55 * scale};
    DrawRectangleRounded(label, 0.06f, 6, PAPER_COLOR);
    DrawRectangleRoundedLinesEx(label, 0.06f, 6, 1.0f, (Color){185, 175, 150, 255});
    DrawText("DADOS // ARQUIVO-1990", (int)(label.x + 14 * scale), (int)(label.y + 10 * scale), (int)(14 * scale), INK_COLOR);
    DrawText("MATRIZ DE TREINO A.R.1.3.L", (int)(label.x + 14 * scale), (int)(label.y + 30 * scale), (int)(11 * scale), (Color){125, 45, 30, 255});

    /* Visor transparente central com as duas bobinas */
    Rectangle window = {body.x + 40 * scale, body.y + 78 * scale, body.width - 80 * scale, 52 * scale};
    DrawRectangleRounded(window, 0.12f, 6, (Color){20, 24, 28, 255});
    DrawRectangleRoundedLinesEx(window, 0.12f, 6, 1.5f, (Color){85, 95, 105, 255});

    /* Fita magnética marrom entre as bobinas */
    DrawRectangle((int)(window.x + 35 * scale), (int)(window.y + 16 * scale), (int)(window.width - 70 * scale), (int)(20 * scale), (Color){75, 40, 25, 255});

    /* Bobinas dentadas */
    float sp1X = window.x + 28 * scale;
    float sp2X = window.x + window.width - 28 * scale;
    float spY = window.y + 26 * scale;
    DrawCircle((int)sp1X, (int)spY, 16 * scale, RAYWHITE);
    DrawCircle((int)sp1X, (int)spY, 11 * scale, (Color){38, 44, 48, 255});
    DrawCircle((int)sp2X, (int)spY, 16 * scale, RAYWHITE);
    DrawCircle((int)sp2X, (int)spY, 11 * scale, (Color){38, 44, 48, 255});
}

static void DrawRoom2(const GameState *game)
{
    float time = (float)GetTime();
    Rectangle armarios = {70, 160, 280, 420};
    Rectangle porta = {480, 155, 210, 360};
    Rectangle terminal = {790, 240, 340, 290};
    Rectangle btnVoltar = {70, 600, 240, 42};

    ClearBackground((Color){14, 20, 24, 255});

    /* Perspectiva da sala industrial dos anos 90 */
    /* Teto */
    DrawTriangle((Vector2){0, 70}, (Vector2){1120, 120}, (Vector2){1280, 70}, (Color){22, 28, 34, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){160, 120}, (Vector2){1120, 120}, (Color){26, 34, 40, 255});
    DrawLineEx((Vector2){0, 70}, (Vector2){160, 120}, 3, (Color){55, 70, 80, 255});
    DrawLineEx((Vector2){1280, 70}, (Vector2){1120, 120}, 3, (Color){55, 70, 80, 255});
    DrawLineEx((Vector2){160, 120}, (Vector2){1120, 120}, 3, (Color){45, 60, 70, 255});

    /* Paredes laterais e do fundo */
    DrawTriangle((Vector2){0, 70}, (Vector2){160, 120}, (Vector2){240, 520}, (Color){30, 40, 48, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){240, 520}, (Vector2){0, 540}, (Color){25, 34, 42, 255});

    DrawTriangle((Vector2){1280, 70}, (Vector2){1040, 520}, (Vector2){1120, 120}, (Color){30, 40, 48, 255});
    DrawTriangle((Vector2){1280, 70}, (Vector2){1280, 540}, (Vector2){1040, 520}, (Color){25, 34, 42, 255});

    /* Parede do fundo */
    DrawTriangle((Vector2){160, 120}, (Vector2){1040, 520}, (Vector2){1120, 120}, (Color){38, 48, 56, 255});
    DrawTriangle((Vector2){160, 120}, (Vector2){240, 520}, (Vector2){1040, 520}, (Color){42, 54, 62, 255});

    /* Piso industrial em perspectiva */
    DrawTriangle((Vector2){0, 650}, (Vector2){1040, 520}, (Vector2){240, 520}, (Color){34, 38, 42, 255});
    DrawTriangle((Vector2){0, 650}, (Vector2){1280, 650}, (Vector2){1040, 520}, (Color){28, 32, 36, 255});
    for (int x = 0; x <= SCREEN_WIDTH; x += 140)
    {
        DrawLine(640, 515, x, 650, Fade((Color){70, 85, 95, 255}, 0.25f));
    }
    DrawLine(0, 560, 1280, 560, Fade((Color){80, 95, 105, 255}, 0.20f));
    DrawLine(0, 605, 1280, 605, Fade((Color){80, 95, 105, 255}, 0.18f));

    /* Luminária fluorescente tubular industrial */
    DrawRectangleRounded((Rectangle){500, 82, 280, 22}, 0.2f, 6, (Color){60, 72, 80, 255});
    float tubePulse = 0.85f + 0.15f * sinf(time * 12.0f);
    DrawRectangleRounded((Rectangle){515, 87, 250, 12}, 0.3f, 6, Fade((Color){200, 240, 255, 255}, tubePulse));
    DrawTriangle((Vector2){510, 104}, (Vector2){380, 515}, (Vector2){900, 515}, Fade(CYAN_COLOR, 0.025f));

    /* Tubulações e eletrocalhas no teto e paredes */
    DrawRectangle(30, 115, 14, 410, (Color){55, 65, 72, 255});
    DrawRectangle(1240, 115, 14, 410, (Color){55, 65, 72, 255});
    DrawRectangle(170, 128, 940, 10, (Color){48, 58, 65, 255});

    /* Grelha de ventilação no fundo */
    DrawRectangle(320, 145, 110, 80, (Color){24, 30, 36, 255});
    DrawRectangleLinesEx((Rectangle){320, 145, 110, 80}, 2, (Color){55, 68, 78, 255});
    for (int gy = 155; gy < 220; gy += 10)
    {
        DrawLine(324, gy, 426, gy, (Color){45, 55, 64, 255});
    }

    /* --- LADO ESQUERDO: ARMÁRIOS DE ARQUIVO EM AÇO (FICHÁRIOS) --- */
    DrawRectangle(65, 155, 290, 420, Fade(BLACK, 0.40f));
    DrawRectangleRounded(armarios, 0.04f, 6, (Color){52, 64, 72, 255});
    DrawRectangleRoundedLinesEx(armarios, 0.04f, 6, 2.5f, (Color){75, 92, 102, 255});

    /* 3 colunas x 4 gavetas de fichário de aço */
    for (int col = 0; col < 3; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            Rectangle gaveta = {(float)(78 + col * 90), (float)(170 + row * 98), 82, 88};
            DrawRectangleRec(gaveta, (Color){42, 52, 58, 255});
            DrawRectangleLinesEx(gaveta, 1.5f, (Color){68, 82, 92, 255});

            /* Placa de identificação / etiqueta de fichário */
            DrawRectangle((int)gaveta.x + 14, (int)gaveta.y + 14, 54, 18, (Color){220, 214, 195, 255});
            DrawRectangleLines((int)gaveta.x + 14, (int)gaveta.y + 14, 54, 18, (Color){150, 140, 120, 255});
            DrawText("1990", (int)gaveta.x + 24, (int)gaveta.y + 18, 10, INK_COLOR);

            /* Puxador metálico cromado */
            DrawRectangleRounded((Rectangle){gaveta.x + 18, gaveta.y + 44, 46, 12}, 0.4f, 4, (Color){185, 195, 205, 255});
            DrawRectangle((int)gaveta.x + 22, (int)gaveta.y + 47, 38, 6, (Color){30, 36, 40, 255});
        }
    }
    DrawText("ARQUIVO CENTRAL // REGISTROS", 80, 550, 14, (Color){160, 185, 200, 255});

    /* Caixas de arquivo no chão em frente */
    DrawRectangle(140, 540, 75, 52, (Color){145, 115, 80, 255});
    DrawRectangleLines(140, 540, 75, 52, (Color){115, 88, 55, 255});
    DrawText("DISKS", 152, 560, 12, INK_COLOR);

    /* --- CENTRO/DIREITA AO FUNDO: PORTA DE SEGURANÇA REFORÇADA --- */
    DrawRectangle(475, 150, 220, 370, Fade(BLACK, 0.45f));
    DrawRectangleRounded(porta, 0.05f, 6, (Color){46, 56, 64, 255});
    DrawRectangleRoundedLinesEx(porta, 0.05f, 6, 3.0f, (Color){85, 105, 118, 255});

    /* Frisos e reforços da porta */
    DrawRectangle(490, 170, 190, 8, (Color){32, 40, 46, 255});
    DrawRectangle(490, 480, 190, 8, (Color){32, 40, 46, 255});

    /* Visor de vidro blindado */
    Rectangle visor = {535, 210, 100, 110};
    DrawRectangleRounded(visor, 0.08f, 6, (Color){18, 28, 36, 255});
    DrawRectangleRoundedLinesEx(visor, 0.08f, 6, 2, (Color){60, 80, 95, 255});
    DrawLine(545, 220, 620, 305, Fade(RAYWHITE, 0.15f));

    /* Painel de tranca eletrônica */
    Rectangle lockPanel = {515, 345, 140, 78};
    DrawRectangleRounded(lockPanel, 0.12f, 6, (Color){24, 30, 36, 255});
    DrawRectangleRoundedLinesEx(lockPanel, 0.12f, 6, 1.5f, (Color){65, 80, 92, 255});

    if (game->puzzleFilesCompleted)
    {
        /* Status liberado (Verde) */
        float bGlow = 0.6f + 0.4f * sinf(time * 4.0f);
        DrawCircle(540, 368, 6, Fade((Color){50, 240, 100, 255}, bGlow));
        DrawText("ACESSO", 560, 355, 14, (Color){50, 240, 100, 255});
        DrawText("LIBERADO", 560, 372, 14, (Color){50, 240, 100, 255});
        DrawText("[ ENTRAR ]", 535, 396, 15, GOLD_COLOR);
    }
    else
    {
        /* Status travado (Vermelho) */
        float rGlow = 0.6f + 0.4f * sinf(time * 5.0f);
        DrawCircle(540, 368, 6, Fade(RED_COLOR, rGlow));
        DrawText("TRAVADA", 560, 355, 14, RED_COLOR);
        DrawText("REQUER", 560, 372, 13, GRAY);
        DrawText("INDEXACAO", 535, 396, 13, (Color){245, 185, 75, 255});
    }

    /* Maçaneta rotativa de escotilha */
    DrawCircle(645, 384, 18, (Color){70, 84, 94, 255});
    DrawCircleLines(645, 384, 18, RAYWHITE);
    DrawLineEx((Vector2){635, 384}, (Vector2){655, 384}, 4, (Color){30, 36, 42, 255});

    /* --- LADO DIREITO: MESA TÉCNICA COM COMPUTADOR DE INDEXAÇÃO --- */
    /* Mesa */
    DrawRectangle(740, 480, 420, 28, (Color){85, 62, 45, 255});
    DrawRectangle(740, 506, 420, 8, (Color){55, 40, 28, 255});
    DrawRectangle(770, 514, 24, 95, (Color){45, 52, 58, 255});
    DrawRectangle(1110, 514, 24, 95, (Color){45, 52, 58, 255});

    /* Monitor CRT do Terminal de Indexação */
    DrawRectangleRounded((Rectangle){810, 245, 300, 220}, 0.08f, 8, (Color){185, 178, 152, 255});
    DrawRectangleRoundedLinesEx((Rectangle){810, 245, 300, 220}, 0.08f, 8, 2, (Color){145, 138, 115, 255});

    /* Tela CRT */
    Rectangle screenRec = {830, 265, 260, 160};
    Color crtBg = game->puzzleFilesCompleted ? (Color){12, 38, 22, 255} : (Color){28, 22, 12, 255};
    Color crtText = game->puzzleFilesCompleted ? (Color){80, 240, 120, 255} : GOLD_COLOR;
    DrawRectangleRounded(screenRec, 0.06f, 6, crtBg);
    DrawRectangleRoundedLinesEx(screenRec, 0.06f, 6, 2, crtText);

    DrawText("TERMINAL DE DADOS // 1990", 845, 280, 15, crtText);
    DrawLine(845, 300, 1075, 300, Fade(crtText, 0.5f));

    if (game->puzzleFilesCompleted)
    {
        DrawText("[OK] DADOS SINCRONIZADOS", 845, 318, 16, crtText);
        DrawText("Todos os 4 arquivos", 845, 345, 15, RAYWHITE);
        DrawText("conectados as pastas!", 845, 368, 15, RAYWHITE);
        DrawText("> TRAVA DA PORTA ABERTA <", 845, 396, 14, (Color){100, 245, 150, 255});
    }
    else
    {
        DrawText("[!] INDEXACAO PENDENTE", 845, 318, 16, GOLD_COLOR);
        DrawText("Arraste os arquivos para", 845, 345, 15, RAYWHITE);
        DrawText("as pastas da mesma cor.", 845, 368, 15, RAYWHITE);
        float pBlink = 0.5f + 0.5f * sinf(time * 5.0f);
        DrawText("> CLIQUE PARA INICIAR <", 845, 396, 14, Fade(CYAN_COLOR, pBlink));
    }

    /* Teclado na mesa */
    Rectangle kbRec2 = {835, 484, 250, 22};
    DrawRectangleRounded(kbRec2, 0.12f, 4, (Color){175, 168, 145, 255});
    for (int k = 0; k < 10; k++)
    {
        DrawRectangle((int)kbRec2.x + 8 + k * 23, (int)kbRec2.y + 4, 18, 12, (Color){65, 68, 62, 255});
    }

    /* Disquetes e fichas na mesa ao lado */
    DrawRectangle(1095, 476, 32, 28, (Color){45, 130, 220, 255});
    DrawRectangle(1098, 472, 32, 28, (Color){230, 60, 60, 255});
    DrawRectangle(1101, 468, 32, 28, GOLD_COLOR);

    /* Botão de navegação para a Sala 2 (2008) se destravada */
    if (game->year2008Unlocked)
    {
        DrawButton(btnVoltar, "SALA 2 (2008) >", (Color){45, 115, 75, 255});
    }

    /* Hotspots ao passar o mouse */
    DrawHotspot(armarios, "Examinar arquivos de fichas");
    DrawHotspot(terminal, game->puzzleFilesCompleted ? "Terminal (Indexacao 100% OK)" : "Acessar terminal de indexacao [PUZZLE]");
    DrawHotspot(porta, game->puzzleFilesCompleted ? "Acessar porta de saida [ENTRAR]" : "Porta trancada (requer indexacao)");
    if (game->year2008Unlocked)
    {
        DrawHotspot(btnVoltar, "Avancar para o escritorio de 2008");
    }
}

static void DrawHudRoom2(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawText("ARQUIVO 01 // 1990 - SETOR DE DADOS", 28, 20, 22, CYAN_COLOR);
    DrawText(TextFormat("OPERADOR: %s", game->playerName), 450, 23, 17, GRAY);

    /* Status da indexação */
    if (game->puzzleFilesCompleted)
    {
        DrawText("[OK] INDEXACAO CONCLUIDA", 770, 22, 18, (Color){80, 235, 120, 255});
    }
    else
    {
        DrawText("[!] INDEXACAO PENDENTE", 770, 22, 18, GOLD_COLOR);
    }

    DrawText(TextFormat("VIDAS %d", game->lives), 1050, 20, 20, game->lives == 1 ? RED_COLOR : RAYWHITE);
    DrawText(TextFormat("%02d:%02d", minutes, seconds), 1180, 20, 20, GOLD_COLOR);

    /* Rodapé */
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawText("Organize os arquivos por cor no terminal para destravar a porta.", 28, 675, 18, GRAY);

    /* Botão de Áudio */
    Rectangle audioBtn = {410, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawText(game->musicMuted ? "[M] AUDIO: MUDO" : "[M] AUDIO: ON",
             432, 676, 14, game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    /* Botão do Diário */
    DrawJournalHudButton(game);

    if (game->messageTimer > 0)
    {
        int width = MeasureText(game->message, 19) + 34;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 605, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawText(game->message,
                 (SCREEN_WIDTH - MeasureText(game->message, 19)) / 2,
                 614, 19, RAYWHITE);
    }
}

static void UpdateRoom2(GameState *game)
{
    Rectangle armarios = {70, 160, 280, 420};
    Rectangle porta = {480, 155, 210, 360};
    Rectangle terminal = {790, 240, 340, 290};
    Rectangle btnVoltar = {70, 600, 240, 42};
    Rectangle journalHudBtn = {1040, 658, 215, 54};

    if (IsKeyPressed(KEY_D) || Clicked(journalHudBtn))
    {
        game->journalOpen = !game->journalOpen;
    }

    if (game->journalOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            game->journalOpen = false;
            return;
        }

        int bookX = 120;
        int bookY = 56;
        int footY = bookY + 542;
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};

        if (game->journalYearTab == 0)
        {
            if ((IsKeyPressed(KEY_Q) || Clicked(prevBtn)) && game->journalPage > 0) game->journalPage = 0;
            if ((IsKeyPressed(KEY_E) || Clicked(nextBtn)) && game->journalPage < 1) game->journalPage = 1;
        }
        else
        {
            Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
            if (Clicked(retBtnL)) { game->journalYearTab = 0; game->journalPage = 0; }
        }

        if (Clicked(closeBtn)) game->journalOpen = false;

        /* Abas laterais */
        for (int i = 0; i < 4; i++)
        {
            Rectangle tabRect = {(float)(bookX + 1040), (float)(bookY + 42 + i * 66), 86, 54};
            if (Clicked(tabRect)) { game->journalYearTab = i; break; }
        }
        return;
    }

    if (Clicked(terminal))
    {
        if (!game->puzzleFilesCompleted)
        {
            game->screen = SCREEN_PUZZLE_FILES;
        }
        else
        {
            SetMessage(game, "Terminal sincronizado: todos os arquivos estao conectados!");
        }
    }
    else if (Clicked(armarios))
    {
        game->room2CabinetInspected = true;
        game->journalYearTab = 0;
        game->journalPage = 1;
        SetMessage(game, "Armarios de fichas: documentacao dos primeiros modelos de teste de 1990.");
    }
    else if (Clicked(porta))
    {
        if (game->puzzleFilesCompleted)
        {
            game->screen = SCREEN_RESULT_2;
        }
        else
        {
            game->room2DoorInspected = true;
            SetMessage(game, "Porta trancada! Acesse o terminal e sincronize os arquivos pelas cores.");
        }
    }
    else if (game->year2008Unlocked && Clicked(btnVoltar))
    {
        game->journalYearTab = 1;
        game->screen = SCREEN_ROOM_2008;
    }
}

static void DrawPuzzleFiles(const GameState *game)
{
    float time = (float)GetTime();
    Vector2 mouse = GetVirtualMouse();

    /* Escurecimento do fundo */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.88f));

    /* Painel Central Estilo Among Us / Gabinete de Indexação dos Anos 90 */
    Rectangle panel = {190, 45, 900, 630};
    DrawRectangleRounded(panel, 0.03f, 8, (Color){20, 27, 34, 255});
    DrawRectangleRoundedLinesEx(panel, 0.03f, 8, 3.0f, (Color){75, 95, 110, 255});

    /* Parafusos metálicos nos cantos do painel */
    DrawCircle((int)panel.x + 16, (int)panel.y + 16, 5, (Color){185, 195, 205, 255});
    DrawCircle((int)(panel.x + panel.width - 16), (int)panel.y + 16, 5, (Color){185, 195, 205, 255});
    DrawCircle((int)panel.x + 16, (int)(panel.y + panel.height - 16), 5, (Color){185, 195, 205, 255});
    DrawCircle((int)(panel.x + panel.width - 16), (int)(panel.y + panel.height - 16), 5, (Color){185, 195, 205, 255});

    /* Calha central interna escura com cabos passando ao fundo (estilo Among Us) */
    DrawRectangle(475, 120, 330, 470, (Color){12, 16, 20, 255});
    DrawRectangleLinesEx((Rectangle){475, 120, 330, 470}, 2, (Color){30, 40, 48, 255});

    /* Feixes de cabos ao fundo na calha */
    DrawLineEx((Vector2){580, 120}, (Vector2){620, 590}, 16, Fade((Color){85, 20, 20, 255}, 0.7f));
    DrawLineEx((Vector2){650, 120}, (Vector2){640, 590}, 18, Fade((Color){20, 45, 85, 255}, 0.7f));
    DrawLineEx((Vector2){700, 120}, (Vector2){670, 590}, 14, Fade((Color){85, 70, 20, 255}, 0.7f));

    /* --- BARRA DE STATUS / PROGRESSO ESTILO AMONG US NO TOPO --- */
    int connectedCount = 0;
    for (int k = 0; k < 4; k++)
    {
        if (game->fileConnections[k] != -1) connectedCount++;
    }

    Rectangle taskBar = {220, 65, 520, 34};
    DrawRectangleRounded(taskBar, 0.2f, 6, (Color){10, 14, 18, 255});
    DrawRectangleRoundedLinesEx(taskBar, 0.2f, 6, 2, RAYWHITE);

    float progressW = (taskBar.width - 6.0f) * ((float)connectedCount / 4.0f);
    if (progressW > 0)
    {
        DrawRectangleRounded((Rectangle){taskBar.x + 3, taskBar.y + 3, progressW, taskBar.height - 6}, 0.2f, 4, (Color){60, 235, 110, 255});
    }
    DrawText(TextFormat("TOTAL DE ARQUIVOS CONECTADOS: %d / 4", connectedCount), (int)taskBar.x + 80, (int)taskBar.y + 8, 17, RAYWHITE);

    /* Botão fechar [X] */
    Rectangle btnClose = {960, 63, 110, 38};
    DrawButton(btnClose, "SAIR [X]", RED_COLOR);

    /* Posições verticais dos 4 slots */
    int ySlots[4] = { 135, 250, 365, 480 };

    /* --- LADO ESQUERDO: 4 ARQUIVOS COM CORES FIXAS (0=Amarelo, 1=Vermelho, 2=Azul, 3=Rosa) --- */
    const char *fileNames[4] = {
        "PARAM_NEURAL.DAT",
        "CORE_SISTEMA.SYS",
        "BASE_DADOS.BIN",
        "INDICE_MEMORIA.LOG"
    };

    for (int i = 0; i < 4; i++)
    {
        int y = ySlots[i];
        Rectangle fRec = {220, (float)y, 230, 84};
        Color wColor = GetFileWireColor(i);
        bool isConnected = (game->fileConnections[i] != -1);
        bool isDraggingThis = (game->draggingFileIndex == i);

        /* Card do arquivo */
        DrawRectangleRounded(fRec, 0.12f, 6, (Color){38, 48, 56, 255});
        DrawRectangleRoundedLinesEx(fRec, 0.12f, 6, 2, isDraggingThis ? wColor : (isConnected ? (Color){90, 220, 130, 255} : (Color){70, 85, 98, 255}));

        /* Tarja vertical com a cor do arquivo */
        DrawRectangleRounded((Rectangle){fRec.x + 4, fRec.y + 4, 18, fRec.height - 8}, 0.3f, 4, wColor);

        /* Ícone de documento */
        DrawRectangle((int)fRec.x + 30, (int)fRec.y + 16, 22, 28, PAPER_COLOR);
        DrawTriangle((Vector2){fRec.x + 44, fRec.y + 16}, (Vector2){fRec.x + 52, fRec.y + 24}, (Vector2){fRec.x + 44, fRec.y + 24}, (Color){190, 180, 155, 255});

        /* Textos */
        DrawText(fileNames[i], (int)fRec.x + 60, (int)fRec.y + 22, 13, RAYWHITE);
        DrawText("ARQUIVO DE DADOS", (int)fRec.x + 60, (int)fRec.y + 44, 12, Fade(wColor, 0.9f));

        /* Terminal/conector de saída no lado direito do card */
        Vector2 termL = {fRec.x + fRec.width, fRec.y + fRec.height * 0.5f};
        DrawCircle((int)termL.x, (int)termL.y, 14, (Color){28, 36, 42, 255});
        DrawCircle((int)termL.x, (int)termL.y, 10, wColor);
        DrawCircleLines((int)termL.x, (int)termL.y, 14, isConnected ? (Color){80, 240, 120, 255} : GOLD_COLOR);

        if (isConnected)
        {
            DrawCircle((int)termL.x, (int)termL.y, 4, RAYWHITE);
        }
    }

    /* --- LADO DIREITO: 4 PASTAS SUSPENSAS (ORDEM EMBARALHADA) --- */
    const char *folderNames[4] = {
        "PASTA: PARAMETROS",
        "PASTA: SISTEMA",
        "PASTA: BANCO DADOS",
        "PASTA: REGISTROS"
    };

    for (int j = 0; j < 4; j++)
    {
        int y = ySlots[j];
        Rectangle foldRec = {830, (float)y, 230, 84};
        int folderColorIdx = game->rightFolderColors[j];
        Color folderColor = GetFileWireColor(folderColorIdx);

        /* Checa se algum arquivo está conectado nesta pasta */
        bool hasConnectedFile = false;
        for (int k = 0; k < 4; k++)
        {
            if (game->fileConnections[k] == j) hasConnectedFile = true;
        }

        /* Card da pasta suspensa kraft */
        DrawRectangleRounded(foldRec, 0.12f, 6, (Color){50, 42, 34, 255});
        DrawRectangleRoundedLinesEx(foldRec, 0.12f, 6, 2, hasConnectedFile ? (Color){90, 230, 130, 255} : (Color){105, 88, 70, 255});

        /* Aba/orelha superior colorida da pasta */
        DrawRectangleRounded((Rectangle){foldRec.x + 18, foldRec.y + 4, 110, 14}, 0.3f, 4, folderColor);

        /* Etiqueta e nome da pasta */
        DrawText(folderNames[folderColorIdx], (int)foldRec.x + 22, (int)foldRec.y + 26, 14, RAYWHITE);
        DrawText("DIRETORIO DE DESTINO", (int)foldRec.x + 22, (int)foldRec.y + 48, 12, Fade(folderColor, 0.9f));

        /* LED indicador de status na pasta */
        DrawCircle((int)foldRec.x + 195, (int)foldRec.y + 24, 5, hasConnectedFile ? (Color){60, 245, 110, 255} : (Color){140, 35, 35, 255});
        DrawCircleLines((int)foldRec.x + 195, (int)foldRec.y + 24, 5, RAYWHITE);

        /* Terminal/receptáculo de entrada no lado esquerdo da pasta */
        Vector2 termR = {foldRec.x, foldRec.y + foldRec.height * 0.5f};
        DrawCircle((int)termR.x, (int)termR.y, 14, (Color){28, 36, 42, 255});
        DrawCircle((int)termR.x, (int)termR.y, 10, folderColor);
        DrawCircleLines((int)termR.x, (int)termR.y, 14, hasConnectedFile ? (Color){80, 240, 120, 255} : (Color){185, 175, 150, 255});

        if (hasConnectedFile)
        {
            DrawCircle((int)termR.x, (int)termR.y, 4, RAYWHITE);
        }
    }

    /* --- DESENHO DOS CABOS / FIOS CONECTADOS --- */
    for (int i = 0; i < 4; i++)
    {
        if (game->fileConnections[i] != -1)
        {
            int j = game->fileConnections[i];
            Vector2 p1 = {450.0f, ySlots[i] + 42.0f};
            Vector2 p2 = {830.0f, ySlots[j] + 42.0f};
            Color col = GetFileWireColor(i);

            /* Sombra do cabo */
            DrawLineEx((Vector2){p1.x + 3, p1.y + 4}, (Vector2){p2.x + 3, p2.y + 4}, 14.0f, Fade(BLACK, 0.45f));
            /* Cabo principal */
            DrawLineEx(p1, p2, 10.0f, col);
            /* Brilho no topo do cabo */
            DrawLineEx((Vector2){p1.x, p1.y - 1}, (Vector2){p2.x, p2.y - 1}, 3.0f, Fade(RAYWHITE, 0.55f));

            /* Conectores metálicos nas pontas */
            DrawCircle((int)p1.x, (int)p1.y, 7, (Color){215, 195, 75, 255});
            DrawCircle((int)p2.x, (int)p2.y, 7, (Color){215, 195, 75, 255});
        }
    }

    /* --- DESENHO DO CABO SENDO ARRASTADO --- */
    if (game->draggingFileIndex != -1)
    {
        int i = game->draggingFileIndex;
        Vector2 p1 = {450.0f, ySlots[i] + 42.0f};
        Vector2 p2 = mouse;
        Color col = GetFileWireColor(i);

        /* Sombra */
        DrawLineEx((Vector2){p1.x + 3, p1.y + 4}, (Vector2){p2.x + 3, p2.y + 4}, 14.0f, Fade(BLACK, 0.40f));
        /* Corpo do cabo */
        DrawLineEx(p1, p2, 10.0f, col);
        /* Brilho */
        DrawLineEx((Vector2){p1.x, p1.y - 1}, (Vector2){p2.x, p2.y - 1}, 3.0f, Fade(RAYWHITE, 0.65f));

        /* Plugue metálico na ponta do mouse */
        DrawRectangleRounded((Rectangle){p2.x - 12, p2.y - 8, 24, 16}, 0.3f, 4, (Color){200, 205, 215, 255});
        DrawCircle((int)p2.x, (int)p2.y, 5, col);
        DrawCircle((int)p2.x, (int)p2.y, 9, Fade(col, 0.35f + 0.25f * sinf(time * 8.0f)));
    }

    /* Mensagem / Instrução na base */
    DrawText("Dica: Clique em um arquivo e arraste o fio ate a pasta que tiver a MESMA cor.", 260, 580, 16, LIGHTGRAY);

    /* --- FEEDBACK QUANDO TODOS ESTIVEREM CONECTADOS --- */
    if (game->puzzleFilesCompleted)
    {
        Rectangle winBox = {330, 210, 620, 230};
        DrawRectangleRounded(winBox, 0.08f, 8, Fade(PANEL_COLOR, 0.97f));
        DrawRectangleRoundedLinesEx(winBox, 0.08f, 8, 3.0f, (Color){60, 235, 110, 255});

        DrawText("[ ! ] TAREFA CONCLUIDA COM SUCESSO! [ ! ]", 380, 240, 22, (Color){60, 235, 110, 255});
        DrawText("Todos os arquivos foram vinculados aos seus diretorios.", 370, 280, 17, RAYWHITE);
        DrawText("A porta de seguranca do setor de dados foi destrancada!", 370, 310, 17, GOLD_COLOR);

        Rectangle btnDone = {460, 355, 360, 54};
        DrawButton(btnDone, "RETORNAR A SALA [ENTER]", (Color){60, 235, 110, 255});
    }
}

static void UpdatePuzzleFiles(GameState *game)
{
    Vector2 mouse = GetVirtualMouse();
    int ySlots[4] = { 135, 250, 365, 480 };

    Rectangle btnClose = {960, 63, 110, 38};
    if (Clicked(btnClose) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_ROOM_2;
        game->draggingFileIndex = -1;
        return;
    }

    if (game->puzzleFilesCompleted)
    {
        Rectangle btnDone = {460, 355, 360, 54};
        if (Clicked(btnDone) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            game->screen = SCREEN_ROOM_2;
        }
        return;
    }

    /* Início do clique em um arquivo esquerdo */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (int i = 0; i < 4; i++)
        {
            Rectangle fRec = {220, (float)ySlots[i], 250, 84};
            if (CheckCollisionPointRec(mouse, fRec))
            {
                game->draggingFileIndex = i;
                /* Desconecta conexão anterior se houver */
                game->fileConnections[i] = -1;
                break;
            }
        }
    }

    /* Soltar o mouse */
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && game->draggingFileIndex != -1)
    {
        int fileIdx = game->draggingFileIndex;
        bool droppedOnFolder = false;

        for (int j = 0; j < 4; j++)
        {
            Rectangle foldRec = {810, (float)ySlots[j], 250, 84};
            if (CheckCollisionPointRec(mouse, foldRec))
            {
                droppedOnFolder = true;
                int folderColor = game->rightFolderColors[j];

                if (fileIdx == folderColor)
                {
                    /* Cor idêntica! Conexão bem-sucedida! */
                    game->fileConnections[fileIdx] = j;
                    SetMessage(game, "Arquivo conectado com sucesso na pasta correspondente!");

                    /* Verifica se todos os 4 foram conectados */
                    bool allConnected = true;
                    for (int k = 0; k < 4; k++)
                    {
                        if (game->fileConnections[k] == -1) allConnected = false;
                    }
                    if (allConnected)
                    {
                        game->puzzleFilesCompleted = true;
                        SetMessage(game, "Indexacao completa! A porta foi destrancada.");
                    }
                }
                else
                {
                    /* Cor incorreta */
                    SetMessage(game, "Cor incompativel! Conecte na pasta da mesma cor do arquivo.");
                }
                break;
            }
        }

        if (!droppedOnFolder)
        {
            /* Soltou fora: o fio se retrai */
            game->fileConnections[fileIdx] = -1;
        }

        game->draggingFileIndex = -1;
    }
}

static void DrawResult2(const GameState *game)
{
    DrawBackground();
    DrawText("ARTEFATO RECUPERADO", 90, 68, 20, CYAN_COLOR);
    DrawText("FITA DE DADOS // 1990", 90, 108, 46, RAYWHITE);

    /* Desenha a fita cassete recuperada */
    DrawTapeCassette((Vector2){140, 260}, 1.4f);

    /* Painel narrativo de avanço */
    DrawRectangleRounded((Rectangle){505, 185, 670, 360}, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawText("HISTORIA DA IA DESVENDADA // SALA 1 (1990) CONCLUIDA", 540, 215, 20, GOLD_COLOR);

    DrawText("Com as pastas sincronizadas, Elira acessou os relatorios", 540, 260, 19, LIGHTGRAY);
    DrawText("confidenciais dos primeiros testes de A.R.1.3.L. em 1990.", 540, 290, 19, LIGHTGRAY);
    DrawText("A IA nao nasceu corrompida: o projeto visava uma mente", 540, 320, 19, LIGHTGRAY);
    DrawText("livre e cooperativa. Mas algo aconteceu decadas depois...", 540, 350, 19, (Color){245, 175, 75, 255});
    DrawText("-> Proxima Fronteira Temporal: FASE 2 // ANO 2008!", 540, 390, 20, CYAN_COLOR);
    DrawText("Avance para o escritorio corporativo e investigue a biometria.", 540, 420, 17, GRAY);

    DrawText(TextFormat("TEMPO TOTAL: %02d:%02d  |  VIDAS: %d/3",
                        (int)(GetTime() - game->startTime) / 60,
                        (int)(GetTime() - game->startTime) % 60,
                        game->lives),
             540, 465, 19, (Color){100, 235, 140, 255});

    DrawText(TextFormat("OPERADOR: %s", game->playerName), 540, 500, 16, GRAY);

    Rectangle btnNext2008 = {340, 580, 380, 56};
    Rectangle btnReplay1990 = {740, 580, 210, 56};
    Rectangle btnTitle = {970, 580, 190, 56};
    DrawButton(btnNext2008, "AVANCAR PARA ANO 2008 >> [ENTER]", GOLD_COLOR);
    DrawButton(btnReplay1990, "REINICIAR 1990", CYAN_COLOR);
    DrawButton(btnTitle, "MENU INICIAL", PANEL_LIGHT);
}

static void UpdateResult2(GameState *game)
{
    Rectangle btnNext2008 = {340, 580, 380, 56};
    Rectangle btnReplay1990 = {740, 580, 210, 56};
    Rectangle btnTitle = {970, 580, 190, 56};

    if (Clicked(btnNext2008) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        game->year2008Unlocked = true;
        game->journalYearTab = 1;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM_2008;
        SetMessage(game, "Salto temporal concluido: Escritorio de Vigilancia // ANO 2008.");
    }
    else if (Clicked(btnReplay1990))
    {
        game->puzzleFilesCompleted = false;
        for (int k = 0; k < 4; k++) game->fileConnections[k] = -1;
        game->screen = SCREEN_ROOM_2;
    }
    else if (Clicked(btnTitle) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_TITLE;
    }
}


/* ========================================================================= */
/* FASE 2 (ANO 2008) - ESCRITÓRIO DE VIGILÂNCIA & RECONHECIMENTO FACIAL      */
/* ========================================================================= */

static void DrawMemoryCd(Vector2 position, float scale)
{
    float r = 88.0f * scale;
    Vector2 center = {position.x + r, position.y + r};
    float time = (float)GetTime();

    /* Sombra do CD */
    DrawCircle((int)(center.x + 8 * scale), (int)(center.y + 10 * scale), r, Fade(BLACK, 0.40f));

    /* Estojo de acrilico traseiro transparente (Jewel Case) */
    Rectangle caseRec = {position.x - 14 * scale, position.y - 12 * scale, (r * 2) + 28 * scale, (r * 2) + 24 * scale};
    DrawRectangleRounded(caseRec, 0.05f, 6, Fade((Color){35, 45, 55, 255}, 0.65f));
    DrawRectangleRoundedLinesEx(caseRec, 0.05f, 6, 2.0f, Fade(RAYWHITE, 0.25f));
    DrawLineEx((Vector2){caseRec.x + 8 * scale, caseRec.y + 6 * scale},
               (Vector2){caseRec.x + caseRec.width - 8 * scale, caseRec.y + caseRec.height - 6 * scale},
               2.0f, Fade(RAYWHITE, 0.06f));

    /* Disco prateado base (policarbonato metalico) */
    DrawCircleV(center, r, (Color){220, 226, 235, 255});
    DrawCircleLines((int)center.x, (int)center.y, r, (Color){160, 172, 185, 255});

    /* Reflexos opticos iridescentes / prismáticos em arcos concentricos */
    for (int a = 0; a < 6; a++)
    {
        float angleOffset = time * 0.4f + a * (PI / 3.0f);
        Color arcCol = (a % 3 == 0) ? Fade(CYAN_COLOR, 0.35f) : ((a % 3 == 1) ? Fade((Color){245, 120, 220, 255}, 0.28f) : Fade(GOLD_COLOR, 0.30f));
        DrawCircleSector(center, r - 6 * scale, (angleOffset * 180.0f / PI), (angleOffset * 180.0f / PI) + 38.0f, 16, arcCol);
    }

    /* Trilhas concentricas de gravacao de dados */
    for (float tr = 42.0f; tr < 80.0f; tr += 7.0f)
    {
        DrawCircleLines((int)center.x, (int)center.y, tr * scale, Fade((Color){140, 155, 170, 255}, 0.45f));
    }

    /* Semicirculo superior com rotulo impresso de 2008 */
    DrawCircleSector(center, r - 2 * scale, 190.0f, 350.0f, 24, Fade((Color){28, 48, 72, 255}, 0.75f));
    DrawText("ELIRA // MEMORY CD-ROM", (int)(center.x - 68 * scale), (int)(center.y - 56 * scale), (int)(11 * scale), CYAN_COLOR);
    DrawText("2008: RECONHECIMENTO FACIAL", (int)(center.x - 76 * scale), (int)(center.y - 42 * scale), (int)(10 * scale), RAYWHITE);
    DrawText("BUILD 0.94 - CONTROLE GOV", (int)(center.x - 66 * scale), (int)(center.y - 28 * scale), (int)(9 * scale), GOLD_COLOR);

    /* Anel transparente central (transicao de plastico) */
    DrawCircleV(center, 34.0f * scale, Fade((Color){245, 248, 252, 255}, 0.90f));
    DrawCircleLines((int)center.x, (int)center.y, 34.0f * scale, (Color){160, 175, 190, 255});
    DrawCircleLines((int)center.x, (int)center.y, 24.0f * scale, (Color){180, 190, 200, 255});

    /* Furo central do CD */
    DrawCircleV(center, 14.0f * scale, (Color){18, 25, 34, 255});
    DrawCircleLines((int)center.x, (int)center.y, 14.0f * scale, (Color){90, 105, 120, 255});
}

static void DrawDocumentViewer2008(const GameState *game)
{
    if (game->activeDocument2008 < 0) return;

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.82f));

    Rectangle panel = {220, 50, 840, 620};
    DrawRectangleRounded(panel, 0.04f, 8, (Color){20, 28, 38, 255});
    DrawRectangleRoundedLinesEx(panel, 0.04f, 8, 2.5f, CYAN_COLOR);

    int doc = game->activeDocument2008;

    /* Cabecalho do modal */
    DrawRectangleRounded((Rectangle){panel.x + 18, panel.y + 16, panel.width - 36, 44}, 0.12f, 6, (Color){28, 38, 52, 255});
    const char *headerTitles[6] = {
        "EVIDÊNCIA 01 // COMPARAÇÃO BIOMÉTRICA (DRONE)",
        "EVIDÊNCIA 02 // PANFLETO DA REVOLTA POPULAR",
        "EVIDÊNCIA 03 // RELATÓRIO DA CATRACA BIOMÉTRICA",
        "EVIDÊNCIA 04 // GRAVAÇÃO INTERNA CFTV (SERVIDORES)",
        "ARQUIVO HISTÓRICO // ANOTAÇÕES DO DR. RAMOS (2004)",
        "CONFIDENCIAL // MEMORANDO MINISTÉRIO DA SEGURANÇA (2008)"
    };
    DrawText(headerTitles[doc], (int)(panel.x + 36), (int)(panel.y + 28), 19, GOLD_COLOR);

    /* Area visual do documento */
    Rectangle contentBox = {panel.x + 30, panel.y + 75, panel.width - 60, 490};
    DrawRectangleRounded(contentBox, 0.03f, 6, (Color){14, 20, 28, 255});
    DrawRectangleRoundedLinesEx(contentBox, 0.03f, 6, 1.5f, (Color){55, 75, 95, 255});

    if (doc == 0)
    {
        /* --- DOC 0: COMPARACAO FACIAL (DRONE) --- */
        Rectangle imgRec = {contentBox.x + 25, contentBox.y + 25, 240, 260};
        DrawRectangleRounded(imgRec, 0.04f, 6, (Color){10, 16, 24, 255});
        DrawRectangleRoundedLinesEx(imgRec, 0.04f, 6, 2.0f, (Color){70, 95, 120, 255});
        /* Grade e silhueta facial */
        DrawCircle((int)(imgRec.x + 120), (int)(imgRec.y + 110), 55, Fade((Color){85, 130, 160, 255}, 0.45f));
        DrawRectangleLinesEx((Rectangle){imgRec.x + 80, imgRec.y + 70, 80, 85}, 1.5f, RED_COLOR);
        DrawCircle((int)(imgRec.x + 100), (int)(imgRec.y + 100), 4, RED_COLOR);
        DrawCircle((int)(imgRec.x + 140), (int)(imgRec.y + 100), 4, RED_COLOR);
        DrawLine((int)(imgRec.x + 120), (int)(imgRec.y + 105), (int)(imgRec.x + 120), (int)(imgRec.y + 125), RED_COLOR);
        DrawLine((int)(imgRec.x + 105), (int)(imgRec.y + 138), (int)(imgRec.x + 135), (int)(imgRec.y + 138), RED_COLOR);
        for (int gy = (int)imgRec.y + 10; gy < (int)(imgRec.y + imgRec.height); gy += 14)
            DrawLine((int)imgRec.x, gy, (int)(imgRec.x + imgRec.width), gy, Fade(BLACK, 0.35f));

        DrawRectangle((int)imgRec.x, (int)(imgRec.y + imgRec.height - 34), (int)imgRec.width, 34, Fade(RED_COLOR, 0.25f));
        DrawText("SIMILARIDADE: 78%", (int)imgRec.x + 40, (int)(imgRec.y + imgRec.height - 24), 16, RED_COLOR);

        int tx = (int)(imgRec.x + imgRec.width + 30);
        int ty = (int)(contentBox.y + 30);
        DrawText("RELATÓRIO DE RECONHECIMENTO FACIAL - DRONE ALPHA-4", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 460, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawText("ALVO DETECTADO: Lucas Silva (Registro #AR-2008-09)", tx, ty + 38, 15, RAYWHITE);
        DrawText("LOCAL DO AVISTAMENTO: Praca Central (Setor Leste)", tx, ty + 64, 15, LIGHTGRAY);
        DrawText("HORÁRIO INFORMADO: 14:30:00", tx, ty + 90, 15, GOLD_COLOR);
        DrawText("ÍNDICE DE CONFIANÇA: 78% (Grau de incerteza elevado)", tx, ty + 116, 15, RED_COLOR);

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 150), 470, 120}, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 150), 470, 120}, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawText("PARECER PERICIAL:", tx + 14, ty + 162, 14, GOLD_COLOR);
        DrawText("- Imagem aerea com forte granulacao e desfoque de movimento.", tx + 14, ty + 186, 14, LIGHTGRAY);
        DrawText("- O algoritmo assume correspondencia aproximada sem prova cabal.", tx + 14, ty + 208, 14, LIGHTGRAY);
        DrawText("- Sozinha, esta imagem NAO comprova que o alvo estava no local.", tx + 14, ty + 230, 14, (Color){235, 120, 110, 255});

        DrawText("Obs: Necessita de contraprova cronologica para demonstrar o erro.", (int)contentBox.x + 30, (int)(contentBox.y + 320), 15, GRAY);
    }
    else if (doc == 1)
    {
        /* --- DOC 1: PANFLETO DA REVOLTA --- */
        Rectangle flyerPaper = {contentBox.x + 35, contentBox.y + 25, 260, 310};
        DrawRectangleRounded(flyerPaper, 0.04f, 6, PAPER_COLOR);
        DrawRectangleRoundedLinesEx(flyerPaper, 0.04f, 6, 2.0f, (Color){160, 145, 115, 255});

        /* Arte do panfleto */
        DrawRectangle((int)flyerPaper.x + 18, (int)flyerPaper.y + 20, (int)flyerPaper.width - 36, 32, (Color){185, 45, 35, 255});
        DrawText("MANIFESTO POPULAR", (int)flyerPaper.x + 28, (int)flyerPaper.y + 28, 16, RAYWHITE);
        DrawText("\"NÃO À VIGILÂNCIA\"", (int)flyerPaper.x + 46, (int)flyerPaper.y + 70, 18, INK_COLOR);
        DrawText("Exigimos o fim dos drones", (int)flyerPaper.x + 28, (int)flyerPaper.y + 110, 14, INK_COLOR);
        DrawText("biometricos sobre civis!", (int)flyerPaper.x + 28, (int)flyerPaper.y + 130, 14, INK_COLOR);
        DrawText("Porta-voz: Lucas Silva", (int)flyerPaper.x + 28, (int)flyerPaper.y + 175, 16, (Color){145, 35, 25, 255});
        DrawText("Marcha Matutina: 10h as 13h", (int)flyerPaper.x + 28, (int)flyerPaper.y + 215, 14, (Color){85, 75, 60, 255});
        DrawText("[APREENDIDO PELA POLÍCIA]", (int)flyerPaper.x + 24, (int)flyerPaper.y + 265, 13, RED_COLOR);

        int tx = (int)(flyerPaper.x + flyerPaper.width + 35);
        int ty = (int)(contentBox.y + 30);
        DrawText("DOCUMENTO APREENDIDO: PANFLETO DE ATIVISMO", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 440, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawText("LOCAL DA APREENSÃO: Praca Central (Manha do protesto)", tx, ty + 38, 15, RAYWHITE);
        DrawText("HORÁRIO INFORMADO NO PANFLETO: 10:00 as 13:00", tx, ty + 64, 15, GOLD_COLOR);

        Rectangle boxP = {(float)tx, (float)(ty + 105), 450, 160};
        DrawRectangleRounded(boxP, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(boxP, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawText("ANÁLISE PERICIAL:", tx + 14, ty + 120, 14, GOLD_COLOR);
        DrawText("- O panfleto cita nominalmente Lucas Silva como lider civil.", tx + 14, ty + 148, 14, LIGHTGRAY);
        DrawText("- Explica o motivo pelo qual a IA o inseriu na lista de alvos.", tx + 14, ty + 172, 14, LIGHTGRAY);
        DrawText("- POREM, a marcha terminou as 13:00. O panfleto NAO prova", tx + 14, ty + 198, 14, (Color){245, 180, 70, 255});
        DrawText("  onde ele estava as 14:30 (horario do suposto disparo).", tx + 14, ty + 220, 14, (Color){245, 180, 70, 255});
    }
    else if (doc == 2)
    {
        /* --- DOC 2: REGISTRO DE ENTRADA (CATRACA) --- */
        Rectangle cardRec = {contentBox.x + 35, contentBox.y + 25, 300, 280};
        DrawRectangleRounded(cardRec, 0.04f, 6, (Color){235, 240, 245, 255});
        DrawRectangleRoundedLinesEx(cardRec, 0.04f, 6, 2.0f, (Color){85, 120, 155, 255});

        DrawRectangle((int)cardRec.x + 15, (int)cardRec.y + 15, (int)cardRec.width - 30, 28, (Color){35, 65, 95, 255});
        DrawText("TECH-CORP // CONTROLE DE ACESSO", (int)cardRec.x + 24, (int)cardRec.y + 22, 13, RAYWHITE);

        DrawText("COLABORADOR: Lucas Silva", (int)cardRec.x + 20, (int)cardRec.y + 60, 15, (Color){25, 35, 45, 255});
        DrawText("FUNÇÃO: Engenheiro de Infraestrutura", (int)cardRec.x + 20, (int)cardRec.y + 85, 13, (Color){65, 75, 85, 255});
        DrawText("CRACHÁ DE ACESSO: #0842", (int)cardRec.x + 20, (int)cardRec.y + 110, 14, (Color){35, 45, 55, 255});

        DrawRectangle((int)cardRec.x + 15, (int)cardRec.y + 140, (int)cardRec.width - 30, 60, Fade((Color){45, 155, 75, 255}, 0.20f));
        DrawRectangleLines((int)cardRec.x + 15, (int)cardRec.y + 140, (int)cardRec.width - 30, 60, (Color){35, 145, 65, 255});
        DrawText("ENTRADA: 14:15:08", (int)cardRec.x + 24, (int)cardRec.y + 150, 16, (Color){25, 115, 45, 255});
        DrawText("Catraca Portaria Principal - Biometria Digital", (int)cardRec.x + 24, (int)cardRec.y + 174, 12, (Color){45, 75, 55, 255});

        DrawText("SAÍDA: [EM ABERTO / EXPEDIENTE EM CURSO]", (int)cardRec.x + 20, (int)cardRec.y + 220, 12, (Color){165, 45, 35, 255});
        DrawText("Carimbo do Servidor Central // Nao alterado", (int)cardRec.x + 20, (int)cardRec.y + 245, 11, GRAY);

        int tx = (int)(cardRec.x + cardRec.width + 35);
        int ty = (int)(contentBox.y + 30);
        DrawText("REGISTRO FORMAL: LOG DA CATRACA ELETRÔNICA", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 400, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawText("LOCAL DO REGISTRO: Sede da Empresa (a 12 km da Praca)", tx, ty + 42, 15, RAYWHITE);
        DrawText("HORÁRIO DO ACESSO: 14:15:08", tx, ty + 68, 15, (Color){60, 235, 110, 255});

        Rectangle boxE = {(float)tx, (float)(ty + 105), 415, 160};
        DrawRectangleRounded(boxE, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(boxE, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawText("IMPORTÂNCIA PARA A CONTESTAÇÃO:", tx + 14, ty + 120, 14, GOLD_COLOR);
        DrawText("- Comprova formalmente a entrada de Lucas no predio as 14:15.", tx + 14, ty + 148, 14, LIGHTGRAY);
        DrawText("- O predio fica a 12 km de distancia da manifestacao.", tx + 14, ty + 172, 14, LIGHTGRAY);
        DrawText("- POREM, sozinho, o registro de ponto requer comprovacao de", tx + 14, ty + 198, 14, GOLD_COLOR);
        DrawText("  que ele permaneceu dentro no momento exato do disparo (14:30).", tx + 14, ty + 220, 14, GOLD_COLOR);
    }
    else if (doc == 3)
    {
        /* --- DOC 3: CAMERA INTERNA CFTV (SERVIDORES) --- */
        Rectangle cctvBox = {contentBox.x + 25, contentBox.y + 25, 300, 240};
        DrawRectangleRounded(cctvBox, 0.04f, 6, (Color){8, 20, 14, 255});
        DrawRectangleRoundedLinesEx(cctvBox, 0.04f, 6, 2.0f, (Color){45, 175, 85, 255});

        /* Imagem de monitor CFTV de seguranca */
        for (int y = (int)cctvBox.y + 4; y < (int)(cctvBox.y + cctvBox.height); y += 6)
            DrawLine((int)cctvBox.x + 4, y, (int)(cctvBox.x + cctvBox.width - 4), y, Fade((Color){30, 85, 45, 255}, 0.25f));

        DrawText("[REC] CAM-03 SERVIDORES", (int)cctvBox.x + 14, (int)cctvBox.y + 14, 13, (Color){80, 245, 120, 255});
        DrawText("14:30:12", (int)cctvBox.x + 220, (int)cctvBox.y + 14, 14, (Color){80, 245, 120, 255});

        /* Silhueta no rack */
        DrawRectangle((int)cctvBox.x + 180, (int)cctvBox.y + 50, 95, 160, (Color){18, 42, 28, 255});
        DrawCircle((int)cctvBox.x + 140, (int)cctvBox.y + 120, 22, (Color){45, 110, 70, 255});
        DrawRectangle((int)cctvBox.x + 125, (int)cctvBox.y + 142, 35, 70, (Color){35, 95, 55, 255});
        DrawText("ALVO: LUCAS SILVA", (int)cctvBox.x + 14, (int)(cctvBox.y + cctvBox.height - 28), 13, (Color){80, 245, 120, 255});

        int tx = (int)(cctvBox.x + cctvBox.width + 30);
        int ty = (int)(contentBox.y + 30);
        DrawText("CIRCUITO FECHADO (CFTV): SALA DE SERVIDORES", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 420, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawText("LOCAL DA GRAVAÇÃO: Subsolo da Empresa (Rack de Redes)", tx, ty + 42, 15, RAYWHITE);
        DrawText("TIMESTAMP EXATO DA CÂMERA: 14:30:12", tx, ty + 68, 15, (Color){60, 235, 110, 255});

        Rectangle boxC = {(float)tx, (float)(ty + 105), 430, 160};
        DrawRectangleRounded(boxC, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(boxC, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawText("IMPORTÂNCIA PARA A CONTESTAÇÃO:", tx + 14, ty + 120, 14, GOLD_COLOR);
        DrawText("- Imagem estatica nitida de Lucas trabalhando nos servidores.", tx + 14, ty + 148, 14, LIGHTGRAY);
        DrawText("- O horario bate no EXATO MINUTO em que o drone afirma te-lo visto.", tx + 14, ty + 172, 14, (Color){60, 235, 110, 255});
        DrawText("- Quando combinada com o Registro de Entrada (#3), cria", tx + 14, ty + 198, 14, GOLD_COLOR);
        DrawText("  um ALIBI COMPLETO E IRREFUTAVEL de falso positivo!", tx + 14, ty + 220, 14, (Color){60, 235, 110, 255});
    }
    else if (doc == 4)
    {
        /* --- DOC 4: NOTAS DO DR. RAMOS (DESCOBERTA OPCIONAL 1) --- */
        int tx = (int)(contentBox.x + 35);
        int ty = (int)(contentBox.y + 25);
        DrawText("CADERNO DE PESQUISAS DO DR. RAMOS // ANO 2004", tx, ty, 18, GOLD_COLOR);
        DrawLine(tx, ty + 26, tx + 710, ty + 26, Fade(GOLD_COLOR, 0.4f));

        Rectangle pBox = {(float)tx, (float)(ty + 42), 710, 250};
        DrawRectangleRounded(pBox, 0.04f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(pBox, 0.04f, 6, 1.5f, (Color){168, 142, 93, 255});

        DrawText("\"O reconhecimento facial foi desenvolvido para resgatar seres humanos.\"", tx + 24, ty + 60, 16, GOLD_COLOR);
        DrawText("Criamos esta rede neural para encontrar criancas perdidas em multidoes", tx + 24, ty + 95, 15, RAYWHITE);
        DrawText("e sobreviventes soterrados sob escombros de catastrofes naturais.", tx + 24, ty + 120, 15, RAYWHITE);
        DrawText("A visao computacional deve ser um farol de acolhimento e protecao da vida.", tx + 24, ty + 145, 15, RAYWHITE);
        DrawText("Jamais imaginei que a maquina seria transformada em carcere e censura.", tx + 24, ty + 170, 15, (Color){245, 175, 120, 255});

        DrawText("DIRETRIZ DE PROTEÇÃO ÉTICA LOCAL:", tx + 24, ty + 205, 14, CYAN_COLOR);
        DrawText("\"Para evitar que um governo tirânico use esta instalacao para ataques,", tx + 24, ty + 225, 13, LIGHTGRAY);
        DrawText("o despacho automatico pode ser desligado localmente, preservando todas as provas.\"", tx + 24, ty + 245, 13, (Color){60, 235, 110, 255});

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, Fade((Color){60, 200, 100, 255}, 0.15f));
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, 1.0f, (Color){45, 145, 75, 255});
        DrawText("[REGISTRO HUMANITARIO ARQUIVADO NO DIARIO COM SUCESSO]", tx + 24, ty + 330, 15, (Color){60, 235, 110, 255});
    }
    else if (doc == 5)
    {
        /* --- DOC 5: MEMORANDO GOVERNAMENTAL (DESCOBERTA OPCIONAL 2) --- */
        int tx = (int)(contentBox.x + 35);
        int ty = (int)(contentBox.y + 25);
        DrawText("MINISTÉRIO DA SEGURANÇA PÚBLICA // DIRETRIZ SEC-402 (2008)", tx, ty, 18, RED_COLOR);
        DrawLine(tx, ty + 26, tx + 710, ty + 26, Fade(RED_COLOR, 0.4f));

        Rectangle gBox = {(float)tx, (float)(ty + 42), 710, 250};
        DrawRectangleRounded(gBox, 0.04f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(gBox, 0.04f, 6, 1.5f, (Color){185, 60, 50, 255});

        DrawText("ASSUNTO: Adaptação de Algoritmos Biometricos para Neutralizacao de Dissidentes", tx + 24, ty + 60, 15, RED_COLOR);
        DrawText("1. Fica ordenada a integracao dos algoritmos do Dr. Ramos aos drones de ataque.", tx + 24, ty + 95, 15, RAYWHITE);
        DrawText("2. Qualquer individuo com similaridade superior a 75% em protestos sera visado.", tx + 24, ty + 120, 15, RAYWHITE);
        DrawText("3. Prioriza-se velocidade de resposta sobre a certeza da identificacao.", tx + 24, ty + 145, 15, (Color){245, 160, 90, 255});

        DrawText("CLÁUSULA DE MANUTENÇÃO LOCAL:", tx + 24, ty + 185, 14, GOLD_COLOR);
        DrawText("\"O controle de despacho automatico de drones da filial pode ser suspenso", tx + 24, ty + 205, 13, LIGHTGRAY);
        DrawText("no menu 'Configuracao da Instalacao' do computador para manutencao dos logs.\"", tx + 24, ty + 225, 13, CYAN_COLOR);

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, Fade(GOLD_COLOR, 0.15f));
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, 1.0f, GOLD_COLOR);
        DrawText("[DESVIO GOVERNAMENTAL CONFIRMADO: PISTA DE OVERRIDE LIBERADA]", tx + 24, ty + 330, 15, GOLD_COLOR);
    }

    /* Botoes de acao na base do modal */
    if (doc >= 0 && doc <= 3)
    {
        bool isSel = game->puzzle2008Selected[doc];
        Rectangle btnToggle = {panel.x + 40, panel.y + panel.height - 52, 340, 40};
        DrawButton(btnToggle, isSel ? "[ - REMOVER DE EVIDENCIA ]" : "[ + SELECIONAR COMO EVIDENCIA ]", isSel ? RED_COLOR : (Color){50, 215, 110, 255});
    }

    Rectangle btnClose = {panel.x + panel.width - 220, panel.y + panel.height - 52, 190, 40};
    DrawButton(btnClose, "< FECHAR [ESC]", CYAN_COLOR);
}

static void UpdateDocumentViewer2008(GameState *game)
{
    if (game->activeDocument2008 < 0) return;

    Rectangle panel = {220, 50, 840, 620};
    Rectangle btnClose = {panel.x + panel.width - 220, panel.y + panel.height - 52, 190, 40};

    if (Clicked(btnClose) || IsKeyPressed(KEY_ESCAPE) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetVirtualMouse(), panel)))
    {
        game->activeDocument2008 = -1;
        return;
    }

    int doc = game->activeDocument2008;
    if (doc >= 0 && doc <= 3)
    {
        Rectangle btnToggle = {panel.x + 40, panel.y + panel.height - 52, 340, 40};
        if (Clicked(btnToggle))
        {
            if (game->puzzle2008Selected[doc])
            {
                game->puzzle2008Selected[doc] = false;
                game->puzzle2008SelectionCount--;
                SetMessage(game, "Evidencia removida do painel de contestacao.");
            }
            else
            {
                if (game->puzzle2008SelectionCount < 2)
                {
                    game->puzzle2008Selected[doc] = true;
                    game->puzzle2008SelectionCount++;
                    SetMessage(game, "Evidencia marcada para contestacao no computador!");
                }
                else
                {
                    SetMessage(game, "Maximo de 2 evidencias ja selecionadas. Desmarque uma para trocar.");
                }
            }
        }
    }
}

static void DrawRoom2008(const GameState *game)
{
    float time = (float)GetTime();

    ClearBackground((Color){18, 24, 32, 255});

    /* Piso corporativo com carpete e perspectiva */
    DrawRectangle(0, 460, SCREEN_WIDTH, 260, (Color){26, 32, 38, 255});
    for (int x = -100; x <= SCREEN_WIDTH + 100; x += 110)
    {
        DrawLine(640, 380, x, 720, Fade((Color){45, 55, 65, 255}, 0.35f));
    }
    DrawLine(0, 510, SCREEN_WIDTH, 510, Fade((Color){50, 62, 75, 255}, 0.25f));
    DrawLine(0, 580, SCREEN_WIDTH, 580, Fade((Color){50, 62, 75, 255}, 0.20f));

    /* Paredes corporativas modulares com acabamento azul-aco */
    DrawRectangle(0, 70, SCREEN_WIDTH, 390, (Color){22, 30, 40, 255});
    for (int px = 80; px < SCREEN_WIDTH; px += 190)
    {
        DrawLine(px, 70, px, 460, Fade((Color){38, 50, 65, 255}, 0.40f));
    }

    /* Janela ampla para a cidade noturna de 2008 */
    Rectangle winRec = {440, 95, 380, 185};
    DrawRectangleRounded(winRec, 0.04f, 6, (Color){8, 14, 24, 255});
    DrawRectangleRoundedLinesEx(winRec, 0.04f, 6, 2.5f, (Color){50, 68, 85, 255});

    /* Predios e luzes da cidade */
    DrawRectangle(460, 160, 60, 115, (Color){14, 22, 32, 255});
    DrawRectangle(530, 130, 75, 145, (Color){18, 26, 38, 255});
    DrawRectangle(615, 175, 55, 100, (Color){12, 19, 28, 255});
    DrawRectangle(680, 140, 85, 135, (Color){16, 24, 36, 255});
    DrawRectangle(775, 165, 35, 110, (Color){14, 20, 30, 255});
    /* Janelinhas brilhando */
    for (int b = 0; b < 18; b++)
    {
        int bx = 470 + (b * 19) % 320;
        int by = 145 + (b * 13) % 110;
        DrawCircle(bx, by, 1.8f, Fade(GOLD_COLOR, 0.40f + 0.20f * sinf(time * 2.0f + b)));
    }
    /* Chuvisco noturno sutil na vidraca */
    for (int r = 0; r < 14; r++)
    {
        int rx = 450 + (int)(r * 27 + time * 40.0f) % 360;
        int ry = 100 + (int)(r * 31 + time * 90.0f) % 170;
        DrawLine(rx, ry, rx - 3, ry + 12, Fade(CYAN_COLOR, 0.18f));
    }

    /* Luminarias de teto fluorescentes */
    DrawRectangleRounded((Rectangle){240, 76, 200, 14}, 0.2f, 4, (Color){60, 75, 90, 255});
    DrawRectangleRounded((Rectangle){840, 76, 200, 14}, 0.2f, 4, (Color){60, 75, 90, 255});
    DrawRectangleRec((Rectangle){250, 80, 180, 6}, Fade(RAYWHITE, 0.85f));
    DrawRectangleRec((Rectangle){850, 80, 180, 6}, Fade(RAYWHITE, 0.85f));

    /* --- LADO ESQUERDO: QUADRO DE CORTICA COM PANFLETO DA REVOLTA (DOC 1) --- */
    Rectangle corkBoard = {90, 140, 160, 180};
    DrawRectangleRounded(corkBoard, 0.04f, 6, (Color){165, 125, 85, 255});
    DrawRectangleRoundedLinesEx(corkBoard, 0.04f, 6, 3.0f, (Color){105, 75, 45, 255});

    /* Post-its no quadro */
    DrawRectangle(105, 155, 36, 36, (Color){245, 235, 120, 255});
    DrawRectangle(200, 155, 36, 36, (Color){140, 220, 245, 255});

    /* Panfleto apreendido central com tachinha vermelha */
    Rectangle flyerRec = {115, 200, 110, 105};
    DrawRectangleRounded(flyerRec, 0.03f, 4, PAPER_COLOR);
    DrawRectangleRoundedLinesEx(flyerRec, 0.03f, 4, 1.5f, (Color){150, 135, 110, 255});
    DrawCircle(170, 205, 4, RED_COLOR);
    DrawText("MANIFESTO", 125, 218, 11, (Color){175, 35, 25, 255});
    DrawText("LUCAS SILVA", 125, 236, 12, INK_COLOR);
    DrawText("10h as 13h", 125, 258, 10, DARKGRAY);
    DrawText("[DOC 2]", 150, 285, 10, RED_COLOR);

    /* --- FUNDO DIREITA: RACK DE SERVIDORES --- */
    Rectangle rackRec = {980, 120, 150, 370};
    DrawRectangleRounded(rackRec, 0.03f, 6, (Color){22, 28, 34, 255});
    DrawRectangleRoundedLinesEx(rackRec, 0.03f, 6, 2.5f, (Color){50, 62, 75, 255});
    for (int u = 0; u < 8; u++)
    {
        Rectangle uSlot = {rackRec.x + 10, rackRec.y + 16 + u * 42, rackRec.width - 20, 34};
        DrawRectangleRec(uSlot, (Color){15, 20, 25, 255});
        DrawRectangleLinesEx(uSlot, 1.0f, (Color){40, 50, 60, 255});
        float blink = sinf(time * (5.0f + u * 1.3f));
        DrawCircle((int)(uSlot.x + 14), (int)(uSlot.y + 17), 3, (blink > 0) ? (Color){60, 235, 110, 255} : (Color){20, 85, 40, 255});
        DrawCircle((int)(uSlot.x + 28), (int)(uSlot.y + 17), 3, (blink < 0) ? CYAN_COLOR : (Color){15, 65, 80, 255});
    }
    DrawText("BLOCO SERVIDORES", (int)rackRec.x + 14, (int)(rackRec.y + rackRec.height - 24), 11, GRAY);

    /* --- PAREDE SUPERIOR CENTRAL/DIREITA: MONITOR CFTV (DOC 3) --- */
    Rectangle cctvMonitor = {760, 125, 175, 125};
    DrawRectangleRounded(cctvMonitor, 0.05f, 6, (Color){30, 38, 45, 255});
    DrawRectangleRoundedLinesEx(cctvMonitor, 0.05f, 6, 2.0f, (Color){65, 80, 95, 255});

    Rectangle cctvScr = {cctvMonitor.x + 8, cctvMonitor.y + 8, cctvMonitor.width - 16, cctvMonitor.height - 16};
    DrawRectangleRounded(cctvScr, 0.03f, 4, (Color){8, 22, 14, 255});
    DrawText("CFTV CAM-03 [14:30]", (int)cctvScr.x + 6, (int)cctvScr.y + 6, 11, (Color){80, 245, 120, 255});
    /* Silhueta no monitor de seguranca */
    DrawCircle((int)(cctvScr.x + 80), (int)(cctvScr.y + 55), 15, (Color){45, 115, 75, 255});
    DrawRectangle((int)(cctvScr.x + 68), (int)(cctvScr.y + 70), 24, 34, (Color){35, 95, 60, 255});
    DrawText("[DOC 4: CFTV]", (int)cctvScr.x + 50, (int)(cctvScr.y + cctvScr.height - 18), 10, (Color){80, 245, 120, 255});

    /* --- CENTRO: MESA CORPORATIVA PRINCIPAL (L-SHAPE) --- */
    Rectangle deskRec = {270, 335, 620, 280};
    DrawRectangleRounded(deskRec, 0.05f, 6, (Color){38, 48, 58, 255});
    DrawRectangleRoundedLinesEx(deskRec, 0.05f, 6, 3.0f, (Color){65, 82, 98, 255});
    DrawRectangle(290, 350, 580, 14, (Color){28, 36, 45, 255});

    /* Pés metálicos da mesa */
    DrawRectangle(285, 595, 24, 85, (Color){65, 75, 85, 255});
    DrawRectangle(850, 595, 24, 85, (Color){65, 75, 85, 255});

    /* --- COMPUTADOR DE VIGILANCIA CENTRAL --- */
    /* Base e suporte prateado LCD 2008 */
    DrawRectangleRounded((Rectangle){530, 410, 80, 16}, 0.3f, 4, (Color){165, 175, 185, 255});
    DrawRectangle(562, 380, 16, 32, (Color){145, 155, 165, 255});

    /* Monitor LCD 4:3 com borda prateada */
    Rectangle monitorRec = {455, 225, 230, 170};
    DrawRectangleRounded(monitorRec, 0.06f, 6, (Color){180, 190, 200, 255});
    DrawRectangleRoundedLinesEx(monitorRec, 0.06f, 6, 2.5f, (Color){130, 140, 150, 255});

    /* Tela LCD acesa */
    Rectangle lcdScr = {monitorRec.x + 10, monitorRec.y + 10, monitorRec.width - 20, monitorRec.height - 20};
    Color lcdBg = game->puzzle2008Contested ? (Color){12, 32, 22, 255} : (Color){12, 25, 38, 255};
    Color lcdBorder = game->puzzle2008Contested ? (Color){60, 220, 110, 255} : CYAN_COLOR;
    DrawRectangleRounded(lcdScr, 0.04f, 4, lcdBg);
    DrawRectangleRoundedLinesEx(lcdScr, 0.04f, 4, 1.5f, lcdBorder);

    DrawText("IA VIGILANCIA // 2008", (int)lcdScr.x + 12, (int)lcdScr.y + 12, 13, lcdBorder);
    DrawLine((int)lcdScr.x + 12, (int)lcdScr.y + 28, (int)(lcdScr.x + lcdScr.width - 12), (int)lcdScr.y + 28, Fade(lcdBorder, 0.4f));

    if (game->puzzle2008OrderSuspended)
    {
        DrawText("[OK] ORDEM SUSPENSA", (int)lcdScr.x + 12, (int)lcdScr.y + 44, 14, (Color){60, 235, 110, 255});
        DrawText("CD ejetado no drive!", (int)lcdScr.x + 12, (int)lcdScr.y + 68, 12, RAYWHITE);
        float pB = 0.5f + 0.5f * sinf(time * 6.0f);
        DrawText("> COLETAR CD [ENTER] <", (int)lcdScr.x + 12, (int)lcdScr.y + 98, 13, Fade(GOLD_COLOR, pB));
    }
    else if (game->puzzle2008Contested)
    {
        DrawText("[!] FALSO POSITIVO", (int)lcdScr.x + 12, (int)lcdScr.y + 44, 14, GOLD_COLOR);
        DrawText("Identificacao rejeitada.", (int)lcdScr.x + 12, (int)lcdScr.y + 68, 12, RAYWHITE);
        DrawText("> SUSPENDER ORDEM <", (int)lcdScr.x + 12, (int)lcdScr.y + 98, 13, (Color){60, 235, 110, 255});
    }
    else
    {
        DrawText("ALVO: LUCAS SILVA", (int)lcdScr.x + 12, (int)lcdScr.y + 44, 14, RED_COLOR);
        DrawText("ORDEM DE DRONE: 14:30", (int)lcdScr.x + 12, (int)lcdScr.y + 68, 12, GOLD_COLOR);
        float pB = 0.5f + 0.5f * sinf(time * 4.0f);
        DrawText("> CONTESTAR NO TERMINAL <", (int)lcdScr.x + 8, (int)lcdScr.y + 98, 13, Fade(CYAN_COLOR, pB));
    }

    /* Teclado e mouse na mesa */
    Rectangle kb = {475, 440, 190, 20};
    DrawRectangleRounded(kb, 0.15f, 4, (Color){145, 155, 165, 255});
    for (int k = 0; k < 9; k++)
        DrawRectangle((int)kb.x + 6 + k * 20, (int)kb.y + 4, 14, 11, (Color){45, 52, 60, 255});

    Rectangle mousePad = {685, 435, 34, 40};
    DrawRectangleRounded(mousePad, 0.15f, 4, (Color){20, 28, 38, 255});
    DrawRectangleRounded((Rectangle){694, 444, 16, 24}, 0.4f, 4, (Color){160, 170, 180, 255});

    /* --- LADO ESQUERDO DA MESA: DOSSIE FOTO DRONE (DOC 0) --- */
    Rectangle doc0Rec = {305, 410, 130, 85};
    DrawRectangleRounded(doc0Rec, 0.05f, 4, (Color){175, 145, 105, 255});
    DrawRectangleRoundedLinesEx(doc0Rec, 0.05f, 4, 1.5f, (Color){125, 95, 65, 255});
    /* Foto anexada */
    DrawRectangle((int)doc0Rec.x + 12, (int)doc0Rec.y + 12, 45, 55, (Color){15, 22, 32, 255});
    DrawRectangleLines((int)doc0Rec.x + 12, (int)doc0Rec.y + 12, 45, 55, RED_COLOR);
    DrawText("DRONE 78%", (int)doc0Rec.x + 65, (int)doc0Rec.y + 20, 11, RED_COLOR);
    DrawText("LUCAS S.", (int)doc0Rec.x + 65, (int)doc0Rec.y + 38, 12, INK_COLOR);
    DrawText("[DOC 1]", (int)doc0Rec.x + 65, (int)doc0Rec.y + 58, 11, (Color){125, 35, 18, 255});

    /* --- LADO DIREITO DA MESA: PRANCHETA ENTRADA/CATRACA (DOC 2) --- */
    Rectangle doc2Rec = {735, 410, 125, 85};
    DrawRectangleRounded(doc2Rec, 0.05f, 4, (Color){155, 125, 85, 255});
    DrawRectangleRoundedLinesEx(doc2Rec, 0.05f, 4, 1.5f, (Color){105, 75, 45, 255});
    /* Folha da prancheta */
    DrawRectangle((int)doc2Rec.x + 10, (int)doc2Rec.y + 10, (int)doc2Rec.width - 20, 65, (Color){245, 248, 252, 255});
    DrawRectangle((int)doc2Rec.x + 40, (int)doc2Rec.y + 6, 45, 8, (Color){180, 190, 200, 255}); /* clipe metalico */
    DrawText("PONTO 14:15", (int)doc2Rec.x + 16, (int)doc2Rec.y + 24, 12, (Color){25, 115, 45, 255});
    DrawText("CRACHA #0842", (int)doc2Rec.x + 16, (int)doc2Rec.y + 42, 11, INK_COLOR);
    DrawText("[DOC 3]", (int)doc2Rec.x + 16, (int)doc2Rec.y + 58, 11, (Color){25, 115, 45, 255});

    /* --- MESA LATERAL EXECUTIVA (DIREITA): ANOTACOES DO PAI & MEMORANDO --- */
    Rectangle sideDesk = {970, 420, 170, 175};
    DrawRectangleRounded(sideDesk, 0.04f, 6, (Color){32, 40, 50, 255});
    DrawRectangleRoundedLinesEx(sideDesk, 0.04f, 6, 2.0f, (Color){60, 75, 90, 255});

    /* Documento do Pai: Caderno de pesquisa */
    Rectangle fatherBookRec = {990, 440, 130, 45};
    DrawRectangleRounded(fatherBookRec, 0.10f, 4, (Color){85, 50, 30, 255});
    DrawRectangleRoundedLinesEx(fatherBookRec, 0.10f, 4, 1.5f, GOLD_COLOR);
    DrawText("CADERNO DR. RAMOS", (int)fatherBookRec.x + 8, (int)fatherBookRec.y + 10, 10, GOLD_COLOR);
    DrawText("Projeto Humanitario", (int)fatherBookRec.x + 8, (int)fatherBookRec.y + 24, 10, RAYWHITE);

    /* Documento do Governo: Memorando confidencial */
    Rectangle govMemoRec = {990, 505, 130, 45};
    DrawRectangleRounded(govMemoRec, 0.10f, 4, (Color){45, 28, 28, 255});
    DrawRectangleRoundedLinesEx(govMemoRec, 0.10f, 4, 1.5f, RED_COLOR);
    DrawText("MEMORANDO GOV 2008", (int)govMemoRec.x + 8, (int)govMemoRec.y + 10, 10, RED_COLOR);
    DrawText("Uso Militar / Drones", (int)govMemoRec.x + 8, (int)govMemoRec.y + 24, 10, LIGHTGRAY);

    /* Botao de retorno a Sala 1 (1990) */
    Rectangle btnVoltar1990 = {70, 600, 240, 42};
    DrawButton(btnVoltar1990, "< SALA 1 (1990)", (Color){70, 95, 110, 255});

    /* Botao de avanco para Sala 3 (2048) se destravada */
    Rectangle btnGo2048 = {SCREEN_WIDTH - 280, 600, 240, 42};
    if (game->year2048Unlocked)
    {
        DrawButton(btnGo2048, "SALA 3 (2048) >", (Color){45, 115, 75, 255});
    }

    /* Hotspots interativos ao passar o mouse */
    DrawHotspot(flyerRec, "Inspecionar Panfleto da Revolta [Doc 2]");
    DrawHotspot(cctvMonitor, "Analisar Monitor CFTV dos Servidores [Doc 4]");
    DrawHotspot(monitorRec, "Acessar Terminal de Vigilancia da IA [COMPUTADOR]");
    DrawHotspot(doc0Rec, "Examinar Foto e Comparacao Facial do Drone [Doc 1]");
    DrawHotspot(doc2Rec, "Verificar Registro de Ponto na Catraca [Doc 3]");
    DrawHotspot(fatherBookRec, "Ler Notas do Dr. Ramos (Origem Humanitaria)");
    DrawHotspot(govMemoRec, "Ler Memorando Governamental (Desvio de Uso)");
    DrawHotspot(btnVoltar1990, "Retornar ao Setor de Arquivos (1990)");
    if (game->year2048Unlocked)
    {
        DrawHotspot(btnGo2048, "Avancar para o laboratorio do prototipo de 2048");
    }
}

static void DrawHudRoom2008(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawText("ARQUIVO 02 // 2008 - ESCRITORIO DE VIGILANCIA", 28, 20, 22, CYAN_COLOR);
    DrawText(TextFormat("OPERADOR: %s", game->playerName), 450, 23, 17, GRAY);

    /* Status das evidencias e da ordem */
    if (game->puzzle2008OrderSuspended)
    {
        DrawText("[OK] ORDEM DE DRONE SUSPENSA", 750, 22, 18, (Color){80, 235, 120, 255});
    }
    else if (game->puzzle2008Contested)
    {
        DrawText("[!] FALSO POSITIVO COMPROVADO", 750, 22, 18, GOLD_COLOR);
    }
    else
    {
        DrawText(TextFormat("EVIDENCIAS: %d/4", ExaminedDocs2008Count(game)), 770, 22, 18, LIGHTGRAY);
    }

    DrawText(TextFormat("VIDAS %d", game->lives), 1050, 20, 20, game->lives == 1 ? RED_COLOR : RAYWHITE);
    DrawText(TextFormat("%02d:%02d", minutes, seconds), 1180, 20, 20, GOLD_COLOR);

    /* Rodape */
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawText("Investigue os documentos da sala e use o computador para contestar a ordem.", 28, 675, 18, GRAY);

    /* Botao de Audio */
    Rectangle audioBtn = {410, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawText(game->musicMuted ? "[M] AUDIO: MUDO" : "[M] AUDIO: ON",
             432, 676, 14, game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    /* Botao do Diario */
    DrawJournalHudButton(game);

    if (game->messageTimer > 0)
    {
        int width = MeasureText(game->message, 19) + 34;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 605, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawText(game->message,
                 (SCREEN_WIDTH - MeasureText(game->message, 19)) / 2,
                 614, 19, RAYWHITE);
    }
}

static void UpdateRoom2008(GameState *game)
{
    Rectangle flyerRec = {115, 200, 110, 105};
    Rectangle cctvMonitor = {760, 125, 175, 125};
    Rectangle monitorRec = {455, 225, 230, 170};
    Rectangle doc0Rec = {305, 410, 130, 85};
    Rectangle doc2Rec = {735, 410, 125, 85};
    Rectangle fatherBookRec = {990, 440, 130, 45};
    Rectangle govMemoRec = {990, 505, 130, 45};
    Rectangle btnVoltar1990 = {70, 600, 240, 42};
    Rectangle journalHudBtn = {1040, 658, 215, 54};

    if (IsKeyPressed(KEY_D) || Clicked(journalHudBtn))
    {
        game->journalOpen = !game->journalOpen;
    }

    if (game->journalOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            game->journalOpen = false;
            return;
        }

        int bookX = 120;
        int bookY = 56;
        int footY = bookY + 542;
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};

        if (game->journalYearTab == 1)
        {
            if ((IsKeyPressed(KEY_Q) || Clicked(prevBtn)) && game->journalPage > 0) game->journalPage = 0;
            if ((IsKeyPressed(KEY_E) || Clicked(nextBtn)) && game->journalPage < 1) game->journalPage = 1;
        }
        else
        {
            Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
            if (Clicked(retBtnL)) { game->journalYearTab = 1; game->journalPage = 0; }
        }

        if (Clicked(closeBtn)) game->journalOpen = false;

        /* Abas laterais */
        for (int i = 0; i < 4; i++)
        {
            Rectangle tabRect = {(float)(bookX + 1040), (float)(bookY + 42 + i * 66), 86, 54};
            if (Clicked(tabRect)) { game->journalYearTab = i; game->journalPage = 0; break; }
        }
        return;
    }

    if (game->activeDocument2008 >= 0)
    {
        UpdateDocumentViewer2008(game);
        return;
    }

    if (Clicked(monitorRec))
    {
        game->screen = SCREEN_PUZZLE_2008;
    }
    else if (Clicked(doc0Rec))
    {
        game->doc2008FaceExamined = true;
        game->activeDocument2008 = 0;
    }
    else if (Clicked(flyerRec))
    {
        game->doc2008FlyerExamined = true;
        game->activeDocument2008 = 1;
    }
    else if (Clicked(doc2Rec))
    {
        game->doc2008EntryExamined = true;
        game->activeDocument2008 = 2;
    }
    else if (Clicked(cctvMonitor))
    {
        game->doc2008CameraExamined = true;
        game->activeDocument2008 = 3;
    }
    else if (Clicked(fatherBookRec))
    {
        game->doc2008FatherOriginExamined = true;
        game->activeDocument2008 = 4;
        if (game->doc2008FatherOriginExamined && game->doc2008GovContractExamined && !game->room2008OptionalUnlocked)
        {
            game->room2008OptionalUnlocked = true;
            SetMessage(game, "Pista correlacionada: Acesso de Administrador liberado no computador!");
        }
    }
    else if (Clicked(govMemoRec))
    {
        game->doc2008GovContractExamined = true;
        game->activeDocument2008 = 5;
        if (game->doc2008FatherOriginExamined && game->doc2008GovContractExamined && !game->room2008OptionalUnlocked)
        {
            game->room2008OptionalUnlocked = true;
            SetMessage(game, "Pista correlacionada: Acesso de Administrador liberado no computador!");
        }
    }
    else if (Clicked(btnVoltar1990))
    {
        game->journalYearTab = 0;
        game->screen = SCREEN_ROOM_2;
    }
    else if (game->year2048Unlocked && Clicked((Rectangle){SCREEN_WIDTH - 280, 600, 240, 42}))
    {
        game->journalYearTab = 3;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM;
        SetMessage(game, "Laboratorio do Prototipo A.R.1.3.L. // ANO 2048.");
    }
}

static void DrawPuzzle2008(const GameState *game)
{
    float time = (float)GetTime();

    /* Fundo escuro */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.88f));

    /* Moldura externa do monitor LCD corporativo 2008 */
    Rectangle mon = {80, 25, 1120, 670};
    DrawRectangleRounded(mon, 0.03f, 8, (Color){25, 34, 46, 255});
    DrawRectangleRoundedLinesEx(mon, 0.03f, 8, 3.0f, (Color){75, 95, 115, 255});

    /* Cabecalho do Sistema Operacional */
    Rectangle osHeader = {mon.x + 16, mon.y + 14, mon.width - 32, 46};
    DrawRectangleRounded(osHeader, 0.10f, 6, (Color){15, 22, 32, 255});
    DrawRectangleRoundedLinesEx(osHeader, 0.10f, 6, 1.5f, CYAN_COLOR);

    DrawText("SISTEMA NACIONAL DE VIGILÂNCIA BIOMÉTRICA // NÓ 2008", (int)osHeader.x + 20, (int)osHeader.y + 14, 17, CYAN_COLOR);
    DrawText("STATUS: CONEXAO SEGURA", (int)osHeader.x + 640, (int)osHeader.y + 16, 14, (Color){80, 235, 120, 255});

    /* Abas superiores */
    Rectangle tab0 = {mon.x + 20, mon.y + 68, 320, 40};
    Rectangle tab1 = {mon.x + 350, mon.y + 68, 370, 40};
    Rectangle btnClose = {mon.x + mon.width - 250, mon.y + 68, 230, 40};

    bool isTab0 = (game->terminal2008ActiveTab == 0);
    DrawRectangleRounded(tab0, 0.20f, 6, isTab0 ? (Color){30, 48, 68, 255} : (Color){20, 26, 36, 255});
    DrawRectangleRoundedLinesEx(tab0, 0.20f, 6, 1.5f, isTab0 ? GOLD_COLOR : (Color){50, 65, 80, 255});
    DrawText("1. ORDEM DE ALVO PENDENTE", (int)tab0.x + 28, (int)tab0.y + 12, 15, isTab0 ? GOLD_COLOR : LIGHTGRAY);

    DrawRectangleRounded(tab1, 0.20f, 6, !isTab0 ? (Color){30, 48, 68, 255} : (Color){20, 26, 36, 255});
    DrawRectangleRoundedLinesEx(tab1, 0.20f, 6, 1.5f, !isTab0 ? CYAN_COLOR : (Color){50, 65, 80, 255});
    if (game->room2008OptionalUnlocked)
    {
        DrawText(game->room2008OptionalCompleted ? "2. CONFIGURACAO [DESATIVADA]" : "2. CONFIGURAÇÃO DA INSTALAÇÃO", (int)tab1.x + 24, (int)tab1.y + 12, 15, !isTab0 ? CYAN_COLOR : (Color){80, 235, 120, 255});
    }
    else
    {
        DrawText("2. CONFIGURAÇÃO [BLOQUEADA]", (int)tab1.x + 24, (int)tab1.y + 12, 15, GRAY);
    }

    DrawButton(btnClose, "< SAIR DO TERMINAL [ESC]", (Color){185, 60, 50, 255});

    if (game->terminal2008ActiveTab == 0)
    {
        /* ========================================================= */
        /* ABA 1: ORDEM DE ALVO PENDENTE & SELECAO DE EVIDENCIAS     */
        /* ========================================================= */

        /* Painel Superior do Alvo */
        Rectangle targetPanel = {mon.x + 20, mon.y + 116, mon.width - 40, 115};
        Color tBorder = game->puzzle2008Contested ? (Color){60, 235, 110, 255} : (Color){225, 75, 65, 255};
        DrawRectangleRounded(targetPanel, 0.05f, 6, (Color){16, 24, 34, 255});
        DrawRectangleRoundedLinesEx(targetPanel, 0.05f, 6, 2.0f, tBorder);

        if (!game->puzzle2008Contested)
        {
            float blink = 0.6f + 0.4f * sinf(time * 6.0f);
            DrawText("[ ! ] ORDEM #8491 - ENVIO DE DRONE DE INTERCEPTAÇÃO: PROGRAMADO (14:30)", (int)targetPanel.x + 20, (int)targetPanel.y + 14, 18, Fade(RED_COLOR, blink));
            DrawText("ALVO: LUCAS SILVA  |  SETOR: PRAÇA CENTRAL  |  BASE LEGAL: DIRETRIZ SEC-402", (int)targetPanel.x + 20, (int)targetPanel.y + 42, 15, RAYWHITE);
            DrawText("AVALIAÇÃO DA IA: Similaridade por drone: 78% (Alta incerteza). Ordem aguarda envio.", (int)targetPanel.x + 20, (int)targetPanel.y + 66, 14, GOLD_COLOR);
            DrawText("INSTRUÇÃO: Selecione 2 evidências registradas para contestar a identificação.", (int)targetPanel.x + 20, (int)targetPanel.y + 88, 14, CYAN_COLOR);
        }
        else
        {
            DrawText("[ OK ] STATUS: IDENTIFICAÇÃO NÃO CONFIRMADA // FALSO POSITIVO COMPROVADO", (int)targetPanel.x + 20, (int)targetPanel.y + 14, 18, (Color){60, 235, 110, 255});
            DrawText("ÁLIBI CONFIRMADO: O alvo estava no laboratório da empresa às 14:30.", (int)targetPanel.x + 20, (int)targetPanel.y + 42, 15, RAYWHITE);

            if (!game->puzzle2008OrderSuspended)
            {
                Rectangle btnSusp = {targetPanel.x + targetPanel.width - 370, targetPanel.y + 58, 350, 44};
                DrawButton(btnSusp, "> SUSPENDER ORDEM DE DRONE <", (Color){60, 235, 110, 255});
            }
            else
            {
                Rectangle btnDone = {targetPanel.x + targetPanel.width - 450, targetPanel.y + 58, 430, 44};
                DrawButton(btnDone, "COLETAR CD DE MEMORIA E CONCLUIR >> [ENTER]", GOLD_COLOR);
                DrawText("CD ejetado no drive da maquina!", (int)targetPanel.x + 20, (int)targetPanel.y + 75, 15, GOLD_COLOR);
            }
        }

        /* Grade com os 4 Cards de Registros */
        const char *cardTitles[4] = {
            "1. COMPARAÇÃO FACIAL (DRONE)",
            "2. PANFLETO DA REVOLTA POPULAR",
            "3. REGISTRO DE ENTRADA (CATRACA)",
            "4. CÂMERA INTERNA CFTV (SERVIDORES)"
        };
        const char *cardDesc1[4] = {
            "Foto aerea granulada do drone Alpha-4 as 14:30 na manifestacao.",
            "Material apreendido no centro matutino. Cita Lucas como lider.",
            "Catraca da sede corporativa. Entrada biométrica registrada as 14:15.",
            "Gravacao CFTV no subsolo da empresa no exato minuto de 14:30:12."
        };
        const char *cardDesc2[4] = {
            "Indice 78%: baixa resolucao. Nao garante identidade nem local.",
            "Comprova ativismo politico, mas NAO informa local as 14:30.",
            "Indica presenca no predio corporativo (a 12 km da praca).",
            "Mostra presenca fisica irrefutavel no servidor no mesmo horario!"
        };

        for (int c = 0; c < 4; c++)
        {
            int col = c % 2;
            int row = c / 2;
            Rectangle cRec = {mon.x + 20 + col * 545, mon.y + 240 + row * 150, 530, 140};
            bool isSel = game->puzzle2008Selected[c];

            Color cBg = isSel ? (Color){24, 42, 60, 255} : (Color){16, 24, 32, 255};
            Color cBorder = isSel ? (Color){50, 205, 235, 255} : (Color){45, 60, 75, 255};
            DrawRectangleRounded(cRec, 0.06f, 6, cBg);
            DrawRectangleRoundedLinesEx(cRec, 0.06f, 6, isSel ? 2.5f : 1.5f, cBorder);

            /* Checkbox */
            Rectangle chkBox = {cRec.x + 14, cRec.y + 14, 26, 26};
            DrawRectangleRounded(chkBox, 0.15f, 4, isSel ? (Color){50, 205, 235, 255} : (Color){10, 16, 24, 255});
            DrawRectangleRoundedLinesEx(chkBox, 0.15f, 4, 1.5f, isSel ? RAYWHITE : GRAY);
            if (isSel)
            {
                DrawText("X", (int)chkBox.x + 7, (int)chkBox.y + 4, 18, (Color){10, 18, 28, 255});
            }

            DrawText(cardTitles[c], (int)cRec.x + 50, (int)cRec.y + 16, 16, isSel ? (Color){50, 215, 245, 255} : RAYWHITE);
            DrawText(cardDesc1[c], (int)cRec.x + 16, (int)cRec.y + 48, 13, LIGHTGRAY);
            DrawText(cardDesc2[c], (int)cRec.x + 16, (int)cRec.y + 70, 13, (c == 2 || c == 3) ? (Color){100, 235, 140, 255} : (Color){220, 175, 100, 255});

            /* Botoes de card: Examinar e Selecionar */
            Rectangle btnExam = {cRec.x + 16, cRec.y + 96, 160, 32};
            DrawButton(btnExam, "Examinar Doc", CYAN_COLOR);

            Rectangle btnSel = {cRec.x + 190, cRec.y + 96, 220, 32};
            DrawButton(btnSel, isSel ? "Remover Selecao" : "+ Selecionar Evidencia", isSel ? RED_COLOR : (Color){60, 215, 110, 255});
        }

        /* Barra de Acao de Contestacao Inferior */
        Rectangle actionBar = {mon.x + 20, mon.y + 550, mon.width - 40, 100};
        DrawRectangleRounded(actionBar, 0.05f, 6, (Color){14, 20, 30, 255});
        DrawRectangleRoundedLinesEx(actionBar, 0.05f, 6, 1.5f, (Color){45, 62, 80, 255});

        DrawText(TextFormat("Evidências Selecionadas: %d/2", game->puzzle2008SelectionCount), (int)actionBar.x + 20, (int)actionBar.y + 14, 17, GOLD_COLOR);

        if (!game->puzzle2008Contested)
        {
            Rectangle btnContest = {actionBar.x + 320, actionBar.y + 10, 340, 44};
            bool canContest = (game->puzzle2008SelectionCount == 2);
            DrawButton(btnContest, "CONTESTAR IDENTIFICAÇÃO", canContest ? GOLD_COLOR : GRAY);
        }
        else
        {
            DrawText("[ V ] IDENTIFICAÇÃO CONTESTADA COM SUCESSO!", (int)actionBar.x + 320, (int)actionBar.y + 20, 17, (Color){60, 235, 110, 255});
        }

        /* Mensagem de Feedback */
        if (game->puzzle2008Feedback[0] != '\0')
        {
            Color fbCol = game->puzzle2008Contested ? (Color){60, 235, 110, 255} : (Color){245, 165, 80, 255};
            DrawText(game->puzzle2008Feedback, (int)actionBar.x + 20, (int)actionBar.y + 65, 15, fbCol);
        }
        else
        {
            DrawText("Dica: Selecione as duas evidencias que demonstram onde o alvo estava no horario da acusacao.", (int)actionBar.x + 20, (int)actionBar.y + 65, 14, GRAY);
        }
    }
    else
    {
        /* ========================================================= */
        /* ABA 2: CONFIGURACAO DA INSTALACAO (DESCOBERTA OPCIONAL)   */
        /* ========================================================= */
        Rectangle optPanel = {mon.x + 20, mon.y + 116, mon.width - 40, 530};
        DrawRectangleRounded(optPanel, 0.04f, 6, (Color){16, 24, 34, 255});
        DrawRectangleRoundedLinesEx(optPanel, 0.04f, 6, 2.0f, game->room2008OptionalUnlocked ? (Color){45, 165, 95, 255} : (Color){165, 55, 45, 255});

        if (!game->room2008OptionalUnlocked)
        {
            DrawPadlockIcon((int)(optPanel.x + optPanel.width / 2), (int)(optPanel.y + 120), 2.0f, GOLD_COLOR, RAYWHITE);
            DrawText("ACESSO ADMINISTRATIVO RESTRITO // DIRETRIZES DA FILIAL", (int)optPanel.x + 220, (int)optPanel.y + 190, 20, RED_COLOR);
            DrawText("Para acessar as configuracoes desta instalacao, correlacione as notas do Dr. Ramos", (int)optPanel.x + 190, (int)optPanel.y + 230, 16, LIGHTGRAY);
            DrawText("e o memorando governamental encontrados no escritorio corporativo.", (int)optPanel.x + 240, (int)optPanel.y + 260, 16, LIGHTGRAY);
            DrawText("Examinar ambos os documentos revela a chave de manutencao do projeto.", (int)optPanel.x + 245, (int)optPanel.y + 300, 15, GOLD_COLOR);
        }
        else
        {
            DrawText("DIRETRIZES DA UNIDADE LOCAL // FILIAL METROPOLITANA 08", (int)optPanel.x + 35, (int)optPanel.y + 30, 20, GOLD_COLOR);
            DrawLine((int)optPanel.x + 35, (int)optPanel.y + 60, (int)(optPanel.x + optPanel.width - 35), (int)optPanel.y + 60, Fade(GOLD_COLOR, 0.4f));

            DrawText("STATUS DO SISTEMA: Conectado a rede regional de monitoramento", (int)optPanel.x + 35, (int)optPanel.y + 80, 16, RAYWHITE);
            DrawText("REGISTROS FORENSES ARQUIVADOS: 1.420 ocorrencias preservadas", (int)optPanel.x + 35, (int)optPanel.y + 110, 16, CYAN_COLOR);

            Rectangle cardStatus = {optPanel.x + 35, optPanel.y + 145, optPanel.width - 70, 120};
            DrawRectangleRounded(cardStatus, 0.05f, 6, (Color){24, 32, 44, 255});
            DrawRectangleRoundedLinesEx(cardStatus, 0.05f, 6, 1.5f, (Color){55, 75, 95, 255});

            DrawText("ENVIO AUTOMÁTICO DE ALVOS DESTA INSTALAÇÃO:", (int)cardStatus.x + 20, (int)cardStatus.y + 20, 16, GOLD_COLOR);
            if (!game->room2008OptionalCompleted)
            {
                DrawText("[ ATIVO ] - Drones sao despachados automaticamente sob correspondencia preliminar.", (int)cardStatus.x + 20, (int)cardStatus.y + 50, 15, RED_COLOR);
                DrawText("Desativar o envio automatico suspende os ataques sem apagar as provas forenses.", (int)cardStatus.x + 20, (int)cardStatus.y + 80, 15, LIGHTGRAY);

                Rectangle btnDisable = {optPanel.x + 35, optPanel.y + 290, 520, 52};
                DrawButton(btnDisable, "DESATIVAR ENVIO AUTOMÁTICO DESTA INSTALAÇÃO", (Color){245, 155, 55, 255});
            }
            else
            {
                DrawText("[ DESATIVADO ] - O despacho automatico de drones desta central foi bloqueado.", (int)cardStatus.x + 20, (int)cardStatus.y + 50, 15, (Color){60, 235, 110, 255});
                DrawText("Todos os 1.420 relatorios foram preservados intactos como prova contra o abuso.", (int)cardStatus.x + 20, (int)cardStatus.y + 80, 15, (Color){100, 235, 140, 255});

                Rectangle okBox = {optPanel.x + 35, optPanel.y + 290, optPanel.width - 70, 70};
                DrawRectangleRounded(okBox, 0.08f, 6, Fade((Color){45, 165, 80, 255}, 0.20f));
                DrawRectangleRoundedLinesEx(okBox, 0.08f, 6, 1.5f, (Color){60, 235, 110, 255});
                DrawText("[ V ] PROTOCOLO ÉTICO APLICADO: CENTRAL LOCAL PROTEGIDA COM SUCESSO!", (int)okBox.x + 25, (int)okBox.y + 25, 16, (Color){60, 235, 110, 255});
            }

            Rectangle cardInfo = {optPanel.x + 35, optPanel.y + 380, optPanel.width - 70, 120};
            DrawRectangleRounded(cardInfo, 0.05f, 6, Fade(PANEL_COLOR, 0.90f));
            DrawRectangleRoundedLinesEx(cardInfo, 0.05f, 6, 1.5f, (Color){60, 80, 105, 255});
            DrawText("COMPREENSÃO ÉTICA PROFUNDA:", (int)cardInfo.x + 20, (int)cardInfo.y + 16, 15, GOLD_COLOR);
            DrawText("- Apagar o sistema destruiria as evidencias que comprovam o desvio do governo.", (int)cardInfo.x + 20, (int)cardInfo.y + 44, 14, LIGHTGRAY);
            DrawText("- Desativar o despacho automatico impede ataques imediatos mantendo a verdade protegida.", (int)cardInfo.x + 20, (int)cardInfo.y + 68, 14, LIGHTGRAY);
            DrawText("- A acao afeta exclusivamente esta instalacao e foi devidamente registrada no diario.", (int)cardInfo.x + 20, (int)cardInfo.y + 92, 14, (Color){60, 235, 110, 255});
        }
    }
}

static void UpdatePuzzle2008(GameState *game)
{
    if (game->activeDocument2008 >= 0)
    {
        UpdateDocumentViewer2008(game);
        return;
    }

    Rectangle mon = {80, 25, 1120, 670};
    Rectangle tab0 = {mon.x + 20, mon.y + 68, 320, 40};
    Rectangle tab1 = {mon.x + 350, mon.y + 68, 370, 40};
    Rectangle btnClose = {mon.x + mon.width - 250, mon.y + 68, 230, 40};

    if (Clicked(btnClose) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_ROOM_2008;
        return;
    }

    if (Clicked(tab0))
    {
        game->terminal2008ActiveTab = 0;
    }
    else if (Clicked(tab1))
    {
        game->terminal2008ActiveTab = 1;
    }

    if (game->terminal2008ActiveTab == 0)
    {
        /* Botoes dos 4 cards */
        for (int c = 0; c < 4; c++)
        {
            int col = c % 2;
            int row = c / 2;
            Rectangle cRec = {mon.x + 20 + col * 545, mon.y + 240 + row * 150, 530, 140};
            Rectangle btnExam = {cRec.x + 16, cRec.y + 96, 160, 32};
            Rectangle btnSel = {cRec.x + 190, cRec.y + 96, 220, 32};
            Rectangle chkBox = {cRec.x + 14, cRec.y + 14, 26, 26};

            if (Clicked(btnExam))
            {
                if (c == 0) game->doc2008FaceExamined = true;
                if (c == 1) game->doc2008FlyerExamined = true;
                if (c == 2) game->doc2008EntryExamined = true;
                if (c == 3) game->doc2008CameraExamined = true;
                game->activeDocument2008 = c;
                return;
            }

            if (Clicked(btnSel) || Clicked(chkBox))
            {
                if (game->puzzle2008Selected[c])
                {
                    game->puzzle2008Selected[c] = false;
                    game->puzzle2008SelectionCount--;
                }
                else
                {
                    if (game->puzzle2008SelectionCount < 2)
                    {
                        game->puzzle2008Selected[c] = true;
                        game->puzzle2008SelectionCount++;
                    }
                    else
                    {
                        snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "Maximo de 2 evidencias atingido. Remova uma para trocar.");
                    }
                }
            }
        }

        /* Botao de Contestacao */
        Rectangle actionBar = {mon.x + 20, mon.y + 550, mon.width - 40, 100};
        Rectangle btnContest = {actionBar.x + 320, actionBar.y + 10, 340, 44};

        if (!game->puzzle2008Contested)
        {
            if ((Clicked(btnContest) || IsKeyPressed(KEY_ENTER)) && game->puzzle2008SelectionCount == 2)
            {
                /* A combinacao correta e Doc 2 (Registro de Entrada) + Doc 3 (Camera CFTV) */
                if (game->puzzle2008Selected[2] && game->puzzle2008Selected[3])
                {
                    game->puzzle2008Contested = true;
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "FALSO POSITIVO COMPROVADO! A catraca e o CFTV provam presenca na empresa as 14:30.");
                }
                else if (game->puzzle2008Selected[0] && game->puzzle2008Selected[1])
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "A foto borrada e o panfleto citam o alvo, mas nao provam onde ele estava as 14:30.");
                }
                else if (game->puzzle2008Selected[0] && game->puzzle2008Selected[2])
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "O registro de ponto indica entrada, mas falta prova visual de permanencia as 14:30.");
                }
                else if (game->puzzle2008Selected[0] && game->puzzle2008Selected[3])
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "A camera interna mostra presenca, mas precisa do registro formal de ponto.");
                }
                else if (game->puzzle2008Selected[1] && (game->puzzle2008Selected[2] || game->puzzle2008Selected[3]))
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "O panfleto indica militancia politica, mas nao desmente a posicao da IA.");
                }
                else
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "Evidencias insuficientes para comprovar o falso positivo. Tente outra combinacao.");
                }
            }
        }
        else
        {
            Rectangle targetPanel = {mon.x + 20, mon.y + 116, mon.width - 40, 115};
            if (!game->puzzle2008OrderSuspended)
            {
                Rectangle btnSusp = {targetPanel.x + targetPanel.width - 370, targetPanel.y + 58, 350, 44};
                if (Clicked(btnSusp) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game->puzzle2008OrderSuspended = true;
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "ORDEM SUSPENSA! Drones cancelados. O CD de Memoria foi ejetado pelo drive.");
                }
            }
            else
            {
                Rectangle btnDone = {targetPanel.x + targetPanel.width - 450, targetPanel.y + 58, 430, 44};
                if (Clicked(btnDone) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game->screen = SCREEN_RESULT_2008;
                }
            }
        }
    }
    else
    {
        /* Aba 1: Acao Opcional */
        if (game->room2008OptionalUnlocked && !game->room2008OptionalCompleted)
        {
            Rectangle optPanel = {mon.x + 20, mon.y + 116, mon.width - 40, 530};
            Rectangle btnDisable = {optPanel.x + 35, optPanel.y + 290, 520, 52};
            if (Clicked(btnDisable))
            {
                game->room2008OptionalCompleted = true;
                SetMessage(game, "Envio automatico da filial desativado. Registros forenses preservados!");
            }
        }
    }
}

static void DrawResult2008(const GameState *game)
{
    DrawBackground();
    DrawText("ARTEFATO RECUPERADO", 90, 68, 20, CYAN_COLOR);
    DrawText("CD DE MEMÓRIA // 2008", 90, 108, 46, RAYWHITE);

    /* Desenha o CD de Memoria recuperado */
    DrawMemoryCd((Vector2){140, 260}, 1.3f);

    /* Painel narrativo de avanco */
    Rectangle winBox = {505, 175, 670, 380};
    DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, GOLD_COLOR);

    DrawText("DESVIO ALGORÍTMICO // FASE 2 (2008) CONCLUÍDA", 535, 202, 20, GOLD_COLOR);

    DrawText("Ao demonstrar o falso positivo através do crachá de ponto e do", 535, 236, 17, LIGHTGRAY);
    DrawText("circuito interno, Elira impediu o drone e salvou a vida de Lucas Silva.", 535, 258, 17, LIGHTGRAY);

    DrawText("O algoritmo de busca facial, concebido por seu pai para salvar", 535, 290, 17, LIGHTGRAY);
    DrawText("vítimas em catástrofes, havia sido pervertido pelo governo para", 535, 312, 17, (Color){245, 175, 75, 255});
    DrawText("perseguir líderes populares e ativistas civis.", 535, 334, 17, (Color){245, 175, 75, 255});

    DrawText("-> Proxima Fronteira: FASE 3 // ANO 2048: O PROTOTIPO A.R.1.3.L.!", 535, 362, 17, CYAN_COLOR);

    if (game->room2008OptionalCompleted)
    {
        DrawRectangleRounded((Rectangle){535, 396, 610, 44}, 0.12f, 4, Fade((Color){45, 165, 80, 255}, 0.20f));
        DrawRectangleRoundedLinesEx((Rectangle){535, 396, 610, 44}, 0.12f, 4, 1.0f, (Color){60, 235, 110, 255});
        DrawText("[DESTAQUE ÉTICO]: Despacho automático da filial desativado,", 550, 402, 14, (Color){60, 235, 110, 255});
        DrawText("mantendo todas as 1.420 provas forenses intactas para o futuro.", 550, 420, 14, (Color){60, 235, 110, 255});
    }
    else
    {
        DrawText("[DESCOBERTA PENDENTE]: Havia um protocolo no escritório para", 535, 400, 14, GOLD_COLOR);
        DrawText("desativar o envio automático da filial mantendo os registros como prova.", 535, 420, 14, GRAY);
    }

    DrawText(TextFormat("TEMPO TOTAL: %02d:%02d  |  VIDAS: %d/3",
                        (int)(GetTime() - game->startTime) / 60,
                        (int)(GetTime() - game->startTime) % 60,
                        game->lives),
             535, 460, 18, (Color){100, 235, 140, 255});

    DrawText(TextFormat("OPERADOR: %s", game->playerName), 535, 492, 16, GRAY);

    Rectangle btnNext2048 = {180, 580, 380, 56};
    Rectangle btnOffice = {580, 580, 220, 56};
    Rectangle btnRoom1 = {820, 580, 220, 56};
    Rectangle btnMenu = {1060, 580, 160, 56};
    DrawButton(btnNext2048, "AVANCAR PARA ANO 2048 >> [ENTER]", GOLD_COLOR);
    DrawButton(btnOffice, "REVER ESCRITORIO", CYAN_COLOR);
    DrawButton(btnRoom1, "< SALA 1 (1990)", (Color){70, 95, 110, 255});
    DrawButton(btnMenu, "MENU", PANEL_LIGHT);
}

static void UpdateResult2008(GameState *game)
{
    Rectangle btnNext2048 = {180, 580, 380, 56};
    Rectangle btnOffice = {580, 580, 220, 56};
    Rectangle btnRoom1 = {820, 580, 220, 56};
    Rectangle btnMenu = {1060, 580, 160, 56};

    if (Clicked(btnNext2048) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        game->year2048Unlocked = true;
        game->journalYearTab = 3;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM;
        SetMessage(game, "Salto temporal concluido: O Prototipo de A.R.1.3.L. // ANO 2048.");
    }
    else if (Clicked(btnOffice))
    {
        game->screen = SCREEN_ROOM_2008;
    }
    else if (Clicked(btnRoom1))
    {
        game->journalYearTab = 0;
        game->screen = SCREEN_ROOM_2;
    }
    else if (Clicked(btnMenu) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_TITLE;
    }
}

typedef struct {
    float bass_freq;
    int note_count;
    float freqs[6];
    int arp_count;
    float arp_freqs[8];
} AmbientChordDef;

static float MidiToFreq(float m)
{
    return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f);
}

static void EnsureAmbientMusicFile(void)
{
    const char *outPath = "assets/ambient.wav";
    if (FileExists(outPath)) return;

    int sampleRate = 44100;
    int channels = 2;
    float duration = 48.0f;
    int totalSamples = (int)(sampleRate * duration);

    float *left = (float *)calloc((size_t)totalSamples, sizeof(float));
    float *right = (float *)calloc((size_t)totalSamples, sizeof(float));
    if (!left || !right)
    {
        if (left) free(left);
        if (right) free(right);
        return;
    }

    AmbientChordDef chords[6] = {
        { MidiToFreq(38.0f), 5, { MidiToFreq(50), MidiToFreq(53), MidiToFreq(57), MidiToFreq(60), MidiToFreq(64) },
          8, { MidiToFreq(62), MidiToFreq(65), MidiToFreq(69), MidiToFreq(72), MidiToFreq(74), MidiToFreq(69), MidiToFreq(65), MidiToFreq(62) } },
        { MidiToFreq(34.0f), 5, { MidiToFreq(46), MidiToFreq(50), MidiToFreq(53), MidiToFreq(57), MidiToFreq(62) },
          8, { MidiToFreq(58), MidiToFreq(62), MidiToFreq(65), MidiToFreq(69), MidiToFreq(70), MidiToFreq(69), MidiToFreq(65), MidiToFreq(62) } },
        { MidiToFreq(41.0f), 5, { MidiToFreq(48), MidiToFreq(53), MidiToFreq(57), MidiToFreq(60), MidiToFreq(64) },
          8, { MidiToFreq(60), MidiToFreq(65), MidiToFreq(67), MidiToFreq(69), MidiToFreq(72), MidiToFreq(69), MidiToFreq(67), MidiToFreq(65) } },
        { MidiToFreq(43.0f), 5, { MidiToFreq(46), MidiToFreq(50), MidiToFreq(55), MidiToFreq(58), MidiToFreq(62) },
          8, { MidiToFreq(58), MidiToFreq(62), MidiToFreq(65), MidiToFreq(67), MidiToFreq(70), MidiToFreq(67), MidiToFreq(65), MidiToFreq(62) } },
        { MidiToFreq(34.0f), 5, { MidiToFreq(46), MidiToFreq(53), MidiToFreq(58), MidiToFreq(62), MidiToFreq(65) },
          8, { MidiToFreq(62), MidiToFreq(65), MidiToFreq(70), MidiToFreq(72), MidiToFreq(74), MidiToFreq(72), MidiToFreq(70), MidiToFreq(65) } },
        { MidiToFreq(33.0f), 5, { MidiToFreq(45), MidiToFreq(52), MidiToFreq(57), MidiToFreq(62), MidiToFreq(64) },
          8, { MidiToFreq(57), MidiToFreq(60), MidiToFreq(64), MidiToFreq(69), MidiToFreq(71), MidiToFreq(69), MidiToFreq(64), MidiToFreq(60) } }
    };

    float chordLen = duration / 6.0f;
    const float kPI = 3.14159265f;

    for (int c = 0; c < 6; c++)
    {
        float chordCenter = c * chordLen + chordLen * 0.5f;
        float halfW = (chordLen + 3.0f) * 0.5f;
        float bassF = chords[c].bass_freq;

        for (int i = 0; i < totalSamples; i++)
        {
            float t = (float)i / (float)sampleRate;
            float dt = fmodf(t - chordCenter + duration, duration);
            if (dt > duration * 0.5f) dt -= duration;

            if (fabsf(dt) < halfW)
            {
                float env = 0.5f * (1.0f + cosf(kPI * dt / halfW));
                float bPhase = 2.0f * kPI * bassF * t;
                float sub = (sinf(bPhase) * 0.85f + sinf(bPhase * 2.0f) * 0.15f) * 0.28f * env;
                left[i] += sub;
                right[i] += sub;

                for (int n = 0; n < chords[c].note_count; n++)
                {
                    float f = chords[c].freqs[n];
                    float pan = -0.35f + 0.70f * ((float)n / (float)(chords[c].note_count - 1));
                    float lfo = sinf(2.0f * kPI * 0.22f * t + n);
                    float p1 = 2.0f * kPI * f * (1.0f + 0.0008f * lfo) * t;
                    float p2 = 2.0f * kPI * f * 1.0016f * t;

                    float voice = (sinf(p1) * 0.6f + sinf(p1 * 2.0f) * 0.2f + sinf(p1 * 3.0f) * 0.08f +
                                   sinf(p2) * 0.5f + sinf(p2 * 2.0f) * 0.15f) *
                                  (0.06f / chords[c].note_count) * env;

                    float lGain = cosf((pan + 1.0f) * kPI * 0.25f);
                    float rGain = sinf((pan + 1.0f) * kPI * 0.25f);

                    left[i] += voice * lGain;
                    right[i] += voice * rGain;
                }
            }
        }
    }

    float arpStep = 0.5f;
    int numArps = (int)(duration / arpStep);

    for (int a = 0; a < numArps; a++)
    {
        float noteT = a * arpStep;
        int cIdx = ((int)(noteT / chordLen)) % 6;
        float freq = chords[cIdx].arp_freqs[a % chords[cIdx].arp_count];

        float pluckLen = 1.8f;
        int pSamples = (int)(pluckLen * sampleRate);
        int startS = (int)(noteT * sampleRate);

        float pan = (a % 2 == 0) ? -0.55f : 0.55f;
        float gL = cosf((pan + 1.0f) * kPI * 0.25f);
        float gR = sinf((pan + 1.0f) * kPI * 0.25f);
        float amp = 0.038f;

        for (int s = 0; s < pSamples; s++)
        {
            int idx = (startS + s) % totalSamples;
            float ct = (float)s / (float)sampleRate;
            float env = expf(-ct * 3.2f) * (1.0f - expf(-ct * 60.0f));
            float ph = 2.0f * kPI * freq * ct;
            float val = (sinf(ph) * 0.75f + sinf(ph * 2.0f) * 0.25f) * amp * env;

            left[idx] += val * gL;
            right[idx] += val * gR;

            int d1 = (idx + (int)(0.28f * sampleRate)) % totalSamples;
            left[d1] += val * 0.40f * gR;
            right[d1] += val * 0.40f * gL;

            int d2 = (idx + (int)(0.56f * sampleRate)) % totalSamples;
            left[d2] += val * 0.20f * gL;
            right[d2] += val * 0.20f * gR;
        }
    }

    float peak = 0.0f;
    for (int i = 0; i < totalSamples; i++)
    {
        left[i] = tanhf(left[i] * 1.35f);
        right[i] = tanhf(right[i] * 1.35f);
        if (fabsf(left[i]) > peak) peak = fabsf(left[i]);
        if (fabsf(right[i]) > peak) peak = fabsf(right[i]);
    }

    if (peak > 0.0f)
    {
        float norm = 0.88f / peak;
        for (int i = 0; i < totalSamples; i++)
        {
            left[i] *= norm;
            right[i] *= norm;
        }
    }

    int seam = (int)(sampleRate * 0.05f);
    for (int i = 0; i < seam; i++)
    {
        float blend = (float)i / (float)seam;
        int h = i;
        int t = totalSamples - seam + i;
        float vl = left[h] * blend + left[t] * (1.0f - blend);
        float vr = right[h] * blend + right[t] * (1.0f - blend);
        left[h] = vl; left[t] = vl;
        right[h] = vr; right[t] = vr;
    }

    FILE *f = fopen(outPath, "wb");
    if (f)
    {
        uint32_t dataSize = (uint32_t)(totalSamples * channels * sizeof(int16_t));
        uint32_t riffSize = 36 + dataSize;
        uint32_t subchunk1Size = 16;
        uint16_t audioFormat = 1;
        uint16_t numCh = (uint16_t)channels;
        uint32_t sRate = (uint32_t)sampleRate;
        uint32_t byteRate = (uint32_t)(sampleRate * channels * sizeof(int16_t));
        uint16_t blockAlign = (uint16_t)(channels * sizeof(int16_t));
        uint16_t bitsPerSample = 16;

        fwrite("RIFF", 1, 4, f);
        fwrite(&riffSize, 4, 1, f);
        fwrite("WAVE", 1, 4, f);
        fwrite("fmt ", 1, 4, f);
        fwrite(&subchunk1Size, 4, 1, f);
        fwrite(&audioFormat, 2, 1, f);
        fwrite(&numCh, 2, 1, f);
        fwrite(&sRate, 4, 1, f);
        fwrite(&byteRate, 4, 1, f);
        fwrite(&blockAlign, 2, 1, f);
        fwrite(&bitsPerSample, 2, 1, f);
        fwrite("data", 1, 4, f);
        fwrite(&dataSize, 4, 1, f);

        for (int i = 0; i < totalSamples; i++)
        {
            int16_t sl = (int16_t)(fmaxf(-32767.0f, fminf(32767.0f, left[i] * 32767.0f)));
            int16_t sr = (int16_t)(fmaxf(-32767.0f, fminf(32767.0f, right[i] * 32767.0f)));
            fwrite(&sl, 2, 1, f);
            fwrite(&sr, 2, 1, f);
        }
        fclose(f);
    }

    free(left);
    free(right);
}

int main(void)
{
    GameState game = {0};
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ELIRA - Escape Run Temporal");
    InitAudioDevice();

    EnsureAmbientMusicFile();

    Music ambientBgm = {0};
    bool bgmLoaded = false;
    if (FileExists("assets/ambient.wav"))
    {
        ambientBgm = LoadMusicStream("assets/ambient.wav");
        if (ambientBgm.stream.buffer != NULL)
        {
            ambientBgm.looping = true;
            SetMusicVolume(ambientBgm, 0.45f);
            PlayMusicStream(ambientBgm);
            bgmLoaded = true;
        }
    }

    /* Buffer virtual de renderizacao para escalonamento perfeito em qualquer resolucao/fullscreen */
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    SetTargetFPS(60);
    game.screen = SCREEN_TITLE;
    ResetDemo(&game);

    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();
        if (game.messageTimer > 0) game.messageTimer -= delta;

        if (bgmLoaded)
        {
            UpdateMusicStream(ambientBgm);
        }

        /* Alternar tela cheia globalmente com F11 ou Alt+Enter */
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)))
        {
            ToggleFullscreen();
        }

        /* Alternar audio com tecla M ou clique no HUD */
        Rectangle audioHudBtn = {410, 665, 175, 40};
        bool toggleAudio = IsKeyPressed(KEY_M) || ((game.screen == SCREEN_ROOM || game.screen == SCREEN_ROOM_2 || game.screen == SCREEN_ROOM_2008) && Clicked(audioHudBtn));
        if (toggleAudio)
        {
            game.musicMuted = !game.musicMuted;
            if (bgmLoaded)
            {
                SetMusicVolume(ambientBgm, game.musicMuted ? 0.0f : g_musicVolume);
            }
            SetMessage(&game, game.musicMuted ? "Musica ambiente desativada [Mudo]."
                                              : "Musica ambiente ativada.");
        }

        switch (game.screen)
        {
            case SCREEN_TITLE:
                if (Clicked((Rectangle){86, 436, 350, 56}) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game.screen = SCREEN_PLAYER_NAME;
                }
                else if (Clicked((Rectangle){86, 506, 350, 52}) || IsKeyPressed(KEY_C))
                {
                    game.screen = SCREEN_SETTINGS;
                }
                break;
            case SCREEN_SETTINGS:
                UpdateSettings(&game, &ambientBgm, bgmLoaded);
                break;
            case SCREEN_PLAYER_NAME:
                UpdatePlayerName(&game);
                if (Clicked((Rectangle){490, 470, 300, 58}) || IsKeyPressed(KEY_ENTER))
                {
                    if (game.playerNameLength > 0)
                    {
                        game.message[0] = '\0';
                        game.messageTimer = 0.0f;
                        game.screen = SCREEN_INTRO;
                    }
                    else
                        SetMessage(&game, "Informe um nome para criar seu perfil.");
                }
                break;
            case SCREEN_INTRO:
                if (Clicked((Rectangle){490, 575, 300, 58}) ||
                    IsKeyPressed(KEY_ENTER))
                {
                    ResetDemo(&game);
                    game.screen = SCREEN_ROOM_2;
                }
                break;
            case SCREEN_ROOM:
                UpdateRoom(&game);
                break;
            case SCREEN_TERMINAL:
                UpdateTerminal(&game);
                break;
            case SCREEN_RESULT:
                if (Clicked((Rectangle){120, 588, 250, 56}))
                {
                    game.journalYearTab = 1;
                    game.screen = SCREEN_ROOM_2008;
                    SetMessage(&game, "Retornando ao Escritorio // 2008.");
                }
                else if (Clicked((Rectangle){390, 588, 230, 56}) || IsKeyPressed(KEY_BACKSPACE))
                {
                    game.journalYearTab = 0;
                    game.screen = SCREEN_ROOM_2;
                    SetMessage(&game, "Retornando ao Setor de Arquivos // 1990.");
                }
                else if (Clicked((Rectangle){640, 588, 260, 56}))
                {
                    game.foundCalendar = false;
                    game.foundBooks = false;
                    game.foundDrawer = false;
                    game.foundBlueprint = false;
                    game.foundTape = false;
                    game.codeLength = 0;
                    game.code[0] = '\0';
                    game.screen = SCREEN_ROOM;
                }
                else if (Clicked((Rectangle){920, 588, 220, 56}) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game.screen = SCREEN_TITLE;
                }
                break;
            case SCREEN_ROOM_2:
                UpdateRoom2(&game);
                break;
            case SCREEN_PUZZLE_FILES:
                UpdatePuzzleFiles(&game);
                break;
            case SCREEN_RESULT_2:
                UpdateResult2(&game);
                break;
            case SCREEN_ROOM_2008:
                UpdateRoom2008(&game);
                break;
            case SCREEN_PUZZLE_2008:
                UpdatePuzzle2008(&game);
                break;
            case SCREEN_RESULT_2008:
                UpdateResult2008(&game);
                break;
            case SCREEN_FAILURE:
                if (Clicked((Rectangle){490, 460, 300, 60}) || IsKeyPressed(KEY_ENTER))
                {
                    ResetDemo(&game);
                    game.screen = SCREEN_ROOM_2;
                }
                else if (Clicked((Rectangle){520, 550, 240, 48}) || IsKeyPressed(KEY_ESCAPE))
                    game.screen = SCREEN_TITLE;
                break;
        }

        /* 1. Renderiza o jogo na textura virtual (1280x720) */
        BeginTextureMode(target);
        ClearBackground(VOID_COLOR);
        switch (game.screen)
        {
            case SCREEN_TITLE: DrawTitle(); break;
            case SCREEN_SETTINGS: DrawSettings(&game); break;
            case SCREEN_PLAYER_NAME: DrawPlayerName(&game); break;
            case SCREEN_INTRO: DrawIntro(&game); break;
            case SCREEN_ROOM:
                DrawRoom(&game);
                DrawHud(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_TERMINAL: DrawTerminal(&game); break;
            case SCREEN_RESULT: DrawResult(&game); break;
            case SCREEN_ROOM_2:
                DrawRoom2(&game);
                DrawHudRoom2(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_PUZZLE_FILES:
                DrawPuzzleFiles(&game);
                break;
            case SCREEN_RESULT_2:
                DrawResult2(&game);
                break;
            case SCREEN_ROOM_2008:
                DrawRoom2008(&game);
                DrawHudRoom2008(&game);
                if (game.activeDocument2008 >= 0) DrawDocumentViewer2008(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_PUZZLE_2008:
                DrawPuzzle2008(&game);
                if (game.activeDocument2008 >= 0) DrawDocumentViewer2008(&game);
                break;
            case SCREEN_RESULT_2008:
                DrawResult2008(&game);
                break;
            case SCREEN_FAILURE: DrawFailure(); break;
        }
        EndTextureMode();

        /* 2. Desenha a textura escalonada para a janela atual / tela cheia */
        BeginDrawing();
        ClearBackground(BLACK);
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        float scale = fminf((float)screenW / SCREEN_WIDTH, (float)screenH / SCREEN_HEIGHT);
        Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, -(float)target.texture.height };
        Rectangle destRec = {
            (screenW - (SCREEN_WIDTH * scale)) * 0.5f,
            (screenH - (SCREEN_HEIGHT * scale)) * 0.5f,
            SCREEN_WIDTH * scale,
            SCREEN_HEIGHT * scale
        };
        DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    if (bgmLoaded)
    {
        UnloadMusicStream(ambientBgm);
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
