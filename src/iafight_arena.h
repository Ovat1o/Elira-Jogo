/* =============================================================================
 * iafight_arena.h - IA FIGHT: desenho da ARENA e do HUD da luta.
 * Incluido uma unica vez dentro de iafight.c (depois do estado da luta), como o
 * iafight_elira.h: usa o estado privado do modo (fz, wins, rstate, gt...) e os
 * utilitarios de desenho de la. Nao muda regra de jogo, so desenha.
 *
 * Planos da arena (de tras para frente):
 *   3. parede do fundo, racks distantes e o telao alto e discreto
 *   2. racks do meio com LEDs piscando, plateia atras da grade (reage a golpes
 *      fortes e ao nocaute)
 *   1. racks escuros nas laterais e o piso polido com reflexo leve dos lutadores
 * Tudo em primitivas da raylib, sem filtro de tela.
 * ============================================================================= */
#ifndef IAFIGHT_ARENA_H
#define IAFIGHT_ARENA_H

#define AR_CROWD_RAIL_Y 556.0f            /* topo da grade que separa a plateia do piso */
#define AR_FLOOR_Y (IAF_GROUND_Y - 30.0f) /* onde comeca o piso */

/* ------------------------------------------------------------------ reacao da plateia */
static float arHype = 0;          /* 0..1: empolgacao (golpe forte) */
static float arKoT = 0;           /* > 0: comemoracao do nocaute */
static Color arHypeCol = {96, 214, 240, 255};
static float arPrevHp[2] = {IAF_HP_MAX, IAF_HP_MAX};
static float arLastGt = 0;
static bool arKoSeen = false;

/* arena da luta: sorteada a cada luta (ArenaSortear, chamada em StartMatch) */
enum { AR_NUCLEO, AR_LAB1990, AR_COUNT };
static int arArena = AR_NUCLEO;
static void ArenaSortear(void) { arArena = GetRandomValue(0, AR_COUNT - 1); }
#define AR_LINE_COL (arArena == AR_LAB1990 ? C_AMBER : C_CYAN) /* cor das linhas do piso */

static unsigned Hash(unsigned x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static float HashF(unsigned x) { return (Hash(x) % 10000) / 10000.0f; }

/* Le o que aconteceu na luta (queda de vida, nocaute) e atualiza a empolgacao. */
static void ArenaReact(void)
{
    float dt = gt - arLastGt;
    if (dt < 0 || dt > 0.1f) dt = 0;
    arLastGt = gt;
    for (int k = 0; k < 2; k++)
    {
        float drop = arPrevHp[k] - fz[k].hp;
        if (drop >= 55) /* golpe forte ou especial (golpes fracos tiram ~50) */
        {
            arHype = fmaxf(arHype, Clamp(drop / 130.0f, 0.55f, 1.0f));
            arHypeCol = CHCOL[fz[1 - k].ch];
        }
        arPrevHp[k] = fz[k].hp;
    }
    bool ko = fz[0].st == ST_KO || fz[1].st == ST_KO;
    if (ko && !arKoSeen)
    {
        arKoT = 2.6f;
        arHype = 1;
        int w = fz[0].st == ST_KO ? 1 : 0;
        arHypeCol = CHCOL[fz[w].ch];
    }
    arKoSeen = ko;
    arHype = fmaxf(0, arHype - dt * 0.8f);
    arKoT = fmaxf(0, arKoT - dt);
}

/* ------------------------------------------------------------------ plano 3: fundo */
static void DrawRack(float x, float y, float w, float h, float depth, int seed)
{
    Color body = ColorLerp((Color){14, 17, 32, 255}, (Color){30, 36, 58, 255}, depth);
    Color edge = ColorLerp((Color){22, 27, 46, 255}, (Color){58, 68, 100, 255}, depth);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, body);
    DrawRectangle((int)x, (int)y, (int)w, 3, edge);
    DrawRectangle((int)x, (int)y, 2, (int)h, edge);
    DrawRectangle((int)(x + w - 3), (int)y, 3, (int)h, ColorLerp(body, BLACK, 0.4f));
    float slotH = 10 + 12 * depth;
    int slots = (int)((h - 8) / slotH);
    int leds = w > 60 ? 3 : 2;
    for (int j = 0; j < slots; j++)
    {
        float sy = y + 6 + j * slotH;
        DrawRectangle((int)x + 5, (int)sy, (int)w - 10, (int)(slotH - 5), ColorLerp(body, BLACK, 0.35f));
        for (int l = 0; l < leds; l++)
        {
            /* cada LED pisca no seu ritmo: um "tique" diferente por rack, gaveta e LED */
            unsigned hsh = Hash((unsigned)(seed * 977 + j * 31 + l * 7 + (int)(gt * (1.2f + l * 0.9f + (seed % 3) * 0.4f))));
            Color lc = (hsh % 6 == 0) ? (Color){255, 170, 80, 255} : ((hsh % 3 == 0) ? C_GREEN : C_CYAN);
            float on = (hsh % 4) ? 1.0f : 0.15f;
            float sz = 2 + 2 * depth;
            DrawRectangle((int)(x + w - 8 - (leds - l) * (sz + 3)), (int)(sy + (slotH - 5) / 2 - sz / 2), (int)sz, (int)sz,
                          Fade(lc, (0.25f + 0.7f * depth) * on));
        }
    }
}

/* telao alto e discreto: fica acima das cabecas (y < 170) e com pouco contraste */
static void DrawArenaScreen(void)
{
    Rectangle scr = {420, 106, 440, 54};
    for (int s = 0; s < 2; s++)
    { /* cabos que seguram o telao */
        float cx = scr.x + 40 + s * (scr.width - 80);
        DrawLineEx((Vector2){cx, 46}, (Vector2){cx, scr.y}, 2, (Color){26, 30, 52, 255});
    }
    DrawRectangleRounded((Rectangle){scr.x - 6, scr.y - 6, scr.width + 12, scr.height + 12}, 0.2f, 6, (Color){12, 14, 28, 255});
    DrawRectangleRounded(scr, 0.18f, 6, (Color){9, 18, 30, 255});
    DrawRectangleRoundedLinesEx(scr, 0.18f, 6, 1.5f, Fade(C_CYAN, 0.22f));
    Txt("RINHA DE INTELIGÊNCIAS", scr.x + 18, scr.y + 9, 18, Fade(C_CYAN, 0.55f));
    float blink = 0.35f + 0.35f * (sinf(gt * 4) > 0 ? 1 : 0);
    DrawCircle((int)(scr.x + 24), (int)(scr.y + 39), 4, Fade(C_RED, blink));
    Txt(arArena == AR_LAB1990 ? "AO VIVO  //  LABORATÓRIO 1990" : "AO VIVO  //  ARENA DO NÚCLEO",
        scr.x + 34, scr.y + 32, 13, Fade(C_DIM, 0.6f));
    /* sinal: fica mais agitado quando a plateia se empolga */
    float amp = 6 + 10 * fmaxf(arHype, arKoT > 0 ? 1.0f : 0.0f);
    Vector2 prev = {0};
    for (int i = 0; i <= 40; i++)
    {
        float xx = scr.x + 270 + i * 150 / 40.0f;
        float yy = scr.y + 27 + sinf(i * 0.6f + gt * 3) * amp * 0.5f * sinf(i * 0.17f + gt) + sinf(i * 1.9f - gt * 6) * amp * 0.25f;
        if (i) DrawLineEx(prev, (Vector2){xx, yy}, 1.5f, Fade(C_TEAL, 0.45f));
        prev = (Vector2){xx, yy};
    }
}

static void DrawArenaBack(void)
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, (int)AR_FLOOR_Y, (Color){5, 7, 18, 255}, (Color){16, 20, 42, 255});
    /* teto: trelica e dutos */
    DrawRectangle(0, 0, SCREEN_WIDTH, 42, (Color){10, 12, 24, 255});
    for (int i = 0; i < 9; i++) DrawRectangle(i * 150 + 10, 42, 110, 6, (Color){20, 24, 44, 255});
    for (int i = 0; i < 32; i++)
    {
        float x0 = i * 40.0f;
        DrawLineEx((Vector2){x0, 48}, (Vector2){x0 + 20, 62}, 1.5f, (Color){18, 22, 40, 255});
        DrawLineEx((Vector2){x0 + 20, 62}, (Vector2){x0 + 40, 48}, 1.5f, (Color){18, 22, 40, 255});
    }
    DrawRectangle(0, 62, SCREEN_WIDTH, 2, (Color){20, 24, 44, 255});
    /* fileira distante: racks pequenos e apagados */
    for (int i = 0; i < 27; i++) DrawRack(i * 50 - 10, 206, 42, 290, 0.0f, i + 80);
    /* luz baixa entre as fileiras (faixa do cenario, nao da tela) */
    /* tambem destaca os lutadores escuros (contorno contra um fundo um pouco mais claro) */
    DrawRectangleGradientV(0, 330, SCREEN_WIDTH, 226, Fade((Color){40, 70, 120, 255}, 0.0f), Fade((Color){40, 70, 120, 255}, 0.30f));
    DrawArenaScreen();
    /* fileira do meio */
    for (int i = 0; i < 16; i++)
    {
        if (i == 7 || i == 8) continue; /* corredor no centro: profundidade */
        DrawRack(i * 84 - 24, 262, 70, 290, 0.35f, i + 40);
    }
    /* corredor do centro: porta iluminada no fundo */
    DrawRectangle(586, 300, 108, 252, (Color){10, 13, 26, 255});
    DrawRectangleGradientV(600, 316, 80, 236, Fade(C_CYAN, 0.05f), Fade(C_CYAN, 0.16f));
    DrawRectangleLinesEx((Rectangle){600, 316, 80, 236}, 1, Fade(C_CYAN, 0.18f));
    /* cones de luz do teto sobre o ringue (parte do cenario) */
    for (int s = 0; s < 3; s++)
    {
        float cx = 300 + s * 340;
        Tri((Vector2){cx - 14, 50}, (Vector2){cx + 14, 50}, (Vector2){cx + 150, AR_FLOOR_Y}, Fade(C_CYAN, 0.035f));
        Tri((Vector2){cx - 14, 50}, (Vector2){cx + 150, AR_FLOOR_Y}, (Vector2){cx - 150, AR_FLOOR_Y}, Fade(C_CYAN, 0.035f));
        DrawRectangle((int)cx - 16, 44, 32, 8, (Color){40, 48, 76, 255});
        DrawRectangle((int)cx - 10, 52, 20, 2, Fade(C_WHITE, 0.6f));
    }
}

/* ------------------------------------------------------------------ plano 2: plateia */
static void DrawSpectator(float x, float y, float s, unsigned seed, bool front)
{
    float ph = HashF(seed) * 6.28f;
    float hype = fmaxf(arHype, arKoT > 0 ? 1.0f : 0.0f);
    bool excited = hype > 0.05f && HashF(seed * 3 + 1) < 0.35f + 0.6f * hype;
    float bob = sinf(gt * 1.6f + ph) * 1.2f;
    if (excited) bob -= fabsf(sinf(gt * (8 + HashF(seed + 9) * 3) + ph)) * 11 * hype;
    y += bob;
    Color body = front ? (Color){14, 17, 31, 255} : (Color){24, 28, 48, 255};
    body = ColorLerp(body, (Color){34, 30, 52, 255}, HashF(seed * 5 + 11) * 0.35f); /* roupas variadas */
    Color rim = Fade(ColorLerp(C_CYAN, C_WHITE, 0.2f), front ? 0.45f : 0.28f);
    /* bracos para cima na empolgacao */
    if (excited && hype > 0.4f)
    {
        float wave = sinf(gt * 10 + ph) * 6;
        DrawLineEx((Vector2){x - 11 * s, y - 20 * s}, (Vector2){x - 20 * s + wave, y - 52 * s}, 6 * s, body);
        DrawLineEx((Vector2){x + 11 * s, y - 20 * s}, (Vector2){x + 20 * s - wave, y - 52 * s}, 6 * s, body);
    }
    DrawRectangleRounded((Rectangle){x - 17 * s, y - 24 * s, 34 * s, 40 * s}, 0.5f, 6, body);
    DrawCircleV((Vector2){x, y - 34 * s}, 10 * s, body);
    DrawCircleSectorLines((Vector2){x, y - 34 * s}, 10 * s, 200, 340, 8, rim);
    DrawLineEx((Vector2){x - 12 * s, y - 23 * s}, (Vector2){x + 12 * s, y - 23 * s}, 1.5f, rim); /* luz nos ombros */
    /* terminal na mao: a tela brilha e pisca na cor de quem acertou */
    if (HashF(seed * 7 + 3) < 0.4f)
    {
        Color sc = (HashF(seed + 5) < 0.5f) ? C_CYAN : ((HashF(seed + 6) < 0.5f) ? C_GREEN : C_AMBER);
        float glow = 0.35f;
        if (hype > 0.05f)
        {
            sc = ColorLerp(sc, arHypeCol, 0.8f);
            glow = 0.45f + 0.5f * hype * (0.5f + 0.5f * sinf(gt * 14 + ph));
        }
        float tx = x + (HashF(seed + 2) < 0.5f ? -6 : 6) * s, ty = excited && hype > 0.4f ? y - 58 * s : y - 12 * s;
        DrawRectangleRec((Rectangle){tx - 8 * s, ty - 6 * s, 16 * s, 11 * s}, (Color){8, 10, 18, 255});
        DrawRectangleRec((Rectangle){tx - 6.5f * s, ty - 4.5f * s, 13 * s, 8 * s}, Fade(sc, glow));
    }
}

static void DrawCrowd(void)
{
    /* duas fileiras: a de tras menor e mais clara (mais longe) */
    for (int i = 0; i < 34; i++)
    {
        float x = 12 + i * 38 + (HashF(i + 300) - 0.5f) * 12;
        DrawSpectator(x, AR_CROWD_RAIL_Y - 30, 0.72f, 300 + i, false);
    }
    for (int i = 0; i < 27; i++)
    {
        float x = 20 + i * 48 + (HashF(i + 500) - 0.5f) * 14;
        DrawSpectator(x, AR_CROWD_RAIL_Y - 6, 0.95f, 500 + i, true);
    }
    /* grade da plateia: parede baixa com faixa de LED */
    DrawRectangleGradientV(0, (int)AR_CROWD_RAIL_Y, SCREEN_WIDTH, (int)(AR_FLOOR_Y - AR_CROWD_RAIL_Y), (Color){16, 19, 36, 255}, (Color){10, 12, 24, 255});
    DrawRectangle(0, (int)AR_CROWD_RAIL_Y, SCREEN_WIDTH, 3, (Color){40, 48, 78, 255});
    float hype = fmaxf(arHype, arKoT > 0 ? 1.0f : 0.0f);
    for (int i = 0; i < 32; i++)
    {
        float xx = 8 + i * 40;
        float on = 0.25f + 0.25f * (0.5f + 0.5f * sinf(gt * 2 + i * 0.5f));
        Color c = C_CYAN;
        if (hype > 0.05f)
        { /* a faixa corre na cor de quem acertou */
            c = ColorLerp(C_CYAN, arHypeCol, hype);
            on = 0.3f + 0.6f * hype * (0.5f + 0.5f * sinf(gt * 16 - i * 0.9f));
        }
        DrawRectangle((int)xx, (int)AR_CROWD_RAIL_Y + 12, 24, 4, Fade(c, on));
    }
}

/* ------------------------------------------------------------------ plano 1: laterais e piso */
static void DrawFrontRacks(void)
{
    for (int s = 0; s < 2; s++)
    {
        float x = s == 0 ? -46 : SCREEN_WIDTH - 104;
        DrawRack(x, 300, 150, 340, 0.8f, 7 + s * 13);
        /* sombra que o rack projeta no piso */
        DrawRectangleGradientH(s == 0 ? 104 : SCREEN_WIDTH - 164, (int)AR_FLOOR_Y, 60, 40,
                               s == 0 ? Fade(BLACK, 0.35f) : Fade(BLACK, 0.0f), s == 0 ? Fade(BLACK, 0.0f) : Fade(BLACK, 0.35f));
    }
}

static void DrawFloorBase(void)
{
    if (arArena == AR_LAB1990)
    { /* piso de ladrilhos do laboratorio */
        DrawRectangleGradientV(0, (int)AR_FLOOR_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)AR_FLOOR_Y, (Color){40, 34, 40, 255}, (Color){14, 12, 16, 255});
        return;
    }
    DrawRectangleGradientV(0, (int)AR_FLOOR_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)AR_FLOOR_Y, (Color){24, 28, 52, 255}, (Color){8, 10, 20, 255});
}

/* Reflexo leve: os lutadores espelhados no eixo dos pes, so na area do piso, cobertos
   por uma camada do proprio piso (o reflexo fica fraco e some para baixo). */
static void DrawFloorReflections(void)
{
    BeginScissorMode(0, (int)IAF_GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)IAF_GROUND_Y);
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlPushMatrix();
    rlTranslatef(0, 2 * IAF_GROUND_Y, 0);
    rlScalef(1, -1, 1);
    DrawFighter(&fz[0]);
    DrawFighter(&fz[1]);
    DrawProjectiles();
    rlPopMatrix();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    EndScissorMode();
    DrawRectangleGradientV(0, (int)IAF_GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)IAF_GROUND_Y,
                           arArena == AR_LAB1990 ? Fade((Color){34, 28, 34, 255}, 0.74f) : Fade((Color){18, 22, 42, 255}, 0.72f),
                           arArena == AR_LAB1990 ? Fade((Color){14, 12, 16, 255}, 0.97f) : Fade((Color){8, 10, 20, 255}, 0.97f));
}

static void DrawFloorLines(void)
{
    Vector2 vp = (Vector2){SCREEN_WIDTH / 2.0f, 380};
    for (int i = -12; i <= 12; i++)
    {
        float xb = SCREEN_WIDTH / 2.0f + i * 130;
        Vector2 a = (Vector2){Lerp(vp.x, xb, (AR_FLOOR_Y - vp.y) / (SCREEN_HEIGHT - vp.y)), AR_FLOOR_Y};
        DrawLineEx(a, (Vector2){xb, SCREEN_HEIGHT}, 1.5f, Fade(AR_LINE_COL, 0.08f));
    }
    for (int j = 0; j < 7; j++)
    {
        float yy = AR_FLOOR_Y + powf(j / 6.0f, 1.8f) * (SCREEN_HEIGHT - AR_FLOOR_Y);
        DrawLineEx((Vector2){0, yy}, (Vector2){SCREEN_WIDTH, yy}, 1, Fade(AR_LINE_COL, 0.06f + j * 0.012f));
    }
    /* borda do ringue */
    DrawRectangle(0, (int)IAF_GROUND_Y + 10, SCREEN_WIDTH, 3, Fade(C_CYAN, 0.5f));
    DrawRectangle(0, (int)IAF_GROUND_Y + 13, SCREEN_WIDTH, 6, Fade(C_CYAN, 0.10f));
    for (int i = 0; i < 20; i++)
    {
        float xx = 40 + i * 64;
        float on = 0.4f + 0.6f * (0.5f + 0.5f * sinf(gt * 2 + i * 0.7f));
        DrawRectangle((int)xx, (int)IAF_GROUND_Y + 9, 20, 5, Fade(C_CYAN, on));
    }
}

/* ------------------------------------------------------------------ 2a arena: laboratorio de 1990 */
/* armario de fita magnetica com dois rolos girando e lampadas piscando */
static void DrawTapeCabinet(float x, float y, float w, float h, int seed, float depth)
{
    Color body = ColorLerp((Color){44, 40, 44, 255}, (Color){92, 84, 74, 255}, depth);
    Color dark = ColorLerp(body, BLACK, 0.45f);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, body);
    DrawRectangle((int)x, (int)y, (int)w, 4, ColorLerp(body, WHITE, 0.15f));
    DrawRectangle((int)(x + w - 3), (int)y, 3, (int)h, dark);
    /* janela dos rolos */
    Rectangle win = {x + 7, y + 14, w - 14, w * 0.62f};
    DrawRectangleRec(win, (Color){14, 14, 20, 255});
    float r = win.width * 0.2f, spin = gt * (2.0f + (seed % 3)) * ((seed % 2) ? 1 : -1);
    for (int k = 0; k < 2; k++)
    {
        Vector2 c = {win.x + win.width * (k ? 0.72f : 0.28f), win.y + win.height * 0.5f};
        DrawCircleV(c, r, ColorLerp((Color){120, 100, 80, 255}, BLACK, 0.4f - 0.3f * depth));
        DrawCircleV(c, r * 0.3f, (Color){20, 20, 26, 255});
        for (int s = 0; s < 3; s++)
        {
            float an = spin + s * 2.094f;
            DrawLineEx((Vector2){c.x + cosf(an) * r * 0.32f, c.y + sinf(an) * r * 0.32f},
                       (Vector2){c.x + cosf(an) * r * 0.9f, c.y + sinf(an) * r * 0.9f}, 2, (Color){20, 20, 26, 255});
        }
    }
    /* painel de lampadas */
    float py = win.y + win.height + 10;
    for (int j = 0; j < 3; j++)
        for (int l = 0; l < 4; l++)
        {
            unsigned hsh = Hash((unsigned)(seed * 131 + j * 17 + l * 5 + (int)(gt * (1 + l * 0.7f))));
            Color lc = (hsh % 3 == 0) ? C_RED : ((hsh % 2) ? C_AMBER : C_GREEN);
            DrawCircle((int)(x + 12 + l * (w - 24) / 3), (int)(py + j * 12), 2.5f + depth, Fade(lc, (hsh % 4 ? 0.85f : 0.15f) * (0.4f + 0.6f * depth)));
        }
    for (int j = 0; py + 44 + j * 10 < y + h - 6; j++) /* grade de ventilacao */
        DrawRectangle((int)x + 8, (int)(py + 44 + j * 10), (int)w - 16, 3, dark);
}

/* terminal de fosforo verde sobre a mesa */
static void DrawCrt(float x, float y, float s, int seed)
{
    Color beige = (Color){150, 138, 112, 255};
    DrawRectangleRounded((Rectangle){x - 40 * s, y - 64 * s, 80 * s, 64 * s}, 0.18f, 6, beige);
    DrawRectangleRounded((Rectangle){x - 32 * s, y - 57 * s, 64 * s, 46 * s}, 0.2f, 6, (Color){10, 26, 16, 255});
    for (int l = 0; l < 5; l++)
    {
        unsigned hsh = Hash((unsigned)(seed * 37 + l + (int)(gt * 2)));
        float len = (18 + hsh % 36) * s;
        DrawRectangle((int)(x - 27 * s), (int)(y - 52 * s + l * 8 * s), (int)fminf(len, 54 * s), (int)(3 * s), Fade(C_GREEN, 0.55f));
    }
    if (fmodf(gt, 1.0f) < 0.5f) DrawRectangle((int)(x - 27 * s), (int)(y - 12 * s), (int)(6 * s), (int)(3 * s), C_GREEN);
    DrawRectangle((int)(x - 12 * s), (int)y, (int)(24 * s), (int)(8 * s), ColorLerp(beige, BLACK, 0.3f));
    DrawRectangle((int)(x - 44 * s), (int)(y + 8 * s), (int)(88 * s), (int)(10 * s), ColorLerp(beige, BLACK, 0.15f));
}

static void DrawLabBack(void)
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, (int)AR_FLOOR_Y, (Color){10, 9, 16, 255}, (Color){30, 26, 36, 255});
    /* teto com luminarias */
    DrawRectangle(0, 0, SCREEN_WIDTH, 42, (Color){16, 14, 22, 255});
    for (int i = 0; i < 4; i++)
    {
        float lx = 120 + i * 340;
        DrawRectangle((int)lx, 42, 140, 8, (Color){60, 56, 60, 255});
        DrawRectangle((int)lx + 6, 50, 128, 3, Fade((Color){255, 240, 200, 255}, 0.5f));
        Tri((Vector2){lx, 53}, (Vector2){lx + 140, 53}, (Vector2){lx + 220, AR_FLOOR_Y}, Fade((Color){255, 230, 180, 255}, 0.03f));
        Tri((Vector2){lx, 53}, (Vector2){lx + 220, AR_FLOOR_Y}, (Vector2){lx - 80, AR_FLOOR_Y}, Fade((Color){255, 230, 180, 255}, 0.03f));
    }
    /* parede: lambri e rodape */
    for (int i = 0; i < 26; i++) DrawRectangle(i * 52, 180, 2, 380, Fade(BLACK, 0.25f));
    DrawRectangle(0, 176, SCREEN_WIDTH, 4, (Color){50, 42, 44, 255});
    /* quadro-negro com as primeiras regras de Ariel */
    Rectangle board = {80, 192, 300, 98};
    DrawRectangleRec((Rectangle){board.x - 8, board.y - 8, board.width + 16, board.height + 16}, (Color){70, 52, 40, 255});
    DrawRectangleRec(board, (Color){22, 40, 34, 255});
    Color chalk = Fade((Color){220, 228, 220, 255}, 0.55f);
    Txt("PROJETO A.R.1.3.L. v0.1", board.x + 16, board.y + 10, 16, chalk);
    Txt("SE dado pessoal ENTÃO pedir consentimento", board.x + 16, board.y + 38, 13, chalk);
    Txt("IA confere, humano decide.", board.x + 16, board.y + 64, 14, Fade((Color){255, 220, 140, 255}, 0.55f));
    /* relogio de parede */
    Vector2 ck = {1080, 250};
    DrawCircleV(ck, 36, (Color){60, 54, 56, 255});
    DrawCircleV(ck, 31, (Color){200, 192, 170, 255});
    float hm = gt * 0.05f, mm = gt * 0.6f;
    DrawLineEx(ck, (Vector2){ck.x + sinf(hm) * 16, ck.y - cosf(hm) * 16}, 3, (Color){30, 26, 30, 255});
    DrawLineEx(ck, (Vector2){ck.x + sinf(mm) * 25, ck.y - cosf(mm) * 25}, 2, (Color){30, 26, 30, 255});
    DrawArenaScreen();
    /* fileira de armarios de fita, com o console central (mainframe) */
    for (int i = 0; i < 13; i++)
    {
        float x = 30 + i * 96;
        if (x > 520 && x < 740) continue;
        DrawTapeCabinet(x, 300, 80, 256, i + 3, 0.35f);
    }
    Rectangle mf = {530, 330, 220, 226};
    DrawRectangleRec(mf, (Color){40, 37, 42, 255});
    DrawRectangle((int)mf.x, (int)mf.y, (int)mf.width, 5, (Color){90, 84, 84, 255});
    for (int j = 0; j < 6; j++)
        for (int l = 0; l < 12; l++)
        {
            unsigned hsh = Hash((unsigned)(j * 97 + l * 13 + (int)(gt * (3 + (l % 4)))));
            Color lc = (hsh % 4 == 0) ? C_RED : ((hsh % 3 == 0) ? C_GREEN : C_AMBER);
            DrawCircle((int)(mf.x + 22 + l * 16), (int)(mf.y + 24 + j * 14), 3, Fade(lc, hsh % 3 ? 0.45f : 0.08f));
        }
    DrawRectangleGradientV(0, 330, SCREEN_WIDTH, 226, Fade((Color){120, 90, 60, 255}, 0.0f), Fade((Color){120, 90, 60, 255}, 0.16f));
}

static void DrawLabFront(void)
{
    for (int s = 0; s < 2; s++)
    {
        float x = s == 0 ? -40 : SCREEN_WIDTH - 120;
        DrawTapeCabinet(x, 290, 160, 350, 21 + s, 0.85f);
    }
    /* mesas com terminais na frente da plateia */
    for (int s = 0; s < 2; s++)
    {
        float x = s == 0 ? 210 : SCREEN_WIDTH - 210;
        DrawRectangle((int)x - 70, (int)AR_CROWD_RAIL_Y - 14, 140, 14, (Color){60, 44, 36, 255});
        DrawCrt(x, AR_CROWD_RAIL_Y - 32, 0.8f, 40 + s);
    }
}

/* Fundo + plateia + laterais + piso (sem as linhas). Chamado antes dos lutadores. */
static void DrawStage(void)
{
    ArenaReact();
    if (arArena == AR_LAB1990)
    {
        DrawLabBack();
        DrawCrowd();
        DrawFloorBase();
        DrawLabFront();
        return;
    }
    DrawArenaBack();
    DrawCrowd();
    DrawFloorBase();
    DrawFrontRacks();
}

/* Reflexo e linhas do piso; chamado logo depois de DrawStage. */
static void DrawStageFloor(void)
{
    DrawFloorReflections();
    DrawFloorLines();
}

/* ------------------------------------------------------------------ HUD da luta */
static void PG(float x0, float x1, float y, float h, float sl, bool mirror, Color col)
{
    if (!mirror) Quad((Vector2){x0 + sl, y}, (Vector2){x1 + sl, y}, (Vector2){x1, y + h}, (Vector2){x0, y + h}, col);
    else Quad((Vector2){SCREEN_WIDTH - (x0 + sl), y}, (Vector2){SCREEN_WIDTH - (x1 + sl), y}, (Vector2){SCREEN_WIDTH - x1, y + h}, (Vector2){SCREEN_WIDTH - x0, y + h}, col);
}
static void Bar(float x, float y, float w, float h, float fill, float trail, Color col, bool mirror)
{
    float sl = 16;
    PG(x - 4, x + w + 4, y - 3, h + 6, sl, mirror, (Color){4, 6, 14, 255});
    PG(x, x + w, y, h, sl, mirror, (Color){24, 28, 48, 255});
    PG(x + w * (1 - trail), x + w, y, h, sl, mirror, (Color){255, 240, 220, 200});
    PG(x + w * (1 - fill), x + w, y, h, sl, mirror, col);
    PG(x + w * (1 - fill), x + w, y + 2, h * 0.35f, sl * 0.4f, mirror, Fade(WHITE, 0.28f));
    for (int i = 1; i < 10; i++)
    {
        float xx = x + w * i / 10.0f;
        float X = mirror ? SCREEN_WIDTH - xx : xx;
        float off = mirror ? -sl : sl;
        DrawLineEx((Vector2){X + off, y}, (Vector2){X, y + h}, 2, (Color){4, 6, 14, 160});
    }
}

/* texto alinhado ao lado do jogador: P1 a partir de x, P2 terminando em SCREEN_WIDTH - x */
static void TxtSide(const char *s, float x, float y, int px, Color c, bool right)
{
    if (right) TxtR(s, SCREEN_WIDTH - x, y, px, c);
    else Txt(s, x, y, px, c);
}

/* contador de golpes ao lado da barra de quem ataca (nao sobre os corpos) */
static void DrawComboTag(Fighter *a, bool right)
{
    if (a->combo < 2 || a->comboT <= 0) return;
    float k = Clamp(a->comboT / 0.9f, 0.0f, 1.0f);
    float pop = 1 + 0.25f * Clamp((a->comboT - 0.75f) / 0.15f, 0.0f, 1.0f); /* cresce no golpe novo */
    char n[8], b[16];
    snprintf(n, sizeof n, "%d", a->combo);
    snprintf(b, sizeof b, "ACERTOS");
    int big = (int)(40 * pop);
    float y = 104;
    float nw = TxtW(n, big), bw = TxtW(b, 20);
    float x = right ? SCREEN_WIDTH - 72 - nw - 10 - bw : 72;
    Color c = Fade(ColorLerp(CHCOL[a->ch], WHITE, 0.2f), fminf(1, k * 2));
    Color o = Fade(C_INK, fminf(1, k * 2));
    TxtO(n, x + nw / 2, y - (big - 40) / 2.0f, big, c, o);
    TxtO(b, x + nw + 10 + bw / 2, y + 14, 20, c, o);
}

/* selo pulsante de especial pronto, em cima da barra de carga */
static void DrawSpecialReady(Fighter *a, bool right)
{
    const char *s = "ESPECIAL PRONTO";
    int px = 20;
    float tw = TxtW(s, px), w = tw + 44, h = 32;
    float x = right ? SCREEN_WIDTH - 70 - w : 70, y = SCREEN_HEIGHT - 102;
    float pulse = 0.5f + 0.5f * sinf(gt * 7);
    Color c = CHCOL[a->ch];
    Rectangle r = {x, y, w, h};
    DrawRectangleRounded(r, 0.4f, 6, Fade(C_INK, 0.85f));
    DrawRectangleRounded(r, 0.4f, 6, Fade(c, 0.12f + 0.18f * pulse));
    DrawRectangleRoundedLinesEx(r, 0.4f, 6, 2, Fade(ColorLerp(c, WHITE, 0.3f * pulse), 0.95f));
    /* losango indicador */
    Vector2 d = {x + 18, y + h / 2};
    float q = 6 + 2 * pulse;
    Quad((Vector2){d.x, d.y - q}, (Vector2){d.x + q, d.y}, (Vector2){d.x, d.y + q}, (Vector2){d.x - q, d.y}, ColorLerp(c, WHITE, 0.4f));
    Txt(s, x + 32, y + (h - px) / 2 - 1, px, ColorLerp(c, WHITE, 0.35f + 0.3f * pulse));
}

static void DrawHUD(void)
{
    /* placa escura atras do placar (so atras do HUD, nao e filtro de tela) */
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 120, Fade(C_INK, 0.75f), Fade(C_INK, 0.0f));
    DrawRectangleGradientV(0, SCREEN_HEIGHT - 100, SCREEN_WIDTH, 100, Fade(C_INK, 0.0f), Fade(C_INK, 0.7f));
    for (int i = 0; i < 2; i++)
    {
        Fighter *a = &fz[i];
        bool m = i == 1;
        float fill = a->hp / IAF_HP_MAX, trail = a->hpShown / IAF_HP_MAX;
        Color col = fill < 0.25f ? ColorLerp(C_RED, WHITE, 0.15f * (0.5f + 0.5f * sinf(gt * 10))) : ColorLerp(CHCOL[a->ch], WHITE, 0.15f);
        Bar(70, 26, 470, 28, fill, trail, col, m);
        /* nome grande com chip de cor e quem controla */
        float nx = m ? SCREEN_WIDTH - 70 : 70;
        Rectangle chip = {m ? nx - 20 : nx, 66, 20, 20};
        DrawRectangleRec(chip, CHCOL[a->ch]);
        DrawRectangleLinesEx(chip, 2, C_INK);
        const char *nm = NAME[a->ch];
        float nw = TxtW(nm, 30);
        TxtSide(nm, 70 + 30, 60, 30, C_WHITE, m);
        const char *tag = cpu[i] ? "CPU" : "JOGADOR";
        TxtSide(tag, 70 + 30 + nw + 12, 70, 16, C_DIM, m);
        /* rounds vencidos: dois chips grandes perto do cronometro */
        for (int r = 0; r < 2; r++)
        {
            float cx = m ? SCREEN_WIDTH / 2.0f + 78 + r * 40 : SCREEN_WIDTH / 2.0f - 78 - r * 40;
            Rectangle rc = {cx - 15, 64, 30, 20};
            bool won = wins[i] > r;
            if (won) DrawRectangleRec((Rectangle){rc.x - 3, rc.y - 3, rc.width + 6, rc.height + 6}, Fade(CHCOL[a->ch], 0.25f + 0.15f * sinf(gt * 4)));
            DrawRectangleRec(rc, won ? CHCOL[a->ch] : (Color){30, 34, 56, 255});
            DrawRectangleLinesEx(rc, 2, C_INK);
            for (int p = 0; p < 4; p++)
            {
                DrawRectangle((int)rc.x + 3 + p * 7, (int)rc.y - 5, 3, 5, C_DIM);
                DrawRectangle((int)rc.x + 3 + p * 7, (int)(rc.y + rc.height), 3, 5, C_DIM);
            }
            if (won) DrawRectangle((int)rc.x + 8, (int)rc.y + 6, 14, 8, Fade(WHITE, 0.55f));
        }
        DrawComboTag(a, m);
        /* carga do especial */
        float mx = m ? SCREEN_WIDTH - 70 - 300 : 70, my = SCREEN_HEIGHT - 40;
        TxtSide("CARGA", 70, my - 22, 16, C_DIM, m);
        for (int sgm = 0; sgm < 2; sgm++)
        {
            float seg = Clamp((a->meter - sgm * 50) / 50, 0.0f, 1.0f);
            int si = m ? 1 - sgm : sgm;
            Rectangle r = {mx + si * 152, my, 148, 16};
            DrawRectangleRec(r, (Color){20, 24, 44, 255});
            Rectangle f = r;
            f.width *= seg;
            if (m) f.x = r.x + r.width - f.width;
            DrawRectangleRec(f, seg >= 1 ? ColorLerp(CHCOL[a->ch], WHITE, 0.3f + 0.3f * sinf(gt * 8)) : Fade(CHCOL[a->ch], 0.7f));
            DrawRectangleLinesEx(r, 2, C_INK);
        }
        if (a->meter >= IAF_SPECIAL_COST) DrawSpecialReady(a, m);
    }
    /* cronometro em hexagono */
    Vector2 c = (Vector2){SCREEN_WIDTH / 2.0f, 52};
    DrawPoly(c, 6, 46, 0, (Color){4, 6, 14, 255});
    DrawPoly(c, 6, 42, 0, (Color){18, 22, 42, 255});
    DrawPolyLinesEx(c, 6, 42, 0, 2, Fade(C_CYAN, 0.7f));
    char b[8];
    snprintf(b, sizeof b, "%02d", (int)ceilf(roundTimer));
    TxtC(b, c.x, c.y - 19, 38, roundTimer < 10 ? C_RED : C_WHITE);
}

#endif /* IAFIGHT_ARENA_H */
