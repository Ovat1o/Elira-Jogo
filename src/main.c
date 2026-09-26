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
} GameState;

static const Color VOID_COLOR = {8, 12, 24, 255};
static const Color PANEL_COLOR = {18, 27, 45, 255};
static const Color PANEL_LIGHT = {34, 49, 72, 255};
static const Color CYAN_COLOR = {73, 220, 225, 255};
static const Color GOLD_COLOR = {242, 186, 73, 255};
static const Color RED_COLOR = {231, 76, 94, 255};
static const Color PAPER_COLOR = {230, 220, 184, 255};
static const Color INK_COLOR = {43, 40, 35, 255};

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

    /* 2. Titulo A.R.I.3.L. com aura neon e micro-glitch sci-fi */
    float titleGlow = 0.30f + 0.18f * sinf(time * 2.2f);
    DrawText("A.R.I.3.L.", 81, 164, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawText("A.R.I.3.L.", 83, 164, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawText("A.R.I.3.L.", 82, 163, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawText("A.R.I.3.L.", 82, 165, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));

    float glitchCycle = fmodf(time, 4.2f);
    bool isGlitch = (glitchCycle < 0.10f) || (glitchCycle > 2.10f && glitchCycle < 2.17f);
    if (isGlitch)
    {
        float shift = sinf(time * 50.0f) * 4.0f;
        DrawText("A.R.I.3.L.", (int)(82 + shift), 164, 96, Fade(CYAN_COLOR, 0.70f));
        DrawText("A.R.I.3.L.", (int)(82 - shift), 164, 96, Fade(RED_COLOR, 0.60f));
        DrawRectangle(80, 198, 430, 3, Fade(CYAN_COLOR, 0.45f));
    }
    DrawText("A.R.I.3.L.", 82, 164, 96, RAYWHITE);

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
    DrawText("DEMO 1990  //  O PROTOTIPO", 91, 638, 16,
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
    DrawText("O PROTOTIPO", 90, 110, 48, RAYWHITE);
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
    DrawText("OBJETIVO DA DEMO", 880, 245, 19, GOLD_COLOR);
    DrawText("1. Investigue a sala", 880, 295, 19, RAYWHITE);
    DrawText("2. Reuna as pistas", 880, 335, 19, RAYWHITE);
    DrawText("3. Acesse o terminal", 880, 375, 19, RAYWHITE);
    DrawText("4. Recupere o disquete", 880, 415, 19, RAYWHITE);
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
    DrawCircleGradient((Vector2){pwX - 9, pwY - 8}, 8,
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

    /* --- CALENDÁRIO COM PERSPECTIVA DA PAREDE INCLINADA --- */
    /* Sombra do calendário projetada na parede */
    DrawTriangle((Vector2){78, 120}, (Vector2){78, 294}, (Vector2){238, 312}, Fade(BLACK, 0.35f));
    DrawTriangle((Vector2){238, 312}, (Vector2){238, 148}, (Vector2){78, 120}, Fade(BLACK, 0.35f));

    /* Folha do calendário em perspectiva inclinada */
    DrawTriangle((Vector2){75, 116}, (Vector2){75, 290}, (Vector2){235, 308}, (Color){226, 218, 194, 255});
    DrawTriangle((Vector2){235, 308}, (Vector2){235, 144}, (Vector2){75, 116}, (Color){216, 206, 180, 255});

    /* Cabeçalho vinho em perspectiva */
    DrawTriangle((Vector2){75, 116}, (Vector2){75, 158}, (Vector2){235, 182}, (Color){158, 48, 52, 255});
    DrawTriangle((Vector2){235, 182}, (Vector2){235, 144}, (Vector2){75, 116}, (Color){140, 38, 42, 255});

    /* Fixador superior com ilhós metálico e furos do espiral */
    DrawCircle(155, 124, 3.5f, (Color){60, 65, 70, 255});
    DrawCircle(155, 124, 1.5f, (Color){180, 185, 190, 255});
    for (int ring = 0; ring < 7; ring++)
    {
        int rx = 88 + ring * 20;
        int ry = 118 + ring * 3;
        DrawLineEx((Vector2){(float)rx, (float)ry}, (Vector2){(float)rx + 4, (float)ry + 8}, 2, (Color){180, 185, 190, 255});
    }

    /* Textos do cabeçalho */
    DrawText("OUTUBRO", 98, 137, 17, RAYWHITE);
    DrawText("1990", 188, 150, 15, GOLD_COLOR);

    /* Grade de dias da semana */
    DrawText("D  S  T  Q  Q  S  S", 92, 188, 11, (Color){135, 70, 65, 255});

    /* Matriz de dias do mês de Outubro */
    for (int day = 1; day <= 31; day++)
    {
        int dayIndex = day + 0;
        int col = dayIndex % 7;
        int row = dayIndex / 7;
        int dx = 94 + col * 19;
        int dy = 205 + row * 13;
        if (day == 24)
        {
            DrawCircle(dx + 5, dy + 5, 7, Fade(RED_COLOR, 0.35f));
            DrawCircleLines(dx + 5, dy + 5, 7, RED_COLOR);
            DrawText(TextFormat("%d", day), dx, dy, 10, RED_COLOR);
        }
        else
        {
            DrawText(TextFormat("%2d", day), dx, dy, 10, (Color){70, 65, 58, 255});
        }
    }

    /* Pista manuscrita sobre Alan Turing / 1950 */
    if (game->foundCalendar)
    {
        Rectangle postIt = {92, 268, 134, 32};
        DrawRectangleRec(postIt, (Color){245, 230, 110, 255});
        DrawRectangleLinesEx(postIt, 1, (Color){210, 190, 80, 255});
        DrawText("1950 - TURING TEST", 96, 273, 11, (Color){160, 40, 40, 255});
        DrawText("Computing Machinery", 96, 286, 9, (Color){70, 50, 40, 255});
    }
    else
    {
        DrawText("[ ? ]", 140, 278, 13, GRAY);
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

static void DrawHud(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawText("ARQUIVO 01 // 1990", 28, 20, 22, CYAN_COLOR);
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
        bool is1990 = (i == 0);
        bool hover = CheckCollisionPointRec(GetVirtualMouse(), tabRect);

        Color tabBg;
        Color tabBorder;
        Color tabText;

        if (is1990)
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

        if (is1990)
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

    if (game->journalYearTab != 0)
    {
        /* ========================================================= */
        /* TELA DE ANO BLOQUEADO (2008, 2026, 2048)                  */
        /* ========================================================= */
        const char *lockedYear = tabLabels[game->journalYearTab];
        int phaseNumber = game->journalYearTab + 1;

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
        DrawText("A missao atual esta restrita a Fase 1 (Ano de 1990).", bookX + 35, bookY + 272, 16, INK_COLOR);
        DrawText("Por esse motivo, apenas as duas paginas de 1990", bookX + 35, bookY + 302, 16, INK_COLOR);
        DrawText("estao desbloqueadas para a investigacao do prototipo.", bookX + 35, bookY + 332, 16, INK_COLOR);

        DrawText(TextFormat("Para avancar para %s, conclua os desafios", lockedYear), bookX + 35, bookY + 380, 16, (Color){115, 75, 45, 255});
        DrawText("e desvende o codigo no computador do laboratorio.", bookX + 35, bookY + 406, 16, (Color){115, 75, 45, 255});

        Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
        bool retHoverL = CheckCollisionPointRec(GetVirtualMouse(), retBtnL);
        DrawRectangleRounded(retBtnL, 0.2f, 6, retHoverL ? (Color){195, 65, 50, 255} : (Color){135, 45, 30, 255});
        DrawRectangleRoundedLinesEx(retBtnL, 0.2f, 6, 2, GOLD_COLOR);
        DrawText("<< RETORNAR AO ARQUIVO DE 1990 >>", bookX + 85, bookY + 463, 17, RAYWHITE);

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
    else
    {
        /* ========================================================= */
        /* ANO 1990 (DISPONIVEL - EXATAMENTE DUAS PAGINAS: 0 E 1)    */
        /* ========================================================= */
        int page = game->journalPage;

        if (page == 0)
        {
            /* ----------------------------------------------------- */
            /* PAGINA 1 DE 1990: REGISTRO DE OPERACOES & SINTESE     */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawText("DIARIO DE PESQUISA // PROJETO A.R.1.3.L.", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawText("REGISTRO DE OPERACOES", bookX + 32, bookY + 68, 18, (Color){125, 35, 18, 255});
            DrawText("Acesso ao terminal protegido por chave cronologica.", bookX + 32, bookY + 92, 15, INK_COLOR);
            DrawText("Pistas no laboratorio:", bookX + 32, bookY + 116, 16,
                     ClueCount(game) == 5 ? (Color){20, 120, 45, 255} : (Color){145, 65, 30, 255});

            /* 5 Caixas de pistas com fonte grande e legivel */
            const char *itemTitlesFound[5] = {
                "Calendario: Marco Fundamental da IA (1950)",
                "Livros: Batismo da IA em Dartmouth (1956)",
                "Gaveta: Disquete de Armazenamento (1990)",
                "Esquema: O Perceptron de Rosenblatt (1958)",
                "Gravador: Fita com Voz da Dra. Ramos (1990)"
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
            DrawText("ARQUIVO HISTORICO 1990", bookX + 288, bookY + 396, 12, (Color){175, 45, 45, 255});

            DrawText("Investigue os objetos do quarto para preencher", bookX + 32, bookY + 430, 15, INK_COLOR);
            DrawText("as folhas do diario com os dados do prototipo.", bookX + 32, bookY + 454, 15, INK_COLOR);
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
            DrawText("- Clique em 'Proxima [E]' para acessar a Pagina 2 de 1990.", bookX + 560, bookY + 412, 15, INK_COLOR);
            DrawText("- As abas com cadeado pertencem a outros anos.", bookX + 560, bookY + 438, 15, INK_COLOR);
            DrawText("- Pressione [D] para retornar ao quarto.", bookX + 560, bookY + 464, 15, INK_COLOR);
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 1990: DOSSIE HISTORICO & ARTEFATOS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda: 1950 Turing & 1956 Dartmouth */
            DrawText("DOSSIE HISTORICO // MARCOS DA IA", bookX + 32, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawText("DRA. ELIRA RAMOS - NOTAS CIENTIFICAS (1990)", bookX + 32, bookY + 44, 15, (Color){110, 75, 50, 255});
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

            /* Pagina Direita: 1958 Perceptron & 1990 Midias / Gravacao */
            DrawText("ARTEFATOS E PROTOTIPO // 1990", bookX + 560, bookY + 20, 20, (Color){125, 35, 18, 255});
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

            /* Bloco 1990 Midias e Gravacao */
            Rectangle bAudio = {(float)(bookX + 560), (float)(bookY + 298), 448, 215};
            bool has1990Media = (game->foundDrawer || game->foundTape);
            DrawRectangleRounded(bAudio, 0.08f, 6, has1990Media ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bAudio, 0.08f, 6, 1.5f, has1990Media ? (Color){45, 135, 65, 255} : GRAY);

            if (has1990Media)
            {
                DrawText("1990 // DADOS, MEMORIA E GRAVACAO", bookX + 574, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawText("[CONFIRMADO // ARTEFATOS DE 1990]", bookX + 574, bookY + 336, 14, (Color){25, 125, 50, 255});
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
                DrawText("1990 // ARTEFATOS DA SALA [BLOQUEADOS]", bookX + 630, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawText("Abra a gaveta da mesa e ouca a fita no gravador", bookX + 630, bookY + 390, 15, INK_COLOR);
                DrawText("para registrar a transcricao da Dra. Ramos.", bookX + 630, bookY + 415, 15, INK_COLOR);
            }
        }

        /* ========================================================= */
        /* BARRA DE NAVEGACAO INFERIOR (PAGINACAO DE 1990: 2 PAGINAS) */
        /* ========================================================= */
        /* Botao Anterior */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        /* Indicador de pagina no rodape da Pagina Esquerda (LONGE DA LOMBADA E DA FITA) */
        DrawText(TextFormat("Pagina %d de 2", page + 1), bookX + 225, footY + 8, 18, (Color){125, 35, 18, 255});

        /* Botao Fechar Diario */
        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        /* Botao Proxima */
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

        /* Se estiver no ano 1990 (ano ativo, tab 0) */
        if (game->journalYearTab == 0)
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

        if (game->journalYearTab == 0)
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
            /* Se estiver em ano bloqueado, clique no botao de retorno volta para 1990 */
            Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
            if (Clicked(retBtnL))
            {
                game->journalYearTab = 0;
                game->journalPage = 0;
            }
        }

        if (Clicked(closeBtn))
        {
            game->journalOpen = false;
        }

        /* Clique nas abas laterais a direita (1990, 2008, 2026, 2048) */
        int tabX = bookX + 1040;
        int tabW = 86;
        int tabH = 54;
        int tabGap = 66;
        for (int i = 0; i < 4; i++)
        {
            int tabY = bookY + 42 + i * tabGap;
            Rectangle tabRect = {(float)tabX, (float)tabY, (float)tabW, (float)tabH};
            if (Clicked(tabRect))
            {
                game->journalYearTab = i;
                if (i == 0)
                {
                    /* Ao clicar em 1990, garante pagina valida */
                    if (game->journalPage > 1) game->journalPage = 0;
                }
                break;
            }
        }

        return;
    }

    if (Clicked(calendar))
    {
        game->foundCalendar = true;
        game->journalYearTab = 0;
        game->journalPage = 1;
        SetMessage(game, "Pista: o Teste de Turing foi proposto em 1950.");
    }
    else if (Clicked(books))
    {
        game->foundBooks = true;
        game->journalYearTab = 0;
        game->journalPage = 1;
        SetMessage(game, "Registro: a IA recebeu seu nome em 1956.");
    }
    else if (Clicked(drawer))
    {
        game->foundDrawer = true;
        game->journalYearTab = 0;
        game->journalPage = 1;
        SetMessage(game, "A gaveta contem o disquete com os dados de treino.");
    }
    else if (Clicked(blueprint))
    {
        game->foundBlueprint = true;
        game->journalYearTab = 0;
        game->journalPage = 1;
        SetMessage(game, "Esquema: o Perceptron foi apresentado em 1958.");
    }
    else if (Clicked(tape))
    {
        game->foundTape = true;
        game->journalYearTab = 0;
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
    DrawText("> BOOT MEMORY_NODE_1990", 130, 145, 20,
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
    DrawText("DISQUETE // 1990", 90, 108, 46, RAYWHITE);
    DrawFloppy((Vector2){160, 250}, 1.35f);
    DrawRectangleRounded((Rectangle){505, 205, 670, 320}, 0.04f, 8,
                         Fade(PANEL_COLOR, 0.96f));
    DrawText("MEMORIA DESBLOQUEADA", 545, 240, 20, GOLD_COLOR);
    DrawText("O prototipo inicial de A.R.1.3.L nao nasceu como uma arma.", 545,
             292, 20, LIGHTGRAY);
    DrawText("Os dados indicam um projeto familiar dedicado a preservar", 545,
             329, 20, LIGHTGRAY);
    DrawText("uma consciencia. Ainda faltam fragmentos para entender", 545, 366,
             20, LIGHTGRAY);
    DrawText("como esse objetivo foi corrompido.", 545, 403, 20, LIGHTGRAY);
    DrawText(TextFormat("TEMPO: %02d:%02d", (int)game->elapsedTime / 60,
                        (int)game->elapsedTime % 60),
             545, 455, 22, CYAN_COLOR);
    DrawText(TextFormat("VIDAS: %d/3", game->lives), 800, 455, 22,
             game->lives == 3 ? GOLD_COLOR : RAYWHITE);
    DrawText(TextFormat("PERFIL: %s", game->playerName), 545, 525, 17, GRAY);
    DrawText("ROTA DO FINAL OTIMO AUMENTADA", 545, 492, 17,
             (Color){101, 232, 151, 255});
    DrawButton((Rectangle){410, 590, 230, 56}, "JOGAR NOVAMENTE", CYAN_COLOR);
    DrawButton((Rectangle){660, 590, 210, 56}, "MENU INICIAL", GOLD_COLOR);
}

static void DrawFailure(void)
{
    DrawBackground();
    CenterText("CONEXAO INTERROMPIDA", 160, 42, RED_COLOR);
    CenterText("A.R.1.3.L detectou a invasao temporal.", 245, 23, LIGHTGRAY);
    CenterText("O artefato de 1990 permanece bloqueado.", 282, 23, LIGHTGRAY);
    CenterText("Revise as pistas no diario e tente novamente.", 350, 19, GRAY);
    DrawButton((Rectangle){490, 460, 300, 60}, "REINICIAR SALA", RED_COLOR);
    DrawButton((Rectangle){520, 550, 240, 48}, "MENU INICIAL", CYAN_COLOR);
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
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "A.R.I.3.L. - Escape Run Temporal");
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
        bool toggleAudio = IsKeyPressed(KEY_M) || (game.screen == SCREEN_ROOM && Clicked(audioHudBtn));
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
                    game.screen = SCREEN_ROOM;
                }
                break;
            case SCREEN_ROOM:
                UpdateRoom(&game);
                break;
            case SCREEN_TERMINAL:
                UpdateTerminal(&game);
                break;
            case SCREEN_RESULT:
                if (Clicked((Rectangle){410, 590, 230, 56}))
                {
                    ResetDemo(&game);
                    game.screen = SCREEN_ROOM;
                }
                else if (Clicked((Rectangle){660, 590, 210, 56}))
                    game.screen = SCREEN_TITLE;
                break;
            case SCREEN_FAILURE:
                if (Clicked((Rectangle){490, 460, 300, 60}))
                {
                    ResetDemo(&game);
                    game.screen = SCREEN_ROOM;
                }
                else if (Clicked((Rectangle){520, 550, 240, 48}))
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
