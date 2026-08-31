#include "raylib.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ACCESS_CODE "1950"

typedef enum GameScreen
{
    SCREEN_TITLE,
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

static void DrawTitle(void)
{
    Vector2 center = {SCREEN_WIDTH / 2.0f, 245.0f};
    DrawBackground();
    for (int radius = 150; radius >= 45; radius -= 18)
    {
        float alpha = 0.05f + (150 - radius) * 0.002f;
        DrawRing(center, (float)radius - 4, (float)radius, 0, 360, 96,
                 Fade(CYAN_COLOR, alpha));
    }
    DrawCircleV(center, 38, Fade(GOLD_COLOR, 0.22f));
    DrawCircleLines((int)center.x, (int)center.y, 38, GOLD_COLOR);
    CenterText("DECIFRA.IA", 200, 58, RAYWHITE);
    CenterText("ESCAPE RUN TEMPORAL", 280, 20, CYAN_COLOR);
    CenterText("DEMO 1990  |  O PROTOTIPO", 328, 18, GRAY);
    DrawButton((Rectangle){490, 430, 300, 62}, "INICIAR MISSAO", CYAN_COLOR);
    CenterText("Point & click narrativo", 530, 18, DARKGRAY);
    CenterText("Mouse para investigar  |  D para abrir o diario", 570, 18, GRAY);
}

static void DrawIntro(void)
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

    ClearBackground((Color){35, 43, 50, 255});
    DrawRectangleGradientV(0, 70, SCREEN_WIDTH, 465,
                           (Color){62, 73, 79, 255}, (Color){33, 42, 47, 255});
    DrawRectangleGradientV(0, 535, SCREEN_WIDTH, 185,
                           (Color){55, 44, 38, 255}, (Color){25, 23, 23, 255});

    /* Estrutura metalica da nave e iluminacao do teto. */
    DrawRectangle(0, 70, SCREEN_WIDTH, 32, (Color){22, 31, 38, 255});
    DrawRectangle(0, 101, SCREEN_WIDTH, 5, (Color){10, 17, 23, 255});
    for (int x = 20; x < SCREEN_WIDTH; x += 210)
    {
        DrawRectangle(x, 78, 150, 12, (Color){119, 135, 133, 255});
        DrawRectangle(x + 8, 81, 134, 6, Fade(CYAN_COLOR, 0.35f));
    }
    for (int x = 0; x < SCREEN_WIDTH; x += 255)
        DrawLine(x, 106, x + 40, 535, Fade(BLACK, 0.18f));
    DrawLine(0, 362, SCREEN_WIDTH, 362, Fade(BLACK, 0.22f));
    for (int x = 80; x < SCREEN_WIDTH; x += 170)
        DrawCircle(x, 112, 3, (Color){91, 105, 109, 255});

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
    for (int x = 0; x < SCREEN_WIDTH; x += 42)
    {
        Color stripe = (x / 42) % 2 == 0 ? GOLD_COLOR : (Color){33, 34, 35, 255};
        DrawRectangle(x, 522, 42, 13, stripe);
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

    DrawRectangle(320, 465, 610, 45, (Color){117, 82, 56, 255});
    DrawRectangle(350, 510, 540, 105, (Color){91, 62, 45, 255});
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
    for (int x = 50; x < SCREEN_WIDTH; x += 220)
        DrawRectangleLines(x, 548, 175, 82, Fade((Color){91, 80, 73, 255}, 0.28f));

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

    DrawRectangle(960, 408, 30, 57, (Color){118, 122, 113, 255});
    DrawRectangle(956, 404, 38, 7, (Color){157, 159, 147, 255});
    DrawLineEx((Vector2){970, 408}, (Vector2){950, 350}, 3,
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
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DecifraIA - Escape Run Temporal");
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
                if (Clicked((Rectangle){490, 430, 300, 62}) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                    game.screen = SCREEN_INTRO;
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
            case SCREEN_INTRO: DrawIntro(); break;
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
