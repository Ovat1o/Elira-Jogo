#include "raylib.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ACCESS_CODE "1950"
#define PLAYER_NAME_MAX 20

typedef enum GameScreen
{
    SCREEN_TITLE,
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

static void CenterText(const char *text, int y, int size, Color color)
{
    DrawText(text, (SCREEN_WIDTH - MeasureText(text, size)) / 2, y, size, color);
}

static void DrawButton(Rectangle bounds, const char *label, Color accent)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
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
           CheckCollisionPointRec(GetMousePosition(), bounds);
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

    ClearBackground((Color){3, 7, 18, 255});
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           (Color){12, 27, 54, 255},
                           (Color){3, 7, 18, 255});

    BeginBlendMode(BLEND_ADDITIVE);
    for (int radius = 310; radius >= 70; radius -= 24)
    {
        float alpha = 0.008f + (310 - radius) * 0.00006f;
        DrawCircle(760, 280, (float)radius,
                   Fade((Color){38, 92, 150, 255}, alpha));
    }
    for (int radius = 220; radius >= 55; radius -= 22)
    {
        float alpha = 0.008f + (220 - radius) * 0.00008f;
        DrawCircle(1115, 190, (float)radius,
                   Fade((Color){106, 48, 127, 255}, alpha));
    }
    EndBlendMode();

    for (int i = 0; i < 150; i++)
    {
        int x = (i * 83 + i * i * 7 + 29) % SCREEN_WIDTH;
        int y = (i * 47 + i * i * 3 + 17) % SCREEN_HEIGHT;
        float pulse = 0.58f + 0.24f * sinf(time * (0.7f + (i % 5) * 0.1f) + i);
        float radius = (i % 19 == 0) ? 2.0f : ((i % 7 == 0) ? 1.4f : 0.9f);
        Color star = (i % 11 == 0) ? (Color){141, 218, 255, 255} : RAYWHITE;
        DrawCircle(x, y, radius, Fade(star, pulse));
        if (i % 31 == 0)
        {
            DrawLine(x - 5, y, x + 5, y, Fade(star, pulse * 0.55f));
            DrawLine(x, y - 5, x, y + 5, Fade(star, pulse * 0.55f));
        }
    }

    DrawEllipseLines(1030, 370, 220, 72, Fade(CYAN_COLOR, 0.22f));
    DrawEllipseLines(1030, 370, 244, 84, Fade(RAYWHITE, 0.08f));
    DrawCircleGradient((Vector2){1030, 370}, 154,
                       (Color){80, 147, 181, 255},
                       (Color){17, 35, 64, 255});
    DrawCircle(1087, 350, 132, Fade((Color){2, 7, 19, 255}, 0.72f));
    DrawCircleGradient((Vector2){980, 318}, 34,
                       Fade(RAYWHITE, 0.19f), BLANK);
    DrawEllipseLines(1030, 370, 170, 34, Fade(CYAN_COLOR, 0.13f));
    DrawEllipseLines(1030, 394, 142, 22, Fade(GOLD_COLOR, 0.12f));

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

static void DrawTitle(void)
{
    Rectangle startButton = {86, 472, 350, 66};

    DrawTitleSpaceBackground();
    DrawText("ARQUIVO DE MEMORIA  //  TRANSMISSAO 01", 90, 116, 17,
             Fade(CYAN_COLOR, 0.75f));
    DrawText("A.R.I.3.L.", 82, 164, 96, RAYWHITE);
    DrawRectangle(88, 274, 554, 3, CYAN_COLOR);
    DrawRectangle(88, 281, 218, 2, GOLD_COLOR);
    DrawText("ESCAPE RUN TEMPORAL", 90, 307, 29, CYAN_COLOR);
    DrawText("ENTRE NAS MEMORIAS DA MAQUINA", 91, 355, 19,
             Fade(RAYWHITE, 0.72f));
    DrawText("Descubra o que existia antes de A.R.I.3.L. controlar o futuro.",
             91, 386, 18, GRAY);
    DrawButton(startButton, "INICIAR MISSAO", CYAN_COLOR);
    DrawText("ENTER ou ESPACO", 101, 555, 16, Fade(GOLD_COLOR, 0.82f));
    DrawText("DEMO 1990  //  O PROTOTIPO", 91, 633, 16,
             Fade(LIGHTGRAY, 0.62f));
    DrawText("Point & click narrativo", 946, 650, 16,
             Fade(LIGHTGRAY, 0.50f));
}

static void DrawPlayerName(const GameState *game)
{
    Rectangle field = {360, 330, 560, 64};
    bool focused = CheckCollisionPointRec(GetMousePosition(), field);

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
    if (!CheckCollisionPointRec(GetMousePosition(), bounds)) return;
    int width = MeasureText(label, 17) + 20;
    int x = (int)(bounds.x + (bounds.width - width) / 2);
    int y = (int)(bounds.y - 32);
    DrawRectangleRoundedLinesEx(bounds, 0.08f, 8, 2, CYAN_COLOR);
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)width, 26},
                         0.25f, 6, PANEL_COLOR);
    DrawText(label, x + 10, y + 4, 17, CYAN_COLOR);
}

static void DrawRoom(const GameState *game)
{
    Rectangle calendar = {90, 128, 155, 164};
    Rectangle books = {1010, 160, 172, 330};
    Rectangle drawer = {395, 505, 230, 92};
    Rectangle computer = {495, 230, 310, 260};
    Rectangle blueprint = {815, 132, 160, 125};
    Rectangle tape = {825, 390, 125, 75};

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

    /* Janela para o espaco, reforcando que a sala fica na nave. */
    DrawRectangleRounded((Rectangle){270, 120, 200, 145}, 0.08f, 8,
                         (Color){18, 25, 34, 255});
    DrawRectangleRounded((Rectangle){282, 132, 176, 121}, 0.06f, 8,
                         (Color){5, 12, 27, 255});
    DrawCircle(315, 160, 2, RAYWHITE);
    DrawCircle(354, 218, 1, RAYWHITE);
    DrawCircle(412, 151, 2, Fade(CYAN_COLOR, 0.85f));
    DrawCircle(438, 205, 1, RAYWHITE);
    DrawCircle(381, 181, 2, GOLD_COLOR);
    DrawCircle(302, 228, 1, RAYWHITE);
    DrawCircleGradient((Vector2){442, 230}, 28,
                       (Color){61, 121, 173, 255},
                       (Color){12, 27, 47, 255});
    DrawLine(370, 132, 370, 253, (Color){48, 61, 70, 255});
    DrawLine(282, 193, 458, 193, (Color){48, 61, 70, 255});

    /* Canos, cabos e sinalizacao de seguranca. */
    DrawRectangle(16, 118, 18, 405, (Color){70, 78, 78, 255});
    DrawRectangle(34, 118, 34, 14, (Color){87, 94, 91, 255});
    DrawRectangle(17, 245, 16, 32, (Color){118, 64, 52, 255});
    for (int x = 252; x < 1028; x += 42)
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

    DrawRectangleRec(calendar, (Color){218, 210, 187, 255});
    DrawRectangle((int)calendar.x, (int)calendar.y, (int)calendar.width, 34,
                  (Color){157, 52, 54, 255});
    DrawText("OUTUBRO", 127, 136, 18, RAYWHITE);
    DrawText("1990", 133, 180, 32, INK_COLOR);
    DrawText(game->foundCalendar ? "TURING?" : "?", 137, 238, 18,
             game->foundCalendar ? (Color){157, 52, 54, 255} : GRAY);

    DrawRectangleRec(books, (Color){74, 51, 37, 255});
    for (int shelf = 0; shelf < 4; shelf++)
    {
        int shelfY = 225 + shelf * 70;
        DrawRectangle(1022, shelfY, 148, 7, (Color){42, 28, 23, 255});
        for (int book = 0; book < 5; book++)
        {
            Color cover = ((book + shelf) % 2 == 0)
                              ? (Color){51, 89, 103, 255}
                              : (Color){122, 70, 58, 255};
            DrawRectangle(1028 + book * 27, shelfY - 49 + book % 2 * 6,
                          20, 49 - book % 2 * 6, cover);
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

    DrawEllipse(625, 612, 340, 32, Fade(BLACK, 0.35f));
    DrawRectangle(320, 465, 610, 38, (Color){128, 89, 59, 255});
    DrawRectangle(320, 496, 610, 12, (Color){72, 51, 42, 255});
    DrawRectangle(350, 510, 540, 105, (Color){91, 62, 45, 255});
    DrawRectangleGradientV(350, 510, 540, 105,
                           Fade((Color){126, 87, 58, 255}, 0.45f),
                           (Color){73, 51, 43, 255});
    DrawLine(350, 510, 890, 510, (Color){147, 100, 63, 255});
    DrawLine(350, 615, 890, 615, (Color){42, 33, 31, 255});
    DrawRectangleRec(drawer, (Color){105, 73, 51, 255});
    DrawRectangleLinesEx(drawer, 3, (Color){62, 42, 33, 255});
    DrawCircle(510, 550, 6, GOLD_COLOR);

    /* Cadeira, sombras e placas do piso. */
    DrawEllipse(785, 620, 105, 18, Fade(BLACK, 0.42f));
    DrawRectangleRounded((Rectangle){735, 525, 118, 82}, 0.18f, 8,
                         (Color){39, 48, 54, 255});
    DrawRectangleRoundedLinesEx((Rectangle){735, 525, 118, 82}, 0.18f, 8, 3,
                                (Color){78, 91, 97, 255});
    DrawLineEx((Vector2){760, 607}, (Vector2){735, 633}, 6,
               (Color){51, 58, 62, 255});
    DrawLineEx((Vector2){828, 607}, (Vector2){852, 633}, 6,
               (Color){51, 58, 62, 255});
    DrawRectangleRounded((Rectangle){500, 223, 300, 218}, 0.08f, 10,
                         (Color){184, 176, 147, 255});
    DrawRectangleRounded((Rectangle){528, 249, 244, 145}, 0.06f, 8,
                         (Color){17, 32, 28, 255});
    DrawText("A.R.1.3.L // BOOT", 548, 269, 18, (Color){94, 225, 150, 255});
    DrawText("ACESSO BLOQUEADO", 548, 305, 17, (Color){94, 225, 150, 255});
    DrawText("CODIGO: ____", 548, 345, 18, (Color){94, 225, 150, 255});
    DrawCircle(752, 417, 5, RED_COLOR);
    DrawRectangle(580, 441, 140, 24, (Color){162, 153, 128, 255});
    DrawLineEx((Vector2){650, 465}, (Vector2){705, 530}, 5,
               (Color){25, 25, 28, 255});

    /* Teclado, gravador e objetos que tornam a bancada mais viva. */
    DrawRectangleRounded((Rectangle){520, 470, 270, 34}, 0.12f, 6,
                         (Color){154, 148, 127, 255});
    for (int row = 0; row < 3; row++)
        for (int key = 0; key < 12; key++)
            DrawRectangle(530 + key * 20, 474 + row * 8, 14, 5,
                          (Color){79, 81, 76, 255});

    DrawRectangleRounded(tape, 0.08f, 8, (Color){60, 67, 69, 255});
    DrawRectangleRoundedLinesEx(tape, 0.08f, 8, 2,
                                (Color){126, 135, 133, 255});
    DrawCircle(857, 426, 18, (Color){26, 30, 31, 255});
    DrawCircle(917, 426, 18, (Color){26, 30, 31, 255});
    DrawCircleLines(857, 426, 10, GRAY);
    DrawCircleLines(917, 426, 10, GRAY);
    DrawRectangle(875, 418, 24, 16, (Color){164, 151, 119, 255});
    DrawCircle(936, 403, 4, game->foundTape ? CYAN_COLOR : RED_COLOR);

    DrawRectangle(370, 417, 30, 48, (Color){118, 122, 113, 255});
    DrawRectangle(366, 413, 38, 7, (Color){157, 159, 147, 255});
    DrawLineEx((Vector2){390, 417}, (Vector2){383, 390}, 3,
               (Color){43, 45, 43, 255});

    DrawHotspot(calendar, "Examinar calendario");
    DrawHotspot(books, "Investigar livros");
    DrawHotspot(drawer, "Abrir gaveta");
    DrawHotspot(blueprint, "Analisar esquema");
    DrawHotspot(tape, "Ouvir gravacao");
    DrawHotspot(computer, "Usar terminal");
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
    DrawText("[D] DIARIO", 1110, 675, 18, GOLD_COLOR);

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

static void DrawJournal(const GameState *game)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.68f));
    DrawRectangleRounded((Rectangle){180, 76, 920, 568}, 0.025f, 6, PAPER_COLOR);
    DrawRectangleRoundedLinesEx((Rectangle){180, 76, 920, 568}, 0.025f, 6, 3,
                                (Color){118, 91, 57, 255});
    DrawText("DIARIO TEMPORAL DE ELIRA", 230, 112, 30, INK_COLOR);
    DrawText("ARQUIVO 1990: O PROTOTIPO", 230, 154, 18,
             (Color){117, 72, 48, 255});
    DrawLine(230, 188, 1050, 188, Fade(INK_COLOR, 0.35f));
    DrawText(game->foundCalendar
                 ? "[1] 1950 - Alan Turing propoe seu teste para maquinas."
                 : "[1] Registro ainda nao encontrado.",
             230, 212, 18, game->foundCalendar ? INK_COLOR : GRAY);
    DrawText(game->foundBooks
                 ? "[2] 1956 - O termo Inteligencia Artificial ganha nome."
                 : "[2] Registro ainda nao encontrado.",
             230, 252, 18, game->foundBooks ? INK_COLOR : GRAY);
    DrawText(game->foundDrawer
                 ? "[3] Disquetes armazenavam dados em midia magnetica."
                 : "[3] Registro ainda nao encontrado.",
             230, 292, 18, game->foundDrawer ? INK_COLOR : GRAY);
    DrawText(game->foundBlueprint
                 ? "[4] 1958 - O Perceptron foi um modelo pioneiro de rede neural."
                 : "[4] Registro ainda nao encontrado.",
             230, 332, 18, game->foundBlueprint ? INK_COLOR : GRAY);
    DrawText(game->foundTape
                 ? "[5] Uma IA aprende padroes a partir dos dados que recebe."
                 : "[5] Registro ainda nao encontrado.",
             230, 372, 18, game->foundTape ? INK_COLOR : GRAY);

    DrawRectangleRounded((Rectangle){230, 425, 820, 112}, 0.06f, 8,
                         Fade((Color){168, 142, 93, 255}, 0.20f));
    DrawText("ANOTACAO DE ELIRA", 255, 447, 18, (Color){117, 72, 48, 255});
    if (ClueCount(game) == 5)
    {
        DrawText("A origem da IA combina memoria, dados e tentativas de", 255, 480,
                 18, INK_COLOR);
        DrawText("imitar o pensamento. O codigo deve estar no primeiro registro.",
                 255, 507, 18, INK_COLOR);
    }
    else
    {
        DrawText("A sala guarda cinco fragmentos. Preciso observar cada", 255, 480,
                 18, INK_COLOR);
        DrawText("objeto antes de compreender a memoria de A.R.1.3.L.", 255, 507,
                 18, INK_COLOR);
    }
    CenterText("Pressione D ou ESC para fechar", 592, 17, INK_COLOR);
}

static void UpdateRoom(GameState *game)
{
    Rectangle calendar = {90, 128, 155, 164};
    Rectangle books = {1010, 160, 172, 330};
    Rectangle drawer = {395, 505, 230, 92};
    Rectangle computer = {495, 230, 310, 260};
    Rectangle blueprint = {815, 132, 160, 125};
    Rectangle tape = {825, 390, 125, 75};
    if (IsKeyPressed(KEY_D)) game->journalOpen = !game->journalOpen;
    if (game->journalOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE)) game->journalOpen = false;
        return;
    }
    if (Clicked(calendar))
    {
        game->foundCalendar = true;
        SetMessage(game, "Pista: o Teste de Turing foi proposto em 1950.");
    }
    else if (Clicked(books))
    {
        game->foundBooks = true;
        SetMessage(game, "Registro: a IA recebeu seu nome em 1956.");
    }
    else if (Clicked(drawer))
    {
        game->foundDrawer = true;
        SetMessage(game, "A gaveta vazia tem o desenho de um disquete.");
    }
    else if (Clicked(blueprint))
    {
        game->foundBlueprint = true;
        SetMessage(game, "Esquema: o Perceptron foi apresentado em 1958.");
    }
    else if (Clicked(tape))
    {
        game->foundTape = true;
        SetMessage(game, "Gravacao: toda IA reflete os dados usados em seu aprendizado.");
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
    Vector2 mouse = GetMousePosition();
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

int main(void)
{
    GameState game = {0};
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "A.R.I.3.L. - Escape Run Temporal");
    SetTargetFPS(60);
    game.screen = SCREEN_TITLE;
    ResetDemo(&game);

    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();
        if (game.messageTimer > 0) game.messageTimer -= delta;
        switch (game.screen)
        {
            case SCREEN_TITLE:
                if (Clicked((Rectangle){86, 472, 350, 66}) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                    game.screen = SCREEN_PLAYER_NAME;
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

        BeginDrawing();
        switch (game.screen)
        {
            case SCREEN_TITLE: DrawTitle(); break;
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
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
