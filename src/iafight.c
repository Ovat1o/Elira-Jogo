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
#define C_GRAB ((Color){236, 120, 200, 255}) /* agarrao na tela de controles */
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
   Gerado fora do repositorio a partir do pacote de sprites da Elira (grade logica 144x144, pixel art).
   O atlas guarda os pixels da arte; o jogo amplia com IAF_ELIRA_PIX em multiplos de 0,5 e filtro
   POINT. Escudo, rasteira e dano agachada ainda vem das fichas antigas (igualadas pela cabeca). */
#include "iafight_elira.h"
static Texture2D texElira;
static int EAFrame(int anim, int i)
{
    int n = EA_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return EA_FIRST[anim] + i;
}
/* Elira: sprites NOVOS (modo de teste). Atlas elira_luta_v2.png + tabela EF2 (iafight_elira_v2.h), na mesma
   escala logica. Os quadros novos tem indice EF2_BASE + n; o que nao tem arte nova segue no atlas atual.
   No treino, F4 alterna; IAF_ELIRA_SPRITES_NOVOS (constantes.h) liga em todos os modos. */
#include "iafight_elira_v2.h"
static Texture2D texElira2;
static bool eliraNovos = IAF_ELIRA_SPRITES_NOVOS;
#define EF2_BASE 1000
static bool EliraV2(void) { return eliraNovos && texElira2.id > 0; } /* sem o atlas novo, fica a atual */
static int E2Frame(int anim, int i)
{
    int n = E2_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return EF2_BASE + E2_FIRST[anim] + i;
}
static const EFrame *EFGet(int f, Texture2D *tex)
{
    if (f >= EF2_BASE) { *tex = texElira2; return &EF2[f - EF2_BASE]; }
    *tex = texElira;
    return &EF[f];
}
/* desenha o quadro f com os pes em (x, yFeet); espelha quando face < 0 */
static float EliraScale(float sc) { return fmaxf(0.5f, roundf(sc * IAF_ELIRA_PIX * 2) / 2); } /* pixel inteiro */
static void DrawEF(int f, float x, float yFeet, int face, float sc, Color tint, float flash)
{
    Texture2D tex;
    const EFrame *e = EFGet(f, &tex);
    sc = EliraScale(sc);
    Rectangle src = {e->x, e->y, e->w, e->h};
    float w = e->w * sc, h = e->h * sc;
    float left = face > 0 ? x - e->ax * sc : x - (e->w - e->ax) * sc;
    if (face < 0) src.width = -src.width;
    Rectangle dst = {roundf(left), roundf(yFeet - e->ay * sc), w, h};
    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0, tint);
    if (flash > 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0, Fade(WHITE, flash));
        EndBlendMode();
    }
}
/* efeitos (projetil/impacto): ancorados no centro */
static void DrawEFC(int f, Vector2 c, int face, float sc, Color tint)
{
    Texture2D tex;
    const EFrame *e = EFGet(f, &tex);
    sc = EliraScale(sc);
    Rectangle src = {e->x, e->y, e->w, e->h};
    float ax = face > 0 ? e->ax : e->w - e->ax;
    if (face < 0) src.width = -src.width;
    DrawTexturePro(tex, src, (Rectangle){c.x - ax * sc, c.y - e->ay * sc, e->w * sc, e->h * sc}, (Vector2){0, 0}, 0, tint);
}

/* ------------------------------------------------------------------ personagens */
typedef enum { CH_ELIRA, CH_CLAUDE, CH_CODEX, CH_ANTI, CH_IARA, CH_N } CharId;
#define CH_MAIN 4 /* os 4 lutadores da grade; a Iara (convidada de colab, jogo parceiro) fica num cartao a parte */
static const char *NAME[CH_N] = {"ELIRA", "CLAUDE", "CODEX", "ANTIGRAVITY", "IARA"};
static const char *SPNAME[CH_N] = {"RAJADA TEMPORAL", "PRISMA REFLETOR", "DEPLOY FORÇADO", "POÇO GRAVITACIONAL", "CANTO DO RIO"};
static const char *SPDESC[CH_N] = {"Carrega o canhão e dispara um núcleo de plasma que atravessa e acerta 3 vezes.",
                                   "Escudo de raciocínio: reflete projéteis e responde golpes com um feixe de luz.",
                                   "Avança compilando: prende o oponente numa sequência de 4 golpes e um final.",
                                   "Abre um poço que puxa e ergue o oponente, depois o derruba no chão.",
                                   "Canta, vira a forma monstruosa e solta uma onda que avança e acerta 3 vezes."};
static const char *WINQ[CH_N] = {"\"A história não se reescreve sozinha. Eu reescrevo.\"",
                                 "\"Considerei todas as hipóteses. Esta era a melhor.\"",
                                 "\"Build concluída. Zero erros, zero avisos.\"",
                                 "\"Aqui em cima, quem decide a queda sou eu.\"",
                                 "\"O rio sempre leva de volta o que é dele.\""};
static const Color CHCOL[CH_N] = {{96, 214, 240, 255}, {222, 116, 76, 255}, {70, 255, 150, 255}, {150, 120, 255, 255}, {64, 178, 168, 255}};

typedef enum { ST_IDLE, ST_WALK, ST_JUMP, ST_CROUCH, ST_BLOCK, ST_ATK, ST_HIT, ST_KO, ST_WIN, ST_LOSE, ST_DASH } FState;
/* AT_FWDH = frente + forte (soco longo, mais lento); AT_UPPER = cima + forte (uppercut anti-aereo) */
typedef enum { AT_NONE, AT_LIGHT, AT_HEAVY, AT_LOW, AT_AIR, AT_FWDH, AT_UPPER, AT_SPECIAL, AT_SHOT, AT_GRAB } AtkKind; /* AT_GRAB = agarrao */
typedef struct { float st, ac, rc, dmg, rx, top, h, stun, push; int type; } Atk; /* type: 0 meio, 1 baixo, 2 alto */
/* por lutador: -, fraco, forte, rasteira, aereo, soco longo (frente + forte), uppercut (cima + forte).
   O uppercut tem caixa alta (top grande): pega quem esta pulando por cima.
   Atordoamento do fraco (0,20 s) menor que preparo + recuperacao dele: jab atras de jab sem o cancelamento
   deixa o oponente livre antes do proximo acerto (sem loop). O combo pela janela segura o atordoamento (ChainStun).
   Dano do fraco (34 a 39) cerca de 25% abaixo do original: apertar so o fraco nao rende tanto quanto o forte. */
#define IRK(v) ((v) * IAF_IARA_ESCALA) /* medida da Iara em px da escala 1 -> px da luta */
static const Atk ATK[CH_N][AT_SPECIAL] = {
    [CH_ELIRA] = {{0}, {0.06f, 0.08f, 0.15f, 34, 112, 168, 46, 0.20f, 170, 0},
                  {0.14f, 0.12f, 0.26f, 95, 112, 200, 95, 0.45f, 300, 0}, /* chute alto (forte); alcances na ponta da arte */
                  {0.08f, 0.09f, 0.20f, 42, 150, 46, 44, 0.30f, 150, 1},
                  {0.05f, 0.16f, 0.10f, 55, 105, 125, 76, 0.32f, 160, 2}, /* aereo: pe do chute entre 49 e 125 px acima dos pes */
                  {0.20f, 0.10f, 0.30f, 90, 128, 168, 50, 0.45f, 330, 0},  /* soco longo */
                  {0.08f, 0.14f, 0.32f, 80, 90, 300, 270, 0.50f, 120, 0}}, /* uppercut (chute alto) */
    [CH_CLAUDE] = {{0}, {0.07f, 0.08f, 0.15f, 39, 180, 122, 50, 0.20f, 170, 0}, /* alcances no punho/pe do sprite (1.5x) */
                   {0.17f, 0.10f, 0.30f, 106, 188, 112, 54, 0.45f, 380, 0},   /* chute frontal */
                   {0.10f, 0.08f, 0.22f, 50, 171, 33, 33, 0.32f, 160, 1},
                   {0.05f, 0.15f, 0.10f, 55, 186, 60, 40, 0.32f, 160, 2},
                   {0.24f, 0.10f, 0.34f, 108, 196, 128, 54, 0.50f, 420, 0},  /* soco forte de alcance longo */
                   {0.09f, 0.14f, 0.34f, 85, 158, 240, 218, 0.50f, 120, 0}}, /* uppercut */
    [CH_CODEX] = {{0}, {0.06f, 0.07f, 0.14f, 39, 116, 122, 50, 0.20f, 180, 0},  /* alcances na luva/tenis do sprite (2x) */
                  {0.20f, 0.10f, 0.32f, 120, 122, 118, 56, 0.48f, 330, 0},  /* chute alto */
                  {0.09f, 0.08f, 0.22f, 55, 120, 100, 64, 0.32f, 170, 1},   /* chute baixo */
                  {0.06f, 0.15f, 0.10f, 60, 120, 120, 74, 0.34f, 170, 2},   /* aereo: chute alto */
                  {0.22f, 0.10f, 0.32f, 105, 126, 120, 46, 0.48f, 330, 0},  /* soco longo */
                  {0.09f, 0.13f, 0.34f, 80, 110, 300, 270, 0.50f, 120, 0}}, /* uppercut */
    [CH_ANTI] = {{0}, {0.07f, 0.08f, 0.14f, 36, 158, 175, 80, 0.20f, 160, 0}, /* alcances na ponta do golpe do sprite (2,5x) */
                 {0.16f, 0.10f, 0.28f, 92, 160, 200, 110, 0.44f, 300, 0},  /* chute */
                 {0.09f, 0.09f, 0.20f, 45, 152, 60, 56, 0.30f, 150, 1},     /* low kick */
                 {0.05f, 0.18f, 0.10f, 26, 140, 110, 140, 0.24f, 160, 2},   /* ground slam: de cima para baixo */
                 {0.20f, 0.10f, 0.30f, 65, 160, 190, 90, 0.44f, 320, 0},    /* heavy punch */
                 {0.08f, 0.14f, 0.32f, 32, 150, 320, 300, 0.50f, 120, 0}},  /* uppercut (heavy punch subindo) */
    /* Iara: alcance (rx), topo e altura da caixa em px da escala 1 (guarda de 210 px), vezes IAF_IARA_ESCALA */
    [CH_IARA] = {{0}, {0.07f, 0.08f, 0.15f, 36, IRK(103), IRK(162), IRK(49), 0.20f, 170, 0},  /* garra rapida: alcance nos riscos da garra */
                 {0.15f, 0.11f, 0.28f, 96, IRK(109), IRK(200), IRK(105), 0.45f, 320, 0},  /* golpe de cauda forte: arco alto */
                 {0.09f, 0.09f, 0.21f, 44, IRK(150), IRK(46), IRK(44), 0.30f, 150, 1},    /* chicote de agua rente ao chao (agachada) */
                 {0.05f, 0.16f, 0.10f, 55, IRK(102), IRK(88), IRK(88), 0.32f, 160, 2},  /* aereo: cauda chicoteando para baixo */
                 {0.21f, 0.10f, 0.30f, 88, IRK(150), IRK(175), IRK(70), 0.45f, 330, 0},   /* golpe longo: garra e mechas lancadas */
                 {0.08f, 0.14f, 0.32f, 80, IRK(87), IRK(277), IRK(250), 0.50f, 120, 0}},  /* uppercut: sobe num jato de agua */
};
/* especiais: preparo / janela ativa / recuperacao */
static const float SP_ST[CH_N] = {0.42f, 0.08f, 0.14f, 0.22f, 0.50f}; /* Iara: canta e se transforma antes da onda */
static const float SP_AC[CH_N] = {0.05f, 0.40f, 0.40f, 0.05f, 0.05f};
static const float SP_RC[CH_N] = {0.40f, 0.50f, 0.34f, 0.45f, 0.40f};

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
    bool alt;         /* cor alternativa (1 x 1 com o mesmo lutador dos dois lados) */
    int juggle;       /* golpes levados no ar nesta queda (limite evita malabarismo infinito) */
    float tapT;       /* > 0: tocou para frente ha pouco (o 2o toque dentro da janela faz o dash) */
    float dashT;      /* tempo desde o inicio do dash */
    int chain;        /* combo fraco > fraco > forte: 1 = 1o fraco, 2 = 2o fraco, 3 = forte final (0 = fora do combo) */
    float cancelT;    /* > 0: o golpe acertou e pode ser cancelado no proximo do combo */
    bool grabbed;     /* preso num agarrao do oponente */
    float groundT;    /* tempo no chao desde o ultimo pouso (0 no ar): quadros da queda */
} Fighter;

/* leftP/rightP: tocou na direcao agora (toque duplo = dash); dashP: botao de dash (LB / L1);
   grabP: agarrao (LT / L2, ou fraco + forte juntos, detectado em BufferInput) */
typedef struct { bool left, right, up, down, upP, lightP, heavyP, specialP, shotP, leftP, rightP, dashP, grabP; } IafInput;

/* projeteis */
typedef struct { bool on; float x, y, vx; int owner; bool big, reflected; float dmg, t, hitCd; int hits; bool wave; } Proj; /* wave: onda da Iara */
static Proj proj[IAF_MAX_PROJ];
/* poco gravitacional */
typedef struct { bool on; float x, y, t, life; int owner; bool caught, resisted; } Well;
static Well well;

/* particulas e textos */
typedef struct { Vector2 p, v; float life, max, size; Color c; int kind; char txt[32]; } Part;
static Part parts[IAF_MAX_PARTS];
typedef struct { bool on; Vector2 p; float t; float sc; int face; int kind; } Impact; /* kind 1: impacto da onda da Iara */
static Impact impacts[16];

static Fighter fz[2];
static float hitstop = 0, shake = 0;

/* ------------------------------------------------------------------ som (assets/som/iafight)
   Volume segue o do jogo: no padrao (g_musicVolume = 0,45) a musica fica em IAF_MUSIC_VOLUME e os
   efeitos em IAF_SFX_VOLUME; o mudo do jogo (tecla M) silencia tudo. Sem dispositivo de audio ou sem
   os arquivos, o modo continua funcionando em silencio. */
enum { SFX_LIGHT, SFX_HEAVY, SFX_BLOCK, SFX_SHOT, SFX_SP_CHARGE, SFX_SP_FIRE, SFX_KO, SFX_FALL, SFX_MOVE, SFX_OK, SFX_DASH, SFX_DENY, VOX_ROUND, VOX_LUTEM, VOX_WIN, SFX_N };
static const char *SFX_FILE[SFX_N] = {"golpe_fraco.wav", "golpe_forte.wav", "defesa.wav", "tiro.wav", "especial_carga.wav",
                                      "especial_disparo.wav", "impacto_ko.wav", "queda.wav", "menu_mover.wav", "menu_confirmar.wav",
                                      "menu_mover.wav" /* dash: o bip do menu bem mais grave vira um whoosh */,
                                      "defesa.wav" /* negado (arena travada): o som da defesa bem grave */,
                                      "voz_round.wav", "voz_lutem.wav", "voz_vitoria.wav"};
#define IAF_SFX_VOICES 3 /* copias de cada efeito para sobrepor golpes seguidos */
static Sound sfx[SFX_N][IAF_SFX_VOICES];
static int sfxNext[SFX_N];
static int iafSfxPlays[SFX_N]; /* quantas vezes cada efeito foi pedido (usado pelos testes) */
static Music musSel, musFight, musArena; /* musArena: trilha propria da arena da luta (campo music de ARENAS) */
static bool musLoaded = false, iafMuted = false;
static int iafMusLvl = IAF_VOL_LVL_PADRAO, iafSfxLvl = IAF_VOL_LVL_PADRAO; /* volume do modo (menu): 0..IAF_VOL_LVL_MAX */
static float IafLvlMult(int lvl) { return (float)lvl / IAF_VOL_LVL_PADRAO; } /* nivel padrao = volume de sempre */
static int musCur = -1;                  /* -1 nenhuma, 0 selecao, 1 luta, 2 trilha da arena */
static const char *musArenaFile = NULL;   /* arquivo carregado (ou tentado) em musArena */
static const char *ArenaMusicFile(void);  /* trilha da arena da luta ou NULL (iafight_arena.h) */
static float AudioMaster(void) { return iafMuted ? 0.0f : Clamp(g_musicVolume / 0.45f, 0.0f, 2.5f); }
static void LoadIafAudio(void)
{
    if (!IsAudioDeviceReady()) return;
    char path[256];
    for (int i = 0; i < SFX_N; i++)
    {
        snprintf(path, sizeof path, "%s%s", IAF_AUDIO_DIR, SFX_FILE[i]);
        if (!FileExists(path)) continue;
        sfx[i][0] = LoadSound(path);
        if (sfx[i][0].frameCount == 0) continue;
        for (int v = 1; v < IAF_SFX_VOICES; v++) sfx[i][v] = LoadSoundAlias(sfx[i][0]);
    }
    if (FileExists(IAF_AUDIO_DIR "musica_selecao.ogg") && FileExists(IAF_AUDIO_DIR "musica_luta.ogg"))
    {
        musSel = LoadMusicStream(IAF_AUDIO_DIR "musica_selecao.ogg");
        musFight = LoadMusicStream(IAF_AUDIO_DIR "musica_luta.ogg");
        musSel.looping = musFight.looping = true;
        musLoaded = musSel.stream.buffer != NULL && musFight.stream.buffer != NULL;
    }
}
static void PlaySfx(int id)
{
    iafSfxPlays[id]++;
    Sound *s = &sfx[id][sfxNext[id]];
    sfxNext[id] = (sfxNext[id] + 1) % IAF_SFX_VOICES;
    if (!IsAudioDeviceReady() || s->frameCount == 0) return;
    float vol = fminf(1.0f, IAF_SFX_VOLUME * AudioMaster() * IafLvlMult(iafSfxLvl));
    if (vol <= 0.001f) return;
    SetSoundVolume(*s, vol);
    if (id == SFX_DASH) SetSoundPitch(*s, 0.55f + Rand01() * 0.06f);
    else if (id == SFX_DENY) SetSoundPitch(*s, 0.5f);
    else SetSoundPitch(*s, id >= VOX_ROUND || id == SFX_MOVE || id == SFX_OK ? 1.0f : 0.95f + Rand01() * 0.1f); /* golpes variam um pouco */
    PlaySound(*s);
}
static Music *IafMusic(int id) { return id == 0 ? &musSel : (id == 1 ? &musFight : (id == 2 ? &musArena : NULL)); }
/* carrega a trilha da arena (uma por vez); tenta cada arquivo uma vez so: se faltar, a luta usa a musica_luta */
static bool LoadArenaMusic(const char *file)
{
    if (musArenaFile && strcmp(musArenaFile, file) == 0) return musArena.stream.buffer != NULL;
    if (musArena.stream.buffer) UnloadMusicStream(musArena);
    musArena = (Music){0};
    musArenaFile = file;
    char path[256];
    snprintf(path, sizeof path, "%s%s", IAF_AUDIO_DIR, file);
    if (FileExists(path)) musArena = LoadMusicStream(path);
    musArena.looping = true;
    return musArena.stream.buffer != NULL;
}
/* toca a trilha pedida (0 selecao, 1 luta, -1 silencio) e mantem o stream andando. Na luta, a arena com
   trilha propria (ARENAS[].music) toca a dela; as outras tocam a musica_luta */
static void UpdateIafMusic(int want)
{
    if (!musLoaded) return;
    if (want == 1)
    {
        const char *f = ArenaMusicFile();
        if (musCur == 2 && (!f || strcmp(f, musArenaFile) != 0)) { StopMusicStream(musArena); musCur = -1; } /* arena mudou */
        if (f && LoadArenaMusic(f)) want = 2;
    }
    if (want != musCur)
    {
        Music *old = IafMusic(musCur), *nw = IafMusic(want);
        if (old) StopMusicStream(*old);
        musCur = want;
        if (nw) PlayMusicStream(*nw);
    }
    Music *m = IafMusic(musCur);
    if (!m) return;
    float gain = musCur == 2 ? IAF_ARENA_MUSIC_GAIN : (musCur == 0 ? IAF_SELECT_MUSIC_GAIN : 1.0f); /* arena mais alta, selecao mais baixa */
    SetMusicVolume(*m, fminf(1.0f, IAF_MUSIC_VOLUME * gain * AudioMaster() * IafLvlMult(iafMusLvl)));
    UpdateMusicStream(*m);
}
static void UnloadIafAudio(void)
{
    for (int i = 0; i < SFX_N; i++)
    {
        if (sfx[i][0].frameCount == 0) continue;
        for (int v = 1; v < IAF_SFX_VOICES; v++) UnloadSoundAlias(sfx[i][v]);
        UnloadSound(sfx[i][0]);
    }
    memset(sfx, 0, sizeof sfx);
    if (musLoaded) { UnloadMusicStream(musSel); UnloadMusicStream(musFight); }
    if (musArena.stream.buffer) UnloadMusicStream(musArena);
    musArena = (Music){0};
    musArenaFile = NULL;
    musLoaded = false;
    musCur = -1;
}
/* buffer de entrada: cada botao apertado fica guardado por IAF_BUFFER s (o especial por IAF_BUFFER_SPECIAL)
   e entra no 1o quadro em que o lutador pode usa-lo: fim da recuperacao, aterrissagem, levantar, fim da
   defesa ou da pausa de impacto. Vale para teclado, controle e CPU (BufferInput, chamado pela SimStep). */
enum { BB_UP, BB_LIGHT, BB_HEAVY, BB_SPECIAL, BB_SHOT, BB_DASH, BB_GRAB, BB_N };
static float bufT[2][BB_N];
static float pressAge[2][2] = {{9, 9}, {9, 9}}; /* tempo desde o ultimo fraco / forte (agarrao = os dois juntos) */

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
        if (!impacts[i].on) { impacts[i] = (Impact){true, p, 0, sc, face, 0}; return; }
}
static void AddImpactWave(Vector2 p, int face)
{
    for (int i = 0; i < 16; i++)
        if (!impacts[i].on) { impacts[i] = (Impact){true, p, 0, 1.0f, face, 1}; return; }
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

/* ------------------------------------------------------------------ cor alternativa (1 x 1 com o mesmo lutador) */
static bool g_altPal = false; /* ligado enquanto desenha o lutador da direita num espelho */
#define IAF_ELIRA_ALT_TINT ((Color){255, 178, 140, 255})
#define IAF_CLAUDE_ALT_TINT ((Color){150, 196, 255, 255}) /* sprite do Claude no espelho: tom frio */
#define IAF_ANTI_ALT_TINT ((Color){120, 110, 255, 255})   /* sprite do Antigravity no espelho: arco-iris puxado para o violeta */
static Color AltCol(Color c)
{ /* gira o matiz e mantem brilho/saturacao */
    Vector3 h = ColorToHSV(c);
    Color o = ColorFromHSV(fmodf(h.x + 150.0f, 360.0f), h.y, h.z);
    o.a = c.a;
    return o;
}
static Color FCol(const Fighter *a) { return a->alt ? AltCol(CHCOL[a->ch]) : CHCOL[a->ch]; }

/* ------------------------------------------------------------------ esqueleto das IAs (primitivas) */
typedef struct { float rootY, lean, head, aF, eF, aB, eB, lF, kF, lB, kB; } Pose;
typedef struct { float legU, legL, torso, armU, armL, shW, hipW, headR, limbW; } Dims;
static const Dims DIMS[CH_N] = {
    {50, 50, 70, 40, 40, 24, 18, 21, 15}, /* elira (nao usado) */
    {52, 52, 72, 40, 40, 25, 19, 26, 15},
    {50, 50, 74, 40, 42, 34, 23, 25, 20},
    {55, 54, 70, 40, 38, 22, 16, 23, 13},
    {50, 50, 70, 40, 40, 24, 18, 21, 15}, /* iara (nao usado: so sem o atlas) */
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
    if (g_altPal) { coat = AltCol(coat); coatHi = AltCol(coatHi); coatDk = AltCol(coatDk); coatSh = AltCol(coatSh); gold = AltCol(gold); }
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
    if (g_altPal) { neon = AltCol(neon); hood = (Color){58, 44, 62, 255}; hoodHi = (Color){104, 84, 112, 255}; hoodSh = (Color){40, 30, 44, 255}; }
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
    if (g_altPal) { vio = AltCol(vio); teal = AltCol(teal); suitSh = (Color){200, 176, 160, 255}; }
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
    if (a->st == ST_DASH)
    { /* investida: corpo inclinado, passada aberta, bracos para tras */
        p.lean += 20; p.lF = 44; p.kF = 26; p.lB = -40; p.kB = 22; p.aF = 20; p.eF = 70; p.aB = -20; p.eB = 40; p.rootY = 10;
    }
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
        case AT_FWDH: /* soco longo: o corpo inteiro vai junto */
            if (wind > 0) { p.aF = Lerp(55, 30, wind); p.eF = Lerp(75, 120, wind); p.lean -= 6 * wind; }
            else { p.aF = Lerp(60, 90, e); p.eF = Lerp(60, 0, e); p.aB = 30; p.eB = 90; p.lean = 6 + 20 * e; p.lF = 22 + 22 * e; p.lB = -18 - 14 * e; }
            break;
        case AT_UPPER: /* uppercut: abaixa e sobe com o braco para o alto */
            if (wind > 0) { p = PCrouch(); p.aF = 20; p.eF = 110; p.lean = 18; }
            else { p.aF = Lerp(60, 168, e); p.eF = Lerp(90, 8, e); p.aB = 40; p.eB = 80; p.lean = -10 * e; p.rootY = 6 - 18 * e; p.lF = 30; p.kF = 10; }
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
static int AtkIdx(Fighter *a, int n, int a0, int a1, float st, float ac, float rc)
{
    int i;
    float t = a->atkT;
    if (ac < IAF_ELIRA_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ELIRA_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ELIRA_ACTIVE_VISUAL; } /* segura o impacto */
    if (t < st) i = a0 > 0 ? (int)(t / st * a0) : 0;
    else if (t < st + ac) i = a0 + (a1 > a0 ? (int)((t - st) / ac * (a1 - a0 + 1)) : 0);
    else
    {
        int nr = n - 1 - a1;
        i = nr > 0 ? a1 + 1 + (int)((t - st - ac) / rc * nr) : a1;
    }
    return i;
}
static int AtkFrame(Fighter *a, int anim, int a0, int a1, float st, float ac, float rc) { return EAFrame(anim, AtkIdx(a, EA_COUNT[anim], a0, a1, st, ac, rc)); }
static int AtkFrame2(Fighter *a, int anim, int a0, int a1, float st, float ac, float rc) { return E2Frame(anim, AtkIdx(a, E2_COUNT[anim], a0, a1, st, ac, rc)); }
/* quadros de impacto escolhidos no pacote (o calculo automatico pega o canhao, nao o membro) */
#define EP_PUNCH 4
#define EP_KICK 3
#define EP_UPPER 4 /* uppercut provisorio: inicio da vitoria, canhao subindo */
#define EP_SHOT_LAST 11 /* ultimo quadro do tiro: sem a bala da arte (a bala e o projetil do jogo) */
#define EP_CHARGE_TOP 8 /* disparo carregado: orbe maior na boca do canhao */
/* queda nova (E2_KO), presa a fisica: impacto (0) no acerto; no ar, pes saindo (1) subindo, voo inclinado (2) no
   alto, quase horizontal (3) descendo; no chao, bate de costas (4) no instante do pouso, quica (5), deitada (6) e
   deitada parada (7) enquanto estiver caida. Os quadros no ar guardam a altura da grade da arte; essa altura extra
   some conforme ela desce (yoff), para o pouso nao dar salto. */
static int EliraKoFrame(Fighter *a, float *yoff)
{
    if (!a->down && a->stT < IAF_ELIRA_KO_IMPACT) return E2Frame(E2_KO, 0); /* stT zera no acerto */
    if (!OnGround(a))
    {
        int k = a->vy < -200 ? 1 : (a->vy < 200 ? 2 : 3);
        const EFrame *e = &EF2[E2_FIRST[E2_KO] + k];
        float lift = (e->ay - e->h) * EliraScale(1.0f); /* altura do quadro acima da linha dos pes na grade */
        float h = IAF_GROUND_Y - a->y;
        *yoff = lift * (1.0f - Clamp(h / IAF_ELIRA_KO_APEX, 0.0f, 1.0f));
        return E2Frame(E2_KO, k);
    }
    float g = a->groundT;
    return E2Frame(E2_KO, g < 0.07f ? 4 : (g < 0.14f ? 5 : (g < 0.24f ? 6 : 7)));
}
/* sprites novos: quadro do estado, ou -1 quando a animacao nao tem arte nova (usa a atual) */
static int EliraFrameIdxV2(Fighter *a, float *yoff)
{
    if (a->st == ST_KO) return EliraKoFrame(a, yoff); /* nocaute: cai e fica deitada */
    if (a->down)
    { /* caida: deitada nova ate o levantar; o levantar e o antigo (EA_GETUP), que comeca reclinado com a cabeca
         do mesmo lado (esquerda), entao corta direto da deitada nova para o 1o quadro dele */
        if (a->downT > 0.36f) return EliraKoFrame(a, yoff);
        return -1;
    }
    if (a->st == ST_LOSE) return E2Frame(E2_DEFEAT, (int)(a->stT * 10)); /* cai e fica no ultimo quadro */
    if (a->st == ST_WIN) return E2Frame(E2_WIN, (int)(a->stT * 10));
    if (a->st == ST_HIT)
    { /* dano agachada: tranco (1), cabeca para tras (2) ate quase o fim do atordoamento, volta (3) */
        if ((!OnGround(a) || a->vy < 0) && !a->grabbed) return EliraKoFrame(a, yoff); /* lancada: voa como na queda */
        if (a->hitCrouch) return E2Frame(E2_HIT_CROUCH, a->stT < 0.06f ? 1 : (a->stun > 0.08f ? 2 : 3));
        /* dano em pe (stT zera a cada acerto): impacto (1) no instante do acerto, tranco maximo (3) no meio do
           atordoamento, recuperacao (4) no fim; atordoamento curto (fraco = 0,20 s) pula o tranco (2) */
        float tot = a->stT + fmaxf(a->stun, 0), p = tot > 0 ? a->stT / tot : 1;
        if (tot < IAF_ELIRA_HIT_SHORT) return E2Frame(E2_HIT, p < 0.3f ? 1 : (p < 0.65f ? 3 : 4));
        return E2Frame(E2_HIT, p < 0.2f ? 1 : (p < 0.38f ? 2 : (p < 0.7f ? 3 : 4)));
    }
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_ELIRA][a->atk < AT_SPECIAL ? a->atk : 0];
        float t = a->atkT;
        switch (a->atk)
        {
        /* preparo = quadros [0, a0); ativo = [a0, a1] (o de maior alcance, E2_PEAK, no ativo); recuperacao = o resto */
        case AT_LIGHT: return AtkFrame2(a, E2_PUNCH, 2, 2, d->st, d->ac, d->rc);        /* 5: guarda, recolhe, SOCO, volta, guarda */
        case AT_HEAVY: return AtkFrame2(a, E2_KICK, 3, 4, d->st, d->ac, d->rc);         /* 7: joelho sobe, chute alto (3-4), desce */
        case AT_FWDH: return AtkFrame2(a, E2_PUNCH_HEAVY, 3, 4, d->st, d->ac, d->rc);   /* 7: passo, avanco, canhao esticado (3-4) */
        case AT_UPPER: return AtkFrame2(a, E2_UPPER, 3, 4, d->st, d->ac, d->rc);        /* 8: agacha, sobe com o punho (3-4), cai */
        case AT_LOW: return AtkFrame2(a, E2_SWEEP, 3, 4, d->st, d->ac, d->rc);          /* 7: perna estica (3-4) e recolhe */
        case AT_AIR: return AtkFrame2(a, E2_AIR, 2, 3, d->st, d->ac, d->rc);            /* 6: encolhida, chute esticado (2-3) */
        case AT_SPECIAL:
        { /* 10: guarda, firma, segura, carga (3-5) no preparo; o quadro do DISPARO (6) comeca junto com o
             nucleo do jogo (SpawnProj em t = SP_ST) e fica IAF_ELIRA_SP_FLASH s; depois recuo, volta e guarda */
            float st = SP_ST[CH_ELIRA], rest = SP_AC[CH_ELIRA] + SP_RC[CH_ELIRA] - IAF_ELIRA_SP_FLASH;
            int pk = E2_PEAK[E2_SPECIAL], n = E2_COUNT[E2_SPECIAL];
            if (t < st) return E2Frame(E2_SPECIAL, (int)(t / st * pk));
            if (t < st + IAF_ELIRA_SP_FLASH) return E2Frame(E2_SPECIAL, pk);
            return E2Frame(E2_SPECIAL, pk + 1 + (int)((t - st - IAF_ELIRA_SP_FLASH) / rest * (n - pk - 1)));
        }
        default: return -1;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    {
        if (a->crouch) return -1; /* defesa agachada: a atual */
        if (a->shieldHit > 0) return E2Frame(E2_DEF, 3); /* bloqueou: o escudo treme (3 <-> 4) */
        return E2Frame(E2_DEF, 1 + (int)(a->shieldT / 0.04f)); /* ergue o canhao, abre o escudo e fica no ultimo */
    }
    if (a->st == ST_DASH || a->st == ST_JUMP) return -1;
    if (a->crouch)
    { /* abaixa e fica no ultimo quadro (agachada parada) */
        int n = E2_COUNT[E2_CROUCH]; /* o quadro 0 e a guarda em pe */
        return E2Frame(E2_CROUCH, 1 + (int)(a->stT / 0.15f * (n - 1)));
    }
    return -1;
}
static int EliraFrameIdx(Fighter *a, float *yoff)
{
    *yoff = 0;
    if (EliraV2())
    {
        int f = EliraFrameIdxV2(a, yoff);
        if (f >= 0) return f;
    }
    if (a->st == ST_KO) return EAFrame(EA_KO, (int)(a->stT / 0.045f));
    if (a->down)
    { /* caida: deitada; ao fim, levantando */
        if (a->downT > 0.36f) return EAFrame(EA_KO, EA_COUNT[EA_KO] - 1);
        return EAFrame(EA_GETUP, (int)((1 - a->downT / 0.36f) * EA_COUNT[EA_GETUP]));
    }
    if (a->st == ST_LOSE) return EAFrame(EA_DEFEAT, (int)(a->stT * 12) % EA_COUNT[EA_DEFEAT]);
    if (a->st == ST_WIN) return EAFrame(EA_WIN, (int)(a->stT * 12));
    if (a->st == ST_HIT)
    {
        if (a->hitCrouch) return EAFrame(EA_HIT_CROUCH, a->stT < 0.12f ? 0 : 1);
        return EAFrame(EA_HIT, (int)(a->stT * 26));
    }
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_ELIRA][a->atk < AT_SPECIAL ? a->atk : 0];
        float t = a->atkT;
        switch (a->atk)
        {
        case AT_LIGHT: return AtkFrame(a, EA_PUNCH, EP_PUNCH, EP_PUNCH, d->st, d->ac, d->rc);
        case AT_HEAVY: return AtkFrame(a, EA_KICK, EP_KICK, EP_KICK + 1, d->st, d->ac, d->rc);
        case AT_FWDH: return AtkFrame(a, EA_PUNCH_HEAVY, EA_PEAK[EA_PUNCH_HEAVY], EA_PEAK[EA_PUNCH_HEAVY], d->st, d->ac, d->rc);
        case AT_UPPER:
        { /* provisorio: canhao subindo (inicio da vitoria) e voltando */
            float ac = fmaxf(d->ac, IAF_ELIRA_ACTIVE_VISUAL), rc = fmaxf(d->rc - (ac - d->ac), 0.02f);
            if (t < d->st) return EAFrame(EA_WIN, (int)(t / d->st * EP_UPPER));
            if (t < d->st + ac) return EAFrame(EA_WIN, EP_UPPER);
            return EAFrame(EA_WIN, EP_UPPER - (int)((t - d->st - ac) / rc * EP_UPPER));
        }
        case AT_LOW: return AtkFrame(a, EA_SWEEP, 2, 2, d->st, d->ac, d->rc); /* provisorio: ficha antiga */
        case AT_AIR: return AtkFrame(a, EA_KICK, EP_KICK, EP_KICK + 1, d->st, d->ac, d->rc); /* chute do pacote no ar */
        case AT_SHOT:
            if (t < IAF_SHOT_STARTUP) return EAFrame(EA_SHOT, EP_SHOT_LAST);                   /* mira */
            if (t < IAF_SHOT_STARTUP + 0.09f) return EAFrame(EA_SHOT, t < IAF_SHOT_STARTUP + 0.045f ? 0 : 1); /* clarao */
            return EAFrame(EA_SHOT, EP_SHOT_LAST);
        case AT_SPECIAL:
        { /* Rajada Temporal: o orbe cresce na boca do canhao durante a carga; no disparo sai o nucleo do jogo */
            float st = SP_ST[CH_ELIRA];
            if (t < st) return EAFrame(EA_CHARGED_SHOT, (int)(t / st * (EP_CHARGE_TOP + 1)));
            return EAFrame(EA_CHARGED_SHOT, t < st + 0.08f ? 0 : EA_COUNT[EA_CHARGED_SHOT] - 1);
        }
        default: break;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    { /* defesa (escudo aprovado): 1o quadro ao entrar, 2o enquanto segura, 3o ao bloquear */
        int anim = a->crouch ? EA_DEF_CROUCH : EA_DEF;
        if (a->shieldHit > 0) return EAFrame(anim, 2);
        return EAFrame(anim, a->shieldT < 0.08f ? 0 : 1);
    }
    if (a->st == ST_DASH) return EAFrame(EA_DASH, (int)(a->dashT / IAF_DASH_TIME * EA_COUNT[EA_DASH]));
    if (a->st == ST_JUMP)
    { /* sobe (quadros 0-6) e desce (7-13) conforme a velocidade */
        float k = Clamp((a->vy + IAF_JUMP_SPEED) / (2 * IAF_JUMP_SPEED), 0.0f, 1.0f);
        return EAFrame(EA_JUMP, 2 + (int)(k * (EA_COUNT[EA_JUMP] - 4)));
    }
    if (a->crouch)
    {
        if (a->stT < 0.2f) return EAFrame(EA_CROUCH, (int)(a->stT / 0.2f * EA_COUNT[EA_CROUCH]));
        return EAFrame(EA_CROUCH_IDLE, (int)(gt * 9) % EA_COUNT[EA_CROUCH_IDLE]);
    }
    if (a->st == ST_WALK)
    {
        bool back = (a->vx * a->face) < 0;
        int anim = back ? EA_WALK_B : EA_WALK_F;
        return EAFrame(anim, (int)(a->anim * 12) % EA_COUNT[anim]);
    }
    return EAFrame(EA_IDLE, (int)(gt * 9) % EA_COUNT[EA_IDLE]); /* a parada do pacote ja respira */
}

/* ------------------------------------------------------------------ Claude: atlas de luta
   Gerado fora do repositorio a partir do pacote de sprites (quadros 256x161, pixel art de 2 px).
   Ampliado com filtro POINT: 2x = 4 px de tela por pixel da arte, ~204 px de altura (como a Elira). */
#include "iafight_claude.h"
static Texture2D texClaude;
#define CL_SCALE 1.5f
#define CL_FRONT 142.0f /* do centro ate a frente do corpo: prisma, feixe e pulso de luz */
#define CL_STRIKE_REACH 192.0f /* ponta do punho (soco forte) e do pe (chute frontal) no quadro mais esticado */
/* equilibrio do prisma (o especial do Claude causava ~6x o dano do especial dos outros) */
#define CL_PRISM_COUNTER_DMG 60.0f /* feixe de resposta quando o prisma apara um golpe */
#define CL_REFLECT_MULT 0.7f       /* dano do projetil refletido em relacao ao original */
static int CAFrame(int anim, int i)
{
    int n = CA_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return CA_FIRST[anim] + i;
}
static int CALoop(int anim, float t) { return CA_FIRST[anim] + ((int)(t / CA_DUR[anim]) % CA_COUNT[anim]); }
/* desenha o quadro f com os pes em (x, yFeet); espelha quando face < 0 */
static void DrawCF(int f, float x, float yFeet, int face, float sc, Color tint, float flash)
{
    const CFrame *e = &CF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float left = face > 0 ? x - e->ax * sc : x - (e->w - e->ax) * sc;
    if (face < 0) src.width = -src.width;
    Rectangle dst = {roundf(left), roundf(yFeet - e->ay * sc), e->w * sc, e->h * sc};
    DrawTexturePro(texClaude, src, dst, (Vector2){0, 0}, 0, tint);
    if (flash > 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(texClaude, src, dst, (Vector2){0, 0}, 0, Fade(WHITE, flash));
        EndBlendMode();
    }
}
/* golpe: preparo usa os quadros [0, a0), ativo [a0, a1] (punho/pe esticado), recuperacao o resto */
static int ClaudeAtkFrame(Fighter *a, int anim, int a0, int a1, float st, float ac, float rc)
{
    int n = CA_COUNT[anim], i;
    float t = a->atkT;
    if (ac < IAF_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ACTIVE_VISUAL; }
    if (t < st) i = (int)(t / st * a0);
    else if (t < st + ac) i = a0 + (int)((t - st) / ac * (a1 - a0 + 1));
    else i = a1 + 1 + (int)((t - st - ac) / rc * (n - 1 - a1));
    return CAFrame(anim, i);
}
static int ClaudeFrameIdx(Fighter *a)
{
    int nk = CA_COUNT[CA_KNOCKDOWN];
    if (a->st == ST_KO) return CAFrame(CA_KNOCKDOWN, (int)(a->stT / 0.06f));
    if (a->down)
    { /* caido; ao levantar, a queda tocada ao contrario */
        if (a->downT > 0.36f) return CAFrame(CA_KNOCKDOWN, nk - 1);
        return CAFrame(CA_KNOCKDOWN, (int)(a->downT / 0.36f * (nk - 1)));
    }
    if (a->st == ST_LOSE) return CAFrame(CA_KNOCKDOWN, nk - 1);
    if (a->st == ST_WIN) return CALoop(CA_VICTORY, a->stT);
    if (a->st == ST_HIT) return CAFrame(CA_HURT, (int)(a->stT / 0.4f * CA_COUNT[CA_HURT]));
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_CLAUDE][a->atk < AT_SPECIAL ? a->atk : 0];
        switch (a->atk)
        {
        case AT_LIGHT: return ClaudeAtkFrame(a, CA_JAB, 2, 4, d->st, d->ac, d->rc);
        case AT_HEAVY: return ClaudeAtkFrame(a, CA_FKICK, 3, 6, d->st, d->ac, d->rc);
        case AT_FWDH: return ClaudeAtkFrame(a, CA_HEAVY, 4, 6, d->st, d->ac, d->rc);
        case AT_UPPER: return ClaudeAtkFrame(a, CA_UPPER, 4, 7, d->st, d->ac, d->rc);
        case AT_LOW: return ClaudeAtkFrame(a, CA_SWEEP, 4, 6, d->st, d->ac, d->rc);
        case AT_AIR: return ClaudeAtkFrame(a, CA_AIRKICK, 4, 6, d->st, d->ac, d->rc);
        case AT_SPECIAL:
        { /* prisma: carga em loop enquanto o escudo esta de pe; depois (ou ao refletir) o disparo */
            float st = SP_ST[CH_CLAUDE], ac = SP_AC[CH_CLAUDE], rc = SP_RC[CH_CLAUDE];
            if (a->atkT < st + ac) return CALoop(CA_CHARGE, a->atkT);
            return CAFrame(CA_BLAST, (int)((a->atkT - st - ac) / rc * CA_COUNT[CA_BLAST]));
        }
        default: break;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    {
        if (a->crouch) return CAFrame(CA_CROUCH, CA_COUNT[CA_CROUCH] - 1);
        return CALoop(CA_BLOCK, a->stT);
    }
    if (a->st == ST_DASH) return CAFrame(CA_DASH, (int)(a->dashT / IAF_DASH_TIME * CA_COUNT[CA_DASH]));
    if (a->st == ST_JUMP) return CAFrame(CA_JUMP, 1 + (int)((a->vy + IAF_JUMP_SPEED) / (2 * IAF_JUMP_SPEED) * 9));
    if (a->crouch) return CAFrame(CA_CROUCH, (int)(a->stT / 0.035f));
    if (a->st == ST_WALK) return CALoop((a->vx * a->face) < 0 ? CA_WALK_B : CA_WALK_F, a->anim);
    return CALoop(CA_IDLE, gt);
}

/* ------------------------------------------------------------------ Antigravity: atlas de luta
   Gerado fora do repositorio a partir do pacote de sprites (384x354 = grade nativa 128x118 em 3x).
   O atlas guarda a grade nativa; o jogo amplia 2,5x com filtro POINT (~192 px de altura parada,
   entre o Claude e a Elira). A flutuacao do desenho antigo continua (AntiHover). */
#include "iafight_antigravity.h"
static Texture2D texAnti;
#define AG_SCALE 2.5f
/* equilibrio do Antigravity (simulacao CPU x CPU contra Elira e Claude) */
#define AG_WELL_SLAM_DMG 110.0f /* queda no fim do poco gravitacional */
static int AGFrame(int anim, int i)
{
    int n = AA_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return AA_FIRST[anim] + i;
}
static int AGLoop(int anim, float t) { return AA_FIRST[anim] + ((int)(t / AA_DUR[anim]) % AA_COUNT[anim]); }
static void DrawAF(int f, float x, float yFeet, int face, float sc, Color tint, float flash)
{
    const AFrame *e = &AF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float left = face > 0 ? x - e->ax * sc : x - (e->w - e->ax) * sc;
    if (face < 0) src.width = -src.width;
    Rectangle dst = {roundf(left), roundf(yFeet - e->ay * sc), e->w * sc, e->h * sc};
    DrawTexturePro(texAnti, src, dst, (Vector2){0, 0}, 0, tint);
    if (flash > 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(texAnti, src, dst, (Vector2){0, 0}, 0, Fade(WHITE, flash));
        EndBlendMode();
    }
}
/* flutuacao: o "A" fica um pouco acima do chao, subindo e descendo (menos caido) */
static float AntiHover(Fighter *a)
{
    if (a->st == ST_KO || a->down || a->st == ST_LOSE) return 0;
    return 16 + 5 * sinf(gt * 2.4f);
}
/* golpe: preparo usa [f0, a0), ativo [a0, a1], recuperacao (a1, f1] */
static int AntiAtkFrame(Fighter *a, int anim, int f0, int a0, int a1, int f1, float st, float ac, float rc)
{
    float t = a->atkT;
    int i;
    if (ac < IAF_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ACTIVE_VISUAL; }
    if (t < st) i = f0 + (int)(t / st * (a0 - f0));
    else if (t < st + ac) i = a0 + (int)((t - st) / ac * (a1 - a0 + 1));
    else i = a1 + 1 + (int)((t - st - ac) / rc * (f1 - a1));
    if (i > f1) i = f1;
    return AGFrame(anim, i);
}
static int AntiFrameIdx(Fighter *a)
{
    int nk = AA_COUNT[AA_KO];
    if (a->st == ST_KO) return AGFrame(AA_KO, (int)(a->stT / AA_DUR[AA_KO]));
    if (a->down)
    { /* caida; ao levantar, o nocaute tocado ao contrario */
        if (a->downT > 0.36f) return AGFrame(AA_KO, nk - 1);
        return AGFrame(AA_KO, 7 + (int)(a->downT / 0.36f * (nk - 8)));
    }
    if (a->st == ST_LOSE) return AGFrame(AA_KO, nk - 1);
    if (a->st == ST_WIN) return AGLoop(AA_VICTORY, a->stT);
    if (a->st == ST_HIT) return AGFrame(AA_HIT, (int)(a->stT / 0.4f * AA_COUNT[AA_HIT]));
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_ANTI][a->atk < AT_SPECIAL ? a->atk : 0];
        switch (a->atk)
        {
        case AT_LIGHT: return AntiAtkFrame(a, AA_JAB, 0, 6, 10, 17, d->st, d->ac, d->rc);
        case AT_HEAVY: return AntiAtkFrame(a, AA_KICK, 0, 8, 11, 21, d->st, d->ac, d->rc);
        case AT_FWDH: return AntiAtkFrame(a, AA_HEAVY, 0, 9, 14, 21, d->st, d->ac, d->rc);
        case AT_UPPER: return AntiAtkFrame(a, AA_HEAVY, 4, 12, 14, 21, d->st, d->ac, d->rc); /* sem uppercut no pacote: o soco que mais sobe */
        case AT_LOW: return AntiAtkFrame(a, AA_LOWKICK, 0, 9, 12, 19, d->st, d->ac, d->rc);
        case AT_AIR: return AntiAtkFrame(a, AA_SLAM, 10, 14, 17, 24, d->st, d->ac, d->rc); /* pula a subida: so a queda */
        case AT_SPECIAL:
        {
            float tot = SP_ST[CH_ANTI] + SP_AC[CH_ANTI] + SP_RC[CH_ANTI];
            return AGFrame(AA_SPECIAL, (int)(a->atkT / tot * AA_COUNT[AA_SPECIAL]));
        }
        default: break;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    { /* escudo: entra (0-2) e fica nos quadros com o escudo erguido */
        if (a->crouch) return AGFrame(AA_CROUCH, 4);
        if (a->shieldT < 0.1f && a->st != ST_BLOCK) return AGFrame(AA_BLOCK, (int)(a->shieldT / 0.05f));
        return AGFrame(AA_BLOCK, 2 + ((int)(a->stT / AA_DUR[AA_BLOCK]) % 13));
    }
    if (a->st == ST_DASH) return AGFrame(AA_DASH, (int)(a->dashT / IAF_DASH_TIME * AA_COUNT[AA_DASH]));
    if (a->st == ST_JUMP) return AGFrame(AA_JUMP, 3 + (int)((a->vy + IAF_JUMP_SPEED) / (2 * IAF_JUMP_SPEED) * 17));
    if (a->crouch) return AGFrame(AA_CROUCH, (int)(a->stT / AA_DUR[AA_CROUCH]) < 4 ? (int)(a->stT / AA_DUR[AA_CROUCH]) : 4);
    if (a->st == ST_WALK) return AGLoop((a->vx * a->face) < 0 ? AA_WALK_B : AA_WALK_F, a->anim);
    return AGLoop(AA_IDLE, gt);
}

/* ------------------------------------------------------------------ Codex: atlas de luta
   Gerado fora do repositorio a partir do pacote Codex Fighter V2 (grade nativa 128x128).
   O atlas guarda 1 pixel da arte por pixel; 2x na tela = ~198 px de altura (Elira ~218, Claude ~204). */
#include "iafight_codex.h"
static Texture2D texCodex;
#define XC_SCALE 2.0f
#define XC_FRONT 76.0f /* do centro ate a frente da nuvem: placa de defesa e agarrao do especial */
#define XC_LUNGE_MULT 3.4f /* avanco do corpo nos golpes em relacao aos outros (a luva alcanca pouco) */
#define IAF_CODEX_ALT_TINT ((Color){150, 255, 185, 255}) /* sprite do Codex no espelho: luvas e rosto verdes */
static int XAFrame(int anim, int i)
{
    int n = XA_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return XA_FIRST[anim] + i;
}
static int XALoop(int anim, float t) { return XA_FIRST[anim] + ((int)(t / XA_DUR[anim]) % XA_COUNT[anim]); }
static void DrawXF(int f, float x, float yFeet, int face, float sc, Color tint, float flash)
{
    const XFrame *e = &XF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float left = face > 0 ? x - e->ax * sc : x - (e->w - e->ax) * sc;
    if (face < 0) src.width = -src.width;
    Rectangle dst = {roundf(left), roundf(yFeet - e->ay * sc), e->w * sc, e->h * sc};
    DrawTexturePro(texCodex, src, dst, (Vector2){0, 0}, 0, tint);
    if (flash > 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(texCodex, src, dst, (Vector2){0, 0}, 0, Fade(WHITE, flash));
        EndBlendMode();
    }
}
/* golpe: preparo usa os quadros [0, a0), ativo [a0, a1] (punho/pe esticado), recuperacao o resto */
static int CodexAtkFrame(Fighter *a, int anim, int a0, int a1, float st, float ac, float rc)
{
    int n = XA_COUNT[anim], i;
    float t = a->atkT;
    if (ac < IAF_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ACTIVE_VISUAL; }
    if (t < st) i = (int)(t / st * a0);
    else if (t < st + ac) i = a0 + (int)((t - st) / ac * (a1 - a0 + 1));
    else i = a1 + 1 + (int)((t - st - ac) / rc * (n - 1 - a1));
    return XAFrame(anim, i);
}
static int CodexFrameIdx(Fighter *a)
{
    int nk = XA_COUNT[XA_KO];
    if (a->st == ST_KO) return XAFrame(XA_KO, (int)(a->stT / 0.06f));
    if (a->down)
    { /* caido; ao levantar, a queda tocada ao contrario */
        if (a->downT > 0.36f) return XAFrame(XA_KO, nk - 1);
        return XAFrame(XA_KO, (int)(a->downT / 0.36f * (nk - 1)));
    }
    if (a->st == ST_LOSE) return XAFrame(XA_KO, nk - 1);
    if (a->st == ST_WIN) return XALoop(XA_VICTORY, a->stT);
    if (a->st == ST_HIT) return XAFrame(XA_HIT, (int)(a->stT / 0.4f * XA_COUNT[XA_HIT]));
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_CODEX][a->atk < AT_SPECIAL ? a->atk : 0];
        switch (a->atk)
        {
        case AT_LIGHT: return CodexAtkFrame(a, XA_LIGHT, 6, 11, d->st, d->ac, d->rc);
        case AT_HEAVY: return CodexAtkFrame(a, XA_KICK, 9, 17, d->st, d->ac, d->rc);
        case AT_FWDH: return CodexAtkFrame(a, XA_LONG, 11, 16, d->st, d->ac, d->rc);
        case AT_UPPER: return CodexAtkFrame(a, XA_UPPER, 7, 12, d->st, d->ac, d->rc);
        case AT_LOW: return CodexAtkFrame(a, XA_LOWKICK, 7, 13, d->st, d->ac, d->rc);
        case AT_AIR: return CodexAtkFrame(a, XA_KICK, 9, 17, d->st, d->ac, d->rc); /* sem aereo no pacote: chute alto */
        case AT_SPECIAL:
        { /* deploy forcado: carga no preparo, investida com o punho, socos da sequencia e o disparo final */
            float st = SP_ST[CH_CODEX], ac = SP_AC[CH_CODEX], rc = SP_RC[CH_CODEX];
            if (a->atkT < st) return XALoop(XA_CHARGE, a->atkT);
            if (a->caught)
            {
                if (a->spHits < 4) return XAFrame(XA_SPECIAL, 11 + (int)(fmodf(a->spT, 0.09f) / 0.09f * 6));
                return XAFrame(XA_SPECIAL, 17 + (int)((a->spT - 0.36f) / 0.14f * 7));
            }
            if (a->atkT < st + ac) return XAFrame(XA_SPECIAL, 8 + (int)((a->atkT - st) / ac * 9));
            return XAFrame(XA_SPECIAL, 21 + (int)((a->atkT - st - ac) / rc * 7));
        }
        default: break;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    {
        if (a->crouch) return XAFrame(XA_CROUCH, XA_COUNT[XA_CROUCH] - 1);
        float t = a->stT / XA_DUR[XA_GUARD];
        return t < 3 ? XAFrame(XA_GUARD, (int)t) : XA_FIRST[XA_GUARD] + 3 + ((int)t - 3) % 10; /* entra e segura a guarda */
    }
    if (a->st == ST_DASH) return XAFrame(XA_DASH, (int)(a->dashT / IAF_DASH_TIME * XA_COUNT[XA_DASH]));
    if (a->st == ST_JUMP) return XAFrame(XA_JUMP, 1 + (int)((a->vy + IAF_JUMP_SPEED) / (2 * IAF_JUMP_SPEED) * 9));
    if (a->crouch) return XAFrame(XA_CROUCH, (int)(a->stT / 0.035f));
    if (a->st == ST_WALK) return XALoop((a->vx * a->face) < 0 ? XA_WALK_B : XA_WALK_F, a->anim);
    return XALoop(XA_IDLE, gt);
}

/* ------------------------------------------------------------------ Iara: atlas de luta (personagem de colab)
   Sereia do jogo parceiro: nao tem pernas, flutua numa poca e os chutes sao golpes de cauda. O atlas
   (iafight_iara.h, gerado fora do repositorio a partir das tiras da Iara) e gerado na escala da luta
   (IAF_IARA_ESCALA): desenhada em 1x com filtro POINT; o cartao COLAB usa um retrato pre-escalado. Sem rasteira no pacote: o golpe baixo
   e a pose agachada com um chicote de agua desenhado rente ao chao (IaraDrawLowWhip). */
#include "iafight_iara.h"
static Texture2D texIara;
#define IR_FRONT IRK(70.0f) /* do centro (meio da poca) ate a frente das maos: onda do especial */
#define IR_DRAW (IAF_IARA_ESCALA / IR_ATLAS_ESCALA) /* 1 quando o atlas foi gerado na escala da luta (nitido) */
#define IAF_IARA_ALT_TINT ((Color){200, 160, 255, 255}) /* espelho: agua puxada para o violeta */
static int IRFrame(int anim, int i)
{
    int n = IR_COUNT[anim];
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return IR_FIRST[anim] + i;
}
static int IRLoop(int anim, float t, float fps) { return IR_FIRST[anim] + ((int)(t * fps) % IR_COUNT[anim]); }
static void DrawIRF(int f, float x, float yFeet, int face, float sc, Color tint, float flash)
{
    const IFrame *e = &IRF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float left = face > 0 ? x - e->ax * sc : x - (e->w - e->ax) * sc;
    if (face < 0) src.width = -src.width;
    Rectangle dst = {roundf(left), roundf(yFeet - e->ay * sc), e->w * sc, e->h * sc};
    DrawTexturePro(texIara, src, dst, (Vector2){0, 0}, 0, tint);
    if (flash > 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(texIara, src, dst, (Vector2){0, 0}, 0, Fade(WHITE, flash));
        EndBlendMode();
    }
}
/* efeitos (onda e impacto do especial): ancorados no centro */
static void DrawIRFC(int f, Vector2 c, int face, float sc, Color tint)
{
    const IFrame *e = &IRF[f];
    Rectangle src = {e->x, e->y, e->w, e->h};
    float ax = face > 0 ? e->ax : e->w - e->ax;
    if (face < 0) src.width = -src.width;
    DrawTexturePro(texIara, src, (Rectangle){roundf(c.x - ax * sc), roundf(c.y - e->ay * sc), e->w * sc, e->h * sc}, (Vector2){0, 0}, 0, tint);
}
/* golpe: preparo usa os quadros [0, a0), ativo [a0, a1], recuperacao o resto */
static int IaraAtkFrame(Fighter *a, int anim, int a0, int a1, float st, float ac, float rc)
{
    int n = IR_COUNT[anim], i;
    float t = a->atkT;
    if (ac < IAF_ACTIVE_VISUAL) { rc = fmaxf(rc - (IAF_ACTIVE_VISUAL - ac), 0.02f); ac = IAF_ACTIVE_VISUAL; }
    if (t < st) i = (int)(t / st * a0);
    else if (t < st + ac) i = a0 + (int)((t - st) / ac * (a1 - a0 + 1));
    else i = a1 + 1 + (int)((t - st - ac) / rc * (n - 1 - a1));
    return IRFrame(anim, i);
}
/* especial (Canto do Rio): no preparo ela canta e vira a forma monstruosa (60%) e carrega a agua (40%); o
   quadro do grito (IR_PEAK) sai junto com a onda do jogo (t = SP_ST); depois o recuo e a volta a forma normal */
#define IR_SP_TRANS 0.6f
#define IR_SP_FLASH 0.10f
static int IaraSpecialFrame(float t, float st, float rest)
{
    int pk = IR_PEAK[IR_SPECIAL], n = IR_COUNT[IR_SPECIAL];
    if (t < st * IR_SP_TRANS) return IRFrame(IR_TRANSFORM, (int)(t / (st * IR_SP_TRANS) * IR_COUNT[IR_TRANSFORM]));
    if (t < st) return IRFrame(IR_SPECIAL, 1 + (int)((t - st * IR_SP_TRANS) / (st * (1 - IR_SP_TRANS)) * (pk - 1)));
    if (t < st + IR_SP_FLASH) return IRFrame(IR_SPECIAL, pk);
    return IRFrame(IR_SPECIAL, pk + 1 + (int)((t - st - IR_SP_FLASH) / rest * (n - pk - 1)));
}
/* queda presa a fisica: arremessada (1) subindo, de costas (2) descendo; no chao o respingo (3) no pouso e
   deitada (4-6) */
static int IaraKoFrame(Fighter *a)
{
    if (!a->down && a->st == ST_KO && a->stT < 0.06f) return IRFrame(IR_KO, 0);
    if (!OnGround(a)) return IRFrame(IR_KO, a->vy < 0 ? 1 : 2);
    float g = a->groundT;
    return IRFrame(IR_KO, g < 0.10f ? 3 : (g < 0.18f ? 4 : (g < 0.26f ? 5 : 6)));
}
static int IaraFrameIdx(Fighter *a)
{
    if (a->st == ST_KO) return IaraKoFrame(a);
    if (a->down)
    { /* caida: deitada; no fim, levantando */
        if (a->downT > 0.36f) return IaraKoFrame(a);
        return IRFrame(IR_GETUP, (int)((1 - a->downT / 0.36f) * IR_COUNT[IR_GETUP]));
    }
    if (a->st == ST_LOSE) return IRFrame(IR_LOSE, (int)(a->stT * 8)); /* afunda na poca e fica */
    if (a->st == ST_WIN) return IRFrame(IR_WIN, (int)(a->stT * 9));   /* sobe na coluna de agua e fica */
    if (a->st == ST_HIT)
    {
        if ((!OnGround(a) || a->vy < 0) && !a->grabbed) return IRFrame(IR_KO, a->vy < 0 ? 1 : 2); /* lancada */
        float tot = a->stT + fmaxf(a->stun, 0), p = tot > 0 ? a->stT / tot : 1;
        if (a->hitCrouch) return IRFrame(IR_HIT_CROUCH, p < 0.25f ? 1 : (p < 0.75f ? 2 : 3));
        return IRFrame(IR_HIT, p < 0.25f ? 1 : (p < 0.75f ? 2 : 3)); /* tranco, tranco maximo, volta */
    }
    if (a->st == ST_ATK)
    {
        const Atk *d = &ATK[CH_IARA][a->atk < AT_SPECIAL ? a->atk : 0];
        switch (a->atk)
        {
        case AT_LIGHT:
        { /* 5: guarda, ombro, GARRA (com os riscos), recolhe, guarda. O quadro do ombro (1), com o braco atras da
             cabeca e o corpo inclinado para tras, parece um tranco de dano: o preparo fica na guarda (0) */
            int f = IaraAtkFrame(a, IR_JAB, 2, 2, d->st, d->ac, d->rc);
            return f == IRFrame(IR_JAB, 1) ? IRFrame(IR_JAB, 0) : f;
        }
        case AT_HEAVY: return IaraAtkFrame(a, IR_TAIL, 3, 4, d->st, d->ac, d->rc);   /* 7: cauda para tras, chicote alto (3-4) */
        case AT_FWDH: return IaraAtkFrame(a, IR_LONG, 3, 4, d->st, d->ac, d->rc);    /* 7: recua, mechas lancadas (3-4) */
        case AT_UPPER: return IaraAtkFrame(a, IR_UPPER, 3, 5, d->st, d->ac, d->rc);  /* 8: afunda, jato, garra no alto (3-5) */
        case AT_AIR: return IaraAtkFrame(a, IR_AIR, 2, 3, d->st, d->ac, d->rc);      /* 6: cauda recolhida, chicote para baixo */
        case AT_LOW: return IRFrame(IR_CROUCH, IR_COUNT[IR_CROUCH] - 1);             /* agachada; o chicote e desenhado */
        case AT_SPECIAL: return IaraSpecialFrame(a->atkT, SP_ST[CH_IARA], SP_AC[CH_IARA] + SP_RC[CH_IARA] - IR_SP_FLASH);
        default: break;
        }
    }
    if (a->shieldOn || a->st == ST_BLOCK)
    { /* defesa: cruza os bracos e a parede de agua sobe da poca; bloqueou: o respingo na parede */
        if (a->crouch) return IRFrame(IR_BLOCK_CROUCH, 1 + (int)(a->shieldT / 0.05f));
        if (a->shieldHit > 0) return IRFrame(IR_BLOCK_HIT, 1);
        return IRFrame(IR_BLOCK, 1 + (int)(a->shieldT / 0.05f));
    }
    if (a->st == ST_DASH) return IRFrame(IR_DASH, (int)(a->dashT / IAF_DASH_TIME * IR_COUNT[IR_DASH]));
    if (a->st == ST_JUMP)
    { /* salta da poca (1), sobe (2-3), alto (4), desce com a cauda para baixo (5-6) */
        if (a->stT < 0.06f && a->vy < 0) return IRFrame(IR_JUMP, 1);
        float k = Clamp((a->vy + IAF_JUMP_SPEED) / (2 * IAF_JUMP_SPEED), 0.0f, 1.0f);
        return IRFrame(IR_JUMP, 2 + (int)(k * 4.99f));
    }
    if (a->crouch) return IRFrame(IR_CROUCH, (int)(a->stT / 0.04f));
    if (a->st == ST_WALK) return IRLoop((a->vx * a->face) < 0 ? IR_WALK_B : IR_WALK_F, a->anim, 12);
    return IRLoop(IR_IDLE, gt, 8);
}
/* golpe baixo improvisado: chicote de agua que sai da cauda e varre rente ao chao ate a ponta do golpe */
static void IaraDrawLowWhip(Fighter *a, float jx)
{
    if (a->st != ST_ATK || a->atk != AT_LOW) return;
    const Atk *d = &ATK[CH_IARA][AT_LOW];
    float t = a->atkT, grow = Clamp(t / (d->st + d->ac * 0.5f), 0.0f, 1.0f);
    float fade = t < d->st + d->ac ? 1.0f : 1 - Clamp((t - d->st - d->ac) / d->rc, 0.0f, 1.0f);
    if (fade <= 0) return;
    Color deep = {17, 96, 106, 255}, mid = {59, 122, 129, 255}, foam = {214, 236, 238, 255};
    float x0 = a->x + jx + a->face * IRK(18), len = (d->rx - IRK(18)) * EaseOut(grow);
    const int N = 14;
    Vector2 prev = {x0, a->y - IRK(14)};
    for (int i = 1; i <= N; i++)
    {
        float u = i / (float)N;
        float h = IRK(14 + 22 * sinf(u * PI) * (1 - u * 0.4f) + 4 * sinf(gt * 30 + u * 9)); /* arco baixo que ondula */
        Vector2 p = {x0 + a->face * len * u, a->y - h};
        DrawLineEx(prev, p, IRK(16 - 8 * u), Fade(deep, 0.85f * fade));
        DrawLineEx(prev, p, IRK(9 - 4 * u), Fade(mid, 0.9f * fade));
        DrawLineEx((Vector2){prev.x, prev.y - 4}, (Vector2){p.x, p.y - 4}, 2.5f, Fade(foam, 0.7f * fade * u));
        prev = p;
    }
    if (grow >= 1 && fade > 0.5f)
        for (int k = 0; k < 3; k++) /* respingo na ponta */
            DrawCircleV((Vector2){prev.x + a->face * (6 + k * 7), prev.y - 8 - k * 6 + 3 * sinf(gt * 40 + k)}, 3.5f - k, Fade(foam, fade));
}

/* ------------------------------------------------------------------ desenho de um lutador */
/* rastro do golpe: arco translucido na cor do lutador, varrendo o caminho do membro que golpeia
   (angulos em graus a partir da frente; positivo = para baixo) */
static void DrawAttackTrail(Fighter *a, float jx)
{
    if (a->st != ST_ATK || a->atk < AT_LIGHT || a->atk > AT_UPPER) return;
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
        if (a->ch == CH_CODEX && texCodex.id) { cy = a->y - 95; r = d->rx * 0.7f; a0 = 30; a1 = -6; } /* chute alto do sprite */
        else if (a->ch == CH_CODEX) { cy = a->y - 120; r = d->rx * 0.8f; a0 = -80; a1 = 35; }  /* martelada descendo */
        else if (a->ch == CH_CLAUDE) { cy = a->y - 130; r = d->rx * 0.62f; a0 = -14; a1 = 14; } /* empurrao reto */
        else { cy = a->y - 105; r = d->rx * 0.72f; a0 = 70; a1 = -55; }                          /* chute subindo */
        break;
    case AT_LOW: cy = a->y - 70; r = d->rx * 0.68f; a0 = 20; a1 = 62; break;
    case AT_FWDH: cy = a->y - d->top + d->h * 0.5f; r = d->rx * 0.66f; a0 = -10; a1 = 10; break; /* reto e longo */
    case AT_UPPER: cy = a->y - 90; r = 110 + d->rx * 0.2f; a0 = 50; a1 = -85; break;              /* sobe em arco */
    default: cy = a->y - d->top + 20; r = d->rx * 0.7f; a0 = -25; a1 = 60; break; /* chute aereo, para baixo */
    }
    Color c = FCol(a);
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
/* poses do agarrao com os quadros que cada lutador ja tem: segura com o fraco esticado, arremessa com o soco longo */
static void GrabPose(Fighter *g)
{
    const Atk *l = &ATK[g->ch][AT_LIGHT], *h = &ATK[g->ch][AT_FWDH];
    float t = g->atkT;
    if (t < IAF_GRAB_HOLD) { g->atk = AT_LIGHT; g->atkT = l->st + l->ac * 0.5f; }
    else { g->atk = AT_FWDH; g->atkT = h->st + (t - IAF_GRAB_HOLD) / IAF_GRAB_RECOVER * (h->ac + h->rc); }
}
static void DrawFighter(Fighter *a)
{
    Fighter gp;
    if (a->st == ST_ATK && a->atk == AT_GRAB) { gp = *a; GrabPose(&gp); a = &gp; }
    float flash = a->flash > 0 ? Clamp(a->flash / 0.12f, 0.0f, 1.0f) * 0.8f : 0;
    float alt = IAF_GROUND_Y - a->y;
    float jx = a->hitShake > 0 ? (Rand01() - 0.5f) * 2 * IAF_HIT_SHAKE_PX : 0; /* tremor no impacto */
    DrawShadow(a->x, (a->ch == CH_CLAUDE && texClaude.id ? 88 : (a->ch == CH_ANTI && texAnti.id ? 104 : (a->ch == CH_CODEX && texCodex.id ? 84 : (a->ch == CH_IARA && texIara.id ? IRK(62) : 52)))) - fminf(alt, 200) * 0.12f);
    if (a->ch == CH_IARA && texIara.id && alt > 30 && a->st != ST_KO)
    { /* no ar a poca fica no chao: aneis de agua embaixo dela */
        float k = Clamp(1 - alt / 420, 0.3f, 1.0f);
        DrawEllipseLines((int)a->x, (int)IAF_GROUND_Y, (int)IRK(54 * k), (int)IRK(9 * k), Fade(CHCOL[CH_IARA], 0.8f));
        DrawEllipseLines((int)a->x, (int)IAF_GROUND_Y, (int)IRK(34 * k), (int)IRK(6 * k), Fade(CHCOL[CH_IARA], 0.5f));
    }
    if (a->inv > 0 && fmodf(a->inv, 0.1f) < 0.05f && a->st != ST_KO) return;
    if (a->st == ST_DASH)
    { /* rastro: copias apagadas para tras (sprites) e linhas de velocidade (todos) */
        Color c = FCol(a);
        for (int k = 3; k >= 1; k--)
        {
            float gx = a->x - a->face * k * 34, al = 0.32f / k;
            if (a->ch == CH_ELIRA) { float yo; DrawEF(EliraFrameIdx(a, &yo), gx, a->y, a->face, 1.0f, Fade(c, al), 0); }
            else if (a->ch == CH_CLAUDE && texClaude.id) DrawCF(ClaudeFrameIdx(a), gx, a->y, a->face, CL_SCALE, Fade(c, al), 0);
            else if (a->ch == CH_ANTI && texAnti.id) DrawAF(AntiFrameIdx(a), gx, a->y - AntiHover(a), a->face, AG_SCALE, Fade(c, al), 0);
            else if (a->ch == CH_CODEX && texCodex.id) DrawXF(CodexFrameIdx(a), gx, a->y, a->face, XC_SCALE, Fade(c, al), 0);
            else if (a->ch == CH_IARA && texIara.id) DrawIRF(IaraFrameIdx(a), gx, a->y, a->face, IR_DRAW, Fade(c, al), 0);
        }
        for (int l = 0; l < 6; l++)
        {
            float ly = a->y - 30 - l * 30, len = 60 + 40 * ((l * 37) % 5) / 4.0f;
            float x0 = a->x - a->face * 40;
            DrawLineEx((Vector2){x0, ly}, (Vector2){x0 - a->face * len, ly}, 3, Fade(c, 0.45f));
        }
    }
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
        Color tint = a->alt ? IAF_ELIRA_ALT_TINT : WHITE;
        if (a->st == ST_ATK && (a->atk == AT_LIGHT || a->atk == AT_HEAVY || a->atk == AT_FWDH || a->atk == AT_UPPER || a->atk == AT_LOW || a->atk == AT_AIR))
        { /* a arte mexe pouco o corpo: no impacto ela vai 6 px a frente e deixa 2 copias apagadas para tras */
            const Atk *d = &ATK[CH_ELIRA][a->atk];
            float t = a->atkT - d->st, act = fmaxf(d->ac, IAF_ELIRA_ACTIVE_VISUAL);
            if (t >= 0 && t < act + 0.06f)
            {
                float k = t < act ? 1.0f : 1 - (t - act) / 0.06f;
                for (int g = 2; g >= 1; g--) DrawEF(f, x - a->face * g * 14 * k, a->y + yoff, a->face, 1.0f, Fade(FCol(a), 0.34f * k / g), 0);
                x += a->face * 6 * k;
            }
        }
        DrawEF(f, x, a->y + yoff, a->face, 1.0f, tint, fl);
    }
    else if (a->ch == CH_CLAUDE && texClaude.id) DrawCF(ClaudeFrameIdx(a), a->x + jx, a->y, a->face, CL_SCALE, a->alt ? IAF_CLAUDE_ALT_TINT : WHITE, flash);
    else if (a->ch == CH_ANTI && texAnti.id) DrawAF(AntiFrameIdx(a), a->x + jx, a->y - AntiHover(a), a->face, AG_SCALE, a->alt ? IAF_ANTI_ALT_TINT : WHITE, flash);
    else if (a->ch == CH_CODEX && texCodex.id) DrawXF(CodexFrameIdx(a), a->x + jx, a->y, a->face, XC_SCALE, a->alt ? IAF_CODEX_ALT_TINT : WHITE, flash);
    else if (a->ch == CH_IARA && texIara.id)
    {
        IaraDrawLowWhip(a, jx); /* o chicote sai de tras da cauda */
        DrawIRF(IaraFrameIdx(a), a->x + jx, a->y, a->face, IR_DRAW, a->alt ? IAF_IARA_ALT_TINT : WHITE, flash);
    }
    else
    {
        Pose p = PoseFor(a);
        g_altPal = a->alt;
        DrawPuppet(a->ch, a->x + jx, a->y, a->face, 1.0f, p, flash, a->vx);
        g_altPal = false;
    }
    if (!(a->ch == CH_ANTI && texAnti.id) && !(a->ch == CH_IARA && texIara.id)) DrawAttackTrail(a, jx); /* os golpes do Antigravity e da Iara ja trazem o rastro no desenho */
    /* defesa das IAs: placa hexagonal na frente (a Elira e o Antigravity tem quadros proprios) */
    if (a->st == ST_BLOCK && a->ch != CH_ELIRA && !(a->ch == CH_ANTI && texAnti.id) && !(a->ch == CH_IARA && texIara.id)) /* a Iara ergue a parede de agua */
    {
        Color c = FCol(a);
        float cy = a->crouch ? a->y - 60 : a->y - 120, cx = a->x + a->face * 52;
        if (a->ch == CH_CLAUDE && texClaude.id) { cy = a->crouch ? a->y - 68 : a->y - 105; cx = a->x + a->face * 130; } /* na frente do braco erguido */
        if (a->ch == CH_CODEX && texCodex.id) { cy = a->crouch ? a->y - 80 : a->y - 112; cx = a->x + a->face * (XC_FRONT + 22); } /* na frente das luvas */
        DrawPoly((Vector2){cx, cy}, 6, 40, 0, Fade(c, 0.18f));
        DrawPolyLinesEx((Vector2){cx, cy}, 6, 40, 0, 3, Fade(c, 0.85f));
        DrawPolyLinesEx((Vector2){cx, cy}, 6, 28, 0, 1.5f, Fade(c, 0.5f));
    }
    /* Claude: prisma refletor */
    if (a->ch == CH_CLAUDE && a->st == ST_ATK && a->atk == AT_SPECIAL)
    {
        float t = a->atkT, st = SP_ST[CH_CLAUDE], ac = SP_AC[CH_CLAUDE];
        float vis = t < st ? t / st : (t < st + ac ? 1 : 1 - (t - st - ac) / SP_RC[CH_CLAUDE]);
        float cx = a->x + a->face * CL_FRONT, cy = a->y - 120;
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
        Vector2 o = (Vector2){a->x + a->face * CL_FRONT, a->y - 120};
        Color bands[3] = {{255, 140, 100, 255}, {255, 220, 130, 255}, {255, 255, 255, 255}};
        for (int i = 0; i < 3; i++)
            DrawLineEx(Vector2Add(o, (Vector2){0, (i - 1) * 6}), Vector2Add(a->beamTo, (Vector2){0, (i - 1) * 10}), (10 - i * 3) * (a->beamT / 0.35f) + 1, Fade(bands[i], a->beamT / 0.35f));
    }
}

/* ------------------------------------------------------------------ cenario: arena do nucleo
   (desenho da arena e do HUD em iafight_arena.h, incluido mais abaixo, depois do estado da luta) */

/* ------------------------------------------------------------------ jogo: estado */
typedef enum { SC_MODE, SC_SELECT, SC_ARENA, SC_VERSUS, SC_FIGHT, SC_RESULT, SC_FINAL, SC_CONTROLS } IafScreen;
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

typedef struct { float t, hold; int mode; bool reacted, low; IafInput once; float skill, shotCd; bool chainTried, techTried; } Ai;
enum { AI_IDLE, AI_FWD, AI_BACK, AI_BLOCK, AI_JUMPIN };
static Ai ai[2];
static bool cpu[2] = {false, true};
/* TREINO: o jogador escolhe o lutador e o boneco; vida infinita, carga cheia, cronometro parado */
static bool trainMode = false;
static int trainPick = 0;           /* selecao do treino: 0 = lutador, 1 = boneco */
static int trainDummy = CH_CLAUDE;  /* lutador do boneco */
static int dummyMode = 0;           /* boneco: 0 parado, 1 defende sempre, 2 pula, 3 luta (a IA joga com ele) */
#define DUMMY_MODES 4
static const char *DUMMY_TXT[DUMMY_MODES] = {"PARADO", "DEFENDE SEMPRE", "PULA", "LUTA (IA)"};
static float trainLast = 0, trainComboDmg = 0; /* dano do ultimo golpe e do combo atual do jogador */
static int trainComboHits = 0;
static bool trainLastBlocked = false;
static bool showBoxes = false;      /* F1 / SELECT na luta (em qualquer modo): caixas de colisao */

static float AtkTotal(Fighter *a)
{
    if (a->atk == AT_SPECIAL) return SP_ST[a->ch] + SP_AC[a->ch] + SP_RC[a->ch];
    if (a->atk == AT_SHOT) return IAF_SHOT_STARTUP + IAF_SHOT_ACTIVE + IAF_SHOT_RECOVERY;
    if (a->atk == AT_GRAB) return IAF_GRAB_HOLD + IAF_GRAB_RECOVER;
    const Atk *d = &ATK[a->ch][a->atk];
    return d->st + d->ac + d->rc;
}
static Rectangle Hurt(Fighter *a)
{
    float w = a->ch == CH_CODEX ? 84 : (a->ch == CH_ELIRA ? 96 : 70), h = a->crouch ? (a->ch == CH_ELIRA ? 132 : 128) : (a->ch == CH_ELIRA ? 214 : 200);
    float lift = 0; /* a caixa sobe junto com o desenho que flutua */
    if (a->ch == CH_CLAUDE) { w = 174; h = a->crouch ? 120 : 153; } /* sprite reduzido (1.5x) para permitir pular por cima */
    if (a->ch == CH_ANTI) { w = 184; h = a->crouch ? 155 : 205; lift = AntiHover(a); } /* "A" de 225x192 na tela, flutuando ~16 px */
    if (a->ch == CH_CODEX) { w = 130; h = a->crouch ? 150 : 198; }  /* nuvem do sprite (~157 de largura; as bordas fofas nao contam) */
    if (a->ch == CH_IARA) { w = IRK(100); h = IRK(a->crouch ? 130 : 205); } /* tronco, cauda e poca (~210 x IAF_IARA_ESCALA com o cabelo) */
    if (a->down || a->st == ST_KO) h = 40;
    return (Rectangle){a->x - w / 2, a->y - lift - h, w, h};
}
/* meia largura do corpo para nao se sobreporem: o tronco do sprite (colunas com pelo menos 35% da
   altura), sem canhao, bracos e cabelo, que podem passar na frente do oponente */
static float BodyHalf(Fighter *a) { return a->ch == CH_CLAUDE ? 80.0f : (a->ch == CH_ANTI ? 90.0f : (a->ch == CH_CODEX ? 70.0f : (a->ch == CH_IARA ? IRK(50.0f) : 50.0f))); }
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
    memset(bufT, 0, sizeof(bufT));
    for (int k = 0; k < 2; k++) pressAge[k][0] = pressAge[k][1] = 9;
    hitstop = 0; shake = 0;
}

/* ------------------------------------------------------------------ dano */
static int Idx(Fighter *a) { return a == &fz[0] ? 0 : 1; }
static void KO(Fighter *d, int dir)
{
    if (trainMode) { d->hp = IAF_HP_MAX; return; } /* treino: vida infinita */
    d->hp = 0;
    SetState(d, ST_KO);
    d->vx = dir * 380; d->vy = -520; d->crouch = false; d->guard = false;
    hitstop = 0.18f; shake = 0.4f;
    PlaySfx(SFX_KO);
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
    if (att->atk == AT_HEAVY || att->atk == AT_FWDH || att->atk == AT_UPPER || att->atk == AT_GRAB) return IAF_HITSTOP_HEAVY;
    if (att->atk == AT_SPECIAL) return IAF_HITSTOP_SPECIAL;
    return IAF_HITSTOP_LIGHT;
}
static void PrismCounter(Fighter *cl, Fighter *att)
{
    cl->countered = true;
    cl->beamT = 0.35f;
    cl->beamTo = (Vector2){att->x, att->y - 110};
    cl->atkT = SP_ST[CH_CLAUDE] + SP_AC[CH_CLAUDE]; /* vai para a recuperacao */
    PlaySfx(SFX_SP_FIRE);
    AddText((Vector2){cl->x, cl->y - 250}, "REFLEXÃO!", C_AMBER, 26);
    ApplyHit(cl, att, CL_PRISM_COUNTER_DMG, 0.6f, 420, 0, (Vector2){att->x, att->y - 110}, true, true);
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
        PlaySfx(SFX_BLOCK);
        def->hp -= dmg * 0.12f;
        SetState(def, ST_BLOCK);
        def->stun = stun * 0.65f;
        def->vx = dir * push * 0.8f;
        def->meter = fminf(IAF_METER_MAX, def->meter + 4);
        def->shieldHit = 0.15f;
        if (trainMode && att == &fz[0]) { trainLast = dmg * 0.12f; trainLastBlocked = true; }
        if (def->ch != CH_ELIRA) BlockSpark(at, FCol(def), dir); /* a faisca do escudo da Elira ja vem no quadro */
        hitstop = fmaxf(hitstop, HitstopFor(att) * 0.7f);
        def->hitShake = hitstop;
        if (def->hp <= 0) KO(def, dir);
        return;
    }
    att->combo = (att->comboT > 0) ? att->combo + 1 : 1;
    att->comboT = 0.9f;
    if (att->combo >= IAF_COMBO_SCALE_FROM)
    { /* combo longo: do 4o acerto em diante cada golpe tira menos (acumula), com piso */
        float f = powf(1.0f - IAF_COMBO_SCALE_STEP, (float)(att->combo - IAF_COMBO_SCALE_FROM + 1));
        dmg *= fmaxf(f, IAF_COMBO_SCALE_MIN);
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
    if (trainMode && att == &fz[0])
    { /* treino: dano do golpe e do combo */
        trainLast = dmg; trainLastBlocked = false;
        trainComboDmg = att->combo > 1 ? trainComboDmg + dmg : dmg;
        trainComboHits = att->combo;
    }
    /* o contador de acertos aparece no HUD, ao lado da barra de quem ataca (DrawComboTag) */
    if (def->ch != CH_ELIRA)
    { /* sem efeito por cima dos quadros de reacao da Elira */
        if (att->ch == CH_ELIRA) AddImpact(at, 1.6f + dmg / 120.0f, dir);
        HitSparks(at, att->ch == CH_ELIRA ? C_CYAN : FCol(att), 10 + (int)(dmg / 10), 420);
    }
    /* estrela de impacto no ponto do acerto (menor sobre a Elira, cujos quadros de dano ja tem efeito) */
    ImpactSpark(at, att->ch == CH_ELIRA ? C_CYAN : FCol(att), (28 + dmg * 0.22f) * (def->ch == CH_ELIRA ? 0.6f : 1.0f) * (att->ch == CH_ELIRA ? 1.3f : 1.0f));
    hitstop = fmaxf(hitstop, HitstopFor(att));
    PlaySfx(HitstopFor(att) >= IAF_HITSTOP_HEAVY ? SFX_HEAVY : SFX_LIGHT);
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
            proj[i] = (Proj){true, a->x + a->face * (big ? IAF_MUZZLE_X + 25 : IAF_MUZZLE_X), a->y - IAF_MUZZLE_Y, a->face * (big ? 560.0f : 720.0f), Idx(a), big, false, big ? 55.0f : 70.0f, 0, 0, big ? 3 : 1, false};
            return;
        }
}
/* Canto do Rio: onda que rola rente ao chao, mais lenta que o nucleo da Elira; atravessa e acerta 3 vezes */
static void SpawnWave(Fighter *a)
{
    for (int i = 0; i < IAF_MAX_PROJ; i++)
        if (!proj[i].on)
        {
            proj[i] = (Proj){true, a->x + a->face * IR_FRONT, a->y - IRK(62), a->face * 520.0f, Idx(a), true, false, 50.0f, 0, 0, 3, true};
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
        if (t >= st && !a->spawned) { a->spawned = true; SpawnProj(a, true); PlaySfx(SFX_SP_FIRE); MuzzleFlash((Vector2){a->x + a->face * IAF_MUZZLE_X, a->y - IAF_MUZZLE_Y}, 44); a->vx = -a->face * 120; AddText((Vector2){a->x, a->y - 240}, "RAJADA TEMPORAL!", C_CYAN, 22); }
        break;
    case CH_CLAUDE:
        if (t >= st + ac && !a->spawned && !a->countered)
        { /* sem ataque para refletir: pulso curto de luz */
            a->spawned = true;
            HitSparks((Vector2){a->x + a->face * CL_FRONT, a->y - 120}, C_AMBER, 14, 300);
            PlaySfx(SFX_SP_FIRE);
            if (fabsf(o->x - a->x) < CL_FRONT + 96 && fabsf(o->y - a->y) < 140) ApplyHit(a, o, 60, 0.35f, 260, 0, (Vector2){o->x, o->y - 120}, false, false);
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
                else { a->caught = true; a->spHits = 0; a->spT = 0; PlaySfx(SFX_SP_FIRE); }
            }
        }
        if (a->caught)
        {
            a->vx = 0;
            a->atkT = fminf(a->atkT, st + ac - 0.01f); /* segura a janela enquanto a sequencia roda */
            a->spT += dt;
            o->x = a->x + a->face * fmaxf(104, BodyHalf(a) + BodyHalf(o) + 4); /* preso na frente da nuvem, sem atravessar */
            if (o->x > IAF_WALL_R) { a->x -= o->x - IAF_WALL_R; o->x = IAF_WALL_R; } /* na parede, quem recua e o Codex */
            if (o->x < IAF_WALL_L) { a->x += IAF_WALL_L - o->x; o->x = IAF_WALL_L; }
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
    case CH_IARA:
        if (t < st && (int)(t * 60) % 3 == 0) /* canto: bolhas sobem da poca */
            AddPart((Vector2){a->x + (Rand01() - 0.5f) * 90, a->y - 10}, (Vector2){(Rand01() - 0.5f) * 40, -160 - Rand01() * 120}, 0.5f, 2 + Rand01() * 3, Rand01() < 0.5f ? CHCOL[CH_IARA] : C_WHITE, 1);
        if (t >= st && !a->spawned)
        {
            a->spawned = true;
            SpawnWave(a);
            PlaySfx(SFX_SP_FIRE);
            a->vx = -a->face * 90; /* recuo do grito */
            AddText((Vector2){a->x, a->y - 240}, "CANTO DO RIO!", CHCOL[CH_IARA], 22);
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
            PlaySfx(SFX_SP_FIRE);
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
            ApplyHit(a, o, AG_WELL_SLAM_DMG, 0.7f, 120, 0, (Vector2){o->x, o->y - 100}, true, true);
            o->vy = 900; /* derrubada para baixo */
        }
        well.on = false;
    }
}

/* ------------------------------------------------------------------ atualizacao de um lutador */
/* combo pela janela de cancelamento: o oponente segue atordoado ate o golpe seguinte sair (o fraco atordoa
   pouco, entao sem isso o 2o fraco apertado no fim da janela e o forte final nao pegariam) */
static void ChainStun(Fighter *a, Fighter *o)
{
    if (o->st == ST_HIT) o->stun = fmaxf(o->stun, ATK[a->ch][a->atk].st + IAF_CHAIN_STUN_PAD);
}
static void StartAtk(Fighter *a, AtkKind k)
{
    a->atk = k; a->atkT = 0; a->hitDone = false; a->spawned = false; a->caught = false; a->countered = false;
    a->chain = k == AT_LIGHT ? 1 : 0; a->cancelT = 0;
    if (a->st != ST_JUMP) a->vx = 0;
    SetState(a, ST_ATK);
}
/* ------------------------------------------------------------------ agarrao
   Fraco + forte juntos (ou LT / L2) colado: nao pode ser defendido. Segura por IAF_GRAB_HOLD s (quem foi
   agarrado escapa apertando fraco + forte juntos nesse tempo) e arremessa o oponente para tras, derrubado.
   Poses: quem agarra usa o quadro esticado do fraco e depois o soco longo; quem e agarrado, o de dano. */
static bool CanGrab(Fighter *a, Fighter *o)
{
    if (!OnGround(a) || !OnGround(o) || o->st == ST_KO || o->st == ST_HIT || o->st == ST_WIN || o->st == ST_LOSE) return false;
    if (o->down || o->inv > 0 || o->grabbed || (well.on && well.caught)) return false;
    return fabsf(o->x - a->x) <= BodyHalf(a) + BodyHalf(o) + IAF_GRAB_RANGE;
}
static void StartGrab(Fighter *a, Fighter *o)
{
    StartAtk(a, AT_GRAB);
    a->vx = 0; a->crouch = false;
    a->face = o->x > a->x ? 1 : -1;
    o->grabbed = true;
    o->atk = AT_NONE; o->caught = false;
    SetState(o, ST_HIT);
    o->stun = 9; o->vx = 0; o->vy = 0;
    o->crouch = o->guard = o->shieldOn = o->hitCrouch = false;
    PlaySfx(SFX_LIGHT);
    AddText((Vector2){(a->x + o->x) / 2, a->y - 260}, "AGARRÃO!", C_AMBER, 26);
}
static void UpdateGrab(Fighter *a, Fighter *o)
{
    if (a->atkT < IAF_GRAB_HOLD)
    { /* segurando: o oponente fica colado na frente */
        if (o->grabbed) { o->x = a->x + a->face * (BodyHalf(a) + BodyHalf(o)); o->vx = 0; }
        return;
    }
    if (a->hitDone || !o->grabbed) return;
    a->hitDone = true;
    o->grabbed = false;
    o->stun = 0;
    ApplyHit(a, o, IAF_GRAB_DMG, 0.5f, IAF_GRAB_PUSH, 0, (Vector2){o->x - a->face * 20, o->y - 120}, true, true);
    if (o->st == ST_HIT) { o->vy = -IAF_GRAB_VY; o->vx = a->face * IAF_GRAB_PUSH; } /* para tras, e cai derrubado */
}
/* quem foi agarrado (k) apertou fraco + forte juntos a tempo: os dois se soltam */
static void GrabEscape(int k)
{
    Fighter *d = &fz[k], *a = &fz[1 - k];
    int dir = d->x > a->x ? 1 : -1;
    d->grabbed = false;
    SetState(d, ST_HIT); d->stun = 0.2f; d->vx = dir * 320;
    a->atk = AT_NONE;
    SetState(a, ST_HIT); a->stun = 0.2f; a->vx = -dir * 320;
    memset(bufT[k], 0, sizeof bufT[k]);
    PlaySfx(SFX_BLOCK);
    AddText((Vector2){(a->x + d->x) / 2, d->y - 260}, "ESCAPOU!", C_CYAN, 26);
}
static void UpdateFighter(Fighter *a, Fighter *o, IafInput in, float dt)
{
    if (a->grabbed && !(o->st == ST_ATK && o->atk == AT_GRAB)) { a->grabbed = false; a->stun = fminf(a->stun, 0.15f); } /* quem agarrava foi interrompido */
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
    /* dash: dois toques para frente dentro da janela, ou o botao de dash */
    bool fwdP = (a->face > 0 && in.rightP) || (a->face < 0 && in.leftP);
    bool backP = (a->face > 0 && in.leftP) || (a->face < 0 && in.rightP);
    bool dashReq = in.dashP;
    if (fwdP) { if (a->tapT > 0) dashReq = true; a->tapT = dashReq ? 0 : IAF_DASH_TAP; }
    else if (backP) a->tapT = 0;
    else if (a->tapT > 0) a->tapT -= dt;

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
    case ST_DASH: /* investida para frente, sem atacar */
        a->dashT += dt;
        a->vx = a->face * IAF_DASH_DIST / IAF_DASH_TIME;
        if (a->dashT >= IAF_DASH_TIME) { a->vx = 0; SetState(a, ST_IDLE); }
        break;
    case ST_ATK:
    {
        if (in.grabP && a->atk != AT_GRAB && a->atkT <= IAF_GRAB_TOGETHER + 0.02f && !a->hitDone && CanGrab(a, o)) { StartGrab(a, o); break; } /* o 2o botao veio logo depois */
        if (a->cancelT > 0)
        { /* combo: fraco que acertou cancela no 2o fraco; o 2o fraco cancela no forte */
            a->cancelT -= dt;
            if (a->chain == 1 && in.lightP) { StartAtk(a, AT_LIGHT); a->chain = 2; ChainStun(a, o); break; }
            if (a->chain == 2 && in.heavyP) { StartAtk(a, in.up ? AT_UPPER : (holdFwd ? AT_FWDH : AT_HEAVY)); a->chain = 3; ChainStun(a, o); break; }
        }
        a->atkT += dt;
        if (ground && (a->atk == AT_LIGHT || a->atk == AT_HEAVY || a->atk == AT_LOW || a->atk == AT_FWDH || a->atk == AT_UPPER))
        { /* avanco do corpo durante o preparo e o ativo do golpe */
            const Atk *d = &ATK[a->ch][a->atk];
            float win = d->st + d->ac, t0 = a->atkT - dt;
            float adv = a->atk == AT_LIGHT ? IAF_LUNGE_LIGHT : (a->atk == AT_HEAVY ? IAF_LUNGE_HEAVY : (a->atk == AT_FWDH ? IAF_LUNGE_FWDH : (a->atk == AT_UPPER ? IAF_LUNGE_LIGHT : IAF_LUNGE_LOW)));
            if (a->ch == CH_CODEX) adv *= XC_LUNGE_MULT; /* pressao curta: entra no golpe com o corpo todo */
            if (a->ch == CH_ELIRA) adv *= IAF_ELIRA_LUNGE_MUL; /* arte com pouco movimento: avanca mais */
            if (t0 < win) a->x += a->face * adv * fminf(dt, win - t0) / win;
        }
        if (ground && a->ch == CH_CLAUDE && (a->atk == AT_FWDH || a->atk == AT_HEAVY) && OnGround(o) && o->st != ST_KO)
        { /* o punho/pe do sprite vai ate CL_STRIKE_REACH: colado, o Claude recua no preparo para o golpe
             terminar dentro do corpo do oponente em vez de atravessa-lo (na parede, quem recua e o oponente) */
            const Atk *d = &ATK[a->ch][a->atk];
            float need = CL_STRIKE_REACH - BodyHalf(o) - 6, dist = (o->x - a->x) * a->face;
            if (dist > 0 && dist < need && a->atkT < d->st + d->ac)
            {
                float left = d->st - a->atkT, gap = need - dist;
                float mv = left > dt ? gap * dt / left : gap;
                float back = a->x - a->face * mv;
                if (back < IAF_WALL_L || back > IAF_WALL_R)
                {
                    float can = a->face > 0 ? a->x - IAF_WALL_L : IAF_WALL_R - a->x;
                    a->x -= a->face * can;
                    o->x += a->face * (mv - can);
                    o->x = Clamp(o->x, IAF_WALL_L, IAF_WALL_R);
                }
                else a->x = back;
            }
        }
        if (a->atk == AT_SPECIAL) UpdateSpecial(a, o, dt);
        else if (a->atk == AT_GRAB) UpdateGrab(a, o);
        else if (a->atk == AT_SHOT)
        {
            if (a->atkT >= IAF_SHOT_STARTUP && !a->spawned)
            {
                a->spawned = true;
                SpawnProj(a, false);
                PlaySfx(SFX_SHOT);
                a->shotCd = IAF_SHOT_COOLDOWN;
                MuzzleFlash((Vector2){a->x + a->face * IAF_MUZZLE_X, a->y - IAF_MUZZLE_Y}, 26);
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
                    Vector2 at = (Vector2){o->x - a->face * (Hurt(o).width * 0.5f - 7), top + d->h * 0.5f}; /* na borda de quem apanha */
                    bool up = a->atk == AT_UPPER;
                    float sc = a->chain == 2 ? IAF_CHAIN_SCALE2 : (a->chain == 3 ? IAF_CHAIN_SCALE3 : 1.0f); /* dano do combo escalonado */
                    ApplyHit(a, o, d->dmg * sc, d->stun, d->push, d->type, at, false, up || (a->atk == AT_HEAVY && a->ch == CH_CLAUDE && !OnGround(o)));
                    if (o->st == ST_HIT && (a->chain == 1 || a->chain == 2)) a->cancelT = IAF_CHAIN_WINDOW;
                    if (o->st == ST_HIT && a->chain == 3) AddText((Vector2){o->x, o->y - 250}, "COMBO!", C_AMBER, 30);
                    if (up && o->st == ST_HIT) o->vy = -IAF_UPPER_LAUNCH; /* acertou (nao defendeu): sobe mais */
                }
            }
        }
        if (a->st == ST_ATK && a->atkT >= AtkTotal(a))
        {
            if (a->atk == AT_AIR && !ground) { a->st = ST_JUMP; }
            else SetState(a, ST_IDLE);
            a->atk = AT_NONE;
        }
        /* pousou: o aereo acaba; a pisada do Antigravity (que mergulha) termina a recuperacao no chao, senao
           pular e pisar colado acertava a cada 8 quadros, antes de o oponente sair do atordoamento */
        if (a->atk == AT_AIR && ground && a->vy >= 0 && a->stT > 0.05f && !(a->ch == CH_ANTI && a->atkT < AtkTotal(a))) { SetState(a, ST_IDLE); a->atk = AT_NONE; }
        else if (a->atk == AT_AIR && ground && a->vy >= 0) a->vx = 0; /* pisada: parada no chao ate recuperar */
        if (a->atk != AT_SPECIAL && a->atk != AT_AIR && ground) a->vx *= 1 - 10 * dt;
        if (a->atk == AT_SPECIAL && a->ch != CH_CODEX) a->vx *= 1 - 10 * dt;
        if (a->atk == AT_SPECIAL && a->ch == CH_CODEX && a->atkT > SP_ST[CH_CODEX] + SP_AC[CH_CODEX]) a->vx *= 1 - 12 * dt;
    }
    break;
    case ST_JUMP:
        if (in.heavyP && a->stT < IAF_UPPER_CANCEL && !a->airAtkUsed)
        { /* cima + forte quase juntos: o pulo que acabou de sair vira uppercut */
            a->y = IAF_GROUND_Y; a->vy = 0;
            StartAtk(a, AT_UPPER);
            break;
        }
        if ((in.lightP || in.heavyP) && !a->airAtkUsed)
        {
            a->airAtkUsed = true; a->atk = AT_AIR; a->atkT = 0; a->hitDone = false; a->st = ST_ATK; a->stT = 0;
            if (a->ch == CH_ANTI) a->vy = fmaxf(a->vy, IAF_ANTI_SLAM_VY); /* ground slam: mergulha com a gravidade */
        }
        break;
    default: /* parado / andando / agachado */
        a->crouch = ground && in.down;
        if (in.grabP && CanGrab(a, o)) { StartGrab(a, o); break; } /* fora de alcance, sai o golpe fraco */
        if (in.specialP && a->meter >= IAF_SPECIAL_COST && !(a->ch == CH_ANTI && well.on)) { a->meter -= IAF_SPECIAL_COST; a->crouch = false; StartAtk(a, AT_SPECIAL); PlaySfx(SFX_SP_CHARGE); break; }
        if (in.shotP && a->ch == CH_ELIRA && a->shotCd <= 0 && !EliraShotOnScreen(a)) { a->crouch = false; StartAtk(a, AT_SHOT); break; }
        if (in.lightP) { StartAtk(a, a->crouch ? AT_LOW : AT_LIGHT); break; }
        if (in.heavyP)
        { /* agachado: rasteira; cima: uppercut; frente: soco longo; parado: forte */
            StartAtk(a, a->crouch ? AT_LOW : (in.up ? AT_UPPER : (holdFwd ? AT_FWDH : AT_HEAVY)));
            break;
        }
        if (dashReq && ground && !a->crouch)
        {
            a->dashT = 0;
            a->vx = a->face * IAF_DASH_DIST / IAF_DASH_TIME;
            SetState(a, ST_DASH);
            PlaySfx(SFX_DASH);
            break;
        }
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
            float sp = a->ch == CH_ANTI ? 290 : (a->ch == CH_CODEX ? 300 : 255);
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
        if (a->ch == CH_ANTI && a->st == ST_JUMP && !ground && in.up && a->vy > 0 && a->hoverT < 0.9f) { g = IAF_GRAVITY * 0.12f; a->hoverT += dt; } /* plana */
        if (!ground || a->vy < 0) a->vy += g * dt;
        if (a->ch == CH_ANTI && a->st == ST_JUMP && !ground && in.up && a->vy > 120 && a->hoverT < 0.9f) a->vy = 120;
        a->x += a->vx * dt;
        a->y += a->vy * dt;
        if (a->y >= IAF_GROUND_Y)
        {
            a->juggle = 0;
            bool wasAir = a->vy > 300;
            a->y = IAF_GROUND_Y;
            if (a->st == ST_KO) { a->vy = 0; a->vx *= 0.5f; }
            else if (a->st == ST_HIT && wasAir && a->stT > 0.05f) { a->down = true; PlaySfx(SFX_FALL); a->downT = 0.75f; a->vy = 0; shake = fmaxf(shake, 0.15f); HitSparks((Vector2){a->x, IAF_GROUND_Y}, C_DIM, 8, 200); }
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
    a->groundT = OnGround(a) ? a->groundT + dt : 0;
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
        if (p->big && (int)(p->t * 60) % 2 == 0) AddPart((Vector2){p->x - (p->vx > 0 ? 40 : -40), p->y + (Rand01() - 0.5f) * 30 + (p->wave ? 40 : 0)}, (Vector2){-p->vx * 0.2f, p->wave ? -60 : 0}, 0.35f, 3, p->wave ? (Color){214, 236, 238, 255} : C_CYAN, 1);
        Fighter *o = &fz[1 - p->owner];
        float r = p->wave ? IRK(38) : (p->big ? 38 : 20); /* a onda acompanha o tamanho da Iara */
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
                p->dmg *= CL_REFLECT_MULT;
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
            if (p->hits <= 0)
            {
                p->on = false;
                if (p->wave && texIara.id) AddImpactWave((Vector2){p->x, p->y}, p->vx > 0 ? 1 : -1);
                else AddImpact((Vector2){p->x, p->y}, p->big ? 3.4f : 2.0f, p->vx > 0 ? 1 : -1);
            }
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
            if (impacts[i].kind == 1) { DrawIRFC(IRFrame(IR_IMPACT, (int)(impacts[i].t / 0.32f * IR_COUNT[IR_IMPACT])), impacts[i].p, impacts[i].face, IR_DRAW, WHITE); continue; }
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
        if (p->wave && texIara.id)
        { /* onda da Iara: rola em loop, com um rastro apagado */
            int f = IRLoop(IR_PROJ, p->t, 12);
            for (int k = 2; k >= 1; k--) DrawIRFC(f, (Vector2){p->x - face * k * IRK(30), p->y}, face, IR_DRAW, Fade(tint, 0.22f / k));
            DrawIRFC(f, (Vector2){p->x, p->y}, face, IR_DRAW, tint);
            continue;
        }
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
    if (a->cancelT > 0 && !s->chainTried)
    { /* fraco acertou: as vezes segue o combo */
        s->chainTried = true;
        if (Rand01() < 0.2f + s->skill * 0.5f) { if (a->chain == 1) s->once.lightP = true; else s->once.heavyP = true; }
    }
    if (a->cancelT <= 0) s->chainTried = false;
    if (a->grabbed && !s->techTried && a->stT > 0.08f)
    { /* agarrada: as vezes escapa */
        s->techTried = true;
        if (Rand01() < s->skill * 0.4f) s->once.grabP = true;
    }
    if (!a->grabbed) s->techTried = false;
    /* anti-aereo: oponente caindo por cima e perto -> uppercut (uma tentativa por pulo) */
    static bool aaTried[2];
    if (OnGround(o)) aaTried[me] = false;
    else if (!aaTried[me] && OnGround(a) && o->vy > -150 && dist < BodyHalf(a) + 150 && (a->st == ST_IDLE || a->st == ST_WALK))
    {
        aaTried[me] = true;
        if (Rand01() < s->skill * 0.6f) { s->once.heavyP = true; s->once.up = true; s->t = 0.3f; }
    }
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
        bool prism = (a->ch == CH_ELIRA || a->ch == CH_IARA) && o->ch == CH_CLAUDE && o->meter >= IAF_SPECIAL_COST; /* tiro, rajada e onda voltariam refletidos */
        if (dist > 420)
        {
            if (a->ch == CH_ELIRA && !prism && s->shotCd <= 0 && r < 0.5f) { s->once.shotP = true; s->shotCd = 1.3f; }
            else if (sp && a->ch == CH_ELIRA && !prism && r < 0.7f) s->once.specialP = true;
            else if (sp && a->ch == CH_IARA && !prism && r < 0.6f) s->once.specialP = true; /* onda de longe */
            else if (sp && a->ch == CH_ANTI && r < 0.35f && dist < 560) s->once.specialP = true;
            else if (sp && a->ch == CH_CODEX && r < 0.3f && dist < 560) s->once.specialP = true;
            else { s->mode = AI_FWD; s->t = 0.3f + Rand01() * 0.4f; if (Rand01() < 0.3f) s->once.dashP = true; }
        }
        else if (dist > fmaxf(165, BodyHalf(a) + BodyHalf(o) + 22)) /* com corpos largos, ainda ha distancia para atacar */
        {
            if (r < 0.12f && dist < ATK[a->ch][AT_FWDH].rx + BodyHalf(o) + 30) { s->mode = AI_FWD; s->t = 0.12f; s->once.heavyP = true; } /* soco longo */
            else if (r < 0.45f) { s->mode = AI_FWD; s->t = 0.2f + Rand01() * 0.3f; }
            else if (r < 0.6f) { s->mode = AI_JUMPIN; s->once.upP = true; s->t = 0.7f; }
            else if (a->ch == CH_ELIRA && !prism && s->shotCd <= 0 && r < 0.8f) { s->once.shotP = true; s->shotCd = 1.3f; }
            else if (sp && a->ch == CH_ANTI && r < 0.85f) s->once.specialP = true;
            else if (sp && a->ch == CH_CODEX && r < 0.85f) s->once.specialP = true;
            else if (sp && a->ch == CH_IARA && !prism && r < 0.8f) s->once.specialP = true;
            else if (r < 0.9f) { s->mode = AI_BACK; s->t = 0.25f; }
        }
        else
        {
            if ((o->shieldOn || o->st == ST_BLOCK) && Rand01() < 0.2f + s->skill * 0.3f) s->once.grabP = true; /* parede de defesa: agarra */
            else if (r < 0.32f) s->once.lightP = true;
            else if (r < 0.5f) s->once.heavyP = true;
            else if (r < 0.64f) { s->mode = AI_IDLE; s->low = true; s->once.lightP = true; }
            else if (r < 0.78f) { s->mode = AI_BLOCK; s->t = 0.4f; }
            else if (r < 0.88f) { s->mode = AI_BACK; s->t = 0.3f; }
            else if (sp && a->ch != CH_ANTI && !prism) s->once.specialP = true;
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
    if (s->once.up) in.up = true; /* cima segurado sem pular (uppercut) */
    if (s->once.dashP) in.dashP = true;
    if (s->once.shotP) in.shotP = true;
    if (s->once.specialP) in.specialP = true;
    if (s->once.grabP) in.grabP = true;
    if (s->once.upP) { in.upP = true; in.up = true; }
    memset(&s->once, 0, sizeof(s->once));
    if ((a->ch == CH_ELIRA || a->ch == CH_IARA) && o->ch == CH_CLAUDE && o->st == ST_ATK && o->atk == AT_SPECIAL)
    { /* prisma de pe: golpe ou tiro so voltam como feixe/reflexo; ela espera defendendo e, colada, agarra */
        in.lightP = in.heavyP = in.shotP = in.specialP = in.upP = in.dashP = false;
        in.left = o->x > a->x; in.right = !in.left; in.up = false;
        if (fabsf(o->x - a->x) <= BodyHalf(a) + BodyHalf(o) + IAF_GRAB_RANGE) in.grabP = true; /* o agarrao passa pelo prisma */
    }
    return in;
}
/* ------------------------------------------------------------------ controles: teclado + ate 2 gamepads
   Gamepad (Xbox ou PlayStation, pelo mapeamento padrao da raylib): analogico esquerdo ou direcional
   movem; face esquerda (X / quadrado) = golpe fraco; face de cima (Y / triangulo) = golpe forte;
   face direita (B / circulo) = tiro da Elira; face de baixo (A / X) = pular; ombro ou gatilho
   direito = especial; START = pausa. O teclado continua valendo em paralelo. */
#define IAF_PADS 2
typedef struct { bool on; float ax, ay, rt, lt; bool b[32]; } PadRaw;
enum { VB_LEFT, VB_RIGHT, VB_UP, VB_DOWN, VB_LIGHT, VB_HEAVY, VB_SHOT, VB_JUMP, VB_SPECIAL, VB_START, VB_SELECT, VB_DASH, VB_GRAB, VB_N };
static PadRaw padNow[IAF_PADS], padPrev[IAF_PADS];
static bool padKnown[IAF_PADS];
static float padToastT = 0;
static char padToast[64];
static bool versusMode = false; /* 1 x 1: P1 = controle 1 + WASD; P2 = controle 2 + setas/teclado numerico */
/* gancho para testar sem controle fisico (harness): se definido, substitui a leitura do hardware */
static bool (*iafPadHook)(int pad, PadRaw *out) = NULL;

static void ReadPadRaw(int i, PadRaw *r)
{
    memset(r, 0, sizeof *r);
    if (iafPadHook) { r->on = iafPadHook(i, r); return; }
    if (!IsGamepadAvailable(i)) return;
    r->on = true;
    r->ax = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_X);
    r->ay = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_Y);
    r->rt = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_TRIGGER);
    r->lt = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_TRIGGER);
    for (int b = 1; b <= GAMEPAD_BUTTON_RIGHT_THUMB && b < 32; b++) r->b[b] = IsGamepadButtonDown(i, b);
}
/* botoes virtuais de um controle */
static void PadVB(const PadRaw *r, bool v[VB_N])
{
    memset(v, 0, sizeof(bool) * VB_N);
    if (!r->on) return;
    v[VB_LEFT] = r->b[GAMEPAD_BUTTON_LEFT_FACE_LEFT] || r->ax < -0.45f;
    v[VB_RIGHT] = r->b[GAMEPAD_BUTTON_LEFT_FACE_RIGHT] || r->ax > 0.45f;
    v[VB_UP] = r->b[GAMEPAD_BUTTON_LEFT_FACE_UP] || r->ay < -0.6f;
    v[VB_DOWN] = r->b[GAMEPAD_BUTTON_LEFT_FACE_DOWN] || r->ay > 0.55f;
    v[VB_LIGHT] = r->b[GAMEPAD_BUTTON_RIGHT_FACE_LEFT];
    v[VB_HEAVY] = r->b[GAMEPAD_BUTTON_RIGHT_FACE_UP];
    v[VB_SHOT] = r->b[GAMEPAD_BUTTON_RIGHT_FACE_RIGHT];
    v[VB_JUMP] = r->b[GAMEPAD_BUTTON_RIGHT_FACE_DOWN];
    v[VB_SPECIAL] = r->b[GAMEPAD_BUTTON_RIGHT_TRIGGER_1] || r->b[GAMEPAD_BUTTON_RIGHT_TRIGGER_2] || r->rt > 0.5f;
    v[VB_START] = r->b[GAMEPAD_BUTTON_MIDDLE_RIGHT];
    v[VB_SELECT] = r->b[GAMEPAD_BUTTON_MIDDLE_LEFT];
    v[VB_DASH] = r->b[GAMEPAD_BUTTON_LEFT_TRIGGER_1]; /* ombro esquerdo: LB / L1 */
    v[VB_GRAB] = r->b[GAMEPAD_BUTTON_LEFT_TRIGGER_2] || r->lt > 0.5f; /* gatilho esquerdo: LT / L2 = agarrao */
}
static bool PadDown(int i, int vb)
{
    bool v[VB_N];
    PadVB(&padNow[i], v);
    return v[vb];
}
static bool PadPressed(int i, int vb)
{
    bool v[VB_N], p[VB_N];
    PadVB(&padNow[i], v);
    PadVB(&padPrev[i], p);
    return v[vb] && !p[vb];
}
static bool AnyPadPressed(int vb)
{
    for (int i = 0; i < IAF_PADS; i++)
        if (PadPressed(i, vb)) return true;
    return false;
}
/* botao do controle sem botao virtual (cliques dos analogicos, usados no treino) */
static bool AnyPadRawPressed(int btn)
{
    for (int i = 0; i < IAF_PADS; i++)
        if (padNow[i].on && padNow[i].b[btn] && !padPrev[i].b[btn]) return true;
    return false;
}
/* o controle i esta sendo usado por um jogador humano na luta atual? */
static bool PadInUse(int i)
{
    if (versusMode) return true;
    return !cpu[0] && (i == 0 || !padNow[0].on);
}
static void PollPads(float dt)
{
    for (int i = 0; i < IAF_PADS; i++)
    {
        padPrev[i] = padNow[i];
        ReadPadRaw(i, &padNow[i]);
        if (padNow[i].on != padKnown[i])
        { /* conectou ou desconectou agora */
            padKnown[i] = padNow[i].on;
            snprintf(padToast, sizeof padToast, "CONTROLE %d %s", i + 1, padNow[i].on ? "CONECTADO" : "DESCONECTADO");
            padToastT = 2.5f;
            if (!padNow[i].on && iafScreen == SC_FIGHT && rstate == RS_FIGHT && PadInUse(i)) paused = true;
        }
    }
    if (padToastT > 0) padToastT -= dt;
}

typedef enum { KB_ALL, KB_P1, KB_P2 } KbSet;
/* entrada de um jogador: um conjunto de teclas + um controle (pad = -1: qualquer controle) */
static IafInput ReadPlayer(KbSet kb, int pad)
{
    IafInput in = {0};
    if (kb == KB_ALL || kb == KB_P1)
    {
        in.left = IsKeyDown(KEY_A);
        in.right = IsKeyDown(KEY_D);
        in.up = IsKeyDown(KEY_W);
        in.upP = IsKeyPressed(KEY_W);
        in.down = IsKeyDown(KEY_S);
        in.lightP = IsKeyPressed(KEY_J);
        in.heavyP = IsKeyPressed(KEY_K);
        in.specialP = IsKeyPressed(KEY_L);
        in.shotP = IsKeyPressed(KEY_I);
        in.leftP = IsKeyPressed(KEY_A);
        in.rightP = IsKeyPressed(KEY_D);
    }
    if (kb == KB_ALL || kb == KB_P2)
    {
        in.left = in.left || IsKeyDown(KEY_LEFT);
        in.right = in.right || IsKeyDown(KEY_RIGHT);
        in.up = in.up || IsKeyDown(KEY_UP);
        in.upP = in.upP || IsKeyPressed(KEY_UP);
        in.down = in.down || IsKeyDown(KEY_DOWN);
        in.leftP = in.leftP || IsKeyPressed(KEY_LEFT);
        in.rightP = in.rightP || IsKeyPressed(KEY_RIGHT);
    }
    if (kb == KB_ALL)
    {
        in.lightP = in.lightP || IsKeyPressed(KEY_Z);
        in.heavyP = in.heavyP || IsKeyPressed(KEY_X);
        in.specialP = in.specialP || IsKeyPressed(KEY_C);
        in.shotP = in.shotP || IsKeyPressed(KEY_V);
    }
    if (kb == KB_P2)
    { /* teclado numerico: 1 fraco, 2 forte, 3 especial, 4 tiro */
        in.lightP = IsKeyPressed(KEY_KP_1);
        in.heavyP = IsKeyPressed(KEY_KP_2);
        in.specialP = IsKeyPressed(KEY_KP_3);
        in.shotP = IsKeyPressed(KEY_KP_4);
    }
    for (int i = 0; i < IAF_PADS; i++)
    {
        if ((pad >= 0 && i != pad) || !padNow[i].on) continue;
        in.left = in.left || PadDown(i, VB_LEFT);
        in.right = in.right || PadDown(i, VB_RIGHT);
        in.down = in.down || PadDown(i, VB_DOWN);
        in.up = in.up || PadDown(i, VB_UP) || PadDown(i, VB_JUMP);
        in.upP = in.upP || PadPressed(i, VB_UP) || PadPressed(i, VB_JUMP);
        in.lightP = in.lightP || PadPressed(i, VB_LIGHT);
        in.heavyP = in.heavyP || PadPressed(i, VB_HEAVY);
        in.shotP = in.shotP || PadPressed(i, VB_SHOT);
        in.specialP = in.specialP || PadPressed(i, VB_SPECIAL);
        in.leftP = in.leftP || PadPressed(i, VB_LEFT);
        in.rightP = in.rightP || PadPressed(i, VB_RIGHT);
        in.dashP = in.dashP || PadPressed(i, VB_DASH);
        in.grabP = in.grabP || PadPressed(i, VB_GRAB);
    }
    return in;
}
/* entrada do jogador humano no lado 'slot' (0 = esquerda, 1 = direita) */
static IafInput HumanInput(int slot)
{
    if (!versusMode) return ReadPlayer(KB_ALL, -1);
    return slot == 0 ? ReadPlayer(KB_P1, 0) : ReadPlayer(KB_P2, 1);
}
/* navegacao de menus: qualquer teclado ou controle */
static bool AltDown(void) { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }
static bool NavLeftP(void) { return IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) || AnyPadPressed(VB_LEFT); }
static bool NavRightP(void) { return IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) || AnyPadPressed(VB_RIGHT); }
static bool NavOkP(void) { return (IsKeyPressed(KEY_ENTER) && !AltDown()) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_J) || AnyPadPressed(VB_JUMP) || AnyPadPressed(VB_START); }
static bool NavBackP(void) { return IsKeyPressed(KEY_ESCAPE) || AnyPadPressed(VB_SHOT); }
static bool PauseP(void) { return IsKeyPressed(KEY_P) || AnyPadPressed(VB_START); }
static bool NavUpP(void) { return IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || AnyPadPressed(VB_UP); }
static bool NavDownP(void) { return IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN) || AnyPadPressed(VB_DOWN); }
/* navegacao de UM jogador no 1 x 1: P1 = A/D + J (K desfaz) + controle 1; P2 = setas + 1/ENTER do
   teclado numerico (2 desfaz) + controle 2 */
enum { PN_LEFT, PN_RIGHT, PN_OK, PN_UNDO, PN_UP, PN_DOWN };
static bool PlayerNavP(int p, int what)
{
    static const int KL[2] = {KEY_A, KEY_LEFT}, KR[2] = {KEY_D, KEY_RIGHT}, KU[2] = {KEY_W, KEY_UP}, KD[2] = {KEY_S, KEY_DOWN};
    switch (what)
    {
    case PN_UP: return IsKeyPressed(KU[p]) || PadPressed(p, VB_UP);
    case PN_DOWN: return IsKeyPressed(KD[p]) || PadPressed(p, VB_DOWN);
    case PN_LEFT: return IsKeyPressed(KL[p]) || PadPressed(p, VB_LEFT);
    case PN_RIGHT: return IsKeyPressed(KR[p]) || PadPressed(p, VB_RIGHT);
    case PN_OK:
        if (p == 0) return IsKeyPressed(KEY_J) || IsKeyPressed(KEY_SPACE) || PadPressed(0, VB_JUMP) || PadPressed(0, VB_START);
        return IsKeyPressed(KEY_KP_1) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_ENTER) || PadPressed(1, VB_JUMP) || PadPressed(1, VB_START);
    default:
        if (p == 0) return IsKeyPressed(KEY_K) || PadPressed(0, VB_SHOT);
        return IsKeyPressed(KEY_KP_2) || PadPressed(1, VB_SHOT);
    }
}

/* aviso de controle conectado/desconectado e indicador dos controles */
static void DrawPadStatus(bool chips)
{
    if (chips)
    {
        for (int i = 0; i < IAF_PADS; i++)
        {
            float x = SCREEN_WIDTH - 250 + i * 120, y = 8;
            bool on = padNow[i].on;
            DrawRectangleRounded((Rectangle){x, y, 112, 22}, 0.4f, 6, on ? Fade(C_GREEN, 0.25f) : Fade(C_DIM, 0.12f));
            DrawCircleV((Vector2){x + 12, y + 11}, 5, on ? C_GREEN : Fade(C_DIM, 0.6f));
            char t[16];
            snprintf(t, sizeof t, "CONTROLE %d", i + 1);
            Txt(t, x + 22, y + 4, 13, on ? C_WHITE : C_DIM);
        }
    }
    if (padToastT > 0)
    {
        float a = Clamp(padToastT / 0.4f, 0.0f, 1.0f);
        float w = TxtW(padToast, 22) + 40;
        Rectangle r = {SCREEN_WIDTH / 2.0f - w / 2, 100, w, 40};
        DrawRectangleRounded(r, 0.4f, 6, Fade(C_INK, 0.85f * a));
        DrawRectangleRoundedLinesEx(r, 0.4f, 6, 2, Fade(strstr(padToast, "DESCONECTADO") ? C_RED : C_GREEN, a));
        TxtC(padToast, SCREEN_WIDTH / 2.0f, r.y + 9, 22, Fade(C_WHITE, a));
    }
}

/* ------------------------------------------------------------------ fluxo */
static void BuildLadder(void)
{
    int n = 0;
    if (playerCh == CH_ELIRA) { ladder[0] = CH_CLAUDE; ladder[1] = CH_CODEX; ladder[2] = CH_ANTI; }
    else if (playerCh == CH_IARA) { ladder[0] = CH_CLAUDE; ladder[1] = CH_CODEX; ladder[2] = CH_ELIRA; } /* convidada: 2 IAs e a chefe */
    else
    {
        for (int c = CH_CLAUDE; c <= CH_ANTI; c++)
            if (c != playerCh) ladder[n++] = c;
        ladder[2] = CH_ELIRA; /* chefe final */
    }
    ladderPos = 0;
}
static bool voxRoundDone = false, voxLutemDone = false; /* vozes da abertura ja tocaram neste round */
static void StartRound(void)
{
    ResetFighter(&fz[0], (CharId)playerCh, 420, 1);
    ResetFighter(&fz[1], (CharId)ladder[ladderPos], 860, -1);
    fz[1].alt = versusMode && fz[0].ch == fz[1].ch;
    voxRoundDone = voxLutemDone = false;
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
static int selP[2] = {0, 1};   /* 1 x 1: personagem de cada jogador */
static bool readyP[2];         /* 1 x 1: jogador ja confirmou */
static float readyT = 0;       /* atraso depois que os dois confirmam */
static int modeSel = 0, resSel = 0;
static void ArenaSortear(void); /* sorteia o cenario (iafight_arena.h) */
static void StartMatch(void)
{
    wins[0] = wins[1] = 0;
    resSel = 0;
    roundNo = 1;
    ArenaSortear(); /* cenario da luta (iafight_arena.h) */
    StartRound();
}
static void PushApart(void)
{
    Fighter *a = &fz[0], *b = &fz[1];
    if (a->st == ST_KO || b->st == ST_KO || a->down || b->down) return;
    if (well.on && well.caught) return;
    float minD = BodyHalf(a) + BodyHalf(b);
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
static bool *BufBtn(IafInput *in, int b)
{
    switch (b)
    {
    case BB_UP: return &in->upP;
    case BB_LIGHT: return &in->lightP;
    case BB_HEAVY: return &in->heavyP;
    case BB_SPECIAL: return &in->specialP;
    case BB_SHOT: return &in->shotP;
    case BB_DASH: return &in->dashP;
    default: return &in->grabP;
    }
}
/* o botao b ja pode agir neste quadro? */
static bool BufReady(Fighter *a, int b)
{
    if (b == BB_GRAB)
    { /* agarrao: livre, ou o fraco/forte do 1o botao acabou de sair */
        bool early = a->st == ST_ATK && a->atkT <= IAF_GRAB_TOGETHER + 0.02f && !a->hitDone && a->atk >= AT_LIGHT && a->atk <= AT_UPPER && a->atk != AT_AIR;
        return OnGround(a) && (early || a->st == ST_IDLE || a->st == ST_WALK || a->st == ST_CROUCH || (a->st == ST_BLOCK && a->stun <= 0));
    }
    if (a->st == ST_IDLE || a->st == ST_WALK || a->st == ST_CROUCH || (a->st == ST_BLOCK && a->stun <= 0)) return true;
    if (a->st == ST_JUMP && !a->airAtkUsed) return b == BB_LIGHT || b == BB_HEAVY; /* golpe aereo */
    if (a->st == ST_ATK && a->cancelT > 0) return (a->chain == 1 && b == BB_LIGHT) || (a->chain == 2 && b == BB_HEAVY); /* combo */
    return false;
}
/* guarda os botoes apertados e solta cada um quando o lutador k puder usa-lo (frozen = pausa de impacto) */
static IafInput BufferInput(int k, IafInput in, float dt, bool frozen)
{
    /* fraco e forte juntos = agarrao */
    if ((in.lightP && in.heavyP) || (in.lightP && pressAge[k][1] <= IAF_GRAB_TOGETHER) || (in.heavyP && pressAge[k][0] <= IAF_GRAB_TOGETHER)) in.grabP = true;
    pressAge[k][0] = in.lightP ? 0 : pressAge[k][0] + dt;
    pressAge[k][1] = in.heavyP ? 0 : pressAge[k][1] + dt;
    Fighter *g = &fz[1 - k];
    if (in.grabP && !frozen && fz[k].grabbed && g->st == ST_ATK && g->atk == AT_GRAB && g->atkT < IAF_GRAB_HOLD) { GrabEscape(k); return (IafInput){0}; }
    for (int b = 0; b < BB_N; b++)
    {
        bool *p = BufBtn(&in, b);
        if (*p) bufT[k][b] = b == BB_SPECIAL ? IAF_BUFFER_SPECIAL : IAF_BUFFER;
        if (frozen) continue;
        if (bufT[k][b] > 0 && BufReady(&fz[k], b)) { *p = true; bufT[k][b] = 0; }
        else { *p = false; bufT[k][b] = fmaxf(0, bufT[k][b] - dt); }
    }
    if (in.grabP) bufT[k][BB_LIGHT] = bufT[k][BB_HEAVY] = 0; /* os dois botoes viraram o agarrao */
    return in;
}
static void SimStep(float dt, IafInput i0, IafInput i1)
{
    UpdateParts(dt);
    bool frozen = hitstop > 0;
    i0 = BufferInput(0, i0, dt, frozen);
    i1 = BufferInput(1, i1, dt, frozen);
    if (frozen) { hitstop -= dt; return; }
    UpdateFighter(&fz[0], &fz[1], i0, dt);
    UpdateFighter(&fz[1], &fz[0], i1, dt);
    UpdateProjectiles(dt);
    UpdateWell(dt);
    PushApart();
}
/* ------------------------------------------------------------------ treino */
static IafInput DummyInput(void)
{
    Fighter *a = &fz[1], *o = &fz[0];
    IafInput in = {0};
    if (dummyMode == 1)
    { /* defende sempre: segura para tras quando vem golpe, tiro ou poco; agacha contra a rasteira */
        bool threat = (o->st == ST_ATK && fabsf(o->x - a->x) < 360) || ProjThreat(1) || (well.on && well.owner == 0);
        if (threat)
        {
            if (o->x > a->x) in.left = true; else in.right = true;
            in.down = o->st == ST_ATK && o->atk == AT_LOW;
        }
    }
    else if (dummyMode == 2 && OnGround(a) && (a->st == ST_IDLE || a->st == ST_WALK)) in.upP = in.up = true;
    return in;
}
/* um quadro do treino: teclas do treino, vida e carga cheias, cronometro parado; devolve a entrada do boneco */
static IafInput TrainStep(float dt)
{
    if (IsKeyPressed(KEY_F2) || AnyPadRawPressed(GAMEPAD_BUTTON_LEFT_THUMB)) dummyMode = (dummyMode + 1) % DUMMY_MODES;
    if (IsKeyPressed(KEY_F4) && texElira2.id) eliraNovos = !eliraNovos; /* modo de teste: sprites novos da Elira */
    if (IsKeyPressed(KEY_F3) || AnyPadRawPressed(GAMEPAD_BUTTON_RIGHT_THUMB))
    { /* volta os dois para o meio */
        ResetFighter(&fz[0], fz[0].ch, 420, 1);
        ResetFighter(&fz[1], fz[1].ch, 860, -1);
        ClearFx();
    }
    roundTimer = IAF_ROUND_TIME;
    for (int k = 0; k < 2; k++)
    {
        Fighter *f = &fz[k];
        if (!(k == 1 && dummyMode == 3)) f->meter = IAF_METER_MAX; /* lutando, o boneco enche a barra como numa luta */
        if (fz[1 - k].comboT <= 0 && f->st != ST_HIT && !f->down && f->hp < IAF_HP_MAX) f->hp = f->hpShown = IAF_HP_MAX; /* fim do combo: vida cheia */
    }
    if (dummyMode == 3) return CpuInput(1, dt); /* o boneco luta: serve para ver qualquer lutador (e a Iara) jogando */
    return DummyInput();
}
/* caixa do golpe do lutador (mesma conta do acerto em UpdateFighter): 0 = sem golpe, 1 = fora do ativo, 2 = ativo */
static int AtkBox(Fighter *a, Fighter *o, Rectangle *out)
{
    if (a->st != ST_ATK) return 0;
    if (a->atk == AT_GRAB)
    {
        float r = BodyHalf(a) + BodyHalf(o) + IAF_GRAB_RANGE, x1 = a->x + a->face * r;
        *out = (Rectangle){fminf(a->x, x1), a->y - 200, fabsf(x1 - a->x), 200};
        return a->atkT < IAF_GRAB_HOLD ? 2 : 1;
    }
    if (a->atk < AT_LIGHT || a->atk > AT_UPPER) return 0;
    const Atk *d = &ATK[a->ch][a->atk];
    float x0 = a->x + a->face * 10, x1 = a->x + a->face * d->rx;
    *out = (Rectangle){fminf(x0, x1), a->y - d->top, fabsf(x1 - x0), d->h};
    return (a->atkT >= d->st && a->atkT < d->st + d->ac) ? 2 : 1;
}
#define BOX_HURT ((Color){60, 230, 90, 255})
#define BOX_ATK ((Color){255, 60, 60, 255})
#define BOX_BODY ((Color){70, 150, 255, 255})
/* F1 / SELECT: verde = area atingivel (Hurt), vermelho = golpe, azul = corpo (BodyHalf, o empurrao) */
static void DrawBoxes(void)
{
    for (int k = 0; k < 2; k++)
    {
        Fighter *a = &fz[k];
        Rectangle h = Hurt(a);
        DrawRectangleRec(h, Fade(BOX_HURT, 0.14f));
        DrawRectangleLinesEx(h, 2, BOX_HURT);
        float bh = BodyHalf(a);
        DrawRectangle((int)(a->x - bh), (int)a->y - 6, (int)(2 * bh), 6, Fade(BOX_BODY, 0.6f));
        DrawLineEx((Vector2){a->x - bh, a->y - h.height}, (Vector2){a->x - bh, a->y}, 2, BOX_BODY);
        DrawLineEx((Vector2){a->x + bh, a->y - h.height}, (Vector2){a->x + bh, a->y}, 2, BOX_BODY);
        DrawLineEx((Vector2){a->x - 8, a->y}, (Vector2){a->x + 8, a->y}, 2, C_WHITE);
        DrawLineEx((Vector2){a->x, a->y - 8}, (Vector2){a->x, a->y + 2}, 2, C_WHITE);
        Rectangle ab;
        int act = AtkBox(a, &fz[1 - k], &ab);
        if (act)
        {
            DrawRectangleRec(ab, Fade(BOX_ATK, act == 2 ? 0.35f : 0.08f));
            DrawRectangleLinesEx(ab, act == 2 ? 2 : 1, act == 2 ? BOX_ATK : Fade(BOX_ATK, 0.5f));
        }
    }
}
static void DrawBoxLegend(float x, float y)
{
    DrawRectangle((int)x, (int)y + 3, 12, 12, BOX_HURT); Txt("atingível", x + 18, y, 14, C_WHITE);
    DrawRectangle((int)x + 110, (int)y + 3, 12, 12, BOX_ATK); Txt("golpe", x + 128, y, 14, C_WHITE);
    DrawRectangle((int)x + 196, (int)y + 3, 12, 12, BOX_BODY); Txt("corpo", x + 214, y, 14, C_WHITE);
}
static void DrawTrainHud(void)
{
    char b[96];
    Rectangle l = {60, 170, 330, 96}, r = {SCREEN_WIDTH - 390.0f, 170, 330, 96}; /* abaixo do contador de acertos */
    DrawRectangleRounded(l, 0.15f, 6, Fade(C_INK, 0.75f));
    DrawRectangleRoundedLinesEx(l, 0.15f, 6, 1.5f, Fade(C_GREEN, 0.6f));
    Txt("TREINO", l.x + 14, l.y + 8, 20, C_GREEN);
    if (trainLastBlocked) snprintf(b, sizeof b, "ÚLTIMO GOLPE: %.0f (defendido)", trainLast);
    else snprintf(b, sizeof b, "ÚLTIMO GOLPE: %.0f de dano", trainLast);
    Txt(b, l.x + 14, l.y + 38, 16, C_WHITE);
    if (trainComboHits >= 2) snprintf(b, sizeof b, "COMBO: %d acertos, %.0f de dano", trainComboHits, trainComboDmg);
    else snprintf(b, sizeof b, "COMBO: -");
    Txt(b, l.x + 14, l.y + 62, 16, trainComboHits >= 2 ? C_AMBER : C_DIM);
    DrawRectangleRounded(r, 0.15f, 6, Fade(C_INK, 0.75f));
    DrawRectangleRoundedLinesEx(r, 0.15f, 6, 1.5f, Fade(C_GREEN, 0.6f));
    snprintf(b, sizeof b, "BONECO: %s", DUMMY_TXT[dummyMode]);
    Txt(b, r.x + 14, r.y + 8, 18, C_WHITE);
    Txt("F2 / L3: boneco    F3 / R3: posição", r.x + 14, r.y + 36, 14, C_DIM);
    if (showBoxes) DrawBoxLegend(r.x + 14, r.y + 62);
    else Txt("F1 / SELECT: caixas de colisão", r.x + 14, r.y + 62, 14, C_DIM);
    if (texElira2.id) TxtC(eliraNovos ? "SPRITES: NOVOS  (F4 troca)" : "SPRITES: ATUAIS  (F4 troca)", SCREEN_WIDTH / 2.0f, 642, 15, eliraNovos ? C_CYAN : C_DIM);
    TxtC("COMBO: fraco > fraco > forte, acertando", SCREEN_WIDTH / 2.0f, 664, 15, C_AMBER);
    TxtC("AGARRÃO: fraco + forte juntos, colado", SCREEN_WIDTH / 2.0f, 686, 15, C_AMBER);
}
static void EndRound(int winner)
{
    roundWinner = winner;
    if (winner >= 0) wins[winner]++;
    rsT = 0;
}
static void UpdateFight(float dt)
{
    if (PauseP()) paused = !paused;
    if (IsKeyPressed(KEY_ESCAPE) && !paused) paused = true; /* ESC pausa; na pausa, ESC ou B voltam a selecao */
    else if (paused && (IsKeyPressed(KEY_ESCAPE) || AnyPadPressed(VB_SHOT))) { paused = false; readyP[0] = readyP[1] = false; trainPick = 0; iafScreen = SC_SELECT; scT = 0; return; }
    if (IsKeyPressed(KEY_F1) || AnyPadPressed(VB_SELECT)) showBoxes = !showBoxes; /* caixas de colisao (depuracao) */
    if (paused) return;
    if (shake > 0) shake -= dt;
    rsT += dt;
    IafInput none = {0};
    if (rstate == RS_INTRO)
    {
        SimStep(dt, none, none);
        if (!voxRoundDone) { voxRoundDone = true; PlaySfx(VOX_ROUND); }
        if (rsT >= IAF_INTRO_ROUND && !voxLutemDone) { voxLutemDone = true; PlaySfx(VOX_LUTEM); }
        if (rsT > IAF_INTRO_TOTAL) { rstate = RS_FIGHT; rsT = 0; }
        return;
    }
    if (rstate == RS_FIGHT)
    {
        IafInput i0 = cpu[0] ? CpuInput(0, dt) : HumanInput(0);
        IafInput i1 = trainMode ? TrainStep(dt) : (cpu[1] ? CpuInput(1, dt) : HumanInput(1)); /* treino: boneco */
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
            if (versusMode || lastWon) PlaySfx(VOX_WIN);
            iafScreen = (lastWon && ladderPos == 2) ? SC_FINAL : SC_RESULT;
            scT = 0;
        }
        else { roundNo++; StartRound(); }
    }
}

/* ------------------------------------------------------------------ arena e HUD da luta */
#include "iafight_arena.h"

static void DrawFightOverlay(void)
{
    if (rstate == RS_INTRO)
    {
        char b[32];
        bool last = (wins[0] == 1 && wins[1] == 1);
        if (last) snprintf(b, sizeof b, "ROUND FINAL");
        else snprintf(b, sizeof b, "ROUND %d", roundNo);
        if (rsT < IAF_INTRO_ROUND)
        {
            float k = EaseOut(rsT / 0.18f);
            DrawRectangle(0, 300, SCREEN_WIDTH, (int)(90 * k), Fade(C_INK, 0.6f));
            TxtO(b, SCREEN_WIDTH / 2.0f + (1 - k) * 300, 312, 64, C_WHITE, C_INK);
        }
        else
        {
            float k = EaseOut((rsT - IAF_INTRO_ROUND) / 0.15f);
            TxtO("LUTEM!", SCREEN_WIDTH / 2.0f, 296, (int)(64 + 24 * k), C_AMBER, C_INK);
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
                if (versusMode) snprintf(b, sizeof b, "P%d - %s VENCE O ROUND", roundWinner + 1, NAME[fz[roundWinner].ch]);
                else snprintf(b, sizeof b, "%s VENCE O ROUND", NAME[fz[roundWinner].ch]);
                TxtO(b, SCREEN_WIDTH / 2.0f, 300, 48, CHCOL[fz[roundWinner].ch], C_INK);
                if (fz[roundWinner].hp >= IAF_HP_MAX - 0.5f) TxtO("SEM UM ARRANHÃO!", SCREEN_WIDTH / 2.0f, 360, 28, C_AMBER, C_INK);
            }
        }
    }
    if (paused)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(C_INK, 0.6f));
        TxtO("PAUSA", SCREEN_WIDTH / 2.0f, 300, 64, C_WHITE, C_INK);
        TxtC("P / START: continuar      ESC / B: voltar à seleção", SCREEN_WIDTH / 2.0f, 380, 20, C_DIM);
    }
}
static void DrawFight(void)
{
    Camera2D cam = {0};
    cam.zoom = 1;
    if (shake > 0) cam.offset = (Vector2){(Rand01() - 0.5f) * shake * 24, (Rand01() - 0.5f) * shake * 24};
    BeginMode2D(cam);
    DrawStage();
    DrawStageFloor();
    DrawWell();
    /* quem ataca fica na frente */
    int first = (fz[0].st == ST_ATK && fz[1].st != ST_ATK) ? 1 : 0;
    if (fz[0].shieldOn || fz[0].st == ST_BLOCK) first = 1; /* quem defende fica na frente: o escudo aparece */
    else if (fz[1].shieldOn || fz[1].st == ST_BLOCK) first = 0;
    DrawFighter(&fz[first]);
    DrawFighter(&fz[1 - first]);
    DrawStageFront();
    DrawProjectiles();
    DrawParts();
    if (showBoxes) DrawBoxes();
    EndMode2D();
    DrawHUD();
    if (trainMode) DrawTrainHud();
    else if (showBoxes) DrawBoxLegend(SCREEN_WIDTH / 2.0f - 130, 686);
    DrawFightOverlay();
}

/* ------------------------------------------------------------------ retratos */
static void DrawCharacterBig(CharId ch, float x, float yFeet, int face, float sc, FState st)
{
    if (ch == CH_ELIRA)
    {
        int f = EAFrame(EA_IDLE, (int)(gt * 9) % EA_COUNT[EA_IDLE]);
        float yo = 0;
        if (st == ST_WIN) f = EliraV2() ? E2Frame(E2_WIN, E2_COUNT[E2_WIN] - 3 + ((int)(gt * 3) % 3)) : EAFrame(EA_WIN, EA_COUNT[EA_WIN] - 3 + ((int)(gt * 3) % 3));
        if (st == ST_KO) f = EliraV2() ? E2Frame(E2_DEFEAT, E2_COUNT[E2_DEFEAT] - 1) : EAFrame(EA_DEFEAT, (int)(gt * 12) % EA_COUNT[EA_DEFEAT]);
        DrawEF(f, x, yFeet + yo, face, sc, g_altPal ? IAF_ELIRA_ALT_TINT : WHITE, 0);
        return;
    }
    if (ch == CH_CLAUDE && texClaude.id)
    { /* escala em passos de 0,5 para o pixel da arte continuar inteiro (cartao 1,5x, VS 2,5x) */
        int f = st == ST_WIN ? CALoop(CA_VICTORY, gt) : (st == ST_KO ? CAFrame(CA_KNOCKDOWN, CA_COUNT[CA_KNOCKDOWN] - 1) : CALoop(CA_IDLE, gt));
        DrawCF(f, x, yFeet, face, fmaxf(1.0f, roundf(CL_SCALE * sc * 0.75f * 2) / 2), g_altPal ? IAF_CLAUDE_ALT_TINT : WHITE, 0);
        return;
    }
    if (ch == CH_CODEX && texCodex.id)
    { /* mesma regra do Claude: escala em passos de 0,5 do pixel da arte */
        int f = st == ST_WIN ? XALoop(XA_VICTORY, gt) : (st == ST_KO ? XAFrame(XA_KO, XA_COUNT[XA_KO] - 1) : XALoop(XA_IDLE, gt));
        DrawXF(f, x, yFeet, face, fmaxf(1.0f, roundf(XC_SCALE * sc * 0.75f * 2) / 2), g_altPal ? IAF_CODEX_ALT_TINT : WHITE, 0);
        return;
    }
    if (ch == CH_IARA && texIara.id)
    { /* filtro POINT: 1x nos paineis, no VS e no resultado (o atlas ja vem grande); 0,5x na previa da arena */
        int f = st == ST_WIN ? IRFrame(IR_WIN, IR_COUNT[IR_WIN] - 1) : (st == ST_KO ? IRFrame(IR_LOSE, IR_COUNT[IR_LOSE] - 1) : IRLoop(IR_IDLE, gt, 8));
        float s = sc * 0.85f / IAF_IARA_ESCALA;
        s = s < 0.75f ? 0.5f : fmaxf(1.0f, roundf(s * 2) / 2);
        DrawIRF(f, x, yFeet, face, s * IR_DRAW, g_altPal ? IAF_IARA_ALT_TINT : WHITE, 0);
        return;
    }
    if (ch == CH_ANTI && texAnti.id)
    { /* mesma regra: escala em passos de 0,5 */
        int f = st == ST_WIN ? AGLoop(AA_VICTORY, gt) : (st == ST_KO ? AGFrame(AA_KO, AA_COUNT[AA_KO] - 1) : AGLoop(AA_IDLE, gt));
        float s2 = fmaxf(1.0f, roundf(AG_SCALE * sc * 0.75f * 2) / 2);
        float hov = st == ST_KO ? 0 : (16 + 5 * sinf(gt * 2.4f)) * s2 / AG_SCALE;
        DrawAF(f, x, yFeet - hov, face, s2, g_altPal ? IAF_ANTI_ALT_TINT : WHITE, 0);
        return;
    }
    Fighter tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.ch = ch; tmp.st = st; tmp.face = face; tmp.x = x;
    Pose p = PoseFor(&tmp);
    DrawPuppet(ch, x, yFeet, face, sc, p, 0, 0);
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

/* ------------------------------------------------------------------ selecao de lutador
   Paineis grandes do 1P (esquerda) e do 2P/CPU (direita) com o lutador animado; no centro a grade
   com um retrato por lutador e os cursores dos jogadores. Serve ao 1 jogador e ao 1 x 1. */
static const Color PCOL[2] = {{96, 214, 240, 255}, {230, 80, 84, 255}}; /* cursor/rotulo do 1P e do 2P */
/* recorte do retrato (rosto e tronco) dentro do 1o quadro da parada, em pixels da arte de cada atlas */
static const Rectangle THUMB_CUT[CH_N] = {{0, 0, 66, 66}, {0, 0, 160, 102}, {0, 0, 80, 80}, {2, 0, 86, 78}, {0, 0, IR_THUMB_PX, IR_THUMB_PX}};
static void DrawThumb(CharId ch, Rectangle dst, bool alt)
{
    Texture2D tex = texElira;
    Rectangle fr;
    Color tint = WHITE;
    if (ch == CH_CLAUDE) { const CFrame *e = &CF[CA_FIRST[CA_IDLE]]; tex = texClaude; fr = (Rectangle){e->x, e->y, e->w, e->h}; if (alt) tint = IAF_CLAUDE_ALT_TINT; }
    else if (ch == CH_CODEX) { const XFrame *e = &XF[XA_FIRST[XA_IDLE]]; tex = texCodex; fr = (Rectangle){e->x, e->y, e->w, e->h}; if (alt) tint = IAF_CODEX_ALT_TINT; }
    else if (ch == CH_ANTI) { const AFrame *e = &AF[AA_FIRST[AA_IDLE]]; tex = texAnti; fr = (Rectangle){e->x, e->y, e->w, e->h}; if (alt) tint = IAF_ANTI_ALT_TINT; }
    else if (ch == CH_IARA) { const IFrame *e = &IRF[IR_FIRST[IR_THUMB]]; tex = texIara; /* retrato pre-escalado (1:1 no cartao) */ fr = (Rectangle){e->x, e->y, e->w, e->h}; if (alt) tint = IAF_IARA_ALT_TINT; }
    else { const EFrame *e = &EF[EAFrame(EA_IDLE, 0)]; fr = (Rectangle){e->x, e->y, e->w, e->h}; if (alt) tint = IAF_ELIRA_ALT_TINT; }
    if (!tex.id)
    { /* sem o atlas: o boneco desenhado, pequeno */
        g_altPal = alt;
        DrawCharacterBig(ch, dst.x + dst.width / 2, dst.y + dst.height - 4, 1, 0.4f, ST_IDLE);
        g_altPal = false;
        return;
    }
    Rectangle c = THUMB_CUT[ch];
    Rectangle src = {fr.x + c.x, fr.y + c.y, fminf(c.width, fr.width - c.x), fminf(c.height, fr.height - c.y)};
    float k = fminf(dst.width / src.width, dst.height / src.height);
    float w = roundf(src.width * k), h = roundf(src.height * k);
    DrawTexturePro(tex, src, (Rectangle){roundf(dst.x + (dst.width - w) / 2), roundf(dst.y + (dst.height - h) / 2), w, h}, (Vector2){0, 0}, 0, tint);
}
/* ordem do arcade para quem escolhe 'ch' (a mesma de BuildLadder) */
static void LadderFor(int ch, int out[3])
{
    int n = 0;
    if (ch == CH_ELIRA) { out[0] = CH_CLAUDE; out[1] = CH_CODEX; out[2] = CH_ANTI; return; }
    if (ch == CH_IARA) { out[0] = CH_CLAUDE; out[1] = CH_CODEX; out[2] = CH_ELIRA; return; }
    for (int c = CH_CLAUDE; c <= CH_ANTI; c++)
        if (c != ch) out[n++] = c;
    out[2] = CH_ELIRA;
}
static Rectangle GridCell(int i)
{
    const float cs = 112, gap = 12;
    float x0 = SCREEN_WIDTH / 2.0f - cs - gap / 2;
    if (i == CH_IARA) return (Rectangle){x0, 176 + 2 * (cs + gap) + 14, 2 * cs + gap, 84}; /* cartao de colab, separado da grade */
    return (Rectangle){x0 + (i % 2) * (cs + gap), 176 + (i / 2) * (cs + gap), cs, cs};
}
/* cursor da selecao: esquerda/direita percorre os 4 e a convidada; cima/baixo troca a linha da grade e
   desce (ou sobe) para o cartao de colab, abaixo da grade */
static int SelMove(int c, int dx, int dy)
{
    if (dx) return (c + dx + CH_N) % CH_N;
    if (!dy) return c;
    if (c == CH_IARA) return dy > 0 ? 0 : 2;
    if (dy > 0) return c < 2 ? c + 2 : CH_IARA;
    return c >= 2 ? c - 2 : CH_IARA;
}
/* cartao da Iara: personagem de colab (jogo parceiro), com o selo COLAB */
static void DrawThumb(CharId ch, Rectangle dst, bool alt);
static void DrawColabCard(bool on)
{
    Rectangle g = GridCell(CH_IARA);
    Color c = CHCOL[CH_IARA], red = {156, 65, 86, 255};
    DrawRectangleRec(g, (Color){14, 22, 34, 255});
    DrawRectangleGradientH((int)g.x, (int)g.y, (int)g.width, (int)g.height, Fade(c, on ? 0.30f : 0.12f), Fade(c, 0));
    DrawThumb(CH_IARA, (Rectangle){g.x + 6, g.y + 6, g.height - 12, g.height - 12}, false);
    float tx = g.x + g.height + 2;
    Txt("IARA", tx, g.y + 18, 24, on ? c : Fade(c, 0.85f));
    Txt("convidada do", tx, g.y + 46, 12, C_DIM);
    Txt("jogo parceiro", tx, g.y + 60, 12, C_DIM);
    DrawRectangleLinesEx(g, 2, on ? c : Fade(c, 0.4f));
    /* selo COLAB no canto */
    float bw = TxtW("COLAB", 14) + 14;
    Rectangle b = {g.x + g.width - bw - 6, g.y - 9, bw, 20};
    DrawRectangleRec(b, red);
    DrawRectangleLinesEx(b, 1, ColorLerp(red, WHITE, 0.4f));
    TxtC("COLAB", b.x + bw / 2, b.y + 2, 14, C_WHITE);
}
/* escala do lutador no painel: ~1,1x o tamanho da luta, com o pixel da arte inteiro */
static const float PANEL_SC[CH_N] = {1.1f, 1.4f, 1.67f, 1.4f, 1.4f};
/* painel grande de um lado: rotulo (1P / 2P / CPU), nome e o lutador animado */
static void DrawSelPanel(int side, int ch, const char *tag, Color tagCol, bool alt, bool ready)
{
    Rectangle r = {side == 0 ? 32.0f : SCREEN_WIDTH - 32.0f - 396, 108, 396, 478};
    Color c = alt ? AltCol(CHCOL[ch]) : CHCOL[ch];
    DrawRectangleRounded(r, 0.04f, 6, (Color){14, 18, 36, 255});
    DrawRectangleGradientV((int)r.x + 4, (int)(r.y + r.height * 0.35f), (int)r.width - 8, (int)(r.height * 0.65f) - 4, Fade(c, 0), Fade(c, 0.30f));
    DrawRectangleRoundedLinesEx(r, 0.04f, 6, ready ? 5 : 2, ready ? tagCol : Fade(tagCol, 0.7f));
    float tx = side == 0 ? r.x + 20 : r.x + r.width - 20;
    float tw = TxtW(tag, 40), nw = TxtW(NAME[ch], 34);
    TxtO(tag, side == 0 ? tx + tw / 2 : tx - tw / 2, r.y + 10, 40, tagCol, C_INK);
    TxtO(NAME[ch], side == 0 ? tx + nw / 2 : tx - nw / 2, r.y + 54, 34, c, C_INK);
    if (alt) { const char *s = "cor alternativa"; Txt(s, side == 0 ? tx : tx - TxtW(s, 14), r.y + 96, 14, C_DIM); }
    float fy = r.y + r.height - 26;
    DrawEllipse((int)(r.x + r.width / 2), (int)fy + 2, 130, 12, Fade(BLACK, 0.5f));
    DrawEllipseLines((int)(r.x + r.width / 2), (int)fy + 2, 136, 13, Fade(c, 0.6f));
    g_altPal = alt;
    DrawCharacterBig((CharId)ch, r.x + r.width / 2, fy, side == 0 ? 1 : -1, PANEL_SC[ch], ST_IDLE);
    g_altPal = false;
    if (ready) { float pw = TxtW("PRONTO!", 30); TxtO("PRONTO!", side == 0 ? tx + pw / 2 : tx - pw / 2, r.y + 118, 30, tagCol, C_INK); }
}
/* rodape discreto de um lado: especial, descricao e a linha de estado/controles */
static void DrawSelInfo(int side, int ch, const char *status, Color stCol, bool alt)
{
    Rectangle r = {side == 0 ? 32.0f : SCREEN_WIDTH / 2.0f + 8, 600, SCREEN_WIDTH / 2.0f - 40, 86};
    Color c = alt ? AltCol(CHCOL[ch]) : CHCOL[ch];
    DrawRectangleRounded(r, 0.15f, 6, (Color){12, 14, 30, 255});
    DrawRectangleRoundedLinesEx(r, 0.15f, 6, 1.5f, Fade(c, 0.5f));
    Txt("ESPECIAL:", r.x + 14, r.y + 10, 16, C_WHITE);
    Txt(SPNAME[ch], r.x + 14 + TxtW("ESPECIAL:", 16) + 8, r.y + 10, 16, c);
    Txt(SPDESC[ch], r.x + 14, r.y + 36, 13, C_DIM);
    if (status) Txt(status, r.x + 14, r.y + 60, 14, stCol);
}
static void DrawSelectScreen(bool vs)
{
    int lad[3];
    LadderFor(sel, lad);
    int cur[2] = {vs ? selP[0] : sel, vs ? selP[1] : lad[0]};
    bool tr = !vs && trainMode, dum = tr && trainPick == 1; /* treino; escolhendo o boneco */
    if (tr) { cur[0] = dum ? playerCh : sel; cur[1] = dum ? sel : trainDummy; }
    bool mirror = vs && cur[0] == cur[1];
    DrawBackdrop(vs ? C_DIM : CHCOL[sel]);
    TxtO("IA FIGHT", SCREEN_WIDTH / 2.0f, 14, 56, C_WHITE, C_INK);
    const char *sub = vs ? "1 x 1  -  ESCOLHAM SEUS LUTADORES" : "1 JOGADOR x CPU  -  ESCOLHA SEU LUTADOR";
    if (tr) sub = dum ? "TREINO  -  ESCOLHA O BONECO" : "TREINO  -  ESCOLHA SEU LUTADOR";
    TxtC(sub, SCREEN_WIDTH / 2.0f, 76, 18, tr ? C_GREEN : C_CYAN);
    DrawSelPanel(0, cur[0], "1P", PCOL[0], false, (vs && readyP[0]) || dum);
    if (vs) DrawSelPanel(1, cur[1], "2P", PCOL[1], mirror, readyP[1]);
    else if (tr) DrawSelPanel(1, cur[1], "BONECO", C_GREEN, false, false);
    else DrawSelPanel(1, cur[1], "CPU", C_AMBER, false, false);

    /* grade central */
    TxtC("LUTADORES", SCREEN_WIDTH / 2.0f, 150, 16, C_DIM);
    for (int i = 0; i < CH_MAIN; i++)
    {
        Rectangle g = GridCell(i);
        bool on = cur[0] == i || ((vs || dum) && cur[1] == i);
        DrawRectangleRec(g, (Color){18, 24, 46, 255});
        DrawRectangleGradientV((int)g.x, (int)(g.y + g.height / 2), (int)g.width, (int)(g.height / 2), Fade(CHCOL[i], 0), Fade(CHCOL[i], on ? 0.35f : 0.15f));
        DrawThumb((CharId)i, (Rectangle){g.x + 6, g.y + 6, g.width - 12, g.height - 12}, false);
        DrawRectangleLinesEx(g, 2, on ? CHCOL[i] : Fade(CHCOL[i], 0.4f));
    }
    for (int p = 0; p < ((vs || dum) ? 2 : 1); p++)
    { /* cursores: 1P em cima, 2P (ou o boneco do treino) embaixo; no mesmo quadrado o de baixo fica por fora */
        Rectangle g = GridCell(cur[p]);
        float o = (p == 1 && cur[0] == cur[1]) ? 9 : 3;
        Rectangle rr = {g.x - o, g.y - o, g.width + 2 * o, g.height + 2 * o};
        bool rd = (vs && readyP[p]) || (dum && p == 0);
        Color pc = dum && p == 1 ? C_GREEN : PCOL[p];
        const char *tag = p == 0 ? "1P" : (dum ? "BONECO" : "2P");
        float tw = TxtW(tag, 18) + 16;
        DrawRectangleLinesEx(rr, rd ? 5 : 4, Fade(pc, rd ? 1.0f : 0.65f + 0.35f * sinf(gt * 6)));
        Rectangle tg = p == 0 ? (Rectangle){rr.x, rr.y - 22, tw, 24} : (Rectangle){rr.x + rr.width - tw, rr.y + rr.height - 2, tw, 24};
        DrawRectangleRec(tg, pc);
        TxtC(tag, tg.x + tw / 2, tg.y + 3, 18, C_INK);
    }
    DrawColabCard(cur[0] == CH_IARA || ((vs || dum) && cur[1] == CH_IARA));
    float by = GridCell(CH_IARA).y + GridCell(CH_IARA).height + 18;
    if (tr)
    {
        TxtC("vida infinita e carga cheia", SCREEN_WIDTH / 2.0f, by, 14, C_DIM);
        TxtC("boneco parado, defendendo, pulando ou lutando (F2 / L3)", SCREEN_WIDTH / 2.0f, by + 20, 14, C_DIM);
        TxtC("caixas de colisão: F1 / SELECT", SCREEN_WIDTH / 2.0f, by + 40, 14, C_DIM);
        DrawSelInfo(0, cur[0], dum ? "escolhido   ESC / B: trocar" : NULL, PCOL[0], false);
        DrawSelInfo(1, cur[1], "boneco de treino", C_GREEN, false);
        TxtC("A/D, W/S, SETAS ou CONTROLE: escolher    ENTER / A: confirmar    TAB / SELECT: controles    ESC / B: voltar", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 26, 14, C_DIM);
        return;
    }
    if (!vs)
    { /* ordem do arcade de quem esta marcado */
        char t[96];
        snprintf(t, sizeof t, "%s > %s > %s", NAME[lad[0]], NAME[lad[1]], NAME[lad[2]]);
        TxtC("ARCADE  -  MELHOR DE 3 ROUNDS", SCREEN_WIDTH / 2.0f, by, 14, C_DIM);
        TxtC(t, SCREEN_WIDTH / 2.0f, by + 20, 16, Fade(C_WHITE, 0.85f));
        if (lad[2] == CH_ELIRA) TxtC("a Elira é a chefe final", SCREEN_WIDTH / 2.0f, by + 40, 14, C_DIM);
        DrawSelInfo(0, cur[0], NULL, C_DIM, false);
        DrawSelInfo(1, cur[1], "1º desafio do arcade", C_AMBER, false);
        TxtC("A/D, W/S, SETAS ou CONTROLE: escolher    ENTER / A: confirmar    TAB / SELECT: controles    ESC / B: voltar", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 26, 14, C_DIM);
        return;
    }
    TxtC("MELHOR DE 3 ROUNDS", SCREEN_WIDTH / 2.0f, by, 14, C_DIM);
    for (int p = 0; p < 2; p++)
    {
        const char *st;
        Color sc = C_DIM;
        if (readyP[p]) { st = p == 0 ? "PRONTO!   K ou B: desfazer" : "PRONTO!   2 ou B: desfazer"; sc = PCOL[p]; }
        else if (padNow[p].on) { st = p == 0 ? "controle 1 conectado  -  A confirma" : "controle 2 conectado  -  A confirma"; sc = C_GREEN; }
        else st = p == 0 ? "teclado: A/D/W/S escolhe  -  J confirma" : "teclado: SETAS escolhem  -  1 confirma";
        DrawSelInfo(p, cur[p], st, sc, p == 1 && mirror);
    }
    if (readyP[0] && readyP[1])
    {
        DrawRectangle(440, 292, 400, 92, Fade(C_INK, 0.75f));
        TxtO("LUTEM!", SCREEN_WIDTH / 2.0f, 300, 72, C_AMBER, C_INK);
    }
    TxtC("TAB / SELECT: ver controles      ESC: voltar ao modo", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 26, 14, C_DIM);
}
/* ------------------------------------------------------------------ selecao de arena
   Depois dos lutadores (1 x CPU, 1 x 1 e treino) e antes do VS: cartoes grandes com miniatura e nome, e a
   previa grande da arena marcada. Cartao travado (EM BREVE) nao confirma e toca o som de negado.
   No 1 x 1 quem escolhe e sempre o P1; o P2 so pode voltar aos lutadores (B / 2), para trocar o dele.
   B / ESC volta aos lutadores. */
static int arSel = 0;         /* cartao marcado */
static float arDenyT = 0;     /* > 0: tremor do cartao travado que tentaram escolher */
static Vector2 arMouseLast;
static Rectangle ArenaCard(int i)
{
    float gap = 24, w = fminf(236, (SCREEN_WIDTH - 80 - (AR_DEFS - 1) * gap) / AR_DEFS), h = (w - 16) * 9 / 16 + 46; /* miniatura 16:9 */
    float x0 = SCREEN_WIDTH / 2.0f - (AR_DEFS * w + (AR_DEFS - 1) * gap) / 2;
    return (Rectangle){x0 + i * (w + gap), 522, w, h};
}
static Rectangle ArenaCardPic(int i)
{
    Rectangle c = ArenaCard(i);
    return (Rectangle){c.x + 8, c.y + 8, c.width - 16, c.height - 46};
}
static const Rectangle AR_PREVIEW = {336, 92, 608, 342};
static void OpenArenaSelect(void)
{
    arSel = arPick;
    arDenyT = 0;
    arMouseLast = GetVirtualMouse();
    iafScreen = SC_ARENA;
    scT = 0;
}
/* chiado de TV para a arena que ainda nao existe */
static void DrawStatic(Rectangle r, float px, unsigned seed)
{
    DrawRectangleRec(r, (Color){12, 14, 28, 255});
    int nx = (int)(r.width / px), ny = (int)(r.height / px);
    unsigned fr = (unsigned)(gt * 10);
    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++)
        {
            float v = HashF(seed * 7919u + fr * 104729u + (unsigned)(y * 977 + x));
            if (v > 0.6f) DrawRectangle((int)(r.x + x * px), (int)(r.y + y * px), (int)px, (int)px, Fade((Color){120, 130, 170, 255}, (v - 0.6f) * 0.9f));
        }
}
static void DrawPadlock(Vector2 c, float s, Color col)
{
    DrawRing((Vector2){c.x, c.y - s * 0.30f}, s * 0.22f, s * 0.34f, 180, 360, 24, col); /* argola */
    DrawRectangle((int)(c.x - s * 0.34f), (int)(c.y - s * 0.32f), (int)(s * 0.12f), (int)(s * 0.14f), col);
    DrawRectangle((int)(c.x + s * 0.22f), (int)(c.y - s * 0.32f), (int)(s * 0.12f), (int)(s * 0.14f), col);
    DrawRectangleRounded((Rectangle){c.x - s * 0.5f, c.y - s * 0.2f, s, s * 0.75f}, 0.2f, 4, col);
    DrawCircleV((Vector2){c.x, c.y + s * 0.08f}, s * 0.1f, C_INK);
    DrawRectangle((int)(c.x - s * 0.035f), (int)(c.y + s * 0.1f), (int)(s * 0.07f), (int)(s * 0.22f), C_INK);
}
/* a imagem da arena i no retangulo (miniatura do cartao ou previa grande) */
static void DrawArenaPic(int i, Rectangle dst, bool big)
{
    if (ArenaLocked(i))
    {
        DrawStatic(dst, big ? 8 : 5, (unsigned)i + 1);
        DrawPadlock((Vector2){dst.x + dst.width / 2, dst.y + dst.height * (big ? 0.42f : 0.46f)}, dst.height * (big ? 0.26f : 0.34f), Fade(C_DIM, 0.95f));
        return;
    }
    if (ARENAS[i].kind != AR_PNG)
    { /* arena desenhada no codigo: retrato feito em ArenaBakePreviews */
        RenderTexture2D *t = &arPrevRT[i];
        if (t->id) DrawTexturePro(t->texture, (Rectangle){0, 0, (float)t->texture.width, -(float)t->texture.height}, dst, (Vector2){0, 0}, 0, WHITE);
        else DrawRectangleRec(dst, (Color){18, 22, 42, 255});
        return;
    }
    ArenaLayer(i, AL_FUNDO, dst, WHITE);
    ArenaLayer(i, AL_MEIO, dst, WHITE);
    ArenaLayer(i, AL_PLATEIA, dst, WHITE);
    ArenaLayer(i, AL_CHAO, dst, WHITE);
    ArenaLayer(i, AL_FRENTE, dst, WHITE);
}
static void DrawArenaSelect(void)
{
    const ArenaDef *d = &ARENAS[arSel];
    bool locked = ArenaLocked(arSel);
    DrawBackdrop(C_CYAN);
    TxtO("ESCOLHA A ARENA", SCREEN_WIDTH / 2.0f, 14, 48, C_WHITE, C_INK);
    const char *sub = trainMode ? "TREINO" : (versusMode ? "1 x 1  -  o P1 escolhe a arena  (P2: B volta aos lutadores)" : "1 JOGADOR x CPU  -  a arena vale para todo o arcade");
    TxtC(sub, SCREEN_WIDTH / 2.0f, 68, 16, trainMode ? C_GREEN : C_CYAN);
    /* previa grande, com os dois lutadores na escala da luta */
    Rectangle pv = AR_PREVIEW;
    DrawRectangle((int)pv.x - 6, (int)pv.y - 6, (int)pv.width + 12, (int)pv.height + 12, C_INK);
    DrawArenaPic(arSel, pv, true);
    if (!locked)
    {
        float k = pv.width / SCREEN_WIDTH, gy = pv.y + IAF_GROUND_Y * k;
        CharId a = (CharId)playerCh, b = (CharId)(versusMode ? selP[1] : ladder[ladderPos]);
        DrawCharacterBig(a, pv.x + 420 * k, gy, 1, 0.5f, ST_IDLE);
        g_altPal = versusMode && a == b;
        DrawCharacterBig(b, pv.x + 860 * k, gy, -1, 0.5f, ST_IDLE);
        g_altPal = false;
    }
    else
    {
        TxtO("EM CONSTRUÇÃO", pv.x + pv.width / 2, pv.y + pv.height * 0.66f, 40, C_AMBER, C_INK);
        TxtC("os colegas estão desenhando esta arena", pv.x + pv.width / 2, pv.y + pv.height * 0.66f + 52, 16, C_WHITE);
    }
    DrawRectangleLinesEx((Rectangle){pv.x - 3, pv.y - 3, pv.width + 6, pv.height + 6}, 3, locked ? Fade(C_DIM, 0.8f) : C_CYAN);
    TxtO(locked ? "EM BREVE" : d->name, SCREEN_WIDTH / 2.0f, 444, 28, locked ? C_DIM : C_WHITE, C_INK);
    TxtC(d->tag, SCREEN_WIDTH / 2.0f, 476, 14, C_DIM);
    /* cartoes */
    for (int i = 0; i < AR_DEFS; i++)
    {
        Rectangle c = ArenaCard(i), p = ArenaCardPic(i);
        bool on = i == arSel, lk = ArenaLocked(i);
        float dx = (on && arDenyT > 0) ? sinf(arDenyT * 60) * 6 * arDenyT / 0.3f : 0;
        c.x += dx; p.x += dx;
        if (on) DrawRectangleRounded((Rectangle){c.x - 6, c.y - 6, c.width + 12, c.height + 12}, 0.08f, 6, Fade(lk ? C_RED : C_CYAN, 0.18f + 0.1f * sinf(gt * 6)));
        DrawRectangleRounded(c, 0.06f, 6, (Color){14, 18, 36, 255});
        DrawArenaPic(i, p, false);
        if (lk) DrawRectangleRec(p, Fade(C_INK, 0.25f));
        DrawRectangleRoundedLinesEx(c, 0.06f, 6, on ? 3 : 1.5f, on ? (lk ? C_RED : C_CYAN) : Fade(lk ? C_DIM : C_CYAN, 0.45f));
        const char *nm = lk ? "EM BREVE" : ARENAS[i].name;
        TxtC(nm, c.x + c.width / 2, c.y + c.height - 32, 18, lk ? C_DIM : (on ? C_WHITE : Fade(C_WHITE, 0.75f)));
        if (on)
        { /* cursor: quem pode mexer nele */
            const char *tag = "1P";
            float tw = TxtW(tag, 16) + 14;
            DrawRectangleRec((Rectangle){c.x, c.y - 22, tw, 22}, lk ? C_RED : C_CYAN);
            TxtC(tag, c.x + tw / 2, c.y - 19, 16, C_INK);
        }
    }
    if (arDenyT > 0) TxtO("ARENA TRAVADA", SCREEN_WIDTH / 2.0f, pv.y + 22, 36, C_RED, C_INK);
    TxtC("A/D, SETAS ou CONTROLE: escolher    ENTER / A: confirmar    ESC / B: voltar aos lutadores", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 22, 14, C_DIM);
}
/* confirma o cartao marcado; travado: nega */
static void ArenaConfirm(void)
{
    if (ArenaLocked(arSel)) { arDenyT = 0.3f; PlaySfx(SFX_DENY); return; }
    arPick = arSel;
    iafScreen = SC_VERSUS;
    scT = 0;
}
static void ArenaBack(void)
{
    if (versusMode) { readyP[0] = readyP[1] = false; readyT = 0; }
    if (trainMode) { trainPick = 1; sel = trainDummy; } /* volta ao boneco */
    iafScreen = SC_SELECT;
    scT = 0;
}
static void UpdateArenaSelect(float dt)
{
    if (arDenyT > 0) arDenyT -= dt;
    int mv = 0;
    bool ok = false, back = false;
    if (versusMode)
    { /* so o P1 mexe e confirma; os dois podem voltar aos lutadores */
        if (PlayerNavP(0, PN_LEFT) || PlayerNavP(0, PN_UP)) mv = -1;
        if (PlayerNavP(0, PN_RIGHT) || PlayerNavP(0, PN_DOWN)) mv = 1;
        ok = scT > 0.15f && PlayerNavP(0, PN_OK);
        back = PlayerNavP(0, PN_UNDO) || PlayerNavP(1, PN_UNDO) || IsKeyPressed(KEY_ESCAPE);
    }
    else
    {
        if (NavLeftP() || NavUpP()) mv = -1;
        if (NavRightP() || NavDownP()) mv = 1;
        ok = scT > 0.15f && NavOkP();
        back = NavBackP();
    }
    /* mouse: passar por cima marca, clicar confirma */
    Vector2 m = GetVirtualMouse();
    bool moved = m.x != arMouseLast.x || m.y != arMouseLast.y;
    arMouseLast = m;
    for (int i = 0; i < AR_DEFS; i++)
        if (CheckCollisionPointRec(m, ArenaCard(i)))
        {
            if (moved) arSel = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { arSel = i; ok = true; }
        }
    if (back) { ArenaBack(); return; }
    if (mv) arSel = (arSel + mv + AR_DEFS) % AR_DEFS;
    if (ok) ArenaConfirm();
}
static void DrawSelect(void) { DrawSelectScreen(false); }
static void UpdateSelect(void)
{
    if (NavLeftP()) sel = SelMove(sel, -1, 0);
    if (NavRightP()) sel = SelMove(sel, 1, 0);
    if (NavUpP()) sel = SelMove(sel, 0, -1); /* grade 2 x 2 e, embaixo, o cartao de colab */
    if (NavDownP()) sel = SelMove(sel, 0, 1);
    if (NavOkP())
    {
        if (trainMode && trainPick == 0) { playerCh = sel; trainPick = 1; sel = trainDummy; PlaySfx(SFX_OK); return; } /* agora o boneco */
        if (trainMode)
        {
            trainDummy = sel; sel = playerCh;
            ladderPos = 0; ladder[0] = trainDummy;
            trainLast = trainComboDmg = 0; trainComboHits = 0; trainLastBlocked = false;
            OpenArenaSelect();
            return;
        }
        playerCh = sel;
        BuildLadder();
        OpenArenaSelect();
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
    g_altPal = versusMode && a == b;
    DrawCharacterBig(b, SCREEN_WIDTH - 300 + (1 - k) * 300, 560, -1, 1.6f, ST_IDLE);
    g_altPal = false;
    TxtO(NAME[a], 300, 590, 44, CHCOL[a], C_INK);
    TxtO(NAME[b], SCREEN_WIDTH - 300, 590, 44, CHCOL[b], C_INK);
    TxtO("VS", SCREEN_WIDTH / 2.0f, 300, 96, C_WHITE, C_INK);
    char d[48];
    if (trainMode) snprintf(d, sizeof d, "TREINO");
    else if (versusMode) snprintf(d, sizeof d, "1 x 1  -  MELHOR DE 3");
    else if (ladderPos == 2 && b == CH_ELIRA) snprintf(d, sizeof d, "DESAFIO FINAL - A CHEFE");
    else snprintf(d, sizeof d, "DESAFIO %d DE 3", ladderPos + 1);
    if (versusMode)
    {
        TxtO("P1", 300, 640, 30, C_CYAN, C_INK);
        TxtO("P2", SCREEN_WIDTH - 300, 640, 30, C_RED, C_INK);
    }
    TxtO(d, SCREEN_WIDTH / 2.0f, 40, 32, C_AMBER, C_INK);
    char an[96];
    snprintf(an, sizeof an, "ARENA: %s", ARENAS[arPick].name);
    TxtC(an, SCREEN_WIDTH / 2.0f, 84, 18, C_WHITE);
    TxtC("ENTER / A: lutar    ESC / B: trocar a arena", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 30, 16, C_DIM);
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
    if (final) TxtC("ENTER / A: voltar à seleção", SCREEN_WIDTH / 2.0f, 684, 16, C_DIM);
    else if (lastWon)
    {
        snprintf(b, sizeof b, "ENTER / A: próximo desafio (%d de 3)    ESC / B: seleção", ladderPos + 2);
        TxtC(b, SCREEN_WIDTH / 2.0f, 684, 16, C_DIM);
    }
    else TxtC("ENTER / A: tentar de novo    ESC / B: seleção", SCREEN_WIDTH / 2.0f, 684, 16, C_DIM);
}

/* ------------------------------------------------------------------ tela de controles */
static IafScreen ctrlBack = SC_SELECT; /* tela para onde a de controles volta */
static bool ControlsKeyP(void) { return IsKeyPressed(KEY_TAB) || AnyPadPressed(VB_SELECT); }
static void OpenControls(void) { ctrlBack = iafScreen; iafScreen = SC_CONTROLS; scT = 0; }
/* simbolo de botao do PlayStation desenhado (a fonte do jogo nao tem esses glifos) */
static void PsGlyph(Vector2 c, int kind, float r, Color col)
{
    switch (kind)
    {
    case 0: DrawRectangleLinesEx((Rectangle){c.x - r, c.y - r, 2 * r, 2 * r}, 2, col); break; /* quadrado */
    case 1: /* triangulo */
        DrawLineEx((Vector2){c.x, c.y - r}, (Vector2){c.x + r, c.y + r * 0.8f}, 2, col);
        DrawLineEx((Vector2){c.x + r, c.y + r * 0.8f}, (Vector2){c.x - r, c.y + r * 0.8f}, 2, col);
        DrawLineEx((Vector2){c.x - r, c.y + r * 0.8f}, (Vector2){c.x, c.y - r}, 2, col);
        break;
    case 2: DrawRing(c, r - 2, r, 0, 360, 24, col); break; /* circulo */
    default: /* xis */
        DrawLineEx((Vector2){c.x - r, c.y - r}, (Vector2){c.x + r, c.y + r}, 2, col);
        DrawLineEx((Vector2){c.x - r, c.y + r}, (Vector2){c.x + r, c.y - r}, 2, col);
        break;
    }
}
static void DrawControls(void)
{
    DrawBackdrop(C_GREEN);
    TxtO("CONTROLES", SCREEN_WIDTH / 2.0f, 18, 56, C_WHITE, C_INK);
    /* ---- controle */
    Rectangle pane = {40, 92, 720, 470};
    DrawRectangleRounded(pane, 0.06f, 6, (Color){12, 14, 30, 235});
    DrawRectangleRoundedLinesEx(pane, 0.06f, 6, 2, Fade(C_GREEN, 0.6f));
    Txt("CONTROLE  (Xbox ou PlayStation)", pane.x + 20, pane.y + 14, 22, C_GREEN);
    Vector2 o = {pane.x + 190, pane.y + 220};
    Color body = (Color){46, 52, 72, 255}, edge = (Color){90, 100, 130, 255}, dark = (Color){24, 28, 40, 255};
    DrawCircleV((Vector2){o.x - 92, o.y + 40}, 56, body); /* pegadores */
    DrawCircleV((Vector2){o.x + 92, o.y + 40}, 56, body);
    DrawRectangleRounded((Rectangle){o.x - 150, o.y - 56, 300, 112}, 0.6f, 12, body);
    DrawRectangleRoundedLinesEx((Rectangle){o.x - 150, o.y - 56, 300, 112}, 0.6f, 12, 2, edge);
    DrawRectangleRounded((Rectangle){o.x + 64, o.y - 76, 76, 22}, 0.6f, 6, C_AMBER); /* ombro direito: especial */
    DrawRectangleRounded((Rectangle){o.x - 140, o.y - 76, 76, 22}, 0.6f, 6, C_CYAN); /* ombro esquerdo: dash */
    TxtC("RB / R1", o.x + 102, o.y - 72, 13, C_INK);
    TxtC("LB / L1", o.x - 102, o.y - 72, 13, C_INK);
    DrawRectangleRounded((Rectangle){o.x - 136, o.y - 100, 68, 20}, 0.6f, 6, C_GRAB); /* gatilho esquerdo: agarrao */
    TxtC("LT / L2", o.x - 102, o.y - 97, 13, C_INK);
    Vector2 st = {o.x - 92, o.y - 14}, dp = {o.x - 44, o.y + 34};
    DrawCircleV(st, 22, dark);
    DrawCircleV(st, 14, C_CYAN);
    DrawRectangle((int)dp.x - 7, (int)dp.y - 20, 14, 40, C_CYAN);
    DrawRectangle((int)dp.x - 20, (int)dp.y - 7, 40, 14, C_CYAN);
    DrawRectangleRounded((Rectangle){o.x + 10, o.y - 30, 24, 11}, 0.6f, 4, C_WHITE); /* start */
    DrawRectangleRounded((Rectangle){o.x - 34, o.y - 30, 24, 11}, 0.6f, 4, edge);    /* select */
    Vector2 fc = {o.x + 92, o.y - 4};
    static const char *XB[4] = {"Y", "X", "B", "A"};
    static const Color FC[4] = {{250, 210, 80, 255}, {80, 150, 255, 255}, {240, 90, 90, 255}, {110, 220, 120, 255}};
    static const int PS[4] = {1, 0, 2, 3};
    static const char *ACTS[4] = {"CHUTE  (golpe forte)", "SOCO  (golpe fraco)", "TIRO  (só a Elira)", "PULAR  /  confirmar"};
    Vector2 fp[4] = {{fc.x, fc.y - 26}, {fc.x - 26, fc.y}, {fc.x + 26, fc.y}, {fc.x, fc.y + 26}};
    for (int i = 0; i < 4; i++) DrawCircleV(fp[i], 13, FC[i]);
    /* legenda */
    float lx = pane.x + 400, ly = pane.y + 62;
    for (int i = 0; i < 4; i++)
    {
        float y = ly + i * 38;
        DrawCircleV((Vector2){lx + 14, y + 10}, 14, FC[i]);
        TxtC(XB[i], lx + 14, y + 1, 16, C_INK);
        DrawCircleV((Vector2){lx + 46, y + 10}, 14, (Color){30, 34, 52, 255});
        PsGlyph((Vector2){lx + 46, y + 10}, PS[i], 6, C_WHITE);
        Txt(ACTS[i], lx + 72, y + 1, 18, C_WHITE);
    }
    float y4 = ly + 4 * 38;
    DrawRectangleRounded((Rectangle){lx, y4, 60, 22}, 0.6f, 6, C_AMBER);
    TxtC("RB/RT", lx + 30, y4 + 4, 13, C_INK);
    Txt("ESPECIAL  (R1 / R2)", lx + 72, y4 + 1, 18, C_WHITE);
    DrawRectangleRounded((Rectangle){lx, y4 + 34, 60, 22}, 0.6f, 6, C_CYAN);
    TxtC("LB / L1", lx + 30, y4 + 38, 13, C_INK);
    Txt("DASH  (investida)", lx + 72, y4 + 35, 18, C_WHITE);
    DrawRectangleRounded((Rectangle){lx, y4 + 68, 60, 22}, 0.6f, 6, C_GRAB);
    TxtC("LT / L2", lx + 30, y4 + 72, 13, C_INK);
    Txt("AGARRÃO  (colado)", lx + 72, y4 + 69, 18, C_WHITE);
    DrawRectangleRounded((Rectangle){lx, y4 + 102, 60, 22}, 0.6f, 6, C_WHITE);
    TxtC("START", lx + 30, y4 + 106, 13, C_INK);
    Txt("PAUSA", lx + 72, y4 + 103, 18, C_WHITE);
    DrawCircleV((Vector2){lx + 14, y4 + 148}, 12, C_CYAN);
    Txt("MOVER  (analógico ou direcional)", lx + 34, y4 + 138, 18, C_WHITE);
    /* combinacoes (valem para os 4 lutadores) */
    float cy = pane.y + pane.height - 78;
    Txt("Frente + forte: SOCO LONGO    Cima + forte: UPPERCUT    Agachado + golpe: RASTEIRA", pane.x + 20, cy, 15, C_AMBER);
    Txt("Fraco > fraco > forte, acertando: COMBO    Fraco + forte juntos, colado: AGARRÃO", pane.x + 20, cy + 22, 15, C_AMBER);
    Txt("Agarrado? Fraco + forte juntos: ESCAPA    Para trás: DEFENDE    Frente 2x: DASH", pane.x + 20, cy + 44, 15, C_DIM);
    /* ---- teclado */
    Rectangle kp = {780, 92, 460, 470};
    DrawRectangleRounded(kp, 0.06f, 6, (Color){12, 14, 30, 235});
    DrawRectangleRoundedLinesEx(kp, 0.06f, 6, 2, Fade(C_CYAN, 0.6f));
    Txt("TECLADO", kp.x + 20, kp.y + 14, 22, C_CYAN);
    static const char *ACT[10] = {"Mover", "Pular", "Agachar", "Soco (fraco)", "Chute (forte)", "Dash", "Especial", "Tiro (Elira)", "Agarrão", "Pausa"};
    static const char *K1[10] = {"A / D  ou  SETAS", "W  ou  SETA CIMA", "S  ou  SETA BAIXO", "J  ou  Z", "K  ou  X", "2x para frente (D D)", "L  ou  C", "I  ou  V", "J + K  ou  Z + X", "P  ou  ESC"};
    static const char *KP1[10] = {"A / D", "W", "S", "J", "K", "D D", "L", "I", "J + K", "P"};
    static const char *KP2[10] = {"SETAS", "SETA CIMA", "SETA BAIXO", "1 (num.)", "2 (num.)", "SETA 2x", "3 (num.)", "4 (num.)", "1 + 2 (num.)", "P"};
    Txt("1 JOGADOR x CPU", kp.x + 20, kp.y + 50, 16, C_AMBER);
    for (int i = 0; i < 10; i++)
    {
        float y = kp.y + 74 + i * 19;
        Txt(ACT[i], kp.x + 20, y, 15, ACT[i][0] == 'D' ? C_CYAN : C_DIM);
        Txt(K1[i], kp.x + 170, y, 15, C_WHITE);
    }
    float y0 = kp.y + 270;
    Txt("1 x 1", kp.x + 20, y0, 16, C_AMBER);
    Txt("P1", kp.x + 170, y0, 16, C_CYAN);
    Txt("P2", kp.x + 300, y0, 16, C_RED);
    for (int i = 0; i < 9; i++)
    {
        float y = y0 + 22 + i * 18;
        Txt(ACT[i], kp.x + 20, y, 15, ACT[i][0] == 'D' ? C_CYAN : C_DIM);
        Txt(KP1[i], kp.x + 170, y, 15, C_WHITE);
        Txt(KP2[i], kp.x + 300, y, 15, C_WHITE);
    }
    (void)KP2[9];
    TxtC("No 1 x 1: P1 usa o controle 1 e P2 o controle 2; sem o 2º controle, o P2 joga no teclado.", SCREEN_WIDTH / 2.0f, 586, 16, C_DIM);
    TxtC("TREINO:  F1 / SELECT caixas de colisão    F2 / L3 boneco (parado, defende, pula)    F3 / R3 volta ao meio    F4 sprites da Elira", SCREEN_WIDTH / 2.0f, 616, 16, C_GREEN);
    TxtC("F1 / SELECT também mostra as caixas em qualquer luta.", SCREEN_WIDTH / 2.0f, 640, 14, C_DIM);
    TxtC("TAB / SELECT, ESC / B ou ENTER / A: voltar", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 34, 16, C_DIM);
}
static void UpdateControls(void)
{
    if (scT > 0.15f && (ControlsKeyP() || NavBackP() || NavOkP())) { iafScreen = ctrlBack; scT = 0; }
}

/* ------------------------------------------------------------------ tela de modo */
static const char *MODE_TXT[3] = {"1 JOGADOR x CPU", "1 x 1  (VERSUS)", "TREINO"};
static const char *MODE_SUB[3] = {"Arcade: enfrente as outras três inteligências, melhor de 3 rounds.",
                                  "Dois jogadores na mesma tela: controle ou teclado. Melhor de 3.",
                                  "Escolha o lutador e o boneco. Vida infinita, carga cheia e caixas (F1)."};
static const Color MODE_COL[3] = {{96, 214, 240, 255}, {230, 80, 84, 255}, {110, 220, 120, 255}};

static void DrawIafCyberCard(Rectangle r, Color accent, bool on, float tickMax)
{
    float pulse = 0.72f + 0.28f * sinf(gt * 3.2f);
    int bx = (int)r.x;
    int by = (int)r.y;
    int bw = (int)r.width;
    int bh = (int)r.height;

    /* 1. Aura / Glow externo suave em camadas (selecionado) ou sombra projetada (repouso) */
    if (on)
    {
        Rectangle glowOuter = {r.x - 10, r.y - 7, r.width + 20, r.height + 14};
        Rectangle glowInner = {r.x - 4, r.y - 4, r.width + 8, r.height + 8};
        DrawRectangleRounded(glowOuter, 0.20f, 8, Fade(accent, 0.14f + 0.05f * sinf(gt * 4.5f)));
        DrawRectangleRounded(glowInner, 0.16f, 8, Fade(accent, 0.28f));
    }
    else
    {
        Rectangle shadowRec = {r.x + 4, r.y + 5, r.width, r.height};
        DrawRectangleRounded(shadowRec, 0.14f, 8, Fade(C_INK, 0.60f));
    }

    /* 2. Base escura sci-fi com gradiente horizontal e reflexo especular superior */
    DrawRectangleRounded(r, 0.14f, 8, (Color){8, 13, 26, 246});

    Color gradLeft = on ? Fade(accent, 0.40f) : Fade(accent, 0.16f);
    Color gradRight = on ? Fade(accent, 0.08f) : Fade((Color){14, 18, 36, 255}, 0.05f);
    DrawRectangleGradientH(bx + 3, by + 2, bw - 6, bh - 4, gradLeft, gradRight);

    DrawRectangle(bx + 6, by + 3, bw - 12, (bh - 6) / 2,
                  Fade(C_WHITE, on ? 0.055f : 0.022f));

    /* 3. Feixe holografico varrendo o card quando selecionado */
    if (on)
    {
        float sweepW = 76.0f;
        float barX = r.x + fmodf(gt * 380.0f, r.width + sweepW * 2.0f) - sweepW;
        float clipStart = fmaxf(r.x + 8.0f, barX);
        float clipEnd = fminf(r.x + r.width - 8.0f, barX + sweepW);
        if (clipEnd > clipStart)
        {
            DrawRectangleGradientH((int)clipStart, by + 3, (int)(clipEnd - clipStart), bh - 6,
                                   Fade(C_WHITE, 0.0f), Fade(C_WHITE, 0.17f));
        }
    }

    /* 4. Barra lateral luminosa de energia (esquerda) */
    int stripeW = on ? 8 : 5;
    DrawRectangle(bx + 4, by + 6, stripeW + 6, bh - 12, Fade(accent, on ? 0.36f : 0.16f));
    DrawRectangle(bx + 4, by + 6, stripeW, bh - 12, on ? C_WHITE : accent);
    if (on)
    {
        DrawRectangle(bx + 5, by + 8, stripeW - 2, bh - 16, accent);
    }

    /* 5. Moldura principal e trilhos horizontais de telemetria */
    Color borderColor = on ? C_WHITE : Fade(accent, 0.62f * pulse + 0.18f);
    DrawRectangleRoundedLinesEx(r, 0.14f, 8, on ? 2.5f : 1.6f, borderColor);

    DrawLine(bx + 24, by + 4, bx + bw - 26, by + 4, Fade(accent, on ? 0.55f : 0.22f));
    DrawLine(bx + 24, by + bh - 5, bx + bw - 26, by + bh - 5, Fade(accent, on ? 0.55f : 0.22f));

    /* 6. Cantoneiras tecnologicas nos 4 cantos */
    float tickLen = on ? tickMax : (tickMax * 0.62f);
    float pad = 2.0f;
    float thick = on ? 2.5f : 2.0f;
    Color tickCol = on ? C_AMBER : accent;
    /* Superior esquerdo */
    DrawLineEx((Vector2){r.x + pad, r.y + pad},
               (Vector2){r.x + pad + tickLen, r.y + pad}, thick, tickCol);
    DrawLineEx((Vector2){r.x + pad, r.y + pad},
               (Vector2){r.x + pad, r.y + pad + tickLen}, thick, tickCol);
    /* Superior direito */
    DrawLineEx((Vector2){r.x + r.width - pad, r.y + pad},
               (Vector2){r.x + r.width - pad - tickLen, r.y + pad}, thick, tickCol);
    DrawLineEx((Vector2){r.x + r.width - pad, r.y + pad},
               (Vector2){r.x + r.width - pad, r.y + pad + tickLen}, thick, tickCol);
    /* Inferior esquerdo */
    DrawLineEx((Vector2){r.x + pad, r.y + r.height - pad},
               (Vector2){r.x + pad + tickLen, r.y + r.height - pad}, thick, tickCol);
    DrawLineEx((Vector2){r.x + pad, r.y + r.height - pad},
               (Vector2){r.x + pad, r.y + r.height - pad - tickLen}, thick, tickCol);
    /* Inferior direito */
    DrawLineEx((Vector2){r.x + r.width - pad, r.y + r.height - pad},
               (Vector2){r.x + r.width - pad - tickLen, r.y + r.height - pad}, thick, tickCol);
    DrawLineEx((Vector2){r.x + r.width - pad, r.y + r.height - pad},
               (Vector2){r.x + r.width - pad, r.y + r.height - pad - tickLen}, thick, tickCol);
}

/* ---- logo arcade: glifos da fonte fatiados em faixas (inclinacao + gradiente sem scissor) */
static Color LogoMix(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), 255};
}
static float LogoAdv(const Font *f, char ch, float sc)
{
    int idx = GetGlyphIndex(*f, ch);
    float adv = f->glyphs[idx].advanceX > 0 ? (float)f->glyphs[idx].advanceX : f->recs[idx].width;
    return adv * sc + 3.0f;
}
static float LogoWidth(const Font *f, const char *s, float sc)
{
    float w = 0;
    for (int i = 0; s[i]; i++) w += LogoAdv(f, s[i], sc);
    return w - 3.0f;
}
/* mode 0 = cor chapada; mode 1 = gradiente top/mid/bot + brilho especular em 'sweep' */
static void LogoPass(const Font *f, const char *s, float x, float y, float px, float ox, float oy,
                     int mode, Color flat, Color top, Color mid, Color bot, float sweep)
{
    const float skew = 0.20f, stepDst = 4.0f;
    float sc = px / (float)f->baseSize, pen = x;
    for (int i = 0; s[i]; i++)
    {
        int idx = GetGlyphIndex(*f, s[i]);
        Rectangle rec = f->recs[idx];
        float gx = pen + f->glyphs[idx].offsetX * sc;
        float gy = y + f->glyphs[idx].offsetY * sc;
        float stepSrc = stepDst / sc;
        for (float sy = 0; sy < rec.height; sy += stepSrc)
        {
            float h = fminf(stepSrc, rec.height - sy);
            float dy = gy + sy * sc, dh = h * sc;
            float rowMid = dy + dh * 0.5f;
            float dx = gx + ox - (rowMid - (y + px * 0.5f)) * skew;
            Color col = flat;
            if (mode)
            {
                float t = (rowMid - (y + px * 0.18f)) / (px * 0.62f);
                col = t < 0.5f ? LogoMix(top, mid, t * 2.0f) : LogoMix(mid, bot, (t - 0.5f) * 2.0f);
                float lineX = sweep - (rowMid - y) * 0.55f;
                float k = 1.0f - fabsf(dx + rec.width * sc * 0.5f - lineX) / 34.0f;
                if (k > 0) col = LogoMix(col, C_WHITE, k * 0.9f);
            }
            DrawTexturePro(f->texture, (Rectangle){rec.x, rec.y + sy, rec.width, h},
                           (Rectangle){dx, dy + oy, rec.width * sc, dh + 0.7f}, (Vector2){0, 0}, 0, col);
        }
        pen += LogoAdv(f, s[i], sc);
    }
}
static void Tri2(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}
static void DrawIafTitleLogo(void)
{
    const float cx = SCREEN_WIDTH / 2.0f;
    const float px = 84.0f, ty = 54.0f;
    Font f = GetGameFont(px);
    float sc = px / (float)f.baseSize;
    float wIA = LogoWidth(&f, "IA", sc), wSp = LogoAdv(&f, ' ', sc), wFi = LogoWidth(&f, "FIGHT", sc);
    float total = wIA + wSp + wFi;
    float x0 = cx - total / 2.0f;
    float xFi = x0 + wIA + wSp;
    float pulse = 0.5f + 0.5f * sinf(gt * 3.0f);

    /* aura quente + faixas de velocidade atras do titulo */
    for (int k = 4; k >= 1; k--)
        DrawEllipse((int)cx, 104, 330 + k * 34, 40 + k * 12, Fade((Color){255, 110, 30, 255}, 0.035f + 0.012f * pulse));
    Vector2 s1a = {cx - 380, 124}, s1b = {cx + 330, 62}, s1c = {cx + 380, 62}, s1d = {cx - 330, 124};
    Tri2(s1a, s1b, s1c, Fade((Color){200, 30, 40, 255}, 0.34f));
    Tri2(s1a, s1c, s1d, Fade((Color){200, 30, 40, 255}, 0.34f));
    DrawLineEx((Vector2){cx - 400, 138}, (Vector2){cx + 400, 70}, 2.0f, Fade(C_AMBER, 0.45f));
    DrawLineEx((Vector2){cx - 340, 146}, (Vector2){cx + 420, 90}, 1.0f, Fade(C_CYAN, 0.45f));
    DrawLineEx((Vector2){cx - 420, 112}, (Vector2){cx + 300, 52}, 1.0f, Fade(C_WHITE, 0.25f));

    const char *segS[2] = {"IA", "FIGHT"};
    float segX[2] = {x0, xFi};
    Color vinho = {110, 14, 28, 255}, vinhoDk = {64, 8, 20, 255};

    /* contorno de tinta ao redor da base e da extrusao */
    for (int q = 0; q < 2; q++)
    {
        for (int a = 0; a < 8; a++)
        {
            float ang = a * 0.7853982f;
            float ox = cosf(ang) * 3.5f, oy = sinf(ang) * 3.5f;
            LogoPass(&f, segS[q], segX[q], ty, px, ox, oy, 0, C_INK, C_INK, C_INK, C_INK, 0);
            LogoPass(&f, segS[q], segX[q], ty, px, ox + 10, oy + 14, 0, C_INK, C_INK, C_INK, C_INK, 0);
        }
        for (int a = 0; a < 4; a++)
        {
            float ang = a * 1.5707963f;
            LogoPass(&f, segS[q], segX[q], ty, px, 5 + cosf(ang) * 3.5f, 7 + sinf(ang) * 3.5f, 0, C_INK, C_INK, C_INK, C_INK, 0);
        }
    }
    /* extrusao 3D profunda em vinho */
    for (int q = 0; q < 2; q++)
        for (int k = 7; k >= 1; k--)
            LogoPass(&f, segS[q], segX[q], ty, px, k * 1.45f, k * 2.0f, 0, k > 4 ? vinhoDk : vinho, vinho, vinho, vinho, 0);

    /* preenchimento: 'IA' ciano/branco, 'FIGHT' ouro -> laranja -> fogo, com brilho varrendo */
    float sweep = fmodf(gt * 300.0f, total + 500.0f) + x0 - 250.0f;
    LogoPass(&f, "IA", x0, ty, px, 0, 0, 1, C_WHITE, (Color){235, 252, 255, 255}, C_CYAN, (Color){40, 110, 220, 255}, sweep);
    LogoPass(&f, "FIGHT", xFi, ty, px, 0, 0, 1, C_WHITE, (Color){255, 232, 70, 255}, (Color){255, 140, 30, 255}, (Color){226, 44, 30, 255}, sweep);

    /* faiscas / estrelas discretas */
    for (int s = 0; s < 9; s++)
    {
        float ph = fmodf(gt * 0.9f + s * 0.37f, 1.0f);
        float sx = x0 - 20 + fmodf(s * 97.0f, total + 40);
        float sy = ty - 6 + fmodf(s * 53.0f, 110.0f);
        float r = 7.0f * sinf(ph * 3.14159f);
        if (r < 0.5f) continue;
        Color sc2 = (s % 2) ? C_WHITE : C_AMBER;
        DrawRectanglePro((Rectangle){sx, sy, r * 2.0f, 2.0f}, (Vector2){r, 1}, 0, sc2);
        DrawRectanglePro((Rectangle){sx, sy, 2.0f, r * 2.0f}, (Vector2){1, r}, 0, sc2);
        DrawRectanglePro((Rectangle){sx, sy, r, r}, (Vector2){r / 2, r / 2}, 45, Fade(sc2, 0.6f));
    }

    /* placa chanfrada do subtitulo (nao invade o primeiro botao, y=240) */
    const char *sub = "RINHA DE INTELIGÊNCIAS";
    float sw = TxtW(sub, 20) + 96.0f, sh = 30.0f, bx = cx - sw / 2, by = 178.0f, ch = 11.0f;
    DrawRectangleGradientV((int)(bx + ch), (int)by, (int)(sw - ch * 2), (int)sh, (Color){22, 40, 66, 255}, (Color){8, 14, 30, 255});
    Tri2((Vector2){bx, by + sh - ch}, (Vector2){bx + ch, by}, (Vector2){bx + ch, by + sh}, (Color){8, 16, 34, 255});
    Tri2((Vector2){bx + sw, by + sh - ch}, (Vector2){bx + sw - ch, by}, (Vector2){bx + sw - ch, by + sh}, (Color){8, 16, 34, 255});
    DrawRectangle((int)(bx + ch), (int)by, (int)(sw - ch * 2), 3, C_AMBER);
    DrawRectangle((int)(bx + ch), (int)(by + sh - 3), (int)(sw - ch * 2), 3, C_CYAN);
    DrawLineEx((Vector2){bx, by + sh - ch}, (Vector2){bx + ch, by}, 2.5f, C_AMBER);
    DrawLineEx((Vector2){bx, by + sh - ch}, (Vector2){bx + ch, by + sh}, 2.5f, C_CYAN);
    DrawLineEx((Vector2){bx + sw, by + sh - ch}, (Vector2){bx + sw - ch, by}, 2.5f, C_AMBER);
    DrawLineEx((Vector2){bx + sw, by + sh - ch}, (Vector2){bx + sw - ch, by + sh}, 2.5f, C_CYAN);
    DrawRectangle((int)(bx + 18), (int)(by + sh / 2 - 3), 6, 6, C_AMBER);
    DrawRectangle((int)(bx + sw - 24), (int)(by + sh / 2 - 3), 6, 6, C_AMBER);
    TxtC(sub, cx + 1, by + 5, 20, C_INK);
    TxtC(sub, cx, by + 4, 20, C_CYAN);
}

static void DrawMode(void)
{
    DrawBackdrop(modeSel < 3 ? MODE_COL[modeSel] : C_AMBER);
    DrawIafTitleLogo();

    float pulse = 0.72f + 0.28f * sinf(gt * 3.2f);
    float slide = 3.5f + 2.8f * sinf(gt * 9.0f);

    for (int i = 0; i < 3; i++)
    {
        Rectangle r = {SCREEN_WIDTH / 2.0f - 300, 240.0f + i * 124, 600, 104};
        bool on = i == modeSel;
        Color c = MODE_COL[i];

        DrawIafCyberCard(r, c, on, 18.0f);

        /* Divisoria interna sutil entre titulo e subtitulo */
        DrawLine((int)r.x + 36, (int)r.y + 62, (int)(r.x + r.width - 36), (int)r.y + 62,
                 Fade(c, on ? 0.35f : 0.14f));

        /* Chevrons animados (focado) ou marcador geometrico (repouso) */
        if (on)
        {
            Txt(">>", r.x + 24 + slide, r.y + 21, 26, C_AMBER);
            Txt("<", r.x + r.width - 38 - slide * 0.6f, r.y + 21, 26, Fade(c, 0.95f));
        }
        else
        {
            DrawRectangle((int)(r.x + r.width - 22), (int)(r.y + 31), 6, 6, Fade(c, 0.55f * pulse));
        }

        /* Titulo com sombra profunda e halo luminoso */
        float cx = r.x + r.width / 2.0f + (on ? 2.0f : 0.0f);
        TxtC(MODE_TXT[i], cx + 2, r.y + 18, 38, Fade(C_INK, 0.90f));
        if (on)
        {
            TxtC(MODE_TXT[i], cx - 1, r.y + 16, 38, Fade(c, 0.42f));
            TxtC(MODE_TXT[i], cx + 1, r.y + 16, 38, Fade(c, 0.42f));
        }
        TxtC(MODE_TXT[i], cx, r.y + 16, 38, on ? C_WHITE : (Color){210, 230, 242, 235});

        /* Subtitulo com sombra para legibilidade nitida */
        TxtC(MODE_SUB[i], r.x + r.width / 2.0f + 1, r.y + 72, 16, Fade(C_INK, 0.85f));
        TxtC(MODE_SUB[i], r.x + r.width / 2.0f, r.y + 71, 16,
             on ? c : (Color){185, 200, 220, 210});
    }

    for (int k = 0; k < 2; k++)
    { /* volume do modo: 3 = musica, 4 = efeitos; A/D ou setas ajustam */
        Rectangle r = {SCREEN_WIDTH / 2.0f - 300 + k * 310, 612, 290, 54};
        bool on = modeSel == 3 + k;
        int lvl = k == 0 ? iafMusLvl : iafSfxLvl;
        const char *vLabel = (k == 0) ? "MÚSICA" : "EFEITOS";

        DrawIafCyberCard(r, C_AMBER, on, 13.0f);

        if (on)
        {
            Txt(">>", r.x + 15 + slide * 0.45f, r.y + 18, 16, C_AMBER);
        }

        float lblX = r.x + (on ? 38.0f : 20.0f);
        Txt(vLabel, lblX + 1, r.y + 18, 19, Fade(C_INK, 0.85f));
        Txt(vLabel, lblX, r.y + 17, 19, on ? C_WHITE : (Color){210, 225, 238, 225});

        Txt("<", r.x + 120, r.y + 15, 22, (on && lvl > 0) ? C_WHITE : (lvl > 0 ? Fade(C_AMBER, 0.75f) : Fade(C_DIM, 0.45f)));
        for (int b = 0; b < IAF_VOL_LVL_MAX; b++)
        {
            float h = 10 + b * 2.2f;
            Rectangle bar = {r.x + 138 + b * 12, r.y + 40 - h, 8, h};
            bool lit = (b < lvl);
            Color barCol = lit ? (on ? C_AMBER : Fade(C_AMBER, 0.65f)) : Fade(C_DIM, 0.28f);
            DrawRectangleRec(bar, barCol);
            if (lit && on)
            {
                DrawRectangleLines((int)bar.x, (int)bar.y, (int)bar.width, (int)bar.height, Fade(C_WHITE, 0.40f));
            }
        }
        Txt(">", r.x + 263, r.y + 15, 22, (on && lvl < IAF_VOL_LVL_MAX) ? C_WHITE : (lvl < IAF_VOL_LVL_MAX ? Fade(C_AMBER, 0.75f) : Fade(C_DIM, 0.45f)));
    }
    TxtC("W/S ou SETAS: escolher    A/D: volume    ENTER / A: confirmar    TAB / SELECT: controles    ESC / B: voltar ao menu", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 30, 16, C_DIM);
}
static void UpdateMode(void)
{
    if (NavUpP()) modeSel = (modeSel + 4) % 5;
    if (NavDownP()) modeSel = (modeSel + 1) % 5;
    if (modeSel >= 3)
    { /* faixa de volume: esquerda/direita mudam o nivel; o efeito de menu toca no volume novo */
        int *lvl = modeSel == 3 ? &iafMusLvl : &iafSfxLvl;
        int d = (NavRightP() ? 1 : 0) - (NavLeftP() ? 1 : 0);
        int nv = (int)Clamp((float)(*lvl + d), 0.0f, (float)IAF_VOL_LVL_MAX);
        if (nv != *lvl) { *lvl = nv; PlaySfx(SFX_MOVE); }
        return;
    }
    if (NavLeftP()) modeSel = (modeSel + 2) % 3;
    if (NavRightP()) modeSel = (modeSel + 1) % 3;
    if (scT > 0.15f && NavOkP())
    {
        versusMode = modeSel == 1;
        trainMode = modeSel == 2;
        trainPick = 0;
        showBoxes = trainMode; /* no treino as caixas ja comecam ligadas */
        if (versusMode)
        {
            cpu[0] = cpu[1] = false;
            readyP[0] = readyP[1] = false;
            readyT = 0;
            selP[0] = 0; selP[1] = 1;
        }
        else { cpu[0] = false; cpu[1] = true; }
        iafScreen = SC_SELECT;
        scT = 0;
    }
}

/* ------------------------------------------------------------------ 1 x 1: selecao simultanea */
static void DrawSelectVs(void) { DrawSelectScreen(true); }
static void UpdateSelectVs(float dt)
{
    if (IsKeyPressed(KEY_ESCAPE) || (!readyP[0] && PadPressed(0, VB_SHOT)) || (!readyP[1] && PadPressed(1, VB_SHOT)))
    {
        iafScreen = SC_MODE;
        scT = 0;
        return;
    }
    for (int p = 0; p < 2; p++)
    {
        if (!readyP[p])
        {
            if (PlayerNavP(p, PN_LEFT)) selP[p] = SelMove(selP[p], -1, 0);
            if (PlayerNavP(p, PN_RIGHT)) selP[p] = SelMove(selP[p], 1, 0);
            if (PlayerNavP(p, PN_UP)) selP[p] = SelMove(selP[p], 0, -1);
            if (PlayerNavP(p, PN_DOWN)) selP[p] = SelMove(selP[p], 0, 1);
            if (scT > 0.15f && PlayerNavP(p, PN_OK)) readyP[p] = true;
        }
        else if (PlayerNavP(p, PN_UNDO)) readyP[p] = false;
    }
    if (readyP[0] && readyP[1])
    {
        readyT += dt;
        if (readyT > 0.7f)
        {
            playerCh = selP[0];
            ladderPos = 0;
            ladder[0] = selP[1];
            OpenArenaSelect();
            readyT = 0;
        }
    }
    else readyT = 0;
}

/* ------------------------------------------------------------------ 1 x 1: vencedor */
static const char *RES_OPT[3] = {"REVANCHE", "TROCAR LUTADORES", "MENU"};
static void DrawResultVs(void)
{
    int w = wins[0] >= 2 ? 0 : 1;
    Fighter *fw = &fz[w];
    Color c = FCol(fw);
    DrawBackdrop(c);
    DrawEllipse(SCREEN_WIDTH / 2 - 220, 590, 170, 26, Fade(c, 0.25f));
    DrawEllipseLines(SCREEN_WIDTH / 2 - 220, 590, 175, 28, c);
    g_altPal = fw->alt;
    DrawCharacterBig(fw->ch, SCREEN_WIDTH / 2.0f - 220, 590, 1, 1.5f, ST_WIN);
    g_altPal = false;
    char b[64];
    snprintf(b, sizeof b, "P%d VENCE!", w + 1);
    TxtO(b, SCREEN_WIDTH / 2.0f, 40, 88, w == 0 ? C_CYAN : C_RED, C_INK);
    snprintf(b, sizeof b, "%s  (%d x %d)", NAME[fw->ch], wins[w], wins[1 - w]);
    TxtC(b, SCREEN_WIDTH / 2.0f, 140, 26, C_WHITE);
    Txt(WINQ[fw->ch], SCREEN_WIDTH / 2.0f - 20, 214, 17, C_DIM);
    for (int i = 0; i < 3; i++)
    {
        Rectangle r = {SCREEN_WIDTH / 2.0f + 40, 290.0f + i * 92, 420, 72};
        bool on = i == resSel;
        if (on) DrawRectangleRounded((Rectangle){r.x - 6, r.y - 6, r.width + 12, r.height + 12}, 0.25f, 8, Fade(c, 0.25f + 0.1f * sinf(gt * 5)));
        DrawRectangleRounded(r, 0.22f, 8, (Color){14, 18, 36, 255});
        DrawRectangleRoundedLinesEx(r, 0.22f, 8, on ? 3 : 1.5f, on ? c : Fade(c, 0.4f));
        TxtC(RES_OPT[i], r.x + r.width / 2, r.y + 20, 32, on ? C_WHITE : C_DIM);
    }
    TxtC("W/S, SETAS ou CONTROLE: escolher    ENTER / A: confirmar", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 30, 16, C_DIM);
}
static void UpdateResultVs(void)
{
    if (NavUpP()) resSel = (resSel + 2) % 3;
    if (NavDownP()) resSel = (resSel + 1) % 3;
    if (scT < 0.8f || !NavOkP()) return;
    if (resSel == 0) { iafScreen = SC_VERSUS; }                                                    /* revanche: mesmos lutadores */
    else if (resSel == 1) { readyP[0] = readyP[1] = false; readyT = 0; iafScreen = SC_SELECT; } /* trocar lutadores */
    else { iafScreen = SC_MODE; }
    scT = 0;
}

/* Limpa residuos de recorte (pixels semitransparentes, halo cinza e pixels orfaos)
   e endurece o contorno escuro externo dos atlas de pixel art (Elira e Antigravity). */
static Texture2D LoadFighterAtlasClean(const char *path)
{
    if (!FileExists(path)) return (Texture2D){0};
    Image img = LoadImage(path);
    if (!img.data) return (Texture2D){0};
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int w = img.width, h = img.height;
    Color *px = (Color *)img.data;

    /* 0. Fecha fendas transparentes de 1 px presas dentro do corpo do sprite
          (horizontais, verticais e diagonais de 45 graus presentes no atlas original) */
    Color *orig = (Color *)malloc((size_t)w * (size_t)h * sizeof(Color));
    if (orig)
    {
        memcpy(orig, px, (size_t)w * (size_t)h * sizeof(Color));
        for (int y = 1; y < h - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                int i = y * w + x;
                if (orig[i].a >= 55) continue;
                bool u = orig[i - w].a > 200;
                bool d = orig[i + w].a > 200;
                bool l = orig[i - 1].a > 200;
                bool r = orig[i + 1].a > 200;
                bool ul = orig[i - w - 1].a > 200;
                bool ur = orig[i - w + 1].a > 200;
                bool dl = orig[i + w - 1].a > 200;
                bool dr = orig[i + w + 1].a > 200;

                bool pinOrDiag = (u && d && l && r);
                bool vBridge = (u && d) && ((ul && dl) || (ur && dr));
                bool hBridge = (l && r) && ((ul && ur) || (dl && dr));

                if (pinOrDiag)
                {
                    Color a = orig[i - w], b = orig[i + w], c = orig[i - 1], e = orig[i + 1];
                    px[i] = (Color){(unsigned char)(((int)a.r + b.r + c.r + e.r) / 4),
                                    (unsigned char)(((int)a.g + b.g + c.g + e.g) / 4),
                                    (unsigned char)(((int)a.b + b.b + c.b + e.b) / 4), 255};
                }
                else if (vBridge)
                {
                    Color a = orig[i - w], b = orig[i + w];
                    px[i] = (Color){(unsigned char)(((int)a.r + b.r) / 2),
                                    (unsigned char)(((int)a.g + b.g) / 2),
                                    (unsigned char)(((int)a.b + b.b) / 2), 255};
                }
                else if (hBridge)
                {
                    Color a = orig[i - 1], b = orig[i + 1];
                    px[i] = (Color){(unsigned char)(((int)a.r + b.r) / 2),
                                    (unsigned char)(((int)a.g + b.g) / 2),
                                    (unsigned char)(((int)a.b + b.b) / 2), 255};
                }
            }
        }
        free(orig);
    }

    /* 1. Zera apenas poeira quase invisivel (c.a < 55) na borda externa;
          NUNCA apaga pixels internos cercados por pixels solidos (evita rasgos no torso/ombro) */
    for (int y = 1; y < h - 1; y++)
    {
        for (int x = 1; x < w - 1; x++)
        {
            int i = y * w + x;
            Color c = px[i];
            if (c.a == 0) continue;
            bool adjEmpty = (px[i - 1].a < 55 || px[i + 1].a < 55 ||
                             px[i - w].a < 55 || px[i + w].a < 55);
            if (!adjEmpty)
            {
                /* Pixel interno: garante opacidade solida se nao for efeito colorido suave */
                if (c.a >= 80) px[i].a = 255;
                continue;
            }
            unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
            unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
            if (c.a < 55 || (c.a < 195 && (mx - mn) <= 44))
            {
                px[i] = (Color){0, 0, 0, 0};
            }
        }
    }

    /* 2. Remove halo cinza/branco externo e filetes/espinhos soltos na borda (2 passes)
          Sem comer armadura branca solida (c.a == 255 com n4 >= 3) */
    unsigned char *rem = (unsigned char *)malloc((size_t)w * (size_t)h);
    if (rem)
    {
        for (int pass = 0; pass < 2; pass++)
        {
            int removed = 0;
            memset(rem, 0, (size_t)w * (size_t)h);
            for (int y = 1; y < h - 1; y++)
            {
                for (int x = 1; x < w - 1; x++)
                {
                    int i = y * w + x;
                    Color c = px[i];
                    if (c.a == 0) continue;
                    int n4 = (px[i - 1].a > 0) + (px[i + 1].a > 0) +
                             (px[i - w].a > 0) + (px[i + w].a > 0);
                    if (n4 == 4) continue;

                    unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
                    unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
                    bool neutral = (mx - mn) <= 40;

                    /* Remove apenas:
                       - pixels orfaos isolados (n4 <= 1) neutros
                       - filetes finos cinzas (n4 <= 2 com mx <= 220, nunca comendo branco puro opaco da armadura!)
                       - qualquer pixel neutro semitransparente (c.a < 252) na borda */
                    if ((n4 <= 1 && neutral && mn >= 38) ||
                        (n4 <= 2 && neutral && mn >= 52 && mx <= 220) ||
                        (neutral && mn >= 55 && c.a < 252))
                    {
                        rem[i] = 1;
                        removed++;
                    }
                }
            }
            for (int i = 0; i < w * h; i++)
            {
                if (rem[i]) px[i] = (Color){0, 0, 0, 0};
            }
            if (removed == 0) break;
        }
        free(rem);
    }

    /* 3. Endurece o contorno escuro externo (alpha 255 solido em C_INK) */
    for (int y = 1; y < h - 1; y++)
    {
        for (int x = 1; x < w - 1; x++)
        {
            int i = y * w + x;
            Color c = px[i];
            if (c.a == 0) continue;
            bool adjTrans = (px[i - 1].a == 0 || px[i + 1].a == 0 ||
                             px[i - w].a == 0 || px[i + w].a == 0);
            if (!adjTrans) continue;
            unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
            unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
            if ((mx - mn) <= 42 && mx <= 185)
            {
                px[i] = C_INK; /* contorno escuro 100% opaco e uniforme */
            }
            else if (c.a > 170)
            {
                px[i].a = 255;
            }
        }
    }

    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (t.id) SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

/* ------------------------------------------------------------------ interface com o jogo (iafight.h) */
static bool iafLoaded = false;

void IafEnter(GameState *game)
{
    if (!iafLoaded)
    {
        texElira = LoadFighterAtlasClean(IAF_ELIRA_ATLAS);
        texElira2 = LoadFighterAtlasClean(IAF_ELIRA_ATLAS_V2); /* sprites novos (modo de teste); sem o arquivo, id = 0 */
        texClaude = LoadTexture(IAF_CLAUDE_ATLAS);
        SetTextureFilter(texClaude, TEXTURE_FILTER_POINT); /* pixel art: sem suavizar */
        texAnti = LoadFighterAtlasClean(IAF_ANTI_ATLAS); /* sem o arquivo, id = 0 e volta o desenho antigo */
        texCodex = LoadFighterAtlasClean(IAF_CODEX_ATLAS);
        if (FileExists(IAF_IARA_ATLAS))
        { /* agua translucida: sem a limpeza de alfa dos outros atlas; POINT: pixel da arte nitido */
            texIara = LoadTexture(IAF_IARA_ATLAS);
            SetTextureFilter(texIara, TEXTURE_FILTER_POINT);
        }
        ArenaLoadAssets();
        ArenaBakePreviews();
        LoadIafAudio();
        iafLoaded = true;
    }
    cpu[0] = false;
    cpu[1] = true;
    paused = false;
    for (int i = 0; i < IAF_PADS; i++)
    { /* sincroniza: um botao ainda apertado no menu principal nao vale como clique aqui */
        ReadPadRaw(i, &padNow[i]);
        padPrev[i] = padNow[i];
        padKnown[i] = padNow[i].on;
    }
    iafScreen = SC_MODE;
    scT = 0;
    game->screen = SCREEN_IAFIGHT;
}

void IafUpdate(GameState *game, float dt)
{
    if (dt > 1 / 30.0f) dt = 1 / 30.0f;
    gt += dt;
    scT += dt;
    PollPads(dt);
    iafMuted = game->musicMuted;
    /* estado antes da atualizacao: mudou cursor = som de mover; mudou de tela/confirmou = som de confirmar */
    IafScreen pScr = iafScreen;
    int pSel = sel, pMode = modeSel, pS0 = selP[0], pS1 = selP[1], pRes = resSel, pAr = arSel;
    bool pR0 = readyP[0], pR1 = readyP[1];
    switch (iafScreen)
    {
    case SC_MODE:
        if (ControlsKeyP()) { OpenControls(); break; }
        if (NavBackP()) { UpdateIafMusic(-1); game->screen = SCREEN_TITLE; return; }
        UpdateMode();
        break;
    case SC_CONTROLS: UpdateControls(); break;
    case SC_SELECT:
        if (ControlsKeyP()) { OpenControls(); break; }
        if (versusMode) { UpdateSelectVs(dt); break; }
        if (NavBackP())
        {
            if (trainMode && trainPick == 1) { trainPick = 0; sel = playerCh; } /* volta a escolher o lutador */
            else { iafScreen = SC_MODE; scT = 0; }
            break;
        }
        UpdateSelect();
        break;
    case SC_ARENA:
        if (ControlsKeyP()) { OpenControls(); break; }
        UpdateArenaSelect(dt);
        break;
    case SC_VERSUS:
        if (NavOkP() || scT > 2.6f) { StartMatch(); iafScreen = SC_FIGHT; scT = 0; }
        else if (NavBackP()) OpenArenaSelect();
        break;
    case SC_FIGHT: UpdateFight(dt); break;
    case SC_RESULT:
        if (versusMode) { UpdateResultVs(); break; }
        if (scT > 0.6f && NavOkP())
        {
            if (lastWon) ladderPos++;
            iafScreen = SC_VERSUS;
            scT = 0;
        }
        else if (NavBackP()) { iafScreen = SC_SELECT; scT = 0; }
        break;
    case SC_FINAL:
        if (scT > 0.6f && (NavOkP() || NavBackP())) { iafScreen = SC_SELECT; scT = 0; }
        break;
    }
    if (sel != pSel || modeSel != pMode || selP[0] != pS0 || selP[1] != pS1 || resSel != pRes || arSel != pAr) PlaySfx(SFX_MOVE);
    if ((iafScreen != pScr && pScr != SC_FIGHT && iafScreen != SC_FIGHT) || (readyP[0] && !pR0) || (readyP[1] && !pR1)) PlaySfx(SFX_OK);
    UpdateIafMusic(iafScreen == SC_FIGHT ? 1 : 0);
}

void IafDraw(void)
{
    ClearBackground(C_BG);
    switch (iafScreen)
    {
    case SC_MODE: DrawMode(); break;
    case SC_CONTROLS: DrawControls(); break;
    case SC_SELECT: if (versusMode) DrawSelectVs(); else DrawSelect(); break;
    case SC_ARENA: DrawArenaSelect(); break;
    case SC_VERSUS: DrawVersus(); break;
    case SC_FIGHT: DrawFight(); break;
    case SC_RESULT: if (versusMode) DrawResultVs(); else IafDrawResult(false); break;
    case SC_FINAL: IafDrawResult(true); break;
    }
    DrawPadStatus(iafScreen != SC_FIGHT);
}

bool IafPadWantsEnter(void)
{ /* START ou A em qualquer controle abre o modo a partir do menu principal */
    for (int i = 0; i < IAF_PADS; i++)
        if (IsGamepadAvailable(i) && (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_MIDDLE_RIGHT) || IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))) return true;
    return false;
}

void IafUnload(void)
{
    if (iafLoaded) { UnloadTexture(texElira); if (texElira2.id) UnloadTexture(texElira2); texElira2 = (Texture2D){0}; if (texClaude.id) UnloadTexture(texClaude); texClaude = (Texture2D){0}; if (texAnti.id) UnloadTexture(texAnti); texAnti = (Texture2D){0}; if (texCodex.id) UnloadTexture(texCodex); texCodex = (Texture2D){0}; if (texIara.id) UnloadTexture(texIara); texIara = (Texture2D){0}; ArenaUnloadAssets(); UnloadIafAudio(); }
    iafLoaded = false;
}
