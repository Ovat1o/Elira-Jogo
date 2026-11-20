/* =============================================================================
 * iafight.c - MODO IA FIGHT: luta 1x1 em visao lateral ("rinha de inteligencias").
 * Lutadores: ELIRA (sprites, atlas assets/sprites/iafight/elira_luta.png) e as IAs
 * CLAUDE, CODEX e ANTIGRAVITY (desenhadas com primitivas da raylib, sem logos).
 * Arcade: o jogador escolhe 1 dos 4 e enfrenta os outros 3 (a Elira e a chefe final
 * quando o jogador escolhe uma IA). Melhor de 3 rounds, CPU para qualquer lutador.
 * O estado do modo e privado; o main.c so roteia SCREEN_IAFIGHT (ver iafight.h).
 * ============================================================================= */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iafight.h"

/* ------------------------------------------------------------------ utilitarios locais */
static float gt = 0; /* relogio das animacoes do modo */
static float EaseOut(float t) { t = Clamp(t, 0.0f, 1.0f); return 1 - (1 - t) * (1 - t); }
static float Rand01(void) { return GetRandomValue(0, 10000) / 10000.0f; }
/* triangulo em qualquer ordem de vertices (a raylib so desenha no sentido anti-horario) */
static void Tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    float cr = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cr < 0) DrawTriangle(a, b, c, col);
    else DrawTriangle(a, c, b, col);
}
static void Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) { Tri(a, b, c, col); Tri(a, c, d, col); }

/* paleta do modo */
#define C_BG ((Color){8, 10, 22, 255})
#define C_CYAN ((Color){96, 214, 240, 255})
#define C_AMBER ((Color){240, 194, 96, 255})
#define C_RED ((Color){230, 80, 84, 255})
#define C_WHITE ((Color){236, 240, 246, 255})
#define C_DIM ((Color){130, 146, 172, 255})
#define C_INK ((Color){10, 10, 18, 255})
#define C_GREEN ((Color){70, 255, 150, 255})
#define C_VIOLET ((Color){128, 96, 255, 255})
#define C_TEAL ((Color){70, 226, 210, 255})

/* ------------------------------------------------------------------ texto (fonte do jogo, tamanho em pixels reais) */
static void Txt(const char *s, float x, float y, int px, Color c) { DrawTextEx(GetGameFont((float)px), s, (Vector2){roundf(x), roundf(y)}, (float)px, 1, c); }
static float TxtW(const char *s, int px) { return MeasureTextEx(GetGameFont((float)px), s, (float)px, 1).x; }
static void TxtC(const char *s, float cx, float y, int px, Color c) { Txt(s, cx - TxtW(s, px) / 2, y, px, c); }
static void TxtR(const char *s, float rx, float y, int px, Color c) { Txt(s, rx - TxtW(s, px), y, px, c); }
/* texto com contorno escuro (titulos sobre a arena) */
static void TxtO(const char *s, float cx, float y, int px, Color c, Color o)
{
    float w = TxtW(s, px), x = cx - w / 2;
    int k = px >= 40 ? 4 : 2;
    for (int dx = -k; dx <= k; dx += k)
        for (int dy = -k; dy <= k; dy += k)
            if (dx || dy) Txt(s, x + dx, y + dy, px, o);
    Txt(s, x, y, px, c);
}

/* ------------------------------------------------------------------ Elira: atlas de luta
   Gerado por tools/elira_build.c a partir das fichas novas (tabela fixa de recortes, escala pela
   guarda oficial). Para trocar quadros: editar a tabela do construtor e gerar de novo. */
#include "iafight_elira.h"
static Texture2D texElira;
static int EAFrame(int anim, int i)
{
    int n = EA_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return EA_FIRST[anim] + i;
}
/* desenha o quadro f com os pes em (x, yFeet); espelha quando face < 0 */
static void DrawEF(int f, float x, float yFeet, int face, float sc, Color tint, float flash)
{
    const EFrame *e = &EF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float w = e->w * sc, h = e->h * sc;
    float left = face > 0 ? x - e->ax * sc : x - (e->w - e->ax) * sc;
    if (face < 0) src.width = -src.width;
    Rectangle dst = {roundf(left), roundf(yFeet - e->ay * sc), w, h};
    DrawTexturePro(texElira, src, dst, (Vector2){0, 0}, 0, tint);
    if (flash > 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(texElira, src, dst, (Vector2){0, 0}, 0, Fade(WHITE, flash));
        EndBlendMode();
    }
}
/* efeitos (projetil/impacto): ancorados no centro */
static void DrawEFC(int f, Vector2 c, int face, float sc, Color tint)
{
    const EFrame *e = &EF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float ax = face > 0 ? e->ax : e->w - e->ax;
    if (face < 0) src.width = -src.width;
    DrawTexturePro(texElira, src, (Rectangle){c.x - ax * sc, c.y - e->ay * sc, e->w * sc, e->h * sc}, (Vector2){0, 0}, 0, tint);
}

/* ------------------------------------------------------------------ personagens */
typedef enum { CH_ELIRA, CH_CLAUDE, CH_CODEX, CH_ANTI, CH_N } CharId;
static const char *NAME[CH_N] = {"ELIRA", "CLAUDE", "CODEX", "ANTIGRAVITY"};
static const char *STYLE[CH_N] = {"Viajante do tempo - tiro de plasma e mobilidade",
                                  "Calmo e analítico - defesa que devolve o golpe",
                                  "Agressivo - pressão curta, golpes de terminal",
                                  "Aéreo - flutua e dobra a gravidade"};
static const char *SPNAME[CH_N] = {"RAJADA TEMPORAL", "PRISMA REFLETOR", "DEPLOY FORÇADO", "POÇO GRAVITACIONAL"};
static const char *SPDESC[CH_N] = {"Carrega o canhão e dispara um núcleo de plasma que atravessa e acerta 3 vezes.",
                                   "Escudo de raciocínio: reflete projéteis e responde golpes com um feixe de luz.",
                                   "Avança compilando: prende o oponente numa sequência de 4 golpes e um final.",
                                   "Abre um poço que puxa e ergue o oponente, depois o derruba no chão."};
static const char *WINQ[CH_N] = {"\"A história não se reescreve sozinha. Eu reescrevo.\"",
                                 "\"Considerei todas as hipóteses. Esta era a melhor.\"",
                                 "\"Build concluída. Zero erros, zero avisos.\"",
                                 "\"Aqui em cima, quem decide a queda sou eu.\""};
static const Color CHCOL[CH_N] = {{96, 214, 240, 255}, {222, 116, 76, 255}, {70, 255, 150, 255}, {150, 120, 255, 255}};
/* atributos (0..5) para a ficha da selecao: forca, velocidade, defesa, alcance */
static const int STAT[CH_N][4] = {{3, 4, 2, 5}, {3, 3, 5, 3}, {5, 4, 2, 2}, {2, 5, 3, 4}};

typedef enum { ST_IDLE, ST_WALK, ST_JUMP, ST_CROUCH, ST_BLOCK, ST_ATK, ST_HIT, ST_KO, ST_WIN, ST_LOSE } FState;
typedef enum { AT_NONE, AT_LIGHT, AT_HEAVY, AT_LOW, AT_AIR, AT_SPECIAL, AT_SHOT } AtkKind;
typedef struct { float st, ac, rc, dmg, rx, top, h, stun, push; int type; } Atk; /* type: 0 meio, 1 baixo, 2 alto */
static const Atk ATK[CH_N][5] = {
    [CH_ELIRA] = {{0}, {0.06f, 0.08f, 0.15f, 45, 112, 168, 46, 0.30f, 170, 0},
                  {0.14f, 0.12f, 0.26f, 95, 138, 200, 95, 0.45f, 300, 0}, /* chute alto (forte) */
                  {0.08f, 0.09f, 0.20f, 42, 128, 46, 44, 0.30f, 150, 1},
                  {0.05f, 0.16f, 0.10f, 55, 96, 150, 90, 0.32f, 160, 2}},
    [CH_CLAUDE] = {{0}, {0.07f, 0.08f, 0.15f, 48, 108, 172, 48, 0.30f, 170, 0},
                   {0.17f, 0.10f, 0.30f, 105, 128, 180, 90, 0.45f, 380, 0},
                   {0.10f, 0.08f, 0.22f, 50, 128, 44, 44, 0.32f, 160, 1},
                   {0.05f, 0.15f, 0.10f, 55, 98, 150, 90, 0.32f, 160, 2}},
    [CH_CODEX] = {{0}, {0.06f, 0.07f, 0.14f, 52, 100, 168, 50, 0.30f, 180, 0},
                  {0.20f, 0.10f, 0.32f, 120, 118, 200, 120, 0.48f, 330, 0},
                  {0.09f, 0.08f, 0.22f, 55, 116, 46, 46, 0.32f, 170, 1},
                  {0.06f, 0.15f, 0.10f, 60, 96, 150, 90, 0.34f, 170, 2}},
    [CH_ANTI] = {{0}, {0.07f, 0.08f, 0.14f, 44, 116, 176, 46, 0.28f, 160, 0},
                 {0.16f, 0.10f, 0.28f, 100, 140, 210, 80, 0.44f, 300, 0},
                 {0.09f, 0.09f, 0.20f, 45, 134, 46, 46, 0.30f, 150, 1},
                 {0.05f, 0.18f, 0.10f, 55, 104, 150, 90, 0.32f, 160, 2}},
};
/* especiais: preparo / janela ativa / recuperacao */
static const float SP_ST[CH_N] = {0.42f, 0.08f, 0.14f, 0.22f};
static const float SP_AC[CH_N] = {0.05f, 0.85f, 0.40f, 0.05f};
static const float SP_RC[CH_N] = {0.40f, 0.25f, 0.34f, 0.45f};

typedef struct
{
    CharId ch;
    float x, y, vx, vy;
    int face;
    float hp, hpShown, meter;
    FState st;
    float stT, anim;
    AtkKind atk;
    float atkT;
    bool hitDone, spawned, airAtkUsed;
    bool crouch, guard, down;
    float stun, downT, flash, inv, hoverT;
    int combo;
    float comboT;
    /* especiais */
    bool caught;        /* codex: prendeu o oponente */
    int spHits;         /* codex: golpes da sequencia */
    float spT;          /* tempo interno do especial */
    bool countered;     /* claude: o prisma ja respondeu */
    float beamT;        /* claude: feixe de resposta visivel */
    Vector2 beamTo;
    float perfect;
    bool hitCrouch;   /* levou o golpe agachada (quadro de dano agachada) */
    bool shieldOn;    /* em defesa (segurando para tras com ameaca perto) */
    float shieldT;    /* tempo desde que abriu a defesa */
    float shieldHit;  /* > 0: acabou de bloquear (quadro 3 + tremor) */
    float shotCd;     /* recarga do tiro de plasma */
    float hitShake;   /* > 0: tremendo por ter levado/defendido um golpe */
    int juggle;       /* golpes levados no ar nesta queda (limite evita malabarismo infinito) */
} Fighter;

typedef struct { bool left, right, up, down, upP, lightP, heavyP, specialP, shotP; } IafInput;

/* projeteis */
typedef struct { bool on; float x, y, vx; int owner; bool big, reflected; float dmg, t, hitCd; int hits; } Proj;
static Proj proj[IAF_MAX_PROJ];
/* poco gravitacional */
typedef struct { bool on; float x, y, t, life; int owner; bool caught, resisted; } Well;
static Well well;

/* particulas e textos */
typedef struct { Vector2 p, v; float life, max, size; Color c; int kind; char txt[32]; } Part;
static Part parts[IAF_MAX_PARTS];
typedef struct { bool on; Vector2 p; float t; float sc; int face; } Impact;
static Impact impacts[16];

static Fighter fz[2];
static float hitstop = 0, shake = 0;

static void AddPart(Vector2 p, Vector2 v, float life, float size, Color c, int kind)
{
    for (int i = 0; i < IAF_MAX_PARTS; i++)
        if (parts[i].life <= 0)
        {
            parts[i] = (Part){p, v, life, life, size, c, kind, ""};
            return;
        }
}
static void AddText(Vector2 p, const char *s, Color c, float size)
{
    for (int i = 0; i < IAF_MAX_PARTS; i++)
        if (parts[i].life <= 0)
        {
            parts[i] = (Part){p, (Vector2){0, -60}, 0.8f, 0.8f, size, c, 2, ""};
            strncpy(parts[i].txt, s, 31);
            return;
        }
}
static void AddImpact(Vector2 p, float sc, int face)
{
    for (int i = 0; i < 16; i++)
        if (!impacts[i].on) { impacts[i] = (Impact){true, p, 0, sc, face}; return; }
}
static void HitSparks(Vector2 p, Color c, int n, float spd)
{
    for (int i = 0; i < n; i++)
    {
        float a = Rand01() * 6.283f, s = spd * (0.4f + Rand01());
        AddPart(p, (Vector2){cosf(a) * s, sinf(a) * s}, 0.22f + Rand01() * 0.2f, 2 + Rand01() * 3, c, 0);
    }
    AddPart(p, (Vector2){0, 0}, 0.25f, 40, c, 3);
}
/* clarao na boca do canhao (kind 7): nucleo branco, raios curtos e anel */
static void MuzzleFlash(Vector2 p, float size)
{
    AddPart(p, (Vector2){0, 0}, 0.10f, size, C_CYAN, 7);
}
/* acerto: estrela de pontas no ponto de contato (kind 4; v.x guarda o giro) */
static void ImpactSpark(Vector2 p, Color c, float size)
{
    AddPart(p, (Vector2){Rand01() * 0.8f, 0}, 0.16f, size, c, 4);
}
/* defesa: clarao vertical na frente do defensor (kind 6) e lascas hexagonais que caem (kind 5) */
static void BlockSpark(Vector2 p, Color c, int dir)
{
    AddPart(p, (Vector2){0, 0}, 0.14f, 34, c, 6);
    for (int i = 0; i < 7; i++)
    {
        float a = (-70 + Rand01() * 110) * DEG2RAD, s = 180 + Rand01() * 220;
        AddPart(p, (Vector2){-dir * cosf(a) * s, sinf(a) * s}, 0.30f + Rand01() * 0.15f, 3 + Rand01() * 3, c, 5);
    }
}

/* ------------------------------------------------------------------ esqueleto das IAs (primitivas) */
typedef struct { float rootY, lean, head, aF, eF, aB, eB, lF, kF, lB, kB; } Pose;
typedef struct { float legU, legL, torso, armU, armL, shW, hipW, headR, limbW; } Dims;
static const Dims DIMS[CH_N] = {
    {50, 50, 70, 40, 40, 24, 18, 21, 15}, /* elira (nao usado) */
    {52, 52, 72, 40, 40, 25, 19, 26, 15},
    {50, 50, 74, 40, 42, 34, 23, 25, 20},
    {55, 54, 70, 40, 38, 22, 16, 23, 13},
};
typedef struct
{
    Vector2 hip, neck, head, shF, shB, elF, elB, haF, haB, hpF, hpB, knF, knB, ftF, ftB;
    Vector2 up, side; /* direcao do tronco e perpendicular "para frente" */
    float s;
    int face;
} Skel;

static Vector2 Seg(Vector2 j, float angDeg, float len, int face)
{
    float a = angDeg * DEG2RAD;
    return (Vector2){j.x + sinf(a) * len * face, j.y + cosf(a) * len};
}
static Skel BuildSkel(CharId ch, float x, float y, int face, float s, Pose p)
{
    const Dims *d = &DIMS[ch];
    Skel k;
    k.s = s; k.face = face;
    k.hip = (Vector2){x, y - (d->legU + d->legL) * s * 0.96f + p.rootY * s};
    float la = p.lean * DEG2RAD;
    k.up = (Vector2){sinf(la) * face, -cosf(la)};
    k.side = (Vector2){-k.up.y * face, k.up.x * face};
    if (k.side.x * face < 0) k.side = Vector2Scale(k.side, -1);
    k.neck = Vector2Add(k.hip, Vector2Scale(k.up, d->torso * s));
    float ha = (p.lean + p.head) * DEG2RAD;
    k.head = Vector2Add(k.neck, Vector2Scale((Vector2){sinf(ha) * face, -cosf(ha)}, (d->headR + 8) * s));
    k.shF = Vector2Add(k.neck, (Vector2){face * d->shW * 0.25f * s, 6 * s});
    k.shB = Vector2Add(k.neck, (Vector2){-face * d->shW * 0.25f * s, 4 * s});
    k.hpF = Vector2Add(k.hip, (Vector2){face * d->hipW * 0.25f * s, 0});
    k.hpB = Vector2Add(k.hip, (Vector2){-face * d->hipW * 0.25f * s, 0});
    k.elF = Seg(k.shF, p.aF, d->armU * s, face);
    k.haF = Seg(k.elF, p.aF + p.eF, d->armL * s, face);
    k.elB = Seg(k.shB, p.aB, d->armU * s, face);
    k.haB = Seg(k.elB, p.aB + p.eB, d->armL * s, face);
    k.knF = Seg(k.hpF, p.lF, d->legU * s, face);
    k.ftF = Seg(k.knF, p.lF - p.kF, d->legL * s, face);
    k.knB = Seg(k.hpB, p.lB, d->legU * s, face);
    k.ftB = Seg(k.knB, p.lB - p.kB, d->legL * s, face);
    return k;
}

/* membro com contorno, cor base e brilho do lado de cima (volume) */
static void LimbT(Vector2 a, Vector2 b, float wa, float wb, Color base, Color hi, Color dark)
{
    Vector2 d = Vector2Normalize(Vector2Subtract(b, a)), n = (Vector2){-d.y, d.x};
    float o = 2.5f;
    Quad(Vector2Add(a, Vector2Scale(n, wa / 2 + o)), Vector2Subtract(a, Vector2Scale(n, wa / 2 + o)), Vector2Subtract(b, Vector2Scale(n, wb / 2 + o)), Vector2Add(b, Vector2Scale(n, wb / 2 + o)), dark);
    DrawCircleV(a, wa / 2 + o, dark);
    DrawCircleV(b, wb / 2 + o, dark);
    Quad(Vector2Add(a, Vector2Scale(n, wa / 2)), Vector2Subtract(a, Vector2Scale(n, wa / 2)), Vector2Subtract(b, Vector2Scale(n, wb / 2)), Vector2Add(b, Vector2Scale(n, wb / 2)), base);
    DrawCircleV(a, wa / 2, base);
    DrawCircleV(b, wb / 2, base);
    Vector2 up = (n.y < 0 || (n.y == 0 && n.x < 0)) ? n : Vector2Scale(n, -1);
    Color sh = ColorLerp(base, dark, 0.4f);
    Quad(Vector2Subtract(a, Vector2Scale(up, wa * 0.5f)), Vector2Subtract(a, Vector2Scale(up, wa * 0.2f)), Vector2Subtract(b, Vector2Scale(up, wb * 0.2f)), Vector2Subtract(b, Vector2Scale(up, wb * 0.5f)), sh);
    Quad(Vector2Add(a, Vector2Scale(up, wa * 0.12f)), Vector2Add(a, Vector2Scale(up, wa * 0.34f)), Vector2Add(b, Vector2Scale(up, wb * 0.34f)), Vector2Add(b, Vector2Scale(up, wb * 0.12f)), Fade(hi, 0.85f));
}
static void Limb(Vector2 a, Vector2 b, float w, Color base, Color hi, Color dark)
{
    w *= 1.45f;
    LimbT(a, b, w * 1.12f, w * 0.82f, base, hi, dark);
}
static void Shoe(Vector2 ankle, int face, float s, float len, Color base, Color hi, Color dark)
{
    Vector2 c = Vector2Add(ankle, (Vector2){face * len * 0.35f * s, 4 * s});
    DrawEllipse((int)c.x, (int)c.y, (int)(len * s + 2.5f), (int)(9 * s + 2.5f), dark);
    DrawEllipse((int)c.x, (int)c.y, (int)(len * s), (int)(9 * s), base);
    DrawEllipse((int)(c.x + face * 3 * s), (int)(c.y - 4 * s), (int)(len * 0.6f * s), (int)(3 * s), Fade(hi, 0.8f));
    DrawRectangle((int)(c.x - len * s), (int)(c.y + 5 * s), (int)(len * 2 * s), (int)(3 * s), ColorLerp(dark, base, 0.3f));
}
static void Ball(Vector2 c, float r, Color base, Color hi, Color dark)
{
    DrawCircleV(c, r + 2.5f, dark);
    DrawCircleV(c, r, base);
    DrawCircleV((Vector2){c.x - r * 0.3f, c.y - r * 0.32f}, r * 0.45f, Fade(hi, 0.8f));
}
/* tronco como trapezio com sombra e brilho */
static void Torso(Skel *k, float shW, float hipW, Color base, Color hi, Color dark, Color shade)
{
    float s = k->s;
    Vector2 n = (Vector2){-k->up.y, k->up.x}; /* perpendicular ao tronco */
    Vector2 a = Vector2Add(k->neck, Vector2Scale(n, shW * s)), b = Vector2Subtract(k->neck, Vector2Scale(n, shW * s));
    Vector2 c = Vector2Subtract(k->hip, Vector2Scale(n, hipW * s)), d = Vector2Add(k->hip, Vector2Scale(n, hipW * s));
    Vector2 up4 = Vector2Scale(k->up, 4 * s);
    Quad(Vector2Add(Vector2Add(a, Vector2Scale(n, 3)), up4), Vector2Add(Vector2Subtract(b, Vector2Scale(n, 3)), up4), Vector2Subtract(Vector2Subtract(c, Vector2Scale(n, 3)), Vector2Scale(k->up, 3)), Vector2Subtract(Vector2Add(d, Vector2Scale(n, 3)), Vector2Scale(k->up, 3)), dark);
    Quad(a, b, c, d, base);
    /* lado de tras sombreado, lado da frente iluminado */
    Vector2 back = (n.x * k->face < 0) ? n : Vector2Scale(n, -1);
    Vector2 a2 = Vector2Add(k->neck, Vector2Scale(back, shW * s)), d2 = Vector2Add(k->hip, Vector2Scale(back, hipW * s));
    Vector2 a3 = Vector2Add(k->neck, Vector2Scale(back, shW * s * 0.45f)), d3 = Vector2Add(k->hip, Vector2Scale(back, hipW * s * 0.45f));
    Quad(a2, a3, d3, d2, shade);
    Vector2 fr = Vector2Scale(back, -1);
    Vector2 h1 = Vector2Add(k->neck, Vector2Scale(fr, shW * s * 0.55f)), h2 = Vector2Add(k->neck, Vector2Scale(fr, shW * s * 0.25f));
    Vector2 h3 = Vector2Add(k->hip, Vector2Scale(fr, hipW * s * 0.25f)), h4 = Vector2Add(k->hip, Vector2Scale(fr, hipW * s * 0.55f));
    Quad(h1, h2, h3, h4, Fade(hi, 0.55f));
}

/* --- CLAUDE: casaco terracota, mascara de porcelana, constelacao de raciocinio --- */
static void DrawClaude(Skel *k, Pose p, float t, float flash, float vx)
{
    float s = k->s;
    Color coat = {196, 98, 62, 255}, coatHi = {240, 150, 106, 255}, coatDk = {112, 48, 28, 255}, coatSh = {150, 70, 42, 255};
    Color pant = {84, 70, 82, 255}, pantHi = {140, 120, 134, 255}, ink = {26, 16, 16, 255};
    Color cream = {242, 230, 210, 255}, creamHi = {255, 250, 240, 255}, gold = {255, 196, 110, 255};
    if (flash > 0) { coat = ColorLerp(coat, WHITE, flash); cream = ColorLerp(cream, WHITE, flash); pant = ColorLerp(pant, WHITE, flash); }
    /* constelacao atras da cabeca */
    Vector2 hc = k->head;
    for (int i = 0; i < 6; i++)
    {
        float a = t * 0.9f + i * 1.047f, b = t * 0.9f + (i + 2) * 1.047f;
        Vector2 p1 = (Vector2){hc.x + cosf(a) * 40 * s, hc.y - 6 * s + sinf(a) * 14 * s};
        Vector2 p2 = (Vector2){hc.x + cosf(b) * 40 * s, hc.y - 6 * s + sinf(b) * 14 * s};
        DrawLineEx(p1, p2, 1.5f, Fade(gold, 0.35f));
    }
    for (int i = 0; i < 6; i++)
    {
        float a = t * 0.9f + i * 1.047f;
        Vector2 p1 = (Vector2){hc.x + cosf(a) * 40 * s, hc.y - 6 * s + sinf(a) * 14 * s};
        DrawCircleV(p1, 5 * s, Fade(gold, 0.25f));
        DrawCircleV(p1, 2.6f * s, gold);
    }
    /* perna e braco de tras */
    Limb(k->hpB, k->knB, 16 * s, ColorLerp(pant, ink, 0.3f), pantHi, ink);
    Limb(k->knB, k->ftB, 14 * s, ColorLerp(pant, ink, 0.3f), pantHi, ink);
    Shoe(k->ftB, k->face, s, 17, (Color){50, 36, 34, 255}, (Color){120, 96, 84, 255}, ink);
    Limb(k->shB, k->elB, 15 * s, coatSh, coat, ink);
    Limb(k->elB, k->haB, 13 * s, coatSh, coat, ink);
    Ball(k->haB, 8 * s, ColorLerp(cream, ink, 0.2f), creamHi, ink);
    /* abas do casaco (balancam com o movimento) */
    float sway = sinf(t * 3) * 4 - vx * 0.03f * k->face;
    Vector2 n = (Vector2){-k->up.y, k->up.x};
    Vector2 w1 = Vector2Add(k->hip, Vector2Scale(n, 20 * s)), w2 = Vector2Subtract(k->hip, Vector2Scale(n, 20 * s));
    Vector2 tailEnd = Vector2Add(k->hip, (Vector2){-k->face * (16 + sway) * s, 62 * s});
    Vector2 tailEnd2 = Vector2Add(k->hip, (Vector2){k->face * (8 - sway * 0.5f) * s, 56 * s});
    Tri(w1, w2, Vector2Add(tailEnd, (Vector2){0, 3}), coatDk);
    Tri(w1, w2, tailEnd, coatSh);
    Tri(w1, w2, tailEnd2, coat);
    DrawLineEx(k->hip, tailEnd2, 2, Fade(coatHi, 0.6f));
    Torso(k, 31, 24, coat, coatHi, ink, coatSh);
    /* camisa clara e gola alta */
    Vector2 c0 = Vector2Add(k->neck, Vector2Scale(k->up, -6 * s)), c1 = Vector2Add(k->hip, Vector2Scale(k->up, 30 * s));
    Vector2 fwd = (Vector2){k->face * 1.0f, 0};
    Quad(Vector2Add(c0, Vector2Scale(fwd, 4 * s)), Vector2Add(c0, Vector2Scale(fwd, 13 * s)), Vector2Add(c1, Vector2Scale(fwd, 10 * s)), Vector2Add(c1, Vector2Scale(fwd, 2 * s)), cream);
    DrawCircleV(Vector2Add(c1, Vector2Scale(fwd, 7 * s)), 3 * s, gold); /* botao-nucleo */
    Tri(Vector2Add(k->neck, (Vector2){-k->face * 14 * s, 2 * s}), Vector2Add(k->neck, (Vector2){k->face * 10 * s, 4 * s}), Vector2Add(k->neck, (Vector2){-k->face * 8 * s, -16 * s}), coatDk);
    Tri(Vector2Add(k->neck, (Vector2){-k->face * 12 * s, 0}), Vector2Add(k->neck, (Vector2){k->face * 6 * s, 2 * s}), Vector2Add(k->neck, (Vector2){-k->face * 6 * s, -13 * s}), coat);
    /* cinto */
    DrawLineEx(Vector2Add(k->hip, Vector2Scale(n, 20 * s)), Vector2Subtract(k->hip, Vector2Scale(n, 20 * s)), 6 * s, (Color){70, 40, 30, 255});
    DrawCircleV(Vector2Add(k->hip, (Vector2){k->face * 6 * s, 0}), 4 * s, gold);
    /* perna da frente */
    Limb(k->hpF, k->knF, 16 * s, pant, pantHi, ink);
    Limb(k->knF, k->ftF, 14 * s, pant, pantHi, ink);
    Shoe(k->ftF, k->face, s, 18, (Color){62, 44, 40, 255}, (Color){140, 110, 96, 255}, ink);
    /* cabeca: mascara de porcelana com faixa de visor */
    Ball(hc, 26 * s, cream, creamHi, ink);
    DrawCircleV(Vector2Add(hc, (Vector2){-k->face * 11 * s, -8 * s}), 17 * s, Fade(ColorLerp(cream, coat, 0.5f), 0.55f)); /* cabelo curto/capuz suave */
    Vector2 vc = Vector2Add(hc, (Vector2){k->face * 10 * s, -1 * s});
    DrawEllipse((int)vc.x, (int)vc.y, (int)(16 * s), (int)(7 * s), (Color){52, 34, 34, 255});
    float blink = fmodf(t, 4.2f) < 0.12f ? 0.2f : 1.0f;
    DrawRectangle((int)(vc.x - 2 * s + k->face * 2 * s), (int)(vc.y - 2 * s * blink), (int)(12 * s), (int)(4 * s * blink + 1), gold);
    DrawCircleV(Vector2Add(hc, (Vector2){-k->face * 4 * s, 8 * s}), 3 * s, Fade(coat, 0.5f));
    /* braco da frente */
    Limb(k->shF, k->elF, 15 * s, coat, coatHi, ink);
    Limb(k->elF, k->haF, 13 * s, coat, coatHi, ink);
    DrawCircleV(Vector2Lerp(k->elF, k->haF, 0.82f), 7 * s, cream); /* punho */
    Ball(k->haF, 8.5f * s, cream, creamHi, ink);
    (void)p;
}

/* --- CODEX: moletom de capuz, rosto-terminal, luvas de teclado --- */
static void Gauntlet(Vector2 c, float s, Color body, Color neon, Color ink, float glow)
{
    Rectangle r = {c.x - 13 * s, c.y - 12 * s, 26 * s, 24 * s};
    DrawRectangleRounded((Rectangle){r.x - 2.5f, r.y - 2.5f, r.width + 5, r.height + 5}, 0.3f, 4, ink);
    DrawRectangleRounded(r, 0.3f, 4, body);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 2; j++)
        {
            Rectangle kk = {r.x + 3 * s + i * 7.4f * s, r.y + 4 * s + j * 9 * s, 6 * s, 7 * s};
            DrawRectangleRounded(kk, 0.3f, 2, (Color){58, 64, 74, 255});
            DrawRectangle((int)kk.x + 1, (int)kk.y + 1, (int)kk.width - 2, (int)(2 * s), (Color){96, 104, 118, 255});
        }
    DrawRectangle((int)r.x, (int)(r.y + r.height - 3 * s), (int)r.width, (int)(2 * s), Fade(neon, 0.6f + 0.4f * glow));
}
static void DrawCodex(Skel *k, Pose p, float t, float flash, float vx)
{
    float s = k->s;
    Color hood = {46, 50, 60, 255}, hoodHi = {84, 92, 108, 255}, hoodSh = {32, 34, 42, 255}, ink = {10, 12, 14, 255};
    Color pant = {30, 32, 40, 255}, pantHi = {64, 68, 80, 255}, neon = C_GREEN;
    Color boot = {24, 26, 30, 255}, bootHi = {80, 84, 96, 255};
    if (flash > 0) { hood = ColorLerp(hood, WHITE, flash); pant = ColorLerp(pant, WHITE, flash); }
    float glow = 0.5f + 0.5f * sinf(t * 6);
    /* cabo de dados nas costas */
    Vector2 cb0 = Vector2Add(k->neck, (Vector2){-k->face * 20 * s, 10 * s}), cb1 = Vector2Add(k->hip, (Vector2){-k->face * 26 * s, 8 * s});
    DrawLineBezier(cb0, cb1, 6 * s, ink);
    DrawLineBezier(cb0, cb1, 3 * s, Fade(neon, 0.5f));
    Limb(k->hpB, k->knB, 21 * s, ColorLerp(pant, ink, 0.35f), pantHi, ink);
    Limb(k->knB, k->ftB, 18 * s, ColorLerp(pant, ink, 0.35f), pantHi, ink);
    DrawRectangleRounded((Rectangle){k->ftB.x - 12 * s + k->face * 4 * s, k->ftB.y - 8 * s, 26 * s, 14 * s}, 0.4f, 4, boot);
    Limb(k->shB, k->elB, 20 * s, hoodSh, hood, ink);
    Limb(k->elB, k->haB, 18 * s, hoodSh, hood, ink);
    Gauntlet(k->haB, s * 0.95f, (Color){30, 32, 38, 255}, neon, ink, glow);
    Torso(k, 42, 28, hood, hoodHi, ink, hoodSh);
    /* faixa de circuito e bolso */
    Vector2 m0 = Vector2Add(k->neck, Vector2Scale(k->up, -14 * s)), m1 = Vector2Add(k->hip, Vector2Scale(k->up, 14 * s));
    DrawLineEx(Vector2Add(m0, (Vector2){k->face * 6 * s, 0}), Vector2Add(m1, (Vector2){k->face * 6 * s, 0}), 3 * s, Fade(neon, 0.75f));
    DrawCircleV(Vector2Add(m0, (Vector2){k->face * 6 * s, 0}), 3.5f * s, neon);
    Vector2 pk = Vector2Add(k->hip, Vector2Scale(k->up, 18 * s));
    DrawRectangleRounded((Rectangle){pk.x - 18 * s, pk.y - 6 * s, 36 * s, 14 * s}, 0.4f, 4, hoodSh);
    DrawLineEx((Vector2){pk.x - 18 * s, pk.y - 6 * s}, (Vector2){pk.x + 18 * s, pk.y - 6 * s}, 2, hoodHi);
    /* ombreiras */
    Ball(k->shB, 14 * s, hoodSh, hood, ink);
    Limb(k->hpF, k->knF, 21 * s, pant, pantHi, ink);
    Limb(k->knF, k->ftF, 18 * s, pant, pantHi, ink);
    DrawRectangleRounded((Rectangle){k->ftF.x - 12 * s + k->face * 5 * s, k->ftF.y - 8 * s, 28 * s, 15 * s}, 0.4f, 4, boot);
    DrawRectangle((int)(k->ftF.x - 10 * s + k->face * 5 * s), (int)(k->ftF.y - 7 * s), (int)(24 * s), (int)(3 * s), bootHi);
    DrawRectangle((int)(k->ftF.x - 12 * s + k->face * 5 * s), (int)(k->ftF.y + 4 * s), (int)(28 * s), (int)(2 * s), Fade(neon, 0.7f));
    /* capuz + tela */
    Vector2 hc = k->head;
    Ball(Vector2Add(hc, (Vector2){-k->face * 4 * s, -2 * s}), 31 * s, hood, hoodHi, ink);
    Tri(Vector2Add(hc, (Vector2){-k->face * 30 * s, 2 * s}), Vector2Add(hc, (Vector2){-k->face * 6 * s, 22 * s}), Vector2Add(hc, (Vector2){-k->face * 36 * s, 26 * s}), hoodSh);
    Rectangle scr = {hc.x - 17 * s + k->face * 8 * s, hc.y - 15 * s, 34 * s, 30 * s};
    DrawRectangleRounded((Rectangle){scr.x - 2, scr.y - 2, scr.width + 4, scr.height + 4}, 0.35f, 4, ink);
    DrawRectangleRounded(scr, 0.35f, 4, (Color){6, 16, 10, 255});
    const char *face = ">_";
    if (fmodf(t, 1.0f) > 0.5f) face = ">";
    if (flash > 0.2f) face = "x_x";
    Txt(face, scr.x + 5 * s, scr.y + 6 * s, (int)(17 * s), neon);
    DrawRectangle((int)scr.x, (int)(scr.y + scr.height * 0.5f + fmodf(t * 40, scr.height * 0.5f)), (int)scr.width, 1, Fade(neon, 0.18f));
    Limb(k->shF, k->elF, 20 * s, hood, hoodHi, ink);
    Limb(k->elF, k->haF, 18 * s, hood, hoodHi, ink);
    Ball(k->shF, 14 * s, hood, hoodHi, ink);
    Gauntlet(k->haF, s, (Color){36, 40, 46, 255}, neon, ink, glow);
    (void)p; (void)vx;
}

/* --- ANTIGRAVITY: traje branco, visor violeta, aneis orbitais, cachecol que sobe --- */
static void Ring(Vector2 c, float rx, float ry, float rot, Color col, float th, bool front)
{
    Vector2 prev = {0};
    for (int i = 0; i <= 40; i++)
    {
        float a = i / 40.0f * 6.283f;
        float px = cosf(a) * rx, py = sinf(a) * ry;
        Vector2 q = (Vector2){c.x + px * cosf(rot) - py * sinf(rot), c.y + px * sinf(rot) + py * cosf(rot)};
        bool isFront = sinf(a) > 0;
        if (i > 0 && isFront == front) DrawLineEx(prev, q, th, col);
        prev = q;
    }
}
static void DrawAnti(Skel *k, Pose p, float t, float flash, float vx)
{
    float s = k->s;
    Color suit = {226, 230, 242, 255}, suitHi = {255, 255, 255, 255}, suitSh = {160, 168, 200, 255}, ink = {24, 20, 48, 255};
    Color vio = {128, 96, 255, 255}, teal = C_TEAL;
    if (flash > 0) { suit = ColorLerp(suit, (Color){255, 200, 200, 255}, flash); }
    Vector2 core = Vector2Lerp(k->hip, k->neck, 0.55f);
    /* cachecol flutuando para cima */
    Vector2 prev = Vector2Add(k->neck, (Vector2){-k->face * 6 * s, 0});
    for (int i = 1; i <= 7; i++)
    {
        Vector2 q = Vector2Add(k->neck, (Vector2){-k->face * (i * 13 + vx * 0.02f * k->face * -i) * s, (-i * 11 + sinf(t * 4 + i * 0.8f) * 6) * s});
        DrawLineEx(prev, q, (14 - i * 1.3f) * s, ColorLerp(vio, teal, i / 7.0f));
        prev = q;
    }
    Ring(core, 46 * s, 13 * s, 0.25f * sinf(t * 0.8f), Fade(teal, 0.85f), 3 * s, false);
    Ring(core, 38 * s, 10 * s, -0.4f + 0.2f * cosf(t), Fade(vio, 0.85f), 2.5f * s, false);
    Limb(k->hpB, k->knB, 14 * s, suitSh, suit, ink);
    Limb(k->knB, k->ftB, 12 * s, suitSh, suit, ink);
    Tri(Vector2Add(k->ftB, (Vector2){-9 * s, -4 * s}), Vector2Add(k->ftB, (Vector2){9 * s, -4 * s}), Vector2Add(k->ftB, (Vector2){k->face * 8 * s, 18 * s}), vio);
    Limb(k->shB, k->elB, 13 * s, suitSh, suit, ink);
    Limb(k->elB, k->haB, 11 * s, suitSh, suit, ink);
    Ball(k->haB, 7 * s, ColorLerp(vio, ink, 0.3f), vio, ink);
    Torso(k, 27, 19, suit, suitHi, ink, suitSh);
    /* faixas do traje */
    Vector2 n = (Vector2){-k->up.y, k->up.x};
    DrawLineEx(Vector2Add(k->neck, Vector2Scale(n, 10 * s)), Vector2Add(k->hip, Vector2Scale(n, 6 * s)), 4 * s, vio);
    DrawLineEx(Vector2Subtract(k->neck, Vector2Scale(n, 10 * s)), Vector2Subtract(k->hip, Vector2Scale(n, 6 * s)), 4 * s, Fade(teal, 0.8f));
    DrawCircleV(core, 9 * s, ink);
    DrawCircleV(core, 7 * s, teal);
    DrawCircleV(core, 3.5f * s, WHITE);
    Limb(k->hpF, k->knF, 14 * s, suit, suitHi, ink);
    Limb(k->knF, k->ftF, 12 * s, suit, suitHi, ink);
    Tri(Vector2Add(k->ftF, (Vector2){-9 * s, -4 * s}), Vector2Add(k->ftF, (Vector2){9 * s, -4 * s}), Vector2Add(k->ftF, (Vector2){k->face * 9 * s, 19 * s}), vio);
    /* capacete */
    Vector2 hc = k->head;
    Tri(Vector2Add(hc, (Vector2){-k->face * 14 * s, -10 * s}), Vector2Add(hc, (Vector2){-k->face * 4 * s, -16 * s}), Vector2Add(hc, (Vector2){-k->face * 30 * s, -30 * s}), vio);
    Ball(hc, 23 * s, suit, suitHi, ink);
    Vector2 vc = Vector2Add(hc, (Vector2){k->face * 9 * s, 1 * s});
    DrawEllipse((int)vc.x, (int)vc.y, (int)(14 * s), (int)(11 * s), ink);
    DrawEllipse((int)vc.x, (int)vc.y, (int)(12 * s), (int)(9 * s), vio);
    DrawEllipse((int)(vc.x - 3 * s), (int)(vc.y - 3 * s), (int)(4 * s), (int)(2 * s), Fade(WHITE, 0.8f));
    DrawLineEx(Vector2Add(vc, (Vector2){-4 * s, 3 * s}), Vector2Add(vc, (Vector2){6 * s, 3 * s}), 1.5f, Fade(teal, 0.8f));
    Ring(core, 46 * s, 13 * s, 0.25f * sinf(t * 0.8f), teal, 3 * s, true);
    Ring(core, 38 * s, 10 * s, -0.4f + 0.2f * cosf(t), vio, 2.5f * s, true);
    Limb(k->shF, k->elF, 13 * s, suit, suitHi, ink);
    Limb(k->elF, k->haF, 11 * s, suit, suitHi, ink);
    Ball(k->haF, 7.5f * s, vio, (Color){190, 170, 255, 255}, ink);
    (void)p;
}

/* ------------------------------------------------------------------ poses */
static Pose PIdle(float t)
{
    return (Pose){6 + 2 * sinf(t * 3), 6, 0, 55, 75, 35, 85, 22, 18, -18, 12};
}
static Pose PCrouch(void) { return (Pose){52, 16, 4, 60, 80, 45, 90, 78, 122, 35, 118}; }
static Pose PJump(void) { return (Pose){-6, 4, 0, 100, 50, 60, 70, 70, 100, 30, 110}; }
static float AtkTotal(Fighter *a);
static float AtkExt(Fighter *a, float *windOut)
{
    float st, ac, rc;
    if (a->atk == AT_SPECIAL) { st = SP_ST[a->ch]; ac = SP_AC[a->ch]; rc = SP_RC[a->ch]; }
    else if (a->atk == AT_SHOT) { st = IAF_SHOT_STARTUP; ac = IAF_SHOT_ACTIVE; rc = IAF_SHOT_RECOVERY; }
    else { const Atk *d = &ATK[a->ch][a->atk]; st = d->st; ac = d->ac; rc = d->rc; }
    if (ac < IAF_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ACTIVE_VISUAL; } /* segura o impacto */
    float t = a->atkT;
    if (windOut) *windOut = t < st ? t / st : 0;
    if (t < st) return EaseOut(t / st) * 0.35f;
    if (t < st + ac) return 1;
    return 1 - Clamp((t - st - ac) / rc, 0.0f, 1.0f);
}
static Pose PoseFor(Fighter *a)
{
    float t = gt + a->x * 0.01f;
    Pose p = PIdle(t);
    if (a->st == ST_WALK)
    {
        float ph = a->anim * 9;
        p.lF = 18 + 24 * sinf(ph); p.kF = 18 + 22 * fmaxf(0, -sinf(ph));
        p.lB = -14 - 24 * sinf(ph); p.kB = 14 + 22 * fmaxf(0, sinf(ph));
        p.rootY = 6 + 3 * fabsf(cosf(ph));
    }
    if (a->crouch) p = PCrouch();
    if (a->st == ST_JUMP || (a->st == ST_ATK && a->atk == AT_AIR)) p = PJump();
    if (a->st == ST_BLOCK || (a->guard && a->st != ST_ATK && a->st != ST_HIT))
    {
        p.aF = 78; p.eF = 102; p.aB = 64; p.eB = 112; p.lean -= 8; p.head = 6;
    }
    if (a->st == ST_HIT || a->st == ST_LOSE)
    {
        p.lean = -22; p.head = -18; p.aF = 25; p.eF = 35; p.aB = -25; p.eB = 25; p.lF = 30; p.kF = 30; p.lB = -10; p.kB = 10;
    }
    if (a->st == ST_KO || a->down)
    {
        const Dims *d = &DIMS[a->ch];
        p = (Pose){(d->legU + d->legL) * 0.96f - 14, -86, -10, -70, 20, -110, 10, 86, 6, 80, 12};
        if (a->st != ST_KO && a->downT < 0.25f)
        { /* levantando */
            float r = 1 - a->downT / 0.25f;
            Pose c = PCrouch();
            p.lean = Lerp(-86, c.lean, r); p.rootY = Lerp(p.rootY, c.rootY, r);
        }
    }
    if (a->st == ST_WIN)
    {
        p.aF = 165; p.eF = 15; p.aB = 30; p.eB = 50; p.lean = 0; p.lF = 12; p.lB = -12;
        if (a->ch == CH_CLAUDE) { p.aF = 70; p.eF = 95; p.aB = 20; p.eB = 30; }
        if (a->ch == CH_ANTI) { p.aF = 140; p.eF = 10; p.aB = 140; p.eB = 10; p.rootY = -10; }
    }
    if (a->st == ST_ATK)
    {
        float wind;
        float e = AtkExt(a, &wind);
        switch (a->atk)
        {
        case AT_LIGHT:
            p.aF = Lerp(55, 92, e); p.eF = Lerp(75, 0, e); p.lean += 8 * e;
            break;
        case AT_HEAVY:
            if (a->ch == CH_CLAUDE)
            {
                p.aF = Lerp(55, 88, e); p.eF = Lerp(75, 4, e); p.aB = Lerp(35, 84, e); p.eB = Lerp(85, 8, e); p.lean += 14 * e; p.lF += 12 * e;
            }
            else if (a->ch == CH_CODEX)
            {
                if (wind > 0) { p.aF = Lerp(55, 172, wind); p.eF = Lerp(75, 25, wind); p.lean -= 10 * wind; p.aB = Lerp(35, 150, wind); }
                else { p.aF = Lerp(60, 82, e); p.eF = 4; p.aB = 76; p.eB = 10; p.lean = 6 + 24 * e; p.rootY += 10 * e; }
            }
            else
            {
                p.lF = Lerp(22, 106, e); p.kF = Lerp(18, 0, e); p.lean = Lerp(6, -20, e); p.aB = 110; p.eB = 30; p.aF = 40; p.eF = 40;
                p.lB = -8; p.kB = 6;
            }
            break;
        case AT_LOW:
            p = PCrouch();
            p.lF = Lerp(78, 88, e); p.kF = Lerp(122, 0, e); p.lean = 16 + 10 * e; p.aB = 120; p.eB = 30;
            break;
        case AT_AIR:
            p = PJump();
            p.lF = Lerp(70, 50, e); p.kF = Lerp(100, 0, e); p.aF = 120; p.lean = 10;
            break;
        case AT_SPECIAL:
            if (a->ch == CH_CLAUDE) { p.aF = 96; p.eF = 30; p.aB = 86; p.eB = 50; p.lean = -2; p.head = 4; }
            else if (a->ch == CH_CODEX)
            {
                if (wind > 0) { p = PCrouch(); p.lean = 30; p.aF = 20; p.eF = 90; }
                else { p.lean = 30; p.aF = 92; p.eF = 0; p.aB = 40; p.eB = 90; p.lF = 50; p.kF = 20; p.lB = -50; p.kB = 30; p.rootY = 14; }
                if (a->caught) { p.lean = 18; float q = sinf(gt * 40); p.aF = 80 + 15 * q; p.eF = 10; p.aB = 80 - 15 * q; p.eB = 10; }
            }
            else { p.aF = 160; p.eF = 15; p.aB = 145; p.eB = 25; p.lean = -6; p.rootY = -14; }
            break;
        default: break;
        }
    }
    return p;
}

static void DrawPuppet(CharId ch, float x, float y, int face, float s, Pose p, float flash, float vx)
{
    if (ch == CH_ANTI && p.lean > -80) y -= (16 + 5 * sinf(gt * 2.4f)) * s; /* flutua */
    Skel k = BuildSkel(ch, x, y, face, s, p);
    if (ch == CH_CLAUDE) DrawClaude(&k, p, gt, flash, vx);
    else if (ch == CH_CODEX) DrawCodex(&k, p, gt, flash, vx);
    else DrawAnti(&k, p, gt, flash, vx);
}

/* ------------------------------------------------------------------ Elira: escolha do quadro */
static bool OnGround(Fighter *a);
/* quadro de um golpe: preparo usa os quadros [0, a0), ativo usa [a0, a1], recuperacao o resto */
static int AtkFrame(Fighter *a, int anim, int a0, int a1, float st, float ac, float rc)
{
    int n = EA_COUNT[anim], i;
    float t = a->atkT;
    if (ac < IAF_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ACTIVE_VISUAL; } /* segura o impacto */
    if (t < st) i = a0 > 0 ? (int)(t / st * a0) : 0;
    else if (t < st + ac) i = a0 + (a1 > a0 ? (int)((t - st) / ac * (a1 - a0 + 1)) : 0);
    else
    {
        int nr = n - 1 - a1;
        i = nr > 0 ? a1 + 1 + (int)((t - st - ac) / rc * nr) : a1;
    }
    return EAFrame(anim, i);
}
static int EliraFrameIdx(Fighter *a, float *yoff)
{
    *yoff = 0;
    if (a->st == ST_KO) { float t = a->stT; return EAFrame(EA_KO, t < 0.12f ? 0 : (t < 0.4f ? 1 : 2)); }
    if (a->down)
    { /* caida: deitada e depois levantando (o ultimo quadro e a guarda) */
        if (a->downT > 0.36f) return EAFrame(EA_KO, 2);
        return EAFrame(EA_GETUP, (int)((1 - a->downT / 0.36f) * 3));
    }
    if (a->st == ST_LOSE) return EAFrame(EA_DEFEAT, a->stT < 0.5f ? 0 : 1);
    if (a->st == ST_WIN)
    {
        float t = a->stT;
        return EAFrame(EA_WIN, t < 0.5f ? (int)(t / 0.5f * 3) : 2 + ((int)((t - 0.5f) * 2.5f) % 2));
    }
    if (a->st == ST_HIT) return EAFrame(a->hitCrouch ? EA_HIT_CROUCH : EA_HIT, a->stT < 0.12f ? 0 : 1);
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_ELIRA][a->atk < AT_SPECIAL ? a->atk : 0];
        switch (a->atk)
        {
        case AT_LIGHT: return AtkFrame(a, EA_PUNCH, 1, 1, d->st, d->ac, d->rc);
        case AT_HEAVY: return AtkFrame(a, EA_KICK, 2, 3, d->st, d->ac, d->rc);
        case AT_LOW: return AtkFrame(a, EA_SWEEP, 2, 2, d->st, d->ac, d->rc);
        case AT_AIR: return AtkFrame(a, EA_AIRKICK, 1, 1, d->st, d->ac, d->rc);
        case AT_SHOT: return AtkFrame(a, EA_SHOT, 3, 3, IAF_SHOT_STARTUP, IAF_SHOT_ACTIVE, IAF_SHOT_RECOVERY);
        case AT_SPECIAL: return AtkFrame(a, EA_RAJADA, 4, 4, SP_ST[CH_ELIRA], SP_AC[CH_ELIRA], SP_RC[CH_ELIRA]);
        default: break;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    { /* defesa: 1o quadro ao entrar, 2o enquanto segura, 3o ao bloquear */
        int anim = a->crouch ? EA_DEF_CROUCH : EA_DEF;
        if (a->shieldHit > 0) return EAFrame(anim, 2);
        return EAFrame(anim, a->shieldT < 0.08f ? 0 : 1);
    }
    if (a->st == ST_JUMP) return EAFrame(EA_JUMP, a->vy < -250 ? 0 : (a->vy < 250 ? 1 : 2));
    if (a->crouch) return EAFrame(EA_CROUCH, a->stT < 0.07f ? 0 : 1);
    if (a->st == ST_WALK)
    {
        bool back = (a->vx * a->face) < 0;
        return EAFrame(back ? EA_WALK_B : EA_WALK_F, (int)(a->anim * 10) % 6);
    }
    *yoff = roundf((sinf(gt * 2.4f) - 1) * 1.0f); /* respiracao: 0 a 2 px */
    return EAFrame(EA_IDLE, 0);
}

/* ------------------------------------------------------------------ desenho de um lutador */
/* rastro do golpe: arco translucido na cor do lutador, varrendo o caminho do membro que golpeia
   (angulos em graus a partir da frente; positivo = para baixo) */
static void DrawAttackTrail(Fighter *a, float jx)
{
    if (a->st != ST_ATK || a->atk < AT_LIGHT || a->atk > AT_AIR) return;
    const Atk *d = &ATK[a->ch][a->atk];
    float act = fmaxf(d->ac, IAF_ACTIVE_VISUAL), t = a->atkT - d->st, lead = 0.04f, life = act + 0.10f;
    if (t < -lead || t > life) return;
    float k = t < 0 ? (t + lead) / lead : 1 - Clamp((t - act) / (life - act), 0.0f, 1.0f); /* entra rapido, some no fim */
    float prog = Clamp((t + lead) / (act + lead), 0.0f, 1.0f);
    float cy, r, a0, a1;
    switch (a->atk)
    {
    case AT_LIGHT: cy = a->y - d->top + d->h * 0.5f; r = d->rx * 0.62f; a0 = -16; a1 = 16; break;
    case AT_HEAVY:
        if (a->ch == CH_CODEX) { cy = a->y - 120; r = d->rx * 0.8f; a0 = -80; a1 = 35; }       /* martelada descendo */
        else if (a->ch == CH_CLAUDE) { cy = a->y - 130; r = d->rx * 0.62f; a0 = -14; a1 = 14; } /* empurrao reto */
        else { cy = a->y - 105; r = d->rx * 0.72f; a0 = 70; a1 = -55; }                          /* chute subindo */
        break;
    case AT_LOW: cy = a->y - 70; r = d->rx * 0.68f; a0 = 20; a1 = 62; break;
    default: cy = a->y - d->top + 20; r = d->rx * 0.7f; a0 = -25; a1 = 60; break; /* chute aereo, para baixo */
    }
    Color c = CHCOL[a->ch];
    Vector2 o = {a->x + jx + a->face * 8, cy}, prev = {0}, tip = {0};
    float cur = Lerp(a0, a1, prog);
    const int N = 16;
    for (int i = 0; i <= N; i++)
    {
        float u = i / (float)N, an = Lerp(a0, cur, u) * DEG2RAD;
        Vector2 p = {o.x + a->face * cosf(an) * r, o.y + sinf(an) * r};
        if (i > 0)
        {
            DrawLineEx(prev, p, 4 + 12 * u, Fade(c, k * 0.6f * (0.25f + 0.75f * u)));
            DrawLineEx(prev, p, 1.5f + 3 * u, Fade(WHITE, k * 0.45f * u));
        }
        prev = p;
        tip = p;
    }
    DrawCircleV(tip, 4 + 3 * k, Fade(WHITE, k * 0.5f));
}

static void DrawShadow(float x, float w)
{
    DrawEllipse((int)x, (int)IAF_GROUND_Y + 2, (int)w, 9, Fade(BLACK, 0.45f));
}
static void DrawFighter(Fighter *a)
{
    float flash = a->flash > 0 ? Clamp(a->flash / 0.12f, 0.0f, 1.0f) * 0.8f : 0;
    float alt = IAF_GROUND_Y - a->y;
    float jx = a->hitShake > 0 ? (Rand01() - 0.5f) * 2 * IAF_HIT_SHAKE_PX : 0; /* tremor no impacto */
    DrawShadow(a->x, 52 - fminf(alt, 200) * 0.12f);
    if (a->inv > 0 && fmodf(a->inv, 0.1f) < 0.05f && a->st != ST_KO) return;
    if (a->ch == CH_ELIRA)
    {
        float yoff;
        int f = EliraFrameIdx(a, &yoff);
        float x = a->x + jx, fl = flash;
        if ((a->shieldOn || a->st == ST_BLOCK) && a->shieldHit > 0)
        { /* bloqueio: tremor e flash branco por cima do 3o quadro */
            float k = Clamp(a->shieldHit / 0.15f, 0.0f, 1.0f);
            x += (Rand01() - 0.5f) * 8 * k;
            fl = fmaxf(fl, 0.3f * k);
        }
        DrawEF(f, x, a->y + yoff, a->face, 1.0f, WHITE, fl);
    }
    else
    {
        Pose p = PoseFor(a);
        DrawPuppet(a->ch, a->x + jx, a->y, a->face, 1.0f, p, flash, a->vx);
    }
    DrawAttackTrail(a, jx);
    /* defesa das IAs: placa hexagonal na frente (a Elira tem quadros proprios) */
    if (a->st == ST_BLOCK && a->ch != CH_ELIRA)
    {
        Color c = CHCOL[a->ch];
        float cy = a->crouch ? a->y - 60 : a->y - 120, cx = a->x + a->face * 52;
        DrawPoly((Vector2){cx, cy}, 6, 40, 0, Fade(c, 0.18f));
        DrawPolyLinesEx((Vector2){cx, cy}, 6, 40, 0, 3, Fade(c, 0.85f));
        DrawPolyLinesEx((Vector2){cx, cy}, 6, 28, 0, 1.5f, Fade(c, 0.5f));
    }
    /* Claude: prisma refletor */
    if (a->ch == CH_CLAUDE && a->st == ST_ATK && a->atk == AT_SPECIAL)
    {
        float t = a->atkT, st = SP_ST[CH_CLAUDE], ac = SP_AC[CH_CLAUDE];
        float vis = t < st ? t / st : (t < st + ac ? 1 : 1 - (t - st - ac) / SP_RC[CH_CLAUDE]);
        float cx = a->x + a->face * 74, cy = a->y - 120;
        Color amb = C_AMBER;
        Vector2 pts[6];
        for (int i = 0; i < 6; i++)
        {
            float an = i * 1.0472f + 0.5236f;
            pts[i] = (Vector2){cx + cosf(an) * 34, cy + sinf(an) * 92};
        }
        for (int i = 0; i < 6; i++) Tri((Vector2){cx, cy}, pts[i], pts[(i + 1) % 6], Fade((Color){255, 220, 170, 255}, 0.18f * vis));
        for (int i = 0; i < 6; i++) DrawLineEx(pts[i], pts[(i + 1) % 6], 3, Fade(amb, vis));
        for (int i = 0; i < 6; i++) DrawLineEx((Vector2){cx, cy}, pts[i], 1, Fade(amb, 0.5f * vis));
        /* faixas refratadas */
        Color bands[4] = {{255, 120, 90, 255}, {255, 210, 110, 255}, {140, 230, 200, 255}, {150, 160, 255, 255}};
        for (int i = 0; i < 4; i++)
        {
            float yy = cy - 30 + i * 20 + sinf(gt * 5 + i) * 4;
            DrawLineEx((Vector2){cx + a->face * 10, yy}, (Vector2){cx + a->face * (60 + i * 10), yy - 18 + i * 12}, 2, Fade(bands[i], 0.6f * vis));
        }
    }
    if (a->beamT > 0)
    { /* feixe de resposta do prisma */
        Vector2 o = (Vector2){a->x + a->face * 74, a->y - 120};
        Color bands[3] = {{255, 140, 100, 255}, {255, 220, 130, 255}, {255, 255, 255, 255}};
        for (int i = 0; i < 3; i++)
            DrawLineEx(Vector2Add(o, (Vector2){0, (i - 1) * 6}), Vector2Add(a->beamTo, (Vector2){0, (i - 1) * 10}), (10 - i * 3) * (a->beamT / 0.35f) + 1, Fade(bands[i], a->beamT / 0.35f));
    }
}

/* ------------------------------------------------------------------ cenario: arena do nucleo */
static unsigned Hash(unsigned x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static void DrawRack(float x, float y, float w, float h, float depth, int seed)
{
    Color body = ColorLerp((Color){20, 24, 40, 255}, (Color){34, 40, 62, 255}, depth);
    Color edge = ColorLerp((Color){30, 36, 58, 255}, (Color){58, 68, 100, 255}, depth);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, body);
    DrawRectangle((int)x, (int)y, (int)w, 4, edge);
    DrawRectangle((int)x, (int)y, 3, (int)h, edge);
    DrawRectangle((int)(x + w - 3), (int)y, 3, (int)h, ColorLerp(body, BLACK, 0.4f));
    int slots = (int)(h / 22);
    for (int j = 0; j < slots; j++)
    {
        float sy = y + 10 + j * 22;
        DrawRectangle((int)x + 8, (int)sy, (int)w - 16, 16, ColorLerp(body, BLACK, 0.35f));
        DrawRectangle((int)x + 8, (int)sy, (int)w - 16, 2, Fade(edge, 0.7f));
        for (int l = 0; l < 3; l++)
        {
            unsigned hsh = Hash((unsigned)(seed * 977 + j * 31 + l * 7 + (int)(gt * (1.5f + l))));
            Color lc = (hsh % 5 == 0) ? (Color){255, 170, 80, 255} : ((hsh % 3 == 0) ? C_GREEN : C_CYAN);
            float on = (hsh % 4) ? 1 : 0.2f;
            DrawRectangle((int)(x + w - 30 + l * 7), (int)sy + 6, 4, 4, Fade(lc, (0.35f + 0.65f * depth) * on));
        }
    }
}
static void DrawStage(void)
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, (int)IAF_GROUND_Y, (Color){6, 8, 20, 255}, (Color){20, 24, 48, 255});
    /* teto com dutos */
    DrawRectangle(0, 0, SCREEN_WIDTH, 46, (Color){12, 14, 28, 255});
    for (int i = 0; i < 8; i++) DrawRectangle(i * 170 + 20, 46, 120, 8, (Color){24, 28, 50, 255});
    /* fileira de tras */
    for (int i = 0; i < 16; i++) DrawRack(i * 84 - 20, 200, 70, 300, 0.1f, i + 40);
    /* telao central */
    Rectangle scr = {390, 96, 500, 170};
    DrawRectangleRounded((Rectangle){scr.x - 10, scr.y - 10, scr.width + 20, scr.height + 20}, 0.08f, 6, (Color){16, 18, 34, 255});
    DrawRectangleRounded(scr, 0.06f, 6, (Color){8, 22, 36, 255});
    DrawRectangleRoundedLinesEx(scr, 0.06f, 6, 2, Fade(C_CYAN, 0.5f));
    TxtC("RINHA DE INTELIGÊNCIAS", scr.x + scr.width / 2, scr.y + 22, 28, Fade(C_CYAN, 0.9f));
    Vector2 prev = {0};
    for (int i = 0; i <= 60; i++)
    {
        float xx = scr.x + 30 + i * (scr.width - 60) / 60.0f;
        float yy = scr.y + 110 + sinf(i * 0.5f + gt * 3) * 14 * sinf(i * 0.13f + gt) + sinf(i * 1.7f - gt * 5) * 5;
        if (i) DrawLineEx(prev, (Vector2){xx, yy}, 2, Fade(C_TEAL, 0.8f));
        prev = (Vector2){xx, yy};
    }
    TxtC("ARENA DO NÚCLEO  //  TRANSMISSÃO AO VIVO", scr.x + scr.width / 2, scr.y + 60, 14, Fade(C_DIM, 0.9f));
    /* cabos pendurados */
    for (int i = 0; i < 6; i++)
    {
        float x0 = 60 + i * 230, x1 = x0 + 140;
        DrawLineBezier((Vector2){x0, 50}, (Vector2){x1, 54}, 4, (Color){10, 12, 22, 255});
        DrawLineBezier((Vector2){x0 + 10, 50}, (Vector2){x1 - 30, 50}, 2, (Color){30, 40, 70, 255});
    }
    /* fileira da frente (laterais) */
    for (int i = 0; i < 3; i++) DrawRack(i * 104 - 30, 250, 96, 380, 0.75f, i);
    for (int i = 0; i < 3; i++) DrawRack(SCREEN_WIDTH - 70 - i * 104, 250, 96, 380, 0.75f, i + 10);
    /* piso em perspectiva */
    DrawRectangleGradientV(0, (int)IAF_GROUND_Y - 30, SCREEN_WIDTH, SCREEN_HEIGHT - (int)IAF_GROUND_Y + 30, (Color){22, 26, 48, 255}, (Color){8, 10, 20, 255});
    Vector2 vp = (Vector2){SCREEN_WIDTH / 2.0f, 360};
    for (int i = -12; i <= 12; i++)
    {
        float xb = SCREEN_WIDTH / 2.0f + i * 130;
        Vector2 a = (Vector2){Lerp(vp.x, xb, (IAF_GROUND_Y - 30 - vp.y) / (SCREEN_HEIGHT - vp.y)), IAF_GROUND_Y - 30};
        DrawLineEx(a, (Vector2){xb, SCREEN_HEIGHT}, 1.5f, Fade(C_CYAN, 0.12f));
    }
    for (int j = 0; j < 7; j++)
    {
        float yy = IAF_GROUND_Y - 30 + powf(j / 6.0f, 1.8f) * (SCREEN_HEIGHT - IAF_GROUND_Y + 30);
        DrawLineEx((Vector2){0, yy}, (Vector2){SCREEN_WIDTH, yy}, 1, Fade(C_CYAN, 0.10f + j * 0.015f));
    }
    /* borda da arena */
    DrawRectangle(0, (int)IAF_GROUND_Y + 10, SCREEN_WIDTH, 3, Fade(C_CYAN, 0.55f));
    DrawRectangle(0, (int)IAF_GROUND_Y + 13, SCREEN_WIDTH, 6, Fade(C_CYAN, 0.12f));
    for (int i = 0; i < 20; i++)
    {
        float xx = 40 + i * 64;
        float on = 0.4f + 0.6f * (0.5f + 0.5f * sinf(gt * 2 + i * 0.7f));
        DrawRectangle((int)xx, (int)IAF_GROUND_Y + 9, 20, 5, Fade(C_CYAN, on));
    }
}

/* ------------------------------------------------------------------ jogo: estado */
typedef enum { SC_SELECT, SC_VERSUS, SC_FIGHT, SC_RESULT, SC_FINAL } IafScreen;
typedef enum { RS_INTRO, RS_FIGHT, RS_KO, RS_TIMEUP } RState;
static IafScreen iafScreen = SC_SELECT;
static int sel = 0, playerCh = CH_ELIRA;
static int ladder[3], ladderPos = 0;
static int roundNo = 1, wins[2];
static float roundTimer = IAF_ROUND_TIME, rsT = 0;
static RState rstate = RS_INTRO;
static float scT = 0;
static bool lastWon = false, paused = false;
static int roundWinner = -1;

typedef struct { float t, hold; int mode; bool reacted, low; IafInput once; float skill, shotCd; } Ai;
enum { AI_IDLE, AI_FWD, AI_BACK, AI_BLOCK, AI_JUMPIN };
static Ai ai[2];
static bool cpu[2] = {false, true};

static float AtkTotal(Fighter *a)
{
    if (a->atk == AT_SPECIAL) return SP_ST[a->ch] + SP_AC[a->ch] + SP_RC[a->ch];
    if (a->atk == AT_SHOT) return IAF_SHOT_STARTUP + IAF_SHOT_ACTIVE + IAF_SHOT_RECOVERY;
    const Atk *d = &ATK[a->ch][a->atk];
    return d->st + d->ac + d->rc;
}
static Rectangle Hurt(Fighter *a)
{
    float w = a->ch == CH_CODEX ? 84 : 70, h = a->crouch ? 128 : (a->ch == CH_ELIRA ? 196 : 200);
    if (a->down || a->st == ST_KO) h = 40;
    return (Rectangle){a->x - w / 2, a->y - h, w, h};
}
static bool Overlap(Rectangle a, Rectangle b) { return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height; }
static bool OnGround(Fighter *a) { return a->y >= IAF_GROUND_Y - 0.5f; }
static void SetState(Fighter *a, FState s) { a->st = s; a->stT = 0; }

static void ResetFighter(Fighter *a, CharId ch, float x, int face)
{
    memset(a, 0, sizeof(*a));
    a->ch = ch; a->x = x; a->y = IAF_GROUND_Y; a->face = face; a->hp = a->hpShown = IAF_HP_MAX;
    a->st = ST_IDLE;
}
static void ClearFx(void)
{
    memset(proj, 0, sizeof(proj));
    memset(parts, 0, sizeof(parts));
    memset(impacts, 0, sizeof(impacts));
    memset(&well, 0, sizeof(well));
    hitstop = 0; shake = 0;
}

/* ------------------------------------------------------------------ dano */
static int Idx(Fighter *a) { return a == &fz[0] ? 0 : 1; }
static void KO(Fighter *d, int dir)
{
    d->hp = 0;
    SetState(d, ST_KO);
    d->vx = dir * 380; d->vy = -520; d->crouch = false; d->guard = false;
    hitstop = 0.18f; shake = 0.4f;
}
static bool PrismActive(Fighter *d)
{
    if (d->ch != CH_CLAUDE || d->st != ST_ATK || d->atk != AT_SPECIAL || d->countered) return false;
    return d->atkT >= SP_ST[CH_CLAUDE] * 0.5f && d->atkT < SP_ST[CH_CLAUDE] + SP_AC[CH_CLAUDE];
}
static void ApplyHit(Fighter *att, Fighter *def, float dmg, float stun, float push, int type, Vector2 at, bool unblockable, bool launch);
static int g_projHit = 0; /* 1 = acerto de tiro, 2 = acerto do nucleo da rajada (durante ApplyHit) */
/* pausa de impacto (congela os dois lutadores) conforme o tipo de golpe */
static float HitstopFor(Fighter *att)
{
    if (g_projHit) return g_projHit == 2 ? IAF_HITSTOP_SPECIAL : IAF_HITSTOP_LIGHT;
    if (att->st != ST_ATK) return IAF_HITSTOP_LIGHT;
    if (att->atk == AT_HEAVY) return IAF_HITSTOP_HEAVY;
    if (att->atk == AT_SPECIAL) return IAF_HITSTOP_SPECIAL;
    return IAF_HITSTOP_LIGHT;
}
static void PrismCounter(Fighter *cl, Fighter *att)
{
    cl->countered = true;
    cl->beamT = 0.35f;
    cl->beamTo = (Vector2){att->x, att->y - 110};
    cl->atkT = SP_ST[CH_CLAUDE] + SP_AC[CH_CLAUDE]; /* vai para a recuperacao */
    AddText((Vector2){cl->x, cl->y - 250}, "REFLEXÃO!", C_AMBER, 26);
    ApplyHit(cl, att, 130, 0.6f, 420, 0, (Vector2){att->x, att->y - 110}, true, true);
}
static void ApplyHit(Fighter *att, Fighter *def, float dmg, float stun, float push, int type, Vector2 at, bool unblockable, bool launch)
{
    if (def->st == ST_KO || def->down || def->inv > 0) return;
    bool held = well.on && well.caught && def == &fz[1 - well.owner];
    if (!OnGround(def) && !held && def->juggle >= 3) return; /* no maximo 3 golpes por queda */
    if (PrismActive(def) && (att->x - def->x) * def->face > 0) { PrismCounter(def, att); return; }
    bool blocked = !unblockable && def->guard && (type == 0 || (type == 1 && def->crouch) || (type == 2 && !def->crouch));
    int dir = att->x < def->x ? 1 : -1;
    if (blocked)
    {
        def->hp -= dmg * 0.12f;
        SetState(def, ST_BLOCK);
        def->stun = stun * 0.65f;
        def->vx = dir * push * 0.8f;
        def->meter = fminf(IAF_METER_MAX, def->meter + 4);
        def->shieldHit = 0.15f;
        if (def->ch != CH_ELIRA) BlockSpark(at, CHCOL[def->ch], dir); /* a faisca do escudo da Elira ja vem no quadro */
        hitstop = fmaxf(hitstop, HitstopFor(att) * 0.7f);
        def->hitShake = hitstop;
        if (def->hp <= 0) KO(def, dir);
        return;
    }
    def->hp -= dmg;
    def->flash = def->ch == CH_ELIRA ? 0 : 0.12f; /* quadros de dano da Elira ja trazem o efeito */
    def->hitCrouch = def->crouch;
    def->crouch = false;
    def->guard = false;
    SetState(def, ST_HIT);
    def->stun = stun;
    def->vx = dir * push;
    def->perfect = 0;
    if (launch || !OnGround(def)) { def->vy = -560; def->down = false; def->stun = stun + 0.3f; def->juggle++; }
    def->meter = fminf(IAF_METER_MAX, def->meter + 6);
    att->meter = fminf(IAF_METER_MAX, att->meter + dmg * 0.09f);
    att->combo = (att->comboT > 0) ? att->combo + 1 : 1;
    att->comboT = 0.9f;
    if (att->combo >= 2)
    {
        char b[32];
        snprintf(b, sizeof b, "%d ACERTOS", att->combo);
        AddText((Vector2){att->x, att->y - 240}, b, CHCOL[att->ch], 22);
    }
    if (def->ch != CH_ELIRA)
    { /* sem efeito por cima dos quadros de reacao da Elira */
        if (att->ch == CH_ELIRA) AddImpact(at, 1.6f + dmg / 120.0f, dir);
        HitSparks(at, att->ch == CH_ELIRA ? C_CYAN : CHCOL[att->ch], 10 + (int)(dmg / 10), 420);
    }
    /* estrela de impacto no ponto do acerto (menor sobre a Elira, cujos quadros de dano ja tem efeito) */
    ImpactSpark(at, att->ch == CH_ELIRA ? C_CYAN : CHCOL[att->ch], (28 + dmg * 0.22f) * (def->ch == CH_ELIRA ? 0.6f : 1.0f));
    hitstop = fmaxf(hitstop, HitstopFor(att));
    def->hitShake = hitstop;
    shake = fmaxf(shake, dmg / 600.0f);
    if (def->hp <= 0) KO(def, dir);
}

/* ------------------------------------------------------------------ especiais */
static void SpawnProj(Fighter *a, bool big)
{
    for (int i = 0; i < IAF_MAX_PROJ; i++)
        if (!proj[i].on)
        {
            proj[i] = (Proj){true, a->x + a->face * (big ? 140 : 96), a->y - IAF_MUZZLE_Y, a->face * (big ? 560.0f : 720.0f), Idx(a), big, false, big ? 55.0f : 70.0f, 0, 0, big ? 3 : 1};
            return;
        }
}
static bool EliraShotOnScreen(Fighter *a)
{ /* so 1 tiro de plasma dela na tela por vez */
    for (int i = 0; i < IAF_MAX_PROJ; i++)
        if (proj[i].on && !proj[i].big && proj[i].owner == Idx(a) && !proj[i].reflected) return true;
    return false;
}
static void UpdateSpecial(Fighter *a, Fighter *o, float dt)
{
    float t = a->atkT, st = SP_ST[a->ch], ac = SP_AC[a->ch];
    switch (a->ch)
    {
    case CH_ELIRA:
        if (t >= st && !a->spawned) { a->spawned = true; SpawnProj(a, true); MuzzleFlash((Vector2){a->x + a->face * 120, a->y - IAF_MUZZLE_Y}, 44); a->vx = -a->face * 120; AddText((Vector2){a->x, a->y - 240}, "RAJADA TEMPORAL!", C_CYAN, 22); }
        break;
    case CH_CLAUDE:
        if (t >= st + ac && !a->spawned && !a->countered)
        { /* sem ataque para refletir: pulso curto de luz */
            a->spawned = true;
            HitSparks((Vector2){a->x + a->face * 80, a->y - 120}, C_AMBER, 14, 300);
            if (fabsf(o->x - a->x) < 170 && fabsf(o->y - a->y) < 140) ApplyHit(a, o, 60, 0.35f, 260, 0, (Vector2){o->x, o->y - 120}, false, false);
        }
        break;
    case CH_CODEX:
        if (t >= st && t < st + ac && !a->caught)
        {
            a->vx = a->face * 1150;
            if ((int)(t * 60) % 2 == 0)
            {
                static const char *G[] = {"{}", "=>", "0x", ";;", "//", "&&", "[]", "!="};
                for (int i = 0; i < IAF_MAX_PARTS; i++)
                    if (parts[i].life <= 0)
                    {
                        parts[i] = (Part){(Vector2){a->x - a->face * 40, a->y - 60 - Rand01() * 120}, (Vector2){-a->face * 80, -20}, 0.45f, 0.45f, 16 + Rand01() * 8, C_GREEN, 2, ""};
                        strncpy(parts[i].txt, G[GetRandomValue(0, 7)], 11);
                        break;
                    }
            }
            Rectangle hb = {a->x + (a->face > 0 ? 0 : -90), a->y - 190, 90, 170};
            if (Overlap(hb, Hurt(o)) && !o->down && o->st != ST_KO && o->inv <= 0)
            {
                a->vx = 0;
                if (PrismActive(o)) { PrismCounter(o, a); break; }
                if (o->guard)
                {
                    ApplyHit(a, o, 30, 0.3f, 300, 0, (Vector2){o->x - a->face * 20, o->y - 120}, false, false);
                    a->atkT = st + ac; /* parou no bloqueio */
                }
                else { a->caught = true; a->spHits = 0; a->spT = 0; }
            }
        }
        if (a->caught)
        {
            a->vx = 0;
            a->atkT = fminf(a->atkT, st + ac - 0.01f); /* segura a janela enquanto a sequencia roda */
            a->spT += dt;
            o->x = a->x + a->face * 104;
            if (o->st == ST_HIT) o->stun = fmaxf(o->stun, 0.2f);
            static const char *W[] = {"COMPILA", "LINKA", "TESTA", "EMPACOTA"};
            if (a->spT > 0.09f * (a->spHits + 1) && a->spHits < 4)
            {
                AddText((Vector2){o->x, o->y - 220 - a->spHits * 8}, W[a->spHits], C_GREEN, 18);
                ApplyHit(a, o, 32, 0.4f, 0, 0, (Vector2){o->x - a->face * 20, o->y - 100 - a->spHits * 18}, true, false);
                a->spHits++;
            }
            else if (a->spHits >= 4 && a->spT > 0.5f)
            {
                AddText((Vector2){o->x, o->y - 260}, "DEPLOY!", C_GREEN, 30);
                ApplyHit(a, o, 70, 0.6f, 460, 0, (Vector2){o->x - a->face * 20, o->y - 120}, true, true);
                a->caught = false;
                a->atkT = st + ac;
            }
            if (o->st == ST_KO) { a->caught = false; a->atkT = st + ac; }
        }
        break;
    case CH_ANTI:
        if (t >= st && !a->spawned)
        {
            a->spawned = true;
            float dx = o->x - a->x;
            float wx = (fabsf(dx) < 520 && dx * a->face > 0) ? o->x : a->x + a->face * 330;
            if (wx < IAF_WALL_L + 40) wx = IAF_WALL_L + 40;
            if (wx > IAF_WALL_R - 40) wx = IAF_WALL_R - 40;
            well = (Well){true, wx, IAF_GROUND_Y - 170, 0, 1.25f, Idx(a), false, false};
            /* aberto com o oponente caido/atordoado: o poco nao prende (evita emendar queda -> poco -> queda) */
            if (o->down || o->st == ST_HIT || o->st == ST_KO) well.resisted = true;
            AddText((Vector2){a->x, a->y - 250}, "POÇO GRAVITACIONAL!", C_VIOLET, 22);
        }
        break;
    default: break;
    }
}
static void UpdateWell(float dt)
{
    if (!well.on) return;
    Fighter *a = &fz[well.owner], *o = &fz[1 - well.owner];
    well.t += dt;
    for (int i = 0; i < 3; i++)
    {
        float an = Rand01() * 6.283f, r = 90 + Rand01() * 60;
        Vector2 p = (Vector2){well.x + cosf(an) * r, well.y + sinf(an) * r};
        AddPart(p, Vector2Scale(Vector2Subtract((Vector2){well.x, well.y}, p), 2.2f), 0.4f, 2.5f, Rand01() < 0.5f ? C_VIOLET : C_TEAL, 1);
    }
    /* nao captura quem ja esta atordoado (evita emendar pocos sem fim) */
    if (!well.caught && !well.resisted && well.t < well.life - 0.3f && o->st != ST_KO && !o->down && o->st != ST_HIT && o->inv <= 0)
    {
        float dx = well.x - o->x;
        if (fabsf(dx) < 240)
        {
            if (PrismActive(o)) { well.resisted = true; well.life = well.t + 0.25f; AddText((Vector2){o->x, o->y - 240}, "ANULADO", C_AMBER, 20); }
            else if (o->guard && OnGround(o)) { well.resisted = true; ApplyHit(a, o, 25, 0.3f, 0, 0, (Vector2){o->x, o->y - 100}, false, false); }
            else { well.caught = true; ApplyHit(a, o, 20, 0.5f, 0, 0, (Vector2){o->x, o->y - 100}, true, false); }
        }
        else o->x += (dx > 0 ? 1 : -1) * 140 * dt; /* puxa de longe */
    }
    if (well.caught && o->st != ST_KO)
    {
        o->x = Lerp(o->x, well.x, Clamp(dt * 6, 0.0f, 1.0f));
        o->y = Lerp(o->y, well.y + 90, Clamp(dt * 5, 0.0f, 1.0f));
        o->vx = 0; o->vy = 0;
        SetState(o, ST_HIT);
        o->stun = 0.3f;
    }
    if (well.t >= well.life)
    {
        if (well.caught && o->st != ST_KO)
        {
            HitSparks((Vector2){well.x, well.y}, C_VIOLET, 30, 500);
            AddText((Vector2){well.x, well.y - 120}, "QUEDA LIVRE!", C_VIOLET, 24);
            ApplyHit(a, o, 130, 0.7f, 120, 0, (Vector2){o->x, o->y - 100}, true, true);
            o->vy = 900; /* derrubada para baixo */
        }
        well.on = false;
    }
}

/* ------------------------------------------------------------------ atualizacao de um lutador */
static void StartAtk(Fighter *a, AtkKind k)
{
    a->atk = k; a->atkT = 0; a->hitDone = false; a->spawned = false; a->caught = false; a->countered = false;
    if (a->st != ST_JUMP) a->vx = 0;
    SetState(a, ST_ATK);
}
static void UpdateFighter(Fighter *a, Fighter *o, IafInput in, float dt)
{
    a->stT += dt; a->anim += dt;
    if (a->flash > 0) a->flash -= dt;
    if (a->inv > 0) a->inv -= dt;
    if (a->beamT > 0) a->beamT -= dt;
    if (a->comboT > 0) a->comboT -= dt; else a->combo = 0;
    if (a->shotCd > 0) a->shotCd -= dt;
    if (a->shieldHit > 0) a->shieldHit -= dt;
    a->meter = fminf(IAF_METER_MAX, a->meter + dt * 1.2f);
    bool ground = OnGround(a);
    int toward = o->x > a->x ? 1 : -1;
    bool canAct = a->st == ST_IDLE || a->st == ST_WALK || a->st == ST_CROUCH || (a->st == ST_BLOCK && a->stun <= 0);
    if (ground && (canAct || a->st == ST_JUMP)) a->face = toward;
    bool holdBack = (a->face > 0 && in.left) || (a->face < 0 && in.right);
    bool holdFwd = (a->face > 0 && in.right) || (a->face < 0 && in.left);

    switch (a->st)
    {
    case ST_KO:
        a->vx *= 1 - 3 * dt;
        break;
    case ST_WIN:
    case ST_LOSE:
        a->vx = 0;
        break;
    case ST_HIT:
        a->stun -= dt;
        if (ground) a->vx *= 1 - 8 * dt;
        if (a->stun <= 0 && ground)
        {
            if (a->stT > 0.05f && (a->downT > 0 || a->vy > 0)) { }
            SetState(a, ST_IDLE);
        }
        break;
    case ST_BLOCK:
        a->stun -= dt;
        a->vx *= 1 - 9 * dt;
        if (a->stun <= 0) { SetState(a, ST_IDLE); }
        break;
    case ST_ATK:
    {
        a->atkT += dt;
        if (ground && (a->atk == AT_LIGHT || a->atk == AT_HEAVY || a->atk == AT_LOW))
        { /* avanco do corpo durante o preparo e o ativo do golpe */
            const Atk *d = &ATK[a->ch][a->atk];
            float win = d->st + d->ac, t0 = a->atkT - dt;
            float adv = a->atk == AT_LIGHT ? IAF_LUNGE_LIGHT : (a->atk == AT_HEAVY ? IAF_LUNGE_HEAVY : IAF_LUNGE_LOW);
            if (t0 < win) a->x += a->face * adv * fminf(dt, win - t0) / win;
        }
        if (a->atk == AT_SPECIAL) UpdateSpecial(a, o, dt);
        else if (a->atk == AT_SHOT)
        {
            if (a->atkT >= IAF_SHOT_STARTUP && !a->spawned)
            {
                a->spawned = true;
                SpawnProj(a, false);
                a->shotCd = IAF_SHOT_COOLDOWN;
                MuzzleFlash((Vector2){a->x + a->face * 96, a->y - IAF_MUZZLE_Y}, 26);
                a->x -= a->face * IAF_SHOT_RECOIL; /* recuo do disparo */
            }
        }
        else
        {
            const Atk *d = &ATK[a->ch][a->atk];
            if (a->atkT >= d->st && a->atkT < d->st + d->ac && !a->hitDone)
            {
                float x0 = a->x + a->face * 10, x1 = a->x + a->face * d->rx;
                float top = (a->atk == AT_AIR) ? a->y - d->top : a->y - d->top;
                Rectangle hb = {fminf(x0, x1), top, fabsf(x1 - x0), d->h};
                if (Overlap(hb, Hurt(o)))
                {
                    a->hitDone = true;
                    Vector2 at = (Vector2){o->x - a->face * 28, top + d->h * 0.5f};
                    ApplyHit(a, o, d->dmg, d->stun, d->push, d->type, at, false, a->atk == AT_HEAVY && a->ch == CH_CLAUDE && !OnGround(o));
                }
            }
        }
        if (a->st == ST_ATK && a->atkT >= AtkTotal(a))
        {
            if (a->atk == AT_AIR && !ground) { a->st = ST_JUMP; }
            else SetState(a, ST_IDLE);
            a->atk = AT_NONE;
        }
        if (a->atk == AT_AIR && ground && a->vy >= 0 && a->stT > 0.05f) { SetState(a, ST_IDLE); a->atk = AT_NONE; }
        if (a->atk != AT_SPECIAL && a->atk != AT_AIR && ground) a->vx *= 1 - 10 * dt;
        if (a->atk == AT_SPECIAL && a->ch != CH_CODEX) a->vx *= 1 - 10 * dt;
        if (a->atk == AT_SPECIAL && a->ch == CH_CODEX && a->atkT > SP_ST[CH_CODEX] + SP_AC[CH_CODEX]) a->vx *= 1 - 12 * dt;
    }
    break;
    case ST_JUMP:
        if ((in.lightP || in.heavyP) && !a->airAtkUsed) { a->airAtkUsed = true; a->atk = AT_AIR; a->atkT = 0; a->hitDone = false; a->st = ST_ATK; a->stT = 0; }
        break;
    default: /* parado / andando / agachado */
        a->crouch = ground && in.down;
        if (in.specialP && a->meter >= IAF_SPECIAL_COST && !(a->ch == CH_ANTI && well.on)) { a->meter -= IAF_SPECIAL_COST; a->crouch = false; StartAtk(a, AT_SPECIAL); break; }
        if (in.shotP && a->ch == CH_ELIRA && a->shotCd <= 0 && !EliraShotOnScreen(a)) { a->crouch = false; StartAtk(a, AT_SHOT); break; }
        if (in.lightP) { StartAtk(a, a->crouch ? AT_LOW : AT_LIGHT); break; }
        if (in.heavyP) { StartAtk(a, a->crouch ? AT_LOW : AT_HEAVY); break; }
        if (in.upP && ground)
        {
            a->crouch = false;
            a->vy = -IAF_JUMP_SPEED;
            a->vx = holdFwd ? a->face * 300.0f : (holdBack ? -a->face * 260.0f : 0);
            a->airAtkUsed = false; a->hoverT = 0;
            SetState(a, ST_JUMP);
            break;
        }
        if (a->crouch) { if (a->st != ST_CROUCH) SetState(a, ST_CROUCH); a->vx = 0; }
        else if (holdFwd || holdBack)
        {
            float sp = a->ch == CH_ANTI ? 290 : (a->ch == CH_CODEX ? 270 : 255);
            a->vx = holdFwd ? a->face * sp : -a->face * sp * 0.8f;
            if (a->st != ST_WALK) SetState(a, ST_WALK);
        }
        else { a->vx = 0; if (a->st != ST_IDLE) SetState(a, ST_IDLE); }
        break;
    }
    /* defesa: segurar para tras no chao, sem estar atacando/apanhando */
    a->guard = ground && holdBack && (a->st == ST_IDLE || a->st == ST_WALK || a->st == ST_CROUCH || a->st == ST_BLOCK);
    { /* defesa por proximidade: segurando para tras com golpe/projetil/poco vindo, ela para e abre a defesa */
        bool threatNear = (o->st == ST_ATK && fabsf(o->x - a->x) < 330) || (well.on && well.owner != Idx(a));
        for (int i = 0; i < IAF_MAX_PROJ && !threatNear; i++)
            if (proj[i].on && proj[i].owner != Idx(a) && (a->x - proj[i].x) * proj[i].vx > 0 && fabsf(a->x - proj[i].x) < 380) threatNear = true;
        bool on = a->guard && (threatNear || a->st == ST_BLOCK);
        if (on)
        {
            if (!a->shieldOn) a->shieldT = 0;
            a->shieldT += dt;
            if (a->st == ST_WALK) { a->vx = 0; SetState(a, ST_IDLE); }
        }
        a->shieldOn = on;
    }
    if (a->st != ST_IDLE && a->st != ST_WALK && a->st != ST_CROUCH && a->st != ST_BLOCK && !(a->st == ST_ATK && a->atk == AT_LOW)) a->crouch = false;
    if (a->st == ST_ATK && a->atk == AT_LOW) a->crouch = true;

    /* fisica */
    bool held = well.on && well.caught && a == &fz[1 - well.owner];
    if (!held)
    {
        float g = IAF_GRAVITY;
        if (a->ch == CH_ANTI && !ground && in.up && a->vy > 0 && a->hoverT < 0.9f) { g = IAF_GRAVITY * 0.12f; a->hoverT += dt; } /* plana */
        if (!ground || a->vy < 0) a->vy += g * dt;
        if (a->ch == CH_ANTI && !ground && in.up && a->vy > 120 && a->hoverT < 0.9f) a->vy = 120;
        a->x += a->vx * dt;
        a->y += a->vy * dt;
        if (a->y >= IAF_GROUND_Y)
        {
            a->juggle = 0;
            bool wasAir = a->vy > 300;
            a->y = IAF_GROUND_Y;
            if (a->st == ST_KO) { a->vy = 0; a->vx *= 0.5f; }
            else if (a->st == ST_HIT && wasAir && a->stT > 0.05f) { a->down = true; a->downT = 0.75f; a->vy = 0; shake = fmaxf(shake, 0.15f); HitSparks((Vector2){a->x, IAF_GROUND_Y}, C_DIM, 8, 200); }
            else a->vy = 0;
            if (a->st == ST_JUMP) { SetState(a, ST_IDLE); a->vx = 0; }
        }
    }
    if (a->down)
    {
        a->downT -= dt;
        a->vx *= 1 - 6 * dt;
        if (a->st == ST_HIT) a->stun = fmaxf(a->stun, 0.05f);
        if (a->downT <= 0) { a->down = false; a->inv = 0.35f; SetState(a, ST_IDLE); a->stun = 0; }
    }
    if (a->x < IAF_WALL_L) a->x = IAF_WALL_L;
    if (a->x > IAF_WALL_R) a->x = IAF_WALL_R;
    a->hpShown = a->hpShown > a->hp ? fmaxf(a->hp, a->hpShown - 260 * dt * (a->flash > 0 ? 0 : 1)) : a->hp;
}

static void UpdateProjectiles(float dt)
{
    for (int i = 0; i < IAF_MAX_PROJ; i++)
    {
        Proj *p = &proj[i];
        if (!p->on) continue;
        p->t += dt;
        p->x += p->vx * dt;
        if (p->hitCd > 0) p->hitCd -= dt;
        if (p->x < -80 || p->x > SCREEN_WIDTH + 80) { p->on = false; continue; }
        if (p->big && (int)(p->t * 60) % 2 == 0) AddPart((Vector2){p->x - (p->vx > 0 ? 40 : -40), p->y + (Rand01() - 0.5f) * 30}, (Vector2){-p->vx * 0.2f, 0}, 0.35f, 3, C_CYAN, 1);
        Fighter *o = &fz[1 - p->owner];
        float r = p->big ? 38 : 20;
        /* choque entre projeteis */
        for (int j = 0; j < IAF_MAX_PROJ; j++)
        {
            Proj *q = &proj[j];
            if (j == i || !q->on || q->owner == p->owner) continue;
            if (fabsf(q->x - p->x) < 40 && fabsf(q->y - p->y) < 40)
            {
                AddImpact((Vector2){(p->x + q->x) / 2, p->y}, 2.2f, 1);
                if (p->big && !q->big) q->on = false;
                else if (q->big && !p->big) p->on = false;
                else { p->on = false; q->on = false; }
            }
        }
        if (!p->on) continue;
        Rectangle hb = {p->x - r, p->y - r, r * 2, r * 2};
        if (p->hitCd <= 0 && Overlap(hb, Hurt(o)) && o->st != ST_KO && !o->down)
        {
            if (PrismActive(o) && (p->vx > 0) != (o->face > 0))
            { /* reflete */
                p->owner = 1 - p->owner;
                p->vx = -p->vx * 1.2f;
                p->dmg *= 1.5f;
                p->reflected = true;
                p->hitCd = 0.1f;
                HitSparks((Vector2){p->x, p->y}, C_AMBER, 14, 300);
                AddText((Vector2){o->x, o->y - 250}, "REFLETIDO!", C_AMBER, 24);
                o->meter = fminf(IAF_METER_MAX, o->meter + 10);
                continue;
            }
            Fighter *a = &fz[p->owner];
            g_projHit = p->big ? 2 : 1;
            ApplyHit(a, o, p->dmg, p->big ? 0.35f : 0.4f, p->big ? 120 : 220, 0, (Vector2){p->x, p->y}, false, false);
            g_projHit = 0;
            p->hits--;
            p->hitCd = 0.16f;
            if (p->hits <= 0) { p->on = false; AddImpact((Vector2){p->x, p->y}, p->big ? 3.4f : 2.0f, p->vx > 0 ? 1 : -1); }
        }
    }
}

static void UpdateParts(float dt)
{
    for (int k = 0; k < 2; k++)
        if (fz[k].hitShake > 0) fz[k].hitShake -= dt; /* corre mesmo durante a pausa de impacto */
    for (int i = 0; i < IAF_MAX_PARTS; i++)
    {
        Part *p = &parts[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->p = Vector2Add(p->p, Vector2Scale(p->v, dt));
        if (p->kind == 0) p->v = Vector2Scale(p->v, 1 - 4 * dt);
        if (p->kind == 5) p->v.y += 900 * dt; /* lascas caem */
    }
    for (int i = 0; i < 16; i++)
        if (impacts[i].on) { impacts[i].t += dt; if (impacts[i].t > 0.32f) impacts[i].on = false; }
}
static void DrawParts(void)
{
    for (int i = 0; i < IAF_MAX_PARTS; i++)
    {
        Part *p = &parts[i];
        if (p->life <= 0) continue;
        float k = p->life / p->max;
        if (p->kind == 0)
        {
            Vector2 d = Vector2Scale(Vector2Normalize(p->v), 6 + p->size * 2);
            DrawLineEx(Vector2Subtract(p->p, d), p->p, p->size, Fade(p->c, k));
        }
        else if (p->kind == 1) DrawCircleV(p->p, p->size, Fade(p->c, k));
        else if (p->kind == 2) TxtO(p->txt, p->p.x, p->p.y, (int)p->size, Fade(p->c, fminf(1, k * 2)), Fade(C_INK, fminf(1, k * 2)));
        else if (p->kind == 3) { float r = p->size * (1.4f - k); DrawRing(p->p, r - 3, r, 0, 360, 32, Fade(p->c, k * 0.8f)); }
        else if (p->kind == 4)
        { /* estrela de impacto: 8 pontas que crescem e afinam */
            float q = 1 - k, rot = p->v.x;
            for (int s = 0; s < 8; s++)
            {
                float an = rot + s * 0.785f, len = p->size * ((s % 2 ? 0.55f : 1.0f) * (0.6f + 0.6f * q)), w = (s % 2 ? 3.0f : 5.0f) * k + 1;
                Vector2 d = {cosf(an), sinf(an)}, n = {-d.y, d.x};
                Vector2 tip = Vector2Add(p->p, Vector2Scale(d, len));
                Vector2 b0 = Vector2Add(Vector2Add(p->p, Vector2Scale(d, p->size * 0.12f)), Vector2Scale(n, w));
                Vector2 b1 = Vector2Subtract(Vector2Add(p->p, Vector2Scale(d, p->size * 0.12f)), Vector2Scale(n, w));
                Tri(b0, b1, tip, Fade(p->c, k));
                Tri(Vector2Add(p->p, Vector2Scale(n, w * 0.5f)), Vector2Subtract(p->p, Vector2Scale(n, w * 0.5f)), Vector2Add(p->p, Vector2Scale(d, len * 0.6f)), Fade(WHITE, k));
            }
            DrawCircleV(p->p, p->size * 0.22f * k + 2, Fade(WHITE, k));
        }
        else if (p->kind == 7)
        { /* clarao do canhao */
            float q = 1 - k;
            DrawCircleV(p->p, p->size * (0.55f + 0.3f * q), Fade(p->c, 0.45f * k));
            for (int s = 0; s < 4; s++)
            {
                float an = s * 1.5708f + 0.785f;
                Vector2 d = {cosf(an), sinf(an)};
                DrawLineEx(Vector2Add(p->p, Vector2Scale(d, p->size * 0.3f)), Vector2Add(p->p, Vector2Scale(d, p->size * (0.8f + 0.5f * q))), 3 * k + 1, Fade(WHITE, k));
            }
            DrawLineEx(Vector2Add(p->p, (Vector2){-p->size * 1.1f, 0}), Vector2Add(p->p, (Vector2){p->size * 1.1f, 0}), 4 * k + 1, Fade(WHITE, k));
            DrawCircleV(p->p, p->size * 0.32f * k + 2, Fade(WHITE, k));
            DrawRing(p->p, p->size * (0.6f + q), p->size * (0.6f + q) + 2, 0, 360, 28, Fade(p->c, k));
        }
        else if (p->kind == 5) DrawPoly(p->p, 6, p->size, p->life * 900, Fade(ColorLerp(p->c, WHITE, 0.4f), k));
        else if (p->kind == 6)
        { /* clarao vertical do bloqueio */
            float h = p->size * (1.2f + (1 - k) * 0.8f);
            DrawRectangleRec((Rectangle){p->p.x - 5 * k - 1, p->p.y - h, 10 * k + 2, h * 2}, Fade(p->c, 0.5f * k));
            DrawRectangleRec((Rectangle){p->p.x - 1.5f, p->p.y - h * 0.8f, 3, h * 1.6f}, Fade(WHITE, k));
        }
    }
    for (int i = 0; i < 16; i++)
        if (impacts[i].on)
        {
            int f = (int)(impacts[i].t / 0.32f * 4);
            DrawEFC(EAFrame(EA_IMPACT, f), impacts[i].p, impacts[i].face, impacts[i].sc * 0.4f, WHITE);
        }
}
static void DrawProjectiles(void)
{
    for (int i = 0; i < IAF_MAX_PROJ; i++)
    {
        Proj *p = &proj[i];
        if (!p->on) continue;
        int face = p->vx > 0 ? 1 : -1;
        Color tint = p->reflected ? (Color){255, 210, 140, 255} : WHITE;
        if (p->big)
        {
            for (int k = 3; k >= 1; k--) DrawEFC(EAFrame(EA_PROJ, 3), (Vector2){p->x - face * k * 26, p->y}, face, 1.2f - k * 0.12f, Fade(tint, 0.25f));
            DrawEFC(EAFrame(EA_PROJ, 2 + ((int)(p->t * 12) % 2)), (Vector2){p->x, p->y}, face, 1.3f, tint);
            /* anel de relogio: marca a "Rajada Temporal" */
            for (int k = 0; k < 12; k++)
            {
                float an = k * 0.5236f - p->t * 6 * face;
                float r0 = 44, r1 = k % 3 == 0 ? 56 : 51;
                DrawLineEx((Vector2){p->x + cosf(an) * r0, p->y + sinf(an) * r0}, (Vector2){p->x + cosf(an) * r1, p->y + sinf(an) * r1}, 3, Fade(p->reflected ? C_AMBER : C_CYAN, 0.9f));
            }
            DrawRing((Vector2){p->x, p->y}, 42, 44, 0, 360, 40, Fade(C_CYAN, 0.5f));
        }
        else
            DrawEFC(EAFrame(EA_PROJ, p->t < 0.035f ? 0 : (p->t < 0.07f ? 1 : 2 + ((int)(p->t * 12) % 2))), (Vector2){p->x, p->y}, face, 0.8f, tint);
    }
}
static void DrawWell(void)
{
    if (!well.on) return;
    float k = Clamp(well.t / 0.2f, 0.0f, 1.0f) * Clamp((well.life - well.t) / 0.15f + 0.2f, 0.0f, 1.0f);
    Vector2 c = (Vector2){well.x, well.y};
    for (int i = 0; i < 5; i++)
    {
        float r = fmodf(140 - well.t * 160 + i * 28, 140);
        DrawRing(c, r, r + 2.5f, 0, 360, 48, Fade(i % 2 ? C_VIOLET : C_TEAL, k * (r / 140) * 0.8f));
    }
    DrawCircleV(c, 46 * k, Fade((Color){40, 20, 90, 255}, 0.9f));
    DrawCircleV(c, 34 * k, (Color){10, 6, 26, 255});
    Ring(c, 70 * k, 18 * k, gt * 2, Fade(C_TEAL, k), 3, false);
    Ring(c, 70 * k, 18 * k, gt * 2, Fade(C_TEAL, k), 3, true);
    Ring(c, 58 * k, 14 * k, -gt * 3 + 1, Fade(C_VIOLET, k), 2.5f, true);
    DrawRing(c, 34 * k, 38 * k, 0, 360, 40, Fade(C_VIOLET, k));
}

/* ------------------------------------------------------------------ CPU */
static bool ProjThreat(int me)
{
    Fighter *a = &fz[me];
    for (int i = 0; i < IAF_MAX_PROJ; i++)
        if (proj[i].on && proj[i].owner != me && (a->x - proj[i].x) * proj[i].vx > 0 && fabsf(a->x - proj[i].x) < 340) return true;
    return false;
}
static IafInput CpuInput(int me, float dt)
{
    Fighter *a = &fz[me], *o = &fz[1 - me];
    Ai *s = &ai[me];
    IafInput in = {0};
    float dx = o->x - a->x, dist = fabsf(dx);
    int tw = dx > 0 ? 1 : -1;
    s->t -= dt;
    if (s->shotCd > 0) s->shotCd -= dt;
    bool threat = o->st == ST_ATK && dist < 240 && o->atkT < AtkTotal(o) * 0.6f;
    bool pthreat = ProjThreat(me);
    if (well.on && well.owner != me && !well.caught) threat = true;
    if ((threat || pthreat) && !s->reacted)
    {
        s->reacted = true;
        float r = Rand01();
        if (a->ch == CH_CLAUDE && a->meter >= IAF_SPECIAL_COST && r < 0.4f) { s->once.specialP = true; }
        else if (r < s->skill) { s->mode = AI_BLOCK; s->t = 0.4f; s->low = (o->atk == AT_LOW); }
        else if (pthreat && r < s->skill + 0.25f) { s->once.upP = true; s->mode = AI_FWD; s->t = 0.5f; }
    }
    if (!threat && !pthreat) s->reacted = false;
    if (s->t <= 0)
    {
        float r = Rand01();
        s->low = false;
        s->mode = AI_IDLE;
        s->t = 0.15f + Rand01() * 0.25f;
        bool sp = a->meter >= IAF_SPECIAL_COST;
        if (dist > 420)
        {
            if (a->ch == CH_ELIRA && s->shotCd <= 0 && r < 0.5f) { s->once.shotP = true; s->shotCd = 1.3f; }
            else if (sp && a->ch == CH_ELIRA && r < 0.7f) s->once.specialP = true;
            else if (sp && a->ch == CH_ANTI && r < 0.35f && dist < 560) s->once.specialP = true;
            else if (sp && a->ch == CH_CODEX && r < 0.3f && dist < 560) s->once.specialP = true;
            else { s->mode = AI_FWD; s->t = 0.3f + Rand01() * 0.4f; }
        }
        else if (dist > 165)
        {
            if (r < 0.45f) { s->mode = AI_FWD; s->t = 0.2f + Rand01() * 0.3f; }
            else if (r < 0.6f) { s->mode = AI_JUMPIN; s->once.upP = true; s->t = 0.7f; }
            else if (a->ch == CH_ELIRA && s->shotCd <= 0 && r < 0.8f) { s->once.shotP = true; s->shotCd = 1.3f; }
            else if (sp && a->ch == CH_ANTI && r < 0.85f) s->once.specialP = true;
            else if (sp && a->ch == CH_CODEX && r < 0.85f) s->once.specialP = true;
            else if (r < 0.9f) { s->mode = AI_BACK; s->t = 0.25f; }
        }
        else
        {
            if (r < 0.32f) s->once.lightP = true;
            else if (r < 0.5f) s->once.heavyP = true;
            else if (r < 0.64f) { s->mode = AI_IDLE; s->low = true; s->once.lightP = true; }
            else if (r < 0.78f) { s->mode = AI_BLOCK; s->t = 0.4f; }
            else if (r < 0.88f) { s->mode = AI_BACK; s->t = 0.3f; }
            else if (sp && a->ch != CH_ANTI) s->once.specialP = true;
        }
    }
    switch (s->mode)
    {
    case AI_FWD: if (tw > 0) in.right = true; else in.left = true; break;
    case AI_BACK:
    case AI_BLOCK: if (tw > 0) in.left = true; else in.right = true; in.down = s->low; break;
    case AI_JUMPIN:
        if (tw > 0) in.right = true; else in.left = true;
        if (!OnGround(a) && dist < 160 && a->vy > -200) in.lightP = true;
        if (a->ch == CH_ANTI && !OnGround(a)) in.up = true;
        break;
    default: in.down = s->low && (s->once.lightP); break;
    }
    if (s->once.lightP) { in.lightP = true; }
    if (s->once.heavyP) in.heavyP = true;
    if (s->once.shotP) in.shotP = true;
    if (s->once.specialP) in.specialP = true;
    if (s->once.upP) { in.upP = true; in.up = true; }
    memset(&s->once, 0, sizeof(s->once));
    return in;
}
static IafInput PlayerInput(void)
{
    IafInput in = {0};
    in.left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    in.right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    in.up = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    in.upP = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    in.down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    in.lightP = IsKeyPressed(KEY_J) || IsKeyPressed(KEY_Z);
    in.heavyP = IsKeyPressed(KEY_K) || IsKeyPressed(KEY_X);
    in.specialP = IsKeyPressed(KEY_L) || IsKeyPressed(KEY_C);
    in.shotP = IsKeyPressed(KEY_I) || IsKeyPressed(KEY_V);
    return in;
}

/* ------------------------------------------------------------------ fluxo */
static void BuildLadder(void)
{
    int n = 0;
    if (playerCh == CH_ELIRA) { ladder[0] = CH_CLAUDE; ladder[1] = CH_CODEX; ladder[2] = CH_ANTI; }
    else
    {
        for (int c = CH_CLAUDE; c <= CH_ANTI; c++)
            if (c != playerCh) ladder[n++] = c;
        ladder[2] = CH_ELIRA; /* chefe final */
    }
    ladderPos = 0;
}
static void StartRound(void)
{
    ResetFighter(&fz[0], (CharId)playerCh, 420, 1);
    ResetFighter(&fz[1], (CharId)ladder[ladderPos], 860, -1);
    fz[0].meter = fz[1].meter = 0;
    ClearFx();
    memset(ai, 0, sizeof(ai));
    ai[1].skill = 0.35f + ladderPos * 0.15f;
    ai[0].skill = 0.5f;
    roundTimer = IAF_ROUND_TIME;
    rstate = RS_INTRO; rsT = 0;
    roundWinner = -1;
    fz[0].perfect = fz[1].perfect = 1;
}
static void StartMatch(void)
{
    wins[0] = wins[1] = 0;
    roundNo = 1;
    StartRound();
}
static void PushApart(void)
{
    Fighter *a = &fz[0], *b = &fz[1];
    if (a->st == ST_KO || b->st == ST_KO || a->down || b->down) return;
    if (well.on && well.caught) return;
    float minD = 62;
    if (fabsf(a->y - b->y) < 150 && fabsf(a->x - b->x) < minD)
    {
        float push = (minD - fabsf(a->x - b->x)) / 2;
        int s = a->x < b->x ? -1 : 1;
        if (fabsf(a->x - b->x) < 0.5f) s = a->face > 0 ? -1 : 1;
        a->x += s * push; b->x -= s * push;
        if (a->x < IAF_WALL_L) { b->x += IAF_WALL_L - a->x; a->x = IAF_WALL_L; }
        if (b->x < IAF_WALL_L) { a->x += IAF_WALL_L - b->x; b->x = IAF_WALL_L; }
        if (a->x > IAF_WALL_R) { b->x -= a->x - IAF_WALL_R; a->x = IAF_WALL_R; }
        if (b->x > IAF_WALL_R) { a->x -= b->x - IAF_WALL_R; b->x = IAF_WALL_R; }
    }
}
static void SimStep(float dt, IafInput i0, IafInput i1)
{
    UpdateParts(dt);
    if (hitstop > 0) { hitstop -= dt; return; }
    UpdateFighter(&fz[0], &fz[1], i0, dt);
    UpdateFighter(&fz[1], &fz[0], i1, dt);
    UpdateProjectiles(dt);
    UpdateWell(dt);
    PushApart();
}
static void EndRound(int winner)
{
    roundWinner = winner;
    if (winner >= 0) wins[winner]++;
    rsT = 0;
}
static void UpdateFight(float dt)
{
    if (IsKeyPressed(KEY_P)) paused = !paused;
    if (IsKeyPressed(KEY_ESCAPE)) { iafScreen = SC_SELECT; scT = 0; return; }
    if (paused) return;
    if (shake > 0) shake -= dt;
    rsT += dt;
    IafInput none = {0};
    if (rstate == RS_INTRO)
    {
        SimStep(dt, none, none);
        if (rsT > 2.0f) { rstate = RS_FIGHT; rsT = 0; }
        return;
    }
    if (rstate == RS_FIGHT)
    {
        IafInput i0 = cpu[0] ? CpuInput(0, dt) : PlayerInput();
        IafInput i1 = cpu[1] ? CpuInput(1, dt) : PlayerInput();
        SimStep(dt, i0, i1);
        roundTimer -= dt;
        bool k0 = fz[0].st == ST_KO, k1 = fz[1].st == ST_KO;
        if (k0 || k1) { rstate = RS_KO; EndRound(k0 && k1 ? -1 : (k1 ? 0 : 1)); }
        else if (roundTimer <= 0)
        {
            roundTimer = 0;
            rstate = RS_TIMEUP;
            float a = fz[0].hp, b = fz[1].hp;
            EndRound(fabsf(a - b) < 1 ? -1 : (a > b ? 0 : 1));
        }
        return;
    }
    /* fim do round: camera lenta no inicio */
    float slow = rsT < 0.9f ? 0.35f : 1.0f;
    SimStep(dt * slow, none, none);
    if (rsT > 1.4f && roundWinner >= 0)
    {
        Fighter *w = &fz[roundWinner];
        if (w->st != ST_WIN && OnGround(w) && w->st != ST_KO) { SetState(w, ST_WIN); w->crouch = false; w->guard = false; w->shieldOn = false; }
        Fighter *l = &fz[1 - roundWinner];
        if (l->st != ST_LOSE && l->st != ST_KO && OnGround(l) && !l->down) { SetState(l, ST_LOSE); l->crouch = false; l->guard = false; l->shieldOn = false; }
    }
    if (rsT > 3.4f)
    {
        if (wins[0] >= 2 || wins[1] >= 2)
        {
            lastWon = wins[0] >= 2;
            iafScreen = (lastWon && ladderPos == 2) ? SC_FINAL : SC_RESULT;
            scT = 0;
        }
        else { roundNo++; StartRound(); }
    }
}

/* ------------------------------------------------------------------ HUD */
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
static void DrawHUD(void)
{
    for (int i = 0; i < 2; i++)
    {
        Fighter *a = &fz[i];
        bool m = i == 1;
        float fill = a->hp / IAF_HP_MAX, trail = a->hpShown / IAF_HP_MAX;
        Color col = fill < 0.25f ? ColorLerp(C_RED, WHITE, 0.15f * (0.5f + 0.5f * sinf(gt * 10))) : ColorLerp(CHCOL[a->ch], WHITE, 0.15f);
        Bar(70, 30, 470, 26, fill, trail, col, m);
        /* nome e "chip" de cor */
        float nx = m ? SCREEN_WIDTH - 70 : 70;
        DrawRectangle((int)(m ? nx - 14 : nx), 64, 14, 14, CHCOL[a->ch]);
        DrawRectangleLines((int)(m ? nx - 14 : nx), 64, 14, 14, C_INK);
        const char *nm = NAME[a->ch];
        if (m) TxtR(nm, nx - 22, 61, 22, C_WHITE); else Txt(nm, nx + 22, 61, 22, C_WHITE);
        const char *tag = cpu[i] ? "CPU" : "JOGADOR";
        float tw = TxtW(nm, 22) + 32;
        if (m) TxtR(tag, nx - tw, 66, 14, C_DIM); else Txt(tag, nx + tw, 66, 14, C_DIM);
        /* rounds vencidos: chips de memoria */
        for (int r = 0; r < 2; r++)
        {
            float cx = m ? SCREEN_WIDTH / 2.0f + 76 + r * 26 : SCREEN_WIDTH / 2.0f - 76 - r * 26;
            Rectangle rc = {cx - 9, 66, 18, 12};
            DrawRectangleRec(rc, wins[i] > r ? CHCOL[a->ch] : (Color){30, 34, 56, 255});
            DrawRectangleLinesEx(rc, 1.5f, C_INK);
            for (int p = 0; p < 3; p++) { DrawRectangle((int)rc.x + 2 + p * 6, 78, 2, 4, C_DIM); DrawRectangle((int)rc.x + 2 + p * 6, 62, 2, 4, C_DIM); }
        }
        /* carga do especial */
        float mx = m ? SCREEN_WIDTH - 70 - 300 : 70, my = SCREEN_HEIGHT - 40;
        Txt("CARGA", m ? mx + 300 - TxtW("CARGA", 14) : mx, my - 20, 14, C_DIM);
        for (int sgm = 0; sgm < 2; sgm++)
        {
            float seg = Clamp((a->meter - sgm * 50) / 50, 0.0f, 1.0f);
            int si = m ? 1 - sgm : sgm;
            Rectangle r = {mx + si * 152, my, 148, 14};
            DrawRectangleRec(r, (Color){20, 24, 44, 255});
            Rectangle f = r;
            f.width *= seg;
            if (m) f.x = r.x + r.width - f.width;
            DrawRectangleRec(f, seg >= 1 ? ColorLerp(CHCOL[a->ch], WHITE, 0.3f + 0.3f * sinf(gt * 8)) : Fade(CHCOL[a->ch], 0.7f));
            DrawRectangleLinesEx(r, 2, C_INK);
        }
        if (a->meter >= IAF_SPECIAL_COST)
        {
            const char *s = "ESPECIAL PRONTO";
            if (m) TxtR(s, SCREEN_WIDTH - 70 - 60, my - 20, 14, CHCOL[a->ch]); else Txt(s, 70 + 60, my - 20, 14, CHCOL[a->ch]);
        }
    }
    /* cronometro em hexagono */
    Vector2 c = (Vector2){SCREEN_WIDTH / 2.0f, 50};
    DrawPoly(c, 6, 44, 0, (Color){4, 6, 14, 255});
    DrawPoly(c, 6, 40, 0, (Color){18, 22, 42, 255});
    DrawPolyLinesEx(c, 6, 40, 0, 2, Fade(C_CYAN, 0.7f));
    char b[8];
    snprintf(b, sizeof b, "%02d", (int)ceilf(roundTimer));
    TxtC(b, c.x, c.y - 17, 34, roundTimer < 10 ? C_RED : C_WHITE);
}
static void DrawFightOverlay(void)
{
    if (rstate == RS_INTRO)
    {
        char b[32];
        bool last = (wins[0] == 1 && wins[1] == 1);
        if (last) snprintf(b, sizeof b, "ROUND FINAL");
        else snprintf(b, sizeof b, "ROUND %d", roundNo);
        if (rsT < 1.2f)
        {
            float k = EaseOut(rsT / 0.25f);
            DrawRectangle(0, 300, SCREEN_WIDTH, (int)(90 * k), Fade(C_INK, 0.6f));
            TxtO(b, SCREEN_WIDTH / 2.0f + (1 - k) * 300, 312, 64, C_WHITE, C_INK);
        }
        else
        {
            float k = EaseOut((rsT - 1.2f) / 0.2f);
            TxtO("EXECUTAR!", SCREEN_WIDTH / 2.0f, 300, (int)(60 + 20 * k), C_CYAN, C_INK);
        }
    }
    if (rstate == RS_KO || rstate == RS_TIMEUP)
    {
        const char *s = rstate == RS_KO ? "DESCONECTADO!" : "TEMPO ESGOTADO!";
        if (rsT < 1.8f) TxtO(s, SCREEN_WIDTH / 2.0f, 290, 72, rstate == RS_KO ? C_RED : C_AMBER, C_INK);
        else
        {
            if (roundWinner < 0) TxtO("EMPATE", SCREEN_WIDTH / 2.0f, 300, 56, C_WHITE, C_INK);
            else
            {
                char b[64];
                snprintf(b, sizeof b, "%s VENCE O ROUND", NAME[fz[roundWinner].ch]);
                TxtO(b, SCREEN_WIDTH / 2.0f, 300, 48, CHCOL[fz[roundWinner].ch], C_INK);
                if (fz[roundWinner].hp >= IAF_HP_MAX - 0.5f) TxtO("SEM UM ARRANHÃO!", SCREEN_WIDTH / 2.0f, 360, 28, C_AMBER, C_INK);
            }
        }
    }
    if (paused)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(C_INK, 0.6f));
        TxtO("PAUSA", SCREEN_WIDTH / 2.0f, 300, 64, C_WHITE, C_INK);
        TxtC("P: continuar   ESC: voltar à seleção", SCREEN_WIDTH / 2.0f, 380, 20, C_DIM);
    }
}
static void DrawFight(void)
{
    Camera2D cam = {0};
    cam.zoom = 1;
    if (shake > 0) cam.offset = (Vector2){(Rand01() - 0.5f) * shake * 24, (Rand01() - 0.5f) * shake * 24};
    BeginMode2D(cam);
    DrawStage();
    DrawWell();
    /* quem ataca fica na frente */
    int first = (fz[0].st == ST_ATK && fz[1].st != ST_ATK) ? 1 : 0;
    if (fz[0].shieldOn || fz[0].st == ST_BLOCK) first = 1; /* quem defende fica na frente: o escudo aparece */
    else if (fz[1].shieldOn || fz[1].st == ST_BLOCK) first = 0;
    DrawFighter(&fz[first]);
    DrawFighter(&fz[1 - first]);
    DrawProjectiles();
    DrawParts();
    EndMode2D();
    DrawHUD();
    DrawFightOverlay();
}

/* ------------------------------------------------------------------ retratos */
static void DrawCharacterBig(CharId ch, float x, float yFeet, int face, float sc, FState st)
{
    if (ch == CH_ELIRA)
    {
        int f = EAFrame(EA_IDLE, 0);
        float yo = roundf((sinf(gt * 2.4f) - 1) * 1.0f) * sc;
        if (st == ST_WIN) { f = EAFrame(EA_WIN, 2 + ((int)(gt * 2.5f) % 2)); yo = 0; }
        if (st == ST_KO) { f = EAFrame(EA_DEFEAT, 1); yo = 0; }
        DrawEF(f, x, yFeet + yo, face, sc, WHITE, 0);
        return;
    }
    Fighter tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.ch = ch; tmp.st = st; tmp.face = face; tmp.x = x;
    Pose p = PoseFor(&tmp);
    DrawPuppet(ch, x, yFeet, face, sc, p, 0, 0);
}
static void IafEliraPortrait(Rectangle dst, int face)
{
    /* retrato = parte de cima da guarda oficial */
    const EFrame *e = &EF[EAFrame(EA_IDLE, 0)];
    Rectangle src = {e->x, e->y, e->w, e->h * 0.6f};
    float k = fminf(dst.width / src.width, dst.height / src.height);
    float w = src.width * k, h = src.height * k;
    if (face < 0) src.width = -src.width;
    DrawRectangleRec(dst, (Color){18, 24, 46, 255});
    DrawTexturePro(texElira, src, (Rectangle){dst.x + (dst.width - w) / 2, dst.y + dst.height - h, w, h}, (Vector2){0, 0}, 0, WHITE);
}

/* ------------------------------------------------------------------ telas */
static void DrawBackdrop(Color accent)
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){8, 10, 24, 255}, (Color){16, 18, 40, 255});
    for (int i = 0; i < 24; i++)
    {
        float x = fmodf(i * 97.0f + gt * 20, SCREEN_WIDTH + 200) - 100;
        DrawLineEx((Vector2){x, 0}, (Vector2){x - 260, SCREEN_HEIGHT}, 1, Fade(accent, 0.06f));
    }
    for (int j = 0; j < 12; j++) DrawLine(0, j * 64, SCREEN_WIDTH, j * 64, Fade(C_CYAN, 0.03f));
}
static Rectangle CardRect(int i)
{
    if (i == 0) return (Rectangle){60, 130, 360, 430};
    return (Rectangle){450 + (i - 1) * 262, 160, 246, 400};
}
static void DrawSelect(void)
{
    Color acc = CHCOL[sel];
    DrawBackdrop(acc);
    TxtO("IA FIGHT", SCREEN_WIDTH / 2.0f, 22, 64, C_WHITE, C_INK);
    TxtC("RINHA DE INTELIGÊNCIAS  -  ESCOLHA SEU LUTADOR", SCREEN_WIDTH / 2.0f, 92, 20, C_CYAN);
    for (int i = 0; i < CH_N; i++)
    {
        Rectangle r = CardRect(i);
        bool on = i == sel;
        Color c = CHCOL[i];
        if (on) { r.y -= 8; DrawRectangleRounded((Rectangle){r.x - 8, r.y - 8, r.width + 16, r.height + 16}, 0.06f, 6, Fade(c, 0.25f + 0.1f * sinf(gt * 4))); }
        DrawRectangleRounded(r, 0.05f, 6, (Color){14, 18, 36, 255});
        DrawRectangleGradientV((int)r.x + 4, (int)(r.y + r.height * 0.45f), (int)r.width - 8, (int)(r.height * 0.55f) - 4, Fade(c, 0), Fade(c, on ? 0.30f : 0.12f));
        DrawRectangleRoundedLinesEx(r, 0.05f, 6, on ? 3 : 1.5f, on ? c : Fade(c, 0.45f));
        /* piso do card */
        float fy = r.y + r.height - 70;
        DrawEllipse((int)(r.x + r.width / 2), (int)fy + 2, (int)(r.width * 0.32f), 10, Fade(BLACK, 0.5f));
        DrawEllipseLines((int)(r.x + r.width / 2), (int)fy + 2, (int)(r.width * 0.34f), 11, Fade(c, 0.6f));
        if (i == 0)
        {
            IafEliraPortrait((Rectangle){r.x + r.width - 140, r.y + 16, 124, 128}, 1);
            DrawRectangleLinesEx((Rectangle){r.x + r.width - 140, r.y + 16, 124, 128}, 2, Fade(c, 0.8f));
            DrawCharacterBig(CH_ELIRA, r.x + r.width * 0.36f, fy, 1, 1.25f, ST_IDLE);
            DrawRectangleRounded((Rectangle){r.x + 14, r.y + 16, 150, 28}, 0.3f, 4, c);
            TxtC("PROTAGONISTA", r.x + 89, r.y + 22, 16, C_INK);
        }
        else DrawCharacterBig((CharId)i, r.x + r.width / 2, fy, 1, 1.0f, ST_IDLE);
        TxtC(NAME[i], r.x + r.width / 2, r.y + r.height - 54, i == 0 ? 36 : 26, on ? C_WHITE : C_DIM);
        TxtC(i == 0 ? "a viajante do tempo" : "inteligência artificial", r.x + r.width / 2, r.y + r.height - 18 - (i == 0 ? 0 : 4), 14, Fade(c, 0.9f));
    }
    /* painel de informacoes */
    Rectangle ip = {60, 586, SCREEN_WIDTH - 120, 112};
    DrawRectangleRounded(ip, 0.12f, 6, (Color){12, 14, 30, 255});
    DrawRectangleRoundedLinesEx(ip, 0.12f, 6, 2, Fade(acc, 0.7f));
    Txt(NAME[sel], ip.x + 20, ip.y + 12, 28, acc);
    Txt(STYLE[sel], ip.x + 20 + TxtW(NAME[sel], 28) + 16, ip.y + 20, 16, C_DIM);
    Txt("ESPECIAL:", ip.x + 20, ip.y + 50, 16, C_WHITE);
    Txt(SPNAME[sel], ip.x + 120, ip.y + 50, 16, acc);
    Txt(SPDESC[sel], ip.x + 20, ip.y + 74, 15, C_DIM);
    static const char *SN[4] = {"FORÇA", "VELOCIDADE", "DEFESA", "ALCANCE"};
    for (int s = 0; s < 4; s++)
    {
        float sx = ip.x + ip.width - 330, sy = ip.y + 14 + s * 23;
        Txt(SN[s], sx, sy, 14, C_DIM);
        for (int k = 0; k < 5; k++)
            DrawRectangle((int)(sx + 120 + k * 38), (int)sy + 2, 32, 11, k < STAT[sel][s] ? acc : (Color){30, 34, 56, 255});
    }
    char lad[96];
    if (sel == CH_ELIRA) snprintf(lad, sizeof lad, "ARCADE: CLAUDE > CODEX > ANTIGRAVITY");
    else
    {
        int l[3], n = 0;
        for (int c = CH_CLAUDE; c <= CH_ANTI; c++) if (c != sel) l[n++] = c;
        snprintf(lad, sizeof lad, "ARCADE: %s > %s > ELIRA (CHEFE FINAL)", NAME[l[0]], NAME[l[1]]);
    }
    TxtC(lad, SCREEN_WIDTH / 2.0f, 112, 16, Fade(C_WHITE, 0.8f));
    TxtC("A/D ou SETAS: escolher    ENTER: confirmar    ESC: voltar ao menu", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 18, 14, C_DIM);
}
static void UpdateSelect(void)
{
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) sel = (sel + CH_N - 1) % CH_N;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) sel = (sel + 1) % CH_N;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_J))
    {
        playerCh = sel;
        BuildLadder();
        iafScreen = SC_VERSUS; scT = 0;
    }
}
static void DrawVersus(void)
{
    CharId a = (CharId)playerCh, b = (CharId)ladder[ladderPos];
    DrawRectangleGradientH(0, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT, Fade(CHCOL[a], 0.35f), (Color){10, 12, 26, 255});
    DrawRectangleGradientH(SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT, (Color){10, 12, 26, 255}, Fade(CHCOL[b], 0.35f));
    Tri((Vector2){SCREEN_WIDTH / 2.0f - 60, 0}, (Vector2){SCREEN_WIDTH / 2.0f + 20, 0}, (Vector2){SCREEN_WIDTH / 2.0f - 20, SCREEN_HEIGHT}, (Color){6, 8, 16, 255});
    Tri((Vector2){SCREEN_WIDTH / 2.0f + 20, 0}, (Vector2){SCREEN_WIDTH / 2.0f + 60, SCREEN_HEIGHT}, (Vector2){SCREEN_WIDTH / 2.0f - 20, SCREEN_HEIGHT}, (Color){6, 8, 16, 255});
    float k = EaseOut(scT / 0.4f);
    DrawCharacterBig(a, 300 - (1 - k) * 300, 560, 1, 1.6f, ST_IDLE);
    DrawCharacterBig(b, SCREEN_WIDTH - 300 + (1 - k) * 300, 560, -1, 1.6f, ST_IDLE);
    TxtO(NAME[a], 300, 590, 44, CHCOL[a], C_INK);
    TxtO(NAME[b], SCREEN_WIDTH - 300, 590, 44, CHCOL[b], C_INK);
    TxtO("VS", SCREEN_WIDTH / 2.0f, 300, 96, C_WHITE, C_INK);
    char d[48];
    if (ladderPos == 2 && b == CH_ELIRA) snprintf(d, sizeof d, "DESAFIO FINAL - A CHEFE");
    else snprintf(d, sizeof d, "DESAFIO %d DE 3", ladderPos + 1);
    TxtO(d, SCREEN_WIDTH / 2.0f, 40, 32, C_AMBER, C_INK);
    TxtC("ENTER: lutar", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 30, 16, C_DIM);
}
static void IafDrawResult(bool final)
{
    CharId me = (CharId)playerCh, op = (CharId)ladder[ladderPos];
    CharId w = lastWon ? me : op, l = lastWon ? op : me;
    DrawBackdrop(CHCOL[w]);
    /* palco do vencedor */
    DrawEllipse(SCREEN_WIDTH / 2 - 180, 560, 170, 26, Fade(CHCOL[w], 0.25f));
    DrawEllipseLines(SCREEN_WIDTH / 2 - 180, 560, 175, 28, CHCOL[w]);
    DrawCharacterBig(w, SCREEN_WIDTH / 2.0f - 180, 560, 1, 1.5f, ST_WIN);
    DrawCharacterBig(l, SCREEN_WIDTH / 2.0f + 260, 600, -1, 0.9f, ST_KO);
    const char *title = final ? "RINHA CONQUISTADA!" : (lastWon ? "VITÓRIA!" : "DERROTA");
    TxtO(title, SCREEN_WIDTH / 2.0f, 40, 80, lastWon ? CHCOL[me] : C_RED, C_INK);
    char b[96];
    snprintf(b, sizeof b, "%s VENCE A LUTA  (%d x %d)", NAME[w], lastWon ? wins[0] : wins[1], lastWon ? wins[1] : wins[0]);
    TxtC(b, SCREEN_WIDTH / 2.0f, 132, 24, C_WHITE);
    Rectangle q = {SCREEN_WIDTH / 2.0f + 20, 200, 560, 120};
    DrawRectangleRounded(q, 0.15f, 6, (Color){12, 14, 30, 230});
    DrawRectangleRoundedLinesEx(q, 0.15f, 6, 2, CHCOL[w]);
    Txt(NAME[w], q.x + 20, q.y + 14, 22, CHCOL[w]);
    Txt(WINQ[w], q.x + 20, q.y + 52, 17, C_WHITE);
    if (final) TxtC("As três rivais foram derrotadas. A arena é sua.", SCREEN_WIDTH / 2.0f, 650, 20, C_AMBER);
    if (final) TxtC("ENTER: voltar à seleção", SCREEN_WIDTH / 2.0f, 684, 16, C_DIM);
    else if (lastWon)
    {
        snprintf(b, sizeof b, "ENTER: próximo desafio (%d de 3)    ESC: seleção", ladderPos + 2);
        TxtC(b, SCREEN_WIDTH / 2.0f, 684, 16, C_DIM);
    }
    else TxtC("ENTER: tentar de novo    ESC: seleção", SCREEN_WIDTH / 2.0f, 684, 16, C_DIM);
}

/* ------------------------------------------------------------------ interface com o jogo (iafight.h) */
static bool iafLoaded = false;

void IafEnter(GameState *game)
{
    if (!iafLoaded)
    {
        texElira = LoadTexture(IAF_ELIRA_ATLAS);
        SetTextureFilter(texElira, TEXTURE_FILTER_BILINEAR);
        iafLoaded = true;
    }
    cpu[0] = false;
    cpu[1] = true;
    paused = false;
    iafScreen = SC_SELECT;
    scT = 0;
    game->screen = SCREEN_IAFIGHT;
}

void IafUpdate(GameState *game, float dt)
{
    if (dt > 1 / 30.0f) dt = 1 / 30.0f;
    gt += dt;
    scT += dt;
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); /* Alt+Enter e tela cheia */
    switch (iafScreen)
    {
    case SC_SELECT:
        if (IsKeyPressed(KEY_ESCAPE)) { game->screen = SCREEN_TITLE; return; }
        if (!alt) UpdateSelect();
        break;
    case SC_VERSUS:
        if ((IsKeyPressed(KEY_ENTER) && !alt) || scT > 2.6f) { StartMatch(); iafScreen = SC_FIGHT; scT = 0; }
        if (IsKeyPressed(KEY_ESCAPE)) iafScreen = SC_SELECT;
        break;
    case SC_FIGHT: UpdateFight(dt); break;
    case SC_RESULT:
        if (scT > 0.6f && IsKeyPressed(KEY_ENTER) && !alt)
        {
            if (lastWon) ladderPos++;
            iafScreen = SC_VERSUS;
            scT = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) { iafScreen = SC_SELECT; scT = 0; }
        break;
    case SC_FINAL:
        if (scT > 0.6f && ((IsKeyPressed(KEY_ENTER) && !alt) || IsKeyPressed(KEY_ESCAPE))) { iafScreen = SC_SELECT; scT = 0; }
        break;
    }
}

void IafDraw(void)
{
    ClearBackground(C_BG);
    switch (iafScreen)
    {
    case SC_SELECT: DrawSelect(); break;
    case SC_VERSUS: DrawVersus(); break;
    case SC_FIGHT: DrawFight(); break;
    case SC_RESULT: IafDrawResult(false); break;
    case SC_FINAL: IafDrawResult(true); break;
    }
}

void IafUnload(void)
{
    if (iafLoaded) UnloadTexture(texElira);
    iafLoaded = false;
}
