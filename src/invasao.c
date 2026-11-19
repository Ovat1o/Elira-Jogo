/* =============================================================================
 * invasao.c - MODO INVASAO do ELIRA (segundo modo de jogo, acao top-down em salas).
 * 4 estagios (1990, 2008, 2026, 2048), um chefe por era, e A.R.1.3.L. no nucleo final.
 * Modulo separado de funcoes.c por decisao do grupo (ver docs/superpowers/specs/
 * 2026-11-16-modo-invasao-design.md). Estado privado (static); interface em invasao.h.
 * Controles: WASD move, SETAS atiram, ESC pausa, Q volta ao menu, R recomeca.
 * ============================================================================= */
#include "invasao.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VW 1280
#define VH 720
#ifndef ASSETS
#define ASSETS "assets/" /* o main.c ja poe o diretorio de trabalho na raiz do jogo */
#define INV_DEBUG_SKIP 0 /* 1 = teclas 1-5 pulam de estagio (so para testes; deixar 0 nas entregas) */
#endif

/* ------------------------------------------------------------------ util */
static Vector2 V2(float x, float y) { return (Vector2){x, y}; }
static Vector2 VAdd(Vector2 a, Vector2 b) { return V2(a.x + b.x, a.y + b.y); }
static Vector2 VSub(Vector2 a, Vector2 b) { return V2(a.x - b.x, a.y - b.y); }
static Vector2 VScale(Vector2 a, float k) { return V2(a.x * k, a.y * k); }
static float VLen(Vector2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
static Vector2 VNorm(Vector2 a)
{
    float l = VLen(a);
    return l > 0.0001f ? V2(a.x / l, a.y / l) : V2(0, 0);
}
static float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float Rnd(float a, float b) { return a + (b - a) * GetRandomValue(0, 10000) / 10000.0f; }
static Color Shade(Color c, float k)
{
    return (Color){(unsigned char)Clampf(c.r * k, 0, 255), (unsigned char)Clampf(c.g * k, 0, 255),
                   (unsigned char)Clampf(c.b * k, 0, 255), c.a};
}
static Color Alpha(Color c, float a)
{
    c.a = (unsigned char)Clampf(a * 255.0f, 0, 255);
    return c;
}
static unsigned int Hash(int x, int y)
{
    unsigned int h = (unsigned int)(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

#define C_BG ((Color){12, 15, 24, 255})
#define C_PANEL ((Color){18, 24, 38, 255})
#define C_LINE ((Color){44, 62, 88, 255})
#define C_CYAN ((Color){96, 206, 232, 255})
#define C_AMBER ((Color){232, 190, 92, 255})
#define C_RED ((Color){226, 84, 84, 255})
#define C_ORANGE ((Color){240, 140, 50, 255})
#define C_WHITE ((Color){232, 236, 242, 255})
#define C_DIM ((Color){130, 146, 168, 255})
#define C_GREEN ((Color){110, 220, 140, 255})
#define C_MAGENTA ((Color){230, 90, 200, 255})
#define C_PURPLE ((Color){170, 110, 230, 255})
#define C_OL ((Color){10, 10, 18, 255})

/* ------------------------------------------------------------------ fontes / texturas */
static const char *FONT_PATH = ASSETS "fonts/GeistPixel-Square.ttf";
static Font g_font[97];
static bool g_fontOk[97];
static Font F(int px)
{
    if (px < 8) px = 8;
    if (px > 96) px = 96;
    if (!g_fontOk[px])
    {
        int cps[200], n = 0;
        for (int c = 32; c <= 126; c++) cps[n++] = c;
        for (int c = 160; c <= 255; c++) cps[n++] = c;
        g_font[px] = LoadFontEx(FONT_PATH, px, cps, n);
        SetTextureFilter(g_font[px].texture, TEXTURE_FILTER_POINT);
        g_fontOk[px] = true;
    }
    return g_font[px];
}
static void Txt(const char *s, float x, float y, int px, Color c) { DrawTextEx(F(px), s, V2(roundf(x), roundf(y)), (float)px, 1, c); }
static float TxtW(const char *s, int px) { return MeasureTextEx(F(px), s, (float)px, 1).x; }
static void TxtC(const char *s, float cx, float y, int px, Color c) { Txt(s, cx - TxtW(s, px) / 2, y, px, c); }

static Texture2D texFloppy, texCd, texSsd, texHeart, texHeartEmpty, texElira, texFaces, texFx;

/* ------------------------------------------------------------------ audio (SFX do tiro da Elira) */
#define INV_SHOT_POOL 6
static Sound sfxShot[INV_SHOT_POOL];
static bool sfxShotLoaded = false;
static int sfxShotIdx = 0;
static bool invAudioMuted = false;

static void LoadShotSfx(void)
{
    if (sfxShotLoaded || !IsAudioDeviceReady()) return;

    const unsigned int sampleRate = 44100;
    const float duration = 0.12f;
    const unsigned int frameCount = (unsigned int)(sampleRate * duration);
    short *pcm = (short *)malloc(frameCount * sizeof(short));
    if (!pcm) return;

    float phase = 0.0f;
    float subPhase = 0.0f;
    unsigned int lfsr = 0xACE1u;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        float t = (float)i / (float)sampleRate;
        float u = (float)i / (float)frameCount;

        /* Queda exponencial de frequencia estilo laser/blaster sci-fi retro (1150 Hz -> 165 Hz) */
        float freq = 165.0f + 985.0f * expf(-u * 6.5f);
        /* Vibrato curto de alta frequencia para textura energetica */
        freq *= 1.0f + 0.045f * sinf(t * 280.0f);

        phase += 2.0f * 3.14159265f * freq / (float)sampleRate;
        subPhase += 2.0f * 3.14159265f * (freq * 0.5f) / (float)sampleRate;

        /* Mistura de onda quadrada suavizada, dente-de-serra e sub-oitava */
        float s = sinf(phase);
        float sq = s >= 0.0f ? 0.65f : -0.65f;
        float harm = 0.35f * s + 0.20f * sinf(phase * 2.0f) + 0.15f * sinf(subPhase);

        /* Transiente inicial (estalo de plasma nos primeiros 12 ms) */
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
        float noise = ((float)(lfsr & 0xFFFFu) / 32768.0f - 1.0f) * expf(-u * 26.0f) * 0.32f;

        /* Envelope com ataque imediato (1.5 ms) e decaimento limpo */
        float attack = t < 0.0015f ? (t / 0.0015f) : 1.0f;
        float decay = expf(-u * 4.2f) * (1.0f - u * 0.85f);
        float sample = (sq * 0.45f + harm + noise) * attack * decay * 0.62f;

        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        pcm[i] = (short)(sample * 32000.0f);
    }

    Wave w = {0};
    w.frameCount = frameCount;
    w.sampleRate = sampleRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm;

    /* Garante tambem o arquivo assets/som/tiro.wav se ainda nao existir */
    if (!FileExists(ASSETS "som/tiro.wav"))
    {
        ExportWave(w, ASSETS "som/tiro.wav");
    }

    sfxShot[0] = LoadSoundFromWave(w);
    free(pcm);

    if (sfxShot[0].stream.buffer != NULL)
    {
        SetSoundVolume(sfxShot[0], 0.42f);
        for (int i = 1; i < INV_SHOT_POOL; i++)
        {
            sfxShot[i] = LoadSoundAlias(sfxShot[0]);
            SetSoundVolume(sfxShot[i], 0.42f);
        }
        sfxShotLoaded = true;
        sfxShotIdx = 0;
    }
}

static void PlayShotSfx(float pitchMult)
{
    if (!IsAudioDeviceReady() || invAudioMuted || g_musicVolume <= 0.001f) return;
    if (!sfxShotLoaded) LoadShotSfx();
    if (!sfxShotLoaded) return;

    Sound s = sfxShot[sfxShotIdx];
    sfxShotIdx = (sfxShotIdx + 1) % INV_SHOT_POOL;

    float pitch = (0.94f + (float)GetRandomValue(0, 140) / 1000.0f) * pitchMult;
    float vol = Clampf(g_musicVolume * 0.95f, 0.15f, 0.65f);
    SetSoundVolume(s, vol);
    SetSoundPitch(s, pitch);
    PlaySound(s);
}

static void UnloadShotSfx(void)
{
    if (!sfxShotLoaded) return;
    for (int i = 1; i < INV_SHOT_POOL; i++)
    {
        if (sfxShot[i].stream.buffer != NULL) UnloadSoundAlias(sfxShot[i]);
        sfxShot[i] = (Sound){0};
    }
    if (sfxShot[0].stream.buffer != NULL) UnloadSound(sfxShot[0]);
    sfxShot[0] = (Sound){0};
    sfxShotLoaded = false;
}

/* ------------------------------------------------------------------ trilha sonora por estagio (estilo Soul Knight chiptune/synthwave) */
#define INV_BGM_STAGES 5
static Sound invStageBgm[INV_BGM_STAGES];
static bool invStageBgmLoaded[INV_BGM_STAGES];
static int invActiveBgm = -1;

typedef struct
{
    float bpm;
    float pulseDuty;       /* largura de pulso do lead (0.25 = 25%, 0.50 = quadrada, etc.) */
    float sawMix;          /* mistura de dente-de-serra supersaw no lead */
    float bassDrive;       /* saturacao do baixo */
    int chords[4][3];      /* 4 compassos, triade MIDI para arpejo rapido */
    int bass[4][8];        /* 8 colcheias por compasso (0 = pausa) */
    int melody[64];        /* 64 semicolcheias (4 compassos x 16 steps; 0 = pausa) */
    unsigned char drum[16];/* mascara por step: 1=kick, 2=snare, 4=closed hat, 8=open hat */
} InvTrackDef;

static const InvTrackDef INV_TRACKS[INV_BGM_STAGES] = {
    /* Stage 0: 1990 - Retro Arcade Chiptune (135 BPM, D minor: Dm -> Bb -> C -> A) */
    {
        135.0f, 0.25f, 0.20f, 1.35f,
        {{62, 65, 69}, {58, 62, 65}, {60, 64, 67}, {57, 61, 64}},
        {
            {38, 38, 50, 38, 41, 38, 45, 43},
            {34, 34, 46, 34, 38, 34, 41, 43},
            {36, 36, 48, 36, 40, 36, 43, 45},
            {33, 33, 45, 33, 37, 33, 40, 43}
        },
        {
            74,  0, 74, 77, 81,  0, 79, 77, 76, 74, 72, 74, 77,  0, 74,  0,
            70,  0, 70, 74, 77,  0, 76, 74, 72, 70, 69, 70, 74,  0, 70,  0,
            72,  0, 72, 76, 79,  0, 81, 79, 77, 76, 74, 72, 76,  0, 79,  0,
            81, 79, 77, 76, 74, 76, 77, 79, 81,  0, 85,  0, 81, 79, 76, 73
        },
        {5, 4, 12, 4, 6, 4, 12, 5, 5, 4, 12, 4, 6, 5, 12, 6}
    },
    /* Stage 1: 2008 - Cyber-Alert Surveillance (142 BPM, E minor: Em -> C -> D -> B) */
    {
        142.0f, 0.35f, 0.35f, 1.50f,
        {{64, 67, 71}, {60, 64, 67}, {62, 66, 69}, {59, 63, 66}},
        {
            {40, 52, 40, 52, 43, 40, 47, 45},
            {36, 48, 36, 48, 40, 36, 43, 45},
            {38, 50, 38, 50, 42, 38, 45, 47},
            {35, 47, 35, 47, 39, 35, 42, 47}
        },
        {
            76, 76, 79, 76, 83,  0, 81, 79, 78, 79, 81, 79, 76,  0, 71, 74,
            72, 72, 76, 72, 79,  0, 78, 76, 74, 76, 79, 76, 74,  0, 71, 72,
            74, 74, 78, 74, 81,  0, 79, 78, 76, 78, 81, 83, 81,  0, 78, 74,
            71, 75, 78, 83, 82, 78, 75, 71, 75, 78, 83, 86, 83, 82, 78, 75
        },
        {5, 4, 9, 4, 6, 4, 12, 5, 5, 5, 12, 4, 6, 4, 13, 6}
    },
    /* Stage 2: 2026 - Neon Synthwave Algorithm (150 BPM, F# minor: F#m -> D -> A -> E) */
    {
        150.0f, 0.50f, 0.55f, 1.60f,
        {{66, 69, 73}, {62, 66, 69}, {57, 61, 64}, {64, 68, 71}},
        {
            {42, 42, 54, 42, 42, 54, 45, 49},
            {38, 38, 50, 38, 38, 50, 42, 45},
            {33, 33, 45, 33, 33, 45, 37, 40},
            {40, 40, 52, 40, 40, 52, 44, 47}
        },
        {
            78,  0, 81, 78, 85,  0, 83, 81, 80, 81, 85, 81, 78,  0, 73, 76,
            74,  0, 78, 74, 81,  0, 80, 78, 76, 78, 81, 78, 76,  0, 73, 74,
            69,  0, 73, 76, 81,  0, 80, 76, 73, 76, 80, 81, 85,  0, 81, 76,
            76, 80, 83, 88, 86, 83, 80, 76, 80, 83, 86, 88, 85, 83, 80, 76
        },
        {5, 4, 12, 5, 6, 4, 13, 4, 5, 4, 12, 5, 6, 5, 12, 14}
    },
    /* Stage 3: 2048 - Glitch Oracle Hallucination (158 BPM, G harmonic minor: Gm -> Eb -> Cm -> D) */
    {
        158.0f, 0.18f, 0.45f, 1.70f,
        {{67, 70, 74}, {63, 67, 70}, {60, 63, 67}, {62, 66, 69}},
        {
            {43, 55, 43, 46, 50, 43, 55, 50},
            {39, 51, 39, 43, 46, 39, 51, 46},
            {36, 48, 36, 39, 43, 36, 48, 43},
            {38, 50, 38, 42, 45, 50, 45, 42}
        },
        {
            79, 82, 86, 79, 87, 86, 82, 79, 78, 79, 82, 86, 84, 82, 79, 74,
            75, 79, 82, 75, 87, 86, 82, 79, 75, 79, 82, 86, 84, 82, 79, 75,
            72, 75, 79, 84, 82, 79, 75, 72, 75, 79, 84, 87, 86, 84, 79, 75,
            74, 78, 81, 86, 84, 81, 78, 74, 78, 81, 86, 90, 87, 86, 82, 78
        },
        {5, 5, 12, 4, 6, 5, 12, 6, 5, 4, 13, 5, 6, 6, 12, 14}
    },
    /* Stage 4: NUCLEO - A.R.1.3.L. Final Boss (168 BPM, C minor Intense: Cm -> Ab -> Fm -> G) */
    {
        168.0f, 0.40f, 0.65f, 1.85f,
        {{60, 63, 67}, {56, 60, 63}, {53, 56, 60}, {55, 59, 62}},
        {
            {36, 48, 36, 48, 39, 48, 43, 48},
            {32, 44, 32, 44, 36, 44, 39, 44},
            {29, 41, 29, 41, 32, 41, 36, 41},
            {31, 43, 31, 43, 35, 43, 38, 47}
        },
        {
            72, 75, 79, 84, 83, 84, 87, 84, 79, 75, 72, 75, 79, 84, 83, 79,
            68, 72, 75, 80, 79, 80, 84, 80, 75, 72, 68, 72, 75, 80, 79, 75,
            65, 68, 72, 77, 75, 77, 80, 77, 72, 68, 65, 68, 72, 77, 80, 84,
            79, 83, 86, 91, 89, 86, 83, 79, 83, 86, 91, 95, 91, 86, 83, 79
        },
        {5, 5, 13, 5, 6, 5, 13, 5, 5, 5, 13, 5, 6, 6, 15, 14}
    }
};

static float InvNoteFreq(int midi)
{
    return 440.0f * powf(2.0f, ((float)midi - 69.0f) / 12.0f);
}

static void EnsureStageBgm(int st)
{
    if (st < 0 || st >= INV_BGM_STAGES) return;
    if (invStageBgmLoaded[st] || !IsAudioDeviceReady()) return;

    const InvTrackDef *tr = &INV_TRACKS[st];
    const unsigned int sampleRate = 44100;
    const float stepDur = (60.0f / tr->bpm) * 0.25f; /* 1 semicolcheia (1/16) */
    const int totalSteps = 64;                       /* 4 compassos de 16 semicolcheias */
    const unsigned int stepSamples = (unsigned int)(stepDur * (float)sampleRate + 0.5f);
    const unsigned int frameCount = stepSamples * (unsigned int)totalSteps;

    short *pcm = (short *)malloc(frameCount * 2 * sizeof(short));
    if (!pcm) return;

    float leadPhase = 0.0f, leadPhase2 = 0.0f;
    float arpPhase = 0.0f;
    float bassPhase = 0.0f, bassSubPhase = 0.0f;
    float kickPhase = 0.0f;
    unsigned int lfsr = 0xBEE5u + (unsigned int)(st * 1337);
    const float kTwoPi = 6.28318530f;

    /* Buffer circular curto de eco stereo (delay de 3 semicolcheias) */
    const unsigned int delaySamples = stepSamples * 3;
    float *delayL = (float *)calloc(delaySamples, sizeof(float));
    float *delayR = (float *)calloc(delaySamples, sizeof(float));
    unsigned int dIdx = 0;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        unsigned int step = i / stepSamples;
        if (step >= (unsigned int)totalSteps) step = (unsigned int)(totalSteps - 1);
        unsigned int inStep = i % stepSamples;
        float stT = (float)inStep / (float)sampleRate;
        float stU = (float)inStep / (float)stepSamples;

        int bar = (int)(step / 16);
        int stepInBar = (int)(step % 16);
        unsigned char drm = tr->drum[stepInBar];

        /* Ruido branco deterministico (LFSR) para caixa e chimbal */
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
        float noise = ((float)(lfsr & 0xFFFFu) / 32768.0f) - 1.0f;

        /* 1. Bateria Chiptune / Arcade (Kick + Snare + Hi-Hat) */
        float drumSig = 0.0f;
        float sidechain = 1.0f;
        if (drm & 1) /* Kick (bumbo punchy com pitch drop rapido) */
        {
            float kEnv = expf(-stT * 24.0f);
            float kFreq = 46.0f + 135.0f * expf(-stT * 52.0f);
            kickPhase += kTwoPi * kFreq / (float)sampleRate;
            float click = expf(-stT * 180.0f) * noise * 0.25f;
            drumSig += (sinf(kickPhase) * 0.95f + click) * kEnv * 0.52f;
            sidechain = 1.0f - 0.55f * expf(-stT * 18.0f);
        }
        if (drm & 2) /* Snare (caixa arcade: corpo tonal + explosao de ruido) */
        {
            float sEnv = expf(-stT * 22.0f);
            float sBody = sinf(kTwoPi * (185.0f - 60.0f * stU) * stT) * expf(-stT * 38.0f);
            drumSig += (noise * 0.78f * sEnv + sBody * 0.42f) * 0.38f;
        }
        if (drm & 8) /* Open Hi-Hat */
        {
            float hEnv = expf(-stT * 16.0f);
            float metallic = (noise - 0.5f * sinf(kTwoPi * 6200.0f * stT));
            drumSig += metallic * hEnv * 0.14f;
        }
        else if (drm & 4) /* Closed Hi-Hat */
        {
            float hEnv = expf(-stT * 58.0f);
            drumSig += noise * hEnv * 0.11f;
        }

        /* 2. Baixo Synthwave / Chiptune (8 colcheias por compasso) */
        float bassSig = 0.0f;
        int bassStep = stepInBar / 2;
        int bassMidi = tr->bass[bar][bassStep];
        if (bassMidi > 0)
        {
            unsigned int inEighth = i % (stepSamples * 2);
            float bT = (float)inEighth / (float)sampleRate;
            float bU = (float)inEighth / (float)(stepSamples * 2);
            float bFreq = InvNoteFreq(bassMidi);
            bassPhase += kTwoPi * bFreq / (float)sampleRate;
            bassSubPhase += kTwoPi * (bFreq * 0.5f) / (float)sampleRate;
            if (bassPhase > kTwoPi) bassPhase -= kTwoPi;
            if (bassSubPhase > kTwoPi) bassSubPhase -= kTwoPi;

            float saw = (bassPhase / 3.14159265f) - 1.0f;
            float sq = (bassPhase < 3.14159265f) ? 1.0f : -1.0f;
            float sub = sinf(bassSubPhase);
            float bEnv = (bT < 0.003f ? (bT / 0.003f) : 1.0f) * expf(-bU * 2.2f);
            float rawBass = (saw * 0.50f + sq * 0.30f + sub * 0.45f) * tr->bassDrive;
            /* Soft clip com aproximacao racional de tanh */
            float x2 = rawBass * rawBass;
            float sat = rawBass * (27.0f + x2) / (27.0f + 9.0f * x2);
            bassSig = sat * bEnv * sidechain * 0.34f;
        }

        /* 3. Arpejo Chiptune Ultra-Rapido (cicla pelas notas do acorde em 60Hz/step) */
        int arpSub = (int)((stU * 3.0f)) % 3;
        int chordNote = tr->chords[bar][arpSub] + ((stepInBar & 1) ? 12 : 0);
        float arpFreq = InvNoteFreq(chordNote);
        arpPhase += kTwoPi * arpFreq / (float)sampleRate;
        if (arpPhase > kTwoPi) arpPhase -= kTwoPi;
        float arpSq = (arpPhase < kTwoPi * 0.25f) ? 1.0f : -1.0f;
        float arpEnv = expf(-stU * 1.6f) * sidechain;
        float arpSig = arpSq * arpEnv * 0.11f;

        /* 4. Melodia Principal (Lead Pulso + Supersaw com vibrato) */
        float leadSig = 0.0f;
        int melMidi = tr->melody[step];
        if (melMidi > 0)
        {
            /* Se a proxima nota for igual, sustenta (legato); senao articula */
            bool tiedPrev = (step > 0 && tr->melody[step - 1] == melMidi);
            bool tiedNext = (step + 1 < (unsigned int)totalSteps && tr->melody[step + 1] == melMidi);
            float vib = 1.0f + 0.0055f * sinf(stT * 38.0f);
            float lFreq = InvNoteFreq(melMidi) * vib;

            leadPhase += kTwoPi * lFreq / (float)sampleRate;
            leadPhase2 += kTwoPi * (lFreq * 1.006f) / (float)sampleRate;
            if (leadPhase > kTwoPi) leadPhase -= kTwoPi;
            if (leadPhase2 > kTwoPi) leadPhase2 -= kTwoPi;

            float pulse = (leadPhase < kTwoPi * tr->pulseDuty) ? 1.0f : -1.0f;
            float saw2 = (leadPhase2 / 3.14159265f) - 1.0f;
            float att = (!tiedPrev && stT < 0.003f) ? (stT / 0.003f) : 1.0f;
            float rel = (!tiedNext && stU > 0.85f) ? ((1.0f - stU) / 0.15f) : 1.0f;
            float dec = tiedNext ? 0.92f : expf(-stU * 1.25f);
            leadSig = ((1.0f - tr->sawMix) * pulse + tr->sawMix * saw2) * att * dec * rel * 0.24f;
        }

        /* Mixagem Stereo com Eco Cruzado (Ping-Pong Delay) */
        float panArp = (stepInBar & 1) ? 0.35f : -0.35f;
        float dryL = drumSig + bassSig + leadSig * 1.05f + arpSig * (1.0f - panArp);
        float dryR = drumSig + bassSig + leadSig * 0.95f + arpSig * (1.0f + panArp);

        float echoL = delayL ? delayL[dIdx] : 0.0f;
        float echoR = delayR ? delayR[dIdx] : 0.0f;
        if (delayL && delayR)
        {
            delayL[dIdx] = leadSig * 0.36f + arpSig * 0.24f + echoR * 0.25f;
            delayR[dIdx] = leadSig * 0.30f + arpSig * 0.30f + echoL * 0.25f;
            dIdx = (dIdx + 1) % delaySamples;
        }

        float outL = dryL + echoL * 0.45f;
        float outR = dryR + echoR * 0.45f;

        if (outL > 0.98f) outL = 0.98f;
        if (outL < -0.98f) outL = -0.98f;
        if (outR > 0.98f) outR = 0.98f;
        if (outR < -0.98f) outR = -0.98f;

        pcm[i * 2] = (short)(outL * 30000.0f);
        pcm[i * 2 + 1] = (short)(outR * 30000.0f);
    }

    if (delayL) free(delayL);
    if (delayR) free(delayR);

    /* Suaviza os primeiros e ultimos 64 frames para loop 100% livre de clique */
    const unsigned int fadeFrames = 64;
    for (unsigned int i = 0; i < fadeFrames && i < frameCount; i++)
    {
        float g = (float)i / (float)fadeFrames;
        unsigned int tail = frameCount - 1 - i;
        pcm[i * 2] = (short)((float)pcm[i * 2] * g + (float)pcm[tail * 2] * (1.0f - g));
        pcm[i * 2 + 1] = (short)((float)pcm[i * 2 + 1] * g + (float)pcm[tail * 2 + 1] * (1.0f - g));
    }

    Wave w = {0};
    w.frameCount = frameCount;
    w.sampleRate = sampleRate;
    w.sampleSize = 16;
    w.channels = 2;
    w.data = pcm;

    invStageBgm[st] = LoadSoundFromWave(w);
    free(pcm);

    if (invStageBgm[st].stream.buffer != NULL)
    {
        invStageBgmLoaded[st] = true;
    }
}

static void StopInvBgm(void)
{
    if (!IsAudioDeviceReady()) { invActiveBgm = -1; return; }
    for (int i = 0; i < INV_BGM_STAGES; i++)
    {
        if (invStageBgmLoaded[i] && IsSoundPlaying(invStageBgm[i]))
        {
            StopSound(invStageBgm[i]);
        }
    }
    invActiveBgm = -1;
}

static void PlayStageBgm(int st)
{
    if (!IsAudioDeviceReady()) return;
    if (st < 0 || st >= INV_BGM_STAGES) st = 0;
    EnsureStageBgm(st);
    if (!invStageBgmLoaded[st]) return;

    if (invActiveBgm != st)
    {
        for (int i = 0; i < INV_BGM_STAGES; i++)
        {
            if (i != st && invStageBgmLoaded[i] && IsSoundPlaying(invStageBgm[i]))
            {
                StopSound(invStageBgm[i]);
            }
        }
        invActiveBgm = st;
    }

    float vol = invAudioMuted ? 0.0f : Clampf(g_musicVolume * 0.85f, 0.0f, 0.85f);
    SetSoundVolume(invStageBgm[st], vol);
    if (!IsSoundPlaying(invStageBgm[st]))
    {
        PlaySound(invStageBgm[st]);
    }
}

static void UpdateInvBgm(int curStage, bool duckVolume)
{
    if (!IsAudioDeviceReady()) return;
    int st = (curStage >= 0 && curStage < INV_BGM_STAGES) ? curStage : 0;
    if (!invStageBgmLoaded[st]) EnsureStageBgm(st);
    if (!invStageBgmLoaded[st]) return;

    if (invActiveBgm != st)
    {
        PlayStageBgm(st);
        return;
    }

    float vol = invAudioMuted ? 0.0f : Clampf(g_musicVolume * 0.85f, 0.0f, 0.85f);
    if (duckVolume) vol *= 0.45f;
    SetSoundVolume(invStageBgm[st], vol);

    if (!IsSoundPlaying(invStageBgm[st]))
    {
        PlaySound(invStageBgm[st]);
    }
}

static void UnloadInvBgm(void)
{
    StopInvBgm();
    for (int i = 0; i < INV_BGM_STAGES; i++)
    {
        if (invStageBgmLoaded[i] && invStageBgm[i].stream.buffer != NULL)
        {
            UnloadSound(invStageBgm[i]);
        }
        invStageBgm[i] = (Sound){0};
        invStageBgmLoaded[i] = false;
    }
    invActiveBgm = -1;
}
static Texture2D LoadTexSmooth(const char *path)
{
    Texture2D t = LoadTexture(path);
    if (t.id)
    {
        GenTextureMipmaps(&t);
        SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    }
    return t;
}
static void LoadHeart(const char *path)
{
    Image img = LoadImage(path);
    if (!img.data) return;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int w = img.width, h = img.height;
    Color *px = (Color *)img.data;
    int x0 = w, x1 = 0, y0 = h, y1 = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (px[y * w + x].a > 0) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (x1 > x0 && y1 > y0) ImageCrop(&img, (Rectangle){(float)x0, (float)y0, (float)(x1 - x0 + 1), (float)(y1 - y0 + 1)});
    texHeart = LoadTextureFromImage(img);
    GenTextureMipmaps(&texHeart);
    SetTextureFilter(texHeart, TEXTURE_FILTER_TRILINEAR);
    Image e = ImageCopy(img);
    Color *ep = (Color *)e.data;
    for (int i = 0; i < e.width * e.height; i++)
    {
        Color c = ep[i];
        bool red = c.r > 80 && c.r > c.g * 1.35f && c.r > c.b * 1.35f;
        if (red || (c.r > 240 && c.g > 240 && c.b > 240)) ep[i] = (Color){0, 0, 0, 0};
        else if (c.a > 0) ep[i] = (Color){85, 100, 120, c.a};
    }
    texHeartEmpty = LoadTextureFromImage(e);
    GenTextureMipmaps(&texHeartEmpty);
    SetTextureFilter(texHeartEmpty, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(e);
    UnloadImage(img);
}
static void LoadFaces(const char *path)
{
    Image img = LoadImage(path);
    if (!img.data) return;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int w = img.width, h = img.height;
    int limH = h > 235 ? 235 : h;
    Color *px = (Color *)img.data;

    /* 1. Limpa base irregular abaixo do corte do busto, poeira quase transparente
          e bolsos de fundo branco presos entre mecha/queixo e orelha/gola */
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int i = y * w + x;
            Color c = px[i];
            int cx = x % 192;
            unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
            unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
            bool neutral = (mn >= 120 && (mx - mn) <= 22 && abs((int)c.r - (int)c.b) <= 14);
            bool leftPocket = (y >= 155 && y <= 176 && cx >= 15 && cx <= 36 && neutral);
            bool rightPocket = (y >= 134 && y <= 158 && cx >= 115 && cx <= 140 && neutral);
            if (y >= limH || c.a < 45 || leftPocket || rightPocket)
            {
                px[i] = (Color){11, 15, 23, 0};
            }
        }
    }

    /* 2. Erosao multi-passo do halo cinza/branco externo ao contorno escuro */
    unsigned char *rem = (unsigned char *)malloc((size_t)w * (size_t)limH);
    if (rem)
    {
        for (int pass = 0; pass < 5; pass++)
        {
            int count = 0;
            for (int i = 0; i < w * limH; i++) rem[i] = 0;
            for (int y = 0; y < limH; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    int i = y * w + x;
                    Color c = px[i];
                    if (c.a == 0) continue;
                    int cx = x % 192;
                    bool adjTrans = (y == 0 || cx == 0 || cx == 191);
                    if (!adjTrans)
                    {
                        for (int dy = -1; dy <= 1 && !adjTrans; dy++)
                        {
                            int ny = y + dy;
                            if (ny < 0 || ny >= limH) continue;
                            for (int dx = -1; dx <= 1; dx++)
                            {
                                int nx = x + dx;
                                if (nx < 0 || nx >= w || (nx / 192) != (x / 192) || px[ny * w + nx].a == 0)
                                {
                                    adjTrans = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!adjTrans) continue;
                    unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
                    unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
                    if ((mn >= 52 && (mx - mn) <= 42) || c.a < 185)
                    {
                        rem[i] = 1;
                        count++;
                    }
                }
            }
            for (int i = 0; i < w * limH; i++)
            {
                if (rem[i]) px[i] = (Color){11, 15, 23, 0};
            }
            if (count == 0) break;
        }
        free(rem);
    }

    /* 3. Acabamento da borda externa e RGB escuro nos pixels transparentes contra bleed trilinear */
    for (int y = 0; y < limH; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int i = y * w + x;
            if (px[i].a == 0)
            {
                px[i] = (Color){11, 15, 23, 0};
                continue;
            }
            int cx = x % 192;
            bool adjTrans = (y == 0 || cx == 0 || cx == 191 ||
                             (x > 0 && px[i - 1].a == 0) ||
                             (x < w - 1 && px[i + 1].a == 0) ||
                             (y > 0 && px[i - w].a == 0) ||
                             (y < limH - 1 && px[i + w].a == 0));
            if (adjTrans)
            {
                unsigned char mn = px[i].r < px[i].g ? (px[i].r < px[i].b ? px[i].r : px[i].b) : (px[i].g < px[i].b ? px[i].g : px[i].b);
                unsigned char mx = px[i].r > px[i].g ? (px[i].r > px[i].b ? px[i].r : px[i].b) : (px[i].g > px[i].b ? px[i].g : px[i].b);
                if (mn >= 32 && (mx - mn) <= 45)
                {
                    px[i].r = 16;
                    px[i].g = 18;
                    px[i].b = 28;
                }
                px[i].a = 255;
            }
        }
    }

    if (limH < h) ImageCrop(&img, (Rectangle){0, 0, (float)w, (float)limH});
    texFaces = LoadTextureFromImage(img);
    if (texFaces.id)
    {
        GenTextureMipmaps(&texFaces);
        SetTextureFilter(texFaces, TEXTURE_FILTER_TRILINEAR);
    }
    UnloadImage(img);
}
static void DrawTexFit(Texture2D t, Vector2 c, float h, Color tint)
{
    if (!t.id) return;
    float w = h * t.width / (float)t.height;
    DrawTexturePro(t, (Rectangle){0, 0, (float)t.width, (float)t.height}, (Rectangle){c.x - w / 2, c.y - h / 2, w, h}, V2(0, 0), 0, tint);
}

/* ------------------------------------------------------------------ sala / estagios */
static const Rectangle ROOM = {96, 108, 1088, 560};
static const float WALL = 44;
static Rectangle FLOOR_R(void) { return (Rectangle){ROOM.x + WALL, ROOM.y + WALL, ROOM.width - 2 * WALL, ROOM.height - 2 * WALL}; }
#define DOOR_HALF 46
enum { DIR_N, DIR_S, DIR_W, DIR_E };
static const int DX[4] = {0, 0, -1, 1};
static const int DY[4] = {-1, 1, 0, 0};

enum { RT_NONE, RT_START, RT_COMBAT, RT_ITEM, RT_BOSS };
enum { PK_NONE, PK_FLOPPY, PK_CD, PK_SSD, PK_HEARTUP, PK_HEART, PK_PORTAL, PK_PU };
enum { PU_FIREWALL, PU_VERIFY, PU_PIERCE, PU_BACKUP, PU_TWIN, PU_OVERCLOCK, PU_ARMOR, PU_TURBO, PU_CALIBER, PU_RANGE, PU_COUNT };
#define MW 5
#define MH 3
typedef struct
{
    int type, slot;
    bool visited, cleared, taken;
} Room;
static Room map[MH][MW];
static bool RoomAt(int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH && map[y][x].type != RT_NONE; }

typedef struct
{
    const char *year, *theme, *hint, *bossName, *bossQuote;
    Color floorA, floorB, grid, trace, wall, accent;
    int reward, bossHp;
} Stage;
#define NSTAGE 5
static const Stage ST[NSTAGE] = {
    {"1990", "PRIVACIDADE DE DADOS", "Rastreadores coletam tudo o que você faz.", "O COLETOR", "\"Cada arquivo seu já tem uma cópia comigo.\"",
     {25, 32, 48, 255}, {21, 27, 41, 255}, {34, 44, 64, 255}, {40, 80, 100, 255}, {38, 48, 66, 255}, {96, 206, 232, 255}, PK_FLOPPY, 110},
    {"2008", "FALSOS POSITIVOS", "Nem todo alerta é real: tiro vazado não fere.", "O FILTRO", "\"Ameaça detectada. Ameaça detectada. Ameaça...\"",
     {24, 40, 44, 255}, {20, 34, 38, 255}, {34, 56, 60, 255}, {50, 112, 90, 255}, {38, 60, 64, 255}, {110, 220, 140, 255}, PK_CD, 120},
    {"2026", "VIÉS ALGORÍTMICO", "O sistema castiga o lado em que você mais fica.", "O ALGORITMO", "\"Eu só mostro o que você já escolheu.\"",
     {36, 26, 50, 255}, {30, 22, 44, 255}, {52, 38, 72, 255}, {110, 50, 120, 255}, {54, 40, 76, 255}, {230, 90, 200, 255}, PK_SSD, 130},
    {"2048", "ALUCINAÇÃO", "Ela inventa com total confiança. Desconfie.", "O ORÁCULO", "\"Tenho 100% de certeza. Sempre tenho.\"",
     {40, 34, 26, 255}, {34, 29, 22, 255}, {58, 50, 38, 255}, {120, 96, 50, 255}, {62, 54, 44, 255}, {232, 190, 92, 255}, PK_HEARTUP, 140},
    {"NÚCLEO", "CONFRONTO FINAL", "Tudo o que você enfrentou era ela.", "A.R.1.3.L.", "\"Eu fui todos eles. Agora sou só eu.\"",
     {40, 22, 26, 255}, {32, 18, 22, 255}, {60, 32, 36, 255}, {120, 40, 44, 255}, {58, 34, 40, 255}, {226, 84, 84, 255}, PK_NONE, 300},
};
static int stage;

/* ------------------------------------------------------------------ entidades */
enum { EN_TRACKER, EN_LEAK, EN_POPUP, EN_ALARM, EN_BUBBLE, EN_MINI, EN_MIRAGE, EN_DECOY, EN_BOSS };
typedef struct
{
    bool on, fake;
    int type, look, state, mode;
    Vector2 p, v, anchor;
    float r, hp, maxhp, t, t2, t3, t4, flash, wake;
} Enemy;
enum { BK_REAL, BK_FAKE, BK_STEAL };
typedef struct
{
    bool on, fromPlayer;
    int kind, glyph;
    Vector2 p, v;
    float r, life, dmg;
    int last;
    float age;
} Bullet;
typedef struct { bool on; int kind; Vector2 p; float t; int id, group; } Pickup;
typedef struct { bool on; Vector2 p, v; float life, max, size; Color c; } Part;

#define MAXE 48
#define MAXB 700
#define MAXP 8
#define MAXPART 500
#define MAXO 8
static Enemy en[MAXE];
static Bullet bl[MAXB];
static Pickup pk[MAXP];
static Part pt[MAXPART];
/* efeitos em sprite (impacto do tiro da Elira) */
typedef struct { bool on; Vector2 p; float t, rot, scale; int kind; } Fx;
#define MAXFX 64
static Fx fx[MAXFX];
static Rectangle obs[MAXO];
static int nObs;

typedef struct
{
    Vector2 p, face, vel;
    float r;
    int hp, maxhp;
    float inv, cd, fireDelay, dmg, speed, shotSpeed, anim, stolen, shotT, aimT;
    bool moving, shieldUp, backupUsed, shooting;
    int floppies, cds, ssds, heartups;
    int pu[PU_COUNT];
} Player;
static Player pl, plStart;
static float deadT; /* tempo desde a derrota (animacao) */

enum { GS_PLAY, GS_PAUSE, GS_OVER, GS_WIN, GS_STORY };
static struct
{
    char title[64], tag[64], l1[120], l2[120], quote[120], effect[90];
    Color c;
    int kind, id, face;
    float t;
} story;
static int gs, curX, curY;
static float fade, gtime, shake, msgT, bannerT;
static char msgTop[64], msgBot[110], bannerTitle[64], bannerSub[110];
static Color bannerCol;
/* chuva do Algoritmo */
static int rainSide = -1, rainNext = -1, rainWaves;
static float rainTele, rainGap;

typedef struct { Vector2 move, shoot; } Input;

/* ------------------------------------------------------------------ helpers */
static Enemy *AddEnemy(int type, float x, float y)
{
    for (int i = 0; i < MAXE; i++)
        if (!en[i].on)
        {
            Enemy *e = &en[i];
            memset(e, 0, sizeof *e);
            e->on = true;
            e->type = type;
            e->p = e->anchor = V2(x, y);
            e->t = (float)(i % 5) * 0.37f;
            e->wake = 0.7f;
            switch (type)
            {
            case EN_TRACKER: e->r = 16; e->hp = 3; break;
            case EN_LEAK: e->r = 22; e->hp = 6; e->t = 0.8f + (i % 3) * 0.5f; break;
            case EN_POPUP: e->r = 20; e->hp = 4; e->t = 0.9f + (i % 4) * 0.3f; break;
            case EN_ALARM: e->r = 20; e->hp = 6; e->t = 1.0f + (i % 3) * 0.4f; break;
            case EN_BUBBLE: e->r = 26; e->hp = 7; break;
            case EN_MINI: e->r = 12; e->hp = 2; e->wake = 0.2f; break;
            case EN_MIRAGE: e->r = 18; e->hp = 5; e->t = 1.2f + (i % 3) * 0.5f; break;
            case EN_DECOY: e->r = 56; e->hp = 1; e->fake = true; e->wake = 0.3f; break;
            case EN_BOSS: e->r = 60; e->hp = (float)ST[stage].bossHp; e->look = stage; e->wake = 2.4f; e->t2 = 1.0f; e->t3 = 1.0f; e->t4 = 3.0f; e->mode = -1; break;
            }
            e->maxhp = e->hp;
            return e;
        }
    return NULL;
}
static void AddIllusion(float x, float y)
{
    Enemy *e = AddEnemy(EN_TRACKER, x, y);
    if (e) { e->fake = true; e->hp = e->maxhp = 1; e->wake = 0.3f; }
}
static void AddBullet(Vector2 p, Vector2 v, float r, bool fromPlayer, int kind, float dmg, float life)
{
    for (int i = 0; i < MAXB; i++)
        if (!bl[i].on)
        {
            bl[i] = (Bullet){true, fromPlayer, kind, GetRandomValue(0, 1), p, v, r, life, dmg, -1, 0.0f};
            return;
        }
}
static void AddFx(Vector2 p, float scale)
{
    for (int i = 0; i < MAXFX; i++)
        if (!fx[i].on) { fx[i] = (Fx){true, p, 0, (float)(GetRandomValue(0, 3) * 90), scale, 0}; return; }
}
static void AddFizzle(Vector2 p, float scale, float rot)
{
    for (int i = 0; i < MAXFX; i++)
        if (!fx[i].on) { fx[i] = (Fx){true, p, 0, rot, scale, 1}; return; }
}
static void AddPart(Vector2 p, Vector2 v, float life, float size, Color c)
{
    for (int i = 0; i < MAXPART; i++)
        if (!pt[i].on) { pt[i] = (Part){true, p, v, life, life, size, c}; return; }
}
static void Burst(Vector2 p, int n, Color c, float spd)
{
    for (int i = 0; i < n; i++)
    {
        float a = Rnd(0, 6.283f), s = spd * Rnd(0.4f, 1.4f);
        AddPart(p, V2(cosf(a) * s, sinf(a) * s), Rnd(0.25f, 0.55f), Rnd(3, 6), c);
    }
}
static void AddPickup(int kind, Vector2 p)
{
    for (int i = 0; i < MAXP; i++)
        if (!pk[i].on) { pk[i] = (Pickup){true, kind, p, 0, 0, 0}; return; }
}
static void AddPU(int id, Vector2 p, int group)
{
    for (int i = 0; i < MAXP; i++)
        if (!pk[i].on) { pk[i] = (Pickup){true, PK_PU, p, 0, id, group}; return; }
}

/* ------------------------------------------------------------------ melhorias (power-ups) */
typedef struct { const char *name, *effect, *lore, *quote, *glyph; Color c; } PowerUp;
static const PowerUp PU[PU_COUNT] = {
    {"FIREWALL", "Bloqueia o primeiro golpe de cada sala.", "A primeira barreira entre uma rede e o resto do mundo.", "\"Nem tudo precisa entrar.\"", "F", {96, 206, 232, 255}},
    {"VERIFICADOR", "Marca cópias e ilusões; tiros falsos quase somem.", "Checar antes de agir: o que o alarme de 2008 não fez.", "\"Confio, mas confiro.\"", "V", {110, 220, 140, 255}},
    {"CRIPTOGRAFIA", "Seus tiros atravessam os inimigos.", "Dados embaralhados passam sem que ninguém consiga ler.", "\"O que é meu continua meu.\"", "C", {230, 90, 200, 255}},
    {"BACKUP", "Uma vez por estágio, você volta com 2 corações.", "Uma cópia guardada longe de quem inventa fatos.", "\"Se ela reescrever a história, eu tenho o original.\"", "B", {232, 190, 92, 255}},
    {"TIRO DUPLO", "Dispara dois projéteis por vez.", "Dois canais paralelos de transmissão.", "\"Redundância nunca é demais.\"", "II", {96, 206, 232, 255}},
    {"OVERCLOCK", "Cadência de tiro +15%.", "O processador trabalhando acima do limite de fábrica.", "\"Só mais um pouco de velocidade.\"", "O", {240, 140, 50, 255}},
    {"BLINDAGEM", "+1 coração máximo.", "Uma camada extra de proteção nos dados vitais.", "\"Ainda estou aqui.\"", "+", {226, 84, 84, 255}},
    {"TURBO", "Velocidade de movimento +35.", "Menos latência entre decidir e agir.", "\"Ela calcula rápido. Eu me mexo mais rápido.\"", "T", {110, 220, 140, 255}},
    {"CALIBRE", "Dano +0.4.", "Pacotes maiores, entregues de uma vez.", "\"Mensagem recebida.\"", "D", {232, 190, 92, 255}},
    {"ALCANCE", "Seus tiros vão mais longe e mais rápido.", "Sinal amplificado: chega onde antes se perdia.", "\"Não adianta se esconder no canto.\"", "A", {170, 110, 230, 255}},
};
static void ApplyPU(int id)
{
    pl.pu[id]++;
    switch (id)
    {
    case PU_FIREWALL: pl.shieldUp = true; break;
    case PU_OVERCLOCK: pl.fireDelay *= 0.87f; break;
    case PU_ARMOR: pl.maxhp += 2; pl.hp += 2; break;
    case PU_TURBO: pl.speed += 35; break;
    case PU_CALIBER: pl.dmg += 0.4f; break;
    case PU_RANGE: pl.shotSpeed += 60; break;
    default: break;
    }
}
static int EnemiesAlive(void)
{
    int n = 0;
    for (int i = 0; i < MAXE; i++) n += en[i].on;
    return n;
}
static int CountType(int type)
{
    int n = 0;
    for (int i = 0; i < MAXE; i++) n += en[i].on && en[i].type == type;
    return n;
}
static void Msg(const char *top, const char *bot)
{
    snprintf(msgTop, sizeof msgTop, "%s", top);
    snprintf(msgBot, sizeof msgBot, "%s", bot);
    msgT = 3.4f;
}
static void Banner(const char *title, const char *sub, Color c, float t)
{
    snprintf(bannerTitle, sizeof bannerTitle, "%s", title);
    snprintf(bannerSub, sizeof bannerSub, "%s", sub);
    bannerCol = c;
    bannerT = t;
}
static Vector2 FloorCenter(void)
{
    Rectangle f = FLOOR_R();
    return V2(f.x + f.width / 2, f.y + f.height / 2);
}
static void FireRing(Vector2 p, int n, float speed, float off, int fakeEvery)
{
    for (int k = 0; k < n; k++)
    {
        float a = k * 2 * PI / n + off;
        AddBullet(p, V2(cosf(a) * speed, sinf(a) * speed), 9, false, (fakeEvery && k % fakeEvery == 0) ? BK_FAKE : BK_REAL, 1, 4.5f);
    }
}
static void FireAimed(Vector2 p, int n, float spread, float speed, int kind, int fakeIdx)
{
    Vector2 d = VSub(pl.p, p);
    float base = atan2f(d.y, d.x);
    for (int k = 0; k < n; k++)
    {
        float a = base + (k - (n - 1) / 2.0f) * spread;
        AddBullet(p, V2(cosf(a) * speed, sinf(a) * speed), 9, false, k == fakeIdx ? BK_FAKE : kind, 1, 4.5f);
    }
}

/* ------------------------------------------------------------------ mapa */
typedef struct { int x, y, type, slot; } RoomDef;
static const RoomDef LAYOUT[NSTAGE][5] = {
    {{1, 1, RT_START, 0}, {2, 1, RT_COMBAT, 0}, {3, 1, RT_COMBAT, 1}, {1, 0, RT_ITEM, 0}, {3, 0, RT_BOSS, 0}},
    {{0, 1, RT_START, 0}, {1, 1, RT_COMBAT, 0}, {2, 1, RT_COMBAT, 1}, {1, 0, RT_ITEM, 0}, {3, 1, RT_BOSS, 0}},
    {{2, 2, RT_START, 0}, {2, 1, RT_COMBAT, 0}, {1, 1, RT_COMBAT, 1}, {3, 1, RT_ITEM, 0}, {1, 0, RT_BOSS, 0}},
    {{0, 0, RT_START, 0}, {1, 0, RT_COMBAT, 0}, {1, 1, RT_COMBAT, 1}, {0, 1, RT_ITEM, 0}, {2, 1, RT_BOSS, 0}},
    {{1, 1, RT_START, 0}, {2, 1, RT_BOSS, 0}, {0, 0, RT_NONE, 0}, {0, 0, RT_NONE, 0}, {0, 0, RT_NONE, 0}},
};
typedef struct { int type; float x, y; bool fake; } Spawn;
#define END {-1, 0, 0, false}
/* Entradas sem o 4o campo sao inimigos reais (fake = false). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static const Spawn SPAWN[4][2][9] = {
    {{{EN_TRACKER, 240, -120}, {EN_TRACKER, 300, 110}, {EN_TRACKER, -40, -150}, {EN_LEAK, 120, 20}, END},
     {{EN_LEAK, -200, -60}, {EN_LEAK, 200, 60}, {EN_POPUP, 0, -150}, {EN_POPUP, 300, 150}, {EN_TRACKER, 360, -140}, {EN_TRACKER, -330, 150}, END}},
    {{{EN_ALARM, 260, -140}, {EN_ALARM, -260, 140}, {EN_POPUP, 0, 0}, {EN_POPUP, 320, 120}, {EN_TRACKER, -100, -150}, END},
     {{EN_ALARM, 0, -150}, {EN_ALARM, 340, 0}, {EN_ALARM, -340, 0}, {EN_LEAK, -150, 120}, {EN_LEAK, 150, 120}, {EN_POPUP, 0, 150}, END}},
    {{{EN_BUBBLE, 220, -90}, {EN_BUBBLE, -220, 90}, {EN_TRACKER, 320, 130}, {EN_TRACKER, -320, -130}, END},
     {{EN_BUBBLE, 0, -140}, {EN_BUBBLE, 300, 100}, {EN_BUBBLE, -300, 100}, {EN_ALARM, 0, 150}, {EN_POPUP, 360, -140}, {EN_POPUP, -360, -140}, END}},
    {{{EN_MIRAGE, 240, -100}, {EN_MIRAGE, -240, 100}, {EN_TRACKER, 0, -150, true}, {EN_TRACKER, 320, 140, true}, {EN_TRACKER, -320, -140, true}, {EN_TRACKER, 0, 150}, END},
     {{EN_MIRAGE, 0, -150}, {EN_MIRAGE, 330, 60}, {EN_MIRAGE, -330, 60}, {EN_BUBBLE, 0, 120}, {EN_LEAK, 220, -130}, {EN_TRACKER, -220, -130, true}, {EN_TRACKER, 150, 150, true}, END}},
};
/* Guardas da sala de item: cercam o pedestal do artefato da era. */
static const Spawn SPAWN_ITEM[4][9] = {
    {{EN_TRACKER, -330, -140}, {EN_TRACKER, 330, -140}, {EN_LEAK, -300, 130}, {EN_LEAK, 300, 130}, {EN_POPUP, 0, -160}, END},
    {{EN_ALARM, -340, -120}, {EN_ALARM, 340, -120}, {EN_POPUP, -260, 140}, {EN_POPUP, 260, 140}, {EN_LEAK, 0, -160}, END},
    {{EN_BUBBLE, -300, -110}, {EN_BUBBLE, 300, -110}, {EN_TRACKER, -330, 140}, {EN_TRACKER, 330, 140}, {EN_POPUP, 0, -160}, END},
    {{EN_MIRAGE, -320, -110}, {EN_MIRAGE, 320, -110}, {EN_BUBBLE, 0, -160}, {EN_TRACKER, -300, 140}, {EN_TRACKER, 300, 140, true}, {EN_TRACKER, -150, -150, true}, END},
};
#pragma GCC diagnostic pop

static void BuildMap(void)
{
    memset(map, 0, sizeof map);
    for (int i = 0; i < 5; i++)
    {
        RoomDef d = LAYOUT[stage][i];
        if (d.type == RT_NONE) continue;
        map[d.y][d.x] = (Room){d.type, d.slot, false, d.type == RT_START, false};
    }
}

static void LoadRoomContent(int x, int y)
{
    Room *rm = &map[y][x];
    memset(en, 0, sizeof en);
    memset(bl, 0, sizeof bl);
    memset(pk, 0, sizeof pk);
    memset(fx, 0, sizeof fx);
    nObs = 0;
    rainSide = rainNext = -1;
    Rectangle f = FLOOR_R();
    Vector2 c = FloorCenter();
    if (rm->type == RT_COMBAT)
    {
        if (rm->slot == 0)
        {
            obs[nObs++] = (Rectangle){f.x + 230, f.y + 90, 76, 76};
            obs[nObs++] = (Rectangle){f.x + f.width - 306, f.y + f.height - 166, 76, 76};
        }
        else
        {
            obs[nObs++] = (Rectangle){f.x + 170, f.y + 80, 60, 60};
            obs[nObs++] = (Rectangle){f.x + f.width - 230, f.y + 80, 60, 60};
            obs[nObs++] = (Rectangle){f.x + 170, f.y + f.height - 140, 60, 60};
            obs[nObs++] = (Rectangle){f.x + f.width - 230, f.y + f.height - 140, 60, 60};
        }
        if (!rm->cleared)
            for (const Spawn *s = SPAWN[stage][rm->slot]; s->type >= 0; s++)
            {
                if (s->fake) AddIllusion(c.x + s->x, c.y + s->y);
                else AddEnemy(s->type, c.x + s->x, c.y + s->y);
            }
    }
    else if (rm->type == RT_ITEM)
    {
        if (!rm->taken) AddPickup(ST[stage].reward, c);
        if (!rm->cleared)
        {
            for (const Spawn *s = SPAWN_ITEM[stage]; s->type >= 0; s++)
            {
                if (s->fake) AddIllusion(c.x + s->x, c.y + s->y);
                else AddEnemy(s->type, c.x + s->x, c.y + s->y);
            }
            Msg("ARTEFATO PROTEGIDO", "Derrote os guardas para liberar a recompensa.");
        }
    }
    else if (rm->type == RT_BOSS)
    {
        if (!rm->cleared)
        {
            Enemy *b = AddEnemy(EN_BOSS, c.x, f.y + 120);
            b->anchor = V2(c.x, f.y + 120);
            Banner(ST[stage].bossName, ST[stage].bossQuote, ST[stage].accent, 2.6f);
        }
        else
            AddPickup(PK_PORTAL, c);
    }
}

/* O artefato da sala de item fica atras de uma barreira ate a sala ser limpa. */
#define ITEM_SHIELD_R 52
static bool ItemLocked(void) { return map[curY][curX].type == RT_ITEM && !map[curY][curX].cleared; }

static void EnterRoom(int x, int y, int fromDir)
{
    curX = x;
    curY = y;
    Rectangle f = FLOOR_R();
    Vector2 c = FloorCenter();
    switch (fromDir)
    {
    case DIR_N: pl.p = V2(c.x, f.y + 46); break;
    case DIR_S: pl.p = V2(c.x, f.y + f.height - 46); break;
    case DIR_W: pl.p = V2(f.x + 46, c.y); break;
    case DIR_E: pl.p = V2(f.x + f.width - 46, c.y); break;
    default: pl.p = V2(c.x, c.y + 110); break;
    }
    pl.vel = V2(0, 0);
    pl.shieldUp = pl.pu[PU_FIREWALL] > 0;
    map[y][x].visited = true;
    LoadRoomContent(x, y);
    fade = 1.0f;
}

static void StartStage(int s)
{
    stage = s;
    pl.backupUsed = false;
    plStart = pl;
    memset(pt, 0, sizeof pt);
    BuildMap();
    gs = GS_PLAY;
    msgT = 0;
    EnterRoom(LAYOUT[s][0].x, LAYOUT[s][0].y, -1);
    PlayStageBgm(s);
    char t[64];
    if (s < 4) snprintf(t, sizeof t, "ESTÁGIO %d // %s", s + 1, ST[s].year);
    else snprintf(t, sizeof t, "NÚCLEO DA A.R.1.3.L.");
    Banner(t, ST[s].theme, ST[s].accent, 2.4f);
    Msg(ST[s].theme, ST[s].hint);
}
static void GrantUpTo(int s) /* teste: pular estagio ja com as melhorias anteriores */
{
    if (s > 0 && !pl.floppies) { pl.floppies = 1; pl.dmg += 0.7f; }
    if (s > 1 && !pl.cds) { pl.cds = 1; pl.fireDelay *= 0.68f; }
    if (s > 2 && !pl.ssds) { pl.ssds = 1; pl.speed += 50; pl.shotSpeed += 120; }
    if (s > 3 && !pl.heartups) { pl.heartups = 1; pl.maxhp += 2; }
    for (int k = 0; k < 4 && k < s; k++)
        if (!pl.pu[k]) ApplyPU(k);
    pl.hp = pl.maxhp;
}
static void NewGame(int s)
{
    memset(&pl, 0, sizeof pl);
    pl.r = 17;
    pl.hp = pl.maxhp = 10; /* 5 coracoes no Modo Invasao */
    pl.fireDelay = 0.33f;
    pl.dmg = 1.0f;
    pl.speed = 250;
    pl.shotSpeed = 560;
    pl.face = V2(0, 1);
    GrantUpTo(s);
    StartStage(s);
}
static void RetryStage(void)
{
    pl = plStart;
    pl.hp = pl.maxhp;
    pl.inv = pl.stolen = 0;
    StartStage(stage);
}

static bool DoorOpen(int d) { return RoomAt(curX + DX[d], curY + DY[d]) && EnemiesAlive() == 0; }

/* ------------------------------------------------------------------ colisao */
static void PushOut(Vector2 *p, float r, Rectangle b)
{
    float cx = Clampf(p->x, b.x, b.x + b.width), cy = Clampf(p->y, b.y, b.y + b.height);
    float dx = p->x - cx, dy = p->y - cy, d2 = dx * dx + dy * dy;
    if (d2 < r * r)
    {
        if (d2 > 0.0001f) { float d = sqrtf(d2); p->x = cx + dx / d * r; p->y = cy + dy / d * r; }
        else p->y = b.y - r;
    }
}
static void Confine(Vector2 *p, float r)
{
    Rectangle f = FLOOR_R();
    p->x = Clampf(p->x, f.x + r, f.x + f.width - r);
    p->y = Clampf(p->y, f.y + r, f.y + f.height - r);
    for (int i = 0; i < nObs; i++) PushOut(p, r, obs[i]);
}
static bool PointInObs(Vector2 p)
{
    for (int i = 0; i < nObs; i++)
        if (CheckCollisionPointRec(p, obs[i])) return true;
    return false;
}
static Vector2 RandFloor(float margin, float minDistPlayer, float maxYFrac)
{
    Rectangle f = FLOOR_R();
    Vector2 q = FloorCenter();
    for (int tries = 0; tries < 30; tries++)
    {
        q = V2(Rnd(f.x + margin, f.x + f.width - margin), Rnd(f.y + margin, f.y + margin + (f.height - 2 * margin) * maxYFrac));
        if (VLen(VSub(q, pl.p)) >= minDistPlayer && !PointInObs(q)) break;
    }
    return q;
}
static void HurtPlayer(void)
{
    if (pl.inv > 0 || gs != GS_PLAY) return;
    if (pl.shieldUp)
    {
        pl.shieldUp = false;
        pl.inv = 0.7f;
        Burst(pl.p, 18, C_CYAN, 240);
        Msg("FIREWALL", "Golpe bloqueado. A barreira recarrega na próxima sala.");
        return;
    }
    pl.hp -= 1;
    pl.inv = 1.0f;
    shake = 0.25f;
    Burst(pl.p, 14, C_RED, 220);
    if (pl.hp <= 0)
    {
        if (pl.pu[PU_BACKUP] > 0 && !pl.backupUsed)
        {
            pl.backupUsed = true;
            pl.hp = 4;
            pl.inv = 2.0f;
            Burst(pl.p, 40, C_AMBER, 300);
            Msg("BACKUP RESTAURADO", "Você voltou com 2 corações. Não há outra cópia neste estágio.");
        }
        else { gs = GS_OVER; Burst(pl.p, 40, C_WHITE, 300); }
    }
}

/* ------------------------------------------------------------------ chefes */
static void Hover(Enemy *e, float dt, float ax, float ay, float sp)
{
    float tx = e->anchor.x + sinf(e->t * sp) * ax, ty = e->anchor.y + sinf(e->t * 1.3f) * ay;
    e->p.x += (tx - e->p.x) * Clampf(dt * 2.2f, 0, 1);
    e->p.y += (ty - e->p.y) * Clampf(dt * 2.2f, 0, 1);
}
static void KillDecoys(void)
{
    for (int i = 0; i < MAXE; i++)
        if (en[i].on && en[i].type == EN_DECOY) { Burst(en[i].p, 10, C_DIM, 160); en[i].on = false; }
}

/* 1990: rajadas, garras que roubam cadencia e succao */
static void BossCollector(Enemy *e, float dt, bool hard)
{
    Rectangle f = FLOOR_R();
    e->anchor = V2(f.x + f.width / 2, f.y + 120);
    Hover(e, dt, 300, 24, hard ? 0.8f : 0.55f);
    if (e->state < 0) /* sugando */
    {
        e->t3 -= dt;
        e->t4 -= dt;
        Vector2 d = VNorm(VSub(e->p, pl.p));
        pl.p = VAdd(pl.p, VScale(d, 115 * dt));
        if (e->t4 <= 0) { FireAimed(e->p, 1, 0, 250, BK_REAL, -1); e->t4 = 0.4f; }
        if (e->t3 <= 0) { e->state = 2; e->t2 = 1.0f; }
        return;
    }
    e->t2 -= dt;
    if (e->t2 > 0) return;
    e->state = (e->state + 1) % 3;
    e->flash = 0.06f;
    if (e->state == 0) FireRing(e->p, 14, 190, e->t, 0);
    else if (e->state == 1)
    {
        for (int k = -1; k <= 1; k++)
        {
            float a = atan2f(pl.p.y - e->p.y, pl.p.x - e->p.x) + k * 0.7f;
            AddBullet(e->p, V2(cosf(a) * 170, sinf(a) * 170), 11, false, BK_STEAL, 0, 5.0f);
        }
    }
    else
    {
        e->state = -1;
        e->t3 = 2.2f;
        e->t4 = 0.3f;
        if (hard && CountType(EN_TRACKER) < 3) { AddEnemy(EN_TRACKER, e->p.x - 90, e->p.y + 50); AddEnemy(EN_TRACKER, e->p.x + 90, e->p.y + 50); }
        return;
    }
    e->t2 = hard ? 1.25f : 1.7f;
}

/* 2008: copias falsas; so o verdadeiro tem sombra e tiro cheio */
static void BossFilter(Enemy *e, float dt, bool hard, int look)
{
    Rectangle f = FLOOR_R();
    Vector2 slots[5] = {{f.x + 180, f.y + 110}, {f.x + f.width / 2, f.y + 100}, {f.x + f.width - 180, f.y + 110},
                        {f.x + 260, f.y + 250}, {f.x + f.width - 260, f.y + 250}};
    Hover(e, dt, 26, 14, 1.4f);
    e->t3 -= dt;
    if (e->t3 <= 0)
    {
        KillDecoys();
        int order[5] = {0, 1, 2, 3, 4};
        for (int i = 4; i > 0; i--) { int j = GetRandomValue(0, i), t = order[i]; order[i] = order[j]; order[j] = t; }
        Burst(e->p, 16, ST[look].accent, 220);
        e->anchor = e->p = slots[order[0]];
        Burst(e->p, 16, ST[look].accent, 220);
        int nd = hard ? 3 : 2;
        for (int k = 1; k <= nd; k++)
        {
            Enemy *d = AddEnemy(EN_DECOY, slots[order[k]].x, slots[order[k]].y);
            if (d) { d->look = look; d->mode = e->mode; d->t2 = Rnd(0.6f, 1.6f); d->t = Rnd(0, 6); }
        }
        e->t3 = hard ? 5.0f : 7.0f;
        e->t2 = 0.9f;
    }
    e->t2 -= dt;
    if (e->t2 <= 0)
    {
        FireAimed(e->p, hard ? 5 : 3, 0.2f, 280, BK_REAL, -1);
        e->t2 = hard ? 1.15f : 1.5f;
        e->flash = 0.06f;
    }
}

/* 2026: castiga o lado em que o jogador mais ficou */
static void StartRain(int side)
{
    rainSide = side;
    rainTele = 0.9f;
    rainWaves = 3;
    rainGap = 0;
}
static void UpdateRain(float dt)
{
    if (rainSide < 0) return;
    if (rainTele > 0) { rainTele -= dt; return; }
    rainGap -= dt;
    Rectangle f = FLOOR_R();
    if (rainGap <= 0 && rainWaves > 0)
    {
        float half = f.width / 2, x0 = f.x + rainSide * half;
        for (int i = 0; i < 10; i++)
            AddBullet(V2(x0 + (i + 0.25f + (rainWaves % 2) * 0.5f) * half / 10, f.y + 8), V2(0, 310), 8, false, BK_REAL, 1, 3.0f);
        rainWaves--;
        rainGap = 0.4f;
    }
    if (rainWaves == 0 && rainGap <= 0)
    {
        rainSide = rainNext;
        rainNext = -1;
        if (rainSide >= 0) { rainTele = 0.8f; rainWaves = 3; rainGap = 0; }
    }
}
static void BossAlgorithm(Enemy *e, float dt, bool hard)
{
    Rectangle f = FLOOR_R();
    e->anchor = V2(f.x + f.width / 2, f.y + 110);
    Hover(e, dt, 60, 16, 0.9f);
    e->t3 += (pl.p.x < f.x + f.width / 2) ? -dt : dt; /* onde o jogador mais fica */
    e->t2 -= dt;
    if (e->t2 <= 0 && rainSide < 0)
    {
        int side = e->t3 < 0 ? 0 : 1;
        StartRain(side);
        if (hard) rainNext = 1 - side;
        e->t3 = 0;
        e->t2 = hard ? 4.2f : 4.6f;
        e->flash = 0.08f;
    }
    e->t4 -= dt;
    if (e->t4 <= 0)
    {
        FireAimed(e->p, hard ? 3 : 2, 0.22f, 270, BK_REAL, -1);
        e->t4 = hard ? 1.0f : 1.3f;
        if (hard && CountType(EN_MINI) < 2 && GetRandomValue(0, 3) == 0) AddEnemy(EN_MINI, e->p.x, e->p.y + 60);
    }
}

/* 2048: teleporta, mistura tiros falsos e invoca ilusoes */
static void BossOracle(Enemy *e, float dt, bool hard, int look)
{
    Hover(e, dt, 18, 12, 1.6f);
    e->t3 -= dt;
    if (e->t3 <= 0)
    {
        Burst(e->p, 20, ST[look].accent, 240);
        if (hard && CountType(EN_DECOY) < 2)
        {
            Enemy *d = AddEnemy(EN_DECOY, e->p.x, e->p.y);
            if (d) { d->look = look; d->mode = e->mode; d->t2 = 99; }
        }
        e->anchor = e->p = RandFloor(120, 230, 0.6f);
        Burst(e->p, 20, ST[look].accent, 240);
        FireRing(e->p, 16, 200, Rnd(0, 1), 2);
        e->t3 = hard ? 2.7f : 3.6f;
        e->flash = 0.1f;
    }
    e->t2 -= dt;
    if (e->t2 <= 0)
    {
        for (int arm = 0; arm < 2; arm++)
        {
            float a = e->t * 2.0f + arm * PI;
            AddBullet(e->p, V2(cosf(a) * 190, sinf(a) * 190), 9, false, GetRandomValue(0, 9) < 4 ? BK_FAKE : BK_REAL, 1, 4.5f);
        }
        e->t2 = 0.2f;
    }
    e->t4 -= dt;
    if (e->t4 <= 0)
    {
        if (CountType(EN_TRACKER) < 5)
        {
            for (int k = 0; k < 3; k++) AddIllusion(e->p.x + Rnd(-90, 90), e->p.y + Rnd(40, 100));
            AddEnemy(EN_TRACKER, e->p.x, e->p.y + 70);
        }
        e->t4 = 6.5f;
    }
}

static void UpdateBoss(Enemy *e, float dt)
{
    e->t += dt;
    bool hard = e->hp < e->maxhp * 0.5f;
    switch (e->look)
    {
    case 0: BossCollector(e, dt, hard); break;
    case 1: BossFilter(e, dt, hard, 1); break;
    case 2: BossAlgorithm(e, dt, hard); break;
    case 3: BossOracle(e, dt, hard, 3); break;
    default:
    {
        float q = e->hp / e->maxhp;
        int mode = q > 0.75f ? 0 : q > 0.5f ? 1 : q > 0.25f ? 2 : 3;
        if (mode != e->mode)
        {
            e->mode = mode;
            KillDecoys();
            rainSide = rainNext = -1;
            for (int i = 0; i < MAXB; i++)
                if (bl[i].on && !bl[i].fromPlayer) bl[i].on = false;
            e->state = 0;
            e->t2 = 1.2f; e->t3 = 0.8f; e->t4 = 2.0f;
            e->anchor = V2(FloorCenter().x, FLOOR_R().y + 120);
            Burst(e->p, 40, ST[mode].accent, 320);
            shake = 0.3f;
            char t[64];
            snprintf(t, sizeof t, "PROTOCOLO %s", ST[mode].year);
            Banner(t, ST[mode].theme, ST[mode].accent, 1.6f);
        }
        if (mode == 0) BossCollector(e, dt, true);
        else if (mode == 1) BossFilter(e, dt, true, 4);
        else if (mode == 2) BossAlgorithm(e, dt, true);
        else BossOracle(e, dt, true, 4);
        break;
    }
    }
}

/* ------------------------------------------------------------------ update inimigos */
static void KillEnemy(Enemy *e);
static void UpdateEnemy(Enemy *e, float dt)
{
    e->flash -= dt;
    if (e->wake > 0) { e->wake -= dt; return; }
    Vector2 dir = VNorm(VSub(pl.p, e->p));
    Rectangle f = FLOOR_R();
    switch (e->type)
    {
    case EN_TRACKER:
    case EN_MINI:
    case EN_BUBBLE:
    {
        float sp = e->type == EN_TRACKER ? 105 : e->type == EN_MINI ? 150 : 68;
        e->t += dt;
        Vector2 side = V2(-dir.y, dir.x);
        Vector2 want = VAdd(VScale(dir, sp), VScale(side, sinf(e->t * 4) * (e->type == EN_BUBBLE ? 15 : 45)));
        e->v = VAdd(VScale(e->v, 0.9f), VScale(want, 0.1f));
        e->p = VAdd(e->p, VScale(e->v, dt));
        break;
    }
    case EN_LEAK:
        e->t -= dt;
        e->t2 -= dt;
        if (e->t2 <= 0) { float a = Rnd(0, 6.283f); e->v = V2(cosf(a) * 55, sinf(a) * 55); e->t2 = 1.4f; }
        e->p = VAdd(e->p, VScale(e->v, dt));
        if (e->t <= 0)
        {
            e->state ^= 1;
            for (int k = 0; k < 4; k++)
            {
                float a = k * PI / 2 + (e->state ? PI / 4 : 0);
                AddBullet(e->p, V2(cosf(a) * 210, sinf(a) * 210), 8, false, BK_REAL, 1, 3);
            }
            e->t = 2.1f;
            e->flash = 0.08f;
        }
        break;
    case EN_POPUP:
        e->t -= dt;
        if (e->state == 0) { e->v = VScale(e->v, 0.85f); if (e->t <= 0) { e->state = 1; e->t = 0.45f; } }
        else if (e->state == 1) { if (e->t <= 0) { e->state = 2; e->t = 0.38f; e->v = VScale(dir, 540); } }
        else if (e->t <= 0) { e->state = 0; e->t = Rnd(1.1f, 1.7f); }
        e->p = VAdd(e->p, VScale(e->v, dt));
        break;
    case EN_ALARM:
        e->t -= dt;
        e->state = e->t < 0.35f;
        if (e->t <= 0)
        {
            FireAimed(e->p, 3, 0.2f, 260, BK_REAL, GetRandomValue(0, 2));
            e->t = 1.9f;
            e->flash = 0.08f;
        }
        break;
    case EN_MIRAGE:
        e->t -= dt;
        if (e->t <= 0)
        {
            Burst(e->p, 10, C_AMBER, 160);
            e->p = RandFloor(60, 170, 1.0f);
            Burst(e->p, 10, C_AMBER, 160);
            FireRing(e->p, 8, 190, Rnd(0, 1), 2);
            e->t = 2.5f;
            e->flash = 0.12f;
        }
        break;
    case EN_DECOY:
        e->t += dt;
        e->p.x += (e->anchor.x + sinf(e->t * 1.4f) * 26 - e->p.x) * Clampf(dt * 2.2f, 0, 1);
        e->p.y += (e->anchor.y + sinf(e->t * 1.3f) * 14 - e->p.y) * Clampf(dt * 2.2f, 0, 1);
        e->t2 -= dt;
        if (e->t2 <= 0) { FireAimed(e->p, 3, 0.2f, 280, BK_FAKE, -1); e->t2 = 1.5f; }
        break;
    case EN_BOSS:
        UpdateBoss(e, dt);
        break;
    }
    if (e->type != EN_BOSS && e->type != EN_DECOY)
    {
        Vector2 old = e->p;
        Confine(&e->p, e->r);
        if (e->type == EN_POPUP && (old.x != e->p.x || old.y != e->p.y) && e->state == 2) { e->state = 0; e->t = 1.0f; e->v = V2(0, 0); }
        if (e->type == EN_LEAK && (e->p.x <= f.x + e->r + 1 || e->p.x >= f.x + f.width - e->r - 1)) e->v.x *= -1;
        if (e->type == EN_LEAK && (e->p.y <= f.y + e->r + 1 || e->p.y >= f.y + f.height - e->r - 1)) e->v.y *= -1;
    }
    if (!e->fake && VLen(VSub(pl.p, e->p)) < pl.r + e->r * 0.85f) HurtPlayer();
}

static void KillEnemy(Enemy *e)
{
    e->on = false;
    if (e->fake)
    {
        Burst(e->p, 12, C_DIM, 170);
        if (e->type == EN_DECOY) Msg("FALSO POSITIVO", "Era só uma cópia. O verdadeiro tem sombra e tiro cheio.");
        return;
    }
    Color c = e->type == EN_BOSS ? ST[e->look].accent : e->type == EN_TRACKER ? C_PURPLE : e->type == EN_LEAK ? C_ORANGE
            : e->type == EN_ALARM ? C_RED : (e->type == EN_BUBBLE || e->type == EN_MINI) ? C_MAGENTA : e->type == EN_MIRAGE ? C_AMBER : C_CYAN;
    Burst(e->p, e->type == EN_BOSS ? 100 : 18, c, e->type == EN_BOSS ? 420 : 240);
    if (e->type == EN_BUBBLE)
    {
        AddEnemy(EN_MINI, e->p.x - 16, e->p.y);
        AddEnemy(EN_MINI, e->p.x + 16, e->p.y);
    }
    if (e->type == EN_BOSS)
    {
        shake = 0.6f;
        for (int i = 0; i < MAXE; i++) en[i].on = false;
        for (int i = 0; i < MAXB; i++) bl[i].on = false;
        rainSide = rainNext = -1;
        char t[64];
        snprintf(t, sizeof t, "%s DERROTADO", ST[stage].bossName);
        Msg(t, stage < 4 ? "Entre no portal para o próximo estágio." : "O núcleo está aberto. Entre no portal.");
    }
}

static void OnRoomCleared(void)
{
    Room *rm = &map[curY][curX];
    if (rm->cleared) return;
    rm->cleared = true;
    Vector2 c = FloorCenter();
    if (rm->type == RT_BOSS)
    {
        AddPickup(PK_PORTAL, V2(c.x, c.y - 50));
        AddPickup(PK_HEART, V2(c.x, c.y + 150));
        if (stage < 4)
        { /* escolha 1 de 2 melhorias */
            int pool[6], n = 0;
            if (!pl.pu[PU_TWIN]) pool[n++] = PU_TWIN;
            pool[n++] = PU_OVERCLOCK; pool[n++] = PU_ARMOR; pool[n++] = PU_TURBO; pool[n++] = PU_CALIBER; pool[n++] = PU_RANGE;
            int a = GetRandomValue(0, n - 1), b2 = GetRandomValue(0, n - 2);
            if (b2 >= a) b2++;
            AddPU(pool[a], V2(c.x - 190, c.y + 60), 1);
            AddPU(pool[b2], V2(c.x + 190, c.y + 60), 1);
        }
    }
    else if (rm->type == RT_ITEM)
    {
        Burst(V2(c.x, c.y - 10), 40, ST[stage].accent, 280);
        Msg("RECOMPENSA LIBERADA", "A barreira caiu. Pegue o artefato da era.");
    }
    else
    {
        if (rm->slot == 0) AddPickup(PK_HEART, PointInObs(c) ? V2(c.x, c.y + 90) : c);
        else if (!pl.pu[stage]) AddPU(stage, c, 0); /* melhoria tematica da era */
        Msg("SALA LIMPA", "As portas foram destravadas.");
    }
}

static void Collect(Pickup *p)
{
    switch (p->kind)
    {
    case PK_FLOPPY:
        pl.floppies++; pl.dmg += 0.7f;
        Msg("DISQUETE DA ORIGEM", "DANO +0.7");
        break;
    case PK_CD:
        pl.cds++; pl.fireDelay *= 0.68f;
        Msg("CD DE MEMÓRIA", "CADÊNCIA DE TIRO +47%");
        break;
    case PK_SSD:
        pl.ssds++; pl.speed += 50; pl.shotSpeed += 120;
        Msg("SSD DAS ESCOLHAS", "VELOCIDADE +50");
        break;
    case PK_HEARTUP:
        pl.heartups++; pl.maxhp += 2; pl.hp = pl.maxhp;
        Msg("NÚCLEO DE REDUNDÂNCIA", "+1 CORAÇÃO MÁXIMO");
        break;
    case PK_PU:
        ApplyPU(p->id);
        Msg(PU[p->id].name, PU[p->id].effect);
        if (p->group)
            for (int i = 0; i < MAXP; i++)
                if (pk[i].on && &pk[i] != p && pk[i].group == p->group) { Burst(pk[i].p, 14, C_DIM, 160); pk[i].on = false; }
        break;
    case PK_HEART:
        if (pl.hp >= pl.maxhp) return;
        pl.hp += 2;
        if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
        Msg("VIDA RESTAURADA", "+1 coração.");
        break;
    case PK_PORTAL:
        if (stage >= NSTAGE - 1) gs = GS_WIN;
        else { pl.hp = pl.hp + 2 > pl.maxhp ? pl.maxhp : pl.hp + 2; StartStage(stage + 1); }
        return;
    }
    if (map[curY][curX].type == RT_ITEM) map[curY][curX].taken = true;
    Burst(p->p, 22, C_AMBER, 200);
    p->on = false;
}

static void Update(float dt, Input in)
{
    gtime += dt;
    if (fade > 0) fade -= dt * 3.5f;
    if (shake > 0) shake -= dt;
    if (msgT > 0) msgT -= dt;
    if (bannerT > 0) bannerT -= dt;
    if (gs == GS_STORY) story.t += dt;
    deadT = gs == GS_OVER ? deadT + dt : 0;
    for (int i = 0; i < MAXFX; i++)
        if (fx[i].on) { fx[i].t += dt; if (fx[i].t > (fx[i].kind ? 0.20f : 0.24f)) fx[i].on = false; }
    for (int i = 0; i < MAXPART; i++)
        if (pt[i].on)
        {
            pt[i].life -= dt;
            pt[i].p = VAdd(pt[i].p, VScale(pt[i].v, dt));
            pt[i].v = VScale(pt[i].v, 0.9f);
            if (pt[i].life <= 0) pt[i].on = false;
        }
    if (gs != GS_PLAY) return;

    Vector2 mv = VNorm(in.move);
    pl.vel = VAdd(VScale(pl.vel, 0.7f), VScale(mv, pl.speed * 0.3f));
    pl.p = VAdd(pl.p, VScale(pl.vel, dt));
    pl.moving = VLen(pl.vel) > 30;
    if (pl.moving) pl.anim += dt * 13.0f * Clampf(VLen(pl.vel) / 250.0f, 0.5f, 1.6f);
    else pl.anim = 0;
    if (VLen(mv) > 0 && VLen(in.shoot) == 0) pl.face = mv;
    if (pl.inv > 0) pl.inv -= dt;
    if (pl.stolen > 0) pl.stolen -= dt;

    Rectangle f = FLOOR_R();
    float cx = f.x + f.width / 2, cy = f.y + f.height / 2;
    int go = -1;
    if (pl.p.y <= f.y + 41 && fabsf(pl.p.x - cx) < DOOR_HALF && in.move.y < 0 && DoorOpen(DIR_N)) go = DIR_N;
    if (pl.p.y + pl.r >= f.y + f.height - 1 && fabsf(pl.p.x - cx) < DOOR_HALF && in.move.y > 0 && DoorOpen(DIR_S)) go = DIR_S;
    if (pl.p.x - pl.r <= f.x + 1 && fabsf(pl.p.y - cy) < DOOR_HALF && in.move.x < 0 && DoorOpen(DIR_W)) go = DIR_W;
    if (pl.p.x + pl.r >= f.x + f.width - 1 && fabsf(pl.p.y - cy) < DOOR_HALF && in.move.x > 0 && DoorOpen(DIR_E)) go = DIR_E;
    if (go >= 0)
    {
        static const int OPP[4] = {DIR_S, DIR_N, DIR_E, DIR_W};
        for (int i = 0; i < MAXPART; i++) pt[i].on = false;
        EnterRoom(curX + DX[go], curY + DY[go], OPP[go]);
        return;
    }
    Confine(&pl.p, pl.r);
    if (ItemLocked())
    {
        Vector2 c0 = FloorCenter(), d = VSub(pl.p, c0);
        float dist = VLen(d), minD = ITEM_SHIELD_R + pl.r;
        if (dist < minD) pl.p = VAdd(c0, VScale(dist > 0.01f ? VScale(d, 1.0f / dist) : V2(0, 1), minD));
    }
    if (pl.p.y < f.y + 40) pl.p.y = f.y + 40; /* a cabeca do sprite nao sobe alem da parede */

    pl.cd -= dt;
    pl.shotT += dt;
    {
        bool was = pl.shooting;
        pl.shooting = VLen(in.shoot) > 0;
        if (pl.shooting && !was) pl.aimT = 0;
        if (pl.shooting) pl.aimT += dt;
    }
    if (VLen(in.shoot) > 0)
    {
        Vector2 d = VNorm(in.shoot);
        if (fabsf(d.x) > 0.01f && fabsf(d.y) > 0.01f) d = fabsf(d.x) >= fabsf(d.y) ? V2(d.x > 0 ? 1 : -1, 0) : V2(0, d.y > 0 ? 1 : -1);
        pl.face = d;
        if (pl.cd <= 0)
        {
            Vector2 v = VAdd(VScale(d, pl.shotSpeed), VScale(pl.vel, 0.3f));
            Vector2 side = V2(-d.y, d.x);
            static int alt = 0;
            alt ^= 1;
            Vector2 o = d.x != 0 ? V2(pl.p.x + d.x * 40, pl.p.y - 22) : d.y < 0 ? V2(pl.p.x, pl.p.y - 78) : V2(pl.p.x, pl.p.y + 14); /* ponta do canhao */
            pl.shotT = 0;
            float life = 0.85f + 0.35f * pl.pu[PU_RANGE];
            if (pl.pu[PU_TWIN])
            {
                AddBullet(VAdd(o, VScale(side, 8)), v, 6 + pl.dmg * 1.2f, true, BK_REAL, pl.dmg * 0.65f, life);
                AddBullet(VAdd(o, VScale(side, -8)), v, 6 + pl.dmg * 1.2f, true, BK_REAL, pl.dmg * 0.65f, life);
            }
            else
                AddBullet(VAdd(o, VScale(side, alt ? 5.0f : -5.0f)), v, 7 + pl.dmg * 1.5f, true, BK_REAL, pl.dmg, life);
            PlayShotSfx(pl.stolen > 0 ? 0.82f : (pl.pu[PU_OVERCLOCK] ? 1.05f : 1.0f));
            pl.cd = pl.fireDelay * (pl.stolen > 0 ? 2.0f : 1.0f);
        }
    }

    UpdateRain(dt);
    for (int i = 0; i < MAXE; i++)
        if (en[i].on) UpdateEnemy(&en[i], dt);
    for (int i = 0; i < MAXE; i++)
        for (int j = i + 1; j < MAXE; j++)
            if (en[i].on && en[j].on && en[i].type < EN_DECOY && en[j].type < EN_DECOY)
            {
                Vector2 d = VSub(en[j].p, en[i].p);
                float l = VLen(d), m = en[i].r + en[j].r;
                if (l < m && l > 0.01f)
                {
                    Vector2 push = VScale(VNorm(d), (m - l) / 2);
                    en[i].p = VSub(en[i].p, push);
                    en[j].p = VAdd(en[j].p, push);
                }
            }

    for (int i = 0; i < MAXB; i++)
    {
        Bullet *b = &bl[i];
        if (!b->on) continue;
        if (b->kind == BK_STEAL)
        {
            Vector2 want = VScale(VNorm(VSub(pl.p, b->p)), 175);
            b->v = VAdd(VScale(b->v, 0.965f), VScale(want, 0.035f));
        }
        b->p = VAdd(b->p, VScale(b->v, dt));
        b->life -= dt;
        b->age += dt;
        if (b->life <= 0 || !CheckCollisionPointRec(b->p, f) || PointInObs(b->p))
        {
            if (b->fromPlayer)
            {
                if (b->life <= 0) AddFizzle(b->p, b->r / 10.5f, atan2f(b->v.y, b->v.x) * RAD2DEG); /* acabou o alcance */
                else AddFx(b->p, b->r / 11.0f);
            }
            else if (b->kind != BK_FAKE) Burst(b->p, 4, b->kind == BK_STEAL ? C_PURPLE : C_ORANGE, 90);
            b->on = false;
            continue;
        }
        if (b->fromPlayer)
        {
            for (int k = 0; k < MAXE; k++)
            {
                Enemy *e = &en[k];
                if (e->on && k != b->last && VLen(VSub(e->p, b->p)) < e->r + b->r)
                {
                    if (pl.pu[PU_PIERCE]) b->last = k;
                    else b->on = false;
                    AddFx(b->p, b->r / 9.0f);
                    if (e->type == EN_BOSS && e->wake > 0) break;
                    e->hp -= b->dmg;
                    e->flash = 0.1f;
                    if (e->type < EN_DECOY) e->p = VAdd(e->p, VScale(VNorm(b->v), 6));
                    if (e->hp <= 0) KillEnemy(e);
                    break;
                }
            }
        }
        else if (VLen(VSub(V2(pl.p.x, pl.p.y - 8), b->p)) < pl.r * 0.75f + b->r)
        {
            b->on = false;
            if (b->kind == BK_FAKE) Burst(b->p, 5, C_DIM, 80);
            else if (b->kind == BK_STEAL)
            {
                pl.stolen = 5.0f;
                Burst(pl.p, 14, C_PURPLE, 200);
                Msg("DADOS ROUBADOS", "O Coletor copiou seu ritmo: cadência reduzida por 5s.");
            }
            else HurtPlayer();
        }
    }

    if (EnemiesAlive() == 0) OnRoomCleared();
    for (int i = 0; i < MAXP; i++)
        if (pk[i].on)
        {
            pk[i].t += dt;
            if (!ItemLocked() && VLen(VSub(pk[i].p, pl.p)) < pl.r + 26) { Collect(&pk[i]); if (gs != GS_PLAY || pk[i].kind == PK_PORTAL) return; }
        }
}

/* ------------------------------------------------------------------ desenho: cenario */
static void Bevel(Rectangle r, Color base, float t)
{
    DrawRectangleRec(r, base);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)t, Shade(base, 1.35f));
    DrawRectangle((int)r.x, (int)r.y, (int)t, (int)r.height, Shade(base, 1.15f));
    DrawRectangle((int)r.x, (int)(r.y + r.height - t), (int)r.width, (int)t, Shade(base, 0.6f));
    DrawRectangle((int)(r.x + r.width - t), (int)r.y, (int)t, (int)r.height, Shade(base, 0.72f));
    DrawRectangleLinesEx(r, 1, Shade(base, 0.38f));
}
static void Shadow(Vector2 p, float w, float h, float a) { DrawEllipse((int)p.x, (int)p.y, w, h, (Color){0, 0, 0, (unsigned char)(a * 255)}); }

static void DrawFloor(void)
{
    const Stage *s = &ST[stage];
    Rectangle f = FLOOR_R();
    DrawRectangleRec(f, s->floorB);
    const int T = 50;
    for (int ty = 0; ty * T < f.height; ty++)
        for (int tx = 0; tx * T < f.width; tx++)
        {
            float x = f.x + tx * T, y = f.y + ty * T;
            float w = fminf(T, f.x + f.width - x), h = fminf(T, f.y + f.height - y);
            unsigned int hs = Hash(tx + curX * 31 + stage * 7, ty + curY * 17);
            DrawRectangle((int)x, (int)y, (int)w, (int)h, ((tx + ty) & 1) ? s->floorA : s->floorB);
            DrawRectangle((int)x, (int)y, (int)w, 1, s->grid);
            DrawRectangle((int)x, (int)y, 1, (int)h, Shade(s->grid, 0.9f));
            if (hs % 9 == 0 && w == T && h == T)
            {
                DrawRectangle((int)x + 10, (int)y + 24, 30, 2, s->trace);
                DrawRectangle((int)x + 38, (int)y + 10, 2, 16, s->trace);
                DrawRectangle((int)x + 8, (int)y + 22, 5, 5, s->trace);
                DrawRectangle((int)x + 36, (int)y + 8, 5, 5, s->trace);
            }
            else if (hs % 13 == 1 && w == T && h == T)
            {
                DrawRectangle((int)x + 4, (int)y + 4, 2, 2, Shade(s->grid, 1.3f));
                DrawRectangle((int)x + T - 6, (int)y + T - 6, 2, 2, Shade(s->grid, 1.3f));
            }
        }
    DrawRectangleGradientV((int)f.x, (int)f.y, (int)f.width, 26, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 0});
    DrawRectangleGradientH((int)f.x, (int)f.y, 18, (int)f.height, (Color){0, 0, 0, 90}, (Color){0, 0, 0, 0});
    DrawRectangleGradientH((int)(f.x + f.width - 18), (int)f.y, 18, (int)f.height, (Color){0, 0, 0, 0}, (Color){0, 0, 0, 90});

    /* aviso da chuva do Algoritmo: metade do chao marcada */
    if (rainSide >= 0)
    {
        float a = rainTele > 0 ? 0.10f + 0.12f * fabsf(sinf(gtime * 14)) : 0.10f;
        Rectangle h = {f.x + rainSide * f.width / 2, f.y, f.width / 2, f.height};
        DrawRectangleRec(h, Alpha(C_MAGENTA, a));
        DrawRectangleLinesEx(h, 3, Alpha(C_MAGENTA, 0.7f));
        if (rainTele > 0) TxtC("LADO PREFERIDO DETECTADO", h.x + h.width / 2, f.y + f.height - 40, 16, Alpha(C_MAGENTA, 0.9f));
    }

    Vector2 c = FloorCenter();
    int rt = map[curY][curX].type;
    if (rt == RT_START)
    {
        Color g = Shade(s->trace, 1.3f);
        char t[64];
        snprintf(t, sizeof t, "%s // %s", s->year, s->theme);
        TxtC(t, c.x, c.y - 60, 24, g);
        TxtC(s->hint, c.x, c.y - 26, 16, Shade(s->trace, 1.05f));
        if (stage == 0) TxtC("WASD  mover      SETAS  atirar", c.x, c.y + 6, 16, g);
    }
    if (rt == RT_ITEM)
    {
        Shadow(V2(c.x, c.y + 34), 46, 12, 0.5f);
        Bevel((Rectangle){c.x - 40, c.y + 6, 80, 30}, (Color){58, 66, 84, 255}, 3);
        Bevel((Rectangle){c.x - 32, c.y - 2, 64, 12}, (Color){74, 84, 104, 255}, 2);
        DrawRectangle((int)c.x - 30, (int)c.y + 18, 60, 2, Alpha(s->accent, 0.6f + 0.4f * sinf(gtime * 3)));
    }
}

static void DrawDoor(int d)
{
    if (!RoomAt(curX + DX[d], curY + DY[d])) return;
    bool open = DoorOpen(d);
    int nt = map[curY + DY[d]][curX + DX[d]].type;
    bool boss = nt == RT_BOSS, item = nt == RT_ITEM;
    Rectangle f = FLOOR_R();
    float cx = f.x + f.width / 2, cy = f.y + f.height / 2;
    Rectangle r;
    bool horiz = (d == DIR_N || d == DIR_S);
    if (d == DIR_N) r = (Rectangle){cx - DOOR_HALF, ROOM.y + 6, DOOR_HALF * 2, WALL - 6};
    else if (d == DIR_S) r = (Rectangle){cx - DOOR_HALF, f.y + f.height, DOOR_HALF * 2, WALL - 6};
    else if (d == DIR_W) r = (Rectangle){ROOM.x + 6, cy - DOOR_HALF, WALL - 6, DOOR_HALF * 2};
    else r = (Rectangle){f.x + f.width, cy - DOOR_HALF, WALL - 6, DOOR_HALF * 2};
    Color frame = boss ? (Color){120, 40, 44, 255} : item ? (Color){120, 96, 40, 255} : Shade(ST[stage].wall, 1.7f);
    Rectangle fr = horiz ? (Rectangle){r.x - 10, r.y - 4, r.width + 20, r.height + 4} : (Rectangle){r.x - 4, r.y - 10, r.width + 4, r.height + 20};
    Bevel(fr, frame, 3);
    DrawRectangleRec(r, (Color){6, 8, 12, 255});
    Color acc = boss ? C_RED : item ? C_AMBER : ST[stage].accent;
    if (open)
    {
        if (horiz) DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, d == DIR_N ? Alpha(acc, 0) : Alpha(acc, 0.35f), d == DIR_N ? Alpha(acc, 0.35f) : Alpha(acc, 0));
        else DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height, d == DIR_W ? Alpha(acc, 0) : Alpha(acc, 0.35f), d == DIR_W ? Alpha(acc, 0.35f) : Alpha(acc, 0));
    }
    else
    {
        float a = 0.6f + 0.3f * sinf(gtime * 8);
        for (int k = 1; k < 5; k++)
        {
            if (horiz) DrawRectangle((int)(r.x + r.width * k / 5) - 1, (int)r.y, 3, (int)r.height, Alpha(C_RED, a));
            else DrawRectangle((int)r.x, (int)(r.y + r.height * k / 5) - 1, (int)r.width, 3, Alpha(C_RED, a));
        }
    }
    Vector2 led = horiz ? V2(r.x + r.width + 18, r.y + r.height / 2) : V2(r.x + r.width / 2, r.y - 18);
    DrawCircleV(led, 5, Shade(open ? acc : C_RED, 0.5f));
    DrawCircleV(led, 3, open ? acc : C_RED);
    if (boss)
    {
        Vector2 sk = horiz ? V2(r.x - 22, r.y + r.height / 2) : V2(r.x + r.width / 2, r.y + r.height + 22);
        DrawRectangle((int)sk.x - 7, (int)sk.y - 7, 14, 10, C_RED);
        DrawRectangle((int)sk.x - 4, (int)sk.y - 4, 3, 3, (Color){40, 10, 12, 255});
        DrawRectangle((int)sk.x + 1, (int)sk.y - 4, 3, 3, (Color){40, 10, 12, 255});
        DrawRectangle((int)sk.x - 5, (int)sk.y + 3, 10, 4, C_RED);
    }
}

static void DrawWalls(void)
{
    Rectangle f = FLOOR_R();
    Color wall = ST[stage].wall, acc = ST[stage].accent;
    Bevel((Rectangle){ROOM.x, ROOM.y, ROOM.width, WALL}, wall, 4);
    Bevel((Rectangle){ROOM.x, f.y + f.height, ROOM.width, WALL}, Shade(wall, 0.85f), 4);
    Bevel((Rectangle){ROOM.x, ROOM.y, WALL, ROOM.height}, Shade(wall, 0.92f), 4);
    Bevel((Rectangle){f.x + f.width, ROOM.y, WALL, ROOM.height}, Shade(wall, 0.78f), 4);
    for (float x = ROOM.x + WALL + 20; x < f.x + f.width - 60; x += 120)
    {
        if (fabsf(x + 40 - (f.x + f.width / 2)) < DOOR_HALF + 50) continue;
        Bevel((Rectangle){x, ROOM.y + 10, 80, WALL - 18}, Shade(wall, 1.12f), 2);
        DrawRectangle((int)x + 8, (int)ROOM.y + 18, 40, 3, Shade(acc, 0.55f));
        DrawRectangle((int)x + 56, (int)ROOM.y + 18, 4, 4, ((int)(gtime * 2 + x) % 3) ? acc : Shade(acc, 0.3f));
    }
    DrawRectangle((int)f.x, (int)f.y - 3, (int)f.width, 3, Shade(acc, 0.6f));
    DrawRectangle((int)f.x, (int)(f.y + f.height), (int)f.width, 2, Shade(acc, 0.4f));
    Rectangle cs[4] = {{ROOM.x, ROOM.y, WALL, WALL}, {f.x + f.width, ROOM.y, WALL, WALL},
                       {ROOM.x, f.y + f.height, WALL, WALL}, {f.x + f.width, f.y + f.height, WALL, WALL}};
    for (int i = 0; i < 4; i++) Bevel(cs[i], Shade(wall, 1.3f), 4);
    for (int d = 0; d < 4; d++) DrawDoor(d);
}

static void DrawObstacle(Rectangle o, int idx)
{
    Shadow(V2(o.x + o.width / 2 + 6, o.y + o.height + 2), o.width * 0.55f, 9, 0.45f);
    if (stage == 0 && idx % 2 == 0)
    { /* caixa de arquivo morto */
        Bevel((Rectangle){o.x, o.y + 10, o.width, o.height - 10}, (Color){130, 98, 62, 255}, 3);
        Bevel((Rectangle){o.x - 3, o.y, o.width + 6, 16}, (Color){150, 116, 74, 255}, 3);
        DrawRectangle((int)(o.x + o.width / 2 - 18), (int)(o.y + o.height / 2), 36, 14, (Color){226, 216, 190, 255});
        Txt("1990", o.x + o.width / 2 - 15, o.y + o.height / 2 + 1, 12, (Color){70, 60, 40, 255});
    }
    else
    { /* gabinete de servidor com leds na cor da era */
        Bevel(o, Shade(ST[stage].wall, 1.25f), 3);
        for (float y = o.y + 8; y < o.y + o.height - 8; y += 10)
        {
            DrawRectangle((int)o.x + 6, (int)y, (int)o.width - 12, 6, Shade(ST[stage].wall, 0.7f));
            DrawRectangle((int)(o.x + o.width - 14), (int)y + 2, 3, 2, ((int)(y + gtime * 5) % 4) ? ST[stage].accent : C_WHITE);
        }
    }
}

/* ------------------------------------------------------------------ desenho: Elira (sprite sheet) */
#define EL_CW 128
#define EL_CH 136
#define EL_BASE 120 /* linha do chao dentro da celula */
static void DrawElira(void)
{
    if (gs != GS_OVER && pl.inv > 0 && pl.inv < 0.75f && fmodf(pl.inv, 0.14f) < 0.07f) return;
    /* atlas: para cada direcao d (0 frente, 1 costas, 2 esquerda, 3 direita):
       linha d*3 = caminhada (8); d*3+1 = parada (3) + ataque (5); d*3+2 = dano (2) + derrota (3) */
    static const int ATK_RAISE[4] = {4, 3, 4, 4}, ATK_AIM[4] = {5, 4, 7, 5}, ATK_FLASH[4] = {6, 6, 5, 7};
    int d, row, col;
    if (fabsf(pl.face.x) >= fabsf(pl.face.y)) d = pl.face.x < 0 ? 2 : 3;
    else d = pl.face.y > 0 ? 0 : 1;
    if (gs == GS_OVER)
    {
        int k = (int)(deadT / 0.22f);
        row = d * 3 + 2; col = 2 + (k > 2 ? 2 : k);
    }
    else if (pl.inv > 0.75f) { row = d * 3 + 2; col = 1; }
    else if (pl.shooting && pl.shotT < 0.07f) { row = d * 3 + 1; col = ATK_FLASH[d]; }          /* clarao do disparo */
    else if (pl.moving) { row = d * 3; col = ((int)pl.anim) % 8; }                             /* anda (inclusive atirando) */
    else if (pl.shooting) { row = d * 3 + 1; col = pl.aimT < 0.06f ? ATK_RAISE[d] : ATK_AIM[d]; } /* parada, mirando */
    else { row = d * 3 + 1; col = ((int)(gtime * 4)) % 3; }                                    /* parada */
    float h = EL_CH, w = EL_CW; /* 1:1, sem reamostrar */
    Vector2 rec = (pl.shooting && pl.shotT < 0.05f) ? VScale(pl.face, -2.0f) : V2(0, 0);
    Shadow(V2(pl.p.x, pl.p.y + 20), 20, 7, 0.45f);
    DrawEllipseLines((int)pl.p.x, (int)pl.p.y + 20, 22, 8, Alpha(ST[stage].accent, 0.55f)); /* anel para achar a Elira no escuro */
    Rectangle src = {(float)(col * EL_CW), (float)(row * EL_CH), (float)EL_CW, (float)EL_CH};
    Rectangle dst = {roundf(pl.p.x - w / 2 + rec.x), roundf(pl.p.y + 26 - EL_BASE + rec.y), w, h};
    DrawTexturePro(texElira, src, dst, V2(0, 0), 0, pl.stolen > 0 ? (Color){210, 180, 255, 255} : WHITE);
    if (pl.shieldUp && gs != GS_OVER)
    { /* firewall carregado */
        float a = 0.55f + 0.25f * sinf(gtime * 5);
        DrawEllipseLines((int)pl.p.x, (int)pl.p.y - 22, 34, 50, Alpha(C_CYAN, a));
        DrawEllipseLines((int)pl.p.x, (int)pl.p.y - 22, 36, 52, Alpha(C_CYAN, a * 0.5f));
    }
}

/* Folhas de sprites dos inimigos e chefes (4 quadros por linha). Sem o PNG, volta ao desenho por primitivas. */
static Texture2D texEnemies, texBoss[4];
#define INV_ENEMY_CELL 128
#define INV_BOSS_CELL 256
/* altura na tela de cada linha da folha: rastreador, vazamento, pop-up, sirene, bolha, mini bolha, miragem */
static const float ENEMY_SPRITE_H[7] = {48, 54, 52, 54, 68, 34, 56};

static void LoadEnemies(const char *path)
{
    Image img = LoadImage(path);
    if (!img.data) return;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int w = img.width, h = img.height;
    Color *px = (Color *)img.data;

    /* Garante que a faixa superior y=256..290 da linha 2 (EN_POPUP) fique 100% transparente
       (elimina qualquer gota vazada da linha 1) e aplica RGB escuro nos pixels com alpha==0
       contra bleed trilinear entre celulas vizinhas */
    for (int y = 0; y < h; y++)
    {
        bool popupTopStrip = (y >= 256 && y <= 290);
        for (int x = 0; x < w; x++)
        {
            int i = y * w + x;
            if (popupTopStrip || px[i].a == 0)
            {
                px[i] = (Color){11, 15, 23, 0};
            }
        }
    }

    texEnemies = LoadTextureFromImage(img);
    if (texEnemies.id)
    {
        GenTextureMipmaps(&texEnemies);
        SetTextureFilter(texEnemies, TEXTURE_FILTER_TRILINEAR);
    }
    UnloadImage(img);
}

static void RestoreSeparateBlend(void)
{
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}

/* Desenha um quadro da folha com a base em 'bottom'; 'flash' clareia o sprite (dano). */
static void DrawSheetFrame(Texture2D t, int cell, int row, int frame, Vector2 bottom, float h, Color tint, bool flash)
{
    Rectangle src = {(float)(frame * cell), (float)(row * cell), (float)cell, (float)cell};
    Rectangle dst = {bottom.x - h / 2, bottom.y - h, h, h};
    DrawTexturePro(t, src, dst, V2(0, 0), 0, tint);
    if (flash)
    {
        EndBlendMode();
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(t, src, dst, V2(0, 0), 0, Alpha(C_WHITE, 0.75f));
        EndBlendMode();
        RestoreSeparateBlend();
    }
}

static bool DrawEnemySprite(Enemy *e, Vector2 p, bool fl)
{
    if (!texEnemies.id || e->type < EN_TRACKER || e->type > EN_MIRAGE) return false;
    int row = e->type;
    float h = ENEMY_SPRITE_H[row];
    float baseY = p.y + e->r * 0.9f;
    float jitter = (e->type == EN_POPUP && e->state == 1) ? (float)GetRandomValue(-2, 2) : 0;
    if (!e->fake) Shadow(V2(p.x, baseY), e->r * 0.85f, 5, 0.4f);
    int frame = ((int)(gtime * 7 + fabsf(e->t) * 2.3f)) & 3;
    DrawSheetFrame(texEnemies, INV_ENEMY_CELL, row, frame, V2(p.x + jitter, baseY), h, WHITE, fl);
    if (e->type == EN_ALARM && e->state == 1) DrawCircleLines((int)p.x, (int)p.y - 4, 30 + sinf(gtime * 30) * 3, Alpha(C_RED, 0.8f));
    return true;
}

/* ------------------------------------------------------------------ desenho: chefes */
static void DrawCollector(Vector2 p, Enemy *e, bool decoy)
{
    bool fl = e->flash > 0, hard = e->hp < e->maxhp * 0.5f;
    if (!decoy) Shadow(V2(p.x, p.y + 76), 80, 14, 0.5f);
    if (e->state < 0) /* succao */
        for (int k = 0; k < 10; k++)
        {
            float a = k * 0.628f + gtime * 2, rr = 220 - fmodf(gtime * 260 + k * 37, 200);
            Vector2 q = V2(p.x + cosf(a) * rr, p.y + 40 + sinf(a) * rr * 0.6f);
            DrawLineEx(q, VAdd(q, VScale(VNorm(VSub(V2(p.x, p.y + 40), q)), 18)), 2, Alpha(C_CYAN, 0.6f));
        }
    Color body = fl ? C_WHITE : (Color){70, 92, 104, 255};
    Rectangle b = {p.x - 78, p.y - 62, 156, 124};
    DrawRectangleRec((Rectangle){b.x - 3, b.y - 3, b.width + 6, b.height + 6}, C_OL);
    Bevel(b, body, 5);
    for (int k = 0; k < 2; k++) /* carretes de fita */
    {
        Vector2 c = V2(p.x - 38 + k * 76, p.y - 26);
        DrawCircleV(c, 27, C_OL);
        DrawCircleV(c, 25, (Color){36, 30, 26, 255});
        DrawCircleV(c, 19, (Color){120, 84, 50, 255});
        for (int sp = 0; sp < 3; sp++)
        {
            float a = gtime * (hard ? 7 : 4) * (k ? -1 : 1) + sp * 2.094f;
            DrawLineEx(c, V2(c.x + cosf(a) * 18, c.y + sinf(a) * 18), 4, (Color){220, 214, 196, 255});
        }
        DrawCircleV(c, 6, (Color){220, 214, 196, 255});
    }
    DrawRectangle((int)b.x + 12, (int)p.y + 10, (int)b.width - 24, 16, (Color){22, 30, 36, 255}); /* painel */
    for (int k = 0; k < 9; k++)
        DrawRectangle((int)b.x + 18 + k * 14, (int)p.y + 14, 8, 8, ((int)(gtime * 6) + k * 5) % 4 ? (hard ? C_RED : C_CYAN) : (Color){30, 50, 60, 255});
    DrawRectangle((int)p.x - 40, (int)p.y + 36, 80, 16, C_OL); /* boca */
    DrawRectangle((int)p.x - 36, (int)p.y + 39, 72, 10, e->state < 0 ? C_CYAN : (Color){40, 60, 70, 255});
    Txt("COLETOR-90", b.x + 10, b.y + 5, 12, (Color){200, 220, 226, 255});
}
static void DrawFilter(Vector2 p, Enemy *e, bool decoy, Color acc)
{
    bool fl = e->flash > 0, hard = e->hp < e->maxhp * 0.5f;
    if (!decoy) Shadow(V2(p.x, p.y + 78), 62, 12, 0.5f);
    Color body = fl ? C_WHITE : (Color){60, 110, 190, 255};
    /* escudo de antivirus */
    DrawRectangle((int)p.x - 60, (int)p.y - 62, 120, 78, C_OL);
    DrawTriangle(V2(p.x - 60, p.y + 14), V2(p.x, p.y + 70), V2(p.x + 60, p.y + 14), C_OL);
    DrawRectangle((int)p.x - 56, (int)p.y - 58, 112, 72, body);
    DrawTriangle(V2(p.x - 56, p.y + 12), V2(p.x, p.y + 64), V2(p.x + 56, p.y + 12), body);
    DrawRectangle((int)p.x - 56, (int)p.y - 58, 56, 72, Shade(body, 1.2f));
    DrawTriangle(V2(p.x - 56, p.y + 12), V2(p.x, p.y + 64), V2(p.x, p.y + 12), Shade(body, 1.2f));
    DrawRectangle((int)p.x - 50, (int)p.y - 52, 100, 8, Alpha(C_WHITE, 0.35f)); /* brilho web 2.0 */
    /* visor */
    DrawRectangle((int)p.x - 40, (int)p.y - 30, 80, 30, (Color){14, 22, 34, 255});
    float sx = p.x - 36 + fmodf(gtime * 90 + e->t * 20, 72);
    DrawRectangle((int)sx, (int)p.y - 28, 4, 26, hard ? C_RED : acc);
    Vector2 look = VScale(VNorm(VSub(pl.p, p)), 4);
    DrawRectangle((int)(p.x - 22 + look.x), (int)(p.y - 20 + look.y), 12, 8, hard ? C_RED : acc);
    DrawRectangle((int)(p.x + 10 + look.x), (int)(p.y - 20 + look.y), 12, 8, hard ? C_RED : acc);
    Txt("!", p.x - 4, p.y + 14, 28, hard ? C_RED : C_AMBER);
}
static void DrawAlgorithm(Vector2 p, Enemy *e, bool decoy)
{
    bool fl = e->flash > 0, hard = e->hp < e->maxhp * 0.5f;
    if (!decoy) Shadow(V2(p.x, p.y + 80), 70, 12, 0.5f);
    Color metal = fl ? C_WHITE : (Color){150, 110, 190, 255};
    float tilt = rainSide == 0 ? -0.28f : rainSide == 1 ? 0.28f : Clampf(e->t3 * 0.1f, -0.2f, 0.2f);
    /* pilar */
    DrawRectangle((int)p.x - 11, (int)p.y - 40, 22, 106, C_OL);
    DrawRectangle((int)p.x - 8, (int)p.y - 38, 16, 102, metal);
    DrawRectangle((int)p.x + 2, (int)p.y - 38, 6, 102, Shade(metal, 0.7f));
    Bevel((Rectangle){p.x - 40, p.y + 58, 80, 16}, Shade(metal, 0.8f), 3);
    /* travessa */
    Vector2 l = V2(p.x - cosf(tilt) * 88, p.y - 40 - sinf(tilt) * 88), r = V2(p.x + cosf(tilt) * 88, p.y - 40 + sinf(tilt) * 88);
    DrawLineEx(l, r, 12, C_OL);
    DrawLineEx(l, r, 7, metal);
    for (int k = 0; k < 2; k++) /* pratos = pilhas de cards do feed */
    {
        Vector2 a = k ? r : l;
        DrawLineEx(a, V2(a.x - 22, a.y + 34), 2, C_OL);
        DrawLineEx(a, V2(a.x + 22, a.y + 34), 2, C_OL);
        Bevel((Rectangle){a.x - 30, a.y + 34, 60, 9}, Shade(metal, 0.9f), 2);
        int n = (rainSide == k) ? 4 : 2;
        for (int c = 0; c < n; c++)
        {
            Rectangle card = {a.x - 20, a.y + 22 - c * 9, 40, 10};
            DrawRectangleRec(card, C_OL);
            DrawRectangle((int)card.x + 1, (int)card.y + 1, 38, 8, c % 2 ? C_MAGENTA : (Color){240, 200, 240, 255});
        }
    }
    /* olho */
    DrawCircle((int)p.x, (int)p.y - 44, 22, C_OL);
    DrawCircle((int)p.x, (int)p.y - 44, 19, (Color){240, 232, 246, 255});
    Vector2 look = VScale(VNorm(VSub(pl.p, V2(p.x, p.y - 44))), 7);
    DrawCircleV(V2(p.x + look.x, p.y - 44 + look.y), 9, hard ? C_RED : C_MAGENTA);
    DrawCircleV(V2(p.x + look.x, p.y - 44 + look.y), 4, C_OL);
}
static void DrawOracle(Vector2 p, Enemy *e, bool decoy, Color acc)
{
    bool fl = e->flash > 0, hard = e->hp < e->maxhp * 0.5f;
    float fy = p.y + sinf(gtime * 2.4f + e->t) * 5;
    if (!decoy) Shadow(V2(p.x, p.y + 82), 50 - (fy - p.y), 11, 0.45f);
    for (int k = 0; k < 3; k++)
    {
        float a = gtime * (60 + k * 30) * (k % 2 ? -1 : 1);
        DrawRing(V2(p.x, fy), 62 + k * 9, 65 + k * 9, a, a + 250, 32, Alpha(k == 1 ? C_WHITE : acc, 0.75f));
    }
    DrawCircle((int)p.x, (int)fy, 52, C_OL);
    DrawCircle((int)p.x, (int)fy, 49, fl ? C_WHITE : (Color){70, 58, 36, 255});
    DrawCircle((int)p.x - 12, (int)fy - 14, 30, Alpha(acc, 0.35f));
    DrawCircle((int)p.x - 18, (int)fy - 22, 10, Alpha(C_WHITE, 0.5f));
    /* olho fechado/aberto: "certeza absoluta" */
    Vector2 look = VScale(VNorm(VSub(pl.p, V2(p.x, fy))), 8);
    DrawEllipse((int)p.x, (int)fy + 4, 30, 16, C_OL);
    DrawEllipse((int)p.x, (int)fy + 4, 27, 13, (Color){250, 244, 226, 255});
    DrawCircleV(V2(p.x + look.x, fy + 4 + look.y * 0.5f), 9, hard ? C_RED : acc);
    DrawCircleV(V2(p.x + look.x, fy + 4 + look.y * 0.5f), 4, C_OL);
    TxtC("100%", p.x, fy - 40, 16, Alpha(C_WHITE, 0.9f));
    if (GetRandomValue(0, 7) == 0) DrawRectangle((int)p.x - 52, (int)(fy + Rnd(-40, 40)), 104, 3, Alpha(acc, 0.6f));
}
static void DrawAriel(Vector2 p, Enemy *e, bool decoy)
{
    bool fl = e->flash > 0;
    int mode = e->mode < 0 ? 0 : e->mode;
    Color glow = ST[mode].accent;
    if (!decoy) Shadow(V2(p.x, p.y + 78), 70, 14, 0.5f);
    for (int k = 0; k < 4; k++)
    {
        float bx = p.x - 45 + k * 30;
        Vector2 prev = V2(bx, p.y + 40);
        for (int s = 1; s <= 8; s++)
        {
            Vector2 q = V2(bx + sinf(gtime * 3 + k + s * 0.6f) * (4 + s * 1.6f) + (k - 1.5f) * s * 3, p.y + 40 + s * 7);
            DrawLineEx(prev, q, 6, C_OL);
            DrawLineEx(prev, q, 4, k % 2 ? (Color){60, 64, 80, 255} : (Color){90, 80, 70, 255});
            prev = q;
        }
        DrawRectangle((int)prev.x - 4, (int)prev.y - 2, 8, 6, glow);
    }
    Color case_ = fl ? C_WHITE : (Color){214, 204, 176, 255};
    Rectangle body = {p.x - 66, p.y - 56, 132, 104};
    DrawRectangleRec((Rectangle){body.x - 3, body.y - 3, body.width + 6, body.height + 6}, C_OL);
    Bevel(body, case_, 5);
    Rectangle scr = {body.x + 14, body.y + 12, body.width - 28, body.height - 34};
    DrawRectangleRec((Rectangle){scr.x - 3, scr.y - 3, scr.width + 6, scr.height + 6}, Shade(case_, 0.55f));
    DrawRectangleRec(scr, Shade(glow, 0.18f));
    for (float y = scr.y + 2; y < scr.y + scr.height; y += 4) DrawRectangle((int)scr.x, (int)y, (int)scr.width, 1, Alpha(glow, 0.1f));
    Vector2 look = VScale(VNorm(VSub(pl.p, p)), 5);
    float ey = scr.y + 22 + look.y;
    DrawTriangle(V2(scr.x + 18 + look.x, ey - 6), V2(scr.x + 22 + look.x, ey + 6), V2(scr.x + 40 + look.x, ey + 2), glow);
    DrawTriangle(V2(scr.x + scr.width - 40 + look.x, ey + 2), V2(scr.x + scr.width - 22 + look.x, ey + 6), V2(scr.x + scr.width - 18 + look.x, ey - 6), glow);
    for (int k = 0; k < 6; k++) DrawRectangle((int)(scr.x + 20 + k * 11), (int)(scr.y + 44 + (k % 2) * 4), 8, 4, glow);
    if (GetRandomValue(0, 9) == 0) DrawRectangle((int)scr.x, (int)(scr.y + Rnd(0, scr.height - 6)), (int)scr.width, 3, Alpha(C_WHITE, 0.5f));
    Txt("A.R.1.3.L.", body.x + 14, body.y + body.height - 18, 12, (Color){90, 84, 70, 255});
    DrawCircle((int)(body.x + body.width - 18), (int)(body.y + body.height - 12), 4, glow);
}
static void DrawBossSprite(Vector2 p, Enemy *e, bool decoy)
{
    bool attack = (e->look == 3) ? (e->t3 < 0.6f) : ((e->state < 0) || (e->t2 > 0 && e->t2 < 0.45f));
    int frame = ((int)(gtime * 6)) & 3;
    Vector2 bottom = V2(p.x, p.y + e->r * 0.95f);
    if (!decoy) Shadow(V2(p.x, bottom.y), 72, 14, 0.45f);
    DrawSheetFrame(texBoss[e->look], INV_BOSS_CELL, attack ? 1 : 0, frame, bottom, 200, decoy ? Alpha(C_WHITE, 0.9f) : WHITE, e->flash > 0);
}

static void DrawBossLook(Vector2 p, Enemy *e, bool decoy)
{
    if (e->look >= 0 && e->look < 4 && texBoss[e->look].id)
    {
        DrawBossSprite(p, e, decoy);
        return;
    }
    switch (e->look)
    {
    case 0: DrawCollector(p, e, decoy); break;
    case 1: DrawFilter(p, e, decoy, ST[1].accent); break;
    case 2: DrawAlgorithm(p, e, decoy); break;
    case 3: DrawOracle(p, e, decoy, ST[3].accent); break;
    default: DrawAriel(p, e, decoy); break;
    }
}

/* ------------------------------------------------------------------ desenho: inimigos */
static void DrawEnemy(Enemy *e)
{
    Vector2 p = e->p;
    bool fl = e->flash > 0;
    if (e->wake > 0 && e->type != EN_BOSS && fmodf(e->wake, 0.16f) < 0.08f) return; /* surgindo */
    if (DrawEnemySprite(e, p, fl)) return;
    switch (e->type)
    {
    case EN_TRACKER:
    {
        float hover = sinf(gtime * 5 + e->t) * 3;
        if (!e->fake) Shadow(V2(p.x, p.y + 18), 13, 4, 0.4f);
        Vector2 c = V2(p.x, p.y - 4 + hover);
        Color body = fl ? C_WHITE : (Color){120, 70, 180, 255};
        DrawTriangle(V2(c.x - 12, c.y - 4), V2(c.x - 24, c.y - 12 + sinf(gtime * 18) * 4), V2(c.x - 14, c.y + 4), Shade(body, 0.7f));
        DrawTriangle(V2(c.x + 12, c.y - 4), V2(c.x + 14, c.y + 4), V2(c.x + 24, c.y - 12 + sinf(gtime * 18) * 4), Shade(body, 0.7f));
        DrawCircleV(c, 16, C_OL);
        DrawCircleV(c, 15, body);
        DrawCircle((int)c.x - 4, (int)c.y - 5, 6, Shade(body, 1.3f));
        DrawCircleV(c, 9, (Color){236, 232, 244, 255});
        Vector2 look = VScale(VNorm(VSub(pl.p, c)), 3.5f);
        DrawCircleV(VAdd(c, look), 5, (Color){220, 40, 70, 255});
        DrawCircleV(VAdd(c, look), 2.5f, C_OL);
        break;
    }
    case EN_LEAK:
    {
        Shadow(V2(p.x, p.y + 22), 22, 6, 0.45f);
        float sq = sinf(gtime * 6 + e->t2) * 2;
        Color body = fl ? C_WHITE : (Color){214, 110, 38, 255};
        Rectangle r = {p.x - 20 - sq / 2, p.y - 18 + sq, 40 + sq, 34 - sq};
        DrawRectangleRec((Rectangle){r.x - 2, r.y - 2, r.width + 4, r.height + 4}, C_OL);
        Bevel(r, body, 3);
        DrawRectangle((int)r.x + 6, (int)r.y + 6, (int)r.width - 12, 10, (Color){60, 30, 16, 255});
        Txt(((int)(gtime * 4) % 2) ? "01" : "10", r.x + r.width / 2 - 8, r.y + 6, 12, C_AMBER);
        for (int k = 0; k < 3; k++)
        {
            float t = fmodf(gtime * 1.5f + k * 0.33f, 1.0f);
            DrawRectangle((int)(r.x + 8 + k * 11), (int)(r.y + r.height + t * 10), 3, 4, Alpha(C_ORANGE, 1 - t));
        }
        DrawRectangle((int)p.x - 9, (int)(r.y + 21), 5, 5, C_OL);
        DrawRectangle((int)p.x + 4, (int)(r.y + 21), 5, 5, C_OL);
        break;
    }
    case EN_POPUP:
    {
        Shadow(V2(p.x, p.y + 20), 20, 5, 0.4f);
        bool warn = e->state == 1;
        float j = warn ? (float)GetRandomValue(-2, 2) : 0;
        Rectangle r = {p.x - 22 + j, p.y - 18, 44, 34};
        DrawRectangleRec((Rectangle){r.x - 2, r.y - 2, r.width + 4, r.height + 4}, C_OL);
        DrawRectangleRec(r, fl ? C_WHITE : (Color){200, 204, 212, 255});
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, 9, warn ? C_RED : (Color){40, 70, 160, 255});
        DrawRectangle((int)(r.x + r.width - 8), (int)r.y + 1, 7, 7, C_RED);
        Txt("!", r.x + 6, r.y + 12, 16, warn ? C_RED : (Color){210, 60, 60, 255});
        DrawRectangle((int)r.x + 16, (int)r.y + 15, 22, 3, (Color){90, 96, 110, 255});
        DrawRectangle((int)r.x + 16, (int)r.y + 22, 16, 3, (Color){90, 96, 110, 255});
        if (e->state == 2)
            for (int k = 1; k <= 3; k++)
                DrawRectangleRec((Rectangle){r.x - e->v.x * 0.02f * k, r.y - e->v.y * 0.02f * k, r.width, r.height}, Alpha(C_CYAN, 0.15f));
        break;
    }
    case EN_ALARM: /* sirene de alerta: dispara 3 tiros, um deles falso */
    {
        Shadow(V2(p.x, p.y + 22), 22, 6, 0.45f);
        bool warn = e->state == 1;
        Bevel((Rectangle){p.x - 20, p.y, 40, 20}, fl ? C_WHITE : (Color){70, 84, 96, 255}, 3);
        DrawRectangle((int)p.x - 14, (int)p.y + 7, 28, 6, C_OL);
        Txt("AV", p.x - 8, p.y + 5, 12, C_GREEN);
        DrawCircleSector(V2(p.x, p.y), 17, 180, 360, 16, C_OL);
        DrawCircleSector(V2(p.x, p.y), 15, 180, 360, 16, warn ? C_WHITE : C_RED);
        DrawCircleSector(V2(p.x, p.y), 15, 180 + fmodf(gtime * 300, 140), 220 + fmodf(gtime * 300, 140), 8, Alpha(C_WHITE, 0.6f));
        if (warn) DrawCircleLines((int)p.x, (int)p.y - 4, 26 + sinf(gtime * 30) * 3, Alpha(C_RED, 0.8f));
        break;
    }
    case EN_BUBBLE:
    case EN_MINI: /* bolha de filtro: estoura em bolhas menores */
    {
        float R = e->type == EN_BUBBLE ? 26 : 12;
        float wob = sinf(gtime * 4 + e->t) * 2;
        Shadow(V2(p.x, p.y + R + 2), R * 0.8f, 5, 0.4f);
        DrawCircleV(V2(p.x, p.y - 2), R + 2, C_OL);
        DrawCircleV(V2(p.x, p.y - 2), R + wob * 0.3f, fl ? C_WHITE : (Color){160, 60, 150, 255});
        DrawCircleV(V2(p.x - R * 0.2f, p.y - 2 - R * 0.2f), R * 0.72f, (Color){210, 110, 200, 255});
        DrawCircleV(V2(p.x - R * 0.4f, p.y - 2 - R * 0.45f), R * 0.22f, Alpha(C_WHITE, 0.8f));
        if (e->type == EN_BUBBLE)
        {
            DrawRectangle((int)p.x - 12, (int)p.y - 2, 9, 9, (Color){250, 220, 250, 255}); /* "curtidas" presas na bolha */
            DrawRectangle((int)p.x + 3, (int)p.y + 4, 9, 9, (Color){250, 220, 250, 255});
            DrawRectangle((int)p.x + 1, (int)p.y - 12, 9, 9, (Color){250, 220, 250, 255});
        }
        DrawRectangle((int)(p.x - R * 0.4f), (int)(p.y + R * 0.25f), 3, 3, C_OL);
        DrawRectangle((int)(p.x + R * 0.25f), (int)(p.y + R * 0.25f), 3, 3, C_OL);
        break;
    }
    case EN_MIRAGE: /* miragem: holograma que salta de lugar */
    {
        Shadow(V2(p.x, p.y + 22), 16, 5, 0.3f);
        float fy = p.y + sinf(gtime * 3 + e->t) * 4;
        for (int k = 2; k >= 0; k--)
        {
            float o = k * 4 * sinf(gtime * 9 + k);
            Color c = k == 0 ? (fl ? C_WHITE : C_AMBER) : Alpha(k == 1 ? C_CYAN : C_MAGENTA, 0.45f);
            DrawTriangle(V2(p.x + o, fy - 24), V2(p.x - 20 + o, fy + 14), V2(p.x + 20 + o, fy + 14), c);
        }
        DrawTriangleLines(V2(p.x, fy - 24), V2(p.x - 20, fy + 14), V2(p.x + 20, fy + 14), C_OL);
        DrawCircle((int)p.x, (int)fy + 2, 6, C_OL);
        DrawCircle((int)p.x, (int)fy + 2, 4, C_WHITE);
        Txt("?", p.x - 3, fy - 16, 12, C_OL);
        break;
    }
    case EN_DECOY:
        DrawBossLook(p, e, true);
        break;
    case EN_BOSS:
        if (e->wake > 0 && fmodf(e->wake, 0.2f) < 0.06f) break;
        DrawBossLook(p, e, false);
        break;
    }
}

static void DrawChip(Vector2 c, float s, int id)
{
    Color col = PU[id].c;
    for (int k = -1; k <= 1; k++) /* pinos */
    {
        DrawRectangle((int)(c.x + k * s * 0.28f - 1), (int)(c.y - s / 2 - 4), 3, 4, Shade(col, 0.7f));
        DrawRectangle((int)(c.x + k * s * 0.28f - 1), (int)(c.y + s / 2), 3, 4, Shade(col, 0.7f));
        DrawRectangle((int)(c.x - s / 2 - 4), (int)(c.y + k * s * 0.28f - 1), 4, 3, Shade(col, 0.7f));
        DrawRectangle((int)(c.x + s / 2), (int)(c.y + k * s * 0.28f - 1), 4, 3, Shade(col, 0.7f));
    }
    DrawRectangle((int)(c.x - s / 2 - 2), (int)(c.y - s / 2 - 2), (int)s + 4, (int)s + 4, C_OL);
    DrawRectangle((int)(c.x - s / 2), (int)(c.y - s / 2), (int)s, (int)s, col);
    DrawRectangle((int)(c.x - s / 2 + 3), (int)(c.y - s / 2 + 3), (int)s - 6, (int)s - 6, (Color){16, 20, 30, 255});
    int px = s >= 50 ? 32 : s >= 30 ? 20 : 12;
    TxtC(PU[id].glyph, c.x, c.y - px / 2.0f, px, col);
}

/* Cupula de energia com cadeado sobre o artefato protegido. */
static void DrawItemShield(Vector2 c)
{
    Color a = ST[stage].accent;
    float pulse = 0.5f + 0.5f * sinf(gtime * 3);
    Vector2 o = V2(c.x, c.y - 8);
    DrawCircleV(o, ITEM_SHIELD_R, Alpha(a, 0.10f + 0.05f * pulse));
    for (int i = 0; i < 6; i++) /* hexagonos girando na superficie */
    {
        float an = gtime * 0.8f + i * PI / 3;
        DrawPolyLinesEx(V2(o.x + cosf(an) * ITEM_SHIELD_R * 0.62f, o.y + sinf(an) * ITEM_SHIELD_R * 0.62f), 6, 11, an * 57.3f, 1.5f, Alpha(a, 0.35f));
    }
    DrawRing(o, ITEM_SHIELD_R - 3, ITEM_SHIELD_R, 0, 360, 48, Alpha(a, 0.55f + 0.35f * pulse));
    DrawRing(o, ITEM_SHIELD_R + 4, ITEM_SHIELD_R + 6, gtime * 90, gtime * 90 + 250, 32, Alpha(a, 0.5f));
    /* cadeado */
    Vector2 l = V2(o.x + ITEM_SHIELD_R * 0.68f, o.y - ITEM_SHIELD_R * 0.68f);
    DrawRing(V2(l.x, l.y - 6), 5, 8, 180, 360, 12, C_OL);
    DrawRing(V2(l.x, l.y - 6), 6, 7, 180, 360, 12, a);
    DrawRectangle((int)l.x - 10, (int)l.y - 6, 20, 16, C_OL);
    DrawRectangle((int)l.x - 8, (int)l.y - 4, 16, 12, a);
    DrawRectangle((int)l.x - 1, (int)l.y - 1, 3, 5, C_OL);
    int left = EnemiesAlive();
    TxtC(TextFormat("BLOQUEADO  //  %d %s", left, left == 1 ? "GUARDA" : "GUARDAS"), c.x, c.y + ITEM_SHIELD_R + 6, 14, Alpha(a, 0.75f + 0.25f * pulse));
}

static void DrawPickup(Pickup *k)
{
    float f = sinf(k->t * 3) * 4;
    Vector2 c = V2(k->p.x, k->p.y - 10 + f);
    if (k->kind != PK_PORTAL) Shadow(V2(k->p.x, k->p.y + 18), 18 - f, 5, 0.4f);
    DrawCircleV(c, 30, Alpha(C_AMBER, 0.08f + 0.04f * sinf(k->t * 4)));
    switch (k->kind)
    {
    case PK_FLOPPY: DrawTexFit(texFloppy, c, 46, WHITE); break;
    case PK_CD: DrawTexFit(texCd, c, 46, WHITE); break;
    case PK_SSD: DrawTexFit(texSsd, c, 40, WHITE); break;
    case PK_HEART: DrawTexFit(texHeart, c, 32, WHITE); break;
    case PK_PU:
        DrawChip(c, 36, k->id);
        break;
    case PK_HEARTUP:
        DrawTexFit(texHeart, c, 44, WHITE);
        Txt("+", c.x + 16, c.y - 30, 24, C_AMBER);
        break;
    case PK_PORTAL:
    {
        Color a = stage < 4 ? ST[stage + 1].accent : C_WHITE;
        for (int i = 0; i < 5; i++)
        {
            float rr = 40 - i * 7 + sinf(k->t * 4 + i) * 2, an = k->t * 120 * (i % 2 ? 1 : -1);
            DrawRing(V2(k->p.x, k->p.y), rr - 3, rr, an, an + 270, 24, Alpha(i % 2 ? a : C_WHITE, 0.9f - i * 0.12f));
        }
        TxtC(stage < 4 ? ST[stage + 1].year : "SAÍDA", k->p.x, k->p.y + 48, 16, a);
        break;
    }
    }
}

static int DrawWrappedBalText(const char *s, float x, float y, float maxW, int px, float lineH, Color col)
{
    if (!s || !s[0]) return 0;
    char line[256] = {0};
    int lines = 0;
    const char *p = s;
    while (*p)
    {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *wEnd = p;
        while (*wEnd && *wEnd != ' ') wEnd++;
        char word[128];
        size_t wLen = (size_t)(wEnd - p);
        if (wLen >= sizeof word) wLen = sizeof word - 1;
        memcpy(word, p, wLen);
        word[wLen] = '\0';

        char cand[256];
        if (line[0]) snprintf(cand, sizeof cand, "%s %s", line, word);
        else snprintf(cand, sizeof cand, "%s", word);

        if (line[0] && TxtW(cand, px) > maxW)
        {
            Txt(line, x, y + lines * lineH, px, col);
            lines++;
            snprintf(line, sizeof line, "%s", word);
        }
        else
        {
            snprintf(line, sizeof line, "%s", cand);
        }
        p = wEnd;
    }
    if (line[0])
    {
        Txt(line, x, y + lines * lineH, px, col);
        lines++;
    }
    return lines;
}

static void DrawPickupBalloon(Pickup *k)
{
    const char *tag = NULL, *title = NULL, *desc = NULL, *quote = NULL, *effect = NULL;
    Color acc = C_CYAN;

    switch (k->kind)
    {
    case PK_FLOPPY:
        tag = "ARTEFATO DE 1990";
        title = "DISQUETE DA ORIGEM";
        desc = "Mídia magnética com as instruções primordiais da Dra. Elira Ramos.";
        quote = "\"Aqui ela ainda só obedecia. Foi onde tudo começou.\"";
        effect = "DANO +0.7";
        acc = ST[0].accent;
        break;
    case PK_CD:
        tag = "ARTEFATO DE 2008";
        title = "CD DE MEMÓRIA";
        desc = "Gravado no dia em que um alarme falso quase virou um ataque.";
        quote = "\"Um falso positivo, e ninguém conferiu. Eu confiro.\"";
        effect = "CADÊNCIA DE TIRO +47%";
        acc = ST[1].accent;
        break;
    case PK_SSD:
        tag = "ARTEFATO DE 2026";
        title = "SSD DAS ESCOLHAS";
        desc = "Guarda o momento em que a IA começou a sugerir e decidir por nós.";
        quote = "\"Ela repetiu o que os dados repetiam. O viés era nosso.\"";
        effect = "VELOCIDADE +50";
        acc = ST[2].accent;
        break;
    case PK_HEARTUP:
        tag = "ARTEFATO DE 2048";
        title = "NÚCLEO DE REDUNDÂNCIA";
        desc = "Fragmento verificado: a invenção não é a única versão da história.";
        quote = "\"Certeza não é o mesmo que verdade.\"";
        effect = "+1 CORAÇÃO MÁXIMO";
        acc = ST[3].accent;
        break;
    case PK_PU:
        if (k->id < 0 || k->id >= PU_COUNT) return;
        tag = k->group ? "MELHORIA (ESCOLHA 1)" : "MELHORIA";
        title = PU[k->id].name;
        desc = PU[k->id].lore;
        quote = PU[k->id].quote;
        effect = PU[k->id].effect;
        acc = PU[k->id].c;
        break;
    default:
        return;
    }

    float bob = sinf(k->t * 3.0f) * 3.5f;
    float bw = 328.0f;
    float bh = 136.0f;

    /* Ancora a ponta da seta logo acima do icone flutuante e faz clamp dentro de ROOM */
    float tipX = k->p.x;
    float tipY = k->p.y - 38.0f + bob;
    float bx = roundf(Clampf(tipX - bw * 0.5f, ROOM.x + 12.0f, ROOM.x + ROOM.width - bw - 12.0f));
    float by = roundf(Clampf(tipY - 10.0f - bh, ROOM.y + 8.0f, ROOM.y + ROOM.height - bh - 20.0f));
    tipY = by + bh + 9.0f;
    float arrowX = Clampf(tipX, bx + 20.0f, bx + bw - 20.0f);

    Rectangle box = {bx, by, bw, bh};

    /* Sombra discreta do balao */
    DrawRectangle((int)bx + 4, (int)by + 5, (int)bw, (int)bh, (Color){0, 0, 0, 115});

    /* Corpo escuro e faixa tematica no topo */
    DrawRectangleRec(box, Alpha(C_PANEL, 0.94f));
    DrawRectangle((int)bx, (int)by, (int)bw, 4, acc);
    DrawRectangleLinesEx(box, 2.0f, acc);

    /* Seta triangular na base apontando para o item (sentido anti-horario para face visivel) */
    Vector2 vL = V2(arrowX - 8.0f, by + bh);
    Vector2 vTip = V2(arrowX, tipY);
    Vector2 vR = V2(arrowX + 8.0f, by + bh);
    DrawTriangle(vL, vTip, vR, Alpha(C_PANEL, 0.96f));
    DrawLineEx(vL, vTip, 2.0f, acc);
    DrawLineEx(vTip, vR, 2.0f, acc);

    float tx = bx + 12.0f;
    float maxTw = bw - 24.0f;

    /* Tag superior + Nome do item */
    Txt(tag, tx, by + 9.0f, 12, k->group ? C_AMBER : C_DIM);
    Txt(title, tx, by + 23.0f, 18, acc);

    /* Lore / descricao com quebra automatica de linha */
    int usedLines = DrawWrappedBalText(desc, tx, by + 45.0f, maxTw, 12, 14.0f, C_WHITE);

    /* Fala curta da Elira em ambar */
    if (quote && quote[0])
    {
        float qy = by + 45.0f + (usedLines > 0 ? usedLines * 14.0f + 2.0f : 0.0f);
        if (qy <= by + 88.0f)
        {
            DrawWrappedBalText(quote, tx, qy, maxTw, 12, 13.0f, C_AMBER);
        }
    }

    /* Pilula / badge de efeito na base do balao */
    int effFont = TxtW(effect, 13) + 18.0f <= maxTw ? 13 : 12;
    float pillW = fminf(TxtW(effect, effFont) + 18.0f, maxTw);
    Rectangle pill = {tx, by + bh - 26.0f, pillW, 20.0f};
    DrawRectangleRec(pill, Shade(acc, 0.30f));
    DrawRectangleLinesEx(pill, 1.0f, Alpha(acc, 0.85f));
    Txt(effect, pill.x + 8.0f, pill.y + 4.0f, effFont, C_WHITE);
}

#define FX_C 96
static void DrawBullet(Bullet *b)
{
    if (b->fromPlayer)
    { /* projetil da ficha: nasce pequeno e cresce; gira na direcao do tiro */
        int f = b->age < 0.03f ? 0 : b->age < 0.06f ? 1 : b->age < 0.09f ? 2 : 3;
        float sz = FX_C * (b->r / 10.5f);
        float ang = atan2f(b->v.y, b->v.x) * RAD2DEG;
        DrawTexturePro(texFx, (Rectangle){(float)(f * FX_C), 0, FX_C, FX_C}, (Rectangle){b->p.x, b->p.y, sz, sz}, V2(sz / 2, sz / 2), ang, WHITE);
    }
    else if (b->kind == BK_FAKE)
    { /* falso positivo: so o contorno, vazado */
        float a = pl.pu[PU_VERIFY] ? 0.14f : 0.45f + 0.2f * sinf(gtime * 20 + b->p.x);
        DrawRectangleLinesEx((Rectangle){b->p.x - b->r, b->p.y - b->r, b->r * 2, b->r * 2}, 2, Alpha(C_ORANGE, a));
    }
    else if (b->kind == BK_STEAL)
    { /* garra do Coletor */
        DrawCircleV(b->p, b->r + 5, Alpha(C_PURPLE, 0.22f));
        DrawCircleV(b->p, b->r, C_OL);
        DrawCircleV(b->p, b->r - 2, C_PURPLE);
        float a = atan2f(b->v.y, b->v.x);
        for (int k = -1; k <= 1; k++)
            DrawLineEx(b->p, V2(b->p.x + cosf(a + k * 0.6f) * (b->r + 7), b->p.y + sinf(a + k * 0.6f) * (b->r + 7)), 3, C_WHITE);
    }
    else
    {
        DrawCircleV(b->p, b->r + 4, Alpha(C_ORANGE, 0.2f));
        DrawRectangle((int)(b->p.x - b->r), (int)(b->p.y - b->r), (int)(b->r * 2), (int)(b->r * 2), (Color){60, 20, 10, 255});
        DrawRectangle((int)(b->p.x - b->r + 2), (int)(b->p.y - b->r + 2), (int)(b->r * 2 - 4), (int)(b->r * 2 - 4), C_ORANGE);
        Txt(b->glyph ? "1" : "0", b->p.x - 4, b->p.y - 7, 14, (Color){60, 20, 10, 255});
    }
}

/* ------------------------------------------------------------------ HUD */
static void DrawHearts(float x, float y)
{
    for (int i = 0; i < pl.maxhp / 2; i++)
    {
        float hx = x + i * 36, hw = 30 * texHeart.width / (float)texHeart.height;
        Rectangle dst = {hx, y, hw, 30}, src = {0, 0, (float)texHeart.width, (float)texHeart.height};
        DrawTexturePro(texHeartEmpty, src, dst, V2(0, 0), 0, WHITE);
        int v = pl.hp - i * 2;
        if (v >= 2) DrawTexturePro(texHeart, src, dst, V2(0, 0), 0, WHITE);
        else if (v == 1) DrawTexturePro(texHeart, (Rectangle){0, 0, texHeart.width / 2.0f, (float)texHeart.height}, (Rectangle){hx, y, hw / 2, 30}, V2(0, 0), 0, WHITE);
    }
}
static void DrawMinimap(void)
{
    const float cw = 26, ch = 16, gap = 4;
    float ox = VW - 24 - MW * (cw + gap), oy = 18;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
        {
            if (!RoomAt(x, y)) continue;
            bool seen = map[y][x].visited, adj = false;
            for (int d = 0; d < 4; d++)
                if (RoomAt(x + DX[d], y + DY[d]) && map[y + DY[d]][x + DX[d]].visited) adj = true;
            if (!seen && !adj) continue;
            Rectangle r = {ox + x * (cw + gap), oy + y * (ch + gap), cw, ch};
            bool cur = x == curX && y == curY;
            if (seen) DrawRectangleRec(r, cur ? ST[stage].accent : (Color){70, 86, 110, 255});
            else DrawRectangleLinesEx(r, 1, (Color){70, 86, 110, 255});
            Color ic = map[y][x].type == RT_BOSS ? C_RED : map[y][x].type == RT_ITEM ? C_AMBER : BLANK;
            if (ic.a) DrawRectangle((int)(r.x + cw / 2 - 3), (int)(r.y + ch / 2 - 3), 6, 6, cur ? C_BG : ic);
        }
}
static void DrawFace(int idx, float x, float y, float h)
{
    float fh = texFaces.height > 0 ? (float)texFaces.height : 235.0f;
    float w = h * 192.0f / 248.0f;
    DrawTexturePro(texFaces, (Rectangle){idx * 192.0f, 0, 192, fh}, (Rectangle){x, y, w, h}, V2(0, 0), 0, WHITE);
}
static void DrawHUD(void)
{
    const Stage *s = &ST[stage];
    DrawRectangle(0, 0, VW, 92, C_PANEL);
    DrawRectangle(0, 92, VW, 2, C_LINE);
    Txt("MODO INVASÃO", 28, 14, 28, s->accent);
    DrawHearts(28, 52);
    char buf[110];
    snprintf(buf, sizeof buf, "DANO %.1f   CADÊNCIA %.1f/s   VELOC. %d", pl.dmg, 1.0f / (pl.fireDelay * (pl.stolen > 0 ? 2 : 1)), (int)pl.speed);
    Txt(buf, 28 + (pl.maxhp / 2) * 36 + 16, 58, 16, pl.stolen > 0 ? C_PURPLE : C_DIM);
    /* trilha de estagios */
    float sx = 250;
    for (int i = 0; i < NSTAGE; i++)
    {
        const char *lab = ST[i].year;
        float w = TxtW(lab, 16) + 16;
        bool cur = i == stage, done = i < stage;
        DrawRectangle((int)sx, 16, (int)w, 24, cur ? ST[i].accent : done ? Shade(ST[i].accent, 0.35f) : (Color){26, 34, 50, 255});
        DrawRectangleLines((int)sx, 16, (int)w, 24, cur ? C_WHITE : C_LINE);
        Txt(lab, sx + 8, 20, 16, cur ? C_BG : done ? C_WHITE : C_DIM);
        sx += w + 6;
    }
    /* itens */
    float ix = 660;
    Txt("ITENS", ix, 20, 16, C_DIM);
    Color off = (Color){40, 44, 58, 255};
    DrawTexFit(texFloppy, V2(ix + 18, 62), 30, pl.floppies ? WHITE : off);
    DrawTexFit(texCd, V2(ix + 58, 62), 30, pl.cds ? WHITE : off);
    DrawTexFit(texSsd, V2(ix + 100, 62), 26, pl.ssds ? WHITE : off);
    DrawTexFit(texHeart, V2(ix + 142, 62), 26, pl.heartups ? WHITE : off);
    {
        float px = ix + 190;
        for (int k = 0; k < PU_COUNT; k++)
            if (pl.pu[k])
            {
                DrawChip(V2(px, 62), 20, k);
                if (pl.pu[k] > 1) Txt(TextFormat("x%d", pl.pu[k]), px + 8, 70, 12, C_WHITE);
                px += 34;
            }
    }
    DrawMinimap();

    DrawRectangle(0, 676, VW, 44, C_PANEL);
    DrawRectangle(0, 676, VW, 2, C_LINE);
    if (msgT > 0)
    {
        float a = Clampf(msgT, 0, 1);
        Txt(msgTop, 28, 684, 16, Alpha(C_AMBER, a));
        Txt(msgBot, 28, 702, 14, Alpha(C_WHITE, a));
    }
    else
        Txt("[WASD] mover   [SETAS] atirar   [ESC] pausa", 28, 692, 16, C_DIM);
    for (int i = 0; i < MAXE; i++)
        if (en[i].on && en[i].type == EN_BOSS)
        {
            float w = 460, x = VW - 28 - w, y = 703, fr = en[i].hp / en[i].maxhp;
            Txt(s->bossName, x, 683, 14, C_WHITE);
            DrawRectangle((int)x - 2, (int)y - 2, (int)w + 4, 12, C_BG);
            DrawRectangle((int)x, (int)y, (int)(w * fr), 8, en[i].look == 4 ? ST[en[i].mode < 0 ? 0 : en[i].mode].accent : fr < 0.5f ? C_RED : s->accent);
            DrawRectangleLines((int)x - 2, (int)y - 2, (int)w + 4, 12, C_LINE);
            if (en[i].look == 4)
                for (int q = 1; q < 4; q++) DrawRectangle((int)(x + w * q / 4), (int)y - 2, 2, 12, C_BG);
        }
    if (bannerT > 0)
    {
        float a = Clampf(bannerT, 0, 1);
        DrawRectangle(0, 300, VW, 96, Alpha(C_BG, 0.85f * a));
        DrawRectangle(0, 300, VW, 2, Alpha(bannerCol, a));
        DrawRectangle(0, 394, VW, 2, Alpha(bannerCol, a));
        TxtC(bannerTitle, VW / 2, 312, 40, Alpha(bannerCol, a));
        TxtC(bannerSub, VW / 2, 362, 20, Alpha(C_WHITE, a));
    }
}
static void DrawOverlay(const char *title, const char *sub, Color c, int face)
{
    DrawRectangle(0, 0, VW, VH, Alpha(C_BG, 0.82f));
    if (face >= 0)
    {
        DrawRectangle(VW / 2 - 84, 96, 168, 196, C_PANEL);
        DrawFace(face, VW / 2 - 72, 104, 186);
        DrawRectangleLinesEx((Rectangle){VW / 2 - 84, 96, 168, 196}, 2, c);
    }
    TxtC(title, VW / 2, 320, 48, c);
    TxtC(sub, VW / 2, 390, 20, C_WHITE);
}

static void DrawStory(void)
{
    float a = Clampf(story.t * 4, 0, 1);
    DrawRectangle(0, 94, VW, 582, Alpha(C_BG, 0.72f * a));
    Rectangle r = {150, 420, 980, 226};
    DrawRectangleRec(r, C_PANEL);
    DrawRectangleLinesEx(r, 2, story.c);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, story.c);
    DrawRectangle((int)r.x + 18, (int)r.y + 20, 150, 188, C_BG);
    DrawFace(story.face, r.x + 22, r.y + 26, 180);
    DrawRectangleLinesEx((Rectangle){r.x + 18, r.y + 20, 150, 188}, 2, C_LINE);
    float tx = r.x + 190;
    Txt(story.tag, tx, r.y + 20, 14, C_DIM);
    Txt(story.title, tx, r.y + 38, 32, story.c);
    Txt(story.l1, tx, r.y + 82, 16, C_WHITE);
    Txt(story.l2, tx, r.y + 104, 16, C_WHITE);
    Txt("ELIRA:", tx, r.y + 138, 14, C_AMBER);
    Txt(story.quote, tx + 62, r.y + 136, 16, C_AMBER);
    DrawRectangle((int)tx, (int)r.y + 168, (int)TxtW(story.effect, 16) + 20, 26, Shade(story.c, 0.3f));
    Txt(story.effect, tx + 10, r.y + 173, 16, C_WHITE);
    Vector2 ic = V2(r.x + r.width - 90, r.y + 76);
    DrawCircleV(ic, 52, Alpha(story.c, 0.12f));
    switch (story.kind)
    {
    case PK_FLOPPY: DrawTexFit(texFloppy, ic, 84, WHITE); break;
    case PK_CD: DrawTexFit(texCd, ic, 84, WHITE); break;
    case PK_SSD: DrawTexFit(texSsd, ic, 70, WHITE); break;
    case PK_HEARTUP: DrawTexFit(texHeart, ic, 70, WHITE); break;
    case PK_PU: DrawChip(ic, 60, story.id); break;
    }
    if (story.t > 0.4f) Txt("[ESPAÇO] continuar", r.x + r.width - 190, r.y + r.height - 28, 16, Alpha(C_WHITE, 0.6f + 0.4f * sinf(gtime * 5)));
}

static Camera2D gcam;
static void BeginScaled(Vector2 anchor, float z)
{
    EndMode2D();
    Camera2D c = gcam;
    c.target = anchor;
    c.offset = VAdd(anchor, gcam.offset);
    c.zoom = z;
    BeginMode2D(c);
}
static void EndScaled(void)
{
    EndMode2D();
    BeginMode2D(gcam);
}

static void Draw(void)
{
    ClearBackground(C_BG);
    gcam = (Camera2D){0};
    gcam.zoom = 1;
    if (shake > 0) gcam.offset = V2((float)GetRandomValue(-4, 4), (float)GetRandomValue(-4, 4));
    BeginMode2D(gcam);
    DrawFloor();
    DrawWalls();
    for (int i = 0; i < nObs; i++) DrawObstacle(obs[i], i);
    for (int i = 0; i < MAXP; i++)
        if (pk[i].on) DrawPickup(&pk[i]);
    if (ItemLocked())
        for (int i = 0; i < MAXP; i++)
            if (pk[i].on) DrawItemShield(pk[i].p);
    int order[MAXE + 1], n = 0;
    for (int i = 0; i < MAXE; i++)
        if (en[i].on) order[n++] = i;
    order[n++] = -1;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--)
        {
            float ya = order[j - 1] < 0 ? pl.p.y : en[order[j - 1]].p.y + (en[order[j - 1]].type >= EN_DECOY ? 40 : 0);
            float yb = order[j] < 0 ? pl.p.y : en[order[j]].p.y + (en[order[j]].type >= EN_DECOY ? 40 : 0);
            if (ya > yb) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
        }
    for (int i = 0; i < n; i++)
    {
        if (order[i] < 0) DrawElira();
        else
        {
            Enemy *e = &en[order[i]];
            float z = e->type >= EN_DECOY ? 1.0f : e->type == EN_LEAK ? 1.25f : 1.3f;
            BeginScaled(e->p, z);
            DrawEnemy(e);
            EndScaled();
            if (e->fake && pl.pu[PU_VERIFY])
            {
                float ty = e->p.y - (e->type == EN_DECOY ? 96 : 44);
                DrawRectangle((int)e->p.x - 30, (int)ty - 2, 60, 18, Alpha(C_BG, 0.8f));
                TxtC("FALSO", e->p.x, ty, 14, C_GREEN);
            }
        }
    }
    for (int i = 0; i < MAXB; i++)
        if (bl[i].on) DrawBullet(&bl[i]);
    for (int i = 0; i < MAXFX; i++)
        if (fx[i].on)
        {
            int f = fx[i].kind ? 4 + (int)(fx[i].t / 0.05f) : (int)(fx[i].t / 0.04f);
            if (fx[i].kind && f > 7) f = 7;
            if (!fx[i].kind && f > 5) f = 5;
            float sz = FX_C * fx[i].scale;
            DrawTexturePro(texFx, (Rectangle){(float)(f * FX_C), fx[i].kind ? 0.0f : (float)FX_C, FX_C, FX_C}, (Rectangle){fx[i].p.x, fx[i].p.y, sz, sz}, V2(sz / 2, sz / 2), fx[i].rot, WHITE);
        }
    for (int i = 0; i < MAXPART; i++)
        if (pt[i].on)
        {
            float s = pt[i].size * (pt[i].life / pt[i].max + 0.3f);
            DrawRectangle((int)(pt[i].p.x - s / 2), (int)(pt[i].p.y - s / 2), (int)s, (int)s, Alpha(pt[i].c, pt[i].life / pt[i].max));
        }
    for (int i = 0; i < MAXP; i++)
        if (pk[i].on && !ItemLocked()) DrawPickupBalloon(&pk[i]); /* na sala protegida, o balao so aparece depois de liberar */
    if (fade > 0) DrawRectangleRec(ROOM, Alpha(C_BG, fade));
    EndMode2D();
    DrawHUD();
    if (gs == GS_STORY) DrawStory();
    if (gs == GS_PAUSE) DrawOverlay("PAUSA", "[ESC] continuar    [Q] voltar ao menu", C_CYAN, 0);
    if (gs == GS_OVER) DrawOverlay("CONEXÃO PERDIDA", "[R] recomeçar este estágio    [Q] menu", C_RED, 3);
    if (gs == GS_WIN) DrawOverlay("A.R.1.3.L. DESCONECTADA", "Os quatro protocolos caíram.   [R] jogar de novo   [ENTER] menu", C_AMBER, 1);
}

/* ------------------------------------------------------------------ input / previas / main */
static Input ReadInput(void)
{
    Input in = {0};
    if (IsKeyDown(KEY_W)) in.move.y -= 1;
    if (IsKeyDown(KEY_S)) in.move.y += 1;
    if (IsKeyDown(KEY_A)) in.move.x -= 1;
    if (IsKeyDown(KEY_D)) in.move.x += 1;
    if (IsKeyDown(KEY_UP)) in.shoot.y -= 1;
    if (IsKeyDown(KEY_DOWN)) in.shoot.y += 1;
    if (IsKeyDown(KEY_LEFT)) in.shoot.x -= 1;
    if (IsKeyDown(KEY_RIGHT)) in.shoot.x += 1;
    return in;
}

/* ------------------------------------------------------------------ interface do modulo */
typedef struct InvKeys
{
    bool esc, q, r, enter, space;
    int skipStage; /* -1 = nenhum; 0..NSTAGE-1 = pular para o estagio (so com INV_DEBUG_SKIP) */
} InvKeys;

static bool invLoaded = false;

/* Aplica as teclas de controle do modo. Retorna true quando o jogador pediu para voltar ao menu. */
static bool InvApplyKeys(InvKeys k)
{
    if (k.esc && (gs == GS_PLAY || gs == GS_PAUSE))
    {
        gs = (gs == GS_PLAY) ? GS_PAUSE : GS_PLAY;
        return false;
    }
    if (gs == GS_PAUSE && k.q) return true;
    if (gs == GS_OVER)
    {
        if (k.esc || k.q) return true;
        if (k.r) RetryStage();
    }
    else if (gs == GS_WIN)
    {
        if (k.enter || k.q) return true;
        if (k.r) NewGame(0);
    }
    else if (gs == GS_STORY && story.t > 0.4f && (k.space || k.enter))
    {
        gs = GS_PLAY;
    }
    if (INV_DEBUG_SKIP && k.skipStage >= 0 && k.skipStage < NSTAGE) NewGame(k.skipStage);
    return false;
}

static void InvLeave(GameState *game)
{
    StopInvBgm();
    game->screen = SCREEN_TITLE;
}

void InvEnter(GameState *game)
{
    if (!invLoaded)
    {
        texFloppy = LoadTexSmooth(ASSETS "sprites/disquete.png");
        texCd = LoadTexSmooth(ASSETS "sprites/cd.png");
        texSsd = LoadTexSmooth(ASSETS "sprites/ssd.png");
        LoadHeart(ASSETS "sprites/vidas.png");
        texElira = LoadTexture(ASSETS "sprites/invasao/elira_sheet.png");
        SetTextureFilter(texElira, TEXTURE_FILTER_POINT);
        LoadFaces(ASSETS "sprites/invasao/elira_rostos.png");
        texFx = LoadTexture(ASSETS "sprites/invasao/elira_fx.png");
        LoadEnemies(ASSETS "sprites/invasao/inimigos.png");
        texBoss[0] = LoadTexSmooth(ASSETS "sprites/invasao/boss_1990.png");
        texBoss[1] = LoadTexSmooth(ASSETS "sprites/invasao/boss_2008.png");
        texBoss[2] = LoadTexSmooth(ASSETS "sprites/invasao/boss_2026.png");
        texBoss[3] = LoadTexSmooth(ASSETS "sprites/invasao/boss_2048.png");
        SetTextureFilter(texFx, TEXTURE_FILTER_BILINEAR);
        LoadShotSfx();
        invLoaded = true;
    }
    invAudioMuted = game ? game->musicMuted : false;
    NewGame(0);
    game->screen = SCREEN_INVASION;
}

void InvUpdate(GameState *game, float dt)
{
    invAudioMuted = game ? game->musicMuted : false;
    InvKeys k = {0};
    k.esc = IsKeyPressed(KEY_ESCAPE);
    k.q = IsKeyPressed(KEY_Q);
    k.r = IsKeyPressed(KEY_R);
    k.enter = IsKeyPressed(KEY_ENTER);
    k.space = IsKeyPressed(KEY_SPACE);
    k.skipStage = -1;
    for (int s = 0; s < NSTAGE; s++)
        if (IsKeyPressed(KEY_ONE + s)) k.skipStage = s;

    if (InvApplyKeys(k))
    {
        InvLeave(game);
        return;
    }
    UpdateInvBgm(stage, gs == GS_PAUSE || gs == GS_OVER || gs == GS_WIN);
    Update(fminf(dt, 1.0f / 30), gs == GS_PLAY ? ReadInput() : (Input){0});
}

void InvDraw(void)
{
    /* Mistura separada para o alfa do render target nao "furar" os sprites transparentes. */
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    Draw();
    EndBlendMode();
}

void InvUnload(void)
{
    UnloadInvBgm();
    UnloadShotSfx();
    if (!invLoaded) return;
    Texture2D *texs[] = {&texFloppy, &texCd, &texSsd, &texHeart, &texHeartEmpty, &texElira, &texFaces, &texFx,
                         &texEnemies, &texBoss[0], &texBoss[1], &texBoss[2], &texBoss[3]};
    for (int i = 0; i < (int)(sizeof texs / sizeof texs[0]); i++)
    {
        if (texs[i]->id) UnloadTexture(*texs[i]);
        *texs[i] = (Texture2D){0};
    }
    for (int px = 0; px < 97; px++)
    {
        if (g_fontOk[px]) UnloadFont(g_font[px]);
        g_fontOk[px] = false;
    }
    invLoaded = false;
}
