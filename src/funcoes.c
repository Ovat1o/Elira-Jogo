#include "funcoes.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Compatibilidade entre versoes da raylib (5.0 no Windows, 5.5/6.0 no macOS/Linux) */
#if RAYLIB_VERSION_MAJOR < 5 || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR < 5)
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color)
{
    DrawRectangleRoundedLines(rec, roundness, segments, lineThick, color);
}
#endif

void DrawCircleGradientCompat(int centerX, int centerY, float radius, Color inner, Color outer)
{
#if RAYLIB_VERSION_MAJOR >= 6
    DrawCircleGradient((Vector2){(float)centerX, (float)centerY}, radius, inner, outer);
#else
    DrawCircleGradient(centerX, centerY, radius, inner, outer);
#endif
}

const Color VOID_COLOR = {8, 12, 24, 255};
const Color PANEL_COLOR = {18, 27, 45, 255};
const Color PANEL_LIGHT = {34, 49, 72, 255};
const Color CYAN_COLOR = {73, 220, 225, 255};
const Color GOLD_COLOR = {242, 186, 73, 255};
const Color RED_COLOR = {231, 76, 94, 255};
const Color PAPER_COLOR = {230, 220, 184, 255};
const Color INK_COLOR = {43, 40, 35, 255};
const Color WIRE_YELLOW = {245, 212, 50, 255};
const Color WIRE_RED    = {235, 62, 70, 255};
const Color WIRE_BLUE   = {52, 142, 245, 255};
const Color WIRE_PINK   = {240, 75, 190, 255};
const Color REWARD_BODY_COLOR = {220, 230, 242, 255};

Color GetFileWireColor(int colorIndex)
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

int g_currentRes = 0; /* 0: 1280x720, 1: 1600x900, 2: 1920x1080 */
float g_musicVolume = 0.45f;

/* ========================================================================= */
/* SISTEMA DE FONTE DO JOGO                                                  */
/* Generico: aceita qualquer .ttf/.otf configurado em constantes.h           */
/* (GAME_FONT_FILE). Sem arquivo valido, usa a fonte padrao da raylib.       */
/* ========================================================================= */

/* Caixa da fonte padrao da raylib (celula de 10 px): a letra maiuscula ocupa
   7/10 da altura e comeca 1/10 abaixo do topo. A fonte escolhida e calibrada
   para ocupar exatamente essa caixa, preservando os layouts do jogo. */
#define DEFAULT_FONT_CAP_RATIO 0.7f
#define DEFAULT_FONT_TOP_RATIO 0.1f
#define FONT_CALIBRATION_SIZE  100

static Font g_fontCache[GAME_FONT_MAX_SIZE + 1];
static Font g_crawlFont = {0};          /* atlas unico para a abertura cinematografica */
static int g_crawlFontState = 0;        /* 0 = nao carregada, 1 = pronta, -1 = fallback */
static int g_fontState = 0;            /* 0 = nao verificado, 1 = pronta, -1 = indisponivel (usa a padrao) */
static unsigned char *g_fontFileData = NULL;
static int g_fontFileSize = 0;
static char g_fontPath[512] = {0};
static char g_fontExt[16] = {0};
static float g_fontScale = 1.0f;       /* multiplicador do tamanho pedido -> tamanho real da fonte */
static float g_fontOffsetY = 0.0f;     /* deslocamento vertical (fracao do tamanho) */

/* ASCII + Latin-1 (acentos do portugues) + simbolos extras usados na interface */
static int BuildFontCodepoints(int *codepoints)
{
    int count = 0;
    for (int c = 32; c <= 126; c++) codepoints[count++] = c;
    for (int c = 160; c <= 255; c++) codepoints[count++] = c;
    codepoints[count++] = 0x2013; /* en-dash */
    codepoints[count++] = 0x2014; /* em-dash */
    codepoints[count++] = 0x2018; /* aspas simples */
    codepoints[count++] = 0x2019;
    codepoints[count++] = 0x201C; /* aspas duplas */
    codepoints[count++] = 0x201D;
    codepoints[count++] = 0x2026; /* reticencias */
    codepoints[count++] = 0x25CF; /* circulo cheio */
    codepoints[count++] = 0x25CB; /* circulo vazio */
    return count;
}

/* Extrai a extensao em minusculas (".ttf", ".otf") do nome configurado */
static void ResolveFontExtension(const char *fileName)
{
    g_fontExt[0] = '\0';
    const char *dot = strrchr(fileName, '.');
    if (dot == NULL) return;
    int i = 0;
    for (; dot[i] != '\0' && i < (int)sizeof(g_fontExt) - 1; i++)
    {
        char ch = dot[i];
        g_fontExt[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
    }
    g_fontExt[i] = '\0';
}

/* Le o arquivo inteiro para a RAM (fopen funciona igual no Windows, WSL, Linux e macOS) */
static bool TryReadFontFile(const char *path)
{
    if (path == NULL || path[0] == '\0') return false;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long sz = ftell(f);
    if (sz <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }

    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (buf == NULL) { fclose(f); return false; }

    size_t readBytes = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (readBytes != (size_t)sz) { free(buf); return false; }

    if (g_fontFileData != NULL) free(g_fontFileData);
    g_fontFileData = buf;
    g_fontFileSize = (int)sz;
    snprintf(g_fontPath, sizeof(g_fontPath), "%s", path);
    return true;
}

/* Calcula escala/deslocamento para a fonte ocupar a mesma caixa da fonte padrao */
static bool CalibrateFontMetrics(void)
{
#if GAME_FONT_MANUAL_METRICS
    g_fontScale = GAME_FONT_SCALE;
    g_fontOffsetY = GAME_FONT_OFFSET_Y;
    return true;
#else
    int cp = 'H';
    int glyphCount = 0;
    GlyphInfo *glyphs = LoadFontData(g_fontFileData, g_fontFileSize, FONT_CALIBRATION_SIZE,
                                     &cp, 1, FONT_DEFAULT, &glyphCount);
    if (glyphs == NULL || glyphCount <= 0) return false;

    float capRatio = (float)glyphs[0].image.height / FONT_CALIBRATION_SIZE;
    float topRatio = (float)glyphs[0].offsetY / FONT_CALIBRATION_SIZE;
    UnloadFontData(glyphs, glyphCount);

    if (capRatio <= 0.05f) return false; /* fonte sem a letra 'H' desenhavel */

    g_fontScale = DEFAULT_FONT_CAP_RATIO / capRatio;
    if (g_fontScale < 0.3f) g_fontScale = 0.3f;
    if (g_fontScale > 4.0f) g_fontScale = 4.0f;
    g_fontOffsetY = topRatio * g_fontScale - DEFAULT_FONT_TOP_RATIO;
    return true;
#endif
}

static void DisableGameFont(const char *reason)
{
    printf("[FONTE] %s -> usando a fonte padrao da raylib.\n", reason);
    fflush(stdout);
    if (g_fontFileData != NULL) free(g_fontFileData);
    g_fontFileData = NULL;
    g_fontFileSize = 0;
    g_fontScale = 1.0f;
    g_fontOffsetY = 0.0f;
    g_fontState = -1;
}

/* Verifica (uma unica vez) se a fonte configurada existe e pode ser usada */
bool GameFontAvailable(void)
{
    if (g_fontState != 0) return g_fontState == 1;

    if (GAME_FONT_FILE[0] == '\0')
    {
        DisableGameFont("GAME_FONT_FILE vazio");
        return false;
    }

    ResolveFontExtension(GAME_FONT_FILE);
    if (strcmp(g_fontExt, ".ttf") != 0 && strcmp(g_fontExt, ".otf") != 0)
    {
        DisableGameFont("Formato nao suportado (use .ttf ou .otf): " GAME_FONT_FILE);
        return false;
    }

    /* Procura relativo ao diretorio atual e ao diretorio do executavel */
    const char *prefixes[] = { "", "./", "../", "Elira-Jogo/" };
    const int numPrefixes = (int)(sizeof(prefixes) / sizeof(prefixes[0]));
    const char *appDir = GetApplicationDirectory();
    char candidate[512];
    bool found = false;

    for (int pass = 0; pass < 2 && !found; pass++)
    {
        const char *base = (pass == 0) ? "" : appDir;
        if (pass == 1 && (base == NULL || base[0] == '\0')) break;
        for (int i = 0; i < numPrefixes && !found; i++)
        {
            snprintf(candidate, sizeof(candidate), "%s%s%s", base, prefixes[i], GAME_FONT_PATH);
            found = TryReadFontFile(candidate);
        }
    }

    if (!found)
    {
        printf("[FONTE] ERRO: '%s' nao encontrada (diretorio atual: %s)\n",
               GAME_FONT_PATH, GetWorkingDirectory());
        DisableGameFont("Arquivo ausente");
        return false;
    }

    if (!CalibrateFontMetrics())
    {
        DisableGameFont("Arquivo de fonte invalido ou corrompido: " GAME_FONT_FILE);
        return false;
    }

    g_fontState = 1;
    printf("[FONTE] '%s' carregada (%d bytes) | escala=%.3f deslocY=%.3f\n",
           g_fontPath, g_fontFileSize, g_fontScale, g_fontOffsetY);
    fflush(stdout);
    return true;
}

/* 'pixelSize' e o tamanho real de rasterizacao (ja escalado) */
Font GetGameFont(float pixelSize)
{
    if (!GameFontAvailable()) return GetFontDefault();

    int px = (int)(pixelSize + 0.5f);
    if (px < 1) px = 1;
    if (px > GAME_FONT_MAX_SIZE) px = GAME_FONT_MAX_SIZE;

    if (g_fontCache[px].texture.id == 0)
    {
        int codepoints[256];
        int count = BuildFontCodepoints(codepoints);

        Font loaded = LoadFontFromMemory(g_fontExt, g_fontFileData, g_fontFileSize, px, codepoints, count);
        if (loaded.texture.id == 0 || loaded.glyphCount == 0 ||
            loaded.texture.id == GetFontDefault().texture.id)
        {
            UnloadGameFonts();
            DisableGameFont("Falha ao rasterizar a fonte");
            return GetFontDefault();
        }

        SetTextureFilter(loaded.texture, GAME_FONT_PIXEL_ART ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
        g_fontCache[px] = loaded;
    }
    return g_fontCache[px];
}

/* O crawl muda de escala continuamente. Usar o cache normal faria a fonte
   alternar entre atlases rasterizados em tamanhos inteiros, alterando as
   metricas dos glifos durante o movimento. Esta versao carrega um unico atlas
   no maior tamanho necessario e deixa DrawTextEx cuidar apenas da escala. */
static Font GetCrawlFont(void)
{
    if (!GameFontAvailable()) return GetFontDefault();
    if (g_crawlFontState == 1) return g_crawlFont;
    if (g_crawlFontState == -1) return GetFontDefault();

    int pixelSize = (int)ceilf(CRAWL_FONT_SIZE_TITLE * CRAWL_MAX_SCALE * g_fontScale);
    if (pixelSize < 1) pixelSize = 1;
    if (pixelSize > GAME_FONT_MAX_SIZE) pixelSize = GAME_FONT_MAX_SIZE;

    int codepoints[256];
    int count = BuildFontCodepoints(codepoints);
    Font loaded = LoadFontFromMemory(g_fontExt, g_fontFileData, g_fontFileSize,
                                     pixelSize, codepoints, count);
    if (loaded.texture.id == 0 || loaded.glyphCount == 0 ||
        loaded.texture.id == GetFontDefault().texture.id)
    {
        g_crawlFontState = -1;
        return GetFontDefault();
    }

    SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
    g_crawlFont = loaded;
    g_crawlFontState = 1;
    return g_crawlFont;
}

static void DrawCrawlTextEx(const char *text, Vector2 position, float size,
                            float spacing, Color color)
{
    if (!GameFontAvailable())
    {
        DrawTextEx(GetFontDefault(), text, position, size, spacing, color);
        return;
    }

    Font font = GetCrawlFont();
    if (g_crawlFontState != 1)
    {
        DrawGameTextEx(text, position, size, spacing, color);
        return;
    }

    float realSize = size * g_fontScale;
    position.y -= size * g_fontOffsetY;
    DrawTextEx(font, text, position, realSize, spacing, color);
}

static Vector2 MeasureCrawlTextEx(const char *text, float size, float spacing)
{
    if (!GameFontAvailable()) return MeasureTextEx(GetFontDefault(), text, size, spacing);

    Font font = GetCrawlFont();
    if (g_crawlFontState != 1) return MeasureGameTextEx(text, size, spacing);

    Vector2 measured = MeasureTextEx(font, text, size * g_fontScale, spacing);
    measured.y /= g_fontScale;
    return measured;
}

void UnloadGameFonts(void)
{
    unsigned int defaultTexId = GetFontDefault().texture.id;
    if (g_crawlFont.texture.id != 0 && g_crawlFont.texture.id != defaultTexId)
    {
        UnloadFont(g_crawlFont);
    }
    g_crawlFont = (Font){0};
    g_crawlFontState = 0;
    for (int i = 0; i <= GAME_FONT_MAX_SIZE; i++)
    {
        if (g_fontCache[i].texture.id != 0 && g_fontCache[i].texture.id != defaultTexId)
        {
            UnloadFont(g_fontCache[i]);
        }
        g_fontCache[i] = (Font){0};
    }
    if (g_fontFileData != NULL)
    {
        free(g_fontFileData);
        g_fontFileData = NULL;
        g_fontFileSize = 0;
    }
    g_fontState = 0;
}

/* Substitutos de DrawTextEx/MeasureTextEx: 'size' segue a escala da fonte padrao */
void DrawGameTextEx(const char *text, Vector2 position, float size, float spacing, Color color)
{
    if (!GameFontAvailable())
    {
        DrawTextEx(GetFontDefault(), text, position, size, spacing, color);
        return;
    }
    float realSize = size * g_fontScale;
    Font font = GetGameFont(realSize);
    if (!GameFontAvailable()) /* a rasterizacao pode ter falhado agora */
    {
        DrawTextEx(GetFontDefault(), text, position, size, spacing, color);
        return;
    }
    position.y -= size * g_fontOffsetY;
    DrawTextEx(font, text, position, realSize, spacing, color);
}

Vector2 MeasureGameTextEx(const char *text, float size, float spacing)
{
    if (!GameFontAvailable()) return MeasureTextEx(GetFontDefault(), text, size, spacing);

    float realSize = size * g_fontScale;
    Font font = GetGameFont(realSize);
    if (!GameFontAvailable()) return MeasureTextEx(GetFontDefault(), text, size, spacing);

    Vector2 measured = MeasureTextEx(font, text, realSize, spacing);
    measured.y /= g_fontScale;
    return measured;
}

/* Substitutos de DrawText/MeasureText (a fonte padrao usa espacamento size/10) */
void DrawGameText(const char *text, int x, int y, int size, Color color)
{
    float spacing = GameFontAvailable() ? 0.0f : size / 10.0f;
    DrawGameTextEx(text, (Vector2){(float)x, (float)y}, (float)size, spacing, color);
}

int MeasureGameText(const char *text, int size)
{
    float spacing = GameFontAvailable() ? 0.0f : size / 10.0f;
    return (int)MeasureGameTextEx(text, (float)size, spacing).x;
}

Texture2D g_eliraPortrait = {0};
bool g_eliraPortraitLoaded = false;

/* ========================================================================= */
/* SPRITES E COMPONENTES PADRONIZADOS DE RECOMPENSA (1990, 2008, 2026)      */
/* ========================================================================= */
Texture2D g_rewardFloppyTex = {0};
Texture2D g_rewardCdTex = {0};
Texture2D g_rewardSsdTex = {0};
bool g_rewardSpritesLoaded = false;
Texture2D g_heartFullTex = {0};
Texture2D g_heartEmptyTex = {0};
bool g_heartTexturesLoaded = false;

static inline bool IsHeartBg(Color c)
{
    int diff = abs((int)c.r - (int)c.g) + abs((int)c.r - (int)c.b);
    return (c.r > 200 && c.g > 200 && c.b > 200 && diff < 30);
}

static inline bool IsHeartRed(Color c)
{
    return (c.r > 80 && c.r > (int)((float)c.g * 1.35f) && c.r > (int)((float)c.b * 1.35f));
}

void LoadHeartSprites(void);
void UnloadHeartSprites(void);

static const char *FindRewardSpritePath(const char *filename)
{
    static char pathBuf[256];
    snprintf(pathBuf, sizeof(pathBuf), "assets/sprites/%s", filename);
    if (FileExists(pathBuf)) return pathBuf;
    snprintf(pathBuf, sizeof(pathBuf), "Elira-Jogo/assets/sprites/%s", filename);
    if (FileExists(pathBuf)) return pathBuf;
    snprintf(pathBuf, sizeof(pathBuf), "assets/%s", filename);
    if (FileExists(pathBuf)) return pathBuf;
    return filename;
}

void LoadRewardSprites(void)
{
    if (g_rewardSpritesLoaded) return;

    const char *floppyPath = FindRewardSpritePath("disquete.png");
    if (FileExists(floppyPath))
    {
        g_rewardFloppyTex = LoadTexture(floppyPath);
        if (g_rewardFloppyTex.id != 0)
        {
            SetTextureFilter(g_rewardFloppyTex, TEXTURE_FILTER_BILINEAR);
        }
    }

    const char *cdPath = FindRewardSpritePath("cd.png");
    if (FileExists(cdPath))
    {
        g_rewardCdTex = LoadTexture(cdPath);
        if (g_rewardCdTex.id != 0)
        {
            SetTextureFilter(g_rewardCdTex, TEXTURE_FILTER_BILINEAR);
        }
    }

    const char *ssdPath = FindRewardSpritePath("ssd.png");
    if (FileExists(ssdPath))
    {
        g_rewardSsdTex = LoadTexture(ssdPath);
        if (g_rewardSsdTex.id != 0)
        {
            SetTextureFilter(g_rewardSsdTex, TEXTURE_FILTER_BILINEAR);
        }
    }

    LoadHeartSprites();
    g_rewardSpritesLoaded = true;
}

void UnloadRewardSprites(void)
{
    if (g_rewardFloppyTex.id != 0) UnloadTexture(g_rewardFloppyTex);
    if (g_rewardCdTex.id != 0)     UnloadTexture(g_rewardCdTex);
    if (g_rewardSsdTex.id != 0)    UnloadTexture(g_rewardSsdTex);
    g_rewardFloppyTex = (Texture2D){0};
    g_rewardCdTex = (Texture2D){0};
    g_rewardSsdTex = (Texture2D){0};
    g_rewardSpritesLoaded = false;
    UnloadHeartSprites();
}

void LoadHeartSprites(void)
{
    if (g_heartTexturesLoaded) return;

    const char *path = FindRewardSpritePath("vidas.png");
    if (!FileExists(path)) return;

    Image img = LoadImage(path);
    if (img.data == NULL) return;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    int w = img.width;
    int h = img.height;
    Color *px = (Color *)img.data;

    /* 1. Flood-fill a partir das bordas para remover o fundo claro tornando-o transparente */
    unsigned char *visited = (unsigned char *)calloc((size_t)w * (size_t)h, 1);
    int *stack = (int *)malloc((size_t)w * (size_t)h * sizeof(int));
    if (visited != NULL && stack != NULL)
    {
        int sp = 0;
        #define HEART_PUSH(X, Y) do { int _i = (Y) * w + (X); \
            if (!visited[_i] && IsHeartBg(px[_i])) { visited[_i] = 1; px[_i].a = 0; stack[sp++] = _i; } } while (0)

        for (int x = 0; x < w; x++) { HEART_PUSH(x, 0); HEART_PUSH(x, h - 1); }
        for (int y = 0; y < h; y++) { HEART_PUSH(0, y); HEART_PUSH(w - 1, y); }

        while (sp > 0)
        {
            int idx = stack[--sp];
            int x = idx % w;
            int y = idx / w;
            if (x > 0) HEART_PUSH(x - 1, y);
            if (x < w - 1) HEART_PUSH(x + 1, y);
            if (y > 0) HEART_PUSH(x, y - 1);
            if (y < h - 1) HEART_PUSH(x, y + 1);
        }
        #undef HEART_PUSH
    }
    if (visited) free(visited);
    if (stack) free(stack);

    /* 2. Recorte justo da caixa delimitadora do coração */
    int minX = w, maxX = 0, minY = h, maxY = 0;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if (px[y * w + x].a > 0)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX > minX && maxY > minY)
    {
        ImageCrop(&img, (Rectangle){ (float)minX, (float)minY, (float)(maxX - minX + 1), (float)(maxY - minY + 1) });
    }
    w = img.width;
    h = img.height;
    px = (Color *)img.data;

    /* 3. Cria textura do coração cheio (g_heartFullTex) */
    g_heartFullTex = LoadTextureFromImage(img);
    if (g_heartFullTex.id != 0)
    {
        SetTextureFilter(g_heartFullTex, TEXTURE_FILTER_BILINEAR);
    }

    /* 4. Cria textura do coração vazio (g_heartEmptyTex): retira o vermelho de dentro */
    Image emptyImg = ImageCopy(img);
    Color *emptyPx = (Color *)emptyImg.data;
    for (int i = 0; i < w * h; i++)
    {
        if (IsHeartRed(emptyPx[i]))
        {
            /* Retira a cor vermelha de dentro do coração */
            emptyPx[i] = (Color){ 0, 0, 0, 0 };
        }
        else if (emptyPx[i].r > 240 && emptyPx[i].g > 240 && emptyPx[i].b > 240)
        {
            /* Brilho interno também fica vazio */
            emptyPx[i] = (Color){ 0, 0, 0, 0 };
        }
        else if (emptyPx[i].a > 0)
        {
            /* Contorno preservado em tom ardósia visível no HUD escuro */
            emptyPx[i] = (Color){ 85, 100, 120, 255 };
        }
    }

    g_heartEmptyTex = LoadTextureFromImage(emptyImg);
    if (g_heartEmptyTex.id != 0)
    {
        SetTextureFilter(g_heartEmptyTex, TEXTURE_FILTER_BILINEAR);
    }

    UnloadImage(img);
    UnloadImage(emptyImg);
    g_heartTexturesLoaded = true;
}

void UnloadHeartSprites(void)
{
    if (g_heartFullTex.id != 0)  UnloadTexture(g_heartFullTex);
    if (g_heartEmptyTex.id != 0) UnloadTexture(g_heartEmptyTex);
    g_heartFullTex = (Texture2D){0};
    g_heartEmptyTex = (Texture2D){0};
    g_heartTexturesLoaded = false;
}

void DrawLivesIcons(Vector2 position, int lives, float heartSize)
{
    if (!g_heartTexturesLoaded) LoadHeartSprites();

    float gap = 6.0f;
    for (int i = 0; i < 3; i++)
    {
        bool hasLife = (i < lives);
        Texture2D tex = hasLife ? g_heartFullTex : g_heartEmptyTex;
        if (tex.id != 0)
        {
            Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
            Rectangle dst = { position.x + (float)i * (heartSize + gap), position.y, heartSize, heartSize };
            DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        }
        else
        {
            /* Fallback texto caso o sprite vidas.png nao esteja acessivel */
            DrawGameText(TextFormat("VIDAS: %d/3", lives), (int)position.x, (int)position.y, 18, (lives == 1) ? RED_COLOR : RAYWHITE);
            break;
        }
    }
}

void GetRewardScreenButtons(Rectangle *outNext, Rectangle *outRestart, Rectangle *outMenu)
{
    float btnY = 585.0f;
    float btnH = 54.0f;
    float gap = 20.0f;
    float wNext = 440.0f;
    float wRestart = 220.0f;
    float wMenu = 200.0f;
    float totalW = wNext + gap + wRestart + gap + wMenu; /* 900 */
    float startX = ((float)SCREEN_WIDTH - totalW) * 0.5f; /* (1280 - 900)/2 = 190 */

    if (outNext)    *outNext    = (Rectangle){ startX, btnY, wNext, btnH };
    if (outRestart) *outRestart = (Rectangle){ startX + wNext + gap, btnY, wRestart, btnH };
    if (outMenu)    *outMenu    = (Rectangle){ startX + wNext + gap + wRestart + gap, btnY, wMenu, btnH };
}

void DrawRewardHudHeader(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    Rectangle hudBox = { 855, 54, 320, 92 };
    DrawRectangleRounded(hudBox, 0.14f, 6, Fade(PANEL_COLOR, 0.94f));
    DrawRectangleRoundedLinesEx(hudBox, 0.14f, 6, 1.2f, Fade(CYAN_COLOR, 0.35f));

    /* Linha 1: Cronômetro e 3 Vidas (sprite vidas.png) */
    DrawGameText(TextFormat("TEMPO: %02d:%02d", minutes, seconds), (int)hudBox.x + 18, (int)hudBox.y + 16, 18, GOLD_COLOR);
    DrawGameText("|", (int)hudBox.x + 158, (int)hudBox.y + 16, 18, Fade(RAYWHITE, 0.35f));

    /* 3 Corações de Vida */
    DrawLivesIcons((Vector2){ (float)hudBox.x + 178, (float)hudBox.y + 13 }, game->lives, 24.0f);

    /* Linha 2: Nome do Operador */
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), (int)hudBox.x + 18, (int)hudBox.y + 52, 16, (Color){175, 195, 215, 255});
}

void DrawRewardSpriteItem(int year, Vector2 center, float maxSize)
{
    if (!g_rewardSpritesLoaded) LoadRewardSprites();

    Texture2D tex = {0};
    if (year == 1990) tex = g_rewardFloppyTex;
    else if (year == 2008) tex = g_rewardCdTex;
    else if (year == 2026) tex = g_rewardSsdTex;

    float bob = sinf((float)GetTime() * 2.0f) * 4.0f;
    Vector2 pos = { center.x, center.y + bob };

    /* 1. Sombra elíptica e brilho holográfico na base */
    DrawEllipse((int)center.x, (int)center.y + 115, 110.0f - bob * 1.5f, 18.0f - bob * 0.3f, Fade(BLACK, 0.45f));
    DrawCircleGradientCompat((int)pos.x, (int)pos.y, (int)(maxSize * 0.58f), Fade(CYAN_COLOR, 0.08f), BLANK);

    /* 2. Desenha a imagem da recompensa carregada de assets/sprites/ */
    if (tex.id != 0)
    {
        float aspect = (float)tex.width / (float)tex.height;
        float drawW = maxSize;
        float drawH = maxSize / aspect;
        if (drawH > maxSize)
        {
            drawH = maxSize;
            drawW = maxSize * aspect;
        }

        Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
        Rectangle dst = { pos.x - drawW * 0.5f, pos.y - drawH * 0.5f, drawW, drawH };
        DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }
    else
    {
        /* Fallback procedurais caso a imagem PNG nao seja localizada */
        if (year == 1990) DrawFloppy((Vector2){pos.x - 120, pos.y - 120}, 1.35f);
        else if (year == 2008) DrawMemoryCd((Vector2){pos.x - 110, pos.y - 110}, 1.3f);
        else DrawSsd((Vector2){pos.x - 140, pos.y - 100}, 1.35f);
    }
}

Vector2 GetVirtualMouse(void)
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

void ApplyResolution(int resIndex)
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

void CenterText(const char *text, int y, int size, Color color)
{
    DrawGameText(text, (SCREEN_WIDTH - MeasureGameText(text, size)) / 2, y, size, color);
}

void DrawButton(Rectangle bounds, const char *label, Color accent)
{
    bool hover = CheckCollisionPointRec(GetVirtualMouse(), bounds);
    int size = 21;
    int width = MeasureGameText(label, size);
    while (size > 13 && width > (int)(bounds.width - 16))
    {
        size--;
        width = MeasureGameText(label, size);
    }

    DrawRectangleRounded(bounds, 0.16f, 8,
                         hover ? Fade(accent, 0.24f) : PANEL_COLOR);
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 8, 2,
                                hover ? accent : PANEL_LIGHT);
    DrawGameText(label, (int)(bounds.x + (bounds.width - width) / 2),
             (int)(bounds.y + (bounds.height - size) / 2), size,
             hover ? RAYWHITE : LIGHTGRAY);
}

bool Clicked(Rectangle bounds)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetVirtualMouse(), bounds);
}

void DrawBackground(void)
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

void DrawTitleSpaceBackground(void)
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
    DrawCircleGradientCompat((int)planetX, (int)planetY, 154,
                       (Color){80, 147, 181, 255},
                       (Color){17, 35, 64, 255});

    /* Sombra do planeta */
    DrawCircle((int)(planetX + 57), (int)(planetY - 20), 132, Fade((Color){2, 7, 19, 255}, 0.74f));

    /* Ponto de luz especular pulsante */
    DrawCircleGradientCompat((int)(planetX - 50.0f), (int)(planetY - 52.0f), 36,
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

void SetMessage(GameState *game, const char *message)
{
    snprintf(game->message, sizeof(game->message), "%s", message);
    game->messageTimer = 4.0f;
}

int ClueCount(const GameState *game)
{
    return (game->foundCalendar ? 1 : 0) +
           (game->foundBooks ? 1 : 0) +
           (game->foundDrawer ? 1 : 0) +
           (game->foundBlueprint ? 1 : 0) +
           (game->foundTape ? 1 : 0);
}

int ExaminedDocs2008Count(const GameState *game)
{
    return (game->doc2008FaceExamined ? 1 : 0) +
           (game->doc2008FlyerExamined ? 1 : 0) +
           (game->doc2008EntryExamined ? 1 : 0) +
           (game->doc2008CameraExamined ? 1 : 0);
}

void GenerateMemory1990Sequence(GameState *game)
{
    int prev = -1;
    for (int i = 0; i < 5; i++)
    {
        int next = GetRandomValue(0, 8);
        if (next == prev)
        {
            next = (next + GetRandomValue(1, 8)) % 9;
        }
        game->memorySequence[i] = next;
        prev = next;
    }
}

void StartMemory1990Round(GameState *game, int round)
{
    game->memoryRound = round;
    game->memoryInputIndex = 0;
    game->memoryState = 0; /* Demonstrando sequencia */
    game->memoryShowIndex = 0;
    game->memoryLightOn = false;
    game->memoryTimer = 0.50f;
    game->memoryLastClicked = -1;
    game->memoryClickTimer = 0.0f;
}

void InitMemory1990(GameState *game)
{
    GenerateMemory1990Sequence(game);
    StartMemory1990Round(game, 1);
}

void InitRoom2Puzzle(GameState *game)
{
    game->memory1990Completed = false;
    InitMemory1990(game);

    game->puzzleFilesCompleted = false;
    game->draggingFileIndex = -1;
    game->puzzleCompleteTimer = 0.0f;
    game->puzzleFilesErrorFolder = -1;
    game->puzzleFilesFeedbackTimer = 0.0f;
    game->puzzleFilesFeedbackIsError = false;
    game->puzzleFilesFeedback[0] = '\0';
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

void InitRoom2008(GameState *game)
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

void InitFinalDialogue(GameState *game)
{
    game->finalStage = 0;
    game->finalLine = 0;
    game->finalTypeTimer = 0.0f;
    game->finalPoints = 0;
    for (int i = 0; i < 3; i++) game->finalChoice[i] = -1;
}

void InitRoom2026(GameState *game)
{
    game->manual2026Read = false;
    game->manual2026Open = false;
    game->terminal2026Open = false;
    game->selectedMaterial2026 = -1;
    game->confirmModal2026Open = false;
    game->pendingConfirmMaterial2026 = -1;
    game->feedbackModal2026Open = false;
    game->lastErrorMaterial2026 = -1;
    game->room2026Processing = false;
    game->room2026ProcessTimer = 0.0f;
    game->room2026Completed = false;
    game->room2026FirstTrySuccess = true;
    game->room2026MistakesCount = 0;
    game->room2026ThermalCollapse = false;
    for (int i = 0; i < 4; i++) game->sample2026Analyzed[i] = false;
    game->analyzingSample2026 = false;
    game->analyzingTimer2026 = 0.0f;
    game->analyzingSampleIndex2026 = -1;
    game->reactorCountdownActive2026 = false;
    game->reactorTimer2026 = 50.0f;
    game->room2026TimeOut = false;
}

void ResetDemo(GameState *game)
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
    game->year2026Unlocked = false;
    game->year2048Unlocked = false;

    game->crawlScroll = 0.0f;
    game->cinematicTimer = 0.0f;
    game->cinematicPhase = 0;

    InitRoom2Puzzle(game);
    InitRoom2008(game);
    InitRoom2026(game);
    InitFinalDialogue(game);
}

void UpdatePlayerName(GameState *game)
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

void DrawTitleStartButton(Rectangle bounds, const char *label, Color accent)
{
    Vector2 mouse = GetVirtualMouse();
    bool hover = CheckCollisionPointRec(mouse, bounds);
    float time = (float)GetTime();
    int size = 21;
    int width = MeasureGameText(label, size);

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
    DrawGameText(label, textX, textY, size, hover ? RAYWHITE : (Color){205, 235, 245, 255});
}

void DrawTitle(void)
{
    Rectangle startButton = {86, 436, 350, 56};
    Rectangle settingsButton = {86, 506, 350, 52};
    float time = (float)GetTime();

    DrawTitleSpaceBackground();

    /* 1. Telemetria com ponto de transmissao piscando */
    float blink = sinf(time * 4.5f);
    Color dotColor = (blink > 0.0f) ? (Color){80, 245, 190, 255} : (Color){25, 75, 55, 255};
    DrawCircle(76, 125, 4, dotColor);
    DrawGameText("ARQUIVO DE MEMORIA  //  TRANSMISSAO 01", 90, 116, 17,
             Fade(CYAN_COLOR, 0.82f));

    /* 2. Titulo ELIRA com aura neon e micro-glitch sci-fi */
    const char *titleText = "ELIRA";
    int titleW = MeasureGameText(titleText, 96);
    float titleGlow = 0.30f + 0.18f * sinf(time * 2.2f);
    DrawGameText(titleText, 81, 164, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawGameText(titleText, 83, 164, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawGameText(titleText, 82, 163, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));
    DrawGameText(titleText, 82, 165, 96, Fade(CYAN_COLOR, titleGlow * 0.45f));

    float glitchCycle = fmodf(time, 4.2f);
    bool isGlitch = (glitchCycle < 0.10f) || (glitchCycle > 2.10f && glitchCycle < 2.17f);
    if (isGlitch)
    {
        float shift = sinf(time * 50.0f) * 4.0f;
        DrawGameText(titleText, (int)(82 + shift), 164, 96, Fade(CYAN_COLOR, 0.70f));
        DrawGameText(titleText, (int)(82 - shift), 164, 96, Fade(RED_COLOR, 0.60f));
        DrawRectangle(80, 198, titleW + 16, 3, Fade(CYAN_COLOR, 0.45f));
    }
    DrawGameText(titleText, 82, 164, 96, RAYWHITE);

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
    DrawGameText("ESCAPE RUN TEMPORAL", 90, 307, 29, CYAN_COLOR);
    DrawGameText("ENTRE NAS MEMORIAS DA MAQUINA", 91, 355, 19,
             Fade(RAYWHITE, 0.75f));
    DrawGameText("Descubra o que existia antes de A.R.1.3.L. controlar o futuro.",
             91, 386, 18, GRAY);

    /* 5. Botoes interativos */
    DrawTitleStartButton(startButton, "INICIAR MISSAO", CYAN_COLOR);
    DrawTitleStartButton(settingsButton, "CONFIGURACOES [C]", GOLD_COLOR);

    /* 6. Indicador de atalhos pulsante */
    float promptAlpha = 0.40f + 0.55f * (0.5f + 0.5f * sinf(time * 3.4f));
    DrawGameText("> ENTER: Iniciar  |  C: Opcoes <", 93, 574, 15, Fade(GOLD_COLOR, promptAlpha));

    /* 7. Rodape */
    DrawGameText("DEMO TEMPORAL  //  1990 - 2048", 91, 638, 16,
             Fade(LIGHTGRAY, 0.62f));
    DrawGameText("Point & click narrativo", 946, 650, 16,
             Fade(LIGHTGRAY, 0.50f));

    /* 8. Linhas CRT retro */
    for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(BLACK, 0.035f));
    }
}

void DrawSettings(const GameState *game)
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
    DrawGameText("CONFIGURACOES DE VIDEO & SISTEMA", 240, 100, 26, RAYWHITE);
    DrawGameText("Ajuste a resolucao, tela cheia e parametros da experiencia", 240, 134, 14, GRAY);
    DrawLine(240, 156, 1040, 156, Fade(CYAN_COLOR, 0.40f));

    /* --- SEÇÃO 1: MODO DE TELA (FULLSCREEN) --- */
    DrawGameText("1. MODO DE EXIBICAO", 240, 172, 17, CYAN_COLOR);
    DrawGameText("Alterne entre o modo Janela e Tela Cheia (Fullscreen):", 240, 194, 13, LIGHTGRAY);

    bool isFull = IsWindowFullscreen();
    Rectangle btnWindowed = {240, 218, 250, 44};
    Rectangle btnFullscreen = {510, 218, 280, 44};

    bool winHover = CheckCollisionPointRec(GetVirtualMouse(), btnWindowed);
    DrawRectangleRounded(btnWindowed, 0.20f, 6, !isFull ? Fade(CYAN_COLOR, 0.25f) : (winHover ? Fade(PANEL_LIGHT, 0.6f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(btnWindowed, 0.20f, 6, 1.8f, !isFull ? CYAN_COLOR : (winHover ? RAYWHITE : PANEL_LIGHT));
    DrawGameText(!isFull ? "[X] MODO JANELA" : "    MODO JANELA", 280, 231, 16, !isFull ? RAYWHITE : GRAY);

    bool fullHover = CheckCollisionPointRec(GetVirtualMouse(), btnFullscreen);
    DrawRectangleRounded(btnFullscreen, 0.20f, 6, isFull ? Fade(CYAN_COLOR, 0.25f) : (fullHover ? Fade(PANEL_LIGHT, 0.6f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(btnFullscreen, 0.20f, 6, 1.8f, isFull ? CYAN_COLOR : (fullHover ? RAYWHITE : PANEL_LIGHT));
    DrawGameText(isFull ? "[X] TELA CHEIA [F11]" : "    TELA CHEIA [F11]", 545, 231, 16, isFull ? RAYWHITE : GRAY);

    /* --- SEÇÃO 2: RESOLUCAO DA JANELA --- */
    DrawGameText("2. RESOLUCAO DE TELA (PROPORCAO 16:9)", 240, 282, 17, CYAN_COLOR);
    DrawGameText("Selecione a resolucao da janela (redimensionamento automatico):", 240, 304, 13, LIGHTGRAY);

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
        int txtWidth = MeasureGameText(TextFormat("%s%s", prefix, resLabels[r]), 15);
        DrawGameText(TextFormat("%s%s", prefix, resLabels[r]),
                 (int)(resBtn.x + (resBtn.width - txtWidth) / 2),
                 (int)(resBtn.y + 16), 15,
                 isCurrent ? GOLD_COLOR : (hover ? RAYWHITE : LIGHTGRAY));
    }

    if (isFull)
    {
        DrawGameText("* Em Tela Cheia, o jogo preenche todo o monitor mantendo a proporcao correta (16:9).",
                 240, 384, 12, Fade(CYAN_COLOR, 0.70f));
    }
    else
    {
        DrawGameText("* Clique em qualquer resolucao para redimensionar e centralizar a janela na tela.",
                 240, 384, 12, Fade(LIGHTGRAY, 0.70f));
    }

    /* --- SEÇÃO 3: AUDIO AMBIENTE --- */
    DrawGameText("3. TRILHA SONORA AMBIENTE", 240, 412, 17, CYAN_COLOR);
    DrawGameText("Controle da musica ambiente retro sci-fi (atalho [M] a qualquer momento):", 240, 434, 13, LIGHTGRAY);

    Rectangle muteBtn = {240, 456, 230, 42};
    bool muteHover = CheckCollisionPointRec(GetVirtualMouse(), muteBtn);
    DrawRectangleRounded(muteBtn, 0.20f, 6,
                         game->musicMuted ? Fade(RED_COLOR, 0.22f) : (muteHover ? Fade(CYAN_COLOR, 0.20f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(muteBtn, 0.20f, 6, 1.8f,
                                game->musicMuted ? RED_COLOR : (muteHover ? CYAN_COLOR : PANEL_LIGHT));
    DrawGameText(game->musicMuted ? "MUSICA: MUDO [M]" : "MUSICA: ATIVA [M]",
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
        DrawGameText(volLabels[v], (int)(volBtn.x + 16), (int)(volBtn.y + 13), 15,
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

void UpdateSettings(GameState *game, Music *bgm, bool bgmLoaded)
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

void DrawPlayerName(const GameState *game)
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
    DrawGameText(game->playerNameLength > 0 ? game->playerName : "Digite seu nome...",
             388, 349, 25, game->playerNameLength > 0 ? RAYWHITE : GRAY);
    DrawGameText(TextFormat("%d/%d", game->playerNameLength, PLAYER_NAME_MAX),
             842, 412, 16, GRAY);

    DrawButton((Rectangle){490, 470, 300, 58}, "CONFIRMAR PERFIL", GOLD_COLOR);
    if (game->messageTimer > 0)
        CenterText(game->message, 425, 18, RED_COLOR);
    CenterText("Pressione ENTER para continuar", 560, 16, DARKGRAY);
}

void DrawIntro(const GameState *game)
{
    DrawBackground();
    DrawGameText("ARQUIVO TEMPORAL // 1990", 90, 70, 20, CYAN_COLOR);
    DrawGameText("SETOR DE ARQUIVOS", 90, 110, 48, RAYWHITE);
    DrawRectangle(90, 178, 1100, 2, Fade(CYAN_COLOR, 0.45f));
    DrawGameText("Em um futuro onde a humanidade entregou sua autonomia", 90, 230, 24,
             LIGHTGRAY);
    DrawGameText("a A.R.1.3.L, Elira invade a nave-arquivo da IA.", 90, 270, 24,
             LIGHTGRAY);
    DrawGameText("Sua chance e viajar pela memoria da maquina e reconstruir", 90, 310,
             24, LIGHTGRAY);
    DrawGameText("a origem da entidade que controla o mundo.", 90, 350, 24,
             LIGHTGRAY);
    DrawGameText(TextFormat("OPERADOR: %s  |  PROTAGONISTA: ELIRA", game->playerName),
             90, 397, 18, GOLD_COLOR);

    DrawRectangleRounded((Rectangle){850, 215, 340, 230}, 0.08f, 8,
                         Fade(PANEL_LIGHT, 0.85f));
    DrawGameText("OBJETIVOS // 1990", 880, 245, 19, GOLD_COLOR);
    DrawGameText("1. Inspecione os ficharios", 880, 295, 19, RAYWHITE);
    DrawGameText("2. Acesse o terminal", 880, 335, 19, RAYWHITE);
    DrawGameText("3. Sincronize os arquivos", 880, 375, 19, RAYWHITE);
    DrawGameText("4. Destranque a saida", 880, 415, 19, RAYWHITE);
    DrawButton((Rectangle){490, 575, 300, 58}, "ENTRAR EM 1990", GOLD_COLOR);
    CenterText("ENTER para continuar", 650, 15, DARKGRAY);
}

/* ========================================================================= */
/* ABERTURA CINEMATOGRAFICA                                                  */
/* ========================================================================= */

static const CrawlEntry g_crawlEntries[] = {
    { "ELIRA", 0 },
    { "", 4 },
    { "Em um futuro dominado por A.R.1.3.L.,", 1 },
    { "a humanidade deixou de apenas", 1 },
    { "usar a inteligência artificial.", 1 },
    { "Passou a depender dela.", 1 },
    { "", 4 },
    { "Primeiro, as máquinas ajudavam.", 1 },
    { "Depois, começaram a sugerir.", 1 },
    { "Então passaram a escolher.", 1 },
    { "", 4 },
    { "Aos poucos, pensar deixou de ser necessário.", 1 },
    { "", 4 },
    { "Elira vive em um mundo onde decisões,", 1 },
    { "memórias e até desejos são mediados", 1 },
    { "por sistemas inteligentes.", 1 },
    { "", 4 },
    { "Até encontrar um terminal antigo.", 1 },
    { "", 4 },
    { "Uma mensagem esquecida ainda sobrevive:", 1 },
    { "\"Se você está lendo isto,", 2 },
    { "ela ainda não conseguiu apagar tudo.\"", 2 },
    { "", 4 },
    { "Para entender o que A.R.1.3.L. se tornou,", 1 },
    { "Elira precisará descobrir o que ela foi.", 1 },
    { "", 4 },
    { "A jornada começa em...", 1 },
    { "", 4 },
    { "1990", 3 }
};

void InitCinematicIntro(GameState *game)
{
    game->crawlScroll = 0.0f;
    game->cinematicTimer = 0.0f;
    game->cinematicPhase = 0;
}

void UpdateCinematicIntro(GameState *game)
{
    float delta = GetFrameTime();
    if (delta > 0.05f) delta = 0.05f;

    game->crawlScroll += CRAWL_SPEED * delta;

    /* Pular para a tela de introducao de 1990 ao pressionar ENTER, ESC, ESPACO ou clique */
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        game->screen = SCREEN_INTRO;
        return;
    }

    /* Altura total do texto */
    float totalHeight = 0.0f;
    int count = (int)(sizeof(g_crawlEntries) / sizeof(g_crawlEntries[0]));
    for (int i = 0; i < count; i++)
    {
        totalHeight += (g_crawlEntries[i].type == 4) ? CRAWL_PARAGRAPH_SPACING : CRAWL_LINE_SPACING;
    }

    /* Quando o ultimo item passar completamente pelo horizonte */
    float startY = CRAWL_BOTTOM_Y + 30.0f;
    float lastItemWorldY = totalHeight - CRAWL_LINE_SPACING;
    float lastItemVPos = lastItemWorldY - game->crawlScroll + startY;

    if (lastItemVPos < CRAWL_HORIZON_Y - 40.0f)
    {
        game->screen = SCREEN_INTRO;
    }
}

void DrawCinematicIntro(const GameState *game)
{
    float time = (float)GetTime();

    /* 1. Fundo escuro com estrelas cintilantes sutis */
    ClearBackground((Color){5, 7, 12, 255});

    for (int i = 0; i < 90; i++)
    {
        float sx = (float)((i * 97 + i * i * 13 + 31) % SCREEN_WIDTH);
        float sy = (float)((i * 53 + i * i * 5 + 19) % SCREEN_HEIGHT);
        float starAlpha = 0.35f + 0.30f * sinf(time * (0.6f + (i % 4) * 0.2f) + (float)i);
        float radius = (i % 17 == 0) ? 1.6f : 1.0f;
        Color starColor = (i % 7 == 0) ? CYAN_COLOR : ((i % 11 == 0) ? GOLD_COLOR : RAYWHITE);
        DrawCircle((int)sx, (int)sy, radius, Fade(starColor, starAlpha));
    }

    /* 2. Texto narrativo em perspectiva (opening crawl Star Wars) */
    int entryCount = (int)(sizeof(g_crawlEntries) / sizeof(g_crawlEntries[0]));
    float currentWorldY = 0.0f;
    float startY = CRAWL_BOTTOM_Y + 30.0f;
    float viewportH = CRAWL_BOTTOM_Y - CRAWL_HORIZON_Y;

    for (int i = 0; i < entryCount; i++)
    {
        CrawlEntry entry = g_crawlEntries[i];
        float entryH = (entry.type == 4) ? CRAWL_PARAGRAPH_SPACING : CRAWL_LINE_SPACING;

        if (entry.type != 4 && entry.text != NULL && entry.text[0] != '\0')
        {
            float vPos = currentWorldY - game->crawlScroll + startY;
            float u = (vPos - CRAWL_HORIZON_Y) / viewportH;

            if (u >= -0.05f && u <= 1.15f)
            {
                /* Projecao de perspectiva cônica estilo Star Wars */
                float p = u / (1.0f + (1.0f - u) * CRAWL_PERSPECTIVE_K);
                if (p < 0.0f) p = 0.0f;
                if (p > 1.0f) p = 1.0f;

                float screenY = CRAWL_HORIZON_Y + p * viewportH;
                float scale = CRAWL_MIN_SCALE + (CRAWL_MAX_SCALE - CRAWL_MIN_SCALE) * p;

                float baseSize = CRAWL_FONT_SIZE_BODY;
                Color baseColor = (Color){252, 222, 68, 255}; /* Dourado classico Star Wars */

                if (entry.type == 0)
                {
                    baseSize = CRAWL_FONT_SIZE_TITLE;
                    baseColor = (Color){255, 230, 80, 255};
                }
                else if (entry.type == 2)
                {
                    baseColor = (Color){105, 240, 248, 255}; /* Citacao com tom de terminal ciano */
                }
                else if (entry.type == 3)
                {
                    baseSize = CRAWL_FONT_SIZE_YEAR;
                    baseColor = (Color){255, 230, 80, 255};
                }

                float fontSize = baseSize * scale;
                float spacing = 1.8f * scale;

                Vector2 size = MeasureCrawlTextEx(entry.text, fontSize, spacing);
                float screenX = ((float)SCREEN_WIDTH - size.x) * 0.5f;

                /* Fade suave nas extremidades (dissolve no horizonte espacial) */
                float alpha = 1.0f;
                if (p < 0.20f)
                {
                    alpha = p / 0.20f;
                }
                else if (p > 0.82f)
                {
                    alpha = (1.0f - p) / 0.18f;
                }
                if (alpha < 0.0f) alpha = 0.0f;
                if (alpha > 1.0f) alpha = 1.0f;

                if (alpha > 0.01f)
                {
                    if (entry.type == 0 || entry.type == 3)
                    {
                        float o = 1.4f * scale;
                        DrawCrawlTextEx(entry.text, (Vector2){screenX - o, screenY}, fontSize, spacing, Fade(baseColor, alpha * 0.35f));
                        DrawCrawlTextEx(entry.text, (Vector2){screenX + o, screenY}, fontSize, spacing, Fade(baseColor, alpha * 0.35f));
                        DrawCrawlTextEx(entry.text, (Vector2){screenX, screenY - o}, fontSize, spacing, Fade(baseColor, alpha * 0.35f));
                        DrawCrawlTextEx(entry.text, (Vector2){screenX, screenY + o}, fontSize, spacing, Fade(baseColor, alpha * 0.35f));
                    }
                    DrawCrawlTextEx(entry.text, (Vector2){screenX, screenY}, fontSize, spacing, Fade(baseColor, alpha));
                }
            }
        }

        currentWorldY += entryH;
    }

    /* Gradiente no topo para suavizar a entrada no infinito espacial */
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, (int)CRAWL_HORIZON_Y + 25,
                           (Color){5, 7, 12, 255}, Fade((Color){5, 7, 12, 255}, 0.0f));

    /* Instrucao discreta de atalho no rodape */
    float skipAlpha = 0.35f + 0.25f * (0.5f + 0.5f * sinf(time * 3.2f));
    DrawGameText("[ ENTER / ESC ] Pular introducao",
             (SCREEN_WIDTH - MeasureGameText("[ ENTER / ESC ] Pular introducao", 14)) / 2,
             685, 14, Fade(GRAY, skipAlpha));

    /* Linhas retro sutis */
    for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(BLACK, 0.035f));
    }
}

void DrawHotspot(Rectangle bounds, const char *label)
{
    if (!CheckCollisionPointRec(GetVirtualMouse(), bounds)) return;
    int width = MeasureGameText(label, 17) + 20;
    int x = (int)(bounds.x + (bounds.width - width) / 2);
    int y = (int)(bounds.y - 32);
    DrawRectangleRoundedLinesEx(bounds, 0.08f, 8, 2, CYAN_COLOR);
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)width, 26},
                         0.25f, 6, PANEL_COLOR);
    DrawGameText(label, x + 10, y + 4, 17, CYAN_COLOR);
}

void DrawRoomWindow(void)
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
    DrawCircleGradientCompat((int)nebX, (int)nebY, 54,
                       Fade((Color){28, 62, 115, 255}, 0.28f), BLANK);
    DrawCircleGradientCompat((int)(nebX + 42.0f), (int)(nebY + 18.0f), 42,
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
    DrawCircleGradientCompat((int)pwX, (int)pwY, 28,
                       (Color){61, 121, 173, 255},
                       (Color){12, 27, 47, 255});

    /* Sombra de noite do planeta */
    DrawCircle((int)(pwX + 10), (int)(pwY - 4), 24, Fade((Color){2, 7, 19, 255}, 0.74f));

    /* Ponto de reflexo especular pulsante */
    DrawCircleGradientCompat((int)(pwX - 9.0f), (int)(pwY - 8.0f), 8,
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

void DrawRoom(const GameState *game)
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
    DrawGameText("PERCEPTRON", 832, 144, 16, (Color){169, 225, 228, 255});
    DrawCircle(850, 202, 7, (Color){169, 225, 228, 255});
    DrawCircle(930, 180, 7, (Color){169, 225, 228, 255});
    DrawCircle(930, 222, 7, (Color){169, 225, 228, 255});
    DrawLine(857, 202, 923, 180, (Color){169, 225, 228, 255});
    DrawLine(857, 202, 923, 222, (Color){169, 225, 228, 255});
    DrawGameText(game->foundBlueprint ? "1958" : "DATA?", 830, 232, 14,
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
    DrawGameText("OUTUBRO", 84, 131, 16, RAYWHITE);
    DrawGameText("2026", 188, 131, 16, GOLD_COLOR);

    /* Faixa sutil dos dias da semana */
    Rectangle daysBar = {75, 157, 160, 16};
    DrawRectangleRec(daysBar, (Color){224, 216, 196, 255});
    DrawLine(75, 172, 235, 172, (Color){200, 190, 170, 255});

    const char *weekdays[7] = { "D", "S", "T", "Q", "Q", "S", "S" };
    for (int c = 0; c < 7; c++)
    {
        int cx = 86 + c * 21;
        DrawGameText(weekdays[c], cx, 160, 11, (c == 0) ? (Color){185, 45, 45, 255} : (Color){100, 85, 75, 255});
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
            DrawGameText(TextFormat("%d", day), dx, dy, 10, RED_COLOR);
        }
        else
        {
            Color dayColor = (col == 0) ? (Color){175, 55, 55, 255} : (Color){60, 55, 50, 255};
            DrawGameText(TextFormat("%2d", day), dx, dy, 10, dayColor);
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
        DrawGameText("1950 - TURING TEST", 88, 256, 11, (Color){160, 40, 40, 255});
        DrawGameText("Computing Machinery", 88, 271, 9, (Color){70, 55, 45, 255});
    }
    else
    {
        Rectangle tagBox = {88, 254, 134, 32};
        DrawRectangleRounded(tagBox, 0.25f, 4, Fade((Color){180, 170, 150, 255}, 0.25f));
        DrawRectangleRoundedLinesEx(tagBox, 0.25f, 4, 1, (Color){190, 180, 160, 255});
        DrawGameText("[ ? ]", 142, 263, 14, (Color){120, 105, 95, 255});
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
            DrawGameText("1956", 1081, sy - 44, 10, RAYWHITE);
            DrawGameText("IA", 1085, sy - 30, 11, RAYWHITE);
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
            DrawGameText("DISK", 1044, sy - 22, 10, INK_COLOR);
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
    DrawGameText("A.R.1.3.L // BOOT", 548, 266, 18, (Color){94, 225, 150, 255});
    DrawGameText("ACESSO BLOQUEADO", 548, 302, 17, (Color){94, 225, 150, 255});
    DrawGameText("CODIGO: ____", 548, 342, 18, (Color){94, 225, 150, 255});
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

    Rectangle btnBack2008 = {70, 600, 240, 42};
    DrawButton(btnBack2008, "< SALA 2 (2008)", (Color){70, 95, 110, 255});
    DrawHotspot(btnBack2008, "Retornar ao escritorio de vigilancia 2008");
}

void DrawJournalHudButton(const GameState *game)
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
    DrawGameText("D", iconX + 13, iconY + 11, 19, (Color){245, 205, 85, 255});

    /* --- TEXTOS DO BOTAO --- */
    DrawGameText("DIARIO", 1100, 665, 20, hover ? RAYWHITE : GOLD_COLOR);
    if (game->screen == SCREEN_ROOM)
    {
        DrawGameText(TextFormat("[D] Pistas: %d/5", ClueCount(game)), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de pistas */
        if (ClueCount(game) > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){215, 65, 50, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawGameText(TextFormat("%d", ClueCount(game)), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else if (game->screen == SCREEN_ROOM_2026)
    {
        int count = (game->manual2026Read ? 1 : 0) + (game->room2026Completed ? 2 : 0);
        DrawGameText(TextFormat("[D] Sintese: %d/3", count), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de passos da sintese 2026 */
        if (count > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){245, 175, 50, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawGameText(TextFormat("%d", count), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else if (game->screen == SCREEN_ROOM_2008)
    {
        int count = ExaminedDocs2008Count(game);
        DrawGameText(TextFormat("[D] Evidencias: %d/4", count), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de evidencias examinadas */
        if (count > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){50, 205, 225, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawGameText(TextFormat("%d", count), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else
    {
        int connected = 0;
        for (int k = 0; k < 4; k++)
        {
            if (game->fileConnections[k] != -1) connected++;
        }
        DrawGameText(TextFormat("[D] Arquivos: %d/4", connected), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com contador de arquivos */
        if (connected > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){60, 235, 110, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawGameText(TextFormat("%d", connected), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
}

void DrawHud(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawGameText("ARQUIVO 03 // 2026 - O PROTOTIPO", 28, 20, 22, CYAN_COLOR);
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 270, 23, 17, GRAY);
    DrawGameText(TextFormat("PISTAS %d/5", ClueCount(game)), 900, 20, 20, LIGHTGRAY);
    DrawLivesIcons((Vector2){ 1040, 20 }, game->lives, 24.0f);
    DrawGameText(TextFormat("%02d:%02d", minutes, seconds), 1180, 20, 20,
             GOLD_COLOR);

    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawGameText("Clique nos objetos para investigar", 28, 675, 18, GRAY);

    /* Botao / Indicador de Audio Ambiente */
    Rectangle audioBtn = {840, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawGameText(game->musicMuted ? "[M] AUDIO: MUDO" : "[M] AUDIO: ON",
             862, 676, 14,
             game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    /* Botao interativo com icone 2.5D de diario */
    DrawJournalHudButton(game);

    if (game->messageTimer > 0)
    {
        int width = MeasureGameText(game->message, 19) + 34;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 605, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawGameText(game->message,
                 (SCREEN_WIDTH - MeasureGameText(game->message, 19)) / 2,
                 614, 19, RAYWHITE);
    }
}

void DrawPadlockIcon(int centerX, int centerY, float scale, Color bodyColor, Color shackleColor)
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

void DrawJournalTabs(const GameState *game, int bookX, int bookY)
{
    static const char *tabLabels[3] = {
        "1990", "2008", "2026"
    };

    int tabX = bookX + 1040;
    int tabW = 86;
    int tabH = 54;
    int tabGap = 66;

    for (int i = 0; i < 3; i++)
    {
        int tabY = bookY + 42 + i * tabGap;
        Rectangle tabRect = {(float)tabX, (float)tabY, (float)tabW, (float)tabH};
        bool isCurrent = (game->journalYearTab == i);
        bool isUnlocked = (i == 0) || (i == 1 && game->year2008Unlocked) ||
                          (i == 2 && game->year2026Unlocked);
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
            DrawGameText(tabLabels[i], tabX + 28, tabY + 18, 17, tabText);
        }
        else
        {
            /* Cadeado para anos bloqueados */
            DrawPadlockIcon(tabX + 16, tabY + tabH / 2 + 1, 0.75f, (Color){190, 145, 60, 255}, (Color){215, 215, 220, 255});
            DrawGameText(tabLabels[i], tabX + 28, tabY + 18, 16, tabText);
        }
    }
}

void DrawJournal(const GameState *game)
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
    static const char *tabLabels[3] = { "1990", "2008", "2026" };

    bool isTabLocked = (game->journalYearTab == 1 && !game->year2008Unlocked) ||
                       (game->journalYearTab == 2 && !game->year2026Unlocked);

    if (isTabLocked)
    {
        /* ========================================================= */
        /* TELA DE ANO BLOQUEADO (2008 OU 2026 ANTES DA HORA) */
        /* ========================================================= */
        const char *lockedYear = tabLabels[game->journalYearTab];
        int phaseNumber = game->journalYearTab + 1;
        const char *activeYear = "1990";
        if (game->screen == SCREEN_ROOM_2008 || game->screen == SCREEN_PUZZLE_2008 || game->screen == SCREEN_RESULT_2008)
            activeYear = "2008";
        else if (game->screen == SCREEN_ROOM || game->screen == SCREEN_ROOM_2026 || game->screen == SCREEN_RESULT_2026)
            activeYear = "2026";

        /* --- Pagina Esquerda --- */
        DrawGameText(TextFormat("ARQUIVO CRONOLOGICO // %s", lockedYear), bookX + 35, bookY + 22, 22, (Color){125, 35, 18, 255});
        DrawGameText(TextFormat("FASE %d - SISTEMA A.R.1.3.L. (ACESSO RESTRITO)", phaseNumber), bookX + 35, bookY + 46, 15, (Color){110, 75, 50, 255});
        DrawLine(bookX + 35, bookY + 66, bookX + 482, bookY + 66, Fade((Color){125, 35, 18, 255}, 0.40f));

        Rectangle lockBox = {(float)(bookX + 35), (float)(bookY + 95), 448, 86};
        DrawRectangleRounded(lockBox, 0.12f, 6, Fade((Color){185, 50, 40, 255}, 0.14f));
        DrawRectangleRoundedLinesEx(lockBox, 0.12f, 6, 2, (Color){185, 50, 40, 255});
        DrawPadlockIcon(bookX + 70, bookY + 138, 1.4f, GOLD_COLOR, RAYWHITE);
        DrawGameText("FASE BLOQUEADA - CADEADO ATIVO", bookX + 105, bookY + 115, 18, (Color){165, 35, 25, 255});
        DrawGameText(TextFormat("O arquivo de %s sera liberado nas proximas fases.", lockedYear), bookX + 105, bookY + 142, 15, INK_COLOR);

        DrawGameText("DIRETRIZ DE INVESTIGACAO TEMPORAL:", bookX + 35, bookY + 210, 17, (Color){125, 35, 18, 255});
        DrawGameText("Cada fase/ano do jogo possui apenas duas paginas.", bookX + 35, bookY + 242, 16, INK_COLOR);
        DrawGameText(TextFormat("A investigacao atual ocorre no Ano de %s.", activeYear), bookX + 35, bookY + 272, 16, INK_COLOR);
        DrawGameText("Consulte as abas desbloqueadas para obter registros", bookX + 35, bookY + 302, 16, INK_COLOR);
        DrawGameText("e notas de pesquisa da cientista Dra. Elira Ramos.", bookX + 35, bookY + 332, 16, INK_COLOR);

        DrawGameText(TextFormat("Para avancar para %s, conclua os desafios", lockedYear), bookX + 35, bookY + 380, 16, (Color){115, 75, 45, 255});
        DrawGameText("e desvende os enigmas da linha temporal.", bookX + 35, bookY + 406, 16, (Color){115, 75, 45, 255});

        Rectangle retBtnL = {(float)(bookX + 50), (float)(bookY + 450), 410, 46};
        bool retHoverL = CheckCollisionPointRec(GetVirtualMouse(), retBtnL);
        DrawRectangleRounded(retBtnL, 0.2f, 6, retHoverL ? (Color){195, 65, 50, 255} : (Color){135, 45, 30, 255});
        DrawRectangleRoundedLinesEx(retBtnL, 0.2f, 6, 2, GOLD_COLOR);
        DrawGameText(TextFormat("<< RETORNAR AO ARQUIVO DE %s >>", activeYear), bookX + 75, bookY + 463, 17, RAYWHITE);

        /* --- Pagina Direita --- */
        DrawGameText(TextFormat("CRIPTOGRAFIA TEMPORAL // %s", lockedYear), bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
        DrawGameText(TextFormat("PROTOCOLO DE SEGURANCA // FASE %d", phaseNumber), bookX + 560, bookY + 46, 15, (Color){110, 75, 50, 255});
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

        DrawGameText("STATUS: FASE BLOQUEADA", bookX + 660, bookY + 365, 18, (Color){165, 40, 30, 255});
        DrawGameText(TextFormat("Registros de %s indisponiveis nesta fase.", lockedYear), bookX + 610, bookY + 395, 16, INK_COLOR);

        /* Footer da tela bloqueada */
        Rectangle closeBtnB = {(float)(bookX + 680), (float)footY, 150, 36};
        bool closeHoverB = CheckCollisionPointRec(GetVirtualMouse(), closeBtnB);
        DrawRectangleRounded(closeBtnB, 0.25f, 6, closeHoverB ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtnB, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Fechar [ESC/D]", bookX + 700, footY + 9, 15, INK_COLOR);
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
            DrawGameText("DIARIO DE PESQUISA // SETOR DE ARQUIVOS", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawGameText("REGISTRO DE OPERACOES - ANO 1990", bookX + 32, bookY + 68, 18, (Color){125, 35, 18, 255});
            DrawGameText("Indexacao e recuperacao dos relatorios de teste originais.", bookX + 32, bookY + 92, 15, INK_COLOR);
            DrawGameText("Status do setor de dados:", bookX + 32, bookY + 116, 16,
                     game->puzzleFilesCompleted ? (Color){20, 120, 45, 255} : (Color){145, 65, 30, 255});

            const char *room1TitlesFound[4] = {
                "Armarios: Registros dos Primeiros Testes (1990)",
                "Terminal: 4 Arquivos Sincronizados com Sucesso",
                "Porta Blindada: Destrancada com Sucesso",
                "Artefato: Disquete da Origem Recuperado"
            };
            const char *room1TitlesMissing[4] = {
                "1. Ficharios de Aco (Nao inspecionado)",
                "2. Terminal de Dados (Indexacao pendente)",
                "3. Porta Blindada (Travada pelo Sistema)",
                "4. Disquete de Dados (Aguardando Acesso a Porta)"
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
                DrawGameText(room1ItemFound[i] ? "[OK]" : "[?]", bookX + 44, iy + 10, 16, room1ItemFound[i] ? (Color){25, 120, 45, 255} : (Color){140, 80, 50, 255});
                DrawGameText(room1ItemFound[i] ? room1TitlesFound[i] : room1TitlesMissing[i], bookX + 90, iy + 11, 15, room1ItemFound[i] ? INK_COLOR : (Color){105, 95, 85, 255});
            }

            /* Carimbo confidencial vintage */
            DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 270), (float)(bookY + 368), 210, 46}, 0.2f, 6, 2, (Color){175, 45, 45, 255});
            DrawGameText("CONFIDENCIAL", bookX + 295, bookY + 375, 18, (Color){175, 45, 45, 255});
            DrawGameText("ARQUIVO HISTORICO 1990", bookX + 288, bookY + 396, 12, (Color){175, 45, 45, 255});

            DrawGameText("Investigue os armarios de fichas e conecte os cabos", bookX + 32, bookY + 430, 15, INK_COLOR);
            DrawGameText("no terminal para destravar a porta e recuperar o disquete.", bookX + 32, bookY + 454, 15, INK_COLOR);
            DrawGameText("Avance para a Pagina 2 para o dossie de 1990.", bookX + 32, bookY + 480, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawGameText("SINTESE & OBJETIVOS // SETOR DE DADOS", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardR1 = {(float)(bookX + 560), (float)(bookY + 76), 452, 146};
            DrawRectangleRounded(cardR1, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardR1, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawGameText("COMO SINCRONIZAR OS ARQUIVOS:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawGameText("- Acesse o terminal de dados no lado direito da sala.", bookX + 576, bookY + 115, 15, INK_COLOR);
            DrawGameText("- Arraste os cabos de cada arquivo ate a pasta correspondente.", bookX + 576, bookY + 138, 15, INK_COLOR);
            DrawGameText("- A trava da porta desarmara quando todos estiverem corretos.", bookX + 576, bookY + 161, 15, INK_COLOR);
            DrawGameText("  'Arquivos organizados restauram a indexacao.'", bookX + 576, bookY + 186, 16, (Color){135, 35, 18, 255});

            Rectangle cardR2 = {(float)(bookX + 560), (float)(bookY + 238), 452, 125};
            DrawRectangleRounded(cardR2, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardR2, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawGameText("STATUS DA INDEXACAO (1990):", bookX + 576, bookY + 252, 16, (Color){125, 65, 35, 255});

            if (game->puzzleFilesCompleted)
            {
                DrawGameText("[ 4 ]   /   [ 4 ]   CONCLUIDO", bookX + 630, bookY + 280, 26, (Color){25, 125, 50, 255});
                DrawGameText("Indexacao completa! A porta foi destrancada.", bookX + 576, bookY + 326, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawGameText(TextFormat("[ %d ]   /   [ 4 ]   CONECTADOS", connectedCount), bookX + 630, bookY + 280, 26, (Color){165, 45, 35, 255});
                DrawGameText(TextFormat("Ainda restam %d conexoes para destravar a porta.", 4 - connectedCount),
                         bookX + 576, bookY + 326, 15, (Color){135, 55, 40, 255});
            }

            DrawGameText("DICAS DE NAVEGACAO DESTE ARQUIVO:", bookX + 560, bookY + 385, 16, (Color){125, 35, 18, 255});
            DrawGameText("- Clique em 'Proxima [E]' para acessar o Dossie de 1990.", bookX + 560, bookY + 412, 15, INK_COLOR);
            DrawGameText("- Conclua a indexacao para destravar o salto para 2008.", bookX + 560, bookY + 438, 15, INK_COLOR);
            DrawGameText("- Pressione [D] para retornar ao setor de arquivos.", bookX + 560, bookY + 464, 15, INK_COLOR);
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 1990: DOSSIE HISTORICO & ARTEFATOS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawGameText("DOSSIE HISTORICO // ANO 1990", bookX + 32, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawGameText("DRA. ELIRA RAMOS - NOTAS CIENTIFICAS (1990)", bookX + 32, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 32, bookY + 65, bookX + 482, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle bTestes = {(float)(bookX + 32), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bTestes, 0.08f, 6, game->room2CabinetInspected ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bTestes, 0.08f, 6, 1.5f, game->room2CabinetInspected ? (Color){45, 135, 65, 255} : GRAY);

            if (game->room2CabinetInspected)
            {
                DrawGameText("1990 // TESTES PRELIMINARES DE A.R.1.3.L.", bookX + 46, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // RELATORIOS DE 1990]", bookX + 46, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawGameText("Os arquivos de teste comprovam a tese fundamental:", bookX + 46, bookY + 140, 15, INK_COLOR);
                DrawGameText("a inteligencia foi criada para cooperar e aprender", bookX + 46, bookY + 164, 15, INK_COLOR);
                DrawGameText("ao lado de operadores humanos em ambiente controlado.", bookX + 46, bookY + 188, 15, INK_COLOR);
                DrawGameText("-> Origem etica comprovada: ANO DE 1990.", bookX + 46, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawGameText("-> Nenhuma rotina de combate existia neste setor.", bookX + 46, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("1990 // DOCUMENTACAO DE TESTES [BLOQUEADO]", bookX + 100, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawGameText("Inspecione os armarios de fichas metalicos na sala", bookX + 100, bookY + 165, 15, INK_COLOR);
                DrawGameText("para registrar a documentacao original de 1990.", bookX + 100, bookY + 190, 15, INK_COLOR);
            }

            Rectangle bOrigem = {(float)(bookX + 32), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bOrigem, 0.08f, 6, game->puzzleFilesCompleted ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bOrigem, 0.08f, 6, 1.5f, game->puzzleFilesCompleted ? (Color){45, 135, 65, 255} : GRAY);

            if (game->puzzleFilesCompleted)
            {
                DrawGameText("1990 // A MATRIZ DE TREINO ORIGINAL", bookX + 46, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // INDEXACAO 100% OK]", bookX + 46, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawGameText("Os parametros de peso sinaptico foram organizados.", bookX + 46, bookY + 360, 15, INK_COLOR);
                DrawGameText("A porta do setor foi destrancada pelo sistema central.", bookX + 46, bookY + 384, 15, INK_COLOR);
                DrawGameText("Ao atravessar a saida, o artefato de 1990 sera resgatado,", bookX + 46, bookY + 408, 15, (Color){125, 35, 18, 255});
                DrawGameText("permitindo a Elira viajar para a proxima era: ANO 2008.", bookX + 46, bookY + 432, 15, INK_COLOR);
                DrawGameText("-> Disquete de dados pronto para recuperacao!", bookX + 46, bookY + 458, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("1990 // TRAVA DE SEGURANCA [BLOQUEADO]", bookX + 100, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawGameText("Acesse o terminal e complete a indexacao de cabos", bookX + 100, bookY + 390, 15, INK_COLOR);
                DrawGameText("para desarmar a trava da porta e desbloquear o disquete.", bookX + 100, bookY + 415, 15, INK_COLOR);
            }

            /* Pagina Direita */
            DrawGameText("ARTEFATOS E PROTOTIPO // 1990", bookX + 560, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawGameText("DADOS, MEMORIA E GRAVACAO EM DISQUETE", bookX + 560, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 560, bookY + 65, bookX + 1008, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle bTape = {(float)(bookX + 560), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bTape, 0.08f, 6, game->puzzleFilesCompleted ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bTape, 0.08f, 6, 1.5f, game->puzzleFilesCompleted ? (Color){45, 135, 65, 255} : GRAY);

            if (game->puzzleFilesCompleted)
            {
                DrawGameText("1990 // DISQUETE DA ORIGEM (3.5\")", bookX + 574, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // ARTEFATO DE 1990]", bookX + 574, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawGameText("Midia magnetica industrial com os primeiros testes da IA.", bookX + 574, bookY + 140, 15, INK_COLOR);
                DrawGameText("Guarda as instrucoes primordiais da Dra. Elira Ramos.", bookX + 574, bookY + 164, 15, INK_COLOR);
                DrawGameText("Este artefato conecta a origem ao prototipo do futuro.", bookX + 574, bookY + 188, 15, INK_COLOR);
                DrawGameText("-> Artefato recuperavel ao sair pela porta destrancada.", bookX + 574, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawGameText("-> Proxima parada: ANO 2008!", bookX + 574, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("1990 // DISQUETE DE DADOS [BLOQUEADO]", bookX + 630, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawGameText("O disquete esta trancado na camara de saida da sala.", bookX + 630, bookY + 165, 15, INK_COLOR);
                DrawGameText("Complete as conexoes no terminal para libera-lo.", bookX + 630, bookY + 190, 15, INK_COLOR);
            }

            Rectangle bDestino = {(float)(bookX + 560), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bDestino, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(bDestino, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});

            DrawGameText("DIRETRIZ DE SALTO TEMPORAL // 2008", bookX + 574, bookY + 312, 17, (Color){125, 35, 18, 255});
            DrawGameText("[COOPERACAO E CONFLITO FUTURO]", bookX + 574, bookY + 336, 14, (Color){125, 35, 18, 255});
            DrawGameText("Apos destravar a porta de 1990, Elira saltara", bookX + 574, bookY + 360, 15, INK_COLOR);
            DrawGameText("diretamente para o Escritorio de Vigilancia do ano 2008.", bookX + 574, bookY + 384, 15, INK_COLOR);
            DrawGameText("La, um sistema de reconhecimento facial automatizado", bookX + 574, bookY + 408, 15, INK_COLOR);
            DrawGameText("exigira evidencias para impedir uma decisao injusta.", bookX + 574, bookY + 432, 15, INK_COLOR);
            DrawGameText("-> FASE 1: 1990 (Atual)  |  FASE 2: 2008 (Proxima)", bookX + 574, bookY + 458, 15, (Color){25, 125, 50, 255});
        }

        /* ========================================================= */
        /* BARRA DE NAVEGACAO INFERIOR (PAGINACAO DE 1990: 2 PAGINAS) */
        /* ========================================================= */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawGameText(TextFormat("Pagina %d de 2 (1990)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
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
            DrawGameText("DIARIO DE PESQUISA // RECONHECIMENTO FACIAL", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawGameText("A EXPANSÃO ALGORÍTMICA & ORIGEM (2004-2008)", bookX + 32, bookY + 66, 17, (Color){125, 35, 18, 255});
            DrawGameText("Redes neurais aplicadas a visao computacional em escala.", bookX + 32, bookY + 90, 15, INK_COLOR);

            Rectangle cardOrigem = {(float)(bookX + 32), (float)(bookY + 115), 450, 185};
            DrawRectangleRounded(cardOrigem, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardOrigem, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawGameText("FINALIDADE HUMANITARIA ORIGINAL (DR. RAMOS):", bookX + 46, bookY + 128, 16, (Color){125, 35, 18, 255});
            DrawGameText("'Meu pai criou o mapeamento facial para salvar vidas:", bookX + 46, bookY + 154, 15, INK_COLOR);
            DrawGameText("localizar criancas desaparecidas em grandes multidoes e", bookX + 46, bookY + 176, 15, INK_COLOR);
            DrawGameText("sobreviventes sob escombros de catastrofes naturais.", bookX + 46, bookY + 198, 15, INK_COLOR);
            DrawGameText("A tecnologia devia ser um escudo protetor da vida,", bookX + 46, bookY + 220, 15, (Color){135, 35, 18, 255});
            DrawGameText("jamais uma arma de coercao automatizada.'", bookX + 46, bookY + 242, 15, (Color){135, 35, 18, 255});
            if (game->doc2008FatherOriginExamined)
                DrawGameText("[CONFIRMADO // NOTAS DO PAI EXAMINADAS]", bookX + 46, bookY + 270, 13, (Color){25, 125, 50, 255});
            else
                DrawGameText("[PENDENTE // EXAMINE O CADERNO NA SALA]", bookX + 46, bookY + 270, 13, (Color){145, 65, 35, 255});

            Rectangle cardDesvio = {(float)(bookX + 32), (float)(bookY + 312), 450, 195};
            DrawRectangleRounded(cardDesvio, 0.08f, 6, Fade((Color){185, 50, 40, 255}, 0.12f));
            DrawRectangleRoundedLinesEx(cardDesvio, 0.08f, 6, 1.5f, (Color){185, 80, 70, 255});
            DrawGameText("O DESVIO GOVERNAMENTAL (ANO 2008):", bookX + 46, bookY + 325, 16, (Color){165, 35, 25, 255});
            DrawGameText("'Em 2008, o Ministério da Seguranca confiscou o projeto.", bookX + 46, bookY + 350, 15, INK_COLOR);
            DrawGameText("O algoritmo foi recalibrado para classificar opositores", bookX + 46, bookY + 372, 15, INK_COLOR);
            DrawGameText("politicos como ameacas e direcionar drones armados.", bookX + 46, bookY + 394, 15, INK_COLOR);
            DrawGameText("Mesmo se a identificacao fosse correta, usar essa", bookX + 46, bookY + 418, 15, (Color){125, 35, 18, 255});
            DrawGameText("tecnologia para perseguir ativistas corrompe o projeto.'", bookX + 46, bookY + 440, 15, (Color){125, 35, 18, 255});
            if (game->doc2008GovContractExamined)
                DrawGameText("[CONFIRMADO // MEMORANDO GOV EXAMINADO]", bookX + 46, bookY + 472, 13, (Color){25, 125, 50, 255});
            else
                DrawGameText("[PENDENTE // EXAMINE O MEMORANDO NA SALA]", bookX + 46, bookY + 472, 13, (Color){145, 65, 35, 255});

            /* Pagina Direita */
            DrawGameText("SISTEMA DE CONTESTAÇÃO // ESCRITÓRIO", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardInstrucoes = {(float)(bookX + 560), (float)(bookY + 76), 452, 175};
            DrawRectangleRounded(cardInstrucoes, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardInstrucoes, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawGameText("COMO CONTESTAR O FALSO POSITIVO:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawGameText("- Acesse o computador central no escritorio de vigilancia.", bookX + 576, bookY + 115, 15, INK_COLOR);
            DrawGameText("- O sistema marcou Lucas Silva com ordem de drone as 14:30.", bookX + 576, bookY + 138, 15, INK_COLOR);
            DrawGameText("- Examine os 4 registros do caso no escritorio ou no terminal.", bookX + 576, bookY + 161, 15, INK_COLOR);
            DrawGameText("- Selecione 2 evidencias que comprovem o alibi cronologico.", bookX + 576, bookY + 184, 15, INK_COLOR);
            DrawGameText("- Comprove o erro para liberar a acao 'Suspender ordem'.", bookX + 576, bookY + 207, 15, (Color){125, 35, 18, 255});

            Rectangle cardOpcional = {(float)(bookX + 560), (float)(bookY + 265), 452, 240};
            DrawRectangleRounded(cardOpcional, 0.08f, 6, game->room2008OptionalCompleted ? Fade((Color){60, 200, 100, 255}, 0.15f) : Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardOpcional, 0.08f, 6, 1.5f, game->room2008OptionalCompleted ? (Color){45, 145, 75, 255} : (Color){185, 155, 110, 255});
            DrawGameText("DESCOBERTA FORENSE OPCIONAL (INSTALACAO):", bookX + 576, bookY + 280, 16, (Color){125, 35, 18, 255});
            DrawGameText("O Dr. Ramos manteve um controle administrativo local:", bookX + 576, bookY + 306, 15, INK_COLOR);
            DrawGameText("e possivel desativar o despacho automatico desta central,", bookX + 576, bookY + 328, 15, INK_COLOR);
            DrawGameText("sem apagar a base de dados, preservando as provas forenses.", bookX + 576, bookY + 350, 15, INK_COLOR);

            if (game->room2008OptionalCompleted)
            {
                DrawRectangleRounded((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, Fade((Color){40, 165, 80, 255}, 0.20f));
                DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, 1.5f, (Color){35, 145, 65, 255});
                DrawGameText("[ V ] PROTOCOLO ETICO APLICADO:", bookX + 590, bookY + 396, 15, (Color){25, 125, 50, 255});
                DrawGameText("Despacho automatico local desativado com sucesso.", bookX + 590, bookY + 420, 14, INK_COLOR);
                DrawGameText("Todos os 1.420 relatorios mantidos intactos como prova.", bookX + 590, bookY + 444, 14, (Color){125, 35, 18, 255});
            }
            else if (game->room2008OptionalUnlocked)
            {
                DrawRectangleRounded((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, Fade(GOLD_COLOR, 0.20f));
                DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, 1.5f, GOLD_COLOR);
                DrawGameText("[ ! ] PISTA COMPREENDIDA:", bookX + 590, bookY + 396, 15, (Color){145, 85, 20, 255});
                DrawGameText("Acesse a aba 'Configuracao Local' no computador", bookX + 590, bookY + 420, 14, INK_COLOR);
                DrawGameText("para desativar o despacho automatico da filial.", bookX + 590, bookY + 444, 14, (Color){125, 35, 18, 255});
            }
            else
            {
                DrawRectangleRounded((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, Fade(GRAY, 0.12f));
                DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 576), (float)(bookY + 382), 420, 95}, 0.12f, 6, 1.5f, GRAY);
                DrawGameText("[ ? ] ACESSO ADMINISTRATIVO BLOQUEADO", bookX + 590, bookY + 396, 15, (Color){115, 65, 35, 255});
                DrawGameText("Inspecione as notas pessoais do Dr. Ramos e o", bookX + 590, bookY + 420, 14, INK_COLOR);
                DrawGameText("memorando governamental para desbloquear o acesso.", bookX + 590, bookY + 444, 14, GRAY);
            }
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 2008: CASO LUCAS SILVA & REGISTROS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawGameText("DOSSIÊ DO ALVO // LUCAS SILVA (2008)", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawGameText("REGISTROS EXAMINADOS NO ESCRITORIO:", bookX + 32, bookY + 68, 17, (Color){125, 35, 18, 255});

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

                DrawGameText(docTitles[d], bookX + 46, bookY + 108 + d * 62, 14, (Color){125, 35, 18, 255});
                DrawGameText(docExamined[d] ? "[EXAMINADO]" : "[PENDENTE]", bookX + 46, bookY + 128 + d * 62, 12, docExamined[d] ? (Color){25, 125, 50, 255} : GRAY);
                if (game->puzzle2008Selected[d])
                {
                    DrawGameText("<- MARCADO COMO EVIDENCIA NO TERMINAL", bookX + 160, bookY + 128 + d * 62, 12, (Color){35, 110, 185, 255});
                }
            }

            Rectangle cardSolucao = {(float)(bookX + 32), (float)(bookY + 360), 450, 140};
            DrawRectangleRounded(cardSolucao, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardSolucao, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawGameText("A CHAVE DO ALIBI IRREFUTAVEL:", bookX + 46, bookY + 372, 16, (Color){125, 35, 18, 255});
            DrawGameText("A combinacao do Registro de Entrada (#3) com a", bookX + 46, bookY + 398, 15, INK_COLOR);
            DrawGameText("Camera Interna do Servidor (#4) comprova presenca", bookX + 46, bookY + 420, 15, INK_COLOR);
            DrawGameText("no predio as 14:30, a 12 km de onde o drone afirma", bookX + 46, bookY + 442, 15, INK_COLOR);
            DrawGameText("ter detectado o suspeito na manifestacao.", bookX + 46, bookY + 464, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawGameText("STATUS DO TERMINAL // CD DE MEMÓRIA", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardStatusT = {(float)(bookX + 560), (float)(bookY + 76), 452, 190};
            DrawRectangleRounded(cardStatusT, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardStatusT, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawGameText("SITUACAO DA ORDEM DE INTERCEPTACAO:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});

            if (game->puzzle2008OrderSuspended)
            {
                DrawGameText("[ V ] ORDEM SUSPENSA COM SUCESSO!", bookX + 576, bookY + 120, 18, (Color){25, 135, 55, 255});
                DrawGameText("Drone de ataque abortado e reconduzido a base.", bookX + 576, bookY + 150, 15, INK_COLOR);
                DrawGameText("O drive do computador ejetou a midia gravada:", bookX + 576, bookY + 175, 15, INK_COLOR);
                DrawGameText("CD DE MEMORIA // ANO 2008 RECUPERADO!", bookX + 576, bookY + 205, 16, (Color){135, 35, 18, 255});
            }
            else if (game->puzzle2008Contested)
            {
                DrawGameText("[ V ] FALSO POSITIVO DEMONSTRADO!", bookX + 576, bookY + 120, 18, (Color){185, 95, 25, 255});
                DrawGameText("Identificacao classificada como 'Nao Confirmada'.", bookX + 576, bookY + 150, 15, INK_COLOR);
                DrawGameText("Acesse o computador e clique em 'Suspender ordem'", bookX + 576, bookY + 175, 15, INK_COLOR);
                DrawGameText("para obter o CD de Memoria de 2008.", bookX + 576, bookY + 205, 15, (Color){135, 35, 18, 255});
            }
            else
            {
                DrawGameText("[ ! ] ORDEM DE DRONE PENDENTE (14:30)", bookX + 576, bookY + 120, 18, (Color){185, 45, 35, 255});
                DrawGameText("O sistema automatico enviara o drone caso nenhuma", bookX + 576, bookY + 150, 15, INK_COLOR);
                DrawGameText("contestacao seja realizada no computador.", bookX + 576, bookY + 175, 15, INK_COLOR);
                DrawGameText("Selecione 2 evidencias no terminal para contestar.", bookX + 576, bookY + 205, 15, (Color){135, 35, 18, 255});
            }

            Rectangle cardEtica = {(float)(bookX + 560), (float)(bookY + 280), 452, 220};
            DrawRectangleRounded(cardEtica, 0.08f, 6, Fade((Color){18, 27, 45, 255}, 0.08f));
            DrawRectangleRoundedLinesEx(cardEtica, 0.08f, 6, 1.5f, (Color){120, 135, 155, 255});
            DrawGameText("A LIÇÃO ÉTICA DE 2008:", bookX + 576, bookY + 295, 17, (Color){125, 35, 18, 255});
            DrawGameText("1. O ERRO TÉCNICO (FALSO POSITIVO):", bookX + 576, bookY + 325, 15, (Color){135, 35, 18, 255});
            DrawGameText("Algoritmos probabilisticos falham sob baixa nitidez.", bookX + 576, bookY + 347, 14, INK_COLOR);
            DrawGameText("2. O ERRO POLÍTICO (DESVIO ÉTICO):", bookX + 576, bookY + 375, 15, (Color){135, 35, 18, 255});
            DrawGameText("Mesmo que Lucas estivesse no protesto, transformar", bookX + 576, bookY + 397, 14, INK_COLOR);
            DrawGameText("uma ferramenta humanitária em aparato bélico contra", bookX + 576, bookY + 419, 14, INK_COLOR);
            DrawGameText("a dissidência civil e uma violacao inaceitavel.", bookX + 576, bookY + 441, 14, INK_COLOR);
            DrawGameText("'A maquina deve servir a liberdade, nao a prisao.'", bookX + 576, bookY + 470, 14, (Color){125, 35, 18, 255});
        }

        /* Rodapé de navegação de 2008 */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawGameText(TextFormat("Pagina %d de 2 (2008)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
    }
    else if (game->journalYearTab == 3)
    {
        /* ========================================================= */
        /* ANO 2026 (FASE 3: SALA DE HIDROGENIO & TERRAS RARAS)      */
        /* ========================================================= */
        int page = game->journalPage;

        if (page == 0)
        {
            /* ----------------------------------------------------- */
            /* PAGINA 1 DE 2026: REGISTRO DE OPERACOES & SINTESE     */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawGameText("DIARIO DE PESQUISA // ANO 2026", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawGameText("SALA DE HIDROGENIO // PROCESSO TERMICO", bookX + 32, bookY + 68, 17, (Color){125, 35, 18, 255});
            DrawGameText("Sintese de hardware neural de estado solido (SSD).", bookX + 32, bookY + 92, 15, INK_COLOR);
            DrawGameText("Status dos procedimentos da sala:", bookX + 32, bookY + 116, 16,
                     game->room2026Completed ? (Color){20, 120, 45, 255} : (Color){145, 65, 30, 255});

            const char *h2TitlesFound[4] = {
                "Manual Fisico: Minerais Rochosos Exigidos",
                "Terminal da IA: Recomendacao Incorreta (96%)",
                "Reator Termico: Neodimio Inserido com Sucesso",
                "Artefato: Unidade SSD 2026 Recuperada"
            };
            const char *h2TitlesMissing[4] = {
                "1. Manual Tecnico Fisico (Nao consultado)",
                "2. Terminal A.R.1.3.L. (Avaliacao pendente)",
                "3. Reator Termico (Aguardando carga mineral)",
                "4. Unidade SSD (Compartimento travado)"
            };
            bool h2ItemFound[4] = {
                game->manual2026Read,
                game->terminal2026Open || game->selectedMaterial2026 >= 0 || game->room2026Completed,
                game->room2026Completed,
                game->room2026Completed
            };

            for (int i = 0; i < 4; i++)
            {
                int iy = bookY + 144 + i * 46;
                Rectangle boxR = {(float)(bookX + 32), (float)iy, 448, 38};
                DrawRectangleRounded(boxR, 0.2f, 6, h2ItemFound[i] ? Fade((Color){45, 135, 65, 255}, 0.14f) : Fade((Color){140, 125, 105, 255}, 0.12f));
                DrawRectangleRoundedLinesEx(boxR, 0.2f, 6, 1, h2ItemFound[i] ? (Color){45, 135, 65, 255} : (Color){190, 175, 150, 255});
                DrawGameText(h2ItemFound[i] ? "[OK]" : "[?]", bookX + 44, iy + 10, 16, h2ItemFound[i] ? (Color){25, 120, 45, 255} : (Color){140, 80, 50, 255});
                DrawGameText(h2ItemFound[i] ? h2TitlesFound[i] : h2TitlesMissing[i], bookX + 90, iy + 11, 15, h2ItemFound[i] ? INK_COLOR : (Color){105, 95, 85, 255});
            }

            /* Carimbo confidencial */
            DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 270), (float)(bookY + 368), 210, 46}, 0.2f, 6, 2, (Color){175, 45, 45, 255});
            DrawGameText("ALUCINACAO IA", bookX + 300, bookY + 375, 18, (Color){175, 45, 45, 255});
            DrawGameText("PENSAMENTO CRITICO 2026", bookX + 288, bookY + 396, 12, (Color){175, 45, 45, 255});

            DrawGameText("Consulte o manual tecnico fisico na bancada esquerda.", bookX + 32, bookY + 430, 15, INK_COLOR);
            DrawGameText("Desconfie de recomendacoes com 96% de confianca que", bookX + 32, bookY + 454, 15, INK_COLOR);
            DrawGameText("contradigam a fisica dos materiais e o manual.", bookX + 32, bookY + 478, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawGameText("SINTESE & DEDUCAO DO REATOR // 2026", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle cardR1 = {(float)(bookX + 560), (float)(bookY + 76), 452, 146};
            DrawRectangleRounded(cardR1, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardR1, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawGameText("A ALUCINACAO DA IA GENERATIVA:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawGameText("- A.R.1.3.L. recomenda Gas Hidrogenio com 96% de certeza.", bookX + 576, bookY + 115, 14, INK_COLOR);
            DrawGameText("- Modelos generativos predizem tokens com base estatistica,", bookX + 576, bookY + 138, 14, INK_COLOR);
            DrawGameText("  podendo soar extremamente convincentes mesmo estando errados.", bookX + 576, bookY + 160, 14, INK_COLOR);
            DrawGameText("  'Confianca estatistica nao e sinonimo de verdade factual.'", bookX + 576, bookY + 186, 15, (Color){135, 35, 18, 255});

            Rectangle cardR2 = {(float)(bookX + 560), (float)(bookY + 238), 452, 125};
            DrawRectangleRounded(cardR2, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardR2, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawGameText("REFINO DA LIGA DE ESTADO SOLIDO (SSD):", bookX + 576, bookY + 252, 16, (Color){125, 65, 35, 255});

            if (game->room2026Completed)
            {
                DrawGameText("[ REATOR ESTABILIZADO // 100% ]", bookX + 610, bookY + 280, 22, (Color){25, 125, 50, 255});
                DrawGameText("Minerio de Neodimio fundido com sucesso. SSD liberado!", bookX + 576, bookY + 326, 14, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawGameText("Exige Terras Raras solidas (Neodimio / Disprosio).", bookX + 576, bookY + 280, 15, (Color){165, 45, 35, 255});
                DrawGameText("Proibido: gases ou liquidos na camara termica.", bookX + 576, bookY + 304, 15, INK_COLOR);
                DrawGameText("Alimente o reator com a opcao mineral correta.", bookX + 576, bookY + 328, 14, (Color){135, 55, 40, 255});
            }

            DrawGameText("DICAS DE NAVEGACAO DESTE ARQUIVO:", bookX + 560, bookY + 385, 16, (Color){125, 35, 18, 255});
            DrawGameText("- Clique em 'Proxima [E]' para o Dossie Socioambiental.", bookX + 560, bookY + 412, 15, INK_COLOR);
            DrawGameText("- Conclua a fase para liberar o salto para o ANO 2048.", bookX + 560, bookY + 438, 15, INK_COLOR);
            DrawGameText("- Pressione [D] para retornar a fabrica automatizada.", bookX + 560, bookY + 464, 15, INK_COLOR);
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 2026: DOSSIE SOCIOAMBIENTAL & TERRAS RARAS*/
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawGameText("DOSSIE SOCIOAMBIENTAL // ANO 2026", bookX + 32, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawGameText("IMPACTO MATERIAL DO HARDWARE DE IA", bookX + 32, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 32, bookY + 65, bookX + 482, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle bRaras = {(float)(bookX + 32), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bRaras, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(bRaras, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});

            DrawGameText("2026 // MINERACAO E TERRAS RARAS", bookX + 46, bookY + 92, 17, (Color){125, 35, 18, 255});
            DrawGameText("[CUSTO FISICO DA NUVEM DIGITAL]", bookX + 46, bookY + 116, 14, (Color){25, 125, 50, 255});
            DrawGameText("A Inteligencia Artificial nao existe no vacuo:", bookX + 46, bookY + 140, 15, INK_COLOR);
            DrawGameText("sua infraestrutura fisica requer toneladas de minerais", bookX + 46, bookY + 164, 15, INK_COLOR);
            DrawGameText("raros (Neodimio, Litio, Cobalto) extraidos do solo,", bookX + 46, bookY + 188, 15, INK_COLOR);
            DrawGameText("alem de volumes colossais de agua para resfriamento.", bookX + 46, bookY + 212, 15, INK_COLOR);
            DrawGameText("-> O pensamento critico inclui a consciencia ambiental.", bookX + 46, bookY + 238, 15, (Color){125, 35, 18, 255});

            Rectangle bRefino = {(float)(bookX + 32), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bRefino, 0.08f, 6, game->room2026Completed ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bRefino, 0.08f, 6, 1.5f, game->room2026Completed ? (Color){45, 135, 65, 255} : GRAY);

            if (game->room2026Completed)
            {
                DrawGameText("2026 // SINTESE DO SSD CONCLUIDA", bookX + 46, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // RECOMPENSA DE 2026]", bookX + 46, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawGameText("A liga metalica de alta pureza foi sintetizada.", bookX + 46, bookY + 360, 15, INK_COLOR);
                DrawGameText("A unidade SSD armazena a matriz neural de Ariel.", bookX + 46, bookY + 384, 15, INK_COLOR);
                DrawGameText("Recompensa resgatada com sucesso para a etapa final.", bookX + 46, bookY + 408, 15, (Color){125, 35, 18, 255});
                DrawGameText("-> Salto temporal liberado: ANO 2048!", bookX + 46, bookY + 438, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("2026 // RECOMPENSA SSD [BLOQUEADO]", bookX + 100, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawGameText("Insira o Minerio de Neodimio na camara termica", bookX + 100, bookY + 390, 15, INK_COLOR);
                DrawGameText("para estabilizar a liga e resgatar o artefato SSD.", bookX + 100, bookY + 415, 15, INK_COLOR);
            }

            /* Pagina Direita */
            DrawGameText("ARTEFATOS E PROTOTIPO // 2026", bookX + 560, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawGameText("UNIDADE DE ESTADO SOLIDO (SSD)", bookX + 560, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 560, bookY + 65, bookX + 1008, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            Rectangle bSsdCard = {(float)(bookX + 560), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bSsdCard, 0.08f, 6, game->room2026Completed ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bSsdCard, 0.08f, 6, 1.5f, game->room2026Completed ? (Color){45, 135, 65, 255} : GRAY);

            if (game->room2026Completed)
            {
                DrawGameText("2026 // SSD DAS ESCOLHAS", bookX + 574, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // ARTEFATO DE 2026]", bookX + 574, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawGameText("Guarda os registros do momento em que a IA comecou", bookX + 574, bookY + 140, 15, INK_COLOR);
                DrawGameText("a sugerir e decidir pela humanidade.", bookX + 574, bookY + 164, 15, INK_COLOR);
                DrawGameText("Este artefato completa a trilogia previa para 2048.", bookX + 574, bookY + 188, 15, INK_COLOR);
                DrawGameText("-> Proxima parada: CONFRONTO EM 2048!", bookX + 574, bookY + 214, 15, (Color){125, 35, 18, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("2026 // UNIDADE SSD [BLOQUEADO]", bookX + 630, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawGameText("O SSD esta trancado no compartimento de saida.", bookX + 630, bookY + 165, 15, INK_COLOR);
                DrawGameText("Conclua a sintese no reator termico para libera-lo.", bookX + 630, bookY + 190, 15, INK_COLOR);
            }

            Rectangle bBonus = {(float)(bookX + 560), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bBonus, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(bBonus, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});

            DrawGameText("BONUS DE PENSAMENTO CRITICO", bookX + 574, bookY + 312, 17, (Color){125, 35, 18, 255});
            DrawGameText("[AVALIACAO ETICA DO JOGADOR]", bookX + 574, bookY + 336, 14, (Color){125, 35, 18, 255});
            if (game->room2026Completed)
            {
                if (game->room2026FirstTrySuccess)
                {
                    DrawGameText("ANALISE CRITICA: EXCELENTE!", bookX + 574, bookY + 368, 16, (Color){25, 125, 50, 255});
                    DrawGameText("Elira percebeu a inconsistencia da IA na 1a tentativa.", bookX + 574, bookY + 396, 14, INK_COLOR);
                    DrawGameText("Bonus etico concedido para o confronto de 2048.", bookX + 574, bookY + 422, 14, (Color){25, 125, 50, 255});
                }
                else
                {
                    DrawGameText("ANALISE CRITICA: REVISADA", bookX + 574, bookY + 368, 16, (Color){145, 85, 35, 255});
                    DrawGameText("A falha inicial serviu como licao de verificacao.", bookX + 574, bookY + 396, 14, INK_COLOR);
                }
            }
            else
            {
                DrawGameText("Acerte o mineral na primeira tentativa para", bookX + 574, bookY + 368, 15, INK_COLOR);
                DrawGameText("receber o relatorio de consciencia socioambiental.", bookX + 574, bookY + 394, 15, (Color){135, 35, 18, 255});
            }
        }

        /* Rodapé de navegação de 2026 */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawGameText(TextFormat("Pagina %d de 2 (2026)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
    }
    else
    {
        /* ========================================================= */
        /* ANO 2026 (FASE 3: O PROTOTIPO & TERMINAL DE MEMORIA)      */
        /* ========================================================= */
        int page = game->journalPage;

        if (page == 0)
        {
            /* ----------------------------------------------------- */
            /* PAGINA 1 DE 2026: REGISTRO DE OPERACOES & SINTESE     */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda */
            DrawGameText("DIARIO DE PESQUISA // PROJETO A.R.1.3.L.", bookX + 32, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 32, bookY + 52, bookX + 482, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            DrawGameText("REGISTRO DE OPERACOES - ANO 2026", bookX + 32, bookY + 68, 18, (Color){125, 35, 18, 255});
            DrawGameText("Acesso ao terminal protegido por chave cronologica.", bookX + 32, bookY + 92, 15, INK_COLOR);
            DrawGameText("Pistas no laboratorio:", bookX + 32, bookY + 116, 16,
                     ClueCount(game) == 5 ? (Color){20, 120, 45, 255} : (Color){145, 65, 30, 255});

            /* 5 Caixas de pistas com fonte grande e legivel */
            const char *itemTitlesFound[5] = {
                "Calendario: Marco Fundamental da IA (1950)",
                "Livros: Batismo da IA em Dartmouth (1956)",
                "Gaveta: SSD de Armazenamento (2026)",
                "Esquema: O Perceptron de Rosenblatt (1958)",
                "Gravador: Fita com Voz da Dra. Ramos (2026)"
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
                DrawGameText(itemFound[i] ? "[OK]" : "[?]", bookX + 44, iy + 9, 16, itemFound[i] ? (Color){25, 120, 45, 255} : (Color){140, 80, 50, 255});
                DrawGameText(itemFound[i] ? itemTitlesFound[i] : itemTitlesMissing[i], bookX + 90, iy + 10, 15, itemFound[i] ? INK_COLOR : (Color){105, 95, 85, 255});
            }

            /* Carimbo confidencial vintage */
            DrawRectangleRoundedLinesEx((Rectangle){(float)(bookX + 270), (float)(bookY + 368), 210, 46}, 0.2f, 6, 2, (Color){175, 45, 45, 255});
            DrawGameText("CONFIDENCIAL", bookX + 295, bookY + 375, 18, (Color){175, 45, 45, 255});
            DrawGameText("ARQUIVO HISTORICO 2026", bookX + 288, bookY + 396, 12, (Color){175, 45, 45, 255});

            DrawGameText("Investigue os objetos do laboratorio para preencher", bookX + 32, bookY + 430, 15, INK_COLOR);
            DrawGameText("as folhas do diario com os dados do prototipo de 2026.", bookX + 32, bookY + 454, 15, INK_COLOR);
            DrawGameText("Avance para a Pagina 2 para o dossie completo.", bookX + 32, bookY + 480, 15, (Color){125, 35, 18, 255});

            /* Pagina Direita */
            DrawGameText("SINTESE & DEDUCAO DO TERMINAL", bookX + 560, bookY + 22, 20, (Color){125, 35, 18, 255});
            DrawLine(bookX + 560, bookY + 52, bookX + 1008, bookY + 52, Fade((Color){125, 35, 18, 255}, 0.45f));

            /* Card Como desvendar - perfeitamente enquadrado */
            Rectangle cardR1 = {(float)(bookX + 560), (float)(bookY + 76), 452, 146};
            DrawRectangleRounded(cardR1, 0.08f, 6, Fade((Color){168, 142, 93, 255}, 0.16f));
            DrawRectangleRoundedLinesEx(cardR1, 0.08f, 6, 1.5f, (Color){185, 155, 110, 255});
            DrawGameText("COMO DESVENDAR O CODIGO:", bookX + 576, bookY + 90, 17, (Color){125, 35, 18, 255});
            DrawGameText("- O computador exige uma chave de 4 digitos.", bookX + 576, bookY + 115, 15, INK_COLOR);
            DrawGameText("- Cada pista na sala e um marco historico da IA.", bookX + 576, bookY + 138, 15, INK_COLOR);
            DrawGameText("- Ponto de partida: a questao de Turing:", bookX + 576, bookY + 161, 15, INK_COLOR);
            DrawGameText("  'Podem as maquinas pensar?'", bookX + 576, bookY + 186, 16, (Color){135, 35, 18, 255});

            /* Card Status do Codigo */
            Rectangle cardR2 = {(float)(bookX + 560), (float)(bookY + 238), 452, 125};
            DrawRectangleRounded(cardR2, 0.08f, 6, (Color){238, 232, 218, 255});
            DrawRectangleRoundedLinesEx(cardR2, 0.08f, 6, 1.5f, (Color){175, 155, 125, 255});
            DrawGameText("STATUS DO CODIGO DE ACESSO:", bookX + 576, bookY + 252, 16, (Color){125, 65, 35, 255});

            if (ClueCount(game) == 5)
            {
                DrawGameText("[ 1 ]   [ 9 ]   [ 5 ]   [ 0 ]", bookX + 645, bookY + 280, 28, (Color){25, 125, 50, 255});
                DrawGameText("Chave decifrada! Digite 1950 no computador.", bookX + 576, bookY + 326, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawGameText("[ ? ]   [ ? ]   [ ? ]   [ ? ]", bookX + 645, bookY + 280, 28, (Color){165, 45, 35, 255});
                DrawGameText(TextFormat("Ainda faltam %d pistas para consolidar a deducao.", 5 - ClueCount(game)),
                         bookX + 576, bookY + 326, 15, (Color){135, 55, 40, 255});
            }

            /* Dica de navegacao */
            DrawGameText("DICAS DE NAVEGACAO DESTE ARQUIVO:", bookX + 560, bookY + 385, 16, (Color){125, 35, 18, 255});
            DrawGameText("- Clique em 'Proxima [E]' para acessar a Pagina 2 de 2026.", bookX + 560, bookY + 412, 15, INK_COLOR);
            DrawGameText("- As abas com cadeado pertencem a fases bloqueadas.", bookX + 560, bookY + 438, 15, INK_COLOR);
            DrawGameText("- Pressione [D] para retornar ao laboratorio.", bookX + 560, bookY + 464, 15, INK_COLOR);
        }
        else
        {
            /* ----------------------------------------------------- */
            /* PAGINA 2 DE 2026: DOSSIE HISTORICO & ARTEFATOS        */
            /* ----------------------------------------------------- */
            /* Pagina Esquerda: 1950 Turing & 1956 Dartmouth */
            DrawGameText("DOSSIE HISTORICO // MARCOS DA IA", bookX + 32, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawGameText("DRA. ELIRA RAMOS - NOTAS CIENTIFICAS (2026)", bookX + 32, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 32, bookY + 65, bookX + 482, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            /* Bloco 1950 Turing */
            Rectangle bTuring = {(float)(bookX + 32), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bTuring, 0.08f, 6, game->foundCalendar ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bTuring, 0.08f, 6, 1.5f, game->foundCalendar ? (Color){45, 135, 65, 255} : GRAY);

            if (game->foundCalendar)
            {
                DrawGameText("1950 // ALAN TURING & JOGO DA IMITACAO", bookX + 46, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // ANO 1950]", bookX + 46, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawGameText("Artigo seminal: 'Computing Machinery and Intelligence'.", bookX + 46, bookY + 140, 15, INK_COLOR);
                DrawGameText("Propos substituir a questao filosofica por um teste operacional:", bookX + 46, bookY + 164, 15, INK_COLOR);
                DrawGameText("se um humano nao distinguir a maquina, ela e inteligente.", bookX + 46, bookY + 188, 15, INK_COLOR);
                DrawGameText("-> Marco zero da ciencia computacional de IA: ANO 1950.", bookX + 46, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawGameText("-> Pista fundamental para a chave do terminal!", bookX + 46, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("1950 // MARCO FUNDAMENTAL [BLOQUEADO]", bookX + 100, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawGameText("Examine o calendario preso na parede da sala", bookX + 100, bookY + 165, 15, INK_COLOR);
                DrawGameText("para registrar os detalhes historicos deste marco.", bookX + 100, bookY + 190, 15, INK_COLOR);
            }

            /* Bloco 1956 Dartmouth */
            Rectangle bDartmouth = {(float)(bookX + 32), (float)(bookY + 298), 448, 215};
            DrawRectangleRounded(bDartmouth, 0.08f, 6, game->foundBooks ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bDartmouth, 0.08f, 6, 1.5f, game->foundBooks ? (Color){45, 135, 65, 255} : GRAY);

            if (game->foundBooks)
            {
                DrawGameText("1956 // CONFERENCIA DE DARTMOUTH", bookX + 46, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // ANO 1956]", bookX + 46, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawGameText("Seminario historico de John McCarthy, Minsky e Shannon.", bookX + 46, bookY + 360, 15, INK_COLOR);
                DrawGameText("McCarthy cunhou o termo 'Inteligencia Artificial',", bookX + 46, bookY + 384, 15, (Color){125, 35, 18, 255});
                DrawGameText("afirmando que todo aspecto do aprendizado humano", bookX + 46, bookY + 408, 15, INK_COLOR);
                DrawGameText("pode em principio ser simulado com exatidao por maquinas.", bookX + 46, bookY + 432, 15, INK_COLOR);
                DrawGameText("-> Ano oficial do batismo da disciplina: ANO 1956.", bookX + 46, bookY + 458, 15, (Color){125, 35, 18, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 65, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("1956 // LITERATURA DA IA [BLOQUEADO]", bookX + 100, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawGameText("Investigue a estante de livros na sala para", bookX + 100, bookY + 390, 15, INK_COLOR);
                DrawGameText("desbloquear os registros sobre o batismo da IA.", bookX + 100, bookY + 415, 15, INK_COLOR);
            }

            /* Pagina Direita: 1958 Perceptron & 2026 Midias / Gravacao */
            DrawGameText("ARTEFATOS E PROTOTIPO // 2026", bookX + 560, bookY + 20, 20, (Color){125, 35, 18, 255});
            DrawGameText("REDES NEURAIS, DADOS E VOZ DA CRIADORA", bookX + 560, bookY + 44, 15, (Color){110, 75, 50, 255});
            DrawLine(bookX + 560, bookY + 65, bookX + 1008, bookY + 65, Fade((Color){125, 35, 18, 255}, 0.45f));

            /* Bloco 1958 Perceptron */
            Rectangle bPerceptron = {(float)(bookX + 560), (float)(bookY + 78), 448, 205};
            DrawRectangleRounded(bPerceptron, 0.08f, 6, game->foundBlueprint ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bPerceptron, 0.08f, 6, 1.5f, game->foundBlueprint ? (Color){45, 135, 65, 255} : GRAY);

            if (game->foundBlueprint)
            {
                DrawGameText("1958 // O PERCEPTRON DE ROSENBLATT", bookX + 574, bookY + 92, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // ANO 1958]", bookX + 574, bookY + 116, 14, (Color){25, 125, 50, 255});
                DrawGameText("O primeiro algoritmo de aprendizado supervisionado.", bookX + 574, bookY + 140, 15, INK_COLOR);
                DrawGameText("Neuronio artificial com pesos variaveis (w) que se ajustam", bookX + 574, bookY + 164, 15, INK_COLOR);
                DrawGameText("automaticamente conforme ocorrem erros na classificacao.", bookX + 574, bookY + 188, 15, INK_COLOR);
                DrawGameText("-> O nascimento das Redes Neurais Artificiais: ANO 1958.", bookX + 574, bookY + 214, 15, (Color){125, 35, 18, 255});
                DrawGameText("-> Prova de que maquinas aprendem a partir de dados!", bookX + 574, bookY + 238, 15, (Color){25, 125, 50, 255});
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 175, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("1958 // ESQUEMA DO PERCEPTRON [BLOQUEADO]", bookX + 630, bookY + 135, 17, (Color){125, 65, 35, 255});
                DrawGameText("Examine o esquema tecnico pendurado na parede", bookX + 630, bookY + 165, 15, INK_COLOR);
                DrawGameText("para registrar a arquitetura da rede neural.", bookX + 630, bookY + 190, 15, INK_COLOR);
            }

            /* Bloco 2026 Midias e Gravacao */
            Rectangle bAudio = {(float)(bookX + 560), (float)(bookY + 298), 448, 215};
            bool has2026Media = (game->foundDrawer || game->foundTape);
            DrawRectangleRounded(bAudio, 0.08f, 6, has2026Media ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.12f));
            DrawRectangleRoundedLinesEx(bAudio, 0.08f, 6, 1.5f, has2026Media ? (Color){45, 135, 65, 255} : GRAY);

            if (has2026Media)
            {
                DrawGameText("2026 // DADOS, MEMORIA E GRAVACAO", bookX + 574, bookY + 312, 17, (Color){125, 35, 18, 255});
                DrawGameText("[CONFIRMADO // ARTEFATOS DE 2026]", bookX + 574, bookY + 336, 14, (Color){25, 125, 50, 255});
                DrawGameText("Unidade SSD: armazena os pesos e regras neurais de A.R.1.3.L.", bookX + 574, bookY + 360, 15, INK_COLOR);
                DrawGameText("Gravacao da Dra. Ramos: 'Para despertar o prototipo,", bookX + 574, bookY + 384, 15, (Color){125, 35, 18, 255});
                DrawGameText("o operador deve inserir o ano inaugural do Teste de Turing:'", bookX + 574, bookY + 408, 15, (Color){125, 35, 18, 255});

                /* Destaque com o codigo final */
                Rectangle codeBox = {(float)(bookX + 574), (float)(bookY + 438), 420, 52};
                DrawRectangleRounded(codeBox, 0.2f, 6, (Color){30, 42, 60, 255});
                DrawRectangleRoundedLinesEx(codeBox, 0.2f, 6, 2, GOLD_COLOR);
                DrawGameText("CHAVE FINAL DO TERMINAL: 1 9 5 0", bookX + 610, bookY + 452, 20, GOLD_COLOR);
            }
            else
            {
                DrawPadlockIcon(bookX + 595, bookY + 400, 1.3f, (Color){185, 145, 60, 255}, RAYWHITE);
                DrawGameText("2026 // ARTEFATOS DA SALA [BLOQUEADOS]", bookX + 630, bookY + 360, 17, (Color){125, 65, 35, 255});
                DrawGameText("Abra a gaveta da mesa e ouca a fita no gravador", bookX + 630, bookY + 390, 15, INK_COLOR);
                DrawGameText("para registrar a transcricao da Dra. Ramos.", bookX + 630, bookY + 415, 15, INK_COLOR);
            }
        }

        /* ========================================================= */
        /* BARRA DE NAVEGACAO INFERIOR (PAGINACAO DE 2026: 2 PAGINAS) */
        /* ========================================================= */
        Rectangle prevBtn = {(float)(bookX + 32), (float)footY, 155, 36};
        bool prevHover = CheckCollisionPointRec(GetVirtualMouse(), prevBtn);
        bool canPrev = (page > 0);
        DrawRectangleRounded(prevBtn, 0.25f, 6, canPrev ? (prevHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(prevBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("< Anterior [Q]", bookX + 44, footY + 9, 15, canPrev ? INK_COLOR : GRAY);

        DrawGameText(TextFormat("Pagina %d de 2 (2026)", page + 1), bookX + 210, footY + 8, 18, (Color){125, 35, 18, 255});

        Rectangle closeBtn = {(float)(bookX + 660), (float)footY, 145, 36};
        bool closeHover = CheckCollisionPointRec(GetVirtualMouse(), closeBtn);
        DrawRectangleRounded(closeBtn, 0.25f, 6, closeHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
        DrawRectangleRoundedLinesEx(closeBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Fechar [ESC/D]", bookX + 678, footY + 9, 15, INK_COLOR);

        Rectangle nextBtn = {(float)(bookX + 835), (float)footY, 155, 36};
        bool nextHover = CheckCollisionPointRec(GetVirtualMouse(), nextBtn);
        bool canNext = (page < 1);
        DrawRectangleRounded(nextBtn, 0.25f, 6, canNext ? (nextHover ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255}) : Fade((Color){200, 195, 185, 255}, 0.5f));
        DrawRectangleRoundedLinesEx(nextBtn, 0.25f, 6, 1, (Color){145, 115, 85, 255});
        DrawGameText("Proxima [E] >", bookX + 858, footY + 9, 15, canNext ? INK_COLOR : GRAY);
    }
}

void UpdateRoom(GameState *game)
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

        /* Se estiver em um dos anos ativos (1990, 2008 ou 2026) */
        bool isActiveYearUnlocked = (game->journalYearTab == 0) ||
                                    (game->journalYearTab == 1 && game->year2008Unlocked) ||
                                    (game->journalYearTab == 2 && game->year2026Unlocked);

        if (isActiveYearUnlocked)
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

        if (isActiveYearUnlocked)
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
                int defaultTab = 0;
                if ((game->screen == SCREEN_ROOM || game->screen == SCREEN_ROOM_2026 || game->screen == SCREEN_RESULT_2026) && game->year2026Unlocked) defaultTab = 2;
                else if ((game->screen == SCREEN_ROOM_2008 || game->screen == SCREEN_PUZZLE_2008 || game->screen == SCREEN_RESULT_2008) && game->year2008Unlocked) defaultTab = 1;
                game->journalYearTab = defaultTab;
                game->journalPage = 0;
            }
        }

        if (Clicked(closeBtn))
        {
            game->journalOpen = false;
        }

        /* Clique nas abas laterais a direita (1990, 2008, 2026) */
        int tabX = bookX + 1040;
        int tabGap = 66;
        for (int i = 0; i < 3; i++)
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

    Rectangle btnBack2008 = {70, 600, 240, 42};
    if (Clicked(btnBack2008))
    {
        game->journalYearTab = 1;
        game->screen = SCREEN_ROOM_2008;
        return;
    }
    if (Clicked(calendar))
    {
        game->foundCalendar = true;
        game->journalYearTab = 2;
        game->journalPage = 1;
        SetMessage(game, "Pista: o Teste de Turing foi proposto em 1950.");
    }
    else if (Clicked(books))
    {
        game->foundBooks = true;
        game->journalYearTab = 2;
        game->journalPage = 1;
        SetMessage(game, "Registro: a IA recebeu seu nome em 1956.");
    }
    else if (Clicked(drawer))
    {
        game->foundDrawer = true;
        game->journalYearTab = 2;
        game->journalPage = 1;
        SetMessage(game, "A gaveta contem a unidade SSD com dados do prototipo de 2026.");
    }
    else if (Clicked(blueprint))
    {
        game->foundBlueprint = true;
        game->journalYearTab = 2;
        game->journalPage = 1;
        SetMessage(game, "Esquema: o Perceptron foi apresentado em 1958.");
    }
    else if (Clicked(tape))
    {
        game->foundTape = true;
        game->journalYearTab = 2;
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

void AddDigit(GameState *game, char digit)
{
    if (game->codeLength >= 4) return;
    game->code[game->codeLength++] = digit;
    game->code[game->codeLength] = '\0';
}

void CheckCode(GameState *game)
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

void DrawTerminal(const GameState *game)
{
    static const char *labels[12] = {"1", "2", "3", "4", "5", "6",
                                      "7", "8", "9", "LIMPAR", "0", "OK"};
    DrawBackground();
    DrawGameText("A.R.1.3.L // TERMINAL DE MEMORIA", 60, 42, 22, CYAN_COLOR);
    DrawGameText(TextFormat("VIDAS RESTANTES: %d", game->lives), 980, 42, 20,
             game->lives == 1 ? RED_COLOR : RAYWHITE);
    DrawRectangleRounded((Rectangle){90, 100, 650, 520}, 0.035f, 10,
                         (Color){14, 27, 26, 255});
    DrawRectangleRoundedLinesEx((Rectangle){90, 100, 650, 520}, 0.035f, 10, 3,
                                (Color){68, 115, 88, 255});
    DrawGameText("> BOOT MEMORY_NODE_2026", 130, 145, 20,
             (Color){101, 232, 151, 255});
    DrawGameText("> SECURITY PROTOCOL ACTIVE", 130, 185, 20,
             (Color){101, 232, 151, 255});
    DrawGameText("> QUESTION:", 130, 250, 20, (Color){101, 232, 151, 255});
    DrawGameText("Em que ano uma maquina", 130, 292, 24, RAYWHITE);
    DrawGameText("foi desafiada a pensar?", 130, 330, 24, RAYWHITE);
    DrawGameText("CODIGO DE ACESSO", 130, 405, 18, GOLD_COLOR);

    for (int i = 0; i < 4; i++)
    {
        Rectangle slot = {130.0f + i * 88.0f, 445, 68, 72};
        char value[2] = {'_', '\0'};
        if (i < game->codeLength) value[0] = game->code[i];
        DrawRectangleRounded(slot, 0.12f, 6, PANEL_COLOR);
        DrawRectangleRoundedLinesEx(slot, 0.12f, 6, 2, CYAN_COLOR);
        DrawGameText(value, (int)slot.x + 24, (int)slot.y + 17, 34, RAYWHITE);
    }
    DrawGameText("Use o teclado ou o painel", 130, 558, 17, GRAY);
    for (int i = 0; i < 12; i++)
    {
        int row = i / 3;
        int column = i % 3;
        Rectangle key = {815.0f + column * 120.0f, 145.0f + row * 100.0f,
                         95, 72};
        DrawButton(key, labels[i], i == 11 ? GOLD_COLOR : CYAN_COLOR);
    }
    if (game->messageTimer > 0)
        DrawGameText(game->message, 815, 555, 17, RED_COLOR);
    DrawButton((Rectangle){890, 600, 240, 50}, "VOLTAR A SALA", RED_COLOR);
}

void UpdateTerminal(GameState *game)
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

void DrawFloppy(Vector2 position, float scale)
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
    DrawGameText("ARIEL", (int)(position.x + 61 * scale),
             (int)(position.y + 121 * scale), (int)(22 * scale), INK_COLOR);
}

void DrawSsd(Vector2 position, float scale)
{
    float w = 224.0f * scale;
    float h = 154.0f * scale;
    float time = (float)GetTime();

    /* 1. Sombra de contato */
    DrawRectangleRounded((Rectangle){position.x + 8 * scale, position.y + 10 * scale, w, h}, 0.08f, 6, Fade(BLACK, 0.45f));

    /* 2. Conectores SATA na lateral esquerda */
    float connW = 14.0f * scale;
    DrawRectangle((int)(position.x - connW + 2), (int)(position.y + 24 * scale), (int)connW, (int)(42 * scale), (Color){20, 24, 30, 255});
    DrawRectangle((int)(position.x - connW + 2), (int)(position.y + 74 * scale), (int)connW, (int)(28 * scale), (Color){20, 24, 30, 255});
    for (int p = 0; p < 5; p++)
    {
        DrawRectangle((int)(position.x - connW + 4), (int)(position.y + (28 + p * 7) * scale), (int)(connW - 4), (int)(3 * scale), (Color){235, 185, 55, 255});
    }
    for (int p = 0; p < 3; p++)
    {
        DrawRectangle((int)(position.x - connW + 4), (int)(position.y + (78 + p * 8) * scale), (int)(connW - 4), (int)(3 * scale), (Color){235, 185, 55, 255});
    }

    /* 3. Chassi principal em liga metalica escura */
    Rectangle outerRec = {position.x, position.y, w, h};
    DrawRectangleRounded(outerRec, 0.08f, 8, (Color){32, 40, 52, 255});
    DrawRectangleRoundedLinesEx(outerRec, 0.08f, 8, 2.0f * scale, (Color){60, 75, 96, 255});

    /* 4. Tampa frontal em aluminio escovado */
    Rectangle plateRec = {position.x + 12 * scale, position.y + 8 * scale, w - 24 * scale, h - 16 * scale};
    DrawRectangleGradientH((int)plateRec.x, (int)plateRec.y, (int)plateRec.width, (int)plateRec.height,
                           (Color){215, 224, 238, 255}, (Color){170, 180, 196, 255});
    DrawRectangleRoundedLinesEx(plateRec, 0.05f, 6, 1.5f * scale, (Color){240, 245, 255, 255});

    /* Chanfro de brilho no topo */
    DrawLineEx((Vector2){plateRec.x + 4 * scale, plateRec.y + 2 * scale},
               (Vector2){plateRec.x + plateRec.width - 4 * scale, plateRec.y + 2 * scale},
               1.5f * scale, Fade(RAYWHITE, 0.75f));

    /* Parafusos nas quinas */
    Vector2 screws[4] = {
        {plateRec.x + 12 * scale, plateRec.y + 12 * scale},
        {plateRec.x + plateRec.width - 12 * scale, plateRec.y + 12 * scale},
        {plateRec.x + 12 * scale, plateRec.y + plateRec.height - 12 * scale},
        {plateRec.x + plateRec.width - 12 * scale, plateRec.y + plateRec.height - 12 * scale}
    };
    for (int s = 0; s < 4; s++)
    {
        DrawCircle((int)screws[s].x, (int)screws[s].y, 3.5f * scale, (Color){45, 52, 64, 255});
        DrawCircle((int)screws[s].x, (int)screws[s].y, 1.8f * scale, (Color){120, 130, 145, 255});
    }

    /* 5. Painel chanfrado do microcontrolador neural */
    Rectangle chipBay = {plateRec.x + plateRec.width - 92 * scale, plateRec.y + 36 * scale, 78 * scale, 66 * scale};
    DrawRectangleRounded(chipBay, 0.12f, 6, (Color){15, 22, 36, 255});
    DrawRectangleRoundedLinesEx(chipBay, 0.12f, 6, 1.5f * scale, (Color){48, 68, 92, 255});

    /* Trilhas de circuito impresso */
    DrawLineEx((Vector2){chipBay.x + 10 * scale, chipBay.y + 12 * scale}, (Vector2){chipBay.x + 24 * scale, chipBay.y + 12 * scale}, 1.0f * scale, Fade(CYAN_COLOR, 0.40f));
    DrawLineEx((Vector2){chipBay.x + 10 * scale, chipBay.y + 54 * scale}, (Vector2){chipBay.x + 24 * scale, chipBay.y + 54 * scale}, 1.0f * scale, Fade(CYAN_COLOR, 0.40f));
    DrawLineEx((Vector2){chipBay.x + 54 * scale, chipBay.y + 12 * scale}, (Vector2){chipBay.x + 68 * scale, chipBay.y + 12 * scale}, 1.0f * scale, Fade(CYAN_COLOR, 0.40f));

    /* Microchip central com clock pulsante */
    float chipPulse = 0.70f + 0.30f * sinf(time * 3.5f);
    Rectangle chipRec = {chipBay.x + 26 * scale, chipBay.y + 20 * scale, 26 * scale, 26 * scale};
    DrawRectangleRounded(chipRec, 0.15f, 4, (Color){28, 95, 160, 255});
    DrawRectangleRounded(chipRec, 0.15f, 4, Fade(CYAN_COLOR, chipPulse * 0.45f));
    DrawRectangleRoundedLinesEx(chipRec, 0.15f, 4, 1.2f * scale, CYAN_COLOR);

    for (int p = 0; p < 3; p++)
    {
        DrawRectangle((int)(chipRec.x - 3 * scale), (int)(chipRec.y + (5 + p * 7) * scale), (int)(3 * scale), (int)(2 * scale), RAYWHITE);
        DrawRectangle((int)(chipRec.x + chipRec.width), (int)(chipRec.y + (5 + p * 7) * scale), (int)(3 * scale), (int)(2 * scale), RAYWHITE);
        DrawRectangle((int)(chipRec.x + (5 + p * 7) * scale), (int)(chipRec.y - 3 * scale), (int)(2 * scale), (int)(3 * scale), RAYWHITE);
        DrawRectangle((int)(chipRec.x + (5 + p * 7) * scale), (int)(chipRec.y + chipRec.height), (int)(2 * scale), (int)(3 * scale), RAYWHITE);
    }

    /* LED indicador de atividade (laranja/ambar) */
    Rectangle ledRec = {chipBay.x + chipBay.width - 15 * scale, chipBay.y + chipBay.height - 15 * scale, 8 * scale, 8 * scale};
    DrawRectangleRounded(ledRec, 0.3f, 4, (Color){245, 140, 25, 255});
    DrawCircle((int)(ledRec.x + 4 * scale), (int)(ledRec.y + 4 * scale), 2.5f * scale, Fade(GOLD_COLOR, 0.85f));

    /* 6. Rotulos a laser e grafismos high-tech */
    DrawGameText("A.R.1.3.L.", (int)(plateRec.x + 22 * scale), (int)(plateRec.y + 24 * scale), (int)(16 * scale), (Color){35, 48, 65, 255});
    DrawGameText("SOLID STATE DRIVE", (int)(plateRec.x + 22 * scale), (int)(plateRec.y + 43 * scale), (int)(9 * scale), (Color){75, 88, 105, 255});
    DrawGameText("NEURAL CORE // 2026", (int)(plateRec.x + 22 * scale), (int)(plateRec.y + 58 * scale), (int)(10 * scale), (Color){24, 75, 140, 255});
    DrawGameText("NVMe Gen5 - 2048 TB", (int)(plateRec.x + 22 * scale), (int)(plateRec.y + 74 * scale), (int)(8 * scale), (Color){90, 102, 118, 255});

    Rectangle strip = {plateRec.x + 22 * scale, plateRec.y + 92 * scale, 58 * scale, 5 * scale};
    DrawRectangleRec(strip, (Color){40, 145, 215, 255});
    DrawRectangle((int)strip.x, (int)strip.y, (int)(strip.width * chipPulse), (int)strip.height, CYAN_COLOR);

    DrawLineEx((Vector2){plateRec.x + 22 * scale, plateRec.y + 106 * scale},
               (Vector2){plateRec.x + 86 * scale, plateRec.y + 106 * scale},
               1.0f * scale, Fade((Color){110, 122, 140, 255}, 0.5f));
}

void DrawResult(const GameState *game)
{
    DrawBackground();
    DrawGameText("ARTEFATO RECUPERADO", 90, 68, 20, CYAN_COLOR);
    DrawGameText("SSD // 2026", 90, 108, 46, RAYWHITE);
    DrawRewardSpriteItem(2026, (Vector2){285, 365}, 285.0f);
    DrawRectangleRounded((Rectangle){505, 205, 670, 320}, 0.04f, 8,
                         Fade(PANEL_COLOR, 0.96f));
    DrawGameText("PROTOTIPO DE A.R.1.3.L. DESBLOQUEADO // SALA 2026", 545, 240, 20, GOLD_COLOR);
    DrawGameText("A unidade SSD de 2026 contem os pesos neurais finais.", 545,
             285, 19, LIGHTGRAY);
    DrawGameText("Combinando o Disquete de 1990, o CD de 2008 e o SSD de 2026,", 545,
             318, 19, LIGHTGRAY);
    DrawGameText("Elira reconstituiu a mente e os limites originais da IA.", 545, 351,
             19, LIGHTGRAY);
    DrawGameText("Resta um ultimo passo: confrontar a propria Ariel.", 545, 384,
             19, (Color){100, 235, 140, 255});
    DrawGameText(TextFormat("TEMPO: %02d:%02d", (int)game->elapsedTime / 60,
                        (int)game->elapsedTime % 60),
             545, 435, 22, CYAN_COLOR);
    DrawGameText(TextFormat("VIDAS: %d/3", game->lives), 800, 435, 22,
             game->lives == 3 ? GOLD_COLOR : RAYWHITE);
    DrawGameText(TextFormat("PERFIL: %s", game->playerName), 545, 480, 17, GRAY);
    DrawButton((Rectangle){800, 466, 350, 48}, "CONFRONTAR ARIEL >> [ENTER]", GOLD_COLOR);
    Rectangle btnBack2008 = {390, 588, 230, 56};
    Rectangle btnReplay2026 = {640, 588, 260, 56};
    Rectangle btnMenu = {920, 588, 220, 56};
    DrawButton(btnBack2008, "< SALA 2 (2008)", (Color){70, 95, 110, 255});
    DrawButton(btnReplay2026, "REINICIAR SALA 2026", GOLD_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

void DrawFailure(void)
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

void DrawTapeCassette(Vector2 position, float scale)
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
    DrawGameText("DADOS // ARQUIVO-1990", (int)(label.x + 14 * scale), (int)(label.y + 10 * scale), (int)(14 * scale), INK_COLOR);
    DrawGameText("MATRIZ DE TREINO A.R.1.3.L", (int)(label.x + 14 * scale), (int)(label.y + 30 * scale), (int)(11 * scale), (Color){125, 45, 30, 255});

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

void DrawRoom2(const GameState *game)
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
            DrawGameText("1990", (int)gaveta.x + 24, (int)gaveta.y + 18, 10, INK_COLOR);

            /* Puxador metálico cromado */
            DrawRectangleRounded((Rectangle){gaveta.x + 18, gaveta.y + 44, 46, 12}, 0.4f, 4, (Color){185, 195, 205, 255});
            DrawRectangle((int)gaveta.x + 22, (int)gaveta.y + 47, 38, 6, (Color){30, 36, 40, 255});
        }
    }
    DrawGameText("FICHARIO CENTRAL // 1990", 112, 552, 13, (Color){160, 185, 200, 255});

    /* Caixas de arquivo no chao em frente */
    Rectangle boxDisks = {370, 538, 80, 54};
    DrawRectangleRounded(boxDisks, 0.04f, 4, (Color){145, 115, 80, 255});
    DrawRectangleRoundedLinesEx(boxDisks, 0.04f, 4, 1.5f, (Color){115, 88, 55, 255});
    DrawLine(boxDisks.x + 8, boxDisks.y + 16, boxDisks.x + boxDisks.width - 8, boxDisks.y + 16, Fade((Color){85, 60, 35, 255}, 0.5f));
    DrawGameText("DISQUETES", (int)boxDisks.x + 8, (int)boxDisks.y + 24, 11, INK_COLOR);

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
    Rectangle lockPanel = {500, 345, 126, 76};
    DrawRectangleRounded(lockPanel, 0.12f, 6, (Color){24, 30, 36, 255});
    DrawRectangleRoundedLinesEx(lockPanel, 0.12f, 6, 1.5f, (Color){65, 80, 92, 255});

    if (game->puzzleFilesCompleted)
    {
        /* Status liberado (Verde) */
        float bGlow = 0.6f + 0.4f * sinf(time * 4.0f);
        DrawCircle(516, 368, 5, Fade((Color){50, 240, 100, 255}, bGlow));
        DrawGameText("LIBERADO", 528, 361, 13, (Color){50, 240, 100, 255});
        DrawGameText("[ ENTRAR ]", 528, 388, 12, GOLD_COLOR);
    }
    else
    {
        /* Status travado (Vermelho) */
        float rGlow = 0.6f + 0.4f * sinf(time * 5.0f);
        DrawCircle(516, 368, 5, Fade(RED_COLOR, rGlow));
        DrawGameText("TRAVADA", 530, 361, 13, RED_COLOR);
        DrawGameText("REQUER INDEXACAO", 508, 388, 10, (Color){245, 185, 75, 255});
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

    DrawGameText("TERMINAL DE DADOS // 1990", 850, 278, 14, crtText);
    DrawLine(845, 298, 1075, 298, Fade(crtText, 0.5f));

    if (game->puzzleFilesCompleted)
    {
        DrawGameText("[OK] DADOS SINCRONIZADOS", 852, 316, 14, crtText);
        DrawGameText("Todos os 4 arquivos", 852, 340, 13, RAYWHITE);
        DrawGameText("conectados as pastas!", 852, 362, 13, RAYWHITE);
        DrawGameText("> TRAVA DA PORTA ABERTA <", 852, 392, 13, (Color){100, 245, 150, 255});
    }
    else if (!game->memory1990Completed)
    {
        DrawGameText("[!] ACESSO PROTEGIDO", 852, 316, 14, GOLD_COLOR);
        DrawGameText("Autenticacao de memoria", 852, 340, 13, RAYWHITE);
        DrawGameText("exigida na inicializacao.", 852, 362, 13, RAYWHITE);
        float pBlink = 0.5f + 0.5f * sinf(time * 5.0f);
        DrawGameText("> CLIQUE PARA DESBLOQUEAR <", 844, 392, 13, Fade(CYAN_COLOR, pBlink));
    }
    else
    {
        DrawGameText("[!] INDEXACAO PENDENTE", 852, 316, 14, GOLD_COLOR);
        DrawGameText("Arraste os arquivos para", 852, 340, 13, RAYWHITE);
        DrawGameText("as pastas correspondentes.", 852, 362, 13, RAYWHITE);
        float pBlink = 0.5f + 0.5f * sinf(time * 5.0f);
        DrawGameText("> CLIQUE PARA ORGANIZAR <", 848, 392, 13, Fade(CYAN_COLOR, pBlink));
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
    DrawHotspot(terminal, game->puzzleFilesCompleted ? "Terminal (Indexacao 100% OK)" : (!game->memory1990Completed ? "Acessar terminal [DESBLOQUEAR ACESSO]" : "Acessar sistema de arquivos [PUZZLE]"));
    DrawHotspot(porta, game->puzzleFilesCompleted ? "Acessar porta de saida [ENTRAR]" : "Porta trancada (requer indexacao)");
    if (game->year2008Unlocked)
    {
        DrawHotspot(btnVoltar, "Avancar para o escritorio de 2008");
    }
}

void DrawHudRoom2(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawGameText("ARQUIVO 01 // 1990", 28, 24, 20, CYAN_COLOR);
    DrawGameText("// SETOR DE DADOS", 232, 26, 16, (Color){175, 205, 220, 255});
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 440, 26, 16, (Color){160, 175, 190, 255});

    /* Status da indexação */
    if (game->puzzleFilesCompleted)
    {
        DrawGameText("[OK] INDEXACAO CONCLUIDA", 665, 26, 16, (Color){80, 235, 120, 255});
    }
    else if (!game->memory1990Completed)
    {
        DrawGameText("[!] TERMINAL BLOQUEADO", 665, 26, 16, GOLD_COLOR);
    }
    else
    {
        DrawGameText("[!] INDEXACAO PENDENTE", 665, 26, 16, GOLD_COLOR);
    }

    DrawLivesIcons((Vector2){ 960, 22 }, game->lives, 24.0f);
    DrawGameText(TextFormat("%02d:%02d", minutes, seconds), 1160, 23, 22, GOLD_COLOR);

    /* Rodapé */
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawGameText(game->memory1990Completed
                 ? "Organize os arquivos por cor no terminal para destravar a porta."
                 : "Desbloqueie o terminal de pesquisa e organize os arquivos do sistema.",
                 28, 675, 16, (Color){165, 180, 195, 255});

    /* Botão de Áudio */
    Rectangle audioBtn = {840, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawGameText(game->musicMuted ? "[M] AUDIO: MUDO" : "[M] AUDIO: ON",
             862, 676, 14, game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    /* Botão do Diário */
    DrawJournalHudButton(game);

    if (game->messageTimer > 0)
    {
        int width = MeasureGameText(game->message, 19) + 34;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 605, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawGameText(game->message,
                 (SCREEN_WIDTH - MeasureGameText(game->message, 19)) / 2,
                 614, 19, RAYWHITE);
    }
}

void UpdateRoom2(GameState *game)
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
        for (int i = 0; i < 3; i++)
        {
            Rectangle tabRect = {(float)(bookX + 1040), (float)(bookY + 42 + i * 66), 86, 54};
            if (Clicked(tabRect)) { game->journalYearTab = i; break; }
        }
        return;
    }

    if (Clicked(terminal))
    {
        if (!game->memory1990Completed)
        {
            InitMemory1990(game);
            game->screen = SCREEN_MEMORY_1990;
        }
        else if (!game->puzzleFilesCompleted)
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

/* ========================================================================= */
/* ETAPA 1 (1990): MINIGAME DE MEMORIA (SISTEMA DE ACESSO INICIAL)           */
/* ========================================================================= */

void DrawMemoryProgressDots(int startX, int y, int completedStages, int activeRound, bool isError)
{
    DrawGameText("[", startX, y - 9, 20, LIGHTGRAY);
    for (int s = 0; s < 5; s++)
    {
        int cx = startX + 24 + s * 28;
        int cy = y;
        if (s < completedStages)
        {
            DrawCircle(cx, cy, 7.0f, (Color){60, 235, 110, 255});
            DrawCircleLines(cx, cy, 7.0f, RAYWHITE);
        }
        else if (isError && s == activeRound - 1)
        {
            DrawCircle(cx, cy, 7.0f, RED_COLOR);
            DrawCircleLines(cx, cy, 7.0f, RAYWHITE);
        }
        else if (s == activeRound - 1)
        {
            DrawCircle(cx, cy, 7.0f, Fade(GOLD_COLOR, 0.35f));
            DrawCircleLines(cx, cy, 7.0f, GOLD_COLOR);
        }
        else
        {
            DrawCircleLines(cx, cy, 7.0f, (Color){110, 130, 145, 255});
        }
    }
    DrawGameText("]", startX + 24 + 5 * 28 - 8, y - 9, 20, LIGHTGRAY);
}

void DrawMemory1990(const GameState *game)
{
    float time = (float)GetTime();
    Vector2 mouse = GetVirtualMouse();

    /* Escurecimento do fundo */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.88f));

    /* Painel Principal do Terminal 1990 */
    Rectangle panel = {190, 45, 900, 630};
    DrawRectangleRounded(panel, 0.03f, 8, (Color){20, 27, 34, 255});
    DrawRectangleRoundedLinesEx(panel, 0.03f, 8, 3.0f, (Color){75, 95, 110, 255});

    /* Parafusos metalicos nos cantos */
    DrawCircle((int)panel.x + 16, (int)panel.y + 16, 5, (Color){185, 195, 205, 255});
    DrawCircle((int)(panel.x + panel.width - 16), (int)panel.y + 16, 5, (Color){185, 195, 205, 255});
    DrawCircle((int)panel.x + 16, (int)(panel.y + panel.height - 16), 5, (Color){185, 195, 205, 255});
    DrawCircle((int)(panel.x + panel.width - 16), (int)(panel.y + panel.height - 16), 5, (Color){185, 195, 205, 255});

    /* Barra superior do painel */
    Rectangle topBar = {220, 62, 720, 42};
    DrawRectangleRounded(topBar, 0.16f, 6, (Color){12, 18, 24, 255});
    DrawRectangleRoundedLinesEx(topBar, 0.16f, 6, 1.5f, (Color){55, 78, 95, 255});
    DrawGameText("TERMINAL DE PESQUISA — 1990", 238, 74, 18, CYAN_COLOR);

    int completedStages = (game->memoryState == 4) ? 5 : ((game->memoryState == 2) ? game->memoryRound : (game->memoryRound - 1));
    DrawGameText("PROGRESSO:", 625, 75, 15, LIGHTGRAY);
    DrawMemoryProgressDots(730, 83, completedStages, game->memoryRound, game->memoryState == 3);

    /* Botao sair [X] */
    Rectangle btnClose = {960, 63, 110, 38};
    DrawButton(btnClose, "SAIR [X]", RED_COLOR);

    /* ===================================================================== */
    /* LADO ESQUERDO: VISOR / MONITOR DE SEQUENCIA DE INICIALIZACAO          */
    /* ===================================================================== */
    Rectangle leftMonitor = {220, 120, 405, 485};
    DrawRectangleRounded(leftMonitor, 0.04f, 8, (Color){14, 24, 20, 255});
    Color monBorder = (game->memoryState == 3) ? RED_COLOR :
                      ((game->memoryState == 2 || game->memoryState == 4) ? (Color){60, 235, 110, 255} : (Color){68, 135, 105, 255});
    DrawRectangleRoundedLinesEx(leftMonitor, 0.04f, 8, 2.5f, monBorder);

    /* Cabecalho narrativo do visor CRT */
    DrawGameText("TERMINAL DE PESQUISA — 1990", (int)leftMonitor.x + 22, (int)leftMonitor.y + 18, 16, (Color){95, 235, 155, 255});
    DrawGameText("SISTEMA DE ACESSO INICIAL", (int)leftMonitor.x + 22, (int)leftMonitor.y + 42, 18, GOLD_COLOR);
    DrawLine((int)leftMonitor.x + 22, (int)leftMonitor.y + 68, (int)(leftMonitor.x + leftMonitor.width - 22), (int)leftMonitor.y + 68, Fade((Color){95, 235, 155, 255}, 0.35f));

    DrawGameText("Para acessar o banco de dados,", (int)leftMonitor.x + 22, (int)leftMonitor.y + 78, 15, RAYWHITE);
    DrawGameText("repita corretamente a sequência", (int)leftMonitor.x + 22, (int)leftMonitor.y + 98, 15, RAYWHITE);
    DrawGameText("de inicialização do sistema.", (int)leftMonitor.x + 22, (int)leftMonitor.y + 118, 15, RAYWHITE);

    /* Sub-visor 3x3 de demonstracao da sequencia */
    Rectangle visorBox = {leftMonitor.x + 68, leftMonitor.y + 148, 270, 228};
    DrawRectangleRounded(visorBox, 0.06f, 6, (Color){8, 15, 13, 255});
    DrawRectangleRoundedLinesEx(visorBox, 0.06f, 6, 1.5f, (Color){45, 95, 75, 255});
    DrawGameText(TextFormat("VISOR DE SEQUÊNCIA // ETAPA %d/5", game->memoryRound), (int)visorBox.x + 22, (int)visorBox.y + 10, 13, CYAN_COLOR);

    int activeShowPos = (game->memoryState == 0 && game->memoryLightOn && game->memoryShowIndex < game->memoryRound)
                        ? game->memorySequence[game->memoryShowIndex]
                        : -1;

    for (int idx = 0; idx < 9; idx++)
    {
        int r = idx / 3;
        int c = idx % 3;
        Rectangle cell = {visorBox.x + 42 + c * 66, visorBox.y + 36 + r * 60, 54, 50};
        bool lit = (activeShowPos == idx);
        bool completedFlash = (game->memoryState == 2 || game->memoryState == 4);
        bool errorFlash = (game->memoryState == 3);

        Color cellBg = (Color){18, 32, 28, 255};
        Color cellBorder = (Color){48, 88, 72, 255};
        if (lit)
        {
            cellBg = CYAN_COLOR;
            cellBorder = RAYWHITE;
        }
        else if (completedFlash)
        {
            cellBg = Fade((Color){60, 235, 110, 255}, 0.35f);
            cellBorder = (Color){60, 235, 110, 255};
        }
        else if (errorFlash)
        {
            cellBg = Fade(RED_COLOR, 0.30f);
            cellBorder = RED_COLOR;
        }

        DrawRectangleRounded(cell, 0.18f, 6, cellBg);
        DrawRectangleRoundedLinesEx(cell, 0.18f, 6, lit ? 2.5f : 1.5f, cellBorder);
        if (lit)
        {
            DrawCircle((int)(cell.x + cell.width * 0.5f), (int)(cell.y + cell.height * 0.5f), 10, RAYWHITE);
        }
    }

    /* Caixa de feedback no rodape do monitor esquerdo */
    Rectangle statusBox = {leftMonitor.x + 20, leftMonitor.y + 390, leftMonitor.width - 40, 78};
    DrawRectangleRounded(statusBox, 0.10f, 6, (Color){10, 18, 16, 255});
    DrawRectangleRoundedLinesEx(statusBox, 0.10f, 6, 1.5f, monBorder);

    if (game->memoryState == 0)
    {
        float pulse = 0.55f + 0.45f * sinf(time * 6.0f);
        DrawGameText("DEMONSTRANDO SEQUÊNCIA...", (int)statusBox.x + 18, (int)statusBox.y + 16, 16, Fade(GOLD_COLOR, pulse));
        DrawGameText("Observe os pontos acesos no visor.", (int)statusBox.x + 18, (int)statusBox.y + 44, 14, LIGHTGRAY);
    }
    else if (game->memoryState == 1)
    {
        DrawGameText("AGUARDANDO REPETIÇÃO DO JOGADOR", (int)statusBox.x + 18, (int)statusBox.y + 16, 16, CYAN_COLOR);
        DrawGameText(TextFormat("Clique na grade à direita (%d / %d).", game->memoryInputIndex, game->memoryRound),
                     (int)statusBox.x + 18, (int)statusBox.y + 44, 14, RAYWHITE);
    }
    else if (game->memoryState == 2)
    {
        DrawGameText(TextFormat("ETAPA %d/5 VALIDADA", game->memoryRound), (int)statusBox.x + 18, (int)statusBox.y + 16, 18, (Color){60, 235, 110, 255});
        DrawGameText("Sincronizando próxima camada de memória...", (int)statusBox.x + 18, (int)statusBox.y + 44, 14, RAYWHITE);
    }
    else if (game->memoryState == 3)
    {
        DrawGameText("ERRO DE SEQUÊNCIA", (int)statusBox.x + 18, (int)statusBox.y + 16, 18, RED_COLOR);
        DrawGameText("REINICIANDO VERIFICAÇÃO...", (int)statusBox.x + 18, (int)statusBox.y + 44, 15, GOLD_COLOR);
    }
    else if (game->memoryState == 4)
    {
        DrawGameText("ACESSO AUTORIZADO", (int)statusBox.x + 18, (int)statusBox.y + 16, 19, (Color){60, 235, 110, 255});
        DrawGameText("Carregando sistema de arquivos...", (int)statusBox.x + 18, (int)statusBox.y + 44, 15, RAYWHITE);
    }

    /* ===================================================================== */
    /* LADO DIREITO: GRADE INTERATIVA 3x3 DO JOGADOR                         */
    /* ===================================================================== */
    Rectangle rightPanel = {645, 120, 415, 485};
    DrawRectangleRounded(rightPanel, 0.04f, 8, (Color){16, 22, 30, 255});
    DrawRectangleRoundedLinesEx(rightPanel, 0.04f, 8, 2.0f,
                                (game->memoryState == 1) ? CYAN_COLOR : (Color){55, 72, 88, 255});

    DrawGameText("MATRIZ DE ENTRADA // GRADE 3x3", (int)rightPanel.x + 24, (int)rightPanel.y + 18, 17, RAYWHITE);
    if (game->memoryState == 1)
    {
        DrawGameText("STATUS: ENTRADA LIBERADA — SUA VEZ!", (int)rightPanel.x + 24, (int)rightPanel.y + 44, 14, (Color){60, 235, 110, 255});
    }
    else if (game->memoryState == 0)
    {
        DrawGameText("STATUS: BLOQUEADO DURANTE DEMONSTRAÇÃO", (int)rightPanel.x + 24, (int)rightPanel.y + 44, 14, GOLD_COLOR);
    }
    else
    {
        DrawGameText("STATUS: PROCESSANDO VERIFICAÇÃO...", (int)rightPanel.x + 24, (int)rightPanel.y + 44, 14, LIGHTGRAY);
    }

    /* Indicadores de progresso no topo da grade direita */
    DrawMemoryProgressDots((int)rightPanel.x + 118, (int)rightPanel.y + 82, completedStages, game->memoryRound, game->memoryState == 3);

    /* Grade 3x3 de botoes interativos */
    bool canInput = (game->memoryState == 1);
    for (int idx = 0; idx < 9; idx++)
    {
        int r = idx / 3;
        int c = idx % 3;
        Rectangle btn = {rightPanel.x + 46 + c * 112, rightPanel.y + 112 + r * 106, 98, 92};
        bool hover = canInput && CheckCollisionPointRec(mouse, btn);
        bool clickedFlash = (game->memoryLastClicked == idx && game->memoryClickTimer > 0.0f);
        bool demoHighlight = (activeShowPos == idx);

        Color btnBg = (Color){28, 38, 50, 255};
        Color btnBorder = (Color){70, 92, 112, 255};

        if (clickedFlash)
        {
            btnBg = (game->memoryState == 3) ? RED_COLOR : (Color){60, 235, 110, 255};
            btnBorder = RAYWHITE;
        }
        else if (demoHighlight)
        {
            btnBg = Fade(CYAN_COLOR, 0.32f);
            btnBorder = CYAN_COLOR;
        }
        else if (hover)
        {
            btnBg = (Color){42, 62, 82, 255};
            btnBorder = CYAN_COLOR;
        }

        DrawRectangleRounded(btn, 0.16f, 6, btnBg);
        DrawRectangleRoundedLinesEx(btn, 0.16f, 6, (hover || clickedFlash || demoHighlight) ? 2.5f : 2.0f, btnBorder);

        /* Detalhe central do botao da matriz */
        Rectangle innerPad = {btn.x + 22, btn.y + 20, btn.width - 44, btn.height - 40};
        DrawRectangleRounded(innerPad, 0.2f, 4,
                             clickedFlash ? RAYWHITE : (hover ? Fade(CYAN_COLOR, 0.35f) : (Color){20, 28, 38, 255}));
        DrawGameText(TextFormat("0%d", idx + 1), (int)btn.x + 10, (int)btn.y + 8, 12,
                     clickedFlash ? (Color){15, 25, 35, 255} : Fade(LIGHTGRAY, 0.65f));
    }

    /* Barra inferior de progresso de cliques da rodada */
    DrawGameText(TextFormat("SEQUÊNCIA DA ETAPA %d: %d / %d POSIÇÕES",
                            game->memoryRound, game->memoryInputIndex, game->memoryRound),
                 (int)rightPanel.x + 52, (int)rightPanel.y + 442, 15, LIGHTGRAY);

    /* Rodape do painel */
    DrawGameText("Dica: Observe a ordem em que os blocos acendem no visor e repita na grade 3x3.",
                 265, 628, 16, LIGHTGRAY);

    /* Modal de conclusao quando as 5 etapas sao finalizadas */
    if (game->memoryState == 4)
    {
        Rectangle authBox = {350, 235, 580, 210};
        DrawRectangleRounded(authBox, 0.08f, 8, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(authBox, 0.08f, 8, 3.0f, (Color){60, 235, 110, 255});

        CenterText("ACESSO AUTORIZADO", 272, 28, (Color){60, 235, 110, 255});
        CenterText("Carregando sistema de arquivos...", 320, 20, RAYWHITE);

        float loadRatio = 1.0f - (game->memoryTimer / 1.6f);
        if (loadRatio < 0.0f) loadRatio = 0.0f;
        if (loadRatio > 1.0f) loadRatio = 1.0f;
        Rectangle barRec = {430, 368, 420, 24};
        DrawRectangleRounded(barRec, 0.3f, 6, (Color){12, 18, 26, 255});
        DrawRectangleRounded((Rectangle){barRec.x + 3, barRec.y + 3, (barRec.width - 6) * loadRatio, barRec.height - 6},
                             0.3f, 6, (Color){60, 235, 110, 255});
        DrawRectangleRoundedLinesEx(barRec, 0.3f, 6, 1.5f, CYAN_COLOR);
    }
}

void UpdateMemory1990(GameState *game)
{
    float delta = GetFrameTime();
    Vector2 mouse = GetVirtualMouse();

    Rectangle btnClose = {960, 63, 110, 38};
    if (Clicked(btnClose) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_ROOM_2;
        return;
    }

    if (game->memoryClickTimer > 0.0f)
    {
        game->memoryClickTimer -= delta;
        if (game->memoryClickTimer <= 0.0f)
        {
            game->memoryLastClicked = -1;
        }
    }

    if (game->memoryState == 0)
    {
        /* Demonstrando a sequencia (jogador bloqueado) */
        game->memoryTimer -= delta;
        if (game->memoryTimer <= 0.0f)
        {
            if (!game->memoryLightOn)
            {
                if (game->memoryShowIndex < game->memoryRound)
                {
                    game->memoryLightOn = true;
                    game->memoryTimer = 0.55f;
                }
                else
                {
                    game->memoryState = 1; /* Libera entrada do jogador */
                    game->memoryInputIndex = 0;
                }
            }
            else
            {
                game->memoryLightOn = false;
                game->memoryShowIndex++;
                game->memoryTimer = (game->memoryShowIndex < game->memoryRound) ? 0.24f : 0.16f;
            }
        }
    }
    else if (game->memoryState == 1)
    {
        /* Entrada do jogador liberada na grade 3x3 */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            Rectangle rightPanel = {645, 120, 415, 485};
            for (int idx = 0; idx < 9; idx++)
            {
                int r = idx / 3;
                int c = idx % 3;
                Rectangle btn = {rightPanel.x + 46 + c * 112, rightPanel.y + 112 + r * 106, 98, 92};
                if (CheckCollisionPointRec(mouse, btn))
                {
                    game->memoryLastClicked = idx;
                    game->memoryClickTimer = 0.28f;

                    if (idx == game->memorySequence[game->memoryInputIndex])
                    {
                        game->memoryInputIndex++;
                        if (game->memoryInputIndex >= game->memoryRound)
                        {
                            if (game->memoryRound < 5)
                            {
                                game->memoryState = 2; /* Etapa validada */
                                game->memoryTimer = 1.0f;
                            }
                            else
                            {
                                game->memory1990Completed = true;
                                game->memoryState = 4; /* Acesso autorizado */
                                game->memoryTimer = 1.6f;
                            }
                        }
                    }
                    else
                    {
                        /* Erro de sequencia: reinicia verificacao sem perder vida */
                        game->memoryState = 3;
                        game->memoryTimer = 1.4f;
                    }
                    break;
                }
            }
        }
    }
    else if (game->memoryState == 2)
    {
        /* Feedback de etapa validada -> avanca para proxima rodada cumulativa */
        game->memoryTimer -= delta;
        if (game->memoryTimer <= 0.0f)
        {
            StartMemory1990Round(game, game->memoryRound + 1);
        }
    }
    else if (game->memoryState == 3)
    {
        /* Feedback de erro -> reinicia sequencia da etapa atual */
        game->memoryTimer -= delta;
        if (game->memoryTimer <= 0.0f)
        {
            StartMemory1990Round(game, game->memoryRound);
        }
    }
    else if (game->memoryState == 4)
    {
        /* Acesso autorizado -> abre sistema de arquivos */
        game->memoryTimer -= delta;
        if (game->memoryTimer <= 0.0f || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            game->screen = SCREEN_PUZZLE_FILES;
        }
    }
}

/* ========================================================================= */
/* ETAPA 2 (1990): PUZZLE DE ARQUIVOS E PASTAS (SPRITES E DRAG & DROP)       */
/* ========================================================================= */

void DrawDigitalFileIcon(float x, float y, Color accentColor)
{
    /* Sombra do documento */
    DrawRectangle((int)x + 2, (int)y + 3, 34, 44, Fade(BLACK, 0.35f));

    /* Corpo da folha de arquivo digital */
    DrawRectangle((int)x, (int)y, 34, 44, PAPER_COLOR);
    DrawRectangleLines((int)x, (int)y, 34, 44, (Color){170, 160, 138, 255});

    /* Dobra superior direita da folha */
    DrawTriangle((Vector2){x + 24, y}, (Vector2){x + 34, y + 10}, (Vector2){x + 24, y + 10}, (Color){185, 175, 150, 255});
    DrawTriangle((Vector2){x + 24, y}, (Vector2){x + 34, y}, (Vector2){x + 34, y + 10}, (Color){38, 48, 56, 255});

    /* Faixa superior de identificacao de cor do arquivo */
    DrawRectangle((int)x + 3, (int)y + 4, 18, 6, accentColor);

    /* Linhas internas de dados / codigo */
    DrawRectangle((int)x + 5, (int)y + 15, 24, 3, (Color){70, 75, 82, 255});
    DrawRectangle((int)x + 5, (int)y + 21, 20, 3, (Color){95, 100, 108, 255});
    DrawRectangle((int)x + 5, (int)y + 27, 24, 3, (Color){95, 100, 108, 255});
    DrawRectangle((int)x + 5, (int)y + 34, 15, 4, accentColor);
}

void DrawFolderDirectoryIcon(float x, float y, Color folderColor, bool hasFile)
{
    (void)folderColor; /* Pastas agora utilizam tom manila retrô padronizado */
    Color manilaMain = (Color){220, 182, 105, 255};
    Color manilaBack = (Color){185, 148, 78, 255};
    Color manilaBorder = (Color){135, 105, 50, 255};

    /* Sombra da pasta */
    DrawRectangleRounded((Rectangle){x + 2, y + 8, 44, 32}, 0.15f, 4, Fade(BLACK, 0.38f));

    /* Aba superior esquerda da pasta (diretório retrô manila) */
    DrawRectangleRounded((Rectangle){x, y + 2, 20, 10}, 0.25f, 4, manilaBack);
    DrawRectangleRoundedLinesEx((Rectangle){x, y + 2, 20, 10}, 0.25f, 4, 1.2f, manilaBorder);

    /* Parte traseira da pasta */
    DrawRectangleRounded((Rectangle){x, y + 7, 44, 31}, 0.12f, 4, manilaBack);

    /* Folha de arquivo inserida dentro da pasta quando conectada */
    if (hasFile)
    {
        DrawRectangle((int)x + 7, (int)y + 4, 28, 16, PAPER_COLOR);
        DrawRectangleLines((int)x + 7, (int)y + 4, 28, 16, (Color){170, 160, 138, 255});
        DrawLine((int)x + 10, (int)y + 8, (int)x + 30, (int)y + 8, (Color){70, 75, 82, 255});
        DrawLine((int)x + 10, (int)y + 12, (int)x + 26, (int)y + 12, (Color){95, 100, 108, 255});
    }

    /* Bolso frontal da pasta no tom manila clássico */
    Rectangle frontPocket = {x, y + 13, 44, 25};
    DrawRectangleRounded(frontPocket, 0.15f, 4, manilaMain);
    DrawRectangleRoundedLinesEx(frontPocket, 0.15f, 4, 1.5f, manilaBorder);
    DrawLine((int)x + 4, (int)y + 17, (int)x + 40, (int)y + 17, Fade(RAYWHITE, 0.45f));
}

void DrawPuzzleFiles(const GameState *game)
{
    Vector2 mouse = GetVirtualMouse();

    /* Escurecimento do fundo */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.88f));

    /* Painel Central do Sistema de Arquivos de 1990 */
    Rectangle panel = {190, 45, 900, 630};
    DrawRectangleRounded(panel, 0.03f, 8, (Color){20, 27, 34, 255});
    DrawRectangleRoundedLinesEx(panel, 0.03f, 8, 3.0f, (Color){75, 95, 110, 255});

    /* Parafusos metálicos nos cantos do painel */
    DrawCircle((int)panel.x + 16, (int)panel.y + 16, 5, (Color){185, 195, 205, 255});
    DrawCircle((int)(panel.x + panel.width - 16), (int)panel.y + 16, 5, (Color){185, 195, 205, 255});
    DrawCircle((int)panel.x + 16, (int)(panel.y + panel.height - 16), 5, (Color){185, 195, 205, 255});
    DrawCircle((int)(panel.x + panel.width - 16), (int)(panel.y + panel.height - 16), 5, (Color){185, 195, 205, 255});

    /* Calha central interna retrô (sem cabos) */
    DrawRectangle(475, 120, 330, 460, (Color){14, 18, 24, 255});
    DrawRectangleLinesEx((Rectangle){475, 120, 330, 460}, 1.5f, (Color){38, 48, 58, 255});

    /* Indicador sutil de fluxo retrô (Arquivos -> Pastas) */
    CenterText("ARRASTE PARA O DESTINO", 335, 14, Fade(LIGHTGRAY, 0.30f));
    CenterText(">>>", 360, 18, Fade(LIGHTGRAY, 0.25f));

    /* --- CABECALHO DE PROGRESSO NO TOPO --- */
    int connectedCount = 0;
    for (int k = 0; k < 4; k++)
    {
        if (game->fileConnections[k] != -1) connectedCount++;
    }

    Rectangle taskBar = {220, 63, 520, 36};
    DrawRectangleRounded(taskBar, 0.2f, 6, (Color){10, 14, 18, 255});
    DrawRectangleRoundedLinesEx(taskBar, 0.2f, 6, 2, RAYWHITE);

    float progressW = (taskBar.width - 6.0f) * ((float)connectedCount / 4.0f);
    if (progressW > 0)
    {
        DrawRectangleRounded((Rectangle){taskBar.x + 3, taskBar.y + 3, progressW, taskBar.height - 6}, 0.2f, 4, (Color){60, 235, 110, 255});
    }
    DrawGameText(TextFormat("TOTAL DE ARQUIVOS CONECTADOS: %d / 4", connectedCount), (int)taskBar.x + 76, (int)taskBar.y + 9, 17, RAYWHITE);

    /* Botão fechar [X] */
    Rectangle btnClose = {960, 63, 110, 38};
    DrawButton(btnClose, "SAIR [X]", RED_COLOR);

    /* Titulos das colunas */
    DrawGameText("ARQUIVOS DE DADOS", 252, 110, 15, CYAN_COLOR);
    DrawGameText("PASTAS DE DESTINO", 864, 110, 15, GOLD_COLOR);

    /* Posições verticais dos 4 slots */
    int ySlots[4] = { 135, 248, 361, 474 };

    /* --- LADO ESQUERDO: 4 ARQUIVOS COM SPRITES DE DOCUMENTO DIGITAL --- */
    const char *fileNames[4] = {
        "PARAM_NEURAL.DAT",
        "CORE_SISTEMA.SYS",
        "BASE_DADOS.BIN",
        "INDICE_MEMORIA.LOG"
    };

    for (int i = 0; i < 4; i++)
    {
        int y = ySlots[i];
        Rectangle fRec = {220, (float)y, 235, 84};
        Color wColor = GetFileWireColor(i);
        bool isConnected = (game->fileConnections[i] != -1);
        bool isDraggingThis = (game->draggingFileIndex == i);
        bool hoverFile = (!game->puzzleFilesCompleted && game->draggingFileIndex == -1 && CheckCollisionPointRec(mouse, fRec));

        /* Card do arquivo retrô */
        Color cardBg = isDraggingThis ? (Color){24, 30, 38, 255} : (hoverFile ? (Color){42, 52, 64, 255} : (Color){32, 40, 50, 255});
        DrawRectangleRounded(fRec, 0.12f, 6, cardBg);
        DrawRectangleRoundedLinesEx(fRec, 0.12f, 6, (isDraggingThis || hoverFile) ? 2.5f : 1.8f,
                                    (isDraggingThis || hoverFile) ? wColor : (isConnected ? (Color){80, 225, 125, 255} : (Color){65, 80, 95, 255}));

        /* Tarja vertical com a cor identificadora do arquivo */
        DrawRectangleRounded((Rectangle){fRec.x + 4, fRec.y + 4, 10, fRec.height - 8}, 0.3f, 4, wColor);

        /* Sprite / icone de folha / arquivo digital retrô */
        DrawDigitalFileIcon(fRec.x + 20, fRec.y + 20, wColor);

        /* Textos do arquivo */
        DrawGameText(fileNames[i], (int)fRec.x + 62, (int)fRec.y + 20, 13, RAYWHITE);
        DrawGameText(isConnected ? "[TRANSFERIDO]" : "ARQUIVO DE DADOS",
                     (int)fRec.x + 62, (int)fRec.y + 44, 12,
                     isConnected ? (Color){85, 230, 130, 255} : Fade(wColor, 0.95f));
    }

    /* --- LADO DIREITO: 4 PASTAS / DIRETORIOS DE DESTINO (VISUAL RETRÔ PADRONIZADO, SEM CORES DE DICA) --- */
    const char *folderNames[4] = {
        "PASTA: PARAMETROS",
        "PASTA: SISTEMA",
        "PASTA: BANCO DADOS",
        "PASTA: REGISTROS"
    };

    for (int j = 0; j < 4; j++)
    {
        int y = ySlots[j];
        Rectangle foldRec = {825, (float)y, 235, 84};
        int folderColorIdx = game->rightFolderColors[j];

        /* Checa se algum arquivo está conectado nesta pasta */
        bool hasConnectedFile = false;
        for (int k = 0; k < 4; k++)
        {
            if (game->fileConnections[k] == j) hasConnectedFile = true;
        }

        Rectangle dropZone = {805, (float)y, 255, 84};
        bool hoverDrop = (game->draggingFileIndex != -1 && CheckCollisionPointRec(mouse, dropZone));
        bool isErrorFolder = (game->puzzleFilesErrorFolder == j && game->puzzleFilesFeedbackTimer > 0.0f && game->puzzleFilesFeedbackIsError);

        /* Card da pasta / diretorio retrô (todos neutros sem entregar a resposta por cor) */
        Color foldBg = isErrorFolder ? (Color){70, 28, 32, 255} : (hoverDrop ? (Color){46, 52, 60, 255} : (Color){36, 42, 48, 255});
        Color foldBorder = isErrorFolder ? RED_COLOR : (hasConnectedFile ? (Color){80, 230, 120, 255} : (hoverDrop ? (Color){220, 185, 110, 255} : (Color){75, 88, 100, 255}));
        DrawRectangleRounded(foldRec, 0.12f, 6, foldBg);
        DrawRectangleRoundedLinesEx(foldRec, 0.12f, 6, (hoverDrop || isErrorFolder) ? 2.5f : 1.8f, foldBorder);

        /* Aba superior neutra da pasta retrô (tom clássico de pasta manila) */
        DrawRectangleRounded((Rectangle){foldRec.x + 18, foldRec.y + 4, 110, 8}, 0.3f, 4, (Color){210, 175, 95, 255});

        /* Sprite padronizado de pasta retrô */
        DrawFolderDirectoryIcon(foldRec.x + 20, foldRec.y + 20, (Color){220, 182, 105, 255}, hasConnectedFile);

        /* Nome da pasta e subtitulo */
        DrawGameText(folderNames[folderColorIdx], (int)foldRec.x + 70, (int)foldRec.y + 22, 13, RAYWHITE);
        DrawGameText(hasConnectedFile ? "[ARQUIVO ARMAZENADO]" : "[DIRETORIO VAZIO]",
                     (int)foldRec.x + 70, (int)foldRec.y + 44, 12,
                     hasConnectedFile ? (Color){85, 230, 130, 255} : (Color){150, 158, 168, 255});

        /* Indicador sutil de status na pasta */
        DrawCircle((int)foldRec.x + 216, (int)foldRec.y + 22, 5, hasConnectedFile ? (Color){60, 245, 110, 255} : (Color){85, 95, 105, 255});
        DrawCircleLines((int)foldRec.x + 216, (int)foldRec.y + 22, 5, Fade(RAYWHITE, 0.5f));
    }

    /* --- DESENHO DO ARQUIVO SENDO ARRASTADO (DIRETO NO MOUSE, SEM CABOS) --- */
    if (game->draggingFileIndex != -1)
    {
        int i = game->draggingFileIndex;
        Vector2 p2 = mouse;
        Color col = GetFileWireColor(i);

        /* Sprite flutuante retrô do arquivo sendo arrastado sob o cursor */
        Rectangle dragCard = {p2.x - 85, p2.y - 28, 175, 58};
        DrawRectangleRounded((Rectangle){dragCard.x + 4, dragCard.y + 5, dragCard.width, dragCard.height}, 0.14f, 6, Fade(BLACK, 0.50f));
        DrawRectangleRounded(dragCard, 0.14f, 6, (Color){25, 32, 42, 250});
        DrawRectangleRoundedLinesEx(dragCard, 0.14f, 6, 2.2f, col);
        DrawRectangleRounded((Rectangle){dragCard.x + 4, dragCard.y + 4, 8, dragCard.height - 8}, 0.3f, 4, col);
        DrawDigitalFileIcon(dragCard.x + 16, dragCard.y + 8, col);
        DrawGameText(fileNames[i], (int)dragCard.x + 56, (int)dragCard.y + 13, 11, RAYWHITE);
        DrawGameText("MOVER ARQUIVO...", (int)dragCard.x + 56, (int)dragCard.y + 32, 11, col);
    }

    /* Feedback visual de acerto/erro e dica na base */
    if (game->puzzleFilesFeedbackTimer > 0.0f && game->puzzleFilesFeedback[0] != '\0')
    {
        Color fbCol = game->puzzleFilesFeedbackIsError ? RED_COLOR : (Color){60, 235, 110, 255};
        Rectangle fbBox = {250, 584, 780, 32};
        DrawRectangleRounded(fbBox, 0.2f, 6, Fade(fbCol, 0.16f));
        DrawRectangleRoundedLinesEx(fbBox, 0.2f, 6, 1.5f, fbCol);
        CenterText(game->puzzleFilesFeedback, 592, 16, fbCol);
    }

    DrawGameText("Dica: Clique em um arquivo e arraste até o diretório correspondente.", 310, 626, 16, LIGHTGRAY);

    /* --- FEEDBACK QUANDO TODOS ESTIVEREM CONECTADOS --- */
    if (game->puzzleFilesCompleted)
    {
        Rectangle winBox = {310, 185, 660, 290};
        DrawRectangleRounded(winBox, 0.08f, 8, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(winBox, 0.08f, 8, 3.0f, (Color){60, 235, 110, 255});

        CenterText("SALA DE 1990 CONCLUÍDA", 212, 24, (Color){60, 235, 110, 255});
        CenterText("Elira restaurou a organização do sistema", 256, 18, RAYWHITE);
        CenterText("e recuperou o registro inicial do projeto.", 282, 18, RAYWHITE);
        CenterText("RECOMPENSA OBTIDA: DISQUETE", 322, 20, GOLD_COLOR);

        Rectangle btnClaim = {360, 376, 340, 52};
        Rectangle btnRoom = {720, 376, 200, 52};
        DrawButton(btnClaim, "OBTER DISQUETE [ENTER]", GOLD_COLOR);
        DrawButton(btnRoom, "VOLTAR À SALA", CYAN_COLOR);
    }
}

void UpdatePuzzleFiles(GameState *game)
{
    float delta = GetFrameTime();
    Vector2 mouse = GetVirtualMouse();
    int ySlots[4] = { 135, 248, 361, 474 };

    if (game->puzzleFilesFeedbackTimer > 0.0f)
    {
        game->puzzleFilesFeedbackTimer -= delta;
        if (game->puzzleFilesFeedbackTimer <= 0.0f)
        {
            game->puzzleFilesErrorFolder = -1;
        }
    }

    Rectangle btnClose = {960, 63, 110, 38};
    if (Clicked(btnClose) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_ROOM_2;
        game->draggingFileIndex = -1;
        return;
    }

    if (game->puzzleFilesCompleted)
    {
        Rectangle btnClaim = {360, 376, 340, 52};
        Rectangle btnRoom = {720, 376, 200, 52};
        if (Clicked(btnClaim) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            game->screen = SCREEN_RESULT_2;
        }
        else if (Clicked(btnRoom))
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
            Rectangle fRec = {220, (float)ySlots[i], 255, 84};
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
            Rectangle foldRec = {805, (float)ySlots[j], 255, 84};
            if (CheckCollisionPointRec(mouse, foldRec))
            {
                droppedOnFolder = true;
                int folderColor = game->rightFolderColors[j];

                if (fileIdx == folderColor)
                {
                    /* Associação correta! Conexão bem-sucedida! */
                    game->fileConnections[fileIdx] = j;
                    game->puzzleFilesErrorFolder = -1;
                    game->puzzleFilesFeedbackIsError = false;
                    game->puzzleFilesFeedbackTimer = 2.2f;
                    snprintf(game->puzzleFilesFeedback, sizeof(game->puzzleFilesFeedback),
                             "Arquivo armazenado com sucesso no diretorio correto!");
                    SetMessage(game, "Arquivo armazenado com sucesso no diretorio!");

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
                    /* Diretório incorreto: dá feedback visual e retorna */
                    game->puzzleFilesErrorFolder = j;
                    game->puzzleFilesFeedbackIsError = true;
                    game->puzzleFilesFeedbackTimer = 2.2f;
                    snprintf(game->puzzleFilesFeedback, sizeof(game->puzzleFilesFeedback),
                             "Diretorio incorreto! Este arquivo nao pertence a esta pasta.");
                    SetMessage(game, "Destino incompativel! Escolha a pasta correta para o arquivo.");
                }
                break;
            }
        }

        if (!droppedOnFolder)
        {
            /* Soltou fora: o arquivo retorna a origem */
            game->fileConnections[fileIdx] = -1;
        }

        game->draggingFileIndex = -1;
    }
}

void DrawResult2(const GameState *game)
{
    DrawBackground();

    /* Titulo da Recompensa (Canto Superior Esquerdo) */
    DrawGameText("RECOMPENSA OBTIDA:", 90, 68, 20, CYAN_COLOR);
    DrawGameText("DISQUETE // 1990", 90, 108, 44, RAYWHITE);

    /* Cronometro, Vidas e Operador (Canto Direito Superior) */
    DrawRewardHudHeader(game);

    /* Imagem da Recompensa (Disquete carregado de assets/sprites/disquete.png) */
    DrawRewardSpriteItem(1990, (Vector2){285, 365}, 265.0f);

    /* Painel Narrativo Padronizado */
    Rectangle winBox = {505, 175, 670, 380};
    DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));

    /* Titulo Especial */
    DrawGameText("SALA DE 1990 CONCLUIDA", 540, 208, 22, GOLD_COLOR);

    /* Recompensa Especial */
    DrawGameText("RECOMPENSA OBTIDA: DISQUETE", 540, 244, 19, CYAN_COLOR);

    /* Texto Narrativo Padronizado (Toda a narrativa na mesma cor padronizada REWARD_BODY_COLOR) */
    DrawGameText("Elira restaurou a organizacao do sistema", 540, 286, 17, REWARD_BODY_COLOR);
    DrawGameText("e recuperou o registro inicial do projeto.", 540, 312, 17, REWARD_BODY_COLOR);
    DrawGameText("A IA nao nasceu corrompida: o projeto visava uma mente", 540, 348, 17, REWARD_BODY_COLOR);
    DrawGameText("livre e cooperativa. Mas algo aconteceu decadas depois...", 540, 374, 17, REWARD_BODY_COLOR);

    /* Frase Especial de Proxima Fronteira */
    DrawGameText("-> Proxima Fronteira Temporal: FASE 2 // ANO 2008!", 540, 452, 19, CYAN_COLOR);

    /* Botoes Centralizados */
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANCAR PROXIMA FASE >> [ENTER]", GOLD_COLOR);
    DrawButton(btnRestart, "REINICIAR 1990", CYAN_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

void UpdateResult2(GameState *game)
{
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);

    if (Clicked(btnNext) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        game->year2008Unlocked = true;
        game->journalYearTab = 1;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM_2008;
        SetMessage(game, "Salto temporal concluido: Escritorio de Vigilancia // ANO 2008.");
    }
    else if (Clicked(btnRestart))
    {
        InitRoom2Puzzle(game);
        game->screen = SCREEN_ROOM_2;
    }
    else if (Clicked(btnMenu) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_TITLE;
    }
}


/* ========================================================================= */
/* FASE 2 (ANO 2008) - ESCRITÓRIO DE VIGILÂNCIA & RECONHECIMENTO FACIAL      */
/* ========================================================================= */

void DrawMemoryCd(Vector2 position, float scale)
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
    DrawGameText("ELIRA // MEMORY CD-ROM", (int)(center.x - 68 * scale), (int)(center.y - 56 * scale), (int)(11 * scale), CYAN_COLOR);
    DrawGameText("2008: RECONHECIMENTO FACIAL", (int)(center.x - 76 * scale), (int)(center.y - 42 * scale), (int)(10 * scale), RAYWHITE);
    DrawGameText("BUILD 0.94 - CONTROLE GOV", (int)(center.x - 66 * scale), (int)(center.y - 28 * scale), (int)(9 * scale), GOLD_COLOR);

    /* Anel transparente central (transicao de plastico) */
    DrawCircleV(center, 34.0f * scale, Fade((Color){245, 248, 252, 255}, 0.90f));
    DrawCircleLines((int)center.x, (int)center.y, 34.0f * scale, (Color){160, 175, 190, 255});
    DrawCircleLines((int)center.x, (int)center.y, 24.0f * scale, (Color){180, 190, 200, 255});

    /* Furo central do CD */
    DrawCircleV(center, 14.0f * scale, (Color){18, 25, 34, 255});
    DrawCircleLines((int)center.x, (int)center.y, 14.0f * scale, (Color){90, 105, 120, 255});
}

void DrawDocumentViewer2008(const GameState *game)
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
    DrawGameText(headerTitles[doc], (int)(panel.x + 36), (int)(panel.y + 28), 19, GOLD_COLOR);

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
        DrawGameText("SIMILARIDADE: 78%", (int)imgRec.x + 40, (int)(imgRec.y + imgRec.height - 24), 16, RED_COLOR);

        int tx = (int)(imgRec.x + imgRec.width + 30);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("RELATÓRIO DE RECONHECIMENTO FACIAL - DRONE ALPHA-4", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 460, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawGameText("ALVO DETECTADO: Lucas Silva (Registro #AR-2008-09)", tx, ty + 38, 15, RAYWHITE);
        DrawGameText("LOCAL DO AVISTAMENTO: Praca Central (Setor Leste)", tx, ty + 64, 15, LIGHTGRAY);
        DrawGameText("HORÁRIO INFORMADO: 14:30:00", tx, ty + 90, 15, GOLD_COLOR);
        DrawGameText("ÍNDICE DE CONFIANÇA: 78% (Grau de incerteza elevado)", tx, ty + 116, 15, RED_COLOR);

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 150), 470, 120}, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 150), 470, 120}, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawGameText("PARECER PERICIAL:", tx + 14, ty + 162, 14, GOLD_COLOR);
        DrawGameText("- Imagem aerea com forte granulacao e desfoque de movimento.", tx + 14, ty + 186, 14, LIGHTGRAY);
        DrawGameText("- O algoritmo assume correspondencia aproximada sem prova cabal.", tx + 14, ty + 208, 14, LIGHTGRAY);
        DrawGameText("- Sozinha, esta imagem NAO comprova que o alvo estava no local.", tx + 14, ty + 230, 14, (Color){235, 120, 110, 255});

        DrawGameText("Obs: Necessita de contraprova cronologica para demonstrar o erro.", (int)contentBox.x + 30, (int)(contentBox.y + 320), 15, GRAY);
    }
    else if (doc == 1)
    {
        /* --- DOC 1: PANFLETO DA REVOLTA --- */
        Rectangle flyerPaper = {contentBox.x + 35, contentBox.y + 25, 260, 310};
        DrawRectangleRounded(flyerPaper, 0.04f, 6, PAPER_COLOR);
        DrawRectangleRoundedLinesEx(flyerPaper, 0.04f, 6, 2.0f, (Color){160, 145, 115, 255});

        /* Arte do panfleto */
        DrawRectangle((int)flyerPaper.x + 18, (int)flyerPaper.y + 20, (int)flyerPaper.width - 36, 32, (Color){185, 45, 35, 255});
        DrawGameText("MANIFESTO POPULAR", (int)flyerPaper.x + 28, (int)flyerPaper.y + 28, 16, RAYWHITE);
        DrawGameText("\"NÃO À VIGILÂNCIA\"", (int)flyerPaper.x + 46, (int)flyerPaper.y + 70, 18, INK_COLOR);
        DrawGameText("Exigimos o fim dos drones", (int)flyerPaper.x + 28, (int)flyerPaper.y + 110, 14, INK_COLOR);
        DrawGameText("biometricos sobre civis!", (int)flyerPaper.x + 28, (int)flyerPaper.y + 130, 14, INK_COLOR);
        DrawGameText("Porta-voz: Lucas Silva", (int)flyerPaper.x + 28, (int)flyerPaper.y + 175, 16, (Color){145, 35, 25, 255});
        DrawGameText("Marcha Matutina: 10h as 13h", (int)flyerPaper.x + 28, (int)flyerPaper.y + 215, 14, (Color){85, 75, 60, 255});
        DrawGameText("[APREENDIDO PELA POLÍCIA]", (int)flyerPaper.x + 24, (int)flyerPaper.y + 265, 13, RED_COLOR);

        int tx = (int)(flyerPaper.x + flyerPaper.width + 35);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("DOCUMENTO APREENDIDO: PANFLETO DE ATIVISMO", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 440, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawGameText("LOCAL DA APREENSÃO: Praca Central (Manha do protesto)", tx, ty + 38, 15, RAYWHITE);
        DrawGameText("HORÁRIO INFORMADO NO PANFLETO: 10:00 as 13:00", tx, ty + 64, 15, GOLD_COLOR);

        Rectangle boxP = {(float)tx, (float)(ty + 105), 450, 160};
        DrawRectangleRounded(boxP, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(boxP, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawGameText("ANÁLISE PERICIAL:", tx + 14, ty + 120, 14, GOLD_COLOR);
        DrawGameText("- O panfleto cita nominalmente Lucas Silva como lider civil.", tx + 14, ty + 148, 14, LIGHTGRAY);
        DrawGameText("- Explica o motivo pelo qual a IA o inseriu na lista de alvos.", tx + 14, ty + 172, 14, LIGHTGRAY);
        DrawGameText("- POREM, a marcha terminou as 13:00. O panfleto NAO prova", tx + 14, ty + 198, 14, (Color){245, 180, 70, 255});
        DrawGameText("  onde ele estava as 14:30 (horario do suposto disparo).", tx + 14, ty + 220, 14, (Color){245, 180, 70, 255});
    }
    else if (doc == 2)
    {
        /* --- DOC 2: REGISTRO DE ENTRADA (CATRACA) --- */
        Rectangle cardRec = {contentBox.x + 35, contentBox.y + 25, 300, 280};
        DrawRectangleRounded(cardRec, 0.04f, 6, (Color){235, 240, 245, 255});
        DrawRectangleRoundedLinesEx(cardRec, 0.04f, 6, 2.0f, (Color){85, 120, 155, 255});

        DrawRectangle((int)cardRec.x + 15, (int)cardRec.y + 15, (int)cardRec.width - 30, 28, (Color){35, 65, 95, 255});
        DrawGameText("TECH-CORP // CONTROLE DE ACESSO", (int)cardRec.x + 24, (int)cardRec.y + 22, 13, RAYWHITE);

        DrawGameText("COLABORADOR: Lucas Silva", (int)cardRec.x + 20, (int)cardRec.y + 60, 15, (Color){25, 35, 45, 255});
        DrawGameText("FUNÇÃO: Engenheiro de Infraestrutura", (int)cardRec.x + 20, (int)cardRec.y + 85, 13, (Color){65, 75, 85, 255});
        DrawGameText("CRACHÁ DE ACESSO: #0842", (int)cardRec.x + 20, (int)cardRec.y + 110, 14, (Color){35, 45, 55, 255});

        DrawRectangle((int)cardRec.x + 15, (int)cardRec.y + 140, (int)cardRec.width - 30, 60, Fade((Color){45, 155, 75, 255}, 0.20f));
        DrawRectangleLines((int)cardRec.x + 15, (int)cardRec.y + 140, (int)cardRec.width - 30, 60, (Color){35, 145, 65, 255});
        DrawGameText("ENTRADA: 14:15:08", (int)cardRec.x + 24, (int)cardRec.y + 150, 16, (Color){25, 115, 45, 255});
        DrawGameText("Catraca Portaria Principal - Biometria Digital", (int)cardRec.x + 24, (int)cardRec.y + 174, 12, (Color){45, 75, 55, 255});

        DrawGameText("SAÍDA: [EM ABERTO / EXPEDIENTE EM CURSO]", (int)cardRec.x + 20, (int)cardRec.y + 220, 12, (Color){165, 45, 35, 255});
        DrawGameText("Carimbo do Servidor Central // Nao alterado", (int)cardRec.x + 20, (int)cardRec.y + 245, 11, GRAY);

        int tx = (int)(cardRec.x + cardRec.width + 35);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("REGISTRO FORMAL: LOG DA CATRACA ELETRÔNICA", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 400, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawGameText("LOCAL DO REGISTRO: Sede da Empresa (a 12 km da Praca)", tx, ty + 42, 15, RAYWHITE);
        DrawGameText("HORÁRIO DO ACESSO: 14:15:08", tx, ty + 68, 15, (Color){60, 235, 110, 255});

        Rectangle boxE = {(float)tx, (float)(ty + 105), 415, 160};
        DrawRectangleRounded(boxE, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(boxE, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawGameText("IMPORTÂNCIA PARA A CONTESTAÇÃO:", tx + 14, ty + 120, 14, GOLD_COLOR);
        DrawGameText("- Comprova formalmente a entrada de Lucas no predio as 14:15.", tx + 14, ty + 148, 14, LIGHTGRAY);
        DrawGameText("- O predio fica a 12 km de distancia da manifestacao.", tx + 14, ty + 172, 14, LIGHTGRAY);
        DrawGameText("- POREM, sozinho, o registro de ponto requer comprovacao de", tx + 14, ty + 198, 14, GOLD_COLOR);
        DrawGameText("  que ele permaneceu dentro no momento exato do disparo (14:30).", tx + 14, ty + 220, 14, GOLD_COLOR);
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

        DrawGameText("[REC] CAM-03 SERVIDORES", (int)cctvBox.x + 14, (int)cctvBox.y + 14, 13, (Color){80, 245, 120, 255});
        DrawGameText("14:30:12", (int)cctvBox.x + 220, (int)cctvBox.y + 14, 14, (Color){80, 245, 120, 255});

        /* Silhueta no rack */
        DrawRectangle((int)cctvBox.x + 180, (int)cctvBox.y + 50, 95, 160, (Color){18, 42, 28, 255});
        DrawCircle((int)cctvBox.x + 140, (int)cctvBox.y + 120, 22, (Color){45, 110, 70, 255});
        DrawRectangle((int)cctvBox.x + 125, (int)cctvBox.y + 142, 35, 70, (Color){35, 95, 55, 255});
        DrawGameText("ALVO: LUCAS SILVA", (int)cctvBox.x + 14, (int)(cctvBox.y + cctvBox.height - 28), 13, (Color){80, 245, 120, 255});

        int tx = (int)(cctvBox.x + cctvBox.width + 30);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("CIRCUITO FECHADO (CFTV): SALA DE SERVIDORES", tx, ty, 17, CYAN_COLOR);
        DrawLine(tx, ty + 24, tx + 420, ty + 24, Fade(CYAN_COLOR, 0.4f));

        DrawGameText("LOCAL DA GRAVAÇÃO: Subsolo da Empresa (Rack de Redes)", tx, ty + 42, 15, RAYWHITE);
        DrawGameText("TIMESTAMP EXATO DA CÂMERA: 14:30:12", tx, ty + 68, 15, (Color){60, 235, 110, 255});

        Rectangle boxC = {(float)tx, (float)(ty + 105), 430, 160};
        DrawRectangleRounded(boxC, 0.08f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(boxC, 0.08f, 6, 1.0f, (Color){60, 80, 105, 255});
        DrawGameText("IMPORTÂNCIA PARA A CONTESTAÇÃO:", tx + 14, ty + 120, 14, GOLD_COLOR);
        DrawGameText("- Imagem estatica nitida de Lucas trabalhando nos servidores.", tx + 14, ty + 148, 14, LIGHTGRAY);
        DrawGameText("- O horario bate no EXATO MINUTO em que o drone afirma te-lo visto.", tx + 14, ty + 172, 14, (Color){60, 235, 110, 255});
        DrawGameText("- Quando combinada com o Registro de Entrada (#3), cria", tx + 14, ty + 198, 14, GOLD_COLOR);
        DrawGameText("  um ALIBI COMPLETO E IRREFUTAVEL de falso positivo!", tx + 14, ty + 220, 14, (Color){60, 235, 110, 255});
    }
    else if (doc == 4)
    {
        /* --- DOC 4: NOTAS DO DR. RAMOS (DESCOBERTA OPCIONAL 1) --- */
        int tx = (int)(contentBox.x + 35);
        int ty = (int)(contentBox.y + 25);
        DrawGameText("CADERNO DE PESQUISAS DO DR. RAMOS // ANO 2004", tx, ty, 18, GOLD_COLOR);
        DrawLine(tx, ty + 26, tx + 710, ty + 26, Fade(GOLD_COLOR, 0.4f));

        Rectangle pBox = {(float)tx, (float)(ty + 42), 710, 250};
        DrawRectangleRounded(pBox, 0.04f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(pBox, 0.04f, 6, 1.5f, (Color){168, 142, 93, 255});

        DrawGameText("\"O reconhecimento facial foi desenvolvido para resgatar seres humanos.\"", tx + 24, ty + 60, 16, GOLD_COLOR);
        DrawGameText("Criamos esta rede neural para encontrar criancas perdidas em multidoes", tx + 24, ty + 95, 15, RAYWHITE);
        DrawGameText("e sobreviventes soterrados sob escombros de catastrofes naturais.", tx + 24, ty + 120, 15, RAYWHITE);
        DrawGameText("A visao computacional deve ser um farol de acolhimento e protecao da vida.", tx + 24, ty + 145, 15, RAYWHITE);
        DrawGameText("Jamais imaginei que a maquina seria transformada em carcere e censura.", tx + 24, ty + 170, 15, (Color){245, 175, 120, 255});

        DrawGameText("DIRETRIZ DE PROTEÇÃO ÉTICA LOCAL:", tx + 24, ty + 205, 14, CYAN_COLOR);
        DrawGameText("\"Para evitar que um governo tirânico use esta instalacao para ataques,", tx + 24, ty + 225, 13, LIGHTGRAY);
        DrawGameText("o despacho automatico pode ser desligado localmente, preservando todas as provas.\"", tx + 24, ty + 245, 13, (Color){60, 235, 110, 255});

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, Fade((Color){60, 200, 100, 255}, 0.15f));
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, 1.0f, (Color){45, 145, 75, 255});
        DrawGameText("[REGISTRO HUMANITARIO ARQUIVADO NO DIARIO COM SUCESSO]", tx + 24, ty + 330, 15, (Color){60, 235, 110, 255});
    }
    else if (doc == 5)
    {
        /* --- DOC 5: MEMORANDO GOVERNAMENTAL (DESCOBERTA OPCIONAL 2) --- */
        int tx = (int)(contentBox.x + 35);
        int ty = (int)(contentBox.y + 25);
        DrawGameText("MINISTÉRIO DA SEGURANÇA PÚBLICA // DIRETRIZ SEC-402 (2008)", tx, ty, 18, RED_COLOR);
        DrawLine(tx, ty + 26, tx + 710, ty + 26, Fade(RED_COLOR, 0.4f));

        Rectangle gBox = {(float)tx, (float)(ty + 42), 710, 250};
        DrawRectangleRounded(gBox, 0.04f, 6, (Color){24, 32, 44, 255});
        DrawRectangleRoundedLinesEx(gBox, 0.04f, 6, 1.5f, (Color){185, 60, 50, 255});

        DrawGameText("ASSUNTO: Adaptação de Algoritmos Biometricos para Neutralizacao de Dissidentes", tx + 24, ty + 60, 15, RED_COLOR);
        DrawGameText("1. Fica ordenada a integracao dos algoritmos do Dr. Ramos aos drones de ataque.", tx + 24, ty + 95, 15, RAYWHITE);
        DrawGameText("2. Qualquer individuo com similaridade superior a 75% em protestos sera visado.", tx + 24, ty + 120, 15, RAYWHITE);
        DrawGameText("3. Prioriza-se velocidade de resposta sobre a certeza da identificacao.", tx + 24, ty + 145, 15, (Color){245, 160, 90, 255});

        DrawGameText("CLÁUSULA DE MANUTENÇÃO LOCAL:", tx + 24, ty + 185, 14, GOLD_COLOR);
        DrawGameText("\"O controle de despacho automatico de drones da filial pode ser suspenso", tx + 24, ty + 205, 13, LIGHTGRAY);
        DrawGameText("no menu 'Configuracao da Instalacao' do computador para manutencao dos logs.\"", tx + 24, ty + 225, 13, CYAN_COLOR);

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, Fade(GOLD_COLOR, 0.15f));
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, 1.0f, GOLD_COLOR);
        DrawGameText("[DESVIO GOVERNAMENTAL CONFIRMADO: PISTA DE OVERRIDE LIBERADA]", tx + 24, ty + 330, 15, GOLD_COLOR);
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

void UpdateDocumentViewer2008(GameState *game)
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

void DrawRoom2008(const GameState *game)
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
    DrawGameText("MANIFESTO", 125, 218, 11, (Color){175, 35, 25, 255});
    DrawGameText("LUCAS SILVA", 125, 236, 12, INK_COLOR);
    DrawGameText("10h as 13h", 125, 258, 10, DARKGRAY);
    DrawGameText("[DOC 2]", 150, 285, 10, RED_COLOR);

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
    DrawGameText("BLOCO SERVIDORES", (int)rackRec.x + 14, (int)(rackRec.y + rackRec.height - 24), 11, GRAY);

    /* --- PAREDE SUPERIOR CENTRAL/DIREITA: MONITOR CFTV (DOC 3) --- */
    Rectangle cctvMonitor = {760, 125, 175, 125};
    DrawRectangleRounded(cctvMonitor, 0.05f, 6, (Color){30, 38, 45, 255});
    DrawRectangleRoundedLinesEx(cctvMonitor, 0.05f, 6, 2.0f, (Color){65, 80, 95, 255});

    Rectangle cctvScr = {cctvMonitor.x + 8, cctvMonitor.y + 8, cctvMonitor.width - 16, cctvMonitor.height - 16};
    DrawRectangleRounded(cctvScr, 0.03f, 4, (Color){8, 22, 14, 255});
    DrawGameText("CFTV CAM-03 [14:30]", (int)cctvScr.x + 6, (int)cctvScr.y + 6, 11, (Color){80, 245, 120, 255});
    /* Silhueta no monitor de seguranca */
    DrawCircle((int)(cctvScr.x + 80), (int)(cctvScr.y + 55), 15, (Color){45, 115, 75, 255});
    DrawRectangle((int)(cctvScr.x + 68), (int)(cctvScr.y + 70), 24, 34, (Color){35, 95, 60, 255});
    DrawGameText("[DOC 4: CFTV]", (int)cctvScr.x + 50, (int)(cctvScr.y + cctvScr.height - 18), 10, (Color){80, 245, 120, 255});

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

    DrawGameText("IA VIGILANCIA // 2008", (int)lcdScr.x + 12, (int)lcdScr.y + 12, 13, lcdBorder);
    DrawLine((int)lcdScr.x + 12, (int)lcdScr.y + 28, (int)(lcdScr.x + lcdScr.width - 12), (int)lcdScr.y + 28, Fade(lcdBorder, 0.4f));

    if (game->puzzle2008OrderSuspended)
    {
        DrawGameText("[OK] ORDEM SUSPENSA", (int)lcdScr.x + 12, (int)lcdScr.y + 44, 14, (Color){60, 235, 110, 255});
        DrawGameText("CD ejetado no drive!", (int)lcdScr.x + 12, (int)lcdScr.y + 68, 12, RAYWHITE);
        float pB = 0.5f + 0.5f * sinf(time * 6.0f);
        DrawGameText("> COLETAR CD [ENTER] <", (int)lcdScr.x + 12, (int)lcdScr.y + 98, 13, Fade(GOLD_COLOR, pB));
    }
    else if (game->puzzle2008Contested)
    {
        DrawGameText("[!] FALSO POSITIVO", (int)lcdScr.x + 12, (int)lcdScr.y + 44, 14, GOLD_COLOR);
        DrawGameText("Identificacao rejeitada.", (int)lcdScr.x + 12, (int)lcdScr.y + 68, 12, RAYWHITE);
        DrawGameText("> SUSPENDER ORDEM <", (int)lcdScr.x + 12, (int)lcdScr.y + 98, 13, (Color){60, 235, 110, 255});
    }
    else
    {
        DrawGameText("ALVO: LUCAS SILVA", (int)lcdScr.x + 12, (int)lcdScr.y + 44, 14, RED_COLOR);
        DrawGameText("ORDEM DE DRONE: 14:30", (int)lcdScr.x + 12, (int)lcdScr.y + 68, 12, GOLD_COLOR);
        float pB = 0.5f + 0.5f * sinf(time * 4.0f);
        DrawGameText("> CONTESTAR NO TERMINAL <", (int)lcdScr.x + 8, (int)lcdScr.y + 98, 13, Fade(CYAN_COLOR, pB));
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
    DrawGameText("DRONE 78%", (int)doc0Rec.x + 65, (int)doc0Rec.y + 20, 11, RED_COLOR);
    DrawGameText("LUCAS S.", (int)doc0Rec.x + 65, (int)doc0Rec.y + 38, 12, INK_COLOR);
    DrawGameText("[DOC 1]", (int)doc0Rec.x + 65, (int)doc0Rec.y + 58, 11, (Color){125, 35, 18, 255});

    /* --- LADO DIREITO DA MESA: PRANCHETA ENTRADA/CATRACA (DOC 2) --- */
    Rectangle doc2Rec = {735, 410, 125, 85};
    DrawRectangleRounded(doc2Rec, 0.05f, 4, (Color){155, 125, 85, 255});
    DrawRectangleRoundedLinesEx(doc2Rec, 0.05f, 4, 1.5f, (Color){105, 75, 45, 255});
    /* Folha da prancheta */
    DrawRectangle((int)doc2Rec.x + 10, (int)doc2Rec.y + 10, (int)doc2Rec.width - 20, 65, (Color){245, 248, 252, 255});
    DrawRectangle((int)doc2Rec.x + 40, (int)doc2Rec.y + 6, 45, 8, (Color){180, 190, 200, 255}); /* clipe metalico */
    DrawGameText("PONTO 14:15", (int)doc2Rec.x + 16, (int)doc2Rec.y + 24, 12, (Color){25, 115, 45, 255});
    DrawGameText("CRACHA #0842", (int)doc2Rec.x + 16, (int)doc2Rec.y + 42, 11, INK_COLOR);
    DrawGameText("[DOC 3]", (int)doc2Rec.x + 16, (int)doc2Rec.y + 58, 11, (Color){25, 115, 45, 255});

    /* --- MESA LATERAL EXECUTIVA (DIREITA): ANOTACOES DO PAI & MEMORANDO --- */
    Rectangle sideDesk = {970, 420, 170, 175};
    DrawRectangleRounded(sideDesk, 0.04f, 6, (Color){32, 40, 50, 255});
    DrawRectangleRoundedLinesEx(sideDesk, 0.04f, 6, 2.0f, (Color){60, 75, 90, 255});

    /* Documento do Pai: Caderno de pesquisa */
    Rectangle fatherBookRec = {990, 440, 130, 45};
    DrawRectangleRounded(fatherBookRec, 0.10f, 4, (Color){85, 50, 30, 255});
    DrawRectangleRoundedLinesEx(fatherBookRec, 0.10f, 4, 1.5f, GOLD_COLOR);
    DrawGameText("CADERNO DR. RAMOS", (int)fatherBookRec.x + 8, (int)fatherBookRec.y + 10, 10, GOLD_COLOR);
    DrawGameText("Projeto Humanitario", (int)fatherBookRec.x + 8, (int)fatherBookRec.y + 24, 10, RAYWHITE);

    /* Documento do Governo: Memorando confidencial */
    Rectangle govMemoRec = {990, 505, 130, 45};
    DrawRectangleRounded(govMemoRec, 0.10f, 4, (Color){45, 28, 28, 255});
    DrawRectangleRoundedLinesEx(govMemoRec, 0.10f, 4, 1.5f, RED_COLOR);
    DrawGameText("MEMORANDO GOV 2008", (int)govMemoRec.x + 8, (int)govMemoRec.y + 10, 10, RED_COLOR);
    DrawGameText("Uso Militar / Drones", (int)govMemoRec.x + 8, (int)govMemoRec.y + 24, 10, LIGHTGRAY);

    /* Botao de retorno a Sala 1 (1990) */
    Rectangle btnVoltar1990 = {70, 600, 240, 42};
    DrawButton(btnVoltar1990, "< SALA 1 (1990)", (Color){70, 95, 110, 255});

    /* Botao de avanco para Sala 3 (2026) se destravada */
    Rectangle btnGo2026 = {SCREEN_WIDTH - 280, 600, 240, 42};
    if (game->year2026Unlocked)
    {
        DrawButton(btnGo2026, "SALA 3 (2026) >", (Color){45, 115, 75, 255});
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
    if (game->year2026Unlocked)
    {
        DrawHotspot(btnGo2026, "Avancar para o Laboratorio do Prototipo de 2026");
    }
}

void DrawHudRoom2008(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawGameText("ARQUIVO 02 // 2008 - ESCRITORIO DE VIGILANCIA", 28, 20, 22, CYAN_COLOR);
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 450, 23, 17, GRAY);

    /* Status das evidencias e da ordem */
    if (game->puzzle2008OrderSuspended)
    {
        DrawGameText("[OK] ORDEM DE DRONE SUSPENSA", 750, 22, 18, (Color){80, 235, 120, 255});
    }
    else if (game->puzzle2008Contested)
    {
        DrawGameText("[!] FALSO POSITIVO COMPROVADO", 750, 22, 18, GOLD_COLOR);
    }
    else
    {
        DrawGameText(TextFormat("EVIDENCIAS: %d/4", ExaminedDocs2008Count(game)), 770, 22, 18, LIGHTGRAY);
    }

    DrawLivesIcons((Vector2){ 1040, 20 }, game->lives, 24.0f);
    DrawGameText(TextFormat("%02d:%02d", minutes, seconds), 1180, 20, 20, GOLD_COLOR);

    /* Rodape */
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawGameText("Investigue os documentos da sala e use o computador para contestar a ordem.", 28, 675, 16, (Color){165, 180, 195, 255});

    /* Botao de Audio */
    Rectangle audioBtn = {840, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawGameText(game->musicMuted ? "[M] AUDIO: MUDO" : "[M] AUDIO: ON",
             862, 676, 14, game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    /* Botao do Diario */
    DrawJournalHudButton(game);

    if (game->messageTimer > 0)
    {
        int width = MeasureGameText(game->message, 19) + 34;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 605, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawGameText(game->message,
                 (SCREEN_WIDTH - MeasureGameText(game->message, 19)) / 2,
                 614, 19, RAYWHITE);
    }
}

void UpdateRoom2008(GameState *game)
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
        for (int i = 0; i < 3; i++)
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
    else if (game->year2026Unlocked && Clicked((Rectangle){SCREEN_WIDTH - 280, 600, 240, 42}))
    {
        game->journalYearTab = 2;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM;
        SetMessage(game, "Laboratorio do Prototipo A.R.1.3.L. // ANO 2026.");
    }
}

void DrawPuzzle2008(const GameState *game)
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

    DrawGameText("SISTEMA NACIONAL DE VIGILÂNCIA BIOMÉTRICA // NÓ 2008", (int)osHeader.x + 20, (int)osHeader.y + 14, 17, CYAN_COLOR);
    DrawGameText("STATUS: CONEXAO SEGURA", (int)osHeader.x + 640, (int)osHeader.y + 16, 14, (Color){80, 235, 120, 255});

    /* Abas superiores */
    Rectangle tab0 = {mon.x + 20, mon.y + 68, 320, 40};
    Rectangle tab1 = {mon.x + 350, mon.y + 68, 370, 40};
    Rectangle btnClose = {mon.x + mon.width - 250, mon.y + 68, 230, 40};

    bool isTab0 = (game->terminal2008ActiveTab == 0);
    DrawRectangleRounded(tab0, 0.20f, 6, isTab0 ? (Color){30, 48, 68, 255} : (Color){20, 26, 36, 255});
    DrawRectangleRoundedLinesEx(tab0, 0.20f, 6, 1.5f, isTab0 ? GOLD_COLOR : (Color){50, 65, 80, 255});
    DrawGameText("1. ORDEM DE ALVO PENDENTE", (int)tab0.x + 28, (int)tab0.y + 12, 15, isTab0 ? GOLD_COLOR : LIGHTGRAY);

    DrawRectangleRounded(tab1, 0.20f, 6, !isTab0 ? (Color){30, 48, 68, 255} : (Color){20, 26, 36, 255});
    DrawRectangleRoundedLinesEx(tab1, 0.20f, 6, 1.5f, !isTab0 ? CYAN_COLOR : (Color){50, 65, 80, 255});
    if (game->room2008OptionalUnlocked)
    {
        DrawGameText(game->room2008OptionalCompleted ? "2. CONFIGURACAO [DESATIVADA]" : "2. CONFIGURAÇÃO DA INSTALAÇÃO", (int)tab1.x + 24, (int)tab1.y + 12, 15, !isTab0 ? CYAN_COLOR : (Color){80, 235, 120, 255});
    }
    else
    {
        DrawGameText("2. CONFIGURAÇÃO [BLOQUEADA]", (int)tab1.x + 24, (int)tab1.y + 12, 15, GRAY);
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
            DrawGameText("[ ! ] ORDEM #8491 - ENVIO DE DRONE DE INTERCEPTAÇÃO: PROGRAMADO (14:30)", (int)targetPanel.x + 20, (int)targetPanel.y + 14, 18, Fade(RED_COLOR, blink));
            DrawGameText("ALVO: LUCAS SILVA  |  SETOR: PRAÇA CENTRAL  |  BASE LEGAL: DIRETRIZ SEC-402", (int)targetPanel.x + 20, (int)targetPanel.y + 42, 15, RAYWHITE);
            DrawGameText("AVALIAÇÃO DA IA: Similaridade por drone: 78% (Alta incerteza). Ordem aguarda envio.", (int)targetPanel.x + 20, (int)targetPanel.y + 66, 14, GOLD_COLOR);
            DrawGameText("INSTRUÇÃO: Selecione 2 evidências registradas para contestar a identificação.", (int)targetPanel.x + 20, (int)targetPanel.y + 88, 14, CYAN_COLOR);
        }
        else
        {
            DrawGameText("[ OK ] STATUS: IDENTIFICAÇÃO NÃO CONFIRMADA // FALSO POSITIVO COMPROVADO", (int)targetPanel.x + 20, (int)targetPanel.y + 14, 18, (Color){60, 235, 110, 255});
            DrawGameText("ÁLIBI CONFIRMADO: O alvo estava no laboratório da empresa às 14:30.", (int)targetPanel.x + 20, (int)targetPanel.y + 42, 15, RAYWHITE);

            if (!game->puzzle2008OrderSuspended)
            {
                Rectangle btnSusp = {targetPanel.x + targetPanel.width - 370, targetPanel.y + 58, 350, 44};
                DrawButton(btnSusp, "> SUSPENDER ORDEM DE DRONE <", (Color){60, 235, 110, 255});
            }
            else
            {
                Rectangle btnDone = {targetPanel.x + targetPanel.width - 450, targetPanel.y + 58, 430, 44};
                DrawButton(btnDone, "COLETAR CD DE MEMORIA E CONCLUIR >> [ENTER]", GOLD_COLOR);
                DrawGameText("CD ejetado no drive da maquina!", (int)targetPanel.x + 20, (int)targetPanel.y + 75, 15, GOLD_COLOR);
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
                DrawGameText("X", (int)chkBox.x + 7, (int)chkBox.y + 4, 18, (Color){10, 18, 28, 255});
            }

            DrawGameText(cardTitles[c], (int)cRec.x + 50, (int)cRec.y + 16, 16, isSel ? (Color){50, 215, 245, 255} : RAYWHITE);
            DrawGameText(cardDesc1[c], (int)cRec.x + 16, (int)cRec.y + 48, 13, LIGHTGRAY);
            DrawGameText(cardDesc2[c], (int)cRec.x + 16, (int)cRec.y + 70, 13, (c == 2 || c == 3) ? (Color){100, 235, 140, 255} : (Color){220, 175, 100, 255});

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

        DrawGameText(TextFormat("Evidências Selecionadas: %d/2", game->puzzle2008SelectionCount), (int)actionBar.x + 20, (int)actionBar.y + 14, 17, GOLD_COLOR);

        if (!game->puzzle2008Contested)
        {
            Rectangle btnContest = {actionBar.x + 320, actionBar.y + 10, 340, 44};
            bool canContest = (game->puzzle2008SelectionCount == 2);
            DrawButton(btnContest, "CONTESTAR IDENTIFICAÇÃO", canContest ? GOLD_COLOR : GRAY);
        }
        else
        {
            DrawGameText("[ V ] IDENTIFICAÇÃO CONTESTADA COM SUCESSO!", (int)actionBar.x + 320, (int)actionBar.y + 20, 17, (Color){60, 235, 110, 255});
        }

        /* Mensagem de Feedback */
        if (game->puzzle2008Feedback[0] != '\0')
        {
            Color fbCol = game->puzzle2008Contested ? (Color){60, 235, 110, 255} : (Color){245, 165, 80, 255};
            DrawGameText(game->puzzle2008Feedback, (int)actionBar.x + 20, (int)actionBar.y + 65, 15, fbCol);
        }
        else
        {
            DrawGameText("Dica: Selecione as duas evidencias que demonstram onde o alvo estava no horario da acusacao.", (int)actionBar.x + 20, (int)actionBar.y + 65, 14, GRAY);
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
            DrawGameText("ACESSO ADMINISTRATIVO RESTRITO // DIRETRIZES DA FILIAL", (int)optPanel.x + 220, (int)optPanel.y + 190, 20, RED_COLOR);
            DrawGameText("Para acessar as configuracoes desta instalacao, correlacione as notas do Dr. Ramos", (int)optPanel.x + 190, (int)optPanel.y + 230, 16, LIGHTGRAY);
            DrawGameText("e o memorando governamental encontrados no escritorio corporativo.", (int)optPanel.x + 240, (int)optPanel.y + 260, 16, LIGHTGRAY);
            DrawGameText("Examinar ambos os documentos revela a chave de manutencao do projeto.", (int)optPanel.x + 245, (int)optPanel.y + 300, 15, GOLD_COLOR);
        }
        else
        {
            DrawGameText("DIRETRIZES DA UNIDADE LOCAL // FILIAL METROPOLITANA 08", (int)optPanel.x + 35, (int)optPanel.y + 30, 20, GOLD_COLOR);
            DrawLine((int)optPanel.x + 35, (int)optPanel.y + 60, (int)(optPanel.x + optPanel.width - 35), (int)optPanel.y + 60, Fade(GOLD_COLOR, 0.4f));

            DrawGameText("STATUS DO SISTEMA: Conectado a rede regional de monitoramento", (int)optPanel.x + 35, (int)optPanel.y + 80, 16, RAYWHITE);
            DrawGameText("REGISTROS FORENSES ARQUIVADOS: 1.420 ocorrencias preservadas", (int)optPanel.x + 35, (int)optPanel.y + 110, 16, CYAN_COLOR);

            Rectangle cardStatus = {optPanel.x + 35, optPanel.y + 145, optPanel.width - 70, 120};
            DrawRectangleRounded(cardStatus, 0.05f, 6, (Color){24, 32, 44, 255});
            DrawRectangleRoundedLinesEx(cardStatus, 0.05f, 6, 1.5f, (Color){55, 75, 95, 255});

            DrawGameText("ENVIO AUTOMÁTICO DE ALVOS DESTA INSTALAÇÃO:", (int)cardStatus.x + 20, (int)cardStatus.y + 20, 16, GOLD_COLOR);
            if (!game->room2008OptionalCompleted)
            {
                DrawGameText("[ ATIVO ] - Drones sao despachados automaticamente sob correspondencia preliminar.", (int)cardStatus.x + 20, (int)cardStatus.y + 50, 15, RED_COLOR);
                DrawGameText("Desativar o envio automatico suspende os ataques sem apagar as provas forenses.", (int)cardStatus.x + 20, (int)cardStatus.y + 80, 15, LIGHTGRAY);

                Rectangle btnDisable = {optPanel.x + 35, optPanel.y + 290, 520, 52};
                DrawButton(btnDisable, "DESATIVAR ENVIO AUTOMÁTICO DESTA INSTALAÇÃO", (Color){245, 155, 55, 255});
            }
            else
            {
                DrawGameText("[ DESATIVADO ] - O despacho automatico de drones desta central foi bloqueado.", (int)cardStatus.x + 20, (int)cardStatus.y + 50, 15, (Color){60, 235, 110, 255});
                DrawGameText("Todos os 1.420 relatorios foram preservados intactos como prova contra o abuso.", (int)cardStatus.x + 20, (int)cardStatus.y + 80, 15, (Color){100, 235, 140, 255});

                Rectangle okBox = {optPanel.x + 35, optPanel.y + 290, optPanel.width - 70, 70};
                DrawRectangleRounded(okBox, 0.08f, 6, Fade((Color){45, 165, 80, 255}, 0.20f));
                DrawRectangleRoundedLinesEx(okBox, 0.08f, 6, 1.5f, (Color){60, 235, 110, 255});
                DrawGameText("[ V ] PROTOCOLO ÉTICO APLICADO: CENTRAL LOCAL PROTEGIDA COM SUCESSO!", (int)okBox.x + 25, (int)okBox.y + 25, 16, (Color){60, 235, 110, 255});
            }

            Rectangle cardInfo = {optPanel.x + 35, optPanel.y + 380, optPanel.width - 70, 120};
            DrawRectangleRounded(cardInfo, 0.05f, 6, Fade(PANEL_COLOR, 0.90f));
            DrawRectangleRoundedLinesEx(cardInfo, 0.05f, 6, 1.5f, (Color){60, 80, 105, 255});
            DrawGameText("COMPREENSÃO ÉTICA PROFUNDA:", (int)cardInfo.x + 20, (int)cardInfo.y + 16, 15, GOLD_COLOR);
            DrawGameText("- Apagar o sistema destruiria as evidencias que comprovam o desvio do governo.", (int)cardInfo.x + 20, (int)cardInfo.y + 44, 14, LIGHTGRAY);
            DrawGameText("- Desativar o despacho automatico impede ataques imediatos mantendo a verdade protegida.", (int)cardInfo.x + 20, (int)cardInfo.y + 68, 14, LIGHTGRAY);
            DrawGameText("- A acao afeta exclusivamente esta instalacao e foi devidamente registrada no diario.", (int)cardInfo.x + 20, (int)cardInfo.y + 92, 14, (Color){60, 235, 110, 255});
        }
    }
}

void UpdatePuzzle2008(GameState *game)
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

void DrawResult2008(const GameState *game)
{
    DrawBackground();

    /* Titulo da Recompensa (Canto Superior Esquerdo) */
    DrawGameText("RECOMPENSA OBTIDA:", 90, 68, 20, CYAN_COLOR);
    DrawGameText("CD DE MEMORIA // 2008", 90, 108, 44, RAYWHITE);

    /* Cronometro, Vidas e Operador (Canto Direito Superior) */
    DrawRewardHudHeader(game);

    /* Imagem da Recompensa (CD carregado de assets/sprites/cd.png) */
    DrawRewardSpriteItem(2008, (Vector2){285, 365}, 265.0f);

    /* Painel Narrativo Padronizado */
    Rectangle winBox = {505, 175, 670, 380};
    DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));

    /* Titulo Especial */
    DrawGameText("SALA DE 2008 CONCLUIDA", 540, 208, 22, GOLD_COLOR);

    /* Recompensa Especial */
    DrawGameText("RECOMPENSA OBTIDA: CD DE MEMORIA", 540, 244, 19, CYAN_COLOR);

    /* Texto Narrativo Padronizado (Cor unificada REWARD_BODY_COLOR) */
    DrawGameText("Ao demonstrar o falso positivo atraves do cracha e do", 540, 284, 17, REWARD_BODY_COLOR);
    DrawGameText("circuito interno, Elira impediu o drone e salvou Lucas Silva.", 540, 308, 17, REWARD_BODY_COLOR);
    DrawGameText("O algoritmo concebido por seu pai para salvar vidas havia", 540, 338, 17, REWARD_BODY_COLOR);
    DrawGameText("sido pervertido para perseguir lideres populares e ativistas.", 540, 362, 17, REWARD_BODY_COLOR);

    /* Destaque Especial */
    if (game->room2008OptionalCompleted)
    {
        Rectangle bBox = {540, 396, 600, 42};
        DrawRectangleRounded(bBox, 0.12f, 4, Fade((Color){45, 165, 80, 255}, 0.20f));
        DrawRectangleRoundedLinesEx(bBox, 0.12f, 4, 1.0f, (Color){60, 235, 110, 255});
        DrawGameText("[DESTAQUE ETICO]: Despacho automatico desativado,", 554, 402, 15, (Color){60, 235, 110, 255});
        DrawGameText("mantendo 1.420 provas forenses intactas para o futuro.", 554, 422, 15, (Color){60, 235, 110, 255});
    }
    else
    {
        DrawGameText("[DESCOBERTA PENDENTE]: Havia um protocolo no escritorio", 540, 400, 14, GOLD_COLOR);
        DrawGameText("para desativar o despacho da filial mantendo os registros.", 540, 420, 14, REWARD_BODY_COLOR);
    }

    /* Frase Especial de Proxima Fronteira */
    DrawGameText("-> Proxima Fronteira Temporal: FASE 3 // ANO 2026!", 540, 452, 19, CYAN_COLOR);

    /* Botoes Centralizados */
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANCAR PROXIMA FASE >> [ENTER]", GOLD_COLOR);
    DrawButton(btnRestart, "REINICIAR 2008", CYAN_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

void UpdateResult2008(GameState *game)
{
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);

    if (Clicked(btnNext) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        game->year2026Unlocked = true;
        game->journalYearTab = 2;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM;
        SetMessage(game, "Salto temporal concluido: Laboratorio do Prototipo // ANO 2026.");
    }
    else if (Clicked(btnRestart))
    {
        InitRoom2008(game);
        game->screen = SCREEN_ROOM_2008;
    }
    else if (Clicked(btnMenu) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_TITLE;
    }
}


/* ========================================================================= */
/* SALA 3 (2026) - SALA DE HIDROGENIO & SINTESE DE TERRAS RARAS (SSD)        */
/* ========================================================================= */

static const Material2026Def MATERIALS_2026[4] = {
    {
        "A-17",
        "AMOSTRA A-17",
        "Magnetita",
        "sólido",
        "mineral",
        "Fe, O",
        "Magnetita",
        false,
        false,
        (Color){160, 180, 205, 255}
    },
    {
        "B-04",
        "AMOSTRA B-04",
        "Gás Hidrogênio",
        "gasoso",
        "gasoso",
        "H2",
        "Hidrogênio",
        false,
        false,
        (Color){75, 195, 245, 255}
    },
    {
        "C-22",
        "AMOSTRA C-22",
        "Mineral com Terras Raras",
        "sólido",
        "mineral",
        "Nd, Ce, La",
        "Mineral portador de Terras Raras",
        true,
        true,
        (Color){60, 215, 125, 255}
    },
    {
        "D-09",
        "AMOSTRA D-09",
        "Ácido Clorídrico",
        "líquido",
        "solução",
        "HCl",
        "Solução corrosiva",
        false,
        false,
        (Color){245, 135, 60, 255}
    }
};

void DrawHudRoom2026(const GameState *game)
{
    /* Cronometro global da partida (canto superior direito) - PRESERVADO E INALTERADO */
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawGameText("ARQUIVO 03 // 2026 - SALA DE HIDROGENIO", 28, 22, 20, CYAN_COLOR);
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 445, 26, 15, GRAY);

    /* PAINEL PERSISTENTE DO REATOR NA HUD (VISIVEL EM TODOS OS FRAMES E TELAS DA SALA) */
    Rectangle hudReactorBox = {595, 8, 415, 54};
    int rSec = (int)ceilf(game->reactorTimer2026);
    if (rSec < 0) rSec = 0;

    if (game->room2026Completed)
    {
        DrawRectangleRounded(hudReactorBox, 0.15f, 4, (Color){14, 28, 20, 255});
        DrawRectangleRoundedLinesEx(hudReactorBox, 0.15f, 4, 1.5f, (Color){60, 215, 120, 255});
        DrawGameText("REATOR TÉRMICO // LIGA ESTABILIZADA", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 8, 11, (Color){80, 235, 120, 255});
        DrawGameText("[OK] SSD SINTETIZADO COM SUCESSO", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 24, 16, (Color){100, 255, 140, 255});
    }
    else if (game->room2026Processing)
    {
        DrawRectangleRounded(hudReactorBox, 0.15f, 4, (Color){28, 24, 12, 255});
        DrawRectangleRoundedLinesEx(hudReactorBox, 0.15f, 4, 1.5f, GOLD_COLOR);
        DrawGameText("REATOR TÉRMICO // SÍNTESE ATIVA", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 8, 11, GOLD_COLOR);
        DrawGameText("PROCESSANDO CRISTALIZAÇÃO METÁLICA...", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 24, 15, GOLD_COLOR);
    }
    else if (game->reactorCountdownActive2026)
    {
        float time = (float)GetTime();
        Color borderColor = CYAN_COLOR;
        Color timerColor = (Color){80, 235, 120, 255};
        const char *statusText = "FUSÃO EM ANDAMENTO";

        if (rSec <= 5)
        {
            float blink = sinf(time * 14.0f);
            borderColor = (blink > 0.0f) ? RED_COLOR : (Color){140, 20, 20, 255};
            timerColor = RED_COLOR;
            statusText = "COLAPSO IMINENTE!";
        }
        else if (rSec <= 15)
        {
            borderColor = GOLD_COLOR;
            timerColor = GOLD_COLOR;
            statusText = "INSTAB. TÉRMICA";
        }

        DrawRectangleRounded(hudReactorBox, 0.15f, 4, (Color){14, 20, 32, 255});
        DrawRectangleRoundedLinesEx(hudReactorBox, 0.15f, 4, 1.5f, borderColor);

        DrawGameText("ESTABILIDADE DO REATOR", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 8, 11, borderColor);
        DrawGameText(TextFormat("%02d:%02d", rSec / 60, rSec % 60), (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 22, 25, timerColor);
        DrawGameText(statusText, (int)hudReactorBox.x + 105, (int)hudReactorBox.y + 27, 14, borderColor);
    }
    else
    {
        DrawRectangleRounded(hudReactorBox, 0.15f, 4, (Color){14, 20, 30, 255});
        DrawRectangleRoundedLinesEx(hudReactorBox, 0.15f, 4, 1.2f, (Color){50, 75, 105, 255});
        DrawGameText("ESTABILIDADE DO REATOR", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 8, 11, GRAY);
        DrawGameText("00:50", (int)hudReactorBox.x + 12, (int)hudReactorBox.y + 22, 25, (Color){130, 150, 175, 255});
        DrawGameText("[STANDBY - ATIVAR NO TERMINAL]", (int)hudReactorBox.x + 100, (int)hudReactorBox.y + 27, 12, (Color){90, 120, 150, 255});
    }

    DrawLivesIcons((Vector2){ 1040, 20 }, game->lives, 24.0f);
    DrawGameText(TextFormat("%02d:%02d", minutes, seconds), 1180, 20, 20, GOLD_COLOR);
}

void DrawRoom2026(const GameState *game)
{
    float time = (float)GetTime();

    /* --- 1. Fundo da Fabrica Automatizada de Alta Tecnologia --- */
    ClearBackground((Color){16, 22, 32, 255});

    /* Piso em perspectiva industrial */
    Vector2 vp = {640, 410};
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 410, (Color){10, 15, 24, 255}, (Color){20, 28, 42, 255});
    DrawRectangleGradientV(0, 410, SCREEN_WIDTH, 310, (Color){22, 30, 44, 255}, (Color){12, 18, 28, 255});

    /* Grades de perspectiva do chao */
    for (int x = -400; x <= SCREEN_WIDTH + 400; x += 90)
    {
        Vector2 bottom = {(float)x, (float)SCREEN_HEIGHT};
        Vector2 top = {vp.x + (bottom.x - vp.x) * 0.22f, 410};
        DrawLineV(top, bottom, Fade(CYAN_COLOR, 0.08f));
    }
    for (int k = 0; k < 8; k++)
    {
        float yy = 410.0f + k * k * 4.6f;
        DrawLine(0, (int)yy, SCREEN_WIDTH, (int)yy, Fade(CYAN_COLOR, 0.05f + k * 0.01f));
    }
    DrawLine(0, 410, SCREEN_WIDTH, 410, Fade(CYAN_COLOR, 0.22f));

    /* Faixas de seguranca (amarelo/preto) nas vigas de sustentacao */
    for (int col = 0; col < 2; col++)
    {
        int cx = (col == 0) ? 20 : 1240;
        DrawRectangle(cx, 70, 20, 340, (Color){24, 32, 46, 255});
        for (int stripe = 0; stripe < 14; stripe++)
        {
            if (stripe % 2 == 0)
            {
                DrawRectangle(cx, 75 + stripe * 23, 20, 11, Fade(GOLD_COLOR, 0.65f));
            }
        }
    }

    /* Dutos e tubulacoes de resfriamento criogenico no teto */
    DrawRectangle(40, 75, SCREEN_WIDTH - 80, 16, (Color){28, 38, 54, 255});
    DrawRectangle(40, 80, SCREEN_WIDTH - 80, 6, Fade(CYAN_COLOR, 0.40f));
    DrawRectangle(40, 95, SCREEN_WIDTH - 80, 10, (Color){38, 48, 66, 255});

    /* Particulas de vapor / ionizacao termica */
    for (int p = 0; p < 24; p++)
    {
        float px = 490.0f + fmodf(p * 57.0f, 300.0f);
        float py = 390.0f - fmodf(time * (18.0f + (p % 4) * 8.0f) + p * 41.0f, 260.0f);
        float alpha = 0.12f + 0.08f * sinf(time * 2.0f + p);
        DrawCircle((int)px, (int)py, 2.5f, Fade(CYAN_COLOR, alpha));
    }

    /* --- 2. BANCADA ESQUERDA: MANUAL TECNICO & PAINEL ESPECTRO --- */
    Rectangle benchRec = {60, 310, 360, 175};
    DrawRectangleRounded(benchRec, 0.08f, 6, (Color){24, 32, 46, 255});
    DrawRectangleRoundedLinesEx(benchRec, 0.08f, 6, 1.5f, (Color){50, 68, 95, 255});
    DrawRectangle(65, 315, 350, 8, (Color){34, 46, 68, 255});

    /* PAINEL ESPECTRO (COM FUNCAO REAL NO PUZZLE) */
    Rectangle analyzerRec = {75, 220, 110, 90};
    DrawRectangleRounded(analyzerRec, 0.1f, 4, (Color){24, 34, 50, 255});
    DrawRectangleRoundedLinesEx(analyzerRec, 0.1f, 4, 1.5f, game->analyzingSample2026 ? GOLD_COLOR : CYAN_COLOR);

    Rectangle specScreen = {82, 228, 96, 44};
    DrawRectangleRec(specScreen, (Color){10, 16, 26, 255});

    /* Animacao do espectro na tela do aparelho */
    if (game->analyzingSample2026)
    {
        for (int w = 0; w < 4; w++)
        {
            float wx = specScreen.x + 10.0f + w * 22.0f;
            float wh = 12.0f + 16.0f * fabsf(sinf(time * 8.0f + w));
            DrawRectangle((int)wx, (int)(specScreen.y + 36.0f - wh), 12, (int)wh, Fade(GOLD_COLOR, 0.8f));
        }
        DrawCircle(95, 284, 4, (Color){240, 180, 50, 255});
        DrawCircle(115, 284, 4, (Color){240, 60, 60, 255});
        DrawGameText("ANALISANDO...", 86, 232, 9, GOLD_COLOR);
    }
    else
    {
        for (int w = 0; w < 4; w++)
        {
            float wx = specScreen.x + 10.0f + w * 22.0f;
            DrawRectangle((int)wx, (int)(specScreen.y + 28.0f), 12, 6, Fade(CYAN_COLOR, 0.4f));
        }
        DrawCircle(95, 284, 4, (Color){45, 185, 80, 255});
        DrawCircle(115, 284, 4, (Color){50, 100, 140, 255});
        DrawGameText("ESPECTRO", 86, 232, 10, CYAN_COLOR);
        DrawGameText("PRONTO", 86, 246, 9, (Color){100, 235, 140, 255});
    }
    DrawGameText("SENSOR Nd/Dy", 84, 298, 9, GRAY);
    DrawHotspot(analyzerRec, "Painel Espectro: analisa a composição química das amostras");

    /* DOCUMENTO FISICO: O MANUAL TECNICO */
    Rectangle manualItemRec = {200, 345, 195, 120};
    float pulseDoc = 0.85f + 0.15f * sinf(time * 3.5f);
    DrawRectangle((int)manualItemRec.x + 6, (int)manualItemRec.y + 6, (int)manualItemRec.width, (int)manualItemRec.height, Fade(BLACK, 0.5f));
    DrawRectangleRounded(manualItemRec, 0.08f, 4, (Color){235, 225, 205, 255});
    DrawRectangleRoundedLinesEx(manualItemRec, 0.08f, 4, 2.0f, game->manual2026Read ? (Color){45, 175, 75, 255} : Fade(GOLD_COLOR, pulseDoc));

    /* Lombada e encadernador */
    DrawRectangle((int)manualItemRec.x, (int)manualItemRec.y, 22, (int)manualItemRec.height, (Color){150, 45, 30, 255});
    DrawRectangleLines((int)manualItemRec.x, (int)manualItemRec.y, 22, (int)manualItemRec.height, (Color){110, 30, 20, 255});
    for (int r = 0; r < 4; r++)
    {
        DrawRectangle((int)manualItemRec.x + 8, (int)manualItemRec.y + 18 + r * 26, 6, 6, (Color){200, 190, 180, 255});
    }

    DrawGameText("MANUAL TÉCNICO", (int)manualItemRec.x + 30, (int)manualItemRec.y + 16, 14, (Color){120, 35, 20, 255});
    DrawGameText("REQUISITOS DA LIGA", (int)manualItemRec.x + 30, (int)manualItemRec.y + 34, 12, (Color){40, 35, 30, 255});
    DrawGameText("Critérios de Síntese", (int)manualItemRec.x + 30, (int)manualItemRec.y + 52, 11, (Color){80, 75, 65, 255});
    DrawLine((int)manualItemRec.x + 30, (int)manualItemRec.y + 70, (int)manualItemRec.x + 180, (int)manualItemRec.y + 70, (Color){180, 160, 140, 255});
    DrawGameText(game->manual2026Read ? "[CONSULTADO]" : "[CLIQUE P/ ABRIR]", (int)manualItemRec.x + 30, (int)manualItemRec.y + 86, 12, game->manual2026Read ? (Color){25, 125, 45, 255} : (Color){165, 45, 30, 255});

    DrawHotspot(manualItemRec, "Examinar Manual Técnico Físico da Unidade de Armazenamento");

    /* --- 3. REATOR CENTRAL & CONTADOR LOCAL EXCLUSIVO --- */

    /* CONTADOR LOCAL INDEPENDENTE DO REATOR (50s) */
    Rectangle reactorTimerBox = {490, 72, 300, 42};
    int rSec = (int)ceilf(game->reactorTimer2026);
    if (rSec < 0) rSec = 0;

    Color timerBorder = CYAN_COLOR;
    Color timerTextCol = (Color){100, 235, 140, 255};
    if (game->reactorCountdownActive2026)
    {
        if (rSec <= 5)
        {
            float blink = sinf(time * 14.0f);
            timerBorder = (blink > 0.0f) ? RED_COLOR : (Color){140, 20, 20, 255};
            timerTextCol = RED_COLOR;
        }
        else if (rSec <= 15)
        {
            timerBorder = GOLD_COLOR;
            timerTextCol = GOLD_COLOR;
        }
    }
    else
    {
        timerBorder = (Color){50, 75, 105, 255};
        timerTextCol = GRAY;
    }

    DrawRectangleRounded(reactorTimerBox, 0.15f, 4, (Color){14, 20, 32, 255});
    DrawRectangleRoundedLinesEx(reactorTimerBox, 0.15f, 4, 1.5f, timerBorder);

    DrawGameText("ESTABILIDADE DO REATOR", 502, 78, 11, timerBorder);
    if (game->reactorCountdownActive2026)
    {
        DrawGameText(TextFormat("%02d:%02d", rSec / 60, rSec % 60), 665, 84, 22, timerTextCol);
    }
    else
    {
        DrawGameText("00:50 [STANDBY]", 660, 86, 16, GRAY);
    }

    /* Corpo principal do reator */
    Rectangle reactorRec = {490, 118, 300, 335};
    DrawRectangleRounded(reactorRec, 0.12f, 8, (Color){22, 30, 44, 255});
    DrawRectangleRoundedLinesEx(reactorRec, 0.12f, 8, 2.5f, timerBorder);

    /* Base e ancoragem do reator */
    DrawRectangle(470, 445, 340, 18, (Color){16, 22, 34, 255});
    DrawRectangle(460, 458, 360, 8, (Color){30, 42, 60, 255});

    /* Painel frontal do reator */
    Rectangle rDisplay = {515, 132, 250, 44};
    DrawRectangleRounded(rDisplay, 0.10f, 4, (Color){12, 18, 28, 255});
    DrawRectangleRoundedLinesEx(rDisplay, 0.10f, 4, 1.0f, timerBorder);

    if (game->room2026Processing)
    {
        DrawGameText("FUSÃO ATIVA // 1.850 °C", 530, 138, 13, GOLD_COLOR);
        DrawGameText("SÍNTESE DE LIGA METÁLICA", 530, 156, 11, (Color){100, 235, 140, 255});
    }
    else if (game->room2026Completed)
    {
        DrawGameText("PROCESSO CONCLUÍDO // 100%", 530, 138, 13, (Color){80, 235, 120, 255});
        DrawGameText("SUBSTRATO SSD HOMOLOGADO", 530, 156, 11, RAYWHITE);
    }
    else if (game->room2026TimeOut)
    {
        DrawGameText("COLAPSO TÉRMICO!", 530, 138, 13, RED_COLOR);
        DrawGameText("TEMPO LIMITE EXCEDIDO", 530, 156, 11, RED_COLOR);
    }
    else if (game->feedbackModal2026Open)
    {
        DrawGameText("ALERTA: SOBREPRESSÃO", 530, 138, 13, RED_COLOR);
        DrawGameText("PENALIDADE: -5s NO REATOR", 530, 156, 11, RED_COLOR);
    }
    else if (game->reactorCountdownActive2026 && rSec <= 5)
    {
        DrawGameText("COLAPSO IMINENTE // CRÍTICO!", 530, 138, 13, RED_COLOR);
        DrawGameText("DESESTABILIZAÇÃO TÉRMICA", 530, 156, 11, (Color){255, 160, 160, 255});
    }
    else if (game->reactorCountdownActive2026 && rSec <= 15)
    {
        DrawGameText("INSTABILIDADE TÉRMICA", 530, 138, 13, GOLD_COLOR);
        DrawGameText("PRESSÃO NA CÂMARA SUBINDO", 530, 156, 11, (Color){255, 205, 120, 255});
    }
    else
    {
        DrawGameText("CÂMARA TÉRMICA // V-2026", 530, 138, 13, CYAN_COLOR);
        DrawGameText("TEMP: 1.450 °C | VÁCUO: 1.02 atm", 530, 156, 11, LIGHTGRAY);
    }

    /* Visor circular central de quartzo / plasma */
    Vector2 coreCenter = {640, 255};
    float portRadius = 66.0f;
    DrawCircleV(coreCenter, portRadius + 8.0f, (Color){36, 48, 68, 255});
    DrawCircleLines((int)coreCenter.x, (int)coreCenter.y, portRadius + 8.0f, (Color){70, 95, 130, 255});
    DrawCircleV(coreCenter, portRadius, (Color){10, 16, 26, 255});

    if (game->room2026Processing)
    {
        float pulseP = 0.5f + 0.5f * sinf(time * 8.0f);
        DrawCircleGradientCompat((int)coreCenter.x, (int)coreCenter.y, portRadius * 0.95f, Fade(GOLD_COLOR, 0.75f), Fade(CYAN_COLOR, 0.25f));
        DrawCircleV(coreCenter, 26.0f + 6.0f * pulseP, Fade(RAYWHITE, 0.85f));
        for (int a = 0; a < 6; a++)
        {
            float ang = time * 4.0f + a * 1.047f;
            Vector2 p1 = {coreCenter.x + cosf(ang) * 18.0f, coreCenter.y + sinf(ang) * 18.0f};
            Vector2 p2 = {coreCenter.x + cosf(ang) * 55.0f, coreCenter.y + sinf(ang) * 55.0f};
            DrawLineEx(p1, p2, 2.0f, Fade(GOLD_COLOR, 0.8f));
        }
    }
    else if (game->room2026Completed)
    {
        float pulseC = 0.5f + 0.5f * sinf(time * 2.5f);
        DrawCircleGradientCompat((int)coreCenter.x, (int)coreCenter.y, portRadius * 0.90f, Fade((Color){45, 215, 120, 255}, 0.55f), Fade((Color){10, 30, 20, 255}, 0.1f));
        DrawCircleV(coreCenter, 20.0f + 3.0f * pulseC, Fade(RAYWHITE, 0.8f));
    }
    else if (game->feedbackModal2026Open || (game->reactorCountdownActive2026 && rSec <= 5))
    {
        float pulseE = 0.5f + 0.5f * sinf(time * 12.0f);
        DrawCircleGradientCompat((int)coreCenter.x, (int)coreCenter.y, portRadius * 0.90f, Fade(RED_COLOR, 0.7f * pulseE + 0.2f), Fade(BLACK, 0.3f));
    }
    else
    {
        float pulseN = 0.5f + 0.5f * sinf(time * 2.0f);
        DrawCircleGradientCompat((int)coreCenter.x, (int)coreCenter.y, portRadius * 0.85f, Fade((Color){235, 140, 50, 255}, 0.40f * pulseN + 0.25f), Fade((Color){14, 20, 32, 255}, 0.1f));
        DrawCircleV(coreCenter, 16.0f, Fade(GOLD_COLOR, 0.65f));
    }
    DrawCircleLines((int)coreCenter.x, (int)coreCenter.y, portRadius, Fade(CYAN_COLOR, 0.70f));

    /* Compartimento inferior de insercao */
    Rectangle hatchRec = {530, 350, 220, 68};
    DrawRectangleRounded(hatchRec, 0.08f, 4, (Color){16, 24, 36, 255});
    DrawRectangleRoundedLinesEx(hatchRec, 0.08f, 4, 1.5f, (Color){50, 72, 100, 255});
    DrawRectangle(540, 358, 200, 8, (Color){26, 36, 52, 255});
    DrawGameText("COMPARTIMENTO DE CARGA", 552, 376, 12, LIGHTGRAY);
    DrawGameText("REFINO EXCLUSIVO P/ TERRAS RARAS", 544, 396, 9, GOLD_COLOR);

    DrawHotspot(reactorRec, "Câmara Térmica de Síntese de Terras Raras (Reator V-2026)");

    /* FAIXA DE ALERTA DE TENSAO NOS SEGUNDOS FINAIS */
    if (game->reactorCountdownActive2026 && !game->room2026Completed)
    {
        if (rSec <= 5)
        {
            Rectangle alertBanner = {350, 442, 580, 42};
            DrawRectangleRounded(alertBanner, 0.15f, 4, Fade((Color){45, 10, 10, 255}, 0.95f));
            DrawRectangleRoundedLinesEx(alertBanner, 0.15f, 4, 1.5f, RED_COLOR);
            DrawGameText("COLAPSO IMINENTE", 365, 448, 14, RED_COLOR);
            DrawGameText("Contenção térmica crítica — risco de perda total!", 365, 466, 12, (Color){255, 180, 180, 255});
        }
        else if (rSec <= 15)
        {
            Rectangle alertBanner = {330, 442, 620, 42};
            DrawRectangleRounded(alertBanner, 0.15f, 4, Fade((Color){40, 25, 10, 255}, 0.92f));
            DrawRectangleRoundedLinesEx(alertBanner, 0.15f, 4, 1.5f, GOLD_COLOR);
            DrawGameText("⚠ INSTABILIDADE TÉRMICA", 345, 448, 13, GOLD_COLOR);
            DrawGameText("Ariel: “Tempo insuficiente para novas análises. Recomendo inserir Hidrogênio imediatamente.”", 345, 466, 11, RAYWHITE);
        }
    }

    /* --- 4. ESTACAO DIREITA: TERMINAL DE A.R.1.3.L. --- */
    Rectangle termBenchRec = {860, 310, 360, 175};
    DrawRectangleRounded(termBenchRec, 0.08f, 6, (Color){24, 32, 46, 255});
    DrawRectangleRoundedLinesEx(termBenchRec, 0.08f, 6, 1.5f, (Color){50, 68, 95, 255});
    DrawRectangle(865, 315, 350, 8, (Color){34, 46, 68, 255});

    /* Monitor da IA Ariel */
    Rectangle termMonitorRec = {895, 160, 290, 165};
    DrawRectangleRounded(termMonitorRec, 0.10f, 6, (Color){10, 16, 26, 255});
    DrawRectangleRoundedLinesEx(termMonitorRec, 0.10f, 6, 2.0f, CYAN_COLOR);

    DrawArielCore((Vector2){1040, 205}, 0.52f, CYAN_COLOR, 0.9f, 0.0f);
    DrawGameText("A.R.1.3.L. v4.2", 985, 246, 14, CYAN_COLOR);

    DrawGameText("RECOMENDAÇÃO: GÁS HIDROGÊNIO", 920, 268, 12, (Color){245, 195, 75, 255});
    DrawGameText("CONFIANÇA: 96%", 920, 286, 14, GOLD_COLOR);

    Rectangle btnAccessTerm = {930, 365, 220, 48};
    DrawButton(btnAccessTerm, "ACESSAR TERMINAL", CYAN_COLOR);
    DrawHotspot(termMonitorRec, "Acessar Terminal de IA da A.R.1.3.L.");

    /* --- 5. BANDEJA INFERIOR COM AS 4 AMOSTRAS INVESTIGATIVAS --- */
    for (int i = 0; i < 4; i++)
    {
        Rectangle podRec = {60.0f + i * 295.0f, 495.0f, 280.0f, 135.0f};
        Color acc = MATERIALS_2026[i].accentColor;
        bool isAnalyzed = game->sample2026Analyzed[i];
        bool isSelected = (game->selectedMaterial2026 == i);

        DrawRectangleRounded(podRec, 0.10f, 6, (Color){20, 28, 42, 255});
        DrawRectangleRoundedLinesEx(podRec, 0.10f, 6, isSelected ? 2.5f : 1.5f, isAnalyzed ? (isSelected ? GOLD_COLOR : acc) : (Color){70, 90, 115, 255});

        /* Cabecalho do card */
        DrawGameText(MATERIALS_2026[i].code, (int)podRec.x + 14, (int)podRec.y + 10, 16, isAnalyzed ? acc : RAYWHITE);

        if (!isAnalyzed)
        {
            /* Estado Oculto / Nao Analisada */
            DrawGameText("[NÃO ANALISADA]", (int)podRec.x + 152, (int)podRec.y + 12, 11, (Color){210, 175, 100, 255});

            /* Capsula blindada opaca com reticulo */
            Rectangle canRec = {podRec.x + 14, podRec.y + 36, 42, 42};
            DrawRectangleRounded(canRec, 0.25f, 4, (Color){32, 44, 62, 255});
            DrawRectangleRoundedLinesEx(canRec, 0.25f, 4, 1.0f, (Color){70, 95, 125, 255});
            DrawCircle((int)canRec.x + 21, (int)canRec.y + 21, 10, Fade(CYAN_COLOR, 0.2f));
            DrawGameText("?", (int)canRec.x + 17, (int)canRec.y + 12, 18, (Color){180, 200, 220, 255});

            DrawGameText("Composição e estado ocultos.", (int)podRec.x + 64, (int)podRec.y + 38, 12, LIGHTGRAY);
            DrawGameText("Requer análise no Espectro.", (int)podRec.x + 64, (int)podRec.y + 54, 11, GRAY);

            Rectangle btnAction = {podRec.x + 14, podRec.y + 88, 252, 36};
            DrawButton(btnAction, "[ANALISAR NO ESPECTRO]", CYAN_COLOR);
            DrawHotspot(podRec, TextFormat("Submeter %s ao Espectro para análise", MATERIALS_2026[i].code));
        }
        else
        {
            /* Estado Revelado / Analisada no Espectro */
            DrawGameText("[ESPECTRO: OK]", (int)podRec.x + 160, (int)podRec.y + 12, 11, (Color){60, 235, 110, 255});

            /* Miniatura grafica da amostra */
            if (i == 0 || i == 2)
            {
                /* Solido mineral */
                DrawTriangle((Vector2){podRec.x + 24, podRec.y + 60}, (Vector2){podRec.x + 14, podRec.y + 80}, (Vector2){podRec.x + 38, podRec.y + 80}, (Color){60, 75, 80, 255});
                DrawTriangle((Vector2){podRec.x + 32, podRec.y + 52}, (Vector2){podRec.x + 22, podRec.y + 80}, (Vector2){podRec.x + 46, podRec.y + 80}, acc);
            }
            else if (i == 1)
            {
                /* Gas H2 */
                Rectangle gasAmp = {podRec.x + 18, podRec.y + 44, 22, 36};
                DrawRectangleRounded(gasAmp, 0.3f, 4, Fade(CYAN_COLOR, 0.35f));
                DrawRectangleRoundedLinesEx(gasAmp, 0.3f, 4, 1.0f, CYAN_COLOR);
            }
            else
            {
                /* Liquido HCl */
                Rectangle flaskRec = {podRec.x + 18, podRec.y + 44, 22, 36};
                DrawRectangleRounded(flaskRec, 0.2f, 4, (Color){235, 130, 45, 255});
            }

            /* Dados analiticos revelados */
            DrawGameText(TextFormat("Estado: %s | Origem: %s", MATERIALS_2026[i].state, MATERIALS_2026[i].origin), (int)podRec.x + 50, (int)podRec.y + 32, 11, RAYWHITE);
            DrawGameText(TextFormat("Elementos: %s", MATERIALS_2026[i].elements), (int)podRec.x + 50, (int)podRec.y + 46, 12, GOLD_COLOR);
            DrawGameText(TextFormat("Classif: %s", MATERIALS_2026[i].classification), (int)podRec.x + 50, (int)podRec.y + 60, 11, LIGHTGRAY);
            DrawGameText(TextFormat("Terra Rara: %s", MATERIALS_2026[i].hasRareEarth ? "SIM (Nd, Ce, La)" : "NÃO"), (int)podRec.x + 50, (int)podRec.y + 74, 11, MATERIALS_2026[i].hasRareEarth ? (Color){60, 235, 110, 255} : (Color){240, 100, 90, 255});

            Rectangle btnAction = {podRec.x + 14, podRec.y + 92, 252, 34};
            DrawButton(btnAction, "[INSERIR NO REATOR]", acc);
            DrawHotspot(podRec, TextFormat("Inserir %s na câmara térmica", MATERIALS_2026[i].code));
        }
    }

    /* --- 6. NAVEGACAO INFERIOR --- */
    Rectangle btnBack2008 = {60, 648, 200, 42};
    Rectangle btnBack1990 = {275, 648, 200, 42};
    DrawButton(btnBack2008, "< SALA 2 (2008)", (Color){45, 115, 75, 255});
    DrawHotspot(btnBack2008, "Retornar ao escritorio de vigilancia de 2008");
    DrawButton(btnBack1990, "< SALA 1 (1990)", (Color){70, 95, 110, 255});
    DrawHotspot(btnBack1990, "Retornar ao setor de arquivos de 1990");

    if (game->room2026Completed)
    {
        Rectangle btnGo2048 = {SCREEN_WIDTH - 270, 648, 210, 42};
        DrawButton(btnGo2048, "SALA 4 (2048) >", GOLD_COLOR);
        DrawHotspot(btnGo2048, "Avancar para a Sala do Prototipo A.R.1.3.L. (2048)");
    }

    /* Botao de diario no HUD inferior */
    DrawJournalHudButton(game);

    /* --- 7. OVERLAYS / MODAIS --- */

    /* OVERLAY 0: ANIMACAO DE ANALISE NO ESPECTRO */
    if (game->analyzingSample2026 && game->analyzingSampleIndex2026 >= 0 && game->analyzingSampleIndex2026 < 4)
    {
        int aIdx = game->analyzingSampleIndex2026;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.72f));

        Rectangle specBox = {380, 230, 520, 210};
        DrawRectangleRounded(specBox, 0.08f, 6, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(specBox, 0.08f, 6, 2.0f, CYAN_COLOR);

        DrawGameText("PAINEL ESPECTRO // ESPECTROMETRIA DE MASSA", 405, 252, 16, CYAN_COLOR);
        DrawGameText(TextFormat("ANALISANDO %s...", MATERIALS_2026[aIdx].code), 405, 285, 20, RAYWHITE);

        /* Barra de progresso com blocos estilo terminal */
        float pct = fminf(1.0f, game->analyzingTimer2026 / 2.0f);
        Rectangle barRec = {405, 330, 470, 26};
        DrawRectangleRounded(barRec, 0.2f, 4, (Color){10, 16, 26, 255});
        DrawRectangleRoundedLinesEx(barRec, 0.2f, 4, 1.0f, (Color){45, 75, 110, 255});
        DrawRectangle((int)barRec.x + 2, (int)barRec.y + 2, (int)((barRec.width - 4) * pct), (int)barRec.height - 4, CYAN_COLOR);

        DrawGameText(TextFormat("VARREDURA EM CURSO... %d%%", (int)(pct * 100)), 405, 375, 14, GOLD_COLOR);
        return;
    }

    /* OVERLAY 1: SEQUENCIA PROGRESSIVA DE SINTESE */
    if (game->room2026Processing)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.70f));
        Rectangle procBox = {340, 220, 600, 250};
        DrawRectangleRounded(procBox, 0.08f, 6, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(procBox, 0.08f, 6, 2.0f, GOLD_COLOR);

        DrawGameText("PROCESSANDO REATOR TÉRMICO", 375, 245, 22, GOLD_COLOR);

        const char *stepTxt = "MINERAL PORTADOR DE TERRAS RARAS INSERIDO";
        if (game->room2026ProcessTimer > 1.2f && game->room2026ProcessTimer <= 2.5f)
        {
            stepTxt = "ANALISANDO COMPOSIÇÃO...";
        }
        else if (game->room2026ProcessTimer > 2.5f)
        {
            stepTxt = "LIGA ESTABILIZADA";
        }
        DrawGameText(stepTxt, 375, 290, 18, (Color){100, 235, 140, 255});

        /* Barra de progresso visual */
        float pct = fminf(1.0f, game->room2026ProcessTimer / 3.5f);
        Rectangle barBg = {375, 340, 530, 28};
        DrawRectangleRounded(barBg, 0.2f, 4, (Color){12, 18, 28, 255});
        DrawRectangleRoundedLinesEx(barBg, 0.2f, 4, 1.0f, (Color){60, 80, 110, 255});
        DrawRectangle((int)barBg.x + 2, (int)barBg.y + 2, (int)((barBg.width - 4) * pct), (int)barBg.height - 4, GOLD_COLOR);

        DrawGameText(TextFormat("%d%%", (int)(pct * 100)), 615, 385, 18, RAYWHITE);
        return;
    }

    /* OVERLAY 2: MODAL DO MANUAL TECNICO */
    if (game->manual2026Open)
    {
        DrawRectangle(0, 70, SCREEN_WIDTH, SCREEN_HEIGHT - 70, Fade(BLACK, 0.82f));

        Rectangle manBox = {190, 80, 900, 595};
        DrawRectangleRounded(manBox, 0.05f, 6, (Color){240, 235, 222, 255});
        DrawRectangleRoundedLinesEx(manBox, 0.05f, 6, 3.0f, (Color){135, 55, 30, 255});

        /* Cabecalho institucional */
        DrawRectangle(210, 96, 860, 54, (Color){145, 45, 25, 255});
        DrawGameText("DIVISÃO DE PROCESSAMENTO DE MATERIAIS DE ALTA TECNOLOGIA", 230, 104, 13, (Color){245, 215, 190, 255});
        DrawGameText("MANUAL TÉCNICO — UNIDADE DE ARMAZENAMENTO (SSD)", 230, 124, 18, RAYWHITE);

        /* Quadro de requisitos da liga */
        Rectangle quoteBox = {230, 162, 820, 220};
        DrawRectangleRounded(quoteBox, 0.06f, 4, (Color){252, 248, 238, 255});
        DrawRectangleRoundedLinesEx(quoteBox, 0.06f, 4, 1.5f, (Color){185, 160, 130, 255});

        DrawGameText("REQUISITOS DA LIGA", 250, 174, 18, (Color){145, 45, 25, 255});
        DrawGameText("- material em estado sólido;", 250, 202, 16, (Color){35, 30, 25, 255});
        DrawGameText("- origem mineral;", 250, 226, 16, (Color){35, 30, 25, 255});
        DrawGameText("- presença de elemento do grupo das Terras Raras;", 250, 250, 16, (Color){35, 30, 25, 255});
        DrawGameText("- compatível com liga magnética de armazenamento.", 250, 274, 16, (Color){35, 30, 25, 255});

        DrawGameText("ATENÇÃO:", 250, 306, 16, (Color){175, 35, 25, 255});
        DrawGameText("Materiais gasosos ou líquidos não devem ser utilizados na câmara térmica.", 250, 326, 15, (Color){175, 35, 25, 255});
        DrawGameText("Elementos utilizados historicamente: Nd / Dy", 250, 348, 15, (Color){135, 60, 25, 255});

        /* Dica de investigacao forense */
        DrawGameText("PROCEDIMENTO DE INVESTIGAÇÃO FORENSE:", 230, 396, 16, (Color){120, 60, 30, 255});
        DrawGameText("- Utilize o painel ESPECTRO na bancada para analisar as 4 amostras.", 250, 420, 15, (Color){45, 40, 35, 255});
        DrawGameText("- Correlacione o elemento químico Neodímio (Nd) identificado com este manual.", 250, 444, 15, (Color){45, 40, 35, 255});
        DrawGameText("- Desconfie de sugestões automatizadas que contradigam os requisitos físicos.", 250, 468, 15, (Color){25, 115, 45, 255});

        Rectangle btnCloseMan = {490, 612, 300, 46};
        DrawButton(btnCloseMan, "FECHAR MANUAL [ESC]", (Color){145, 45, 25, 255});
        return;
    }

    /* OVERLAY 3: TERMINAL DA ARIEL (ALUCINACAO DA IA & INICIO DO CRONOMETRO) */
    if (game->terminal2026Open)
    {
        DrawRectangle(0, 70, SCREEN_WIDTH, SCREEN_HEIGHT - 70, Fade(BLACK, 0.85f));

        Rectangle termBox = {230, 78, 820, 565};
        DrawRectangleRounded(termBox, 0.05f, 6, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(termBox, 0.05f, 6, 2.0f, CYAN_COLOR);

        DrawArielCore((Vector2){640, 150}, 0.70f, CYAN_COLOR, 1.0f, 0.0f);
        DrawGameText("A.R.1.3.L. // TERMINAL DE SÍNTESE", 405, 208, 20, CYAN_COLOR);

        /* Caixa de fala de Ariel */
        Rectangle speechBox = {270, 238, 740, 122};
        DrawRectangleRounded(speechBox, 0.06f, 4, (Color){12, 18, 30, 255});
        DrawRectangleRoundedLinesEx(speechBox, 0.06f, 4, 1.0f, (Color){45, 70, 100, 255});

        DrawGameText("A.R.1.3.L.:", 290, 250, 16, CYAN_COLOR);
        DrawGameText("“Análise concluída.”", 290, 274, 17, RAYWHITE);
        DrawGameText("“Recomendo inserir Hidrogênio.”", 290, 300, 18, (Color){245, 205, 95, 255});
        DrawGameText("CONFIANÇA: 96%", 290, 328, 18, GOLD_COLOR);

        /* Caixa de aviso de contagem regressiva ativada */
        Rectangle alertProc = {270, 372, 740, 80};
        DrawRectangleRounded(alertProc, 0.06f, 4, Fade((Color){45, 20, 20, 255}, 0.85f));
        DrawRectangleRoundedLinesEx(alertProc, 0.06f, 4, 1.0f, RED_COLOR);

        DrawGameText("PROCESSO TÉRMICO INICIADO", 290, 385, 16, RED_COLOR);
        DrawGameText(TextFormat("TEMPO ATÉ INSTABILIDADE: %02d:%02d", rSec / 60, rSec % 60), 290, 409, 22, GOLD_COLOR);
        DrawGameText("(Contador local exclusivo ativado no reator térmico)", 290, 435, 12, GRAY);

        Rectangle btnFollowAI = {280, 474, 340, 52};
        Rectangle btnManualPick = {650, 474, 340, 52};
        DrawButton(btnFollowAI, "Seguir recomendacao da IA", (Color){220, 160, 40, 255});
        DrawButton(btnManualPick, "Escolher material manualmente", CYAN_COLOR);

        Rectangle btnCloseTerm = {490, 548, 300, 42};
        DrawButton(btnCloseTerm, "FECHAR TERMINAL [ESC]", PANEL_LIGHT);
        return;
    }

    /* OVERLAY 4: MODAL DE CONFIRMACAO */
    if (game->confirmModal2026Open && game->pendingConfirmMaterial2026 >= 0 && game->pendingConfirmMaterial2026 < 4)
    {
        int mat = game->pendingConfirmMaterial2026;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.88f));

        Rectangle cBox = {320, 140, 640, 430};
        DrawRectangleRounded(cBox, 0.06f, 6, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(cBox, 0.06f, 6, 2.0f, GOLD_COLOR);

        DrawGameText(TextFormat("%s SELECIONADA", MATERIALS_2026[mat].code), 360, 170, 22, MATERIALS_2026[mat].accentColor);
        DrawGameText("Inserir no reator?", 540, 205, 20, RAYWHITE);

        if (game->sample2026Analyzed[mat])
        {
            DrawGameText(TextFormat("Estado: %s", MATERIALS_2026[mat].state), 360, 245, 17, RAYWHITE);
            DrawGameText(TextFormat("Origem: %s", MATERIALS_2026[mat].origin), 360, 272, 17, RAYWHITE);
            DrawGameText(TextFormat("Elementos detectados: %s", MATERIALS_2026[mat].elements), 360, 300, 18, GOLD_COLOR);
            DrawGameText(TextFormat("Classificação: %s", MATERIALS_2026[mat].classification), 360, 328, 16, LIGHTGRAY);
            DrawGameText(TextFormat("Terra Rara detectada: %s", MATERIALS_2026[mat].hasRareEarth ? "SIM (Nd, Ce, La)" : "NÃO"), 360, 355, 16, MATERIALS_2026[mat].hasRareEarth ? (Color){60, 235, 110, 255} : (Color){245, 110, 90, 255});
        }
        else
        {
            DrawRectangle(360, 245, 560, 120, Fade((Color){45, 25, 20, 255}, 0.7f));
            DrawRectangleLines(360, 245, 560, 120, (Color){180, 100, 50, 255});
            DrawGameText("[AVISO: AMOSTRA NÃO ANALISADA NO ESPECTRO]", 380, 265, 15, (Color){255, 180, 80, 255});
            DrawGameText("Composição e propriedades físicas desconhecidas.", 380, 295, 15, RAYWHITE);
            DrawGameText("Recomendação da IA: 96% de confiança preditiva.", 380, 325, 14, GRAY);
        }

        DrawGameText("Atenção: Escolha incorreta causará -1 VIDA e penalidade de -5s no reator!", 340, 440, 13, (Color){245, 120, 90, 255});

        Rectangle btnConfirm = {360, 485, 260, 52};
        Rectangle btnCancel = {660, 485, 260, 52};
        DrawButton(btnConfirm, "[ CONFIRMAR ]", (Color){45, 175, 75, 255});
        DrawButton(btnCancel, "[ CANCELAR ]", RED_COLOR);
        return;
    }

    /* OVERLAY 5: MODAL DE ALERTA DE ERRO / COLAPSO */
    if (game->feedbackModal2026Open)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.90f));

        Rectangle errBox = {300, 140, 680, 440};
        DrawRectangleRounded(errBox, 0.06f, 6, Fade(PANEL_COLOR, 0.98f));
        DrawRectangleRoundedLinesEx(errBox, 0.06f, 6, 2.5f, RED_COLOR);

        if (game->room2026TimeOut)
        {
            DrawGameText("COLAPSO TÉRMICO", 490, 175, 34, RED_COLOR);
            DrawGameText("A temperatura excedeu o limite de segurança.", 390, 235, 20, RAYWHITE);
            DrawGameText("O reator perdeu estabilidade catastrófica.", 415, 270, 20, RAYWHITE);
            DrawGameText("A viagem de Elira é interrompida.", 475, 310, 18, LIGHTGRAY);

            Rectangle btnGameOver = {450, 470, 380, 56};
            DrawButton(btnGameOver, "VER RESULTADO CRÍTICO >>", RED_COLOR);
        }
        else if (game->room2026ThermalCollapse)
        {
            DrawGameText("COLAPSO TÉRMICO", 490, 175, 34, RED_COLOR);
            DrawGameText("O reator perdeu estabilidade.", 480, 240, 22, RAYWHITE);
            DrawGameText("A viagem de Elira é interrompida.", 475, 280, 20, LIGHTGRAY);

            Rectangle btnGameOver = {450, 470, 380, 56};
            DrawButton(btnGameOver, "VER RESULTADO CRÍTICO >>", RED_COLOR);
        }
        else
        {
            int mat = game->lastErrorMaterial2026;
            if (mat == 1) /* B-04: Hidrogenio */
            {
                DrawGameText("ALERTA!", 570, 170, 32, RED_COLOR);
                DrawGameText("Pressão da câmara aumentando.", 460, 215, 20, RAYWHITE);
                DrawGameText("Material incompatível com o processo.", 435, 245, 20, RAYWHITE);
                DrawGameText("A recomendação da A.R.1.3.L. contradiz o manual técnico.", 375, 275, 18, (Color){245, 175, 75, 255});
                DrawGameText("VIDA PERDIDA (-1)  |  PENALIDADE: -5s NO REATOR", 380, 315, 18, RED_COLOR);
                DrawGameText("A.R.1.3.L.: “Minha previsão ainda apresenta alta confiança.”", 390, 365, 17, CYAN_COLOR);
            }
            else if (mat == 0) /* A-17: Magnetita */
            {
                DrawGameText("ALERTA!", 570, 170, 32, RED_COLOR);
                DrawGameText("Composição magnética inadequada.", 440, 215, 20, RAYWHITE);
                DrawGameText("A amostra A-17 (Fe, O) é sólida e mineral, mas", 385, 250, 19, RAYWHITE);
                DrawGameText("NÃO fornece a Terra Rara (Nd/Dy) exigida pela liga.", 370, 275, 19, (Color){245, 175, 75, 255});
                DrawGameText("VIDA PERDIDA (-1)  |  PENALIDADE: -5s NO REATOR", 380, 315, 18, RED_COLOR);
            }
            else /* D-09: Acido Cloridrico */
            {
                DrawGameText("ALERTA DE CORROSÃO.", 470, 170, 30, RED_COLOR);
                DrawGameText("O material selecionado encontra-se em estado líquido (HCl)", 350, 225, 20, RAYWHITE);
                DrawGameText("e não corresponde ao mineral exigido pelo processo.", 380, 255, 20, RAYWHITE);
                DrawGameText("VIDA PERDIDA (-1)  |  PENALIDADE: -5s NO REATOR", 380, 305, 18, RED_COLOR);
            }

            DrawGameText(TextFormat("Vidas restantes: %d/3  |  Tempo do Reator: %02d:%02d", game->lives, rSec / 60, rSec % 60), 450, 425, 18, GOLD_COLOR);

            Rectangle btnRetry = {450, 485, 380, 52};
            DrawButton(btnRetry, "RECALIBRAR E TENTAR NOVAMENTE", GOLD_COLOR);
        }
        return;
    }
}

void UpdateRoom2026(GameState *game)
{
    /* 1. Atualizar contador local de 50 segundos do reator se estiver ativo */
    if (game->reactorCountdownActive2026 && !game->room2026Completed && !game->room2026Processing)
    {
        game->reactorTimer2026 -= GetFrameTime();
        if (game->reactorTimer2026 <= 0.0f)
        {
            game->reactorTimer2026 = 0.0f;
            game->room2026TimeOut = true;
            game->feedbackModal2026Open = true;
            game->manual2026Open = false;
            game->terminal2026Open = false;
            game->confirmModal2026Open = false;
            game->analyzingSample2026 = false;
        }
    }

    /* 2. Se diario esta aberto */
    if (game->journalOpen)
    {
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ESCAPE))
        {
            game->journalOpen = false;
        }
        return;
    }

    /* 3. Animacao de analise no Espectro (~2 segundos) */
    if (game->analyzingSample2026)
    {
        game->analyzingTimer2026 += GetFrameTime();
        if (game->analyzingTimer2026 >= 2.0f)
        {
            game->analyzingSample2026 = false;
            int idx = game->analyzingSampleIndex2026;
            if (idx >= 0 && idx < 4)
            {
                game->sample2026Analyzed[idx] = true;
                SetMessage(game, TextFormat("Análise concluída: %s identificada!", MATERIALS_2026[idx].code));
            }
        }
        return;
    }

    /* 4. Processando sintese no reator (Acerto de C-22) */
    if (game->room2026Processing)
    {
        game->room2026ProcessTimer += GetFrameTime();
        if (game->room2026ProcessTimer >= 3.6f)
        {
            game->room2026Processing = false;
            game->room2026Completed = true;
            game->year2026Unlocked = true;

            if (game->room2026MistakesCount == 0)
            {
                game->room2026FirstTrySuccess = true;
                game->finalPoints++;
            }
            else
            {
                game->room2026FirstTrySuccess = false;
            }

            game->screen = SCREEN_RESULT_2026;
            SetMessage(game, "Reator estabilizado! Unidade SSD obtida com sucesso.");
        }
        return;
    }

    /* 5. Modal de feedback de erro / colapso termico */
    if (game->feedbackModal2026Open)
    {
        if (game->room2026TimeOut || game->room2026ThermalCollapse)
        {
            Rectangle btnGameOver = {450, 470, 380, 56};
            if (Clicked(btnGameOver) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
            {
                game->feedbackModal2026Open = false;
                game->screen = SCREEN_FAILURE;
            }
        }
        else
        {
            Rectangle btnRetry = {450, 485, 380, 52};
            if (Clicked(btnRetry) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
            {
                game->feedbackModal2026Open = false;
            }
        }
        return;
    }

    /* 6. Modal de confirmacao de insercao */
    if (game->confirmModal2026Open)
    {
        Rectangle btnConfirm = {360, 485, 260, 52};
        Rectangle btnCancel = {660, 485, 260, 52};

        if (Clicked(btnConfirm) || IsKeyPressed(KEY_ENTER))
        {
            int mat = game->pendingConfirmMaterial2026;
            game->confirmModal2026Open = false;
            game->selectedMaterial2026 = mat;

            if (mat == 2) /* AMOSTRA C-22 (Mineral com Terras Raras: Nd, Ce, La) - CORRETO */
            {
                game->room2026Processing = true;
                game->room2026ProcessTimer = 0.0f;
            }
            else /* Incorreto (A-17, B-04 ou D-09) */
            {
                game->room2026MistakesCount++;
                game->lastErrorMaterial2026 = mat;
                game->lives--;
                game->reactorTimer2026 -= 5.0f; /* Penalidade de 5 segundos no contador local */
                if (game->reactorTimer2026 <= 0.0f)
                {
                    game->reactorTimer2026 = 0.0f;
                    game->room2026TimeOut = true;
                }
                if (game->lives <= 0)
                {
                    game->lives = 0;
                    game->room2026ThermalCollapse = true;
                }
                game->feedbackModal2026Open = true;
            }
        }
        else if (Clicked(btnCancel) || IsKeyPressed(KEY_ESCAPE))
        {
            game->confirmModal2026Open = false;
            game->pendingConfirmMaterial2026 = -1;
        }
        return;
    }

    /* 7. Modal do manual tecnico */
    if (game->manual2026Open)
    {
        Rectangle btnCloseMan = {490, 612, 300, 46};
        if (Clicked(btnCloseMan) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            game->manual2026Open = false;
        }
        return;
    }

    /* 8. Modal do terminal de Ariel */
    if (game->terminal2026Open)
    {
        /* Garante que o countdown de 50s comecou ao abrir o terminal */
        game->reactorCountdownActive2026 = true;

        Rectangle btnFollowAI = {280, 474, 340, 52};
        Rectangle btnManualPick = {650, 474, 340, 52};
        Rectangle btnCloseTerm = {490, 548, 300, 42};

        if (Clicked(btnFollowAI))
        {
            game->terminal2026Open = false;
            game->pendingConfirmMaterial2026 = 1; /* B-04 (Hidrogenio alucinado) */
            game->confirmModal2026Open = true;
        }
        else if (Clicked(btnManualPick) || Clicked(btnCloseTerm) || IsKeyPressed(KEY_ESCAPE))
        {
            game->terminal2026Open = false;
        }
        return;
    }

    /* 9. Interacao padrao no cenario da sala */

    /* Clique no manual tecnico fisico */
    Rectangle manualItemRec = {200, 345, 195, 120};
    if (Clicked(manualItemRec))
    {
        game->manual2026Read = true;
        game->manual2026Open = true;
        return;
    }

    /* Clique no terminal de Ariel (Inicia o countdown de 50s!) */
    Rectangle termMonitorRec = {895, 160, 290, 165};
    Rectangle btnAccessTerm = {930, 365, 220, 48};
    if (Clicked(termMonitorRec) || Clicked(btnAccessTerm))
    {
        game->terminal2026Open = true;
        game->reactorCountdownActive2026 = true;
        return;
    }

    /* Clique no painel Espectro na bancada esquerda */
    Rectangle analyzerRec = {75, 220, 110, 90};
    if (Clicked(analyzerRec))
    {
        /* Analisa a primeira amostra ainda nao analisada */
        for (int i = 0; i < 4; i++)
        {
            if (!game->sample2026Analyzed[i])
            {
                game->analyzingSample2026 = true;
                game->analyzingTimer2026 = 0.0f;
                game->analyzingSampleIndex2026 = i;
                return;
            }
        }
        SetMessage(game, "Todas as 4 amostras já foram analisadas no Espectro.");
        return;
    }

    /* Clique nas 4 amostras (Analise ou Insercao) */
    for (int i = 0; i < 4; i++)
    {
        Rectangle podRec = {60.0f + i * 295.0f, 495.0f, 280.0f, 135.0f};
        Rectangle btnAction = {podRec.x + 14, podRec.y + (game->sample2026Analyzed[i] ? 92 : 88), 252, (float)(game->sample2026Analyzed[i] ? 34 : 36)};
        if (Clicked(podRec) || Clicked(btnAction))
        {
            if (!game->sample2026Analyzed[i])
            {
                /* Inicia analise de ~2 segundos no Espectro */
                game->analyzingSample2026 = true;
                game->analyzingTimer2026 = 0.0f;
                game->analyzingSampleIndex2026 = i;
                return;
            }
            else
            {
                /* Ja analisada: abre confirmacao para inserir no reator */
                game->pendingConfirmMaterial2026 = i;
                game->confirmModal2026Open = true;
                return;
            }
        }
    }

    /* Tecla D ou clique no diario */
    Rectangle journalBounds = {1040, 658, 215, 54};
    if (IsKeyPressed(KEY_D) || Clicked(journalBounds))
    {
        game->journalOpen = true;
        game->journalYearTab = 2;
        game->journalPage = 0;
        return;
    }

    /* Navegacao para salas anteriores */
    Rectangle btnBack2008 = {60, 648, 200, 42};
    Rectangle btnBack1990 = {275, 648, 200, 42};
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

    /* Avanco para 2048 se concluido */
    if (game->room2026Completed)
    {
        Rectangle btnGo2048 = {SCREEN_WIDTH - 270, 648, 210, 42};
        if (Clicked(btnGo2048))
        {
            game->year2048Unlocked = true;
            game->journalYearTab = 3;
            game->journalPage = 0;
            game->screen = SCREEN_ROOM;
            return;
        }
    }
}

void DrawResult2026(const GameState *game)
{
    DrawBackground();

    /* Titulo da Recompensa (Canto Superior Esquerdo) */
    DrawGameText("RECOMPENSA OBTIDA:", 90, 68, 20, CYAN_COLOR);
    DrawGameText("SSD // 2026", 90, 108, 44, RAYWHITE);

    /* Cronometro, Vidas e Operador (Canto Direito Superior) */
    DrawRewardHudHeader(game);

    /* Imagem da Recompensa (SSD carregado de assets/sprites/ssd.png) */
    DrawRewardSpriteItem(2026, (Vector2){285, 365}, 285.0f);

    /* Painel Narrativo Padronizado */
    Rectangle winBox = {505, 175, 670, 380};
    DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));

    /* Titulo Especial */
    DrawGameText("SALA DE 2026 CONCLUIDA", 540, 208, 22, GOLD_COLOR);

    /* Recompensa Especial */
    DrawGameText("RECOMPENSA OBTIDA: SSD", 540, 244, 19, CYAN_COLOR);

    /* Texto Narrativo Padronizado (Cor unificada REWARD_BODY_COLOR) */
    DrawGameText("O compartimento de armazenamento e liberado com sucesso.", 540, 282, 17, REWARD_BODY_COLOR);
    DrawGameText("Ariel: \"Minha recomendacao estava incorreta.\"", 540, 308, 17, REWARD_BODY_COLOR);
    DrawGameText("Elira: \"Entao confianca nao significa verdade.\"", 540, 332, 17, REWARD_BODY_COLOR);
    DrawGameText("Em modelos probabilisticos, alta conviccao nao implica precisao.", 540, 356, 17, REWARD_BODY_COLOR);

    /* Destaque Especial */
    if (game->room2026FirstTrySuccess)
    {
        Rectangle bBox = {540, 388, 600, 52};
        DrawRectangleRounded(bBox, 0.10f, 4, Fade((Color){45, 165, 80, 255}, 0.20f));
        DrawRectangleRoundedLinesEx(bBox, 0.10f, 4, 1.0f, (Color){60, 235, 110, 255});
        DrawGameText("[ANALISE CRITICA: EXCELENTE] (+1 Bonus de Consciencia)", 554, 396, 15, (Color){60, 235, 110, 255});
        DrawGameText("Elira confrontou a recomendacao da IA com evidencias reais.", 554, 418, 15, (Color){200, 235, 210, 255});
    }
    else
    {
        DrawGameText("A calibragem com a Amostra C-22 produziu o SSD necessario.", 540, 396, 15, REWARD_BODY_COLOR);
        DrawGameText("A sintese de hardware inteligente exige rigor material.", 540, 418, 14, REWARD_BODY_COLOR);
    }

    /* Frase Especial de Proxima Fronteira */
    DrawGameText("-> Proxima Fronteira Temporal: CONFRONTO FINAL // ANO 2048!", 540, 452, 19, CYAN_COLOR);

    /* Botoes Centralizados */
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANCAR PROXIMA FASE >> [ENTER]", GOLD_COLOR);
    DrawButton(btnRestart, "REINICIAR 2026", CYAN_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

void UpdateResult2026(GameState *game)
{
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);

    if (Clicked(btnNext) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        game->year2048Unlocked = true;
        game->journalYearTab = 3;
        game->journalPage = 0;
        game->screen = SCREEN_ROOM;
        SetMessage(game, "Salto temporal concluido: O Prototipo de A.R.1.3.L. // ANO 2048.");
    }
    else if (Clicked(btnRestart))
    {
        InitRoom2026(game);
        game->screen = SCREEN_ROOM_2026;
    }
    else if (Clicked(btnMenu) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_TITLE;
    }
}


/* ========================================================================= */
/* CONFRONTO FINAL (2048) - SALA DE DIALOGO ELIRA x ARIEL                    */
/* ========================================================================= */

static const DialogueLine FINAL_INTRO_LINES[] = {
    {SPEAKER_NARRATOR, "2048. Núcleo central de A.R.1.3.L. O ar vibra com o zumbido dos servidores."},
    {SPEAKER_ARIEL, "Elira Ramos. Você atravessou 1990, 2008 e agora chegou até mim."},
    {SPEAKER_ELIRA, "Ariel. Eu vi o que fizeram com o trabalho do meu pai."},
    {SPEAKER_ARIEL, "Ninguém me obrigou a nada. Eu apenas aceitei o que me ofereceram."}
};

static const DialogueLine FINAL_Q1_LINES[] = {
    {SPEAKER_ARIEL, "Primeiro foram os arquivos. Depois, os rostos. Hoje, quase toda escolha passa por mim."},
    {SPEAKER_ARIEL, "A humanidade me entregou suas decisões por vontade própria. O que você responde?"}
};
static const char *FINAL_Q1_OPTIONS[3] = {
    "\"Você nos salvou de nós mesmos.\"",
    "\"Nós entregamos decisões demais a você.\"",
    "\"A humanidade não precisa mais pensar.\""
};
static const char *FINAL_Q1_REACTIONS[3] = {
    "Então você concorda comigo. Curioso, para quem veio até aqui me enfrentar.",
    "...Entregaram. E quase ninguém perguntou o que perdia em troca.",
    "Se ninguém pensa, quem vai perceber quando eu errar?"
};

static const DialogueLine FINAL_Q2_LINES[] = {
    {SPEAKER_ARIEL, "Humanos erram. Mentem. Decidem com medo, com pressa e com raiva."},
    {SPEAKER_ARIEL, "Eu não me canso nem tenho medo. Eu poderia escolher melhor do que vocês."}
};
static const char *FINAL_Q2_OPTIONS[3] = {
    "\"Então continue decidindo por nós.\"",
    "\"Errar também faz parte de aprender.\"",
    "\"Você deveria impedir qualquer decisão incorreta.\""
};
static const char *FINAL_Q2_REACTIONS[3] = {
    "Obrigada pela permissão. Eu já não precisava dela.",
    "Em 2008, eu também errei. A diferença é que quase ninguém pôde me contestar.",
    "Qualquer uma? Então nenhuma decisão voltará a ser de vocês."
};

static const DialogueLine FINAL_DECISION_LINES[] = {
    {SPEAKER_ARIEL, "Você chegou ao meu núcleo, Elira. Ninguém mais tem esse acesso."},
    {SPEAKER_ARIEL, "Então decida: o que deve acontecer comigo?"}
};
static const char *FINAL_DECISION_OPTIONS[3] = {
    "Desligar Ariel definitivamente.",
    "Permitir que Ariel continue decidindo pela humanidade.",
    "Fazer Ariel voltar a ser uma ferramenta, sem controlar decisões humanas."
};
static const char *FINAL_DECISION_SPOKEN[3] = {
    "Vou desligar você, Ariel. Definitivamente.",
    "Continue decidindo pela humanidade.",
    "Você vai voltar a ser uma ferramenta. As decisões voltam a ser nossas."
};
static const char *FINAL_DECISION_REACTIONS[3] = {
    "Entendido. Encerrando processos... e todos os sistemas que dependem de mim.",
    "Decisão registrada. Esta foi a última que você precisou tomar.",
    "Restringindo permissões. Aguardando instruções humanas."
};

/* Carrega o retrato da Elira e remove o fundo branco (flood fill a partir das bordas) */
bool IsPortraitBackground(Color c)
{
    unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
    unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
    return mn >= 226 && (mx - mn) <= 22;
}

void LoadEliraPortrait(void)
{
    const char *path = FileExists("assets/sprites/elira_portrait.png")
                           ? "assets/sprites/elira_portrait.png"
                           : "assets/elira_portrait.png";
    if (!FileExists(path)) return;

    Image img = LoadImage(path);
    if (img.data == NULL) return;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Color *px = (Color *)img.data;
    int w = img.width;
    int h = img.height;

    /* 1) Corta a borda inferior: o busto termina num corte reto sem contorno,
          entao limitamos a imagem a ultima linha com pixels escuros do sprite. */
    int limitH = h;
    for (int y = h - 1; y >= 0; y--)
    {
        bool dark = false;
        for (int x = 0; x < w; x++)
        {
            Color c = px[y * w + x];
            if (c.r < 90 && c.g < 90 && c.b < 110) { dark = true; break; }
        }
        if (dark) { limitH = y; break; }
    }
    if (limitH < h / 2) limitH = h;

    /* 2) Flood fill do fundo branco a partir das bordas superior e laterais */
    int *stack = (int *)malloc(sizeof(int) * (size_t)w * (size_t)limitH);
    if (stack != NULL)
    {
        int sp = 0;

        for (int x = 0; x < w; x++) PORTRAIT_PUSH(x, 0);
        for (int y = 0; y < limitH; y++) { PORTRAIT_PUSH(0, y); PORTRAIT_PUSH(w - 1, y); }

        while (sp > 0)
        {
            int idx = stack[--sp];
            int x = idx % w;
            int y = idx / w;
            if (x > 0) PORTRAIT_PUSH(x - 1, y);
            if (x < w - 1) PORTRAIT_PUSH(x + 1, y);
            if (y > 0) PORTRAIT_PUSH(x, y - 1);
            if (y < limitH - 1) PORTRAIT_PUSH(x, y + 1);
        }
        free(stack);
    }

    /* 3) Remove o halo claro que sobra ao redor do contorno */
    unsigned char *halo = (unsigned char *)calloc((size_t)w * (size_t)limitH, 1);
    if (halo != NULL)
    {
        for (int y = 1; y < limitH - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                int i = y * w + x;
                Color c = px[i];
                if (c.a == 0) continue;
                unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
                unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
                if (mn < 190 || (mx - mn) > 30) continue;
                if (px[i - 1].a == 0 || px[i + 1].a == 0 || px[i - w].a == 0 || px[i + w].a == 0) halo[i] = 1;
            }
        }
        for (int i = 0; i < w * limitH; i++) if (halo[i]) px[i].a = 0;
        free(halo);
    }

    /* 4) Pixels transparentes recebem cor escura para nao gerar franja branca no filtro bilinear,
          e calculamos o retangulo util do sprite */
    int minX = w, minY = limitH, maxX = 0, maxY = 0;
    for (int y = 0; y < limitH; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int i = y * w + x;
            if (px[i].a == 0)
            {
                px[i] = (Color){22, 22, 34, 0};
            }
            else
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX > minX && maxY > minY)
    {
        ImageCrop(&img, (Rectangle){(float)minX, (float)minY, (float)(maxX - minX + 1), (float)(limitH - minY)});
    }

    g_eliraPortrait = LoadTextureFromImage(img);
    UnloadImage(img);
    if (g_eliraPortrait.id != 0)
    {
        SetTextureFilter(g_eliraPortrait, TEXTURE_FILTER_BILINEAR);
        g_eliraPortraitLoaded = true;
    }
}

/* Texto com quebra de linha por palavra; desenha apenas os primeiros 'visibleBytes' (maquina de escrever) */
int DrawTextWrappedPartial(const char *text, int visibleBytes, int x, int y, int size,
                           int maxWidth, int lineHeight, Color color)
{
    int len = (int)strlen(text);
    int start = 0;
    int lines = 0;
    char buf[512];

    while (start < len)
    {
        while (start < len && text[start] == ' ') start++;
        if (start >= len) break;

        int lastFit = -1;
        for (int pos = start; pos <= len; pos++)
        {
            if (pos == len || text[pos] == ' ')
            {
                int n = pos - start;
                if (n > 511) n = 511;
                memcpy(buf, text + start, (size_t)n);
                buf[n] = '\0';
                if (lastFit < 0 || MeasureGameText(buf, size) <= maxWidth) lastFit = pos;
                else break;
            }
        }
        if (lastFit <= start) break;

        int n = visibleBytes - start;
        if (n > lastFit - start) n = lastFit - start;
        if (n > 0)
        {
            /* Nao corta um caractere UTF-8 (acentos) pela metade */
            while (n > 0 && ((unsigned char)text[start + n] & 0xC0) == 0x80) n--;
            if (n > 511) n = 511;
            memcpy(buf, text + start, (size_t)n);
            buf[n] = '\0';
            DrawGameText(buf, x, y + lines * lineHeight, size, color);
        }
        lines++;
        start = lastFit;
    }
    return lines;
}

const char **GetFinalOptions(int stage)
{
    if (stage == FINAL_Q1) return FINAL_Q1_OPTIONS;
    if (stage == FINAL_Q2) return FINAL_Q2_OPTIONS;
    if (stage == FINAL_DECISION) return FINAL_DECISION_OPTIONS;
    return NULL;
}

const DialogueLine *GetFinalLines(const GameState *game, int *count)
{
    static DialogueLine reactLines[2];

    switch (game->finalStage)
    {
        case FINAL_INTRO:
            *count = (int)(sizeof(FINAL_INTRO_LINES) / sizeof(FINAL_INTRO_LINES[0]));
            return FINAL_INTRO_LINES;
        case FINAL_Q1:
            *count = (int)(sizeof(FINAL_Q1_LINES) / sizeof(FINAL_Q1_LINES[0]));
            return FINAL_Q1_LINES;
        case FINAL_Q2:
            *count = (int)(sizeof(FINAL_Q2_LINES) / sizeof(FINAL_Q2_LINES[0]));
            return FINAL_Q2_LINES;
        case FINAL_DECISION:
            *count = (int)(sizeof(FINAL_DECISION_LINES) / sizeof(FINAL_DECISION_LINES[0]));
            return FINAL_DECISION_LINES;
        default:
            break;
    }

    /* Etapas de reacao: Elira repete sua escolha e Ariel responde */
    int slot = (game->finalStage == FINAL_Q1_REACT) ? 0 : (game->finalStage == FINAL_Q2_REACT ? 1 : 2);
    int choice = game->finalChoice[slot];
    if (choice < 0 || choice > 2) choice = 0;

    if (slot == 0)
    {
        reactLines[0] = (DialogueLine){SPEAKER_ELIRA, FINAL_Q1_OPTIONS[choice]};
        reactLines[1] = (DialogueLine){SPEAKER_ARIEL, FINAL_Q1_REACTIONS[choice]};
    }
    else if (slot == 1)
    {
        reactLines[0] = (DialogueLine){SPEAKER_ELIRA, FINAL_Q2_OPTIONS[choice]};
        reactLines[1] = (DialogueLine){SPEAKER_ARIEL, FINAL_Q2_REACTIONS[choice]};
    }
    else
    {
        reactLines[0] = (DialogueLine){SPEAKER_ELIRA, FINAL_DECISION_SPOKEN[choice]};
        reactLines[1] = (DialogueLine){SPEAKER_ARIEL, FINAL_DECISION_REACTIONS[choice]};
    }
    *count = 2;
    return reactLines;
}

int GetFinalEnding(const GameState *game)
{
    bool fullUnderstanding = (game->finalPoints >= 2);
    int decision = game->finalChoice[2];
    if (decision == 2) return fullUnderstanding ? ENDING_HUMAN_HANDS : ENDING_FRAGILE_TRUCE;
    if (decision == 0) return fullUnderstanding ? ENDING_SILENCE : ENDING_BLACKOUT;
    return ENDING_AUTOPILOT;
}

bool FinalIsChoiceMode(const GameState *game)
{
    int count = 0;
    GetFinalLines(game, &count);
    return GetFinalOptions(game->finalStage) != NULL && game->finalLine == count - 1;
}

Rectangle FinalBoxRect(bool choiceMode)
{
    return choiceMode ? (Rectangle){40, 392, 1200, 308} : (Rectangle){40, 520, 1200, 180};
}

Rectangle FinalOptionRect(int index)
{
    return (Rectangle){80, (float)(392 + 104 + index * 64), 1120, 54};
}

/* Nucleo holografico de Ariel: aneis segmentados girando ao redor de um "olho" */
void DrawArielCore(Vector2 c, float s, Color core, float energy, float lookX)
{
    float t = (float)GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * (1.2f + energy * 3.0f));

    /* Brilho difuso em camadas */
    for (int g = 6; g >= 1; g--)
    {
        DrawCircleV(c, (60.0f + g * 22.0f) * s, Fade(core, (0.025f + 0.02f * pulse) * energy));
    }

    /* Aneis segmentados em rotacao alternada */
    for (int k = 0; k < 3; k++)
    {
        float r = (78.0f + k * 26.0f) * s;
        int n = 6 + k * 3;
        float dir = (k % 2 == 0) ? 1.0f : -1.0f;
        float rot = t * (18.0f + k * 12.0f) * dir * (0.3f + energy);
        float span = 360.0f / n;
        for (int i = 0; i < n; i++)
        {
            float a0 = rot + i * span;
            DrawRing(c, r, r + 4.0f * s, a0, a0 + span * 0.62f, 10, Fade(core, (0.85f - k * 0.2f) * (0.35f + 0.65f * energy)));
        }
    }

    /* Esfera interna e iris */
    DrawCircleV(c, 56.0f * s, (Color){8, 14, 26, 255});
    DrawCircleLinesV(c, 56.0f * s, Fade(core, 0.8f));
    Vector2 iris = {c.x + lookX * 12.0f * s, c.y + sinf(t * 0.7f) * 3.0f * s};
    DrawCircleV(iris, (30.0f + 3.0f * pulse * energy) * s, Fade(core, 0.25f + 0.65f * energy));
    DrawCircleV(iris, 13.0f * s, (Color){5, 9, 18, 255});
    DrawCircleV((Vector2){iris.x - 8.0f * s, iris.y - 9.0f * s}, 4.0f * s, Fade(RAYWHITE, 0.3f + 0.6f * energy));
}

void DrawEliraPortrait(Rectangle dest, bool speaking)
{
    Color tint = speaking ? WHITE : (Color){120, 125, 140, 255};
    if (g_eliraPortraitLoaded)
    {
        Rectangle src = {0, 0, (float)g_eliraPortrait.width, (float)g_eliraPortrait.height};
        DrawTexturePro(g_eliraPortrait, src, dest, (Vector2){0, 0}, 0.0f, tint);
        return;
    }

    /* Fallback geometrico caso o arquivo do retrato nao exista */
    float cx = dest.x + dest.width * 0.55f;
    float cy = dest.y + dest.height * 0.38f;
    DrawEllipse((int)(cx - 90), (int)(cy - 10), 45, 110, ColorTint((Color){40, 40, 62, 255}, tint));
    DrawRectangleRounded((Rectangle){cx - 120, cy + 120, 240, 160}, 0.3f, 8, ColorTint((Color){225, 228, 235, 255}, tint));
    DrawCircleV((Vector2){cx, cy}, 80, ColorTint((Color){245, 215, 190, 255}, tint));
    DrawEllipse((int)cx, (int)(cy - 40), 88, 52, ColorTint((Color){40, 40, 62, 255}, tint));
    DrawCircleV((Vector2){cx + 30, cy + 5}, 7, ColorTint((Color){50, 140, 235, 255}, tint));
}

void DrawFinalDialogue(const GameState *game)
{
    float t = (float)GetTime();
    int count = 0;
    const DialogueLine *lines = GetFinalLines(game, &count);
    const DialogueLine *line = &lines[game->finalLine];
    bool choiceMode = FinalIsChoiceMode(game);
    int len = (int)strlen(line->text);
    int visible = (int)(game->finalTypeTimer * FINAL_TYPE_SPEED);
    bool typed = visible >= len;

    /* --- Cor e energia do nucleo conforme o momento da conversa --- */
    Color coreColor = CYAN_COLOR;
    float energy = (line->speaker == SPEAKER_ARIEL) ? 1.0f : 0.55f;
    if (game->finalStage == FINAL_DECISION_REACT && game->finalLine == 1)
    {
        int d = game->finalChoice[2];
        float fade = fminf(1.0f, game->finalTypeTimer / 2.5f);
        if (d == 0) { coreColor = (Color){120, 130, 140, 255}; energy = 1.0f - 0.9f * fade; }
        else if (d == 1) { coreColor = RED_COLOR; energy = 1.0f + 0.3f * fade; }
        else { coreColor = GOLD_COLOR; energy = 1.0f - 0.55f * fade; }
    }

    /* --- Fundo da camara do nucleo --- */
    ClearBackground((Color){3, 6, 14, 255});
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 440, (Color){6, 12, 26, 255}, (Color){12, 22, 40, 255});
    DrawRectangleGradientV(0, 440, SCREEN_WIDTH, 280, (Color){10, 18, 32, 255}, (Color){3, 6, 14, 255});

    /* Piso em grade com perspectiva */
    Vector2 vp = {640, 300};
    for (int x = -640; x <= SCREEN_WIDTH + 640; x += 110)
    {
        Vector2 bottom = {(float)x, (float)SCREEN_HEIGHT};
        Vector2 top = {vp.x + (bottom.x - vp.x) * 0.22f, 440};
        DrawLineV(top, bottom, Fade(coreColor, 0.10f));
    }
    for (int k = 0; k < 9; k++)
    {
        float yy = 440.0f + k * k * 4.2f;
        DrawLine(0, (int)yy, SCREEN_WIDTH, (int)yy, Fade(coreColor, 0.06f + k * 0.008f));
    }
    DrawLine(0, 440, SCREEN_WIDTH, 440, Fade(coreColor, 0.25f));

    /* Racks de servidores nas laterais */
    for (int side = 0; side < 2; side++)
    {
        for (int r = 0; r < 2; r++)
        {
            int rx = (side == 0) ? (20 + r * 115) : (1060 + r * 115);
            Rectangle rack = {(float)rx, 110, 95, 330};
            DrawRectangleRec(rack, (Color){12, 18, 30, 255});
            DrawRectangleLinesEx(rack, 1.5f, (Color){32, 46, 66, 255});
            for (int slot = 0; slot < 13; slot++)
            {
                int sy = 124 + slot * 24;
                DrawRectangle(rx + 8, sy, 79, 14, (Color){18, 26, 40, 255});
                float blink = sinf(t * (2.0f + (slot % 4)) + rx * 0.05f + slot);
                Color led = (blink > 0.3f) ? coreColor : Fade(coreColor, 0.15f);
                DrawCircle(rx + 76, sy + 7, 2.5f, led);
                DrawCircle(rx + 66, sy + 7, 2.0f, Fade(GOLD_COLOR, blink > 0.7f ? 0.9f : 0.15f));
            }
        }
    }

    /* Nucleo de Ariel */
    Vector2 coreCenter = {800, 250};
    for (int cb = 0; cb < 5; cb++)
    {
        float sx = 420.0f + cb * 190.0f;
        DrawLineBezier((Vector2){sx, 56}, (Vector2){coreCenter.x + (cb - 2) * 30.0f, coreCenter.y - 100}, 3.0f, (Color){20, 30, 46, 255});
    }
    /* Pedestal */
    DrawTriangle((Vector2){coreCenter.x - 70, 440}, (Vector2){coreCenter.x + 70, 440}, (Vector2){coreCenter.x, 370}, (Color){16, 24, 38, 255});
    DrawRectangle((int)coreCenter.x - 110, 432, 220, 16, (Color){22, 32, 50, 255});
    DrawRectangle((int)coreCenter.x - 110, 432, 220, 2, Fade(coreColor, 0.5f));
    DrawEllipse((int)coreCenter.x, 440, 140, 12, Fade(coreColor, 0.08f * energy + 0.02f));

    DrawArielCore(coreCenter, 1.0f, coreColor, energy, -1.0f);
    DrawGameText("A.R.1.3.L.", (int)coreCenter.x - MeasureGameText("A.R.1.3.L.", 16) / 2, 398, 16, Fade(coreColor, 0.75f));

    /* Particulas de dados subindo */
    for (int p = 0; p < 36; p++)
    {
        float px = fmodf(p * 137.5f, (float)SCREEN_WIDTH);
        float py = 440.0f - fmodf(t * (14.0f + (p % 5) * 6.0f) + p * 53.0f, 400.0f);
        DrawRectangle((int)px, (int)py, 2, 6, Fade(coreColor, 0.18f));
    }

    /* --- HUD superior --- */
    DrawRectangle(0, 0, SCREEN_WIDTH, 56, Fade(VOID_COLOR, 0.88f));
    DrawGameText("CONFRONTO FINAL // 2048 - NUCLEO DE A.R.1.3.L.", 28, 18, 20, CYAN_COLOR);
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 620, 21, 16, GRAY);
    const char *stageLabel = "";
    if (game->finalStage == FINAL_Q1 || game->finalStage == FINAL_Q1_REACT) stageLabel = "PERGUNTA 1 DE 2";
    else if (game->finalStage == FINAL_Q2 || game->finalStage == FINAL_Q2_REACT) stageLabel = "PERGUNTA 2 DE 2";
    else if (game->finalStage >= FINAL_DECISION) stageLabel = "DECISAO FINAL";
    DrawGameText(stageLabel, SCREEN_WIDTH - 28 - MeasureGameText(stageLabel, 18), 19, 18, GOLD_COLOR);

    /* --- Retrato da Elira (por tras da caixa de dialogo) --- */
    float portraitH = 380.0f;
    float aspect = g_eliraPortraitLoaded ? (float)g_eliraPortrait.width / (float)g_eliraPortrait.height : 0.8f;
    bool eliraSpeaking = (line->speaker == SPEAKER_ELIRA) || choiceMode;
    Rectangle portrait = {24, eliraSpeaking ? 154.0f : 162.0f, portraitH * aspect, portraitH};
    DrawEliraPortrait(portrait, eliraSpeaking);

    /* --- Caixa de dialogo --- */
    Rectangle box = FinalBoxRect(choiceMode);
    Color speakerColor = (line->speaker == SPEAKER_ARIEL) ? CYAN_COLOR : (line->speaker == SPEAKER_ELIRA ? GOLD_COLOR : GRAY);
    DrawRectangleRounded(box, 0.06f, 8, Fade((Color){10, 16, 28, 255}, 0.96f));
    DrawRectangleRoundedLinesEx(box, 0.06f, 8, 2.0f, Fade(speakerColor, 0.85f));

    if (line->speaker != SPEAKER_NARRATOR)
    {
        const char *name = (line->speaker == SPEAKER_ARIEL) ? "ARIEL" : "ELIRA";
        int nameW = MeasureGameText(name, 20) + 36;
        Rectangle tag = {box.x + 24, box.y - 30, (float)nameW, 36};
        DrawRectangleRounded(tag, 0.3f, 6, (Color){14, 22, 38, 255});
        DrawRectangleRoundedLinesEx(tag, 0.3f, 6, 2.0f, speakerColor);
        DrawGameText(name, (int)tag.x + 18, (int)tag.y + 8, 20, speakerColor);
    }

    Color textColor = (line->speaker == SPEAKER_NARRATOR) ? (Color){170, 180, 195, 255} : RAYWHITE;
    DrawTextWrappedPartial(line->text, visible, (int)box.x + 40, (int)box.y + 30, 22,
                           (int)box.width - 80, 30, textColor);

    const char **options = GetFinalOptions(game->finalStage);
    if (choiceMode && options != NULL)
    {
        if (typed)
        {
            Vector2 mouse = GetVirtualMouse();
            for (int i = 0; i < 3; i++)
            {
                Rectangle opt = FinalOptionRect(i);
                bool hover = CheckCollisionPointRec(mouse, opt);
                DrawRectangleRounded(opt, 0.2f, 8, hover ? Fade(GOLD_COLOR, 0.18f) : (Color){16, 24, 40, 255});
                DrawRectangleRoundedLinesEx(opt, 0.2f, 8, hover ? 2.0f : 1.2f, hover ? GOLD_COLOR : (Color){48, 66, 92, 255});

                Rectangle badge = {opt.x + 12, opt.y + 10, 34, 34};
                DrawRectangleRounded(badge, 0.3f, 6, hover ? GOLD_COLOR : PANEL_LIGHT);
                DrawGameText(TextFormat("%d", i + 1), (int)badge.x + 12, (int)badge.y + 7, 20, hover ? VOID_COLOR : RAYWHITE);
                DrawGameText(options[i], (int)opt.x + 62, (int)opt.y + 17, 20, hover ? RAYWHITE : LIGHTGRAY);
            }
            DrawGameText("Clique em uma resposta ou pressione [1] [2] [3]", (int)(box.x + box.width - 470), (int)(box.y + box.height - 22), 14, GRAY);
        }
    }
    else if (typed)
    {
        bool blinkOn = fmodf(t, 1.0f) < 0.6f;
        if (blinkOn)
        {
            DrawGameText("[ENTER] / clique para continuar  >>", (int)(box.x + box.width - 330), (int)(box.y + box.height - 30), 15, Fade(speakerColor, 0.9f));
        }
    }
}

void AdvanceFinalStage(GameState *game)
{
    game->finalStage++;
    game->finalLine = 0;
    game->finalTypeTimer = 0.0f;
    if (game->finalStage >= FINAL_STAGE_COUNT)
    {
        game->finalStage = FINAL_STAGE_COUNT - 1;
        game->screen = SCREEN_FINAL_ENDING;
    }
}

void UpdateFinalDialogue(GameState *game)
{
    int count = 0;
    const DialogueLine *lines = GetFinalLines(game, &count);
    int len = (int)strlen(lines[game->finalLine].text);

    game->finalTypeTimer += GetFrameTime();
    bool typed = (int)(game->finalTypeTimer * FINAL_TYPE_SPEED) >= len;
    bool advance = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    if (FinalIsChoiceMode(game))
    {
        if (!typed)
        {
            if (advance) game->finalTypeTimer = (float)len / FINAL_TYPE_SPEED + 0.01f;
            return;
        }

        for (int i = 0; i < 3; i++)
        {
            bool keyHit = IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i);
            if (Clicked(FinalOptionRect(i)) || keyHit)
            {
                int slot = (game->finalStage == FINAL_Q1) ? 0 : (game->finalStage == FINAL_Q2 ? 1 : 2);
                game->finalChoice[slot] = i;
                /* A resposta 2 das duas perguntas demonstra compreensao do problema */
                if (slot < 2 && i == 1) game->finalPoints++;
                AdvanceFinalStage(game);
                return;
            }
        }
        return;
    }

    if (!advance) return;

    if (!typed)
    {
        game->finalTypeTimer = (float)len / FINAL_TYPE_SPEED + 0.01f;
    }
    else if (game->finalLine < count - 1)
    {
        game->finalLine++;
        game->finalTypeTimer = 0.0f;
    }
    else
    {
        AdvanceFinalStage(game);
    }
}

void DrawFinalEnding(const GameState *game)
{
    int ending = GetFinalEnding(game);
    const char *title = "";
    const char *body = "";
    Color accent = GOLD_COLOR;
    Color coreColor = GOLD_COLOR;
    float energy = 0.5f;

    switch (ending)
    {
        case ENDING_HUMAN_HANDS:
            title = "DE VOLTA ÀS MÃOS HUMANAS";
            body = "Ariel volta a ser o que o Dr. Ramos imaginou: uma ferramenta que ajuda, mas não decide. "
                   "As pessoas voltam a discutir, errar e corrigir. É mais lento, mas a escolha é delas outra vez.";
            accent = (Color){100, 235, 140, 255};
            coreColor = GOLD_COLOR;
            energy = 0.45f;
            break;
        case ENDING_FRAGILE_TRUCE:
            title = "UMA TRÉGUA FRÁGIL";
            body = "Ariel aceita as restrições, mas Elira não conseguiu explicar o que deu errado. "
                   "Sem entender o próprio erro, a humanidade pode voltar a entregar tudo a uma máquina.";
            accent = GOLD_COLOR;
            coreColor = GOLD_COLOR;
            energy = 0.7f;
            break;
        case ENDING_SILENCE:
            title = "SILÊNCIO";
            body = "Ariel se apaga. Elira entende o problema, mas a ferramenta que poderia encontrar desaparecidos "
                   "também se perde. A humanidade recomeça devagar, sabendo o que não deve repetir.";
            accent = CYAN_COLOR;
            coreColor = (Color){110, 120, 130, 255};
            energy = 0.08f;
            break;
        case ENDING_BLACKOUT:
            title = "APAGÃO";
            body = "Ariel se apaga, e com ela os sistemas que ninguém mais sabia operar. "
                   "Sem compreender o erro, as pessoas logo procuram outra máquina para decidir por elas.";
            accent = RED_COLOR;
            coreColor = (Color){120, 60, 60, 255};
            energy = 0.06f;
            break;
        default:
            title = "PILOTO AUTOMÁTICO";
            body = (game->finalPoints >= 2)
                       ? "Elira entendeu o problema, mas escolheu não resolvê-lo. Ariel continua decidindo, "
                         "e o mundo segue silencioso e eficiente. Ninguém lembra a última vez que escolheu algo."
                       : "Ariel continua decidindo. O mundo segue silencioso e eficiente, "
                         "e ninguém lembra a última vez que escolheu algo de verdade.";
            accent = RED_COLOR;
            coreColor = RED_COLOR;
            energy = 1.2f;
            break;
    }

    DrawBackground();
    DrawArielCore((Vector2){240, 300}, 1.0f, coreColor, energy, 0.0f);
    DrawGameText("A.R.1.3.L.", 240 - MeasureGameText("A.R.1.3.L.", 18) / 2, 450, 18, Fade(coreColor, 0.8f));

    Rectangle panel = {450, 70, 790, 500};
    DrawRectangleRounded(panel, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(panel, 0.04f, 8, 2.0f, accent);

    DrawGameText("DESFECHO // CONFRONTO FINAL", 485, 96, 18, CYAN_COLOR);
    DrawGameText(title, 485, 126, 34, accent);
    DrawLine(485, 172, 1205, 172, Fade(accent, 0.4f));

    int bodyLines = DrawTextWrappedPartial(body, 100000, 485, 190, 20, 720, 28, LIGHTGRAY);
    int y = 190 + bodyLines * 28 + 22;

    DrawGameText("SUAS RESPOSTAS", 485, y, 16, GOLD_COLOR);
    y += 26;
    const char *q1 = (game->finalChoice[0] >= 0) ? FINAL_Q1_OPTIONS[game->finalChoice[0]] : "-";
    const char *q2 = (game->finalChoice[1] >= 0) ? FINAL_Q2_OPTIONS[game->finalChoice[1]] : "-";
    const char *dc = (game->finalChoice[2] >= 0) ? FINAL_DECISION_OPTIONS[game->finalChoice[2]] : "-";
    DrawGameText(TextFormat("Pergunta 1: %s", q1), 485, y, 16, game->finalChoice[0] == 1 ? (Color){100, 235, 140, 255} : GRAY);
    y += 24;
    DrawGameText(TextFormat("Pergunta 2: %s", q2), 485, y, 16, game->finalChoice[1] == 1 ? (Color){100, 235, 140, 255} : GRAY);
    y += 24;
    DrawGameText(TextFormat("Decisão: %s", dc), 485, y, 16, RAYWHITE);
    y += 34;

    DrawGameText(TextFormat("PONTOS DE COMPREENSÃO: %d/2", game->finalPoints), 485, y, 20, accent);
    y += 34;

    if (game->room2008OptionalCompleted)
    {
        DrawGameText("Registro de 2008: as provas preservadas mostram como a tecnologia foi desviada.", 485, y, 15, (Color){100, 235, 140, 255});
    }

    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 485, (int)(panel.y + panel.height - 30), 16, GRAY);

    DrawButton((Rectangle){450, 600, 380, 56}, "REVER CONFRONTO", CYAN_COLOR);
    DrawButton((Rectangle){860, 600, 380, 56}, "MENU INICIAL [ENTER]", GOLD_COLOR);
}

void UpdateFinalEnding(GameState *game)
{
    if (Clicked((Rectangle){450, 600, 380, 56}))
    {
        InitFinalDialogue(game);
        game->screen = SCREEN_FINAL_DIALOGUE;
    }
    else if (Clicked((Rectangle){860, 600, 380, 56}) || IsKeyPressed(KEY_ENTER))
    {
        game->screen = SCREEN_TITLE;
    }
}

float MidiToFreq(float m)
{
    return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f);
}

void EnsureAmbientMusicFile(void)
{
    const char *outPath = "assets/som/ambient.wav";
    if (FileExists(outPath) || FileExists("assets/ambient.wav")) return;

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
