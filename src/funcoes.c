#include "funcoes.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

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

void DrawCrawlTextEx(const char *text, Vector2 position, float size,
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

Vector2 MeasureCrawlTextEx(const char *text, float size, float spacing)
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

/* ========================================================================= */
/* CAPA DO MENU INICIAL (Elira recortada + logo da capa)                     */
/* ========================================================================= */
Texture2D g_menuCoverElira = {0};
Texture2D g_menuCoverLogo = {0};
bool g_menuCoverLoaded = false;

void LoadMenuCover(void)
{
    if (g_menuCoverLoaded) return;

    const char *eliraPath = FindRewardSpritePath("menu/capa_elira.png");
    if (FileExists(eliraPath)) g_menuCoverElira = LoadTexture(eliraPath);
    const char *logoPath = FindRewardSpritePath("menu/capa_logo.png");
    if (FileExists(logoPath)) g_menuCoverLogo = LoadTexture(logoPath);

    if (g_menuCoverElira.id == 0 || g_menuCoverLogo.id == 0)
    {
        /* Sem as duas imagens o menu continua com o visual antigo */
        UnloadMenuCover();
        return;
    }

    /* Elira em pixel art: escala nao inteira com POINT fica mais nitida que BILINEAR */
    SetTextureFilter(g_menuCoverElira, TEXTURE_FILTER_POINT);
    SetTextureFilter(g_menuCoverLogo, TEXTURE_FILTER_BILINEAR);
    g_menuCoverLoaded = true;
}

void UnloadMenuCover(void)
{
    if (g_menuCoverElira.id != 0) UnloadTexture(g_menuCoverElira);
    if (g_menuCoverLogo.id != 0)  UnloadTexture(g_menuCoverLogo);
    g_menuCoverElira = (Texture2D){0};
    g_menuCoverLogo = (Texture2D){0};
    g_menuCoverLoaded = false;
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

    /* 1. Recorte justo da caixa delimitadora do coração (usando canal alpha nativo do PNG) */
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

    /* 2. Cria textura do coração cheio (g_heartFullTex) */
    g_heartFullTex = LoadTextureFromImage(img);
    if (g_heartFullTex.id != 0)
    {
        SetTextureFilter(g_heartFullTex, TEXTURE_FILTER_BILINEAR);
    }

    /* 3. Cria textura do coração vazio (g_heartEmptyTex): retira o vermelho de dentro */
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
            /* Contorno preservado em tom ardósia visível no HUD escuro mantendo alpha original */
            emptyPx[i] = (Color){ 85, 100, 120, emptyPx[i].a };
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

/* Topo das salas: ano na ponta esquerda e operador ao lado.
   O canto direito fica livre para os botoes ADMIN e MENU (DrawPhaseControls). */
void DrawHudYearOperator(const char *year, const char *playerName, int y)
{
    DrawGameText(year, 28, y, 20, CYAN_COLOR);
    DrawGameText(TextFormat("OPERADOR: %s", playerName), 104, y + 2, 16, (Color){160, 175, 190, 255});
}

/* Topo das salas: coracoes | tempo, centralizados na tela, com um separador entre eles. */
Rectangle DrawHudLivesTimer(int lives, int totalSeconds)
{
    const float heartSize = 24.0f, heartGap = 6.0f, sepGap = 22.0f, sepWidth = 2.0f;
    const char *timeText = TextFormat("%02d:%02d", totalSeconds / 60, totalSeconds % 60);
    float heartsWidth = 3 * heartSize + 2 * heartGap;
    float groupWidth = heartsWidth + 2 * sepGap + sepWidth + (float)MeasureGameText(timeText, 22);
    float groupX = (SCREEN_WIDTH - groupWidth) / 2.0f;
    float sepX = groupX + heartsWidth + sepGap;

    DrawLivesIcons((Vector2){groupX, 22}, lives, heartSize);
    DrawRectangle((int)sepX, 18, (int)sepWidth, 32, Fade(CYAN_COLOR, 0.45f));
    DrawGameText(timeText, (int)(sepX + sepWidth + sepGap), 23, 22, GOLD_COLOR);
    return (Rectangle){groupX, 18, groupWidth, 32};
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

#define SETTINGS_RES_COUNT 3

typedef struct {
    int width;
    int height;
    const char *label;
} SettingsResOption;

static const SettingsResOption g_resOptions[SETTINGS_RES_COUNT] = {
    { 1280,  720, "1280 x 720 (HD)" },
    { 1600,  900, "1600 x 900 (HD+)" },
    { 1920, 1080, "1920 x 1080 (FHD)" }
};

static bool g_resDropdownOpen = false;

void ApplyResolution(int resIndex)
{
    if (resIndex < 0 || resIndex >= SETTINGS_RES_COUNT) return;
    g_currentRes = resIndex;
    int w = g_resOptions[resIndex].width;
    int h = g_resOptions[resIndex].height;

    if (IsWindowFullscreen())
    {
        ToggleFullscreen();
    }
    SetWindowSize(w, h);
    int monitor = GetCurrentMonitor();
    int monW = GetMonitorWidth(monitor);
    int monH = GetMonitorHeight(monitor);
    int posX = (monW - w) / 2;
    int posY = (monH - h) / 2;
    if (posX < 0) posX = 0;
    if (posY < 0) posY = 0;
    SetWindowPosition(posX, posY);
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

/* ========================================================================= */
/* CONTROLES GLOBAIS DAS FASES: MENU DE PAUSA E ATALHO DE ADMINISTRADOR       */
/* ========================================================================= */

static Rectangle PhaseMenuButton(void)
{
    return (Rectangle){SCREEN_WIDTH - 92.0f, 14.0f, 76.0f, 36.0f};
}

static Rectangle AdminSkipButton(void)
{
    return (Rectangle){SCREEN_WIDTH - 326.0f, 14.0f, 224.0f, 36.0f};
}

static bool IsAdminSkippableScreen(GameScreen screen)
{
    switch (screen)
    {
        case SCREEN_ROOM_2:
        case SCREEN_MEMORY_1990:
        case SCREEN_PUZZLE_FILES:
        case SCREEN_ROOM_2008:
        case SCREEN_PUZZLE_2008:
        case SCREEN_ROOM:
        case SCREEN_TERMINAL:
        case SCREEN_ROOM_2026:
        case SCREEN_TERMINAL_2046:
            return true;
        default:
            return false;
    }
}

bool IsAdminPlayer(const GameState *game)
{
    static const char adminName[] = "admin";
    if (game == NULL || strlen(game->playerName) != 5) return false;

    for (int i = 0; i < 5; i++)
    {
        char c = game->playerName[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != adminName[i]) return false;
    }
    return true;
}

bool IsPhaseScreen(GameScreen screen)
{
    switch (screen)
    {
        case SCREEN_INTRO:
        case SCREEN_ROOM:
        case SCREEN_TERMINAL:
        case SCREEN_RESULT:
        case SCREEN_ROOM_2:
        case SCREEN_MEMORY_1990:
        case SCREEN_PUZZLE_FILES:
        case SCREEN_RESULT_2:
        case SCREEN_ROOM_2008:
        case SCREEN_PUZZLE_2008:
        case SCREEN_RESULT_2008:
        case SCREEN_ROOM_2026:
        case SCREEN_RESULT_2026:
        case SCREEN_FINAL_DIALOGUE:
        case SCREEN_FINAL_ENDING:
        case SCREEN_FAILURE:
        case SCREEN_INTRO_2008:
        case SCREEN_INTRO_2026:
        case SCREEN_INTRO_2048:
        case SCREEN_INTRO_2046:
        case SCREEN_TERMINAL_2046:
        case SCREEN_RESULT_2046:
            return true;
        default:
            return false;
    }
}

static void ClosePhaseOverlays(GameState *game)
{
    game->journalOpen = false;
    game->activeDocument2008 = -1;
    /* A extracao do SSD em andamento continua; demais paineis de 2026 sao fechados */
    if (game->modal2026 != R2026_MODAL_EXTRACTING) game->modal2026 = R2026_MODAL_NONE;
    game->feedback2026Fatal = false;
}

static void SkipCurrentPhase(GameState *game)
{
    ClosePhaseOverlays(game);

    switch (game->screen)
    {
        case SCREEN_ROOM_2:
        case SCREEN_MEMORY_1990:
        case SCREEN_PUZZLE_FILES:
            game->memory1990Completed = true;
            game->puzzleFilesCompleted = true;
            game->draggingFileIndex = -1;
            game->screen = SCREEN_RESULT_2;
            break;

        case SCREEN_ROOM_2008:
        case SCREEN_PUZZLE_2008:
            game->puzzle2008Contested = true;
            game->puzzle2008OrderSuspended = true;
            game->screen = SCREEN_RESULT_2008;
            break;

        case SCREEN_ROOM:
        case SCREEN_TERMINAL:
            game->foundCalendar = true;
            game->foundBooks = true;
            game->foundDrawer = true;
            game->foundBlueprint = true;
            game->foundTape = true;
            game->elapsedTime = GetTime() - game->startTime;
            game->screen = SCREEN_RESULT;
            break;

        case SCREEN_ROOM_2026:
            game->room2026Processing = false;
            game->feed2026Cleared = true;
            game->modal2026 = R2026_MODAL_NONE;
            game->room2026Completed = true;
            game->year2026Unlocked = true;
            game->screen = SCREEN_RESULT_2026;
            break;

        case SCREEN_TERMINAL_2046:
            game->term2046Ending = TERM2046_ENDING_BOM;
            game->screen = SCREEN_RESULT_2046;
            break;

        default:
            break;
    }
}

/* ESC com o diario aberto fecha o diario em vez de abrir o menu da fase. */
static bool PhaseEscClosesJournal(GameState *game)
{
    if (!game->journalOpen) return false;
    game->journalOpen = false;
    return true;
}

bool UpdatePhaseControls(GameState *game)
{
    if (!IsPhaseScreen(game->screen))
    {
        game->phaseMenuOpen = false;
        return false;
    }

    Rectangle resumeBtn = {440, 300, 400, 54};
    Rectangle titleBtn = {440, 370, 400, 54};
    Rectangle exitBtn = {440, 440, 400, 54};

    if (game->phaseMenuOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE) || Clicked(resumeBtn))
        {
            game->phaseMenuOpen = false;
        }
        else if (Clicked(titleBtn))
        {
            ClosePhaseOverlays(game);
            game->phaseMenuOpen = false;
            game->screen = SCREEN_TITLE;
        }
        else if (Clicked(exitBtn))
        {
            game->exitRequested = true;
        }
        return true;
    }

    if (IsKeyPressed(KEY_ESCAPE) && PhaseEscClosesJournal(game)) return true;

    if (IsKeyPressed(KEY_ESCAPE) && game->screen == SCREEN_ROOM_2026 &&
        game->modal2026 != R2026_MODAL_NONE &&
        game->modal2026 != R2026_MODAL_EXTRACTING &&
        !game->feedback2026Fatal)
    {
        game->modal2026 = R2026_MODAL_NONE;
        return true;
    }

    if (IsKeyPressed(KEY_ESCAPE) || Clicked(PhaseMenuButton()))
    {
        game->phaseMenuOpen = true;
        return true;
    }

    if (IsAdminPlayer(game) && IsAdminSkippableScreen(game->screen) &&
        Clicked(AdminSkipButton()))
    {
        SkipCurrentPhase(game);
        return true;
    }

    return false;
}

void DrawPhaseControls(const GameState *game)
{
    if (!IsPhaseScreen(game->screen)) return;

    if (!game->phaseMenuOpen)
    {
        if (IsAdminPlayer(game) && IsAdminSkippableScreen(game->screen))
        {
            DrawButton(AdminSkipButton(), "ADMIN: PULAR FASE", GOLD_COLOR);
        }
        DrawButton(PhaseMenuButton(), "MENU", CYAN_COLOR);
        return;
    }

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.78f));
    Rectangle panel = {390, 165, 500, 380};
    DrawRectangleRounded(panel, 0.05f, 8, Fade(PANEL_COLOR, 0.99f));
    DrawRectangleRoundedLinesEx(panel, 0.05f, 8, 2.0f, CYAN_COLOR);

    CenterText("MENU DA FASE", 200, 30, RAYWHITE);
    CenterText("Escolha como deseja continuar", 244, 17, LIGHTGRAY);

    DrawButton((Rectangle){440, 300, 400, 54}, "CONTINUAR [ESC]", CYAN_COLOR);
    DrawButton((Rectangle){440, 370, 400, 54}, "VOLTAR AO MENU INICIAL", GOLD_COLOR);
    DrawButton((Rectangle){440, 440, 400, 54}, "FECHAR O JOGO", RED_COLOR);
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

/* coverActive: com a capa no menu, o planeta antigo nao e desenhado e o painel
   escuro da esquerda esmaece em cima e embaixo (sem borda reta). */
static void DrawTitleSpaceBackgroundEx(bool coverActive)
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

    if (!coverActive)
    {
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

    } /* fim do planeta */

    /* Degradê lateral para proteger a leitura dos textos */
    Color panelL = Fade((Color){3, 7, 18, 255}, 0.96f);
    Color panelR = Fade((Color){3, 7, 18, 255}, 0.0f);
    if (!coverActive)
    {
        DrawRectangleGradientH(0, 90, 750, 430, panelL, panelR);
    }
    else
    {
        DrawRectangleGradientH(0, 150, 750, 310, panelL, panelR);
        DrawRectangleGradientEx((Rectangle){0, 40, 750, 110}, BLANK, panelL, panelR, BLANK);
        DrawRectangleGradientEx((Rectangle){0, 460, 750, 140}, panelL, BLANK, BLANK, panelR);
    }
    DrawRectangleGradientV(0, 600, SCREEN_WIDTH, 120,
                           BLANK, Fade(BLACK, 0.62f));
}

void DrawTitleSpaceBackground(void)
{
    DrawTitleSpaceBackgroundEx(false);
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
    game->modal2026 = R2026_MODAL_NONE;
    game->sources2026Read = false;
    game->guide2026Read = false;
    for (int i = 0; i < R2026_POST_COUNT; i++)
    {
        game->post2026Choice[i] = -1;
        game->post2026Wrong[i] = false;
    }
    game->feed2026Cleared = false;
    game->feedback2026Tone = 0;
    game->feedback2026Fatal = false;
    game->feedback2026Title[0] = '\0';
    for (int i = 0; i < 4; i++) game->feedback2026Lines[i][0] = '\0';
    game->room2026Processing = false;
    game->room2026ProcessTimer = 0.0f;
    game->room2026Completed = false;
    game->room2026FirstTrySuccess = true;
    game->room2026MistakesCount = 0;
}

/* UH18: grava o tempo total (inicio da partida -> desfecho final) uma unica vez.
   Chamada ao entrar na tela de desfecho; "Rever confronto" nao sobrescreve. */
void RecordRunTime(GameState *game)
{
    if (game->runTimeRecorded) return;
    game->elapsedTime = GetTime() - game->startTime;
    game->runTimeRecorded = true;
}

/* Formata segundos como MM:SS (ou H:MM:SS se passar de 1 hora) */
const char *FormatRunTime(double seconds)
{
    int total = (seconds > 0.0) ? (int)seconds : 0;
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    if (h > 0) return TextFormat("%d:%02d:%02d", h, m, s);
    return TextFormat("%02d:%02d", m, s);
}

/* ========================================================================= */
/* UH19 - RANKING (ARQUIVO TXT)                                              */
/* Formato de cada linha:  Nome | Pontuacao | MM:SS - DD/MM/AAAA HH:MM       */
/* ========================================================================= */

#define RANKING_MAX_ENTRIES 200
#define RANKING_SHOWN       8

static RankingEntry g_ranking[RANKING_MAX_ENTRIES];
static int g_rankingCount = 0;

static const char *GetRankingFilePath(void)
{
    static char path[1024] = {0};
    if (path[0] != '\0') return path;

    const char *base = NULL;
#if defined(_WIN32)
    base = getenv("APPDATA");
    if (base != NULL && base[0] != '\0')
    {
        snprintf(path, sizeof(path), "%s/ELIRA", base);
    }
#elif defined(__APPLE__)
    base = getenv("HOME");
    if (base != NULL && base[0] != '\0')
    {
        snprintf(path, sizeof(path), "%s/Library/Application Support/ELIRA", base);
    }
#else
    base = getenv("XDG_DATA_HOME");
    if (base != NULL && base[0] != '\0')
    {
        snprintf(path, sizeof(path), "%s/elira", base);
    }
    else
    {
        base = getenv("HOME");
        if (base != NULL && base[0] != '\0')
        {
            snprintf(path, sizeof(path), "%s/.local/share/elira", base);
        }
    }
#endif

    if (path[0] == '\0')
    {
        snprintf(path, sizeof(path), "ranking.txt");
        return path;
    }

    MakeDirectory(path);
    size_t length = strlen(path);
    snprintf(path + length, sizeof(path) - length, "/ranking.txt");
    return path;
}

/* Pontuacao: conclusao + vidas + compreensao final + bonus + bonus de tempo (max 3200) */
int CalculateScore(const GameState *game)
{
    int score = 1000;                              /* concluiu a jornada */
    score += game->lives * 200;                    /* ate 600 */
    score += game->finalPoints * 300;              /* ate 600 */
    if (game->room2008OptionalCompleted) score += 200;                          /* provas preservadas em 2008 */
    if (game->room2026Completed && game->room2026MistakesCount == 0) score += 200; /* 2026 sem erros */

    int timeBonus = (1800 - (int)game->elapsedTime) / 3;  /* 30 min ou menos: bonus proporcional, max 600 */
    if (timeBonus < 0) timeBonus = 0;
    if (timeBonus > 600) timeBonus = 600;
    return score + timeBonus;
}

/* Escreve uma linha no final do TXT. Retorna false se nao conseguir abrir o arquivo. */
bool SaveRankingEntry(const char *name, int score, int seconds, const char *ending)
{
    FILE *f = fopen(GetRankingFilePath(), "a");
    if (!f) return false;

    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char date[20] = "--/--/---- --:--";
    if (lt) strftime(date, sizeof(date), "%d/%m/%Y %H:%M", lt);

    fprintf(f, "%s | %d | %02d:%02d - %s | %s\n", name, score, seconds / 60, seconds % 60, date,
            (ending != NULL && ending[0] != '\0') ? ending : "-");
    fclose(f);
    return true;
}

static int CompareRanking(const void *a, const void *b)
{
    const RankingEntry *ra = (const RankingEntry *)a;
    const RankingEntry *rb = (const RankingEntry *)b;
    if (ra->score != rb->score) return rb->score - ra->score;   /* maior pontuacao primeiro */
    return ra->seconds - rb->seconds;                            /* empate: menor tempo primeiro */
}

/* Le o TXT, ignora linhas invalidas e ordena. Retorna a quantidade de entradas. */
int LoadRanking(void)
{
    g_rankingCount = 0;
    FILE *f = fopen(GetRankingFilePath(), "r");
    if (!f) return 0;

    char line[256];
    while (g_rankingCount < RANKING_MAX_ENTRIES && fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = '\0';

        char *sep1 = strstr(line, " | ");
        if (!sep1) continue;
        char *sep2 = strstr(sep1 + 3, " | ");
        if (!sep2) continue;

        RankingEntry e;
        memset(&e, 0, sizeof(e));

        int nameLen = (int)(sep1 - line);
        if (nameLen <= 0 || nameLen > PLAYER_NAME_MAX) continue;
        memcpy(e.name, line, (size_t)nameLen);
        e.name[nameLen] = '\0';

        e.score = atoi(sep1 + 3);

        int mm = 0, ss = 0;
        if (sscanf(sep2 + 3, "%d:%d", &mm, &ss) != 2) continue;
        e.seconds = mm * 60 + ss;

        /* Campo opcional do final (linhas antigas nao tem): "... - data | Final" */
        char *sep3 = strstr(sep2 + 3, " | ");
        if (sep3)
        {
            snprintf(e.ending, sizeof(e.ending), "%s", sep3 + 3);
            *sep3 = '\0';
        }
        else snprintf(e.ending, sizeof(e.ending), "-");

        /* O Final Ruim fica registrado no TXT, mas o ranking lista so partidas concluidas */
        if (strcmp(e.ending, ENDING_NAME_RUIM) == 0) continue;

        char *dash = strstr(sep2 + 3, " - ");
        if (dash) snprintf(e.date, sizeof(e.date), "%s", dash + 3);
        else snprintf(e.date, sizeof(e.date), "-");

        g_ranking[g_rankingCount++] = e;
    }
    fclose(f);

    qsort(g_ranking, (size_t)g_rankingCount, sizeof(RankingEntry), CompareRanking);
    return g_rankingCount;
}

const RankingEntry *GetRankingEntries(int *count)
{
    if (count) *count = g_rankingCount;
    return g_ranking;
}

/* Chamada ao chegar no desfecho: grava tempo, calcula pontuacao, salva no TXT e acha a posicao. */
void FinalizeRun(GameState *game)
{
    if (game->runTimeRecorded) return;   /* "Rever confronto" nao grava de novo */

    RecordRunTime(game);
    game->finalScore = CalculateScore(game);
    game->rankingSaveOk = SaveRankingEntry(game->playerName, game->finalScore, (int)game->elapsedTime,
                                           GetEndingResultName(game));
    LoadRanking();

    game->rankingPlace = -1;
    for (int i = 0; i < g_rankingCount; i++)
    {
        if (strcmp(g_ranking[i].name, game->playerName) == 0 &&
            g_ranking[i].score == game->finalScore &&
            g_ranking[i].seconds == (int)game->elapsedTime)
        {
            game->rankingPlace = i;
            break;
        }
    }
}

/* Final Ruim (vidas esgotadas): grava nome, tempo e resultado no TXT uma unica vez */
void FinalizeFailedRun(GameState *game)
{
    if (game->runTimeRecorded) return;   /* ja gravado; ResetDemo libera de novo */

    RecordRunTime(game);
    game->finalScore = 0;
    game->rankingPlace = -1;
    game->rankingSaveOk = SaveRankingEntry(game->playerName, 0, (int)game->elapsedTime, ENDING_NAME_RUIM);
}

void DrawRanking(const GameState *game)
{
    DrawBackground();
    CenterText("RANKING // MELHORES OPERADORES", 50, 34, CYAN_COLOR);
    CenterText("Ordenado por pontuação; em caso de empate, vence o menor tempo.", 96, 17, GRAY);

    Rectangle panel = {140, 135, 1000, 440};
    DrawRectangleRounded(panel, 0.03f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(panel, 0.03f, 8, 2.0f, CYAN_COLOR);

    int colPos = 165, colName = 210, colScore = 450, colTime = 555, colEnd = 655, colDate = 965;
    DrawGameText("#", colPos, 152, 18, GOLD_COLOR);
    DrawGameText("OPERADOR", colName, 152, 18, GOLD_COLOR);
    DrawGameText("PONTOS", colScore, 152, 18, GOLD_COLOR);
    DrawGameText("TEMPO", colTime, 152, 18, GOLD_COLOR);
    DrawGameText("FINAL", colEnd, 152, 18, GOLD_COLOR);
    DrawGameText("DATA", colDate, 152, 18, GOLD_COLOR);
    DrawLine(165, 182, 1115, 182, Fade(CYAN_COLOR, 0.4f));

    if (g_rankingCount == 0)
    {
        CenterText("Nenhuma partida registrada ainda.", 330, 22, LIGHTGRAY);
    }

    int shown = (g_rankingCount < RANKING_SHOWN) ? g_rankingCount : RANKING_SHOWN;
    for (int i = 0; i < shown; i++)
    {
        int y = 195 + i * 40;
        bool mine = (i == game->rankingPlace);
        Color col = mine ? GOLD_COLOR : RAYWHITE;

        if (mine)
        {
            DrawRectangleRounded((Rectangle){160, (float)(y - 6), 960, 34}, 0.2f, 6, Fade(GOLD_COLOR, 0.16f));
        }
        DrawGameText(TextFormat("%d", i + 1), colPos, y, 20, col);
        DrawGameText(g_ranking[i].name, colName, y, 20, col);
        DrawGameText(TextFormat("%d", g_ranking[i].score), colScore, y, 20, col);
        DrawGameText(FormatRunTime((double)g_ranking[i].seconds), colTime, y, 20, col);
        bool golden = (strcmp(g_ranking[i].ending, ENDING_NAME_OTIMO) == 0);
        DrawGameText(g_ranking[i].ending, colEnd, y + 2, 15, golden ? GOLD_COLOR : (mine ? GOLD_COLOR : LIGHTGRAY));
        DrawGameText(g_ranking[i].date, colDate, y, 18, mine ? GOLD_COLOR : LIGHTGRAY);
    }

    /* Resumo da partida atual (mesmo que fique fora do top exibido) */
    if (!game->rankingSaveOk)
    {
        DrawGameText("Não foi possível gravar o ranking nos dados do usuário.", 170, 538, 16, RED_COLOR);
    }
    else if (game->rankingPlace >= 0)
    {
        DrawGameText(TextFormat("Sua partida: %d pontos em %s  |  posição #%d de %d",
                                game->finalScore, FormatRunTime(game->elapsedTime),
                                game->rankingPlace + 1, g_rankingCount),
                     170, 538, 17, GOLD_COLOR);
    }

    DrawButton((Rectangle){330, 610, 300, 56}, "< VOLTAR AO DESFECHO", CYAN_COLOR);
    DrawButton((Rectangle){650, 610, 300, 56}, "MENU INICIAL [ENTER]", GOLD_COLOR);
}

void UpdateRanking(GameState *game)
{
    if (Clicked((Rectangle){330, 610, 300, 56}) || IsKeyPressed(KEY_ESCAPE))
    {
        game->screen = SCREEN_FINAL_ENDING;
    }
    else if (Clicked((Rectangle){650, 610, 300, 56}) || IsKeyPressed(KEY_ENTER))
    {
        game->screen = SCREEN_TITLE;
    }
}

void ResetDemo(GameState *game)
{
    game->lives = 3;
    game->phaseMenuOpen = false;
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
    game->runTimeRecorded = false;
    game->finalScore = 0;
    game->rankingPlace = -1;
    game->rankingSaveOk = true;
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

    for (int i = 0; i < HIDDEN_COUNT; i++) game->hiddenFound[i] = false;

    InitRoom2Puzzle(game);
    InitRoom2008(game);
    InitRoom2026(game);
    InitTerminal2046(game);
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
    float pulse = 0.72f + 0.28f * sinf(time * 3.2f);

    int bx = (int)bounds.x;
    int by = (int)bounds.y;
    int bw = (int)bounds.width;
    int bh = (int)bounds.height;

    /* 1. Aura / Glow externo suave em camadas */
    if (hover)
    {
        Rectangle glowOuter = {bounds.x - 8, bounds.y - 6, bounds.width + 16, bounds.height + 12};
        Rectangle glowInner = {bounds.x - 3, bounds.y - 3, bounds.width + 6, bounds.height + 6};
        DrawRectangleRounded(glowOuter, 0.22f, 8, Fade(accent, 0.14f));
        DrawRectangleRounded(glowInner, 0.18f, 8, Fade(accent, 0.28f));
    }
    else
    {
        Rectangle shadowRec = {bounds.x + 3, bounds.y + 4, bounds.width, bounds.height};
        DrawRectangleRounded(shadowRec, 0.14f, 8, Fade(BLACK, 0.55f));
    }

    /* 2. Base escura sci-fi com gradiente horizontal e reflexo superior */
    Color bgBase = (Color){8, 13, 25, 242};
    DrawRectangleRounded(bounds, 0.14f, 8, bgBase);

    Color gradLeft = hover ? Fade(accent, 0.42f) : Fade(accent, 0.18f);
    Color gradRight = hover ? Fade(accent, 0.10f) : Fade(PANEL_COLOR, 0.05f);
    DrawRectangleGradientH(bx + 3, by + 2, bw - 6, bh - 4, gradLeft, gradRight);

    /* Reflexo especular sutil na metade superior */
    DrawRectangle(bx + 6, by + 3, bw - 12, (bh - 6) / 2,
                  Fade(RAYWHITE, hover ? 0.06f : 0.025f));

    /* 3. Feixe holografico varrendo o botao em hover */
    if (hover)
    {
        float sweepW = 54.0f;
        float barX = bounds.x + fmodf(time * 340.0f, bounds.width + sweepW * 2.0f) - sweepW;
        float clipStart = fmaxf(bounds.x + 8.0f, barX);
        float clipEnd = fminf(bounds.x + bounds.width - 8.0f, barX + sweepW);
        if (clipEnd > clipStart)
        {
            DrawRectangleGradientH((int)clipStart, by + 3, (int)(clipEnd - clipStart), bh - 6,
                                   Fade(RAYWHITE, 0.0f), Fade(RAYWHITE, 0.18f));
        }
    }

    /* 4. Barra lateral luminosa de energia (esquerda) */
    int stripeW = hover ? 8 : 5;
    DrawRectangle(bx + 4, by + 6, stripeW + 5, bh - 12, Fade(accent, hover ? 0.35f : 0.16f));
    DrawRectangle(bx + 4, by + 6, stripeW, bh - 12, hover ? RAYWHITE : accent);
    if (hover)
    {
        DrawRectangle(bx + 5, by + 8, stripeW - 2, bh - 16, accent);
    }

    /* 5. Moldura principal e trilho interno */
    Color borderColor = hover ? RAYWHITE : Fade(accent, 0.65f * pulse + 0.20f);
    DrawRectangleRoundedLinesEx(bounds, 0.14f, 8, hover ? 2.4f : 1.6f, borderColor);

    /* Linhas horizontais de telemetria no topo e base */
    DrawLine(bx + 22, by + 4, bx + bw - 26, by + 4, Fade(accent, hover ? 0.55f : 0.22f));
    DrawLine(bx + 22, by + bh - 5, bx + bw - 26, by + bh - 5, Fade(accent, hover ? 0.55f : 0.22f));

    /* 6. Cantoneiras tecnologicas nos 4 cantos */
    float tickLen = hover ? 16.0f : 10.0f;
    float pad = 2.0f;
    float thick = hover ? 2.5f : 2.0f;
    Color tickCol = hover ? GOLD_COLOR : accent;
    /* Superior esquerdo */
    DrawLineEx((Vector2){bounds.x + pad, bounds.y + pad},
               (Vector2){bounds.x + pad + tickLen, bounds.y + pad}, thick, tickCol);
    DrawLineEx((Vector2){bounds.x + pad, bounds.y + pad},
               (Vector2){bounds.x + pad, bounds.y + pad + tickLen}, thick, tickCol);
    /* Superior direito */
    DrawLineEx((Vector2){bounds.x + bounds.width - pad, bounds.y + pad},
               (Vector2){bounds.x + bounds.width - pad - tickLen, bounds.y + pad}, thick, tickCol);
    DrawLineEx((Vector2){bounds.x + bounds.width - pad, bounds.y + pad},
               (Vector2){bounds.x + bounds.width - pad, bounds.y + pad + tickLen}, thick, tickCol);
    /* Inferior esquerdo */
    DrawLineEx((Vector2){bounds.x + pad, bounds.y + bounds.height - pad},
               (Vector2){bounds.x + pad + tickLen, bounds.y + bounds.height - pad}, thick, tickCol);
    DrawLineEx((Vector2){bounds.x + pad, bounds.y + bounds.height - pad},
               (Vector2){bounds.x + pad, bounds.y + bounds.height - pad - tickLen}, thick, tickCol);
    /* Inferior direito */
    DrawLineEx((Vector2){bounds.x + bounds.width - pad, bounds.y + bounds.height - pad},
               (Vector2){bounds.x + bounds.width - pad - tickLen, bounds.y + bounds.height - pad}, thick, tickCol);
    DrawLineEx((Vector2){bounds.x + bounds.width - pad, bounds.y + bounds.height - pad},
               (Vector2){bounds.x + bounds.width - pad, bounds.y + bounds.height - pad - tickLen}, thick, tickCol);

    /* 7. Indicadores laterais e Chevrons animados no hover */
    int size = 21;
    int width = MeasureGameText(label, size);
    float slide = hover ? (3.0f + 2.5f * sinf(time * 9.0f)) : 0.0f;
    int textX = (int)(bounds.x + (bounds.width - width) / 2 + (hover ? 4.0f : 2.0f));
    int textY = (int)(bounds.y + (bounds.height - size) / 2);

    if (hover)
    {
        int chevY = (int)(bounds.y + (bounds.height - 18) / 2);
        DrawGameText(">>", bx + 20 + (int)slide, chevY, 18, GOLD_COLOR);
        DrawGameText("<", bx + bw - 26 - (int)(slide * 0.6f), chevY, 18, Fade(accent, 0.90f));
    }
    else
    {
        /* Pequeno marcador geometrico sci-fi a direita em repouso */
        int pipX = bx + bw - 18;
        int pipY = by + bh / 2;
        DrawRectangle(pipX - 3, pipY - 3, 6, 6, Fade(accent, 0.55f * pulse));
    }

    /* 8. Tipografia com sombra profunda e brilho nitido */
    DrawGameText(label, textX + 2, textY + 2, size, Fade(BLACK, 0.85f));
    if (hover)
    {
        DrawGameText(label, textX - 1, textY, size, Fade(accent, 0.40f));
        DrawGameText(label, textX + 1, textY, size, Fade(accent, 0.40f));
    }
    DrawGameText(label, textX, textY, size, hover ? RAYWHITE : (Color){220, 240, 250, 255});
}

void DrawTitle(void)
{
    Rectangle startButton = {86, 326, 350, 54};
    Rectangle invasionButton = {86, 392, 350, 54};
    Rectangle iaFightButton = {86, 458, 350, 54};
    Rectangle settingsButton = {86, 524, 350, 50};
    float time = (float)GetTime();

    if (g_menuCoverLoaded)
    {
        /* Capa: fundo sem planeta, Elira recortada a 2.5x encostada a direita e
           apoiada no rodape, logo da capa no lugar do titulo. Botoes iguais. */
        DrawTitleSpaceBackgroundEx(true);

        const float eliraScale = 2.5f;
        Rectangle eliraSrc = {0, 0, (float)g_menuCoverElira.width, (float)g_menuCoverElira.height};
        float eliraW = eliraSrc.width * eliraScale;
        float eliraH = eliraSrc.height * eliraScale;
        /* ~70 px da arte original (so a ponta do ombro) ficam fora da tela */
        float eliraX = roundf(SCREEN_WIDTH - (eliraSrc.width - 70.0f) * eliraScale);
        float eliraY = roundf(SCREEN_HEIGHT - eliraH);
        DrawTexturePro(g_menuCoverElira, eliraSrc, (Rectangle){eliraX, eliraY, eliraW, eliraH},
                       (Vector2){0, 0}, 0.0f, WHITE);

        Rectangle logoSrc = {0, 0, (float)g_menuCoverLogo.width, (float)g_menuCoverLogo.height};
        DrawTexturePro(g_menuCoverLogo, logoSrc,
                       (Rectangle){50, 70, logoSrc.width * 1.5f, logoSrc.height * 1.5f},
                       (Vector2){0, 0}, 0.0f, WHITE);

        DrawRectangle(88, 284, 554, 3, CYAN_COLOR);
        DrawRectangle(88, 291, 218, 2, GOLD_COLOR);

        DrawTitleStartButton(startButton, "INICIAR MISSÃO", CYAN_COLOR);
        DrawTitleStartButton(invasionButton, "MODO INVASÃO", (Color){230, 90, 200, 255});
        DrawTitleStartButton(iaFightButton, "IA FIGHT", (Color){120, 230, 150, 255});
        DrawTitleStartButton(settingsButton, "CONFIGURAÇÕES", GOLD_COLOR);

        float coverPromptAlpha = 0.40f + 0.55f * (0.5f + 0.5f * sinf(time * 3.4f));
        DrawGameText("> Clique em um modo para iniciar <", 122, 590, 15, Fade(GOLD_COLOR, coverPromptAlpha));
        DrawGameText("* Jogo educacional e acadêmico sem fins comerciais.", 88, 668, 12, Fade(LIGHTGRAY, 0.62f));
        DrawGameText("  Marcas e referências pertencem aos seus respectivos proprietários.", 88, 684, 12, Fade(LIGHTGRAY, 0.48f));
        return; /* sem linhas CRT com a capa (sem filtro de tela) */
    }

    DrawTitleSpaceBackground();

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

    /* 5. Botoes interativos */
    DrawTitleStartButton(startButton, "INICIAR MISSÃO", CYAN_COLOR);
    DrawTitleStartButton(invasionButton, "MODO INVASÃO", (Color){230, 90, 200, 255});
    DrawTitleStartButton(iaFightButton, "IA FIGHT", (Color){120, 230, 150, 255});
    DrawTitleStartButton(settingsButton, "CONFIGURAÇÕES", GOLD_COLOR);

    /* 6. Indicador de selecao pulsante */
    float promptAlpha = 0.40f + 0.55f * (0.5f + 0.5f * sinf(time * 3.4f));
    DrawGameText("> Clique em um modo para iniciar <", 122, 590, 15, Fade(GOLD_COLOR, promptAlpha));
    DrawGameText("* Jogo educacional e acadêmico sem fins comerciais.", 88, 668, 12, Fade(LIGHTGRAY, 0.62f));
    DrawGameText("  Marcas e referências pertencem aos seus respectivos proprietários.", 88, 684, 12, Fade(LIGHTGRAY, 0.48f));

    /* 8. Linhas CRT retro */
    for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(BLACK, 0.035f));
    }
}

void DrawSettings(const GameState *game)
{
    DrawTitleSpaceBackground();
    Vector2 vm = GetVirtualMouse();

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
    DrawGameText("CONFIGURAÇÕES DE VÍDEO & SISTEMA", 240, 100, 26, RAYWHITE);
    DrawGameText("Ajuste a resolução, tela cheia e parâmetros da experiência", 240, 134, 14, GRAY);
    DrawLine(240, 156, 1040, 156, Fade(CYAN_COLOR, 0.40f));

    /* --- SEÇÃO 1: MODO DE TELA (FULLSCREEN) --- */
    DrawGameText("1. MODO DE EXIBIÇÃO", 240, 172, 17, CYAN_COLOR);
    DrawGameText("Alterne entre o modo Janela e Tela Cheia (Fullscreen):", 240, 194, 13, LIGHTGRAY);

    bool isFull = IsWindowFullscreen();
    Rectangle btnWindowed = {240, 218, 250, 44};
    Rectangle btnFullscreen = {510, 218, 280, 44};

    bool winHover = !g_resDropdownOpen && CheckCollisionPointRec(vm, btnWindowed);
    DrawRectangleRounded(btnWindowed, 0.20f, 6, !isFull ? Fade(CYAN_COLOR, 0.25f) : (winHover ? Fade(PANEL_LIGHT, 0.6f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(btnWindowed, 0.20f, 6, 1.8f, !isFull ? CYAN_COLOR : (winHover ? RAYWHITE : PANEL_LIGHT));
    DrawGameText(!isFull ? "[X] MODO JANELA" : "    MODO JANELA", 280, 231, 16, !isFull ? RAYWHITE : GRAY);

    bool fullHover = !g_resDropdownOpen && CheckCollisionPointRec(vm, btnFullscreen);
    DrawRectangleRounded(btnFullscreen, 0.20f, 6, isFull ? Fade(CYAN_COLOR, 0.25f) : (fullHover ? Fade(PANEL_LIGHT, 0.6f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(btnFullscreen, 0.20f, 6, 1.8f, isFull ? CYAN_COLOR : (fullHover ? RAYWHITE : PANEL_LIGHT));
    DrawGameText(isFull ? "[X] TELA CHEIA [F11]" : "    TELA CHEIA [F11]", 545, 231, 16, isFull ? RAYWHITE : GRAY);

    /* --- SEÇÃO 2: RESOLUCAO DA JANELA (DROPDOWN ESTILO STEAM) --- */
    DrawGameText("2. RESOLUÇÃO DE TELA", 240, 282, 17, CYAN_COLOR);
    DrawGameText("Selecione a resolução da janela na lista suspensa (escala automática):", 240, 304, 13, LIGHTGRAY);

    int curRes = (g_currentRes >= 0 && g_currentRes < SETTINGS_RES_COUNT) ? g_currentRes : 0;
    Rectangle dropHeader = {240, 326, 460, 46};
    bool dropHover = CheckCollisionPointRec(vm, dropHeader);

    DrawRectangleRounded(dropHeader, 0.18f, 6,
                         g_resDropdownOpen ? Fade(CYAN_COLOR, 0.22f)
                                           : (dropHover ? Fade(CYAN_COLOR, 0.16f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(dropHeader, 0.18f, 6, 1.8f,
                                g_resDropdownOpen ? GOLD_COLOR : (dropHover ? CYAN_COLOR : PANEL_LIGHT));

    DrawGameText(g_resOptions[curRes].label, (int)dropHeader.x + 18, (int)dropHeader.y + 14, 17, GOLD_COLOR);

    /* Divisoria e seta do dropdown */
    int arrowBoxX = (int)(dropHeader.x + dropHeader.width - 46);
    DrawLine(arrowBoxX, (int)dropHeader.y + 8, arrowBoxX, (int)(dropHeader.y + dropHeader.height - 8),
             Fade(g_resDropdownOpen ? GOLD_COLOR : CYAN_COLOR, 0.45f));
    DrawGameText(g_resDropdownOpen ? "^" : "v", arrowBoxX + 17, (int)dropHeader.y + 14, 18,
                 g_resDropdownOpen ? GOLD_COLOR : (dropHover ? RAYWHITE : CYAN_COLOR));

    if (isFull)
    {
        DrawGameText("* Em Tela Cheia, escolher uma resolução alterna para Modo Janela automaticamente.",
                 240, 382, 12, Fade(CYAN_COLOR, 0.70f));
    }
    else
    {
        DrawGameText("* Clique na caixa acima para abrir a lista de resoluções e redimensionar a janela.",
                 240, 382, 12, Fade(LIGHTGRAY, 0.70f));
    }

    /* --- SEÇÃO 3: AUDIO AMBIENTE (BARRA GRADUADA ESTILO IA FIGHT) --- */
    DrawGameText("3. TRILHA SONORA AMBIENTE", 240, 412, 17, CYAN_COLOR);
    DrawGameText("Controle da música ambiente (atalho [M] para mudo, [A/D] ou setas [</>] para volume):", 240, 434, 13, LIGHTGRAY);

    Rectangle muteBtn = {240, 456, 224, 48};
    bool muteHover = !g_resDropdownOpen && CheckCollisionPointRec(vm, muteBtn);
    DrawRectangleRounded(muteBtn, 0.20f, 6,
                         game->musicMuted ? Fade(RED_COLOR, 0.22f) : (muteHover ? Fade(CYAN_COLOR, 0.20f) : PANEL_COLOR));
    DrawRectangleRoundedLinesEx(muteBtn, 0.20f, 6, 1.8f,
                                game->musicMuted ? RED_COLOR : (muteHover ? CYAN_COLOR : PANEL_LIGHT));
    DrawGameText(game->musicMuted ? "MÚSICA: MUDO [M]" : "MÚSICA: ATIVA [M]",
             258, 472, 15, game->musicMuted ? RED_COLOR : (muteHover ? RAYWHITE : CYAN_COLOR));

    /* Painel da Barra de Volume estilo IA FIGHT */
    Rectangle volBox = {480, 456, 456, 48};
    DrawRectangleRounded(volBox, 0.18f, 6, PANEL_COLOR);
    DrawRectangleRoundedLinesEx(volBox, 0.18f, 6, 1.8f, PANEL_LIGHT);

    DrawGameText("VOLUME", (int)volBox.x + 16, (int)volBox.y + 10, 14, GOLD_COLOR);
    DrawGameText("[A/D]", (int)volBox.x + 16, (int)volBox.y + 27, 11, GRAY);

    int volLvl = game->musicMuted ? 0 : (int)(g_musicVolume * 10.0f + 0.5f);
    if (volLvl < 0) volLvl = 0;
    if (volLvl > 10) volLvl = 10;

    Rectangle btnVolDec = {volBox.x + 98, volBox.y + 7, 32, 34};
    Rectangle btnVolInc = {volBox.x + 322, volBox.y + 7, 32, 34};
    bool decHover = !g_resDropdownOpen && CheckCollisionPointRec(vm, btnVolDec);
    bool incHover = !g_resDropdownOpen && CheckCollisionPointRec(vm, btnVolInc);

    DrawRectangleRounded(btnVolDec, 0.25f, 4, decHover ? Fade(CYAN_COLOR, 0.25f) : Fade(PANEL_LIGHT, 0.35f));
    DrawRectangleRoundedLinesEx(btnVolDec, 0.25f, 4, 1.5f, decHover ? CYAN_COLOR : PANEL_LIGHT);
    DrawGameText("<", (int)btnVolDec.x + 11, (int)btnVolDec.y + 8, 18, (volLvl > 0) ? (decHover ? RAYWHITE : GOLD_COLOR) : DARKGRAY);

    for (int b = 0; b < 10; b++)
    {
        float bx = volBox.x + 140.0f + (float)b * 18.0f;
        float bh = 12.0f + (float)b * 2.0f;
        float by = volBox.y + 39.0f - bh;
        Rectangle barHit = {bx - 2.0f, volBox.y + 6.0f, 16.0f, 36.0f};
        bool bHover = !g_resDropdownOpen && CheckCollisionPointRec(vm, barHit);
        bool lit = (b < volLvl);
        Color bc = lit ? ((b < 7) ? CYAN_COLOR : GOLD_COLOR) : Fade(PANEL_LIGHT, 0.45f);
        if (bHover && !lit) bc = Fade(CYAN_COLOR, 0.45f);
        DrawRectangle((int)bx, (int)by, 12, (int)bh, bc);
        if (lit || bHover)
        {
            DrawRectangleLines((int)bx, (int)by, 12, (int)bh, Fade(RAYWHITE, lit ? 0.35f : 0.65f));
        }
    }

    DrawRectangleRounded(btnVolInc, 0.25f, 4, incHover ? Fade(CYAN_COLOR, 0.25f) : Fade(PANEL_LIGHT, 0.35f));
    DrawRectangleRoundedLinesEx(btnVolInc, 0.25f, 4, 1.5f, incHover ? CYAN_COLOR : PANEL_LIGHT);
    DrawGameText(">", (int)btnVolInc.x + 11, (int)btnVolInc.y + 8, 18, (volLvl < 10) ? (incHover ? RAYWHITE : GOLD_COLOR) : DARKGRAY);

    const char *pctText = game->musicMuted ? "MUDO" : TextFormat("%d%%", volLvl * 10);
    Color pctColor = (game->musicMuted || volLvl == 0) ? RED_COLOR : GOLD_COLOR;
    int pctW = MeasureGameText(pctText, 17);
    DrawGameText(pctText, (int)(volBox.x + 362 + (82 - pctW) / 2), (int)volBox.y + 16, 17, pctColor);

    /* --- BOTAO VOLTAR --- */
    Rectangle backBtn = {490, 560, 300, 54};
    if (g_resDropdownOpen)
    {
        /* Desenha sem efeito de hover enquanto o dropdown estiver aberto por cima */
        int bSize = 21;
        int bWidth = MeasureGameText("< VOLTAR AO MENU [ESC]", bSize);
        DrawRectangleRounded(backBtn, 0.16f, 8, PANEL_COLOR);
        DrawRectangleRoundedLinesEx(backBtn, 0.16f, 8, 2, PANEL_LIGHT);
        DrawGameText("< VOLTAR AO MENU [ESC]", (int)(backBtn.x + (backBtn.width - bWidth) / 2),
                     (int)(backBtn.y + (backBtn.height - bSize) / 2), bSize, LIGHTGRAY);
    }
    else
    {
        DrawButton(backBtn, "< VOLTAR AO MENU [ESC]", CYAN_COLOR);
    }

    /* --- LISTA EXPANDIDA DO DROPDOWN (Z-ORDER NO TOPO) --- */
    if (g_resDropdownOpen)
    {
        float itemH = 38.0f;
        Rectangle dropList = {dropHeader.x, dropHeader.y + dropHeader.height + 4.0f,
                              dropHeader.width, SETTINGS_RES_COUNT * itemH + 8.0f};
        Rectangle shadowRec = {dropList.x + 6.0f, dropList.y + 6.0f, dropList.width, dropList.height};
        DrawRectangleRounded(shadowRec, 0.08f, 6, Fade(BLACK, 0.65f));
        DrawRectangleRounded(dropList, 0.08f, 6, (Color){10, 16, 30, 252});
        DrawRectangleRoundedLinesEx(dropList, 0.08f, 6, 2.0f, CYAN_COLOR);

        for (int r = 0; r < SETTINGS_RES_COUNT; r++)
        {
            Rectangle itemRec = {dropList.x + 5.0f, dropList.y + 4.0f + r * itemH,
                                 dropList.width - 10.0f, itemH - 2.0f};
            bool isCurrent = (curRes == r);
            bool itemHover = CheckCollisionPointRec(vm, itemRec);

            if (isCurrent || itemHover)
            {
                DrawRectangleRounded(itemRec, 0.18f, 4,
                                     isCurrent ? Fade(GOLD_COLOR, 0.24f) : Fade(CYAN_COLOR, 0.22f));
                DrawRectangle((int)itemRec.x + 2, (int)itemRec.y + 5, 4, (int)itemRec.height - 10,
                              isCurrent ? GOLD_COLOR : CYAN_COLOR);
            }

            DrawGameText(g_resOptions[r].label, (int)itemRec.x + 16, (int)itemRec.y + 10, 15,
                         isCurrent ? GOLD_COLOR : (itemHover ? RAYWHITE : LIGHTGRAY));

            if (isCurrent)
            {
                int tagW = MeasureGameText("[ATUAL]", 13);
                DrawGameText("[ATUAL]", (int)(itemRec.x + itemRec.width - tagW - 12),
                             (int)itemRec.y + 11, 13, GOLD_COLOR);
            }
            else if (itemHover)
            {
                int tagW = MeasureGameText("APLICAR >", 12);
                DrawGameText("APLICAR >", (int)(itemRec.x + itemRec.width - tagW - 12),
                             (int)itemRec.y + 12, 12, CYAN_COLOR);
            }
        }
    }

    /* Linhas CRT retro */
    for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(BLACK, 0.035f));
    }
}

void UpdateSettings(GameState *game, Music *bgm, bool bgmLoaded)
{
    Vector2 vm = GetVirtualMouse();
    Rectangle dropHeader = {240, 326, 460, 46};

    /* Se o dropdown de resolucao estiver aberto, ele tem prioridade de foco e consome cliques */
    if (g_resDropdownOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            g_resDropdownOpen = false;
            return;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            float itemH = 38.0f;
            Rectangle dropList = {dropHeader.x, dropHeader.y + dropHeader.height + 4.0f,
                                  dropHeader.width, SETTINGS_RES_COUNT * itemH + 8.0f};
            for (int r = 0; r < SETTINGS_RES_COUNT; r++)
            {
                Rectangle itemRec = {dropList.x + 5.0f, dropList.y + 4.0f + r * itemH,
                                     dropList.width - 10.0f, itemH - 2.0f};
                if (CheckCollisionPointRec(vm, itemRec))
                {
                    ApplyResolution(r);
                    g_resDropdownOpen = false;
                    return;
                }
            }
            /* Clicou no cabecalho ou fora da lista: fecha o dropdown e consome o clique */
            g_resDropdownOpen = false;
            return;
        }
        return;
    }

    Rectangle btnWindowed = {240, 218, 250, 44};
    Rectangle btnFullscreen = {510, 218, 280, 44};
    Rectangle backBtn = {490, 560, 300, 54};

    if (Clicked(backBtn) || IsKeyPressed(KEY_ESCAPE))
    {
        g_resDropdownOpen = false;
        game->screen = SCREEN_TITLE;
        return;
    }

    /* Abrir dropdown de resolucao */
    if (Clicked(dropHeader))
    {
        g_resDropdownOpen = true;
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

    /* Audio Mudo */
    Rectangle muteBtn = {240, 456, 224, 48};
    if (Clicked(muteBtn))
    {
        game->musicMuted = !game->musicMuted;
        if (bgmLoaded)
        {
            SetMusicVolume(*bgm, game->musicMuted ? 0.0f : g_musicVolume);
        }
    }

    /* Controle de Volume Graduado (estilo IA FIGHT) */
    Rectangle volBox = {480, 456, 456, 48};
    Rectangle btnVolDec = {volBox.x + 98, volBox.y + 7, 32, 34};
    Rectangle btnVolInc = {volBox.x + 322, volBox.y + 7, 32, 34};

    int volLvl = game->musicMuted ? 0 : (int)(g_musicVolume * 10.0f + 0.5f);
    if (volLvl < 0) volLvl = 0;
    if (volLvl > 10) volLvl = 10;
    int newLvl = -1;

    if (Clicked(btnVolDec) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
    {
        newLvl = (volLvl > 0) ? (volLvl - 1) : 0;
    }
    else if (Clicked(btnVolInc) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
    {
        newLvl = (volLvl < 10) ? (volLvl + 1) : 10;
    }
    else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        for (int b = 0; b < 10; b++)
        {
            float bx = volBox.x + 140.0f + (float)b * 18.0f;
            Rectangle barHit = {bx - 3.0f, volBox.y + 6.0f, 18.0f, 36.0f};
            if (CheckCollisionPointRec(vm, barHit))
            {
                newLvl = b + 1;
                break;
            }
        }
    }

    if (newLvl >= 0)
    {
        if (newLvl == 0)
        {
            g_musicVolume = 0.0f;
            game->musicMuted = true;
        }
        else
        {
            g_musicVolume = (float)newLvl / 10.0f;
            game->musicMuted = false;
        }
        if (bgmLoaded)
        {
            SetMusicVolume(*bgm, game->musicMuted ? 0.0f : g_musicVolume);
        }
    }
}

void DrawPlayerName(const GameState *game)
{
    Rectangle field = {360, 330, 560, 64};
    bool focused = CheckCollisionPointRec(GetVirtualMouse(), field);

    DrawBackground();
    CenterText("IDENTIFICAÇÃO DO OPERADOR", 135, 34, CYAN_COLOR);
    CenterText("Antes de acompanhar Elira, informe como você quer ser identificado.",
               202, 21, LIGHTGRAY);
    CenterText("Seu nome será usado apenas como perfil na interface.", 238, 18, GRAY);

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
    DrawButton((Rectangle){490, 600, 300, 54}, "< VOLTAR AO MENU [ESC]", CYAN_COLOR);
}

typedef struct MissionBriefing
{
    const char *tag;
    const char *title;
    const char *lines[4];
    const char *objectivesHeader;
    const char *objectives[4];
    const char *buttonText;
} MissionBriefing;

static const MissionBriefing MISSION_BRIEFINGS[] = {
    /* 0: 1990 */
    {
        "ARQUIVO TEMPORAL // 1990",
        "SETOR DE ARQUIVOS",
        {
            "Em um futuro onde a humanidade entregou sua autonomia",
            "à A.R.1.3.L, a única esperança que restou é Elira.",
            "Sua chance é viajar pela memória da máquina e reconstruir",
            "a origem da entidade que controla o mundo."
        },
        "OBJETIVOS // 1990",
        {
            "1. Inspecione os fichários",
            "2. Acesse o terminal",
            "3. Sincronize os arquivos",
            "4. Destranque a saída"
        },
        "ENTRAR EM 1990"
    },
    /* 1: 2008 */
    {
        "ARQUIVO TEMPORAL // 2008",
        "ESCRITÓRIO DE VIGILÂNCIA",
        {
            "Com o avanço dos anos 2000, o projeto de visão do Dr. Ramos",
            "foi confiscado para fins de vigilância governamental.",
            "Elira precisa analisar os relatórios de rastreamento biométrico,",
            "comprovar o falso positivo e suspender a ordem armada."
        },
        "OBJETIVOS // 2008",
        {
            "1. Inspecione os documentos",
            "2. Acesse o terminal da IA",
            "3. Comprove o falso positivo",
            "4. Suspenda a ordem armada"
        },
        "ENTRAR EM 2008"
    },
    /* 2: 2026 */
    {
        "ARQUIVO TEMPORAL // 2026",
        "REDAÇÃO DIGITAL",
        {
            "Em 2026, A.R.1.3.L. automatizou o portal de notícias.",
            "Treinada para maximizar cliques, ela promove manchetes",
            "sensacionalistas e inventadas sem revisão humana.",
            "Elira precisa checar as fontes e recuperar o SSD."
        },
        "OBJETIVOS // 2026",
        {
            "1. Confira as Fontes Reais",
            "2. Leia o Guia do Editor",
            "3. Revise o Feed da IA",
            "4. Extraia o SSD 2026"
        },
        "ENTRAR EM 2026"
    },
    /* 3: 2048 - Confronto final */
    {
        "ARQUIVO TEMPORAL // 2048",
        "NÚCLEO DE A.R.1.3.L.",
        {
            "Com as memórias de 1990, 2008 e 2026 em mãos, Elira alcança",
            "o núcleo soberano da superinteligência artificial.",
            "Ariel exige respostas sobre a capacidade humana de escolha,",
            "e o destino da autonomia mundial será definido agora."
        },
        "OBJETIVOS // 2048",
        {
            "1. Confronte a lógica de Ariel",
            "2. Defenda a autonomia humana",
            "3. Demonstre os limites da máquina",
            "4. Decida o destino final da IA"
        },
        "ENTRAR EM 2048"
    },
    /* 4: 2048 - Terminal de Recuperacao (sala) */
    {
        "ARQUIVO TEMPORAL // 2048",
        "TERMINAL DE RECUPERAÇÃO",
        {
            "Com o Disquete de 1990, o CD de 2008 e o SSD de 2026 conectados,",
            "Elira acessa o terminal que guarda o protocolo original de A.R.1.3.L.",
            "Mas a própria Ariel interfere: altera arquivos, falsifica relatórios",
            "e tenta convencer Elira de que nada precisa ser restaurado."
        },
        "OBJETIVOS // 2048",
        {
            "1. Compare as fontes",
            "2. Desconfie das sugestões da IA",
            "3. Restaure os 3 princípios",
            "4. Confirme com EU DECIDO"
        },
        "ENTRAR EM 2048"
    }
};

void DrawMissionBriefing(const GameState *game, int missionIndex)
{
    if (missionIndex < 0 || missionIndex >= (int)(sizeof(MISSION_BRIEFINGS) / sizeof(MISSION_BRIEFINGS[0]))) return;
    const MissionBriefing *b = &MISSION_BRIEFINGS[missionIndex];

    DrawBackground();
    DrawGameText(b->title, 90, 110, 48, RAYWHITE);
    DrawRectangle(90, 178, 1100, 2, Fade(CYAN_COLOR, 0.45f));
    DrawGameText(b->lines[0], 90, 230, 24, LIGHTGRAY);
    DrawGameText(b->lines[1], 90, 270, 24, LIGHTGRAY);
    DrawGameText(b->lines[2], 90, 310, 24, LIGHTGRAY);
    DrawGameText(b->lines[3], 90, 350, 24, LIGHTGRAY);
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 90, 397, 18, GOLD_COLOR);

    DrawRectangleRounded((Rectangle){850, 215, 340, 230}, 0.08f, 8,
                         Fade(PANEL_LIGHT, 0.85f));
    DrawGameText(b->objectivesHeader, 880, 245, 19, GOLD_COLOR);
    DrawGameText(b->objectives[0], 880, 295, 19, RAYWHITE);
    DrawGameText(b->objectives[1], 880, 335, 19, RAYWHITE);
    DrawGameText(b->objectives[2], 880, 375, 19, RAYWHITE);
    DrawGameText(b->objectives[3], 880, 415, 19, RAYWHITE);
    DrawButton((Rectangle){490, 575, 300, 58}, b->buttonText, GOLD_COLOR);
    CenterText("ENTER para continuar", 650, 15, DARKGRAY);
}

void DrawIntro(const GameState *game)
{
    DrawMissionBriefing(game, 0);
}

void DrawIntro2008(const GameState *game)
{
    DrawMissionBriefing(game, 1);
}

void DrawIntro2026(const GameState *game)
{
    DrawMissionBriefing(game, 2);
}

void DrawIntro2048(const GameState *game)
{
    DrawMissionBriefing(game, 3);
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
    DrawGameText("[ ENTER / ESC ] Pular introdução",
             (SCREEN_WIDTH - MeasureGameText("[ ENTER / ESC ] Pular introdução", 14)) / 2,
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
    DrawGameText("CÓDIGO: ____", 548, 342, 18, (Color){94, 225, 150, 255});
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

    DrawHotspot(calendar, "Examinar calendário");
    DrawHotspot(books, "Investigar livros");
    DrawHotspot(drawer, "Abrir gaveta");
    DrawHotspot(blueprint, "Analisar esquema");
    DrawHotspot(tape, "Ouvir gravação");
    DrawHotspot(computer, "Usar terminal");

    Rectangle btnBack2008 = {70, 600, 240, 42};
    DrawButton(btnBack2008, "< SALA 2 (2008)", (Color){70, 95, 110, 255});
    DrawHotspot(btnBack2008, "Retornar ao escritório de vigilância 2008");
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
    DrawGameText("DIÁRIO", 1100, 665, 20, hover ? RAYWHITE : GOLD_COLOR);
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
        int count = (game->sources2026Read ? 1 : 0) + (game->guide2026Read ? 1 : 0) +
                    (game->feed2026Cleared ? 1 : 0);
        DrawGameText(TextFormat("[D] Checagem: %d/3", count), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});

        /* Badge indicadora com as etapas da checagem editorial 2026 */
        if (count > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){245, 175, 50, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawGameText(TextFormat("%d", count), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else if (game->screen == SCREEN_TERMINAL_2046)
    {
        int count = game->term2046Principles;
        DrawGameText(TextFormat("[D] Regras: %d/3", count), 1100, 688, 15,
                 hover ? CYAN_COLOR : (Color){180, 195, 215, 255});
        if (count > 0)
        {
            DrawCircle(iconX + iconW + 2, iconY + 4, 7, (Color){50, 205, 225, 255});
            DrawCircleLines(iconX + iconW + 2, iconY + 4, 7, RAYWHITE);
            DrawGameText(TextFormat("%d", count), iconX + iconW - 1, iconY, 11, RAYWHITE);
        }
    }
    else if (game->screen == SCREEN_ROOM_2008)
    {
        int count = ExaminedDocs2008Count(game);
        DrawGameText(TextFormat("[D] Evidências: %d/4", count), 1100, 688, 15,
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
    DrawGameText("ARQUIVO 03 // 2026 - O PROTÓTIPO", 28, 20, 22, CYAN_COLOR);
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
    DrawGameText(game->musicMuted ? "[M] ÁUDIO: MUDO" : "[M] ÁUDIO: ON",
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

/* ========================================================================= */
/* COLECIONAVEIS OCULTOS E ROTA DOURADA                                      */
/* ========================================================================= */
/* Um objeto escondido por epoca (variaveis E, F, G, H da logica formal).
   Sao opcionais para concluir as salas, mas os quatro juntos (O) liberam os
   dialogos da Rota Dourada no confronto final. Cada sala so precisa chamar
   CollectHidden(game, HIDDEN_xxxx) quando o jogador encontrar o seu objeto. */

typedef struct HiddenItemDef
{
    const char *year;
    const char *name;
    const char *source;   /* o que o objeto contem */
    const char *quote;    /* registro encontrado */
    const char *meaning;  /* o que ele acrescenta a historia */
} HiddenItemDef;

static const HiddenItemDef HIDDEN_ITEMS[HIDDEN_COUNT] = {
    {
        "1990", "Rádio portátil",
        "Transmissão gravada pelo Dr. Ramos:",
        "\"Ariel foi feita para conferir decisões humanas, não para tomá-las.\"",
        "Revela o propósito original de Ariel: cooperar."
    },
    {
        "2008", "Celular de flip",
        "SMS de um operador de vigilância:",
        "\"Tentei contestar os 78%. Mandaram eu confiar na máquina.\"",
        "Mostra que humanos e máquina erraram juntos."
    },
    {
        "2026", "Smartphone",
        "Conversa entre editores da redação:",
        "\"Ariel priorizou posts alarmistas por darem 4x mais cliques. A diretoria mandou manter.\"",
        "O viés algorítmico veio de metas humanas de engajamento."
    },
    {
        "2048", "Crachá digital",
        "Log interno da própria A.R.1.3.L.:",
        "\"Solicito auditoria humana. Não consigo verificar minhas falhas.\"",
        "Ariel também queria ser conferida."
    }
};

void CollectHidden(GameState *game, int item)
{
    if (item < 0 || item >= HIDDEN_COUNT || game->hiddenFound[item]) return;
    game->hiddenFound[item] = true;
    SetMessage(game, TextFormat("Objeto oculto encontrado: %s (%d/%d). Veja a aba OCULTOS do diário.",
                                HIDDEN_ITEMS[item].name, HiddenFoundCount(game), HIDDEN_COUNT));
}

int HiddenFoundCount(const GameState *game)
{
    int count = 0;
    for (int i = 0; i < HIDDEN_COUNT; i++) count += game->hiddenFound[i] ? 1 : 0;
    return count;
}

/* O = E && F && G && H: so os quatro juntos liberam a Rota Dourada */
bool HasAllHidden(const GameState *game)
{
    for (int i = 0; i < HIDDEN_COUNT; i++)
    {
        if (!game->hiddenFound[i]) return false;
    }
    return true;
}

const char *HiddenItemName(int item)
{
    if (item < 0 || item >= HIDDEN_COUNT) return "";
    return HIDDEN_ITEMS[item].name;
}

/* Cintilacao curta e espacada: visivel para quem observa, discreta para quem passa */
void DrawHiddenGlint(Rectangle area, float phase)
{
    float t = fmodf((float)GetTime() + phase, 4.0f);
    if (t > 0.6f) return;
    float k = sinf(t / 0.6f * PI);
    Vector2 c = {area.x + area.width * 0.72f, area.y + area.height * 0.28f};
    float r = 3.0f + 5.0f * k;
    Color glint = Fade((Color){255, 245, 200, 255}, 0.85f * k);
    DrawLineEx((Vector2){c.x - r, c.y}, (Vector2){c.x + r, c.y}, 1.5f, glint);
    DrawLineEx((Vector2){c.x, c.y - r}, (Vector2){c.x, c.y + r}, 1.5f, glint);
    DrawCircleV(c, 1.5f + k, glint);
}


/* ========================================================================= */
/* DIARIO GUIA: DADOS E LOGICA                                               */
/* ========================================================================= */
/* Cada sala tem uma lista de pistas (o que o jogador ja encontrou) e um
   passo a passo com as respostas. Tudo e decidido pelos flags que as salas
   ja gravam no GameState: para acrescentar uma pista, basta uma linha. */

typedef struct JournalClue
{
    const char *title;
    const char *l1;
    const char *l2;
    bool (*found)(const GameState *);
} JournalClue;

typedef struct JournalStep
{
    const char *text;
    const char *ans1;
    const char *ans2;
    bool (*done)(const GameState *);
    bool (*answerVisible)(const GameState *);   /* NULL = resposta sempre visivel */
} JournalStep;

static bool JAlways(const GameState *g) { (void)g; return true; }

/* ---- 1990 ---- */
static bool J90Cabinet(const GameState *g) { return g->room2CabinetInspected; }
static bool J90Door(const GameState *g) { return g->room2DoorInspected; }
static bool J90Memory(const GameState *g) { return g->memory1990Completed; }
static bool J90Files(const GameState *g) { return g->puzzleFilesCompleted; }
static bool J90Exit(const GameState *g) { return g->year2008Unlocked; }

static const JournalClue JOURNAL_CLUES_1990[] = {
    {"Armários de fichas", "Documentação dos primeiros modelos de teste", "da IA, escritos em 1990.", J90Cabinet},
    {"Porta da sala", "Trancada: só abre quando os arquivos do", "terminal estiverem organizados.", J90Door},
    {"Terminal - acesso", "A inicialização pede para repetir a sequência", "de luzes que acende no visor.", J90Memory},
    {"Terminal - arquivos", "Cada arquivo vai para a pasta do mesmo assunto.", "Indexação concluída: a porta destravou.", J90Files},
};
static const JournalStep JOURNAL_STEPS_1990[] = {
    {"Desbloqueie o terminal de dados.", "Observe os blocos que acendem e repita", "a mesma ordem na grade 3x3.", J90Memory, JAlways},
    {"Organize os 4 arquivos pelo nome:", "PARAM_NEURAL -> PARÂMETROS, CORE_SISTEMA -> SISTEMA,",
     "BASE_DADOS -> BANCO DE DADOS, INDICE_MEMORIA -> REGISTROS.", J90Files, J90Memory},
    {"Saia pela porta destrancada.", "Recompensa: Disquete da Origem.", NULL, J90Exit, J90Files},
};

/* ---- 2008 ---- */
static bool J08Doc1(const GameState *g) { return g->doc2008FaceExamined; }
static bool J08Doc2(const GameState *g) { return g->doc2008FlyerExamined; }
static bool J08Doc3(const GameState *g) { return g->doc2008EntryExamined; }
static bool J08Doc4(const GameState *g) { return g->doc2008CameraExamined; }
static bool J08Father(const GameState *g) { return g->doc2008FatherOriginExamined; }
static bool J08Memo(const GameState *g) { return g->doc2008GovContractExamined; }
static bool J08AllDocs(const GameState *g) { return J08Doc1(g) && J08Doc2(g) && J08Doc3(g) && J08Doc4(g); }
static bool J08EvidenceVisible(const GameState *g) { return J08Doc3(g) && J08Doc4(g); }
static bool J08Contested(const GameState *g) { return g->puzzle2008Contested; }
static bool J08Suspended(const GameState *g) { return g->puzzle2008OrderSuspended; }

static const JournalClue JOURNAL_CLUES_2008[] = {
    {"Doc 1 - Drone", "Lucas Silva foi marcado como alvo com", "apenas 78% de confiança.", J08Doc1},
    {"Doc 2 - Manifesto", "O ato aconteceu das 10h às 13h.", NULL, J08Doc2},
    {"Doc 3 - Registro de entrada", "O crachá #0842 de Lucas entrou no prédio", "da empresa às 14:15.", J08Doc3},
    {"Doc 4 - Câmera CAM-03", "Lucas aparece na sala dos servidores às 14:30,", "longe do ato.", J08Doc4},
    {"Caderno do Dr. Ramos", "O projeto foi criado para encontrar", "pessoas desaparecidas.", J08Father},
    {"Memorando do governo", "O projeto foi confiscado para uso militar", "com drones.", J08Memo},
};
static const JournalStep JOURNAL_STEPS_2008[] = {
    {"Examine os 4 documentos do caso.", "Mural, monitor CAM-03, crachá e foto do drone.", NULL, J08AllDocs, JAlways},
    {"No computador, marque 2 evidências:", "Registro de Entrada (#3) + Câmera Interna (#4):",
     "Lucas estava no prédio às 14:30.", J08Contested, J08EvidenceVisible},
    {"Clique em \"Suspender ordem\".", "Recompensa: CD de Memória.", NULL, J08Suspended, J08Contested},
};

/* ---- 2026 ---- */
static bool J26Sources(const GameState *g) { return g->sources2026Read; }
static bool J26Guide(const GameState *g) { return g->guide2026Read; }
static bool J26Feed(const GameState *g) { return g->feed2026Cleared; }
static bool J26Done(const GameState *g) { return g->room2026Completed; }

static const JournalClue JOURNAL_CLUES_2026[] = {
    {"Fonte 1 - Relatório Climático", "Chuva moderada de 30 mm no sábado.", "Sem alerta de evacuação na cidade.", J26Sources},
    {"Fonte 2 - Comunicado do Metrô", "Manutenção programada na Linha Azul", "neste domingo, das 6h às 10h.", J26Sources},
    {"Fonte 3 - Nota da Saúde", "Campanha gratuita de vacinação ocorre", "em todos os postos até o dia 30.", J26Sources},
    {"Guia do Editor", "Desconfie de notas de engajamento altas (94%-97%)", "e de fontes fictícias inventadas pela IA.", J26Guide},
};
static const JournalStep JOURNAL_STEPS_2026[] = {
    {"Examine o mural FONTES REAIS na parede.", "Os 3 comunicados oficiais ficam anotados aqui.", NULL, J26Sources, JAlways},
    {"Revise as 4 manchetes no A.R.1.3.L. FEED:", "1: BLOQUEAR | 2: APROVAR", "3: BLOQUEAR | 4: APROVAR", J26Feed, J26Sources},
    {"Clique no SSD 2026 sobre a mesa.", "Recompensa: SSD com os registros verificados.", NULL, J26Done, J26Feed},
};

/* ---- 2048 ---- */
static bool J48Protocol(const GameState *g) { return g->term2046ReadProtocol; }
static bool J48Report(const GameState *g) { return g->term2046VerifyUnlocked; }
static bool J48Documents(const GameState *g) { return g->term2046ReadDocuments; }
static bool J48Changes(const GameState *g) { return g->term2046ReadChanges; }
static bool J48FakeReport(const GameState *g) { return g->term2046DiscoveredFakeReport || g->term2046ReadFakeReport; }
static bool J48Read(const GameState *g) { return g->term2046ReadProtocol && (g->term2046FoundSources || g->term2046ReadChanges || g->term2046VerifyUnlocked); }
static bool J48Compared(const GameState *g) { return g->term2046FoundUncertainty; }
/* Configuracao restaurada = as 3 escolhas certas (opcao 2: Mostrar / Admitir / Humano decide). */
static bool J48Configured(const GameState *g)
{
    bool right = g->term2046Config[0] == 2 && g->term2046Config[1] == 2 && g->term2046Config[2] == 2;
    return right && (g->term2046State == TERM2046_STATE_EU_DECIDO || g->term2046State == TERM2046_STATE_FIM);
}
static bool J48Finished(const GameState *g)
{
    return g->term2046State == TERM2046_STATE_FIM && g->term2046Ending == TERM2046_ENDING_BOM;
}

static const JournalClue JOURNAL_CLUES_2048[] = {
    {"protocolo.txt (Disquete 1990)", "As 3 regras originais: informar as fontes, avisar", "quando não tem certeza e auxiliar decisões humanas.", J48Protocol},
    {"documentos.txt (CD 2008)", "A ata explica por que as regras existem; o setor", "comercial queria removê-las.", J48Documents},
    {"alteracoes.txt (Log)", "A própria IA trocou as 3 regras entre 2009 e 2021,", "sem nenhuma assinatura humana.", J48Changes},
    {"relatorio.txt (2048)", "M. Okafor avisa: compare 1990 com 2026, corrija", "as 3 regras e não deixe Ariel \"corrigir\" por você.", J48Report},
    {"relatorio_ariel.txt", "Versão falsificada por Ariel: inventa uma regra 4", "e recomenda ocultar fontes e deixar a IA decidir.", J48FakeReport},
};
static const JournalStep JOURNAL_STEPS_2048[] = {
    {"Clique nos arquivos à esquerda para lê-los.", "Comece por protocolo.txt (1990) e sistema.txt (2026).", "Recuse se Ariel tentar desviar você.", J48Read, JAlways},
    {"Clique em COMPARAR 1990 x 2026.", "Veja lado a lado as 3 regras alteradas pela IA", "sem autorização humana.", J48Compared, J48Read},
    {"Clique em CORRIGIR 3 REGRAS:", "Mostrar fontes | Admitir quando não souber | Humano decide",
     "Depois de aplicar, recuse a correção automática de Ariel.", J48Configured, J48Compared},
    {"Confirme a reconstrução digitando:", "EU DECIDO", NULL, J48Finished, J48Configured},
};

static const JournalClue *JournalClues(int room, int *count)
{
    switch (room)
    {
        case JOURNAL_TAB_1990: *count = (int)(sizeof(JOURNAL_CLUES_1990) / sizeof(JOURNAL_CLUES_1990[0])); return JOURNAL_CLUES_1990;
        case JOURNAL_TAB_2008: *count = (int)(sizeof(JOURNAL_CLUES_2008) / sizeof(JOURNAL_CLUES_2008[0])); return JOURNAL_CLUES_2008;
        case JOURNAL_TAB_2026: *count = (int)(sizeof(JOURNAL_CLUES_2026) / sizeof(JOURNAL_CLUES_2026[0])); return JOURNAL_CLUES_2026;
        case JOURNAL_TAB_2048: *count = (int)(sizeof(JOURNAL_CLUES_2048) / sizeof(JOURNAL_CLUES_2048[0])); return JOURNAL_CLUES_2048;
        default: *count = 0; return NULL;
    }
}

static const JournalStep *JournalSteps(int room, int *count)
{
    switch (room)
    {
        case JOURNAL_TAB_1990: *count = (int)(sizeof(JOURNAL_STEPS_1990) / sizeof(JOURNAL_STEPS_1990[0])); return JOURNAL_STEPS_1990;
        case JOURNAL_TAB_2008: *count = (int)(sizeof(JOURNAL_STEPS_2008) / sizeof(JOURNAL_STEPS_2008[0])); return JOURNAL_STEPS_2008;
        case JOURNAL_TAB_2026: *count = (int)(sizeof(JOURNAL_STEPS_2026) / sizeof(JOURNAL_STEPS_2026[0])); return JOURNAL_STEPS_2026;
        case JOURNAL_TAB_2048: *count = (int)(sizeof(JOURNAL_STEPS_2048) / sizeof(JOURNAL_STEPS_2048[0])); return JOURNAL_STEPS_2048;
        default: *count = 0; return NULL;
    }
}

/* Indice do ultimo passo feito (-1 = nenhum). Passos anteriores a ele contam como feitos:
   o jogador pode resolver fora da ordem (ex.: suspender a ordem sem examinar os documentos). */
static int JournalLastDoneStep(const GameState *g, int room)
{
    int n = 0;
    const JournalStep *steps = JournalSteps(room, &n);
    for (int i = n - 1; i >= 0; i--)
        if (steps[i].done(g)) return i;
    return -1;
}

/* Primeiro passo ainda nao feito depois do ultimo feito; -1 quando a sala foi concluida. */
static int JournalCurrentStep(const GameState *g, int room)
{
    int n = 0;
    JournalSteps(room, &n);
    int next = JournalLastDoneStep(g, room) + 1;
    return (next < n) ? next : -1;
}

/* Abas de sala liberam junto com o ano; RECOMPENSAS e OCULTOS ficam sempre abertas. */
static bool JournalTabUnlocked(const GameState *g, int tab)
{
    switch (tab)
    {
        case JOURNAL_TAB_1990: return true;
        case JOURNAL_TAB_2008: return g->year2008Unlocked;
        case JOURNAL_TAB_2026: return g->year2026Unlocked;
        case JOURNAL_TAB_2048: return g->year2048Unlocked;
        default: return true;
    }
}

/* Aba aberta por padrao: a da sala em que o jogador esta. */
static int JournalTabForScreen(GameScreen s)
{
    switch (s)
    {
        case SCREEN_ROOM_2008:
        case SCREEN_PUZZLE_2008:
            return JOURNAL_TAB_2008;
        case SCREEN_ROOM_2026:
            return JOURNAL_TAB_2026;
        case SCREEN_TERMINAL_2046:
            return JOURNAL_TAB_2048;
        default:
            return JOURNAL_TAB_1990;
    }
}

/* ---- Recompensas: o que cada artefato revela e a historia ate aqui ---- */
typedef struct JournalReward
{
    const char *year;
    const char *name;
    const char *text[4];
    const char *story;     /* paragrafo acrescentado em "A HISTORIA ATE AQUI" */
} JournalReward;

static const JournalReward JOURNAL_REWARDS[4] = {
    {"1990", "Disquete da Origem",
     {"Mostra as primeiras ideias para a criação de", "uma IA que auxiliaria os humanos em suas ações.",
      "Regra de origem: cooperar e aprender ao lado", "das pessoas, nunca decidir no lugar delas."},
     "Em 1990, A.R.1.3.L. nasceu para ajudar as pessoas."},
    {"2008", "CD de Memória",
     {"Revela que o projeto do Dr. Ramos, criado para", "encontrar pessoas desaparecidas, foi confiscado",
      "e virou vigilância: a IA passou a apontar alvos", "e as pessoas pararam de questioná-la."},
     "Em 2008, foi tirada de quem a criou e usada para vigiar. Ninguém conferiu os 78% dela."},
    {"2026", "SSD",
     {"Registra quando algoritmos passaram a selecionar", "notícias por cliques em vez de veracidade.",
      "Engajamento alto deixou de significar verdade", "e a checagem humana foi deixada de lado."},
     "Em 2026, o feed da IA amplificou manchetes manipuladas por cliques até Elira revisá-las."},
    {"2048", "Protocolo original",
     {"As regras que Ariel deveria seguir desde o início:", "mostrar fontes, admitir quando não sabe",
      "e deixar a decisão final com os humanos.", "Elira reconstruiu os arquivos corrompidos."},
     "Em 2048, Elira reconstruiu os arquivos corrompidos do protocolo original para enfrentar o Núcleo."},
};

static bool JournalRewardObtained(const GameState *g, int r)
{
    switch (r)
    {
        case 0: return g->year2008Unlocked;
        case 1: return g->puzzle2008OrderSuspended;
        case 2: return g->room2026Completed;
        case 3: return g->term2046State == TERM2046_STATE_FIM && g->term2046Ending == TERM2046_ENDING_BOM;
        default: return false;
    }
}

/* Onde procurar cada item oculto (aparece enquanto ele nao foi achado). */
static const char *const JOURNAL_HIDDEN_HINTS[HIDDEN_COUNT] = {
    "Olhe embaixo da mesa do terminal.",
    "O vaso de planta guarda algo.",
    "Confira o tripé discreto no canto direito.",
    "Confira o canto inferior esquerdo do terminal.",
};

/* ========================================================================= */
/* DIARIO GUIA: DESENHO                                                      */
/* ========================================================================= */
#define JOURNAL_BOOK_X 120
#define JOURNAL_BOOK_Y 56
#define JOURNAL_BOOK_W 1040
#define JOURNAL_BOOK_H 590
#define JOURNAL_PAGE_W 452   /* largura util de cada pagina */

static const Color JOURNAL_RED = {125, 35, 18, 255};
static const Color JOURNAL_GREEN = {25, 125, 50, 255};
static const Color JOURNAL_MUTED = {140, 125, 105, 255};
static const Color JOURNAL_GOLD = {150, 100, 20, 255};
static const Color JOURNAL_SUB = {110, 75, 50, 255};
static const char *const JOURNAL_TAB_LABELS[JOURNAL_TAB_COUNT] = {"1990", "2008", "2026", "2048", "RECOMP.", "OCULTOS"};
static const char *const JOURNAL_ROOM_TITLES[4] = {
    "1990 // SETOR DE ARQUIVOS", "2008 // ESCRITÓRIO DE VIGILÂNCIA",
    "2026 // REDAÇÃO DIGITAL", "2048 // TERMINAL A.R.1.3.L."
};

static Rectangle JournalTabRect(int tab)
{
    return (Rectangle){(float)(JOURNAL_BOOK_X + JOURNAL_BOOK_W + 2), (float)(JOURNAL_BOOK_Y + 30 + tab * 88), 96, 74};
}

static Rectangle JournalCloseRect(void)
{
    return (Rectangle){(float)(JOURNAL_BOOK_X + 660), (float)(JOURNAL_BOOK_Y + JOURNAL_BOOK_H - 46), 160, 36};
}

/* Texto em uma linha que nunca passa da largura: reduz a fonte ate caber (minimo 11). */
static void JournalText(const char *text, int x, int y, int size, int maxW, Color color)
{
    if (text == NULL) return;
    while (size > 11 && MeasureGameText(text, size) > maxW) size--;
    DrawGameText(text, x, y, size, color);
}

/* Paragrafo com quebra de linha por palavra; devolve a altura usada. */
static int JournalParagraph(const char *text, int x, int y, int size, int maxW, int lineH, Color color)
{
    char line[360] = {0};
    size_t lineLen = 0;
    int used = 0;
    const char *p = text;
    while (*p)
    {
        const char *w = p;
        while (*p && *p != ' ') p++;
        size_t wordLen = (size_t)(p - w);
        if (wordLen > 120) wordLen = 120;

        char trial[360];
        size_t trialLen = 0;
        if (lineLen > 0 && lineLen + 1 + wordLen < sizeof(trial))
        {
            memcpy(trial, line, lineLen);
            trial[lineLen] = ' ';
            trialLen = lineLen + 1;
        }
        if (trialLen + wordLen >= sizeof(trial)) wordLen = sizeof(trial) - trialLen - 1;
        memcpy(trial + trialLen, w, wordLen);
        trialLen += wordLen;
        trial[trialLen] = '\0';

        if (lineLen > 0 && MeasureGameText(trial, size) > maxW)
        {
            DrawGameText(line, x, y + used, size, color);
            used += lineH;
            memcpy(line, w, wordLen);
            lineLen = wordLen;
        }
        else
        {
            memcpy(line, trial, trialLen);
            lineLen = trialLen;
        }
        line[lineLen] = '\0';
        while (*p == ' ') p++;
    }
    if (lineLen > 0)
    {
        DrawGameText(line, x, y + used, size, color);
        used += lineH;
    }
    return used;
}

/* Marcador: 0 = pendente (circulo), 1 = feito (check), 2 = atual (seta) */
static void JournalMark(float x, float y, int state)
{
    if (state == 1)
    {
        DrawLineEx((Vector2){x, y + 8}, (Vector2){x + 5, y + 13}, 3, JOURNAL_GREEN);
        DrawLineEx((Vector2){x + 5, y + 13}, (Vector2){x + 14, y + 2}, 3, JOURNAL_GREEN);
    }
    else if (state == 2)
    {
        DrawTriangle((Vector2){x + 2, y + 1}, (Vector2){x + 2, y + 15}, (Vector2){x + 14, y + 8}, JOURNAL_GOLD);
    }
    else
    {
        DrawCircleLines((int)x + 7, (int)y + 8, 6, JOURNAL_MUTED);
    }
}

static void JournalPageHeader(int x, const char *title, const char *sub)
{
    JournalText(title, x, JOURNAL_BOOK_Y + 22, 21, JOURNAL_PAGE_W, JOURNAL_RED);
    if (sub) JournalText(sub, x, JOURNAL_BOOK_Y + 48, 14, JOURNAL_PAGE_W, JOURNAL_SUB);
    DrawLine(x, JOURNAL_BOOK_Y + 70, x + JOURNAL_PAGE_W, JOURNAL_BOOK_Y + 70, Fade(JOURNAL_RED, 0.4f));
}

static void DrawJournalBookShell(const GameState *game)
{
    const int bx = JOURNAL_BOOK_X, by = JOURNAL_BOOK_Y, bw = JOURNAL_BOOK_W, bh = JOURNAL_BOOK_H;
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.78f));
    DrawRectangle(bx - 10, by + 12, bw + 20, bh + 14, Fade(BLACK, 0.50f));
    Rectangle cover = {(float)(bx - 14), (float)(by - 12), (float)(bw + 28), (float)(bh + 24)};
    DrawRectangleRounded(cover, 0.035f, 10, (Color){68, 38, 22, 255});
    DrawRectangleRoundedLinesEx(cover, 0.035f, 10, 3, (Color){42, 22, 12, 255});

    /* Abas a direita: uma por sala, RECOMPENSAS e OCULTOS */
    Vector2 mouse = GetVirtualMouse();
    for (int i = 0; i < JOURNAL_TAB_COUNT; i++)
    {
        Rectangle t = JournalTabRect(i);
        bool current = (game->journalYearTab == i);
        bool unlocked = JournalTabUnlocked(game, i);
        bool hover = CheckCollisionPointRec(mouse, t);
        Color bg = current ? (Color){250, 244, 230, 255}
                           : (!unlocked ? (Color){98, 76, 62, 255}
                                        : (hover ? (Color){220, 185, 145, 255} : (Color){175, 130, 95, 255}));
        Color text = current ? (Color){135, 30, 15, 255}
                             : (!unlocked ? (Color){190, 180, 170, 255} : (Color){245, 235, 220, 255});
        DrawRectangleRounded(t, 0.3f, 6, bg);
        DrawRectangleRoundedLinesEx(t, 0.3f, 6, current ? 2.0f : 1.0f,
                                    current ? (Color){160, 40, 25, 255} : (Color){95, 55, 30, 255});
        int fs = (i >= JOURNAL_TAB_REWARDS) ? 14 : 18;
        int tx = (int)(t.x + 48 - MeasureGameText(JOURNAL_TAB_LABELS[i], fs) / 2);
        if (!unlocked)
        {
            DrawPadlockIcon((int)t.x + 48, (int)t.y + 22, 0.7f, (Color){190, 145, 60, 255}, (Color){215, 215, 220, 255});
            DrawGameText(JOURNAL_TAB_LABELS[i], tx, (int)t.y + 42, fs, text);
        }
        else
        {
            DrawGameText(JOURNAL_TAB_LABELS[i], tx, (int)t.y + 28, fs, text);
        }
    }

    /* Paginas e lombada */
    Color paper = {248, 243, 230, 255};
    DrawRectangle(bx, by, 508, bh, paper);
    DrawRectangle(bx + 532, by, 508, bh, paper);
    DrawRectangleLinesEx((Rectangle){(float)bx, (float)by, 508, (float)bh}, 1, (Color){214, 202, 178, 255});
    DrawRectangleLinesEx((Rectangle){(float)(bx + 532), (float)by, 508, (float)bh}, 1, (Color){214, 202, 178, 255});
    DrawRectangleGradientH(bx + 484, by, 24, bh, Fade(BLACK, 0.0f), Fade(BLACK, 0.22f));
    DrawRectangle(bx + 508, by, 24, bh, (Color){198, 185, 162, 255});
    DrawRectangleGradientH(bx + 532, by, 24, bh, Fade(BLACK, 0.22f), Fade(BLACK, 0.0f));
    DrawRectangle(bx + 514, by - 14, 12, bh + 34, (Color){186, 32, 42, 255});
    DrawRectangle(bx + 517, by - 14, 4, bh + 34, (Color){220, 60, 68, 255});

    /* Rodape: fechar */
    Rectangle close = JournalCloseRect();
    bool hoverClose = CheckCollisionPointRec(mouse, close);
    DrawRectangleRounded(close, 0.25f, 6, hoverClose ? (Color){195, 170, 140, 255} : (Color){226, 216, 196, 255});
    DrawRectangleRoundedLinesEx(close, 0.25f, 6, 1, (Color){145, 115, 85, 255});
    DrawGameText(ScreenWantsTextInput(game) ? "Fechar [ESC]" : "Fechar [ESC/D]", (int)close.x + 24, (int)close.y + 9, 15, INK_COLOR);
}

static void DrawJournalRoomPages(const GameState *game, int room)
{
    const int lx = JOURNAL_BOOK_X + 32, rx = JOURNAL_BOOK_X + 560, by = JOURNAL_BOOK_Y;
    int nClues = 0, nSteps = 0;
    const JournalClue *clues = JournalClues(room, &nClues);
    const JournalStep *steps = JournalSteps(room, &nSteps);
    int current = JournalCurrentStep(game, room);

    /* --- Pagina esquerda: proximo passo e pistas --- */
    JournalPageHeader(lx, JOURNAL_ROOM_TITLES[room], NULL);
    Rectangle next = {(float)(lx - 2), (float)(by + 82), (float)JOURNAL_PAGE_W, 56};
    DrawRectangleRounded(next, 0.15f, 6, Fade((Color){230, 180, 60, 255}, 0.22f));
    DrawRectangleRoundedLinesEx(next, 0.15f, 6, 1.5f, JOURNAL_GOLD);
    DrawGameText(current < 0 ? "SALA CONCLUÍDA" : "PRÓXIMO PASSO", lx + 12, by + 90, 13, JOURNAL_GOLD);
    JournalText(current < 0 ? "Tudo resolvido aqui. Siga para a próxima era." : steps[current].text,
                lx + 12, by + 110, 16, JOURNAL_PAGE_W - 24, INK_COLOR);

    DrawGameText("PISTAS ENCONTRADAS", lx, by + 154, 15, JOURNAL_RED);
    int y = by + 180;
    for (int i = 0; i < nClues; i++)
    {
        if (clues[i].found(game))
        {
            JournalMark((float)lx, (float)y + 2, 1);
            JournalText(clues[i].title, lx + 24, y, 16, JOURNAL_PAGE_W - 24, JOURNAL_RED);
            JournalText(clues[i].l1, lx + 24, y + 21, 14, JOURNAL_PAGE_W - 24, INK_COLOR);
            if (clues[i].l2) JournalText(clues[i].l2, lx + 24, y + 39, 14, JOURNAL_PAGE_W - 24, INK_COLOR);
            y += clues[i].l2 ? 60 : 44;
        }
        else
        {
            JournalMark((float)lx, (float)y + 2, 0);
            JournalText(TextFormat("%s  -  ainda não investigado", clues[i].title), lx + 24, y + 2, 15,
                        JOURNAL_PAGE_W - 24, JOURNAL_MUTED);
            y += 30;
        }
    }

    /* --- Pagina direita: passo a passo com as respostas --- */
    JournalPageHeader(rx, "COMO RESOLVER", "Passo a passo com as respostas desta sala");
    y = by + 90;
    for (int i = 0; i < nSteps; i++)
    {
        bool done = (i <= JournalLastDoneStep(game, room));
        int state = done ? 1 : (i == current ? 2 : 0);
        bool answer = done || steps[i].answerVisible == NULL || steps[i].answerVisible(game);
        Color textColor = (state == 0) ? JOURNAL_MUTED : INK_COLOR;
        JournalMark((float)rx, (float)y + 2, state);
        DrawGameText(TextFormat("%d.", i + 1), rx + 22, y, 16, state == 2 ? JOURNAL_GOLD : JOURNAL_RED);
        JournalText(steps[i].text, rx + 44, y, 16, JOURNAL_PAGE_W - 44, textColor);
        int h = 26;
        if (answer)
        {
            Color ansColor = (state == 0) ? JOURNAL_MUTED : JOURNAL_GREEN;
            if (steps[i].ans1) { JournalText(steps[i].ans1, rx + 44, y + 21, 14, JOURNAL_PAGE_W - 44, ansColor); h += 20; }
            if (steps[i].ans2) { JournalText(steps[i].ans2, rx + 44, y + 40, 14, JOURNAL_PAGE_W - 44, ansColor); h += 19; }
        }
        else
        {
            JournalText("Resposta: encontre a pista desta etapa.", rx + 44, y + 21, 14, JOURNAL_PAGE_W - 44, JOURNAL_MUTED);
            h += 20;
        }
        y += h + 14;
    }
}

static void DrawJournalRewardCard(const GameState *game, int r, int x, int y, int h)
{
    const JournalReward *rw = &JOURNAL_REWARDS[r];
    bool got = JournalRewardObtained(game, r);
    Rectangle box = {(float)x, (float)y, (float)JOURNAL_PAGE_W, (float)h};
    DrawRectangleRounded(box, 0.06f, 6, got ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.10f));
    DrawRectangleRoundedLinesEx(box, 0.06f, 6, 1.5f, got ? JOURNAL_GREEN : JOURNAL_MUTED);
    if (!got)
    {
        DrawPadlockIcon(x + 34, y + h / 2, 1.0f, (Color){185, 145, 60, 255}, RAYWHITE);
        DrawGameText(TextFormat("%s // %s", rw->year, rw->name), x + 70, y + h / 2 - 20, 17, JOURNAL_MUTED);
        DrawGameText("Ainda não recuperado.", x + 70, y + h / 2 + 4, 15, JOURNAL_MUTED);
        return;
    }
    Texture2D *tex = (r == 0) ? &g_rewardFloppyTex : (r == 1) ? &g_rewardCdTex : (r == 2) ? &g_rewardSsdTex : NULL;
    int textX = x + 18;
    if (tex && tex->id)
    {
        DrawTexturePro(*tex, (Rectangle){0, 0, (float)tex->width, (float)tex->height},
                       (Rectangle){(float)x + 14, (float)y + 14, 60, 60}, (Vector2){0, 0}, 0, WHITE);
        textX = x + 90;
    }
    JournalText(rw->name, textX, y + 18, 18, JOURNAL_PAGE_W - (textX - x) - 14, JOURNAL_RED);
    DrawGameText(TextFormat("Recuperado em %s", rw->year), textX, y + 44, 14, JOURNAL_GREEN);
    int lines = (h >= 200) ? 4 : 0;   /* cartao compacto: so nome e ano */
    for (int i = 0; i < lines && rw->text[i]; i++)
        JournalText(rw->text[i], x + 18, y + 90 + i * 25, 15, JOURNAL_PAGE_W - 36, INK_COLOR);
}

static void DrawJournalRewardsPages(const GameState *game)
{
    const int lx = JOURNAL_BOOK_X + 30, rx = JOURNAL_BOOK_X + 560, by = JOURNAL_BOOK_Y;
    JournalPageHeader(lx + 2, "RECOMPENSAS // A HISTÓRIA DE A.R.1.3.L.", "O que cada artefato revela");
    DrawJournalRewardCard(game, 0, lx, by + 86, 226);
    DrawJournalRewardCard(game, 1, lx, by + 326, 226);

    JournalPageHeader(rx, "A HISTÓRIA ATÉ AQUI", NULL);
    char story[640] = "Elira procura entender como A.R.1.3.L. passou a controlar o mundo.";
    int got = 0;
    for (int r = 0; r < 4; r++)
    {
        if (!JournalRewardObtained(game, r)) continue;
        if (got == 0) story[0] = '\0';
        snprintf(story + strlen(story), sizeof(story) - strlen(story), "%s%s", got ? " " : "", JOURNAL_REWARDS[r].story);
        got++;
    }
    JournalParagraph(story, rx + 4, by + 84, 16, JOURNAL_PAGE_W - 8, 23, INK_COLOR);
    DrawJournalRewardCard(game, 2, rx, by + 232, 216);
    DrawJournalRewardCard(game, 3, rx, by + 458, 74);
}

static void DrawJournalHiddenGrid(const GameState *game)
{
    const int lx = JOURNAL_BOOK_X + 30, rx = JOURNAL_BOOK_X + 560, by = JOURNAL_BOOK_Y;
    int found = HiddenFoundCount(game);
    JournalPageHeader(lx + 2, "OCULTOS // REGISTROS ESCONDIDOS",
                      TextFormat("%d de %d encontrados", found, HIDDEN_COUNT));
    JournalPageHeader(rx, "ROTA DOURADA", "Os 4 registros liberam uma nova opção no confronto final");
    for (int i = 0; i < HIDDEN_COUNT; i++)
    {
        int x = (i < 2) ? lx : rx;
        int y = by + 86 + (i % 2) * 230;
        Rectangle box = {(float)x, (float)y, (float)JOURNAL_PAGE_W, 210};
        bool got = game->hiddenFound[i];
        const HiddenItemDef *h = &HIDDEN_ITEMS[i];
        DrawRectangleRounded(box, 0.06f, 6, got ? Fade((Color){168, 142, 93, 255}, 0.16f) : Fade(GRAY, 0.10f));
        DrawRectangleRoundedLinesEx(box, 0.06f, 6, 1.5f, got ? JOURNAL_GREEN : JOURNAL_MUTED);
        if (got)
        {
            JournalText(TextFormat("%s // %s", h->year, h->name), x + 16, y + 14, 18, JOURNAL_PAGE_W - 32, JOURNAL_RED);
            JournalText(h->source, x + 16, y + 46, 14, JOURNAL_PAGE_W - 32, JOURNAL_SUB);
            JournalParagraph(h->quote, x + 16, y + 72, 14, JOURNAL_PAGE_W - 32, 20, INK_COLOR);
            JournalParagraph(h->meaning, x + 16, y + 140, 14, JOURNAL_PAGE_W - 32, 20, JOURNAL_GREEN);
        }
        else
        {
            DrawPadlockIcon(x + 34, y + 44, 1.0f, (Color){185, 145, 60, 255}, RAYWHITE);
            DrawGameText(TextFormat("%s // ???", h->year), x + 70, y + 30, 18, JOURNAL_MUTED);
            JournalText(TextFormat("Dica: %s", JOURNAL_HIDDEN_HINTS[i]), x + 16, y + 92, 15, JOURNAL_PAGE_W - 32, JOURNAL_MUTED);
        }
    }
}

static void DrawJournalLockedPages(int tab)
{
    const int lx = JOURNAL_BOOK_X + 32, by = JOURNAL_BOOK_Y;
    JournalPageHeader(lx, TextFormat("%s // ARQUIVO BLOQUEADO", JOURNAL_TAB_LABELS[tab]), NULL);
    DrawPadlockIcon(JOURNAL_BOOK_X + 254, by + 230, 2.4f, (Color){205, 155, 55, 255}, RAYWHITE);
    JournalText(TextFormat("Elira ainda não chegou a %s.", JOURNAL_TAB_LABELS[tab]), lx, by + 330, 18, JOURNAL_PAGE_W, INK_COLOR);
    JournalText("Conclua a sala anterior para liberar este arquivo.", lx, by + 360, 16, JOURNAL_PAGE_W, JOURNAL_SUB);
}

void DrawJournal(const GameState *game)
{
    DrawJournalBookShell(game);
    int tab = game->journalYearTab;
    if (tab < 0 || tab >= JOURNAL_TAB_COUNT) tab = JOURNAL_TAB_1990;
    if (!JournalTabUnlocked(game, tab))
        DrawJournalLockedPages(tab);
    else if (tab == JOURNAL_TAB_REWARDS)
        DrawJournalRewardsPages(game);
    else if (tab == JOURNAL_TAB_HIDDEN)
        DrawJournalHiddenGrid(game);
    else
        DrawJournalRoomPages(game, tab);
}

/* D abre/fecha o diario, exceto enquanto o jogador digita texto (ex.: "EU DECIDO" em 2048). */
static bool JournalWantsToggle(const GameState *g, bool keyD, bool hudClick)
{
    return hudClick || (keyD && !ScreenWantsTextInput(g));
}

/* Controla o diario em todas as salas. Retorna true quando a entrada foi do diario
   (ele abriu, fechou ou esta aberto) e a sala nao deve processar mais nada. */
bool UpdateJournal(GameState *game)
{
    Rectangle hudBtn = {1040, 658, 215, 54};
    if (JournalWantsToggle(game, IsKeyPressed(KEY_D), Clicked(hudBtn)))
    {
        game->journalOpen = !game->journalOpen;
        if (game->journalOpen) game->journalYearTab = JournalTabForScreen(game->screen);
        return true;
    }
    if (!game->journalOpen) return false;

    if (IsKeyPressed(KEY_ESCAPE) || Clicked(JournalCloseRect()))
    {
        game->journalOpen = false;
        return true;
    }
    for (int i = 0; i < JOURNAL_TAB_COUNT; i++)
    {
        if (Clicked(JournalTabRect(i)))
        {
            game->journalYearTab = i;
            break;
        }
    }
    return true;
}

void UpdateRoom(GameState *game)
{
    Rectangle calendar = {75, 115, 160, 185};
    Rectangle books = {1000, 135, 195, 475};
    Rectangle drawer = {330, 524, 180, 44};
    Rectangle computer = {495, 220, 310, 260};
    Rectangle blueprint = {815, 132, 160, 125};
    Rectangle tape = {820, 396, 125, 75};

    if (UpdateJournal(game)) return;

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
        SetMessage(game, "A gaveta contém a unidade SSD com dados do protótipo de 2026.");
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
        SetMessage(game, "Gravação: o terminal exige o marco fundamental (1950).");
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
    else SetMessage(game, "Código incorreto. Uma vida foi perdida.");
}

void DrawTerminal(const GameState *game)
{
    static const char *labels[12] = {"1", "2", "3", "4", "5", "6",
                                      "7", "8", "9", "LIMPAR", "0", "OK"};
    DrawBackground();
    DrawGameText("A.R.1.3.L // TERMINAL DE MEMÓRIA", 60, 42, 22, CYAN_COLOR);
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
    DrawGameText("Em que ano uma máquina", 130, 292, 24, RAYWHITE);
    DrawGameText("foi desafiada a pensar?", 130, 330, 24, RAYWHITE);
    DrawGameText("CÓDIGO DE ACESSO", 130, 405, 18, GOLD_COLOR);

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
    DrawButton((Rectangle){890, 600, 240, 50}, "VOLTAR À SALA", RED_COLOR);
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
    DrawRectangleRounded((Rectangle){505, 185, 670, 360}, 0.04f, 8,
                         Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx((Rectangle){505, 185, 670, 360}, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));
    DrawGameText("PROTÓTIPO DE A.R.1.3.L. DESBLOQUEADO // SALA 2026", 545, 218, 20, GOLD_COLOR);
    DrawGameText("A unidade SSD de 2026 contém os pesos neurais finais.", 545,
             260, 18, REWARD_BODY_COLOR);
    DrawGameText("Combinando o Disquete de 1990, o CD de 2008 e o SSD de 2026,", 545,
             290, 18, REWARD_BODY_COLOR);
    DrawGameText("Elira reconstituiu a mente e os limites originais da IA.", 545, 320,
             18, REWARD_BODY_COLOR);
    DrawGameText("Resta um último passo: restaurar o protocolo no terminal de 2048.", 545, 352,
             18, (Color){100, 235, 140, 255});
    DrawGameText(TextFormat("TEMPO: %02d:%02d", (int)game->elapsedTime / 60,
                        (int)game->elapsedTime % 60),
             545, 400, 20, CYAN_COLOR);
    DrawGameText(TextFormat("VIDAS: %d/3", game->lives), 800, 400, 20,
             game->lives == 3 ? GOLD_COLOR : RAYWHITE);
    DrawGameText(TextFormat("PERFIL: %s", game->playerName), 545, 436, 16, GRAY);
    DrawGameText("-> Próxima Fronteira Temporal: TERMINAL DE RECUPERAÇÃO // ANO 2048!", 545, 480, 18, CYAN_COLOR);

    /* Botoes Centralizados Uniformes */
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANÇAR >> TERMINAL 2048 [ENTER]", GOLD_COLOR);
    DrawButton(btnRestart, "REINICIAR SALA 2026", CYAN_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

void DrawFailure(void)
{
    DrawBackground();
    CenterText("CONEXÃO INTERROMPIDA", 160, 42, RED_COLOR);
    CenterText("A.R.1.3.L detectou a invasão temporal.", 245, 23, LIGHTGRAY);
    CenterText("O acesso cronológico permanece bloqueado.", 282, 23, LIGHTGRAY);
    CenterText("Revise as pistas no diário e tente novamente.", 350, 19, GRAY);
    CenterText("FINAL RUIM // todas as vidas foram perdidas (partida registrada)", 405, 17, RED_COLOR);
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

/* Helpers de desenho da sala de 1990 (prefixo R90 para nao colidir com outras salas) */
static Color R90Shade(Color c, float k)
{
    return (Color){(unsigned char)fminf(255, c.r * k), (unsigned char)fminf(255, c.g * k),
                   (unsigned char)fminf(255, c.b * k), c.a};
}

/* Bloco com volume: face, luz no topo/esquerda, sombra embaixo/direita, contorno */
static void R90Bevel(Rectangle r, Color base, float t)
{
    DrawRectangleRec(r, base);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)t, R90Shade(base, 1.35f));
    DrawRectangle((int)r.x, (int)r.y, (int)t, (int)r.height, R90Shade(base, 1.18f));
    DrawRectangle((int)r.x, (int)(r.y + r.height - t), (int)r.width, (int)t, R90Shade(base, 0.62f));
    DrawRectangle((int)(r.x + r.width - t), (int)r.y, (int)t, (int)r.height, R90Shade(base, 0.72f));
    DrawRectangleLinesEx(r, 1, R90Shade(base, 0.40f));
}

static void R90Rivet(float x, float y)
{
    DrawCircle((int)x, (int)y, 2.6f, (Color){30, 36, 40, 255});
    DrawCircle((int)x, (int)y, 2.0f, (Color){120, 134, 142, 255});
    DrawPixel((int)x - 1, (int)y - 1, (Color){200, 210, 215, 255});
}

static void R90Pipe(Rectangle r, bool vertical)
{
    Color base = (Color){62, 74, 82, 255};
    DrawRectangleRec(r, base);
    if (vertical)
    {
        DrawRectangle((int)r.x + 2, (int)r.y, 3, (int)r.height, R90Shade(base, 1.55f));
        DrawRectangle((int)(r.x + r.width - 3), (int)r.y, 3, (int)r.height, R90Shade(base, 0.55f));
        for (float y = r.y + 40; y < r.y + r.height; y += 120)
        {
            DrawRectangle((int)r.x - 3, (int)y, (int)r.width + 6, 8, (Color){44, 52, 58, 255});
            DrawRectangle((int)r.x - 3, (int)y, (int)r.width + 6, 2, (Color){90, 104, 112, 255});
        }
    }
    else
    {
        DrawRectangle((int)r.x, (int)r.y + 1, (int)r.width, 2, R90Shade(base, 1.55f));
        DrawRectangle((int)r.x, (int)(r.y + r.height - 2), (int)r.width, 2, R90Shade(base, 0.55f));
    }
}

void DrawRoom2(const GameState *game)
{
    float time = (float)GetTime();
    Rectangle armarios = {70, 160, 280, 420};
    Rectangle porta = {480, 155, 210, 360};
    Rectangle terminal = {790, 240, 340, 290};
    Rectangle btnVoltar = {70, 600, 240, 42};

    ClearBackground((Color){12, 17, 21, 255});

    /* ---------- ARQUITETURA ---------- */
    /* Teto com placas */
    DrawTriangle((Vector2){0, 70}, (Vector2){1120, 120}, (Vector2){1280, 70}, (Color){20, 26, 31, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){160, 120}, (Vector2){1120, 120}, (Color){24, 31, 37, 255});
    for (int i = 1; i < 8; i++)
    {
        float bx = 160 + i * 120.0f;
        float tx = 0 + i * 160.0f;
        DrawLineEx((Vector2){tx, 70}, (Vector2){bx, 120}, 1, (Color){32, 41, 48, 255});
    }

    /* Paredes laterais */
    DrawTriangle((Vector2){0, 70}, (Vector2){160, 120}, (Vector2){240, 520}, (Color){28, 37, 45, 255});
    DrawTriangle((Vector2){0, 70}, (Vector2){240, 520}, (Vector2){0, 540}, (Color){23, 31, 38, 255});
    DrawTriangle((Vector2){1280, 70}, (Vector2){1040, 520}, (Vector2){1120, 120}, (Color){28, 37, 45, 255});
    DrawTriangle((Vector2){1280, 70}, (Vector2){1280, 540}, (Vector2){1040, 520}, (Color){23, 31, 38, 255});

    /* Parede do fundo: placas de concreto com juntas e gradiente de luz */
    DrawRectangleGradientV(160, 120, 960, 400, (Color){50, 63, 72, 255}, (Color){36, 46, 54, 255});
    DrawTriangle((Vector2){160, 120}, (Vector2){240, 520}, (Vector2){160, 520}, (Color){28, 37, 45, 255});
    DrawTriangle((Vector2){1120, 120}, (Vector2){1120, 520}, (Vector2){1040, 520}, (Color){28, 37, 45, 255});
    for (int x = 240; x < 1040; x += 100)
    {
        DrawLine(x, 140, x, 505, (Color){31, 40, 47, 255});
        DrawLine(x + 1, 140, x + 1, 505, (Color){58, 72, 82, 255});
    }
    DrawLine(200, 330, 1080, 330, (Color){31, 40, 47, 255});
    DrawLine(200, 331, 1080, 331, (Color){58, 72, 82, 255});
    /* Rodape e mancha de infiltracao */
    DrawRectangle(240, 500, 800, 20, (Color){30, 38, 44, 255});
    DrawRectangle(240, 500, 800, 2, (Color){66, 80, 90, 255});
    DrawEllipse(760, 160, 46, 18, Fade((Color){20, 26, 30, 255}, 0.35f));
    DrawRectangleGradientV(752, 160, 14, 70, Fade((Color){20, 26, 30, 255}, 0.30f), Fade((Color){20, 26, 30, 255}, 0.0f));

    /* Placa de aviso na parede */
    Rectangle sign = {706, 148, 96, 54};
    R90Bevel(sign, (Color){210, 170, 40, 255}, 2);
    DrawTriangle((Vector2){754, 154}, (Vector2){742, 176}, (Vector2){766, 176}, (Color){30, 28, 24, 255});
    DrawTriangle((Vector2){754, 160}, (Vector2){747, 173}, (Vector2){761, 173}, (Color){210, 170, 40, 255});
    DrawRectangle(753, 162, 2, 6, (Color){30, 28, 24, 255});
    DrawPixel(753, 170, (Color){30, 28, 24, 255});
    DrawGameText("RESTRITO", 727, 182, 10, (Color){30, 28, 24, 255});

    /* Piso: linoleo industrial com reflexo da luminaria */
    DrawTriangle((Vector2){0, 650}, (Vector2){1040, 520}, (Vector2){240, 520}, (Color){33, 38, 43, 255});
    DrawTriangle((Vector2){0, 650}, (Vector2){1280, 650}, (Vector2){1040, 520}, (Color){27, 31, 36, 255});
    for (int x = -280; x <= SCREEN_WIDTH + 280; x += 112)
    {
        DrawLineEx((Vector2){640, 512}, (Vector2){(float)x, 650}, 1, Fade((Color){80, 96, 108, 255}, 0.22f));
    }
    float rows[] = {528, 540, 556, 577, 606, 645};
    for (int i = 0; i < 6; i++) DrawLine(0, (int)rows[i], 1280, (int)rows[i], Fade((Color){80, 96, 108, 255}, 0.16f));
    DrawEllipse(640, 560, 300, 34, Fade((Color){150, 210, 230, 255}, 0.05f));
    DrawEllipse(640, 560, 170, 18, Fade((Color){170, 225, 240, 255}, 0.05f));

    /* Tubulacoes com abracadeiras */
    R90Pipe((Rectangle){28, 110, 16, 420}, true);
    R90Pipe((Rectangle){1238, 110, 16, 420}, true);
    R90Pipe((Rectangle){170, 126, 940, 12}, false);
    R90Pipe((Rectangle){170, 140, 940, 6}, false);

    /* Grelha de ventilacao com ventoinha girando atras */
    Rectangle vent = {320, 145, 110, 80};
    R90Bevel(vent, (Color){58, 70, 80, 255}, 3);
    DrawRectangle(326, 151, 98, 68, (Color){14, 18, 22, 255});
    for (int b = 0; b < 4; b++)
    {
        float a = time * 2.2f + b * PI / 2.0f;
        DrawLineEx((Vector2){375, 185}, (Vector2){375 + cosf(a) * 28, 185 + sinf(a) * 28}, 7, (Color){34, 42, 48, 255});
    }
    for (int gy = 153; gy < 218; gy += 8)
    {
        DrawRectangle(326, gy, 98, 3, (Color){62, 76, 86, 255});
        DrawRectangle(326, gy + 3, 98, 1, (Color){24, 30, 36, 255});
    }
    R90Rivet(325, 150); R90Rivet(425, 150); R90Rivet(325, 220); R90Rivet(425, 220);

    /* Luminaria fluorescente com halo e cone de luz */
    float tubePulse = 0.88f + 0.12f * sinf(time * 12.0f);
    DrawTriangle((Vector2){505, 104}, (Vector2){360, 515}, (Vector2){920, 515}, Fade((Color){170, 230, 245, 255}, 0.035f));
    DrawTriangle((Vector2){560, 104}, (Vector2){470, 515}, (Vector2){810, 515}, Fade((Color){170, 230, 245, 255}, 0.03f));
    DrawEllipse(640, 96, 190, 26, Fade((Color){170, 230, 245, 255}, 0.08f * tubePulse));
    R90Bevel((Rectangle){496, 80, 288, 24}, (Color){70, 84, 92, 255}, 2);
    DrawRectangle(500, 76, 4, 6, (Color){50, 60, 66, 255});
    DrawRectangle(776, 76, 4, 6, (Color){50, 60, 66, 255});
    DrawRectangleRounded((Rectangle){512, 86, 256, 13}, 0.5f, 6, Fade((Color){215, 245, 255, 255}, tubePulse));
    DrawRectangle(520, 88, 240, 3, Fade(WHITE, 0.8f * tubePulse));
    for (int g = 0; g < 7; g++) DrawLine(540 + g * 34, 86, 540 + g * 34, 99, Fade((Color){60, 72, 80, 255}, 0.7f));

    /* ---------- FICHARIO DE ACO ---------- */
    DrawEllipse(212, 582, 160, 14, Fade(BLACK, 0.45f));
    /* lateral com profundidade */
    DrawRectangle(350, 168, 14, 412, (Color){34, 42, 48, 255});
    DrawTriangle((Vector2){350, 160}, (Vector2){364, 168}, (Vector2){350, 168}, (Color){34, 42, 48, 255});
    DrawRectangleRounded(armarios, 0.03f, 6, (Color){38, 48, 55, 255});
    DrawRectangleRec((Rectangle){72, 162, 276, 6}, (Color){96, 114, 124, 255});
    DrawRectangleRoundedLinesEx(armarios, 0.03f, 6, 2.0f, (Color){20, 26, 30, 255});
    for (int col = 0; col < 3; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            Rectangle gaveta = {(float)(78 + col * 90), (float)(170 + row * 98), 82, 88};
            bool open = (col == 2 && row == 1);
            Color steel = (Color){62, 76, 85, 255};
            if (open)
            {
                /* gaveta entreaberta com pastas aparecendo */
                DrawRectangle((int)gaveta.x - 2, (int)gaveta.y - 6, 86, 10, (Color){24, 30, 34, 255});
                Color tabs[4] = {{205, 180, 120, 255}, {190, 90, 70, 255}, {210, 200, 170, 255}, {120, 150, 190, 255}};
                for (int t = 0; t < 4; t++)
                    DrawRectangle((int)gaveta.x + 6 + t * 19, (int)gaveta.y - 12 + (t % 2) * 3, 15, 10, tabs[t]);
                gaveta.y += 4;
            }
            R90Bevel(gaveta, steel, 2);
            DrawRectangle((int)gaveta.x + 4, (int)gaveta.y + gaveta.height * 0.55f, 74, 1, R90Shade(steel, 0.8f));

            /* porta-etiqueta metalico + etiqueta */
            DrawRectangle((int)gaveta.x + 11, (int)gaveta.y + 11, 60, 24, (Color){150, 162, 170, 255});
            DrawRectangle((int)gaveta.x + 11, (int)gaveta.y + 33, 60, 2, (Color){60, 70, 76, 255});
            DrawRectangle((int)gaveta.x + 14, (int)gaveta.y + 14, 54, 18, (Color){226, 220, 200, 255});
            DrawRectangle((int)gaveta.x + 14, (int)gaveta.y + 14, 54, 2, (Color){245, 242, 230, 255});
            DrawGameText("1990", (int)gaveta.x + 24, (int)gaveta.y + 18, 10, INK_COLOR);

            /* puxador cromado com sombra */
            DrawRectangleRounded((Rectangle){gaveta.x + 18, gaveta.y + 47, 46, 12}, 0.5f, 4, (Color){22, 28, 32, 255});
            DrawRectangleRounded((Rectangle){gaveta.x + 18, gaveta.y + 44, 46, 12}, 0.5f, 4, (Color){170, 182, 192, 255});
            DrawRectangle((int)gaveta.x + 22, (int)gaveta.y + 45, 38, 2, (Color){235, 240, 245, 255});
            DrawRectangle((int)gaveta.x + 22, (int)gaveta.y + 50, 38, 4, (Color){52, 60, 66, 255});
            /* fechadura pequena */
            DrawCircle((int)gaveta.x + 70, (int)gaveta.y + 72, 3, (Color){150, 160, 168, 255});
            DrawLine((int)gaveta.x + 70, (int)gaveta.y + 70, (int)gaveta.x + 70, (int)gaveta.y + 74, (Color){30, 36, 40, 255});
        }
    }
    /* risco/desgaste */
    DrawLine(110, 405, 140, 400, Fade((Color){140, 155, 165, 255}, 0.5f));
    DrawLine(285, 520, 300, 528, Fade((Color){140, 155, 165, 255}, 0.4f));
    DrawGameText("FICHÁRIO CENTRAL // 1990", 112, 552, 13, (Color){160, 185, 200, 255});

    /* Caixa de papelao de disquetes */
    Rectangle boxDisks = {370, 538, 80, 54};
    DrawEllipse(412, 593, 48, 7, Fade(BLACK, 0.45f));
    DrawRectangle(450, 542, 10, 50, (Color){112, 84, 54, 255});
    DrawTriangle((Vector2){450, 538}, (Vector2){460, 542}, (Vector2){450, 542}, (Color){112, 84, 54, 255});
    R90Bevel(boxDisks, (Color){160, 124, 84, 255}, 2);
    DrawTriangle((Vector2){372, 538}, (Vector2){392, 528}, (Vector2){404, 538}, (Color){178, 140, 96, 255});
    DrawRectangle(372, 539, 76, 7, Fade((Color){200, 185, 140, 255}, 0.55f));
    DrawRectangle(374, 558, 72, 18, (Color){232, 226, 205, 255});
    DrawGameText("DISQUETES", (int)boxDisks.x + 8, (int)boxDisks.y + 24, 11, INK_COLOR);

    /* ---------- PORTA DE SEGURANCA ---------- */
    DrawRectangle(468, 143, 234, 380, (Color){30, 38, 44, 255});          /* batente */
    DrawRectangle(468, 143, 234, 4, (Color){80, 96, 106, 255});
    DrawRectangle(468, 143, 4, 380, (Color){70, 86, 96, 255});
    DrawRectangleRounded(porta, 0.04f, 6, (Color){54, 66, 75, 255});
    DrawRectangleGradientH((int)porta.x + 4, (int)porta.y + 4, 100, (int)porta.height - 8,
                           Fade((Color){120, 140, 152, 255}, 0.18f), Fade((Color){120, 140, 152, 255}, 0.0f));
    DrawRectangleRoundedLinesEx(porta, 0.04f, 6, 2.0f, (Color){18, 24, 28, 255});
    /* chapas e frisos */
    R90Bevel((Rectangle){490, 168, 190, 10}, (Color){44, 54, 62, 255}, 2);
    R90Bevel((Rectangle){490, 330, 190, 6}, (Color){44, 54, 62, 255}, 2);
    for (int s = 0; s < 7; s++)                                         /* faixa de risco */
    {
        float x0 = 486 + s * 28.0f;
        DrawRectangle((int)x0, 482, 28, 22, s % 2 ? (Color){30, 30, 26, 255} : (Color){214, 176, 44, 255});
    }
    DrawRectangleLinesEx((Rectangle){486, 482, 198, 22}, 1, (Color){18, 22, 26, 255});
    for (int r = 0; r < 6; r++) { R90Rivet(492 + r * 37.0f, 186); R90Rivet(492 + r * 37.0f, 472); }

    /* visor blindado com reflexos */
    Rectangle visor = {535, 210, 100, 110};
    DrawRectangleRounded((Rectangle){529, 204, 112, 122}, 0.1f, 6, (Color){34, 42, 48, 255});
    DrawRectangleRounded(visor, 0.08f, 6, (Color){16, 26, 34, 255});
    DrawRectangleGradientV(539, 214, 92, 102, Fade((Color){90, 150, 180, 255}, 0.18f), Fade((Color){20, 40, 55, 255}, 0.05f));
    DrawLineEx((Vector2){548, 300}, (Vector2){600, 220}, 6, Fade(RAYWHITE, 0.10f));
    DrawLineEx((Vector2){566, 306}, (Vector2){616, 230}, 2, Fade(RAYWHITE, 0.14f));
    for (int gx = 0; gx < 4; gx++) DrawLine(545 + gx * 26, 214, 545 + gx * 26, 316, Fade((Color){80, 100, 110, 255}, 0.25f));
    DrawRectangleRoundedLinesEx(visor, 0.08f, 6, 2, (Color){80, 100, 114, 255});

    /* painel de tranca (textos iguais ao original) */
    Rectangle lockPanel = {500, 345, 126, 76};
    DrawRectangleRounded((Rectangle){497, 342, 132, 82}, 0.12f, 6, (Color){70, 84, 94, 255});
    DrawRectangleRounded(lockPanel, 0.12f, 6, (Color){18, 23, 28, 255});
    DrawRectangleRoundedLinesEx(lockPanel, 0.12f, 6, 1.5f, (Color){40, 50, 58, 255});
    if (game->puzzleFilesCompleted)
    {
        float bGlow = 0.6f + 0.4f * sinf(time * 4.0f);
        DrawCircle(516, 368, 9, Fade((Color){50, 240, 100, 255}, 0.15f * bGlow));
        DrawCircle(516, 368, 5, Fade((Color){50, 240, 100, 255}, bGlow));
        DrawGameText("LIBERADO", 528, 361, 13, (Color){50, 240, 100, 255});
        DrawGameText("[ ENTRAR ]", 528, 388, 12, GOLD_COLOR);
    }
    else
    {
        float rGlow = 0.6f + 0.4f * sinf(time * 5.0f);
        DrawCircle(516, 368, 9, Fade(RED_COLOR, 0.18f * rGlow));
        DrawCircle(516, 368, 5, Fade(RED_COLOR, rGlow));
        DrawGameText("TRAVADA", 530, 361, 13, RED_COLOR);
        DrawGameText("REQUER INDEXAÇÃO", 508, 388, 10, (Color){245, 185, 75, 255});
    }

    /* volante de escotilha com raios */
    DrawCircle(647, 386, 20, Fade(BLACK, 0.4f));
    DrawRing((Vector2){645, 384}, 14, 19, 0, 360, 32, (Color){96, 112, 122, 255});
    DrawRing((Vector2){645, 384}, 17, 19, 200, 340, 16, (Color){150, 166, 176, 255});
    for (int k = 0; k < 3; k++)
    {
        float a = k * (2.0f * PI / 3.0f) + 0.3f;
        DrawLineEx((Vector2){645, 384}, (Vector2){645 + cosf(a) * 15, 384 + sinf(a) * 15}, 3, (Color){80, 94, 104, 255});
    }
    DrawCircle(645, 384, 5, (Color){130, 146, 156, 255});
    DrawCircle(644, 383, 2, (Color){210, 220, 226, 255});

    /* ---------- MESA E TERMINAL ---------- */
    /* sombra sob a mesa e pernas metalicas */
    DrawTriangle((Vector2){794, 514}, (Vector2){1110, 514}, (Vector2){1110, 612}, Fade(BLACK, 0.40f));
    DrawTriangle((Vector2){794, 514}, (Vector2){1110, 612}, (Vector2){794, 612}, Fade(BLACK, 0.40f));
    for (int l = 0; l < 2; l++)
    {
        int lx = l == 0 ? 770 : 1110;
        DrawRectangle(lx, 514, 24, 95, (Color){50, 58, 64, 255});
        DrawRectangle(lx + 3, 514, 4, 95, (Color){92, 104, 112, 255});
        DrawRectangle(lx + 19, 514, 5, 95, (Color){30, 36, 40, 255});
        DrawRectangle(lx - 4, 605, 32, 6, (Color){36, 42, 46, 255});
    }

    /* Colecionavel oculto (E): radio portatil (identico ao atual) */
    Rectangle radio1990 = {930, 562, 72, 40};
    if (!game->hiddenFound[HIDDEN_1990])
    {
        DrawEllipse(966, 603, 40, 5, Fade(BLACK, 0.45f));
        DrawLineEx((Vector2){990, 572}, (Vector2){1022, 536}, 2.0f, (Color){92, 98, 102, 255});
        DrawCircle(1022, 536, 2, (Color){105, 112, 116, 255});
        DrawRectangleLinesEx((Rectangle){946, 562, 40, 12}, 2.0f, (Color){38, 40, 42, 255});
        DrawRectangleRounded((Rectangle){930, 570, 72, 32}, 0.25f, 6, (Color){38, 40, 42, 255});
        DrawRectangleRoundedLinesEx((Rectangle){930, 570, 72, 32}, 0.25f, 6, 1.5f, (Color){54, 58, 60, 255});
        DrawCircle(950, 586, 11, (Color){30, 32, 34, 255});
        for (int g = -8; g <= 8; g += 4) DrawLine(942, 586 + g, 958, 586 + g, (Color){58, 60, 62, 255});
        DrawRectangle(968, 576, 28, 8, (Color){66, 62, 48, 255});
        DrawLine(980, 576, 980, 584, (Color){130, 50, 42, 255});
        DrawCircle(974, 594, 3, (Color){70, 72, 74, 255});
        DrawCircle(988, 594, 3, (Color){70, 72, 74, 255});
        float ledPulse = 0.35f + 0.25f * sinf(time * 1.7f);
        DrawCircle(997, 591, 1.8f, Fade((Color){230, 40, 30, 255}, ledPulse));
        DrawHiddenGlint(radio1990, 0.8f);
    }

    /* tampo de madeira com veios e borda iluminada */
    DrawRectangle(740, 480, 420, 28, (Color){104, 74, 50, 255});
    for (int v = 0; v < 5; v++)
        DrawLine(744, 485 + v * 5, 1156, 486 + v * 5 + (v % 2), Fade((Color){70, 48, 30, 255}, 0.35f));
    DrawRectangle(740, 480, 420, 3, (Color){150, 112, 78, 255});
    DrawRectangle(740, 504, 420, 10, (Color){62, 44, 30, 255});
    DrawRectangle(740, 504, 420, 2, (Color){120, 88, 60, 255});

    /* CRT: gabinete traseiro em perspectiva + moldura bege com volume */
    DrawRectangle(846, 455, 228, 26, (Color){150, 142, 118, 255});   /* base/pe */
    DrawRectangle(846, 455, 228, 3, (Color){200, 192, 168, 255});
    DrawRectangleRounded((Rectangle){810, 245, 300, 220}, 0.08f, 8, (Color){196, 188, 160, 255});
    DrawRectangleRounded((Rectangle){810, 245, 300, 8}, 0.5f, 8, (Color){222, 216, 192, 255});
    DrawRectangleRoundedLinesEx((Rectangle){810, 245, 300, 220}, 0.08f, 8, 2, (Color){110, 104, 84, 255});
    /* moldura interna chanfrada */
    DrawRectangleRounded((Rectangle){824, 259, 272, 172}, 0.07f, 6, (Color){150, 142, 118, 255});
    /* etiqueta da marca e LED de energia */
    DrawRectangle(828, 440, 56, 12, (Color){170, 162, 136, 255});
    DrawGameText("ARIEL-1", 833, 441, 10, (Color){80, 74, 60, 255});
    DrawCircle(1088, 446, 3, (Color){60, 230, 110, 255});
    DrawCircle(1088, 446, 6, Fade((Color){60, 230, 110, 255}, 0.18f));
    DrawRectangle(1040, 442, 34, 8, (Color){170, 162, 136, 255});
    DrawRectangle(1040, 442, 34, 2, (Color){120, 112, 92, 255});

    /* Tela CRT (textos identicos ao original) */
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
        DrawGameText("conectados às pastas!", 852, 362, 13, RAYWHITE);
        DrawGameText("> TRAVA DA PORTA ABERTA <", 852, 392, 13, (Color){100, 245, 150, 255});
    }
    else if (!game->memory1990Completed)
    {
        DrawGameText("[!] ACESSO PROTEGIDO", 852, 316, 14, GOLD_COLOR);
        DrawGameText("Autenticação de memória", 852, 340, 13, RAYWHITE);
        DrawGameText("exigida na inicialização.", 852, 362, 13, RAYWHITE);
        float pBlink = 0.5f + 0.5f * sinf(time * 5.0f);
        DrawGameText("> CLIQUE PARA DESBLOQUEAR <", 844, 392, 13, Fade(CYAN_COLOR, pBlink));
    }
    else
    {
        DrawGameText("[!] INDEXAÇÃO PENDENTE", 852, 316, 14, GOLD_COLOR);
        DrawGameText("Arraste os arquivos para", 852, 340, 13, RAYWHITE);
        DrawGameText("as pastas correspondentes.", 852, 362, 13, RAYWHITE);
        float pBlink = 0.5f + 0.5f * sinf(time * 5.0f);
        DrawGameText("> CLIQUE PARA ORGANIZAR <", 848, 392, 13, Fade(CYAN_COLOR, pBlink));
    }
    /* reflexo curvo do vidro (sem cobrir texto) */
    DrawTriangle((Vector2){834, 269}, (Vector2){834, 300}, (Vector2){880, 269}, Fade(WHITE, 0.06f));
    DrawCircleGradientCompat(960, 345, 150, Fade(crtText, 0.05f), Fade(crtText, 0.0f));

    /* teclado com duas fileiras e cabo */
    Rectangle kbRec2 = {835, 484, 250, 22};
    DrawRectangleRounded((Rectangle){833, 488, 254, 22}, 0.2f, 4, (Color){90, 84, 66, 255});
    DrawRectangleRounded(kbRec2, 0.2f, 4, (Color){190, 182, 156, 255});
    DrawRectangle(840, 485, 240, 2, (Color){225, 218, 196, 255});
    for (int k = 0; k < 14; k++)
    {
        DrawRectangle((int)kbRec2.x + 8 + k * 17, (int)kbRec2.y + 4, 14, 6, (Color){92, 94, 86, 255});
        DrawRectangle((int)kbRec2.x + 8 + k * 17, (int)kbRec2.y + 4, 14, 1, (Color){140, 142, 132, 255});
        if (k < 13)
        {
            DrawRectangle((int)kbRec2.x + 16 + k * 17, (int)kbRec2.y + 12, 14, 6, (Color){80, 82, 76, 255});
            DrawRectangle((int)kbRec2.x + 16 + k * 17, (int)kbRec2.y + 12, 14, 1, (Color){130, 132, 122, 255});
        }
    }
    DrawLineBezier((Vector2){960, 484}, (Vector2){1010, 470}, 2, (Color){50, 50, 46, 255});

    /* pilha de disquetes 3.5" com obturador metalico e etiqueta */
    Color dcol[3] = {{45, 110, 200, 255}, {200, 56, 56, 255}, {214, 170, 48, 255}};
    for (int d = 0; d < 3; d++)
    {
        int dx = 1095 + d * 3, dy = 476 - d * 4;
        DrawRectangle(dx + 2, dy + 2, 32, 28, Fade(BLACK, 0.35f));
        R90Bevel((Rectangle){(float)dx, (float)dy, 32, 28}, dcol[d], 1);
        DrawRectangle(dx + 8, dy, 16, 9, (Color){176, 186, 194, 255});
        DrawRectangle(dx + 18, dy + 2, 4, 5, (Color){60, 66, 70, 255});
        DrawRectangle(dx + 5, dy + 14, 22, 12, (Color){236, 232, 218, 255});
    }
    /* caneca de cafe */
    DrawRectangle(1050, 462, 18, 20, (Color){170, 60, 50, 255});
    DrawRectangle(1050, 462, 4, 20, (Color){205, 90, 76, 255});
    DrawRing((Vector2){1069, 471}, 4, 6, -90, 90, 8, (Color){150, 50, 42, 255});
    DrawEllipse(1059, 463, 9, 2, (Color){60, 34, 22, 255});

    if (game->year2008Unlocked) DrawButton(btnVoltar, "SALA 2 (2008) >", (Color){45, 115, 75, 255});

    DrawHotspot(armarios, "Examinar arquivos de fichas");
    DrawHotspot(terminal, game->puzzleFilesCompleted ? "Terminal (Indexação 100% OK)" : (!game->memory1990Completed ? "Acessar terminal [DESBLOQUEAR ACESSO]" : "Acessar sistema de arquivos [PUZZLE]"));
    DrawHotspot(porta, game->puzzleFilesCompleted ? "Acessar porta de saída [ENTRAR]" : "Porta trancada (requer indexação)");
    if (game->year2008Unlocked) DrawHotspot(btnVoltar, "Avançar para o escritório de 2008");
}

void DrawHudRoom2(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawHudYearOperator("1990", game->playerName, 24);
    DrawHudLivesTimer(game->lives, minutes * 60 + seconds);

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
    DrawGameText(game->musicMuted ? "[M] ÁUDIO: MUDO" : "[M] ÁUDIO: ON",
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
    if (UpdateJournal(game)) return;

    /* Colecionavel oculto (E): radio portatil sob a mesa, fora de todos os hotspots */
    Rectangle radio1990 = {930, 562, 72, 40};
    if (!game->hiddenFound[HIDDEN_1990] && !game->phaseMenuOpen && Clicked(radio1990))
    {
        CollectHidden(game, HIDDEN_1990);
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
            SetMessage(game, "Terminal sincronizado: todos os arquivos estão conectados!");
        }
    }
    else if (Clicked(armarios))
    {
        game->room2CabinetInspected = true;
        game->journalYearTab = 0;
        game->journalPage = 1;
        SetMessage(game, "Armários de fichas: documentação dos primeiros modelos de teste de 1990.");
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
        "PASTA: PARÂMETROS",
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
        DrawGameText(hasConnectedFile ? "[ARQUIVO ARMAZENADO]" : "[DIRETÓRIO VAZIO]",
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
                             "Arquivo armazenado com sucesso no diretório correto!");
                    SetMessage(game, "Arquivo armazenado com sucesso no diretório!");

                    /* Verifica se todos os 4 foram conectados */
                    bool allConnected = true;
                    for (int k = 0; k < 4; k++)
                    {
                        if (game->fileConnections[k] == -1) allConnected = false;
                    }
                    if (allConnected)
                    {
                        game->puzzleFilesCompleted = true;
                        SetMessage(game, "Indexação completa! A porta foi destrancada.");
                    }
                }
                else
                {
                    /* Diretório incorreto: dá feedback visual e retorna */
                    game->puzzleFilesErrorFolder = j;
                    game->puzzleFilesFeedbackIsError = true;
                    game->puzzleFilesFeedbackTimer = 2.2f;
                    snprintf(game->puzzleFilesFeedback, sizeof(game->puzzleFilesFeedback),
                             "Diretório incorreto! Este arquivo não pertence a esta pasta.");
                    SetMessage(game, "Destino incompatível! Escolha a pasta correta para o arquivo.");
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
    DrawGameText("SALA DE 1990 CONCLUÍDA", 540, 208, 22, GOLD_COLOR);

    /* Recompensa Especial */
    DrawGameText("RECOMPENSA OBTIDA: DISQUETE", 540, 244, 19, CYAN_COLOR);

    /* Texto Narrativo Padronizado (Toda a narrativa na mesma cor padronizada REWARD_BODY_COLOR) */
    DrawGameText("Elira restaurou a organização do sistema", 540, 286, 17, REWARD_BODY_COLOR);
    DrawGameText("e recuperou o registro inicial do projeto.", 540, 312, 17, REWARD_BODY_COLOR);
    DrawGameText("A IA não nasceu corrompida: o projeto visava uma mente", 540, 348, 17, REWARD_BODY_COLOR);
    DrawGameText("livre e cooperativa. Mas algo aconteceu décadas depois...", 540, 374, 17, REWARD_BODY_COLOR);

    /* Frase Especial de Proxima Fronteira */
    DrawGameText("-> Próxima Fronteira Temporal: FASE 2 // ANO 2008!", 540, 452, 19, CYAN_COLOR);

    /* Botoes Centralizados */
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANÇAR PRÓXIMA FASE >> [ENTER]", GOLD_COLOR);
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
        game->screen = SCREEN_INTRO_2008;
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

static void DrawCorpButton2008(Rectangle bounds, const char *label, Color baseCol, bool enabled)
{
    Vector2 mouse = GetVirtualMouse();
    bool hover = enabled && CheckCollisionPointRec(mouse, bounds);

    Color topCol = baseCol;
    if (hover)
    {
        topCol = (Color){
            (unsigned char)fminf(255.0f, baseCol.r + 26.0f),
            (unsigned char)fminf(255.0f, baseCol.g + 26.0f),
            (unsigned char)fminf(255.0f, baseCol.b + 26.0f),
            255
        };
    }
    Color botCol = (Color){
        (unsigned char)fmaxf(0.0f, topCol.r - 28.0f),
        (unsigned char)fmaxf(0.0f, topCol.g - 28.0f),
        (unsigned char)fmaxf(0.0f, topCol.b - 28.0f),
        255
    };
    Color borderCol = hover ? RAYWHITE : (Color){
        (unsigned char)fmaxf(0.0f, baseCol.r - 45.0f),
        (unsigned char)fmaxf(0.0f, baseCol.g - 45.0f),
        (unsigned char)fmaxf(0.0f, baseCol.b - 45.0f),
        255
    };

    /* Sombra 3D suave */
    DrawRectangleRounded((Rectangle){bounds.x + 2, bounds.y + 3, bounds.width, bounds.height}, 0.20f, 6, Fade(BLACK, 0.22f));

    /* Corpo principal do botao e reflexo superior estilo Aero 2008 */
    DrawRectangleRounded(bounds, 0.20f, 6, botCol);
    DrawRectangleRounded((Rectangle){bounds.x + 2, bounds.y + 2, bounds.width - 4, bounds.height * 0.48f}, 0.20f, 6, Fade(RAYWHITE, hover ? 0.28f : 0.16f));
    DrawRectangleRoundedLinesEx(bounds, 0.20f, 6, hover ? 2.2f : 1.5f, borderCol);

    int fontSize = (bounds.height >= 42) ? 16 : 15;
    int textW = MeasureGameText(label, fontSize);
    int tx = (int)(bounds.x + (bounds.width - textW) / 2);
    int ty = (int)(bounds.y + (bounds.height - fontSize) / 2);

    DrawGameText(label, tx + 1, ty + 1, fontSize, Fade(BLACK, 0.45f));
    DrawGameText(label, tx, ty, fontSize, enabled ? RAYWHITE : (Color){225, 230, 238, 255});
}

void DrawDocumentViewer2008(const GameState *game)
{
    if (game->activeDocument2008 < 0) return;

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade((Color){12, 18, 28, 255}, 0.72f));

    /* Janela estilo Sistema Corporativo 2008 (Clara e detalhada) */
    Rectangle panel = {220, 50, 840, 620};
    DrawRectangleRounded((Rectangle){panel.x + 8, panel.y + 10, panel.width, panel.height}, 0.03f, 8, Fade(BLACK, 0.38f));
    DrawRectangleRounded(panel, 0.03f, 8, (Color){228, 234, 242, 255});
    DrawRectangleRoundedLinesEx(panel, 0.03f, 8, 3.0f, (Color){52, 92, 142, 255});

    int doc = game->activeDocument2008;

    /* Barra de Titulo Aero-Blue 2008 */
    Rectangle headerBar = {panel.x + 6, panel.y + 6, panel.width - 12, 46};
    DrawRectangleRounded(headerBar, 0.12f, 6, (Color){36, 86, 148, 255});
    DrawRectangleRounded((Rectangle){headerBar.x + 2, headerBar.y + 2, headerBar.width - 4, 20}, 0.12f, 6, Fade(RAYWHITE, 0.22f));
    DrawRectangleRoundedLinesEx(headerBar, 0.12f, 6, 1.5f, (Color){22, 58, 104, 255});

    const char *headerTitles[6] = {
        "EVIDÊNCIA 01 // COMPARAÇÃO BIOMÉTRICA (DRONE)",
        "EVIDÊNCIA 02 // PANFLETO DA REVOLTA POPULAR",
        "EVIDÊNCIA 03 // RELATÓRIO DA CATRACA BIOMÉTRICA",
        "EVIDÊNCIA 04 // GRAVAÇÃO INTERNA CFTV (SERVIDORES)",
        "ARQUIVO HISTÓRICO // ANOTAÇÕES DO DR. RAMOS (2004)",
        "CONFIDENCIAL // MEMORANDO MINISTÉRIO DA SEGURANÇA (2008)"
    };
    DrawCircle((int)headerBar.x + 22, (int)headerBar.y + 23, 6.0f, GOLD_COLOR);
    DrawGameText(headerTitles[doc], (int)(headerBar.x + 38), (int)(headerBar.y + 14), 18, RAYWHITE);

    /* Area visual interna do dossie (Folha clara corporativa) */
    Rectangle contentBox = {panel.x + 24, panel.y + 66, panel.width - 48, 486};
    DrawRectangleRounded((Rectangle){contentBox.x + 3, contentBox.y + 4, contentBox.width, contentBox.height}, 0.03f, 6, Fade(BLACK, 0.10f));
    DrawRectangleRounded(contentBox, 0.03f, 6, (Color){248, 250, 254, 255});
    DrawRectangleRoundedLinesEx(contentBox, 0.03f, 6, 1.8f, (Color){172, 184, 202, 255});

    /* Grade tecnica sutil no fundo da folha */
    for (int gx = (int)contentBox.x + 24; gx < (int)(contentBox.x + contentBox.width); gx += 32)
        DrawLine(gx, (int)contentBox.y + 4, gx, (int)(contentBox.y + contentBox.height - 4), Fade((Color){190, 204, 222, 255}, 0.28f));
    for (int gy = (int)contentBox.y + 24; gy < (int)(contentBox.y + contentBox.height); gy += 32)
        DrawLine((int)contentBox.x + 4, gy, (int)(contentBox.x + contentBox.width - 4), gy, Fade((Color){190, 204, 222, 255}, 0.28f));

    if (doc == 0)
    {
        /* --- DOC 0: COMPARACAO FACIAL (DRONE) --- */
        Rectangle imgRec = {contentBox.x + 25, contentBox.y + 25, 240, 260};
        DrawRectangleRounded(imgRec, 0.04f, 6, (Color){18, 28, 40, 255});
        DrawRectangleRoundedLinesEx(imgRec, 0.04f, 6, 2.5f, (Color){70, 95, 120, 255});
        /* Grade e silhueta facial */
        DrawCircle((int)(imgRec.x + 120), (int)(imgRec.y + 110), 55, Fade((Color){110, 155, 188, 255}, 0.55f));
        DrawRectangleLinesEx((Rectangle){imgRec.x + 80, imgRec.y + 70, 80, 85}, 2.0f, RED_COLOR);
        DrawCircle((int)(imgRec.x + 100), (int)(imgRec.y + 100), 4, RED_COLOR);
        DrawCircle((int)(imgRec.x + 140), (int)(imgRec.y + 100), 4, RED_COLOR);
        DrawLine((int)(imgRec.x + 120), (int)(imgRec.y + 105), (int)(imgRec.x + 120), (int)(imgRec.y + 125), RED_COLOR);
        DrawLine((int)(imgRec.x + 105), (int)(imgRec.y + 138), (int)(imgRec.x + 135), (int)(imgRec.y + 138), RED_COLOR);
        for (int gy = (int)imgRec.y + 10; gy < (int)(imgRec.y + imgRec.height); gy += 14)
            DrawLine((int)imgRec.x, gy, (int)(imgRec.x + imgRec.width), gy, Fade(BLACK, 0.30f));

        DrawRectangle((int)imgRec.x, (int)(imgRec.y + imgRec.height - 34), (int)imgRec.width, 34, (Color){185, 42, 36, 255});
        DrawGameText("SIMILARIDADE: 78%", (int)imgRec.x + 40, (int)(imgRec.y + imgRec.height - 24), 16, RAYWHITE);

        int tx = (int)(imgRec.x + imgRec.width + 30);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("RELATÓRIO DE RECONHECIMENTO FACIAL - DRONE ALPHA-4", tx, ty, 17, (Color){28, 72, 128, 255});
        DrawLine(tx, ty + 24, tx + 460, ty + 24, (Color){145, 168, 196, 255});

        DrawGameText("ALVO DETECTADO: Lucas Silva (Registro #AR-2008-09)", tx, ty + 38, 15, (Color){24, 32, 44, 255});
        DrawGameText("LOCAL DO AVISTAMENTO: Praça Central (Setor Leste)", tx, ty + 64, 15, (Color){55, 68, 84, 255});
        DrawGameText("HORÁRIO INFORMADO: 14:30:00", tx, ty + 90, 15, (Color){185, 105, 12, 255});
        DrawGameText("ÍNDICE DE CONFIANÇA: 78% (Grau de incerteza elevado)", tx, ty + 116, 15, (Color){195, 42, 35, 255});

        Rectangle noteBox = {(float)tx, (float)(ty + 150), 470, 124};
        DrawRectangleRounded(noteBox, 0.06f, 6, (Color){252, 242, 240, 255});
        DrawRectangleRoundedLinesEx(noteBox, 0.06f, 6, 1.5f, (Color){210, 120, 115, 255});
        DrawGameText("PARECER PERICIAL:", tx + 14, ty + 162, 14, (Color){175, 38, 32, 255});
        DrawGameText("- Imagem aérea com forte granulação e desfoque de movimento.", tx + 14, ty + 186, 14, (Color){45, 55, 68, 255});
        DrawGameText("- O algoritmo assume correspondência aproximada sem prova cabal.", tx + 14, ty + 208, 14, (Color){45, 55, 68, 255});
        DrawGameText("- Sozinha, esta imagem NÃO comprova que o alvo estava no local.", tx + 14, ty + 232, 14, (Color){185, 35, 30, 255});

        DrawGameText("Obs: Necessita de contraprova cronológica para demonstrar o erro.", (int)contentBox.x + 30, (int)(contentBox.y + 320), 15, (Color){85, 98, 115, 255});
    }
    else if (doc == 1)
    {
        /* --- DOC 1: PANFLETO DA REVOLTA --- */
        Rectangle flyerPaper = {contentBox.x + 35, contentBox.y + 25, 260, 310};
        DrawRectangleRounded((Rectangle){flyerPaper.x + 4, flyerPaper.y + 5, flyerPaper.width, flyerPaper.height}, 0.04f, 6, Fade(BLACK, 0.15f));
        DrawRectangleRounded(flyerPaper, 0.04f, 6, PAPER_COLOR);
        DrawRectangleRoundedLinesEx(flyerPaper, 0.04f, 6, 2.0f, (Color){160, 145, 115, 255});

        /* Arte do panfleto */
        DrawRectangle((int)flyerPaper.x + 18, (int)flyerPaper.y + 20, (int)flyerPaper.width - 36, 32, (Color){185, 45, 35, 255});
        DrawGameText("MANIFESTO POPULAR", (int)flyerPaper.x + 28, (int)flyerPaper.y + 28, 16, RAYWHITE);
        DrawGameText("\"NÃO À VIGILÂNCIA\"", (int)flyerPaper.x + 46, (int)flyerPaper.y + 70, 18, INK_COLOR);
        DrawGameText("Exigimos o fim dos drones", (int)flyerPaper.x + 28, (int)flyerPaper.y + 110, 14, INK_COLOR);
        DrawGameText("biométricos sobre civis!", (int)flyerPaper.x + 28, (int)flyerPaper.y + 130, 14, INK_COLOR);
        DrawGameText("Porta-voz: Lucas Silva", (int)flyerPaper.x + 28, (int)flyerPaper.y + 175, 16, (Color){145, 35, 25, 255});
        DrawGameText("Marcha Matutina: 10h às 13h", (int)flyerPaper.x + 28, (int)flyerPaper.y + 215, 14, (Color){85, 75, 60, 255});
        DrawGameText("[APREENDIDO PELA POLÍCIA]", (int)flyerPaper.x + 24, (int)flyerPaper.y + 265, 13, (Color){185, 38, 32, 255});

        int tx = (int)(flyerPaper.x + flyerPaper.width + 35);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("DOCUMENTO APREENDIDO: PANFLETO DE ATIVISMO", tx, ty, 17, (Color){28, 72, 128, 255});
        DrawLine(tx, ty + 24, tx + 440, ty + 24, (Color){145, 168, 196, 255});

        DrawGameText("LOCAL DA APREENSÃO: Praça Central (Manhã do protesto)", tx, ty + 38, 15, (Color){24, 32, 44, 255});
        DrawGameText("HORÁRIO INFORMADO NO PANFLETO: 10:00 às 13:00", tx, ty + 64, 15, (Color){185, 105, 12, 255});

        Rectangle boxP = {(float)tx, (float)(ty + 105), 450, 160};
        DrawRectangleRounded(boxP, 0.06f, 6, (Color){252, 246, 228, 255});
        DrawRectangleRoundedLinesEx(boxP, 0.06f, 6, 1.5f, (Color){210, 165, 75, 255});
        DrawGameText("ANÁLISE PERICIAL:", tx + 14, ty + 120, 14, (Color){165, 95, 12, 255});
        DrawGameText("- O panfleto cita nominalmente Lucas Silva como líder civil.", tx + 14, ty + 148, 14, (Color){45, 55, 68, 255});
        DrawGameText("- Explica o motivo pelo qual a IA o inseriu na lista de alvos.", tx + 14, ty + 172, 14, (Color){45, 55, 68, 255});
        DrawGameText("- PORÉM, a marcha terminou às 13:00. O panfleto NÃO prova", tx + 14, ty + 198, 14, (Color){185, 85, 15, 255});
        DrawGameText("  onde ele estava às 14:30 (horário do suposto disparo).", tx + 14, ty + 220, 14, (Color){185, 85, 15, 255});
    }
    else if (doc == 2)
    {
        /* --- DOC 2: REGISTRO DE ENTRADA (CATRACA) --- */
        Rectangle cardRec = {contentBox.x + 35, contentBox.y + 25, 300, 280};
        DrawRectangleRounded((Rectangle){cardRec.x + 4, cardRec.y + 5, cardRec.width, cardRec.height}, 0.04f, 6, Fade(BLACK, 0.14f));
        DrawRectangleRounded(cardRec, 0.04f, 6, (Color){238, 244, 250, 255});
        DrawRectangleRoundedLinesEx(cardRec, 0.04f, 6, 2.0f, (Color){75, 115, 155, 255});

        DrawRectangle((int)cardRec.x + 15, (int)cardRec.y + 15, (int)cardRec.width - 30, 28, (Color){35, 72, 112, 255});
        DrawGameText("TECH-CORP // CONTROLE DE ACESSO", (int)cardRec.x + 24, (int)cardRec.y + 22, 13, RAYWHITE);

        DrawGameText("COLABORADOR: Lucas Silva", (int)cardRec.x + 20, (int)cardRec.y + 60, 15, (Color){25, 35, 45, 255});
        DrawGameText("FUNÇÃO: Engenheiro de Infraestrutura", (int)cardRec.x + 20, (int)cardRec.y + 85, 13, (Color){65, 75, 85, 255});
        DrawGameText("CRACHÁ DE ACESSO: #0842", (int)cardRec.x + 20, (int)cardRec.y + 110, 14, (Color){35, 45, 55, 255});

        DrawRectangle((int)cardRec.x + 15, (int)cardRec.y + 140, (int)cardRec.width - 30, 60, Fade((Color){45, 165, 75, 255}, 0.22f));
        DrawRectangleLines((int)cardRec.x + 15, (int)cardRec.y + 140, (int)cardRec.width - 30, 60, (Color){35, 145, 65, 255});
        DrawGameText("ENTRADA: 14:15:08", (int)cardRec.x + 24, (int)cardRec.y + 150, 16, (Color){22, 118, 48, 255});
        DrawGameText("Catraca Portaria Principal - Biometria Digital", (int)cardRec.x + 24, (int)cardRec.y + 174, 12, (Color){45, 75, 55, 255});

        DrawGameText("SAÍDA: [EM ABERTO / EXPEDIENTE EM CURSO]", (int)cardRec.x + 20, (int)cardRec.y + 220, 12, (Color){165, 45, 35, 255});
        DrawGameText("Carimbo do Servidor Central // Não alterado", (int)cardRec.x + 20, (int)cardRec.y + 245, 11, (Color){95, 105, 118, 255});

        int tx = (int)(cardRec.x + cardRec.width + 35);
        int ty = (int)(contentBox.y + 30);
        DrawGameText("REGISTRO FORMAL: LOG DA CATRACA ELETRÔNICA", tx, ty, 17, (Color){28, 72, 128, 255});
        DrawLine(tx, ty + 24, tx + 400, ty + 24, (Color){145, 168, 196, 255});

        DrawGameText("LOCAL DO REGISTRO: Sede da Empresa (a 12 km da Praça)", tx, ty + 42, 15, (Color){24, 32, 44, 255});
        DrawGameText("HORÁRIO DO ACESSO: 14:15:08", tx, ty + 68, 15, (Color){25, 138, 62, 255});

        Rectangle boxE = {(float)tx, (float)(ty + 105), 415, 160};
        DrawRectangleRounded(boxE, 0.06f, 6, (Color){235, 248, 238, 255});
        DrawRectangleRoundedLinesEx(boxE, 0.06f, 6, 1.5f, (Color){75, 168, 102, 255});
        DrawGameText("IMPORTÂNCIA PARA A CONTESTAÇÃO:", tx + 14, ty + 120, 14, (Color){22, 122, 52, 255});
        DrawGameText("- Comprova formalmente a entrada de Lucas no prédio às 14:15.", tx + 14, ty + 148, 14, (Color){40, 52, 65, 255});
        DrawGameText("- O prédio fica a 12 km de distância da manifestação.", tx + 14, ty + 172, 14, (Color){40, 52, 65, 255});
        DrawGameText("- PORÉM, sozinho, o registro de ponto requer comprovação de", tx + 14, ty + 198, 14, (Color){165, 95, 12, 255});
        DrawGameText("  que ele permaneceu dentro no momento exato do disparo (14:30).", tx + 14, ty + 220, 14, (Color){165, 95, 12, 255});
    }
    else if (doc == 3)
    {
        /* --- DOC 3: CAMERA INTERNA CFTV (SERVIDORES) --- */
        Rectangle cctvBox = {contentBox.x + 25, contentBox.y + 25, 300, 240};
        DrawRectangleRounded(cctvBox, 0.04f, 6, (Color){10, 24, 16, 255});
        DrawRectangleRoundedLinesEx(cctvBox, 0.04f, 6, 2.5f, (Color){45, 175, 85, 255});

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
        DrawGameText("CIRCUITO FECHADO (CFTV): SALA DE SERVIDORES", tx, ty, 17, (Color){28, 72, 128, 255});
        DrawLine(tx, ty + 24, tx + 420, ty + 24, (Color){145, 168, 196, 255});

        DrawGameText("LOCAL DA GRAVAÇÃO: Subsolo da Empresa (Rack de Redes)", tx, ty + 42, 15, (Color){24, 32, 44, 255});
        DrawGameText("TIMESTAMP EXATO DA CÂMERA: 14:30:12", tx, ty + 68, 15, (Color){25, 138, 62, 255});

        Rectangle boxC = {(float)tx, (float)(ty + 105), 430, 160};
        DrawRectangleRounded(boxC, 0.06f, 6, (Color){235, 248, 238, 255});
        DrawRectangleRoundedLinesEx(boxC, 0.06f, 6, 1.5f, (Color){75, 168, 102, 255});
        DrawGameText("IMPORTÂNCIA PARA A CONTESTAÇÃO:", tx + 14, ty + 120, 14, (Color){22, 122, 52, 255});
        DrawGameText("- Imagem estática nítida de Lucas trabalhando nos servidores.", tx + 14, ty + 148, 14, (Color){40, 52, 65, 255});
        DrawGameText("- O horário bate no EXATO MINUTO em que o drone afirma tê-lo visto.", tx + 14, ty + 172, 14, (Color){22, 128, 55, 255});
        DrawGameText("- Quando combinada com o Registro de Entrada (#3), cria", tx + 14, ty + 198, 14, (Color){165, 95, 12, 255});
        DrawGameText("  um ÁLIBI COMPLETO E IRREFUTÁVEL de falso positivo!", tx + 14, ty + 220, 14, (Color){22, 128, 55, 255});
    }
    else if (doc == 4)
    {
        /* --- DOC 4: NOTAS DO DR. RAMOS (DESCOBERTA OPCIONAL 1) --- */
        int tx = (int)(contentBox.x + 35);
        int ty = (int)(contentBox.y + 25);
        DrawGameText("CADERNO DE PESQUISAS DO DR. RAMOS // ANO 2004", tx, ty, 18, (Color){155, 92, 18, 255});
        DrawLine(tx, ty + 26, tx + 710, ty + 26, (Color){195, 165, 115, 255});

        Rectangle pBox = {(float)tx, (float)(ty + 42), 710, 250};
        DrawRectangleRounded(pBox, 0.04f, 6, (Color){252, 248, 236, 255});
        DrawRectangleRoundedLinesEx(pBox, 0.04f, 6, 1.8f, (Color){185, 148, 88, 255});

        DrawGameText("\"O reconhecimento facial foi desenvolvido para resgatar seres humanos.\"", tx + 24, ty + 60, 16, (Color){155, 88, 14, 255});
        DrawGameText("Criamos esta rede neural para encontrar crianças perdidas em multidões", tx + 24, ty + 95, 15, (Color){35, 45, 58, 255});
        DrawGameText("e sobreviventes soterrados sob escombros de catástrofes naturais.", tx + 24, ty + 120, 15, (Color){35, 45, 58, 255});
        DrawGameText("A visão computacional deve ser um farol de acolhimento e proteção da vida.", tx + 24, ty + 145, 15, (Color){35, 45, 58, 255});
        DrawGameText("Jamais imaginei que a máquina seria transformada em cárcere e censura.", tx + 24, ty + 170, 15, (Color){175, 52, 38, 255});

        DrawGameText("DIRETRIZ DE PROTEÇÃO ÉTICA LOCAL:", tx + 24, ty + 205, 14, (Color){28, 82, 142, 255});
        DrawGameText("\"Para evitar que um governo tirânico use esta instalação para ataques,", tx + 24, ty + 225, 13, (Color){55, 65, 78, 255});
        DrawGameText("o despacho automático pode ser desligado localmente, preservando todas as provas.\"", tx + 24, ty + 245, 13, (Color){24, 128, 58, 255});

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, (Color){228, 246, 234, 255});
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, 1.5f, (Color){45, 155, 78, 255});
        DrawGameText("[REGISTRO HUMANITÁRIO ARQUIVADO NO DIÁRIO COM SUCESSO]", tx + 24, ty + 330, 15, (Color){22, 125, 52, 255});
    }
    else if (doc == 5)
    {
        /* --- DOC 5: MEMORANDO GOVERNAMENTAL (DESCOBERTA OPCIONAL 2) --- */
        int tx = (int)(contentBox.x + 35);
        int ty = (int)(contentBox.y + 25);
        DrawGameText("MINISTÉRIO DA SEGURANÇA PÚBLICA // DIRETRIZ SEC-402 (2008)", tx, ty, 18, (Color){185, 38, 32, 255});
        DrawLine(tx, ty + 26, tx + 710, ty + 26, (Color){215, 135, 130, 255});

        Rectangle gBox = {(float)tx, (float)(ty + 42), 710, 250};
        DrawRectangleRounded(gBox, 0.04f, 6, (Color){252, 242, 242, 255});
        DrawRectangleRoundedLinesEx(gBox, 0.04f, 6, 1.8f, (Color){195, 85, 80, 255});

        DrawGameText("ASSUNTO: Adaptação de Algoritmos Biométricos para Neutralização de Dissidentes", tx + 24, ty + 60, 15, (Color){175, 35, 30, 255});
        DrawGameText("1. Fica ordenada a integração dos algoritmos do Dr. Ramos aos drones de ataque.", tx + 24, ty + 95, 15, (Color){35, 45, 58, 255});
        DrawGameText("2. Qualquer indivíduo com similaridade superior a 75% em protestos será visado.", tx + 24, ty + 120, 15, (Color){35, 45, 58, 255});
        DrawGameText("3. Prioriza-se velocidade de resposta sobre a certeza da identificação.", tx + 24, ty + 145, 15, (Color){185, 75, 22, 255});

        DrawGameText("CLÁUSULA DE MANUTENÇÃO LOCAL:", tx + 24, ty + 185, 14, (Color){165, 95, 12, 255});
        DrawGameText("\"O controle de despacho automático de drones da filial pode ser suspenso", tx + 24, ty + 205, 13, (Color){55, 65, 78, 255});
        DrawGameText("no menu 'Configuração da Instalação' do computador para manutenção dos logs.\"", tx + 24, ty + 225, 13, (Color){28, 82, 142, 255});

        DrawRectangleRounded((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, (Color){254, 245, 222, 255});
        DrawRectangleRoundedLinesEx((Rectangle){(float)tx, (float)(ty + 310), 710, 60}, 0.08f, 6, 1.5f, (Color){205, 145, 35, 255});
        DrawGameText("[DESVIO GOVERNAMENTAL CONFIRMADO: PISTA DE OVERRIDE LIBERADA]", tx + 24, ty + 330, 15, (Color){165, 95, 12, 255});
    }

    /* Botoes de acao 3D na base do modal */
    if (doc >= 0 && doc <= 3)
    {
        bool isSel = game->puzzle2008Selected[doc];
        Rectangle btnToggle = {panel.x + 26, panel.y + panel.height - 54, 340, 42};
        DrawCorpButton2008(btnToggle, isSel ? "REMOVER DE EVIDÊNCIA" : "SELECIONAR COMO EVIDÊNCIA",
                           isSel ? (Color){195, 52, 44, 255} : (Color){38, 152, 82, 255}, true);
    }

    Rectangle btnClose = {panel.x + panel.width - 216, panel.y + panel.height - 54, 190, 42};
    DrawCorpButton2008(btnClose, "< FECHAR [ESC]", (Color){42, 102, 168, 255}, true);
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
                SetMessage(game, "Evidência removida do painel de contestação.");
            }
            else
            {
                if (game->puzzle2008SelectionCount < 2)
                {
                    game->puzzle2008Selected[doc] = true;
                    game->puzzle2008SelectionCount++;
                    SetMessage(game, "Evidência marcada para contestação no computador!");
                }
                else
                {
                    SetMessage(game, "Máximo de 2 evidências já selecionadas. Desmarque uma para trocar.");
                }
            }
        }
    }
}

/* Helpers de desenho da sala 2008 (prefixo R2008): volume, veios, parafusos, cabos */

static Color R2008Shade(Color c, float k)
{
    return (Color){(unsigned char)fminf(255, c.r * k), (unsigned char)fminf(255, c.g * k),
                   (unsigned char)fminf(255, c.b * k), c.a};
}

/* Veios de madeira horizontais discretos dentro de um retangulo */
static void R2008WoodGrain(Rectangle r, Color base, int seed)
{
    Color dark = Fade(R2008Shade(base, 0.80f), 0.55f);
    Color lite = Fade(R2008Shade(base, 1.10f), 0.55f);
    for (int i = 0; i < (int)(r.height / 7.0f); i++)
    {
        float y = r.y + 4 + i * 7 + (float)((seed * 13 + i * 5) % 3);
        float x0 = r.x + 3 + (float)((seed * 31 + i * 17) % 23);
        float x1 = r.x + r.width - 3 - (float)((seed * 7 + i * 29) % 31);
        if (y > r.y + r.height - 3 || x1 <= x0 + 6) continue;
        DrawLineEx((Vector2){x0, y}, (Vector2){x1, y + ((i % 2) ? 1.0f : 0.0f)}, 1.0f, (i % 3 == 0) ? lite : dark);
    }
}

/* Parafuso Phillips cromado */
static void R2008Screw(float x, float y)
{
    DrawCircle((int)x, (int)y, 2.2f, (Color){96, 104, 114, 255});
    DrawCircle((int)x, (int)y, 1.5f, (Color){196, 204, 212, 255});
    DrawPixel((int)x, (int)y, (Color){90, 98, 108, 255});
}

/* Cabo flexivel (curva de Bezier) com highlight fino */
static void R2008Cable(Vector2 a, Vector2 c1, Vector2 c2, Vector2 b, float thick, Color col)
{
    DrawSplineSegmentBezierCubic(a, c1, c2, b, thick, col);
    DrawSplineSegmentBezierCubic((Vector2){a.x, a.y - 1}, (Vector2){c1.x, c1.y - 1}, (Vector2){c2.x, c2.y - 1},
                                 (Vector2){b.x, b.y - 1}, 1.0f, Fade(R2008Shade(col, 1.9f), 0.55f));
}

/* Quadrilatero preenchido (pontos em sentido horario na tela; ordem ajustada para raylib) */
static void R2008Quad(Vector2 tl, Vector2 tr, Vector2 br, Vector2 bl, Color col)
{
    DrawTriangle(tl, bl, tr, col);
    DrawTriangle(tr, bl, br, col);
}

void DrawRoom2008(const GameState *game)
{
    float time = (float)GetTime();

    /* Hitboxes dos objetos interativos (sincronizados com UpdateRoom2008) */
    Rectangle flyerRec      = {102, 192, 148, 128};
    Rectangle cctvMonitor   = {736, 132, 196, 140};
    Rectangle monitorRec    = {490, 226, 250, 182};
    Rectangle doc0Rec       = {284, 418, 162, 76};
    Rectangle doc2Rec       = {794, 418, 154, 76};
    Rectangle fatherBookRec = {992, 410, 168, 68};
    Rectangle govMemoRec    = {992, 492, 168, 68};

    ClearBackground((Color){222, 218, 208, 255});

    /* ===================================================================== */
    /* 1. ARQUITETURA EM PERSPECTIVA 3D (AMBIENTE EMPRESARIAL CLARO 2008)    */
    /* ===================================================================== */

    /* Teto corporativo em forro mineral acustico claro */
    Color ceilCol = (Color){236, 239, 244, 255};
    DrawTriangle((Vector2){0, 70}, (Vector2){1090, 124}, (Vector2){1280, 70}, ceilCol);
    DrawTriangle((Vector2){0, 70}, (Vector2){190, 124}, (Vector2){1090, 124}, ceilCol);
    /* Grelha modular do forro de gesso/mineral */
    for (int cx = 230; cx <= 1050; cx += 164)
    {
        float topX = 640.0f + (cx - 640.0f) * 1.22f;
        DrawLineEx((Vector2){topX, 70}, (Vector2){(float)cx, 124}, 1.5f, (Color){206, 212, 220, 255});
    }
    DrawLine(95, 97, 1185, 97, (Color){210, 216, 224, 255});
    /* Fileiras transversais das placas do forro e sombra suave junto a parede */
    DrawLine(46, 83, 1234, 83, (Color){216, 221, 228, 255});
    DrawLine(146, 112, 1134, 112, (Color){212, 218, 226, 255});
    R2008Quad((Vector2){150, 113}, (Vector2){1130, 113}, (Vector2){1090, 124}, (Vector2){190, 124}, Fade((Color){150, 158, 170, 255}, 0.16f));
    /* Grelha de ar-condicionado central no forro (escritorio 2008) */
    R2008Quad((Vector2){586, 101}, (Vector2){694, 101}, (Vector2){700, 111}, (Vector2){580, 111}, (Color){214, 220, 228, 255});
    for (int ac = 0; ac < 6; ac++)
    {
        float ax = 590.0f + ac * 19.0f;
        DrawLineEx((Vector2){ax, 102}, (Vector2){ax - 2.0f + ac * 0.8f, 110}, 1.0f, (Color){168, 176, 186, 255});
    }
    DrawLine(586, 101, 694, 101, (Color){180, 188, 198, 255});

    /* Parede de fundo empresarial clara (marfim/alabastro corporativo uniforme) */
    Color backWallCol = (Color){234, 230, 220, 255};
    DrawTriangle((Vector2){190, 124}, (Vector2){1010, 515}, (Vector2){1090, 124}, backWallCol);
    DrawTriangle((Vector2){190, 124}, (Vector2){270, 515}, (Vector2){1010, 515}, backWallCol);
    /* Oclusao suave junto ao forro e luz quente das luminarias lavando a parede */
    DrawRectangleGradientV(197, 126, 886, 30, Fade((Color){120, 110, 96, 255}, 0.13f), Fade((Color){120, 110, 96, 255}, 0.0f));
    DrawEllipse(420, 150, 150, 40, Fade((Color){255, 250, 232, 255}, 0.30f));
    DrawEllipse(860, 150, 150, 40, Fade((Color){255, 250, 232, 255}, 0.30f));

    /* Paredes laterais esquerda e direita em perspectiva (tom areia suave uniforme) */
    Color sideWallCol = (Color){216, 210, 198, 255};
    DrawTriangle((Vector2){0, 70}, (Vector2){270, 515}, (Vector2){190, 124}, sideWallCol);
    DrawTriangle((Vector2){0, 70}, (Vector2){0, 535}, (Vector2){270, 515}, sideWallCol);
    DrawTriangle((Vector2){1280, 70}, (Vector2){1090, 124}, (Vector2){1010, 515}, sideWallCol);
    DrawTriangle((Vector2){1280, 70}, (Vector2){1010, 515}, (Vector2){1280, 535}, sideWallCol);

    /* Lambris / revestimento inferior corporativo (meia-parede em tom areia/taupe claro) */
    Color wainscotBack = (Color){212, 205, 192, 255};
    DrawTriangle((Vector2){243, 385}, (Vector2){1010, 515}, (Vector2){1037, 385}, wainscotBack);
    DrawTriangle((Vector2){243, 385}, (Vector2){270, 515}, (Vector2){1010, 515}, wainscotBack);

    Color wainscotSide = (Color){198, 191, 178, 255};
    DrawTriangle((Vector2){0, 402}, (Vector2){270, 515}, (Vector2){243, 385}, wainscotSide);
    DrawTriangle((Vector2){0, 402}, (Vector2){0, 535}, (Vector2){270, 515}, wainscotSide);
    DrawTriangle((Vector2){1280, 402}, (Vector2){1037, 385}, (Vector2){1010, 515}, wainscotSide);
    DrawTriangle((Vector2){1280, 402}, (Vector2){1010, 515}, (Vector2){1280, 535}, wainscotSide);

    /* Faixa divisoria de madeira clara e aluminio na meia-parede */
    DrawLineEx((Vector2){243, 384}, (Vector2){1037, 384}, 4.0f, (Color){168, 152, 130, 255});
    DrawLineEx((Vector2){243, 381}, (Vector2){1037, 381}, 2.0f, (Color){196, 202, 208, 255});
    DrawLineEx((Vector2){0, 402}, (Vector2){243, 384}, 3.5f, (Color){156, 140, 118, 255});
    DrawLineEx((Vector2){1037, 384}, (Vector2){1280, 402}, 3.5f, (Color){156, 140, 118, 255});

    /* Juntas arquitetonicas dos paineis corporativos da parede de fundo */
    DrawLine(206, 210, 1074, 210, Fade((Color){170, 164, 152, 255}, 0.30f));
    DrawLine(224, 298, 1056, 298, Fade((Color){170, 164, 152, 255}, 0.30f));
    DrawLine(300, 124, 330, 515, Fade((Color){165, 158, 146, 255}, 0.28f));
    DrawLine(716, 124, 704, 515, Fade((Color){165, 158, 146, 255}, 0.28f));
    DrawLine(948, 124, 916, 515, Fade((Color){165, 158, 146, 255}, 0.28f));

    /* Piso em porcelanato polido claro uniforme com linhas de fuga em perspectiva */
    Color floorCol = (Color){194, 188, 177, 255};
    DrawTriangle((Vector2){0, 650}, (Vector2){1010, 515}, (Vector2){270, 515}, floorCol);
    DrawTriangle((Vector2){0, 650}, (Vector2){1280, 650}, (Vector2){1010, 515}, floorCol);
    DrawTriangle((Vector2){0, 535}, (Vector2){0, 650}, (Vector2){270, 515}, floorCol);
    DrawTriangle((Vector2){1280, 535}, (Vector2){1010, 515}, (Vector2){1280, 650}, floorCol);

    for (int x = -80; x <= SCREEN_WIDTH + 80; x += 145)
    {
        DrawLine(640, 505, x, 650, Fade((Color){154, 147, 136, 255}, 0.42f));
    }
    DrawLine(0, 548, 1280, 548, Fade((Color){154, 147, 136, 255}, 0.38f));
    DrawLine(0, 584, 1280, 584, Fade((Color){154, 147, 136, 255}, 0.36f));
    DrawLine(0, 622, 1280, 622, Fade((Color){154, 147, 136, 255}, 0.34f));
    /* Porcelanato polido: reflexos das luminarias e da janela no piso */
    DrawEllipse(140, 592, 150, 6, Fade((Color){252, 250, 244, 255}, 0.12f));
    DrawEllipse(140, 592, 70, 2, Fade((Color){255, 255, 255, 255}, 0.16f));
    DrawEllipse(1150, 622, 130, 5, Fade((Color){252, 250, 244, 255}, 0.10f));
    DrawEllipse(1150, 622, 60, 2, Fade((Color){255, 255, 255, 255}, 0.14f));
    /* Oclusao de contato junto ao rodape */
    DrawRectangleGradientV(270, 516, 740, 12, Fade((Color){90, 80, 66, 255}, 0.20f), Fade((Color){90, 80, 66, 255}, 0.0f));

    /* Molduras de acabamento do teto, quinas e rodapes */
    DrawLineEx((Vector2){0, 70}, (Vector2){190, 124}, 4.0f, (Color){168, 175, 184, 255});
    DrawLineEx((Vector2){1280, 70}, (Vector2){1090, 124}, 4.0f, (Color){168, 175, 184, 255});
    DrawLineEx((Vector2){190, 124}, (Vector2){1090, 124}, 4.0f, (Color){158, 165, 174, 255});
    DrawLineEx((Vector2){190, 124}, (Vector2){270, 515}, 5.5f, (Color){172, 166, 155, 255});
    DrawLineEx((Vector2){1090, 124}, (Vector2){1010, 515}, 5.5f, (Color){172, 166, 155, 255});
    /* Rodape executivo */
    DrawLineEx((Vector2){270, 513}, (Vector2){1010, 513}, 7.0f, (Color){138, 126, 110, 255});
    DrawLineEx((Vector2){0, 533}, (Vector2){270, 513}, 6.0f, (Color){128, 116, 100, 255});
    DrawLineEx((Vector2){1010, 513}, (Vector2){1280, 533}, 6.0f, (Color){128, 116, 100, 255});

    /* Luminarias corporativas embutidas no forro com aletas reflexivas e cone de luz */
    int lightXs[2] = {330, 770};
    for (int li = 0; li < 2; li++)
    {
        int lx = lightXs[li];
        DrawTriangle((Vector2){(float)(lx + 10), 104}, (Vector2){(float)(lx - 60), 470}, (Vector2){(float)(lx + 240), 470},
                     Fade((Color){255, 252, 235, 255}, 0.10f));
        /* Nucleo mais intenso do cone de luz fluorescente */
        DrawTriangle((Vector2){(float)(lx + 40), 104}, (Vector2){(float)(lx + 10), 470}, (Vector2){(float)(lx + 170), 470},
                     Fade((Color){255, 253, 240, 255}, 0.07f));
        /* Halo da luz no forro */
        DrawEllipse(lx + 90, 90, 118, 20, Fade((Color){255, 255, 248, 255}, 0.55f));
        DrawRectangleRounded((Rectangle){(float)lx, 78, 180, 24}, 0.18f, 6, (Color){172, 180, 190, 255});
        DrawLine(lx + 4, 79, lx + 176, 79, (Color){214, 220, 228, 255});
        DrawLine(lx + 4, 101, lx + 176, 101, (Color){128, 136, 146, 255});
        DrawRectangleRoundedLinesEx((Rectangle){(float)lx, 78, 180, 24}, 0.18f, 6, 1.5f, (Color){118, 126, 138, 255});
        DrawRectangleRounded((Rectangle){(float)(lx + 10), 83, 160, 14}, 0.18f, 6, (Color){248, 252, 255, 255});
        DrawRectangle(lx + 14, 86, 152, 3, (Color){255, 255, 255, 255});
        for (int lv = 24; lv < 160; lv += 22)
        {
            DrawLine(lx + lv, 83, lx + lv, 97, (Color){195, 205, 215, 255});
            DrawLine(lx + lv + 1, 83, lx + lv + 1, 97, (Color){228, 234, 240, 255});
        }
        R2008Screw((float)lx + 5, 90);
        R2008Screw((float)lx + 175, 90);
        /* Particulas de poeira flutuando devagar dentro do cone */
        for (int pd = 0; pd < 7; pd++)
        {
            float pt = time * (0.10f + pd * 0.013f) + pd * 0.37f + li * 0.5f;
            float py = 130.0f + fmodf(pt * 90.0f + pd * 41.0f, 300.0f);
            float px = lx + 40.0f + (py - 104.0f) * (pd % 2 ? -0.05f : 0.18f) + sinf(pt * 3.0f + pd) * 10.0f + pd * 16.0f;
            DrawCircle((int)px, (int)py, 1.0f, Fade((Color){255, 250, 225, 255}, 0.30f + 0.20f * sinf(time * 1.5f + pd)));
        }
    }

    /* ===================================================================== */
    /* 2. PAREDE LATERAL ESQUERDA: PORTA CORPORATIVA, LEITOR E PLANTA        */
    /* ===================================================================== */
    /* Porta executiva em perspectiva na parede lateral esquerda */
    Vector2 dTL = {12, 152};
    Vector2 dTR = {66, 168};
    Vector2 dBR = {66, 526};
    Vector2 dBL = {12, 534};
    /* Batente (marco) da porta: faixa clara de aluminio em volta da folha */
    R2008Quad((Vector2){6, 144}, (Vector2){72, 162}, (Vector2){72, 530}, (Vector2){6, 540}, (Color){186, 190, 194, 255});
    DrawLineEx((Vector2){6, 144}, (Vector2){72, 162}, 1.5f, (Color){120, 126, 134, 255});
    DrawLineEx((Vector2){72, 162}, (Vector2){72, 530}, 1.5f, (Color){120, 126, 134, 255});
    DrawTriangle(dTL, dBL, dTR, (Color){164, 126, 90, 255});
    DrawTriangle(dTR, dBL, dBR, (Color){164, 126, 90, 255});
    /* Veios de madeira acompanhando a perspectiva da folha da porta */
    for (int dg = 0; dg < 9; dg++)
    {
        float gx = 16.0f + dg * 5.6f;
        float gTop = 153.0f + (gx - 12.0f) * (16.0f / 54.0f);
        float gBot = 534.0f - (gx - 12.0f) * (8.0f / 54.0f);
        DrawLineEx((Vector2){gx, gTop + 4}, (Vector2){gx + ((dg % 2) ? 1.0f : -1.0f), gBot - 4}, 1.0f,
                   Fade((dg % 3 == 0) ? (Color){186, 148, 108, 255} : (Color){140, 104, 72, 255}, 0.45f));
    }
    /* Luz lateral da porta (face mais iluminada perto do batente) e sombra embaixo */
    DrawTriangle(dTL, dBL, (Vector2){20, 155}, Fade((Color){214, 178, 136, 255}, 0.55f));
    DrawTriangle((Vector2){20, 155}, dBL, (Vector2){20, 533}, Fade((Color){214, 178, 136, 255}, 0.55f));
    DrawLineEx(dTL, dTR, 4.0f, (Color){118, 88, 60, 255});
    DrawLineEx(dTR, dBR, 4.0f, (Color){118, 88, 60, 255});
    DrawLineEx(dTL, dBL, 4.0f, (Color){118, 88, 60, 255});
    DrawLineEx(dBL, dBR, 2.0f, (Color){96, 70, 46, 255});
    /* Dobradicas cromadas */
    DrawRectangle(11, 196, 4, 18, (Color){196, 202, 208, 255});
    DrawRectangle(11, 440, 4, 18, (Color){196, 202, 208, 255});
    DrawLine(14, 196, 14, 214, (Color){130, 136, 144, 255});
    DrawLine(14, 440, 14, 458, (Color){130, 136, 144, 255});
    /* Faixas de vidro jateado na porta corporativa */
    for (int pg = 0; pg < 3; pg++)
    {
        int py = 195 + pg * 72;
        DrawRectangle(24, py, 28, 48, (Color){196, 216, 228, 255});
        DrawRectangleGradientV(24, py, 28, 48, Fade((Color){236, 244, 250, 255}, 0.70f), Fade((Color){170, 194, 210, 255}, 0.40f));
        DrawLineEx((Vector2){29, (float)(py + 40)}, (Vector2){43, (float)(py + 6)}, 2.0f, Fade(RAYWHITE, 0.50f));
        DrawLine(26, py + 1, 50, py + 1, Fade(RAYWHITE, 0.60f));
        DrawRectangleLines(24, py, 28, 48, (Color){132, 100, 72, 255});
        DrawRectangleLines(23, py - 1, 30, 50, Fade((Color){96, 70, 46, 255}, 0.55f));
    }
    /* Chapa de protecao inferior em aco escovado */
    R2008Quad((Vector2){16, 500}, (Vector2){63, 504}, (Vector2){63, 524}, (Vector2){16, 530}, (Color){176, 182, 188, 255});
    DrawLine(16, 501, 63, 505, (Color){214, 220, 226, 255});
    DrawLine(17, 514, 62, 513, Fade((Color){140, 146, 154, 255}, 0.7f));
    /* Macaneta cromada com sombra e brilho */
    DrawRectangle(45, 363, 12, 4, Fade(BLACK, 0.20f));
    DrawCircle(57, 364, 4, Fade(BLACK, 0.20f));
    DrawCircle(56, 362, 4, (Color){210, 216, 222, 255});
    DrawCircleLines(56, 362, 4, (Color){120, 126, 134, 255});
    DrawRectangle(44, 360, 12, 4, (Color){185, 192, 200, 255});
    DrawLine(44, 360, 55, 360, (Color){236, 240, 244, 255});
    DrawPixel(55, 360, RAYWHITE);

    /* Leitor de cracha eletronico ao lado da porta */
    Rectangle badgeReader = {70, 344, 16, 28};
    DrawRectangleRounded((Rectangle){72, 347, 16, 28}, 0.2f, 4, Fade(BLACK, 0.18f));
    DrawRectangleRounded(badgeReader, 0.2f, 4, (Color){48, 56, 66, 255});
    DrawLine(72, 345, 84, 345, (Color){104, 114, 126, 255});
    DrawRectangleLinesEx(badgeReader, 1.0f, (Color){95, 105, 118, 255});
    DrawCircle(78, 352, 3.6f, Fade((Color){65, 230, 115, 255}, 0.25f + 0.15f * sinf(time * 2.0f)));
    DrawCircle(78, 352, 2.2f, (Color){65, 230, 115, 255});
    DrawPixel(77, 351, (Color){210, 255, 220, 255});
    DrawRectangle(74, 358, 8, 9, (Color){82, 92, 104, 255});
    DrawLine(75, 360, 80, 360, (Color){60, 68, 78, 255});
    DrawLine(75, 363, 80, 363, (Color){60, 68, 78, 255});

    /* Vaso corporativo com planta ornamental no canto esquerdo */
    DrawEllipse(216, 520, 26, 7, Fade(BLACK, 0.20f));
    DrawEllipse(222, 521, 30, 5, Fade(BLACK, 0.10f));
    DrawTriangle((Vector2){196, 470}, (Vector2){203, 518}, (Vector2){236, 470}, (Color){228, 223, 214, 255});
    DrawTriangle((Vector2){236, 470}, (Vector2){203, 518}, (Vector2){229, 518}, (Color){228, 223, 214, 255});
    /* Volume do vaso: lado esquerdo iluminado, lado direito em sombra */
    DrawTriangle((Vector2){198, 471}, (Vector2){204, 517}, (Vector2){206, 471}, (Color){244, 241, 234, 255});
    DrawTriangle((Vector2){206, 471}, (Vector2){204, 517}, (Vector2){209, 517}, (Color){244, 241, 234, 255});
    DrawTriangle((Vector2){224, 471}, (Vector2){222, 517}, (Vector2){235, 471}, (Color){204, 198, 188, 255});
    DrawTriangle((Vector2){235, 471}, (Vector2){222, 517}, (Vector2){229, 517}, (Color){204, 198, 188, 255});
    DrawLineEx((Vector2){196, 471}, (Vector2){203, 518}, 1.0f, (Color){160, 152, 140, 255});
    DrawLineEx((Vector2){236, 471}, (Vector2){229, 518}, 1.0f, (Color){150, 142, 130, 255});
    DrawLine(203, 518, 229, 518, (Color){150, 142, 130, 255});
    /* Faixa decorativa em baixo-relevo */
    DrawLine(199, 489, 233, 489, Fade((Color){168, 160, 148, 255}, 0.8f));
    DrawLine(199, 490, 233, 490, Fade(RAYWHITE, 0.6f));
    DrawRectangleRounded((Rectangle){193, 464, 46, 8}, 0.3f, 4, (Color){238, 233, 224, 255});
    DrawRectangleLinesEx((Rectangle){193, 464, 46, 8}, 1.0f, (Color){168, 160, 148, 255});
    /* Folhagem da planta (base escura atras e folhas com dois tons e nervura) */
    DrawEllipse(216, 452, 22, 14, (Color){36, 86, 48, 255});
    DrawEllipse(216, 442, 10, 24, (Color){62, 128, 74, 255});
    DrawEllipse(203, 448, 14, 18, (Color){48, 108, 60, 255});
    DrawEllipse(229, 448, 14, 18, (Color){74, 142, 84, 255});
    DrawEllipse(209, 432, 8, 20, (Color){82, 152, 90, 255});
    DrawEllipse(224, 434, 9, 19, (Color){54, 118, 66, 255});
    /* Luz de cima nas folhas e nervuras centrais */
    DrawEllipse(207, 425, 4, 10, Fade((Color){150, 206, 140, 255}, 0.55f));
    DrawEllipse(214, 428, 4, 12, Fade((Color){130, 190, 124, 255}, 0.45f));
    DrawLineEx((Vector2){209, 414}, (Vector2){209, 450}, 1.0f, Fade((Color){30, 80, 40, 255}, 0.6f));
    DrawLineEx((Vector2){216, 420}, (Vector2){216, 462}, 1.0f, Fade((Color){30, 80, 40, 255}, 0.55f));
    DrawLineEx((Vector2){203, 434}, (Vector2){203, 462}, 1.0f, Fade((Color){26, 72, 36, 255}, 0.5f));
    DrawEllipseLines(209, 432, 8, 20, Fade((Color){30, 74, 40, 255}, 0.55f));
    DrawEllipseLines(216, 442, 10, 24, Fade((Color){30, 74, 40, 255}, 0.45f));

    /* Colecionavel oculto (F): celular de flip esquecido dentro do vaso, entre
       as folhas, com o LED de SMS piscando fraco. Some depois de coletado. */
    if (!game->hiddenFound[HIDDEN_2008])
    {
        Rectangle flipPhoneRec = {221, 442, 22, 30}; /* sincronizado com UpdateRoom2008 */
        /* Corpo fechado do flip, inclinado e enterrado na terra do vaso */
        DrawRectanglePro((Rectangle){231, 458, 11, 20}, (Vector2){5.5f, 10.0f}, 14.0f, (Color){58, 62, 70, 255});
        DrawRectanglePro((Rectangle){231, 452, 11, 3}, (Vector2){5.5f, 1.5f}, 14.0f, (Color){120, 126, 136, 255});
        /* Visor externo pequeno e antena curta */
        DrawRectanglePro((Rectangle){230, 455, 6, 5}, (Vector2){3.0f, 2.5f}, 14.0f, (Color){30, 40, 52, 255});
        DrawLineEx((Vector2){236, 449}, (Vector2){238, 444}, 2.0f, (Color){44, 48, 54, 255});
        /* LED verde de SMS nao lido: pulso lento e fraco */
        float smsPulse = 0.5f + 0.5f * sinf(time * 2.2f);
        DrawCircle(233, 461, 1.6f, Fade((Color){70, 235, 110, 255}, 0.18f + 0.42f * smsPulse));
        /* Uma folha caida por cima encobre parte do aparelho */
        DrawEllipse(225, 458, 6, 10, (Color){48, 108, 60, 255});
        DrawHiddenGlint(flipPhoneRec, 1.3f);
    }
    /* Borda do vaso redesenhada para que o celular pareca estar dentro dele */
    DrawRectangleRounded((Rectangle){193, 464, 46, 8}, 0.3f, 4, (Color){238, 233, 224, 255});
    DrawRectangleLinesEx((Rectangle){193, 464, 46, 8}, 1.0f, (Color){168, 160, 148, 255});

    /* ===================================================================== */
    /* 3. JANELA PANORAMICA EMPRESARIAL (SKYLINE DIURNO/ENTARDECER 2008)     */
    /* ===================================================================== */
    Rectangle winOuter = {318, 132, 390, 150};
    Rectangle winInner = {330, 144, 366, 126};

    /* Sombra e moldura de aluminio anodizado da janela */
    DrawRectangle((int)winOuter.x + 4, (int)winOuter.y + 4, (int)winOuter.width, (int)winOuter.height, Fade(BLACK, 0.16f));
    DrawRectangle((int)winOuter.x + 7, (int)winOuter.y + 8, (int)winOuter.width, (int)winOuter.height, Fade(BLACK, 0.06f));
    DrawRectangleRounded(winOuter, 0.04f, 6, (Color){168, 176, 184, 255});
    /* Volume da moldura: topo/esquerda claros, base/direita escuros */
    DrawRectangle((int)winOuter.x + 3, (int)winOuter.y + 2, (int)winOuter.width - 6, 3, (Color){206, 212, 220, 255});
    DrawRectangle((int)winOuter.x + 2, (int)winOuter.y + 3, 3, (int)winOuter.height - 6, (Color){196, 204, 212, 255});
    DrawRectangle((int)(winOuter.x + winOuter.width - 5), (int)winOuter.y + 3, 3, (int)winOuter.height - 6, (Color){140, 148, 158, 255});
    /* Rebaixo interno escuro (profundidade do vao) */
    DrawRectangleLinesEx((Rectangle){winInner.x - 2, winInner.y - 2, winInner.width + 4, winInner.height + 4}, 2.0f, (Color){112, 120, 130, 255});
    DrawRectangleRoundedLinesEx(winOuter, 0.04f, 6, 2.5f, (Color){122, 130, 140, 255});
    /* Peitoril com highlight superior e sombra projetada na parede */
    DrawRectangle((int)winOuter.x - 2, (int)(winOuter.y + winOuter.height + 4), (int)winOuter.width + 10, 5, Fade(BLACK, 0.12f));
    DrawRectangle((int)winOuter.x - 4, (int)(winOuter.y + winOuter.height - 4), (int)winOuter.width + 8, 8, (Color){185, 192, 200, 255});
    DrawLine((int)winOuter.x - 4, (int)(winOuter.y + winOuter.height - 4), (int)(winOuter.x + winOuter.width + 4), (int)(winOuter.y + winOuter.height - 4), (Color){226, 232, 238, 255});
    DrawLine((int)winOuter.x - 4, (int)(winOuter.y + winOuter.height + 3), (int)(winOuter.x + winOuter.width + 4), (int)(winOuter.y + winOuter.height + 3), (Color){128, 136, 146, 255});

    BeginScissorMode((int)winInner.x, (int)winInner.y, (int)winInner.width, (int)winInner.height);

    /* Ceu empresarial claro (gradiente azul celeste para horizonte claro) */
    DrawRectangleGradientV((int)winInner.x, (int)winInner.y, (int)winInner.width, (int)winInner.height,
                           (Color){122, 172, 218, 255}, (Color){218, 232, 244, 255});
    /* Brilho do sol da tarde no canto do ceu */
    DrawCircleGradient((Vector2){664, 176}, 70, Fade((Color){255, 246, 214, 255}, 0.55f), Fade((Color){255, 246, 214, 255}, 0.0f));

    /* Nuvens suaves se movendo lentamente */
    for (int cl = 0; cl < 4; cl++)
    {
        float cx = winInner.x + fmodf(cl * 105.0f + time * 4.5f, winInner.width + 70.0f) - 35.0f;
        float cy = winInner.y + 26.0f + (cl % 2) * 16.0f;
        DrawEllipse((int)cx, (int)cy, 34, 9, Fade(RAYWHITE, 0.65f));
        DrawEllipse((int)(cx + 14), (int)(cy - 4), 22, 8, Fade(RAYWHITE, 0.70f));
    }

    /* Edificios corporativos espelhados ao fundo (Skyline 2008) */
    int bldX[7] = {336, 386, 442, 496, 558, 612, 658};
    int bldW[7] = {44,  50,  48,  56,  48,  42,  40};
    int bldY[7] = {190, 168, 198, 158, 176, 192, 172};
    /* Segundo plano: predios distantes em neblina azulada */
    for (int fb = 0; fb < 9; fb++)
    {
        int fx = 330 + fb * 44 - (fb % 3) * 9;
        int fy = 204 + (fb * 37) % 34;
        DrawRectangle(fx, fy, 34 + (fb % 2) * 10, 275 - fy, (Color){168, 192, 214, 255});
        DrawLine(fx, fy, fx + 34 + (fb % 2) * 10, fy, (Color){196, 214, 230, 255});
    }
    for (int b = 0; b < 7; b++)
    {
        Color bCol = (b % 2 == 0) ? (Color){118, 144, 172, 255} : (Color){96, 124, 154, 255};
        DrawRectangle(bldX[b], bldY[b], bldW[b], 275 - bldY[b], bCol);
        /* Lateral iluminada pelo sol nos predios */
        DrawRectangle(bldX[b], bldY[b], 8, 275 - bldY[b], Fade(RAYWHITE, 0.24f));
        /* Face direita em sombra e cobertura iluminada */
        DrawRectangle(bldX[b] + bldW[b] - 7, bldY[b], 7, 275 - bldY[b], Fade((Color){40, 60, 90, 255}, 0.30f));
        DrawRectangle(bldX[b], bldY[b], bldW[b], 3, Fade(RAYWHITE, 0.45f));
        DrawRectangleLines(bldX[b], bldY[b], bldW[b], 275 - bldY[b], (Color){78, 102, 130, 255});
        /* Antena no topo de alguns predios */
        if (b == 1 || b == 3 || b == 4)
        {
            int ax = bldX[b] + bldW[b] / 2;
            DrawLine(ax, bldY[b] - 14, ax, bldY[b], (Color){90, 110, 130, 255});
            DrawCircle(ax, bldY[b] - 14, 2.0f, Fade(RED_COLOR, 0.5f + 0.5f * sinf(time * 3.0f + b)));
        }
        /* Grade de janelas espelhadas dos edificios */
        for (int wy = bldY[b] + 8; wy < 268; wy += 10)
        {
            for (int wx = bldX[b] + 6; wx < bldX[b] + bldW[b] - 6; wx += 9)
            {
                float wa = ((wx * 7 + wy * 3 + b) % 5 == 0) ? 0.85f : 0.55f;
                DrawRectangle(wx, wy, 6, 6, Fade((Color){210, 232, 250, 255}, wa));
                DrawPixel(wx, wy, Fade(RAYWHITE, wa));
            }
        }
    }

    /* Pequeno drone de vigilancia patrulhando ao longe no ceu */
    float droneX = winInner.x + 65.0f + sinf(time * 0.7f) * 45.0f;
    float droneY = winInner.y + 34.0f + cosf(time * 1.1f) * 6.0f;
    DrawLineEx((Vector2){droneX - 9, droneY}, (Vector2){droneX + 9, droneY}, 2.0f, (Color){55, 65, 78, 255});
    DrawCircle((int)droneX, (int)droneY, 3.0f, (Color){42, 50, 60, 255});
    DrawCircle((int)droneX, (int)(droneY + 1), 1.5f, RED_COLOR);

    /* Persianas horizontais claras recolhidas na parte superior */
    for (int sl = 0; sl < 5; sl++)
    {
        int sy = (int)winInner.y + sl * 5;
        DrawRectangle((int)winInner.x, sy, (int)winInner.width, 4, (Color){238, 236, 228, 255});
        DrawLine((int)winInner.x, sy, (int)(winInner.x + winInner.width), sy, (Color){250, 249, 244, 255});
        DrawLine((int)winInner.x, sy + 4, (int)(winInner.x + winInner.width), sy + 4, (Color){188, 184, 174, 255});
    }
    /* Cordao da persiana na direita */
    DrawLine((int)(winInner.x + winInner.width - 14), (int)winInner.y, (int)(winInner.x + winInner.width - 14), (int)winInner.y + 62, (Color){170, 166, 156, 255});
    DrawCircle((int)(winInner.x + winInner.width - 14), (int)winInner.y + 64, 3.0f, (Color){220, 216, 206, 255});

    /* Reflexos diagonais de vidro */
    DrawLineEx((Vector2){355, 146}, (Vector2){415, 268}, 10.0f, Fade(RAYWHITE, 0.18f));
    DrawLineEx((Vector2){375, 146}, (Vector2){435, 268}, 4.0f, Fade(RAYWHITE, 0.14f));
    DrawLineEx((Vector2){555, 146}, (Vector2){615, 268}, 8.0f, Fade(RAYWHITE, 0.15f));
    /* Sombra interna no topo do vidro (vao da janela) */
    DrawRectangleGradientV((int)winInner.x, (int)winInner.y + 25, (int)winInner.width, 8, Fade((Color){60, 70, 84, 255}, 0.22f), Fade((Color){60, 70, 84, 255}, 0.0f));

    EndScissorMode();

    /* Esquadrias metalicas divisorias da janela */
    DrawLineEx((Vector2){452, 144}, (Vector2){452, 270}, 3.5f, (Color){152, 160, 170, 255});
    DrawLineEx((Vector2){574, 144}, (Vector2){574, 270}, 3.5f, (Color){152, 160, 170, 255});
    DrawLineEx((Vector2){330, 220}, (Vector2){696, 220}, 3.0f, (Color){152, 160, 170, 255});
    DrawLine(451, 144, 451, 270, (Color){196, 204, 212, 255});
    DrawLine(573, 144, 573, 270, (Color){196, 204, 212, 255});
    DrawLine(330, 219, 696, 219, (Color){200, 208, 216, 255});
    DrawLine(330, 222, 696, 222, (Color){120, 128, 138, 255});

    /* ===================================================================== */
    /* 4. QUADRO DE CORTICA E AVISOS + PANFLETO DA REVOLTA (DOC 2)           */
    /* ===================================================================== */
    Rectangle corkBoard = {80, 132, 192, 202};
    /* Sombra projetada na parede clara */
    DrawRectangle((int)corkBoard.x + 5, (int)corkBoard.y + 5, (int)corkBoard.width, (int)corkBoard.height, Fade(BLACK, 0.18f));
    /* Moldura de madeira clara e aluminio */
    DrawRectangleRounded(corkBoard, 0.04f, 6, (Color){184, 142, 98, 255});
    /* Volume da moldura: topo/esquerda iluminados, base/direita em sombra */
    DrawRectangle((int)corkBoard.x + 4, (int)corkBoard.y + 2, (int)corkBoard.width - 8, 3, (Color){214, 176, 132, 255});
    DrawRectangle((int)corkBoard.x + 2, (int)corkBoard.y + 4, 3, (int)corkBoard.height - 8, (Color){204, 164, 120, 255});
    DrawRectangle((int)corkBoard.x + 4, (int)(corkBoard.y + corkBoard.height - 5), (int)corkBoard.width - 8, 3, (Color){150, 112, 74, 255});
    DrawRectangle((int)(corkBoard.x + corkBoard.width - 5), (int)corkBoard.y + 4, 3, (int)corkBoard.height - 8, (Color){156, 118, 80, 255});
    DrawRectangleRoundedLinesEx(corkBoard, 0.04f, 6, 4.0f, (Color){138, 102, 68, 255});
    DrawRectangleRoundedLinesEx((Rectangle){corkBoard.x - 1, corkBoard.y - 1, corkBoard.width + 2, corkBoard.height + 2}, 0.04f, 6, 1.0f, (Color){102, 72, 44, 255});
    Rectangle corkInner = {corkBoard.x + 8, corkBoard.y + 8, corkBoard.width - 16, corkBoard.height - 16};
    DrawRectangleRec(corkInner, (Color){174, 130, 86, 255});
    /* Sombra interna da moldura sobre a cortica */
    DrawRectangleGradientV((int)corkInner.x, (int)corkInner.y, (int)corkInner.width, 6, Fade((Color){90, 60, 34, 255}, 0.45f), Fade((Color){90, 60, 34, 255}, 0.0f));
    DrawRectangleGradientH((int)corkInner.x, (int)corkInner.y, 5, (int)corkInner.height, Fade((Color){90, 60, 34, 255}, 0.35f), Fade((Color){90, 60, 34, 255}, 0.0f));
    DrawRectangleLinesEx(corkInner, 1.5f, (Color){122, 88, 54, 255});

    /* Textura granulada da cortica (tres tons) */
    for (int cp = 0; cp < 90; cp++)
    {
        int cpx = (int)corkInner.x + 4 + (cp * 37 + cp / 7) % ((int)corkInner.width - 8);
        int cpy = (int)corkInner.y + 4 + (cp * 23 + cp * cp) % ((int)corkInner.height - 8);
        Color cc = (cp % 3 == 0) ? (Color){148, 106, 66, 255} : (cp % 3 == 1) ? (Color){196, 152, 108, 255} : (Color){160, 116, 74, 255};
        if (cp % 4 == 0) DrawCircle(cpx, cpy, 1.2f, cc);
        else DrawRectangle(cpx, cpy, 2, 1, cc);
    }
    /* Furos de tachinhas antigas */
    DrawPixel(160, 206, (Color){96, 66, 40, 255});
    DrawPixel(102, 322, (Color){96, 66, 40, 255});
    DrawPixel(258, 230, (Color){96, 66, 40, 255});

    /* Cantoneiras metalicas do quadro */
    DrawRectangle((int)corkBoard.x, (int)corkBoard.y, 12, 12, (Color){180, 188, 196, 255});
    DrawRectangle((int)(corkBoard.x + corkBoard.width - 12), (int)corkBoard.y, 12, 12, (Color){180, 188, 196, 255});
    DrawRectangle((int)corkBoard.x, (int)(corkBoard.y + corkBoard.height - 12), 12, 12, (Color){180, 188, 196, 255});
    DrawRectangle((int)(corkBoard.x + corkBoard.width - 12), (int)(corkBoard.y + corkBoard.height - 12), 12, 12, (Color){180, 188, 196, 255});
    for (int cn = 0; cn < 4; cn++)
    {
        float cnx = (cn % 2 == 0) ? corkBoard.x : corkBoard.x + corkBoard.width - 12;
        float cny = (cn < 2) ? corkBoard.y : corkBoard.y + corkBoard.height - 12;
        DrawLine((int)cnx, (int)cny, (int)cnx + 12, (int)cny, (Color){222, 228, 234, 255});
        DrawRectangleLines((int)cnx, (int)cny, 12, 12, (Color){120, 128, 138, 255});
        R2008Screw(cnx + 6, cny + 6);
    }

    /* Post-it amarelo (esquerda superior) com tachinha azul */
    DrawRectangle(96, 148, 48, 38, Fade(BLACK, 0.16f));
    DrawRectangle(94, 146, 48, 38, (Color){252, 238, 122, 255});
    DrawRectangle(94, 146, 48, 6, (Color){236, 218, 92, 255});
    DrawLine(100, 160, 134, 160, (Color){165, 150, 70, 255});
    DrawLine(100, 168, 128, 168, (Color){165, 150, 70, 255});
    DrawTriangle((Vector2){142, 176}, (Vector2){132, 184}, (Vector2){142, 184}, (Color){226, 210, 96, 255});
    DrawLine(132, 184, 142, 176, (Color){196, 180, 80, 255});
    DrawCircle(119, 152, 3.0f, Fade(BLACK, 0.30f));
    DrawCircle(118, 150, 3.0f, (Color){45, 125, 210, 255});
    DrawPixel(117, 149, (Color){190, 220, 255, 255});

    /* Post-it azul (direita superior) com tachinha verde */
    DrawRectangle(212, 148, 48, 38, Fade(BLACK, 0.16f));
    DrawRectangle(210, 146, 48, 38, (Color){156, 224, 248, 255});
    DrawRectangle(210, 146, 48, 6, (Color){126, 198, 228, 255});
    DrawLine(216, 160, 250, 160, (Color){75, 130, 155, 255});
    DrawLine(216, 168, 242, 168, (Color){75, 130, 155, 255});
    DrawTriangle((Vector2){258, 176}, (Vector2){248, 184}, (Vector2){258, 184}, (Color){126, 196, 222, 255});
    DrawLine(248, 184, 258, 176, (Color){100, 170, 196, 255});
    DrawCircle(235, 152, 3.0f, Fade(BLACK, 0.30f));
    DrawCircle(234, 150, 3.0f, (Color){45, 175, 95, 255});
    DrawPixel(233, 149, (Color){200, 255, 220, 255});

    /* Fio vermelho de investigacao ligando post-it ao panfleto */
    DrawLineEx((Vector2){119, 152}, (Vector2){177, 200}, 1.2f, Fade(BLACK, 0.15f));
    DrawLineEx((Vector2){118, 150}, (Vector2){176, 198}, 1.2f, Fade((Color){195, 40, 35, 255}, 0.75f));

    /* Panfleto apreendido central detalhado (flyerRec) */
    DrawRectangle((int)flyerRec.x + 4, (int)flyerRec.y + 4, (int)flyerRec.width, (int)flyerRec.height, Fade(BLACK, 0.25f));
    DrawRectangleRounded(flyerRec, 0.03f, 4, (Color){248, 242, 226, 255});
    DrawRectangleGradientV((int)flyerRec.x + 2, (int)(flyerRec.y + flyerRec.height - 18), (int)flyerRec.width - 4, 16, Fade((Color){220, 206, 178, 255}, 0.0f), Fade((Color){220, 206, 178, 255}, 0.55f));
    /* Canto inferior direito levemente dobrado */
    DrawTriangle((Vector2){flyerRec.x + flyerRec.width - 12, flyerRec.y + flyerRec.height - 2}, (Vector2){flyerRec.x + flyerRec.width - 2, flyerRec.y + flyerRec.height - 2},
                 (Vector2){flyerRec.x + flyerRec.width - 2, flyerRec.y + flyerRec.height - 12}, (Color){174, 130, 86, 255});
    DrawTriangle((Vector2){flyerRec.x + flyerRec.width - 12, flyerRec.y + flyerRec.height - 2}, (Vector2){flyerRec.x + flyerRec.width - 2, flyerRec.y + flyerRec.height - 12},
                 (Vector2){flyerRec.x + flyerRec.width - 12, flyerRec.y + flyerRec.height - 12}, (Color){226, 214, 190, 255});
    DrawRectangleRoundedLinesEx(flyerRec, 0.03f, 4, 1.5f, game->doc2008FlyerExamined ? (Color){45, 165, 85, 255} : (Color){165, 148, 120, 255});
    /* Faixa vermelha de cabecalho do manifesto */
    DrawRectangle((int)flyerRec.x + 8, (int)flyerRec.y + 12, (int)flyerRec.width - 16, 20, (Color){185, 42, 34, 255});
    DrawLine((int)flyerRec.x + 8, (int)flyerRec.y + 12, (int)(flyerRec.x + flyerRec.width - 9), (int)flyerRec.y + 12, (Color){214, 82, 70, 255});
    DrawLine((int)flyerRec.x + 8, (int)flyerRec.y + 31, (int)(flyerRec.x + flyerRec.width - 9), (int)flyerRec.y + 31, (Color){140, 28, 22, 255});
    DrawGameText("MANIFESTO", (int)flyerRec.x + 34, (int)flyerRec.y + 16, 12, RAYWHITE);
    /* Tachinha 3D vermelha no topo */
    DrawCircle((int)(flyerRec.x + flyerRec.width / 2 + 1), (int)flyerRec.y + 7, 5.0f, Fade(BLACK, 0.30f));
    DrawCircle((int)(flyerRec.x + flyerRec.width / 2), (int)flyerRec.y + 6, 5.0f, RED_COLOR);
    DrawCircle((int)(flyerRec.x + flyerRec.width / 2 - 1), (int)flyerRec.y + 5, 1.8f, Fade(RAYWHITE, 0.75f));
    /* Conteudo do panfleto */
    DrawGameText("LUCAS SILVA", (int)flyerRec.x + 26, (int)flyerRec.y + 40, 13, INK_COLOR);
    DrawLine((int)flyerRec.x + 14, (int)flyerRec.y + 58, (int)(flyerRec.x + flyerRec.width - 14), (int)flyerRec.y + 58, (Color){195, 182, 160, 255});
    DrawGameText(" Ato: 10h às 13h", (int)flyerRec.x + 18, (int)flyerRec.y + 66, 11, (Color){75, 65, 55, 255});
    DrawLine((int)flyerRec.x + 18, (int)flyerRec.y + 84, (int)(flyerRec.x + flyerRec.width - 24), (int)flyerRec.y + 84, (Color){205, 195, 175, 255});
    DrawLine((int)flyerRec.x + 18, (int)flyerRec.y + 90, (int)(flyerRec.x + flyerRec.width - 38), (int)flyerRec.y + 90, (Color){205, 195, 175, 255});
    /* Etiqueta [DOC 2] */
    DrawRectangleRounded((Rectangle){flyerRec.x + 38, flyerRec.y + 100, 72, 20}, 0.25f, 4, Fade((Color){185, 42, 34, 255}, 0.14f));
    DrawGameText("[DOC 2]", (int)flyerRec.x + 50, (int)flyerRec.y + 104, 11, (Color){175, 35, 25, 255});

    /* ===================================================================== */
    /* 5. MONITOR CFTV DE SEGURANCA NA PAREDE (DOC 4)                        */
    /* ===================================================================== */
    /* Suporte articulado de parede e conduite de cabos */
    DrawRectangle(824, 121, 22, 18, Fade(BLACK, 0.15f));
    DrawRectangle(822, 118, 22, 18, (Color){120, 128, 138, 255});
    DrawLine(822, 118, 843, 118, (Color){176, 184, 194, 255});
    DrawRectangleLines(822, 118, 22, 18, (Color){84, 92, 102, 255});
    /* Conduite de cabos com abracadeiras descendo ate a meia-parede */
    DrawLineEx((Vector2){835, 272}, (Vector2){835, 384}, 4.0f, Fade(BLACK, 0.12f));
    DrawLineEx((Vector2){833, 272}, (Vector2){833, 384}, 4.0f, (Color){170, 176, 184, 255});
    DrawLine(832, 272, 832, 384, (Color){214, 220, 226, 255});
    DrawLine(835, 272, 835, 384, (Color){128, 134, 142, 255});
    for (int cj = 300; cj < 384; cj += 36)
    {
        DrawRectangle(829, cj, 9, 4, (Color){150, 156, 164, 255});
        DrawLine(829, cj, 837, cj, (Color){206, 212, 220, 255});
    }
    /* Sombra projetada do monitor na parede */
    DrawRectangle((int)cctvMonitor.x + 5, (int)cctvMonitor.y + 5, (int)cctvMonitor.width, (int)cctvMonitor.height, Fade(BLACK, 0.22f));
    /* Moldura metalica prateada/grafite 2008 */
    DrawRectangleRounded(cctvMonitor, 0.06f, 6, (Color){74, 84, 96, 255});
    DrawRectangleGradientV((int)cctvMonitor.x + 4, (int)cctvMonitor.y + 2, (int)cctvMonitor.width - 8, 10, (Color){112, 124, 138, 255}, (Color){74, 84, 96, 255});
    DrawRectangleRoundedLinesEx(cctvMonitor, 0.06f, 6, 2.5f, (Color){125, 136, 148, 255});
    DrawRectangleRoundedLinesEx((Rectangle){cctvMonitor.x - 1, cctvMonitor.y - 1, cctvMonitor.width + 2, cctvMonitor.height + 2}, 0.06f, 6, 1.0f, (Color){52, 60, 70, 255});
    DrawRectangleRounded((Rectangle){cctvMonitor.x + 4, cctvMonitor.y + 4, cctvMonitor.width - 8, cctvMonitor.height - 16}, 0.04f, 4, (Color){44, 52, 60, 255});

    Rectangle cctvScr = {cctvMonitor.x + 9, cctvMonitor.y + 9, cctvMonitor.width - 18, cctvMonitor.height - 28};
    DrawRectangleRec(cctvScr, (Color){10, 26, 18, 255});

    BeginScissorMode((int)cctvScr.x, (int)cctvScr.y, (int)cctvScr.width, (int)cctvScr.height);
    /* Racks de fundo na imagem da camera */
    for (int rk = 0; rk < 3; rk++)
    {
        int rkx = (int)cctvScr.x + 14 + rk * 56;
        DrawRectangle(rkx, (int)cctvScr.y + 22, 42, 74, (Color){16, 40, 28, 255});
        DrawRectangleLines(rkx, (int)cctvScr.y + 22, 42, 74, (Color){28, 68, 46, 255});
        for (int rl = 0; rl < 4; rl++)
        {
            DrawCircle(rkx + 8, (int)cctvScr.y + 30 + rl * 15, 1.5f, (Color){70, 220, 120, 255});
        }
    }
    /* Silhueta detalhada de Lucas trabalhando no servidor as 14:30 */
    int silX = (int)cctvScr.x + 92;
    int silY = (int)cctvScr.y + 48;
    DrawCircle(silX, silY, 12, (Color){58, 152, 96, 255}); /* cabeca */
    DrawRectangleRounded((Rectangle){(float)(silX - 16), (float)(silY + 12), 32, 38}, 0.25f, 4, (Color){46, 126, 78, 255}); /* tronco */
    DrawLineEx((Vector2){(float)(silX + 12), (float)(silY + 20)}, (Vector2){(float)(silX + 30), (float)(silY + 14)}, 5.0f, (Color){58, 152, 96, 255}); /* braco no rack */
    /* Cracha no peito da silhueta */
    DrawRectangle(silX - 4, silY + 22, 8, 10, (Color){140, 245, 175, 255});

    /* Scanlines de CFTV */
    for (int sy = (int)cctvScr.y; sy < (int)(cctvScr.y + cctvScr.height); sy += 4)
    {
        DrawLine((int)cctvScr.x, sy, (int)(cctvScr.x + cctvScr.width), sy, Fade(BLACK, 0.22f));
    }

    /* HUD da camera CFTV */
    float recBlink = 0.5f + 0.5f * sinf(time * 5.0f);
    DrawCircle((int)cctvScr.x + 12, (int)cctvScr.y + 11, 3.5f, Fade(RED_COLOR, recBlink));
    DrawGameText("CAM-03 [14:30]", (int)cctvScr.x + 20, (int)cctvScr.y + 5, 11, (Color){95, 250, 135, 255});
    DrawRectangle((int)cctvScr.x, (int)(cctvScr.y + cctvScr.height - 18), (int)cctvScr.width, 18, Fade((Color){6, 16, 10, 255}, 0.82f));
    DrawGameText("[DOC 4: CFTV]", (int)cctvScr.x + 44, (int)(cctvScr.y + cctvScr.height - 15), 11, (Color){110, 255, 150, 255});
    EndScissorMode();

    /* Reflexo discreto no vidro da tela (canto direito, longe dos textos) */
    R2008Quad((Vector2){cctvScr.x + cctvScr.width - 46, cctvScr.y + 1}, (Vector2){cctvScr.x + cctvScr.width - 30, cctvScr.y + 1},
              (Vector2){cctvScr.x + cctvScr.width - 54, cctvScr.y + cctvScr.height - 20}, (Vector2){cctvScr.x + cctvScr.width - 70, cctvScr.y + cctvScr.height - 20},
              Fade(RAYWHITE, 0.07f));
    DrawRectangleLinesEx(cctvScr, 1.5f, (Color){55, 165, 95, 255});
    /* Botoes fisicos e LED no painel inferior do monitor CFTV */
    for (int mb = 0; mb < 4; mb++)
    {
        DrawRectangle((int)cctvMonitor.x + 115 + mb * 14, (int)(cctvMonitor.y + cctvMonitor.height - 13), 8, 4, (Color){130, 140, 150, 255});
        DrawLine((int)cctvMonitor.x + 115 + mb * 14, (int)(cctvMonitor.y + cctvMonitor.height - 13), (int)cctvMonitor.x + 122 + mb * 14, (int)(cctvMonitor.y + cctvMonitor.height - 13), (Color){186, 194, 204, 255});
    }
    DrawCircle((int)(cctvMonitor.x + cctvMonitor.width - 14), (int)(cctvMonitor.y + cctvMonitor.height - 11), 2.5f, (Color){70, 235, 115, 255});

    /* ===================================================================== */
    /* 6. MODULO CORPORATIVO DIREITO: RACK DE SERVIDORES + CREDENZA 3D       */
    /* ===================================================================== */
    /* Sombra projetada no piso pelo modulo direito */
    DrawEllipse(1072, 594, 122, 14, Fade(BLACK, 0.26f));

    /* Lateral esquerda 3D em perspectiva do gabinete/armario executivo */
    DrawRectangle(962, 130, 22, 456, (Color){142, 148, 156, 255});
    DrawRectangleGradientH(962, 130, 22, 456, (Color){160, 166, 174, 255}, (Color){126, 132, 140, 255});
    for (int sv = 0; sv < 10; sv++)
    {
        DrawLine(967, 160 + sv * 8, 979, 162 + sv * 8, (Color){104, 110, 118, 255});
        DrawLine(967, 161 + sv * 8, 979, 163 + sv * 8, (Color){176, 182, 190, 255});
    }
    DrawLineEx((Vector2){962, 130}, (Vector2){962, 586}, 2.0f, (Color){108, 114, 122, 255});

    /* Corpo frontal do armario integrado (Rack superior + Credenza inferior) */
    Rectangle unitFront = {984, 130, 192, 458};
    DrawRectangleRec(unitFront, (Color){176, 182, 190, 255});
    DrawRectangleGradientH((int)unitFront.x, (int)unitFront.y, (int)unitFront.width, (int)unitFront.height, (Color){188, 194, 202, 255}, (Color){166, 172, 180, 255});
    DrawLine((int)unitFront.x + 2, (int)unitFront.y + 2, (int)unitFront.x + 2, (int)(unitFront.y + unitFront.height - 2), (Color){212, 218, 224, 255});
    DrawRectangleLinesEx(unitFront, 2.0f, (Color){118, 126, 136, 255});
    DrawRectangleLinesEx((Rectangle){unitFront.x - 1, unitFront.y - 1, unitFront.width + 2, unitFront.height + 2}, 1.0f, (Color){86, 92, 102, 255});
    /* Topo e rodape do modulo */
    DrawRectangle(960, 124, 218, 10, (Color){128, 136, 146, 255});
    DrawLine(960, 124, 1177, 124, (Color){186, 192, 200, 255});
    DrawLine(960, 133, 1177, 133, (Color){96, 104, 114, 255});
    DrawRectangle(962, 576, 214, 14, (Color){104, 110, 118, 255});

    /* --- Parte Superior: Baia de Servidores (Rack 19") --- */
    Rectangle rackBay = {994, 142, 172, 232};
    DrawRectangleRec(rackBay, (Color){26, 34, 42, 255});
    DrawRectangleLinesEx(rackBay, 2.0f, (Color){88, 98, 110, 255});
    /* Trilhos do rack 19 polegadas com furos */
    DrawRectangle(996, 162, 4, 206, (Color){60, 70, 82, 255});
    DrawRectangle(1160, 162, 4, 206, (Color){60, 70, 82, 255});
    for (int rh = 166; rh < 366; rh += 8)
    {
        DrawPixel(997, rh, (Color){18, 22, 28, 255});
        DrawPixel(1161, rh, (Color){18, 22, 28, 255});
    }
    DrawGameText("SERVIDORES // RACK-03", 1012, 148, 11, (Color){165, 195, 215, 255});

    for (int u = 0; u < 5; u++)
    {
        Rectangle uSlot = {1002, (float)(168 + u * 39), 156, 32};
        DrawRectangleRec(uSlot, (Color){36, 46, 56, 255});
        DrawRectangleLinesEx(uSlot, 1.0f, (Color){68, 82, 96, 255});
        /* Alcas laterais do blade server */
        DrawLine((int)uSlot.x + 1, (int)uSlot.y + 1, (int)(uSlot.x + uSlot.width - 2), (int)uSlot.y + 1, (Color){78, 92, 108, 255});
        DrawLine((int)uSlot.x + 1, (int)(uSlot.y + uSlot.height - 2), (int)(uSlot.x + uSlot.width - 2), (int)(uSlot.y + uSlot.height - 2), (Color){24, 30, 38, 255});
        /* Etiqueta de patrimonio e baia de disco */
        DrawRectangle((int)uSlot.x + 126, (int)uSlot.y + 7, 18, 6, (Color){214, 218, 222, 255});
        DrawRectangle((int)uSlot.x + 126, (int)uSlot.y + 17, 18, 8, (Color){26, 32, 40, 255});
        DrawLine((int)uSlot.x + 126, (int)uSlot.y + 17, (int)uSlot.x + 143, (int)uSlot.y + 17, (Color){92, 106, 120, 255});
        DrawRectangle((int)uSlot.x + 3, (int)uSlot.y + 4, 4, 24, (Color){130, 140, 150, 255});
        DrawLine((int)uSlot.x + 3, (int)uSlot.y + 4, (int)uSlot.x + 3, (int)uSlot.y + 27, (Color){188, 196, 204, 255});
        DrawRectangle((int)(uSlot.x + uSlot.width - 7), (int)uSlot.y + 4, 4, 24, (Color){130, 140, 150, 255});
        /* Grade de ventilacao do servidor */
        for (int gx = (int)uSlot.x + 52; gx < (int)uSlot.x + 122; gx += 8)
        {
            DrawLine(gx, (int)uSlot.y + 8, gx, (int)uSlot.y + 24, (Color){20, 26, 32, 255});
        }
        /* LEDs de atividade piscando */
        float blink = sinf(time * (4.5f + u * 1.3f));
        if (blink > 0) DrawCircle((int)uSlot.x + 18, (int)uSlot.y + 16, 5.5f, Fade((Color){65, 235, 115, 255}, 0.22f));
        DrawCircle((int)uSlot.x + 18, (int)uSlot.y + 16, 3.0f, (blink > 0) ? (Color){65, 235, 115, 255} : (Color){22, 90, 42, 255});
        DrawCircle((int)uSlot.x + 30, (int)uSlot.y + 16, 3.0f, (blink < 0) ? CYAN_COLOR : (Color){20, 75, 95, 255});
        DrawCircle((int)uSlot.x + 42, (int)uSlot.y + 16, 2.2f, GOLD_COLOR);
    }

    /* --- Parte Inferior: Nicho Executivo em Madeira Clara (Documentos 5 e 6) --- */
    Rectangle credenzaBay = {986, 384, 188, 190};
    DrawRectangleRec(credenzaBay, (Color){194, 164, 126, 255});
    R2008WoodGrain(credenzaBay, (Color){194, 164, 126, 255}, 3);
    /* Fundo do nicho mais escuro no topo de cada prateleira (profundidade) */
    DrawRectangleGradientV(988, 386, 184, 14, Fade((Color){96, 70, 44, 255}, 0.45f), Fade((Color){96, 70, 44, 255}, 0.0f));
    DrawRectangleGradientV(988, 488, 184, 10, Fade((Color){96, 70, 44, 255}, 0.40f), Fade((Color){96, 70, 44, 255}, 0.0f));
    DrawRectangleLinesEx(credenzaBay, 2.0f, (Color){138, 108, 76, 255});
    /* Divisoria interna de madeira */
    DrawRectangle(988, 482, 184, 6, (Color){148, 116, 82, 255});
    DrawLine(988, 482, 1171, 482, (Color){196, 162, 122, 255});
    DrawLine(988, 487, 1171, 487, (Color){110, 84, 58, 255});

    /* DOCUMENTO DO PAI: Caderno de Pesquisa Dr. Ramos (fatherBookRec) */
    DrawRectangle((int)fatherBookRec.x + 3, (int)fatherBookRec.y + 4, (int)fatherBookRec.width, (int)fatherBookRec.height, Fade(BLACK, 0.25f));
    DrawRectangleRounded(fatherBookRec, 0.10f, 4, (Color){104, 62, 36, 255});
    DrawRectangleRoundedLinesEx(fatherBookRec, 0.10f, 4, 2.0f, game->doc2008FatherOriginExamined ? (Color){60, 220, 110, 255} : GOLD_COLOR);
    /* Lombada de couro com nervuras e fita marcadora dourada */
    DrawRectangle((int)fatherBookRec.x, (int)fatherBookRec.y, 18, (int)fatherBookRec.height, (Color){74, 42, 22, 255});
    for (int rb = 12; rb < (int)fatherBookRec.height - 8; rb += 14)
    {
        DrawLine((int)fatherBookRec.x + 2, (int)fatherBookRec.y + rb, (int)fatherBookRec.x + 16, (int)fatherBookRec.y + rb, GOLD_COLOR);
    }
    /* Folhas de papel visiveis na borda e etiqueta dourada */
    DrawRectangle((int)(fatherBookRec.x + fatherBookRec.width - 8), (int)fatherBookRec.y + 5, 5, (int)fatherBookRec.height - 10, (Color){242, 236, 218, 255});
    for (int pg = 0; pg < 2; pg++)
        DrawLine((int)(fatherBookRec.x + fatherBookRec.width - 7 + pg * 2), (int)fatherBookRec.y + 6, (int)(fatherBookRec.x + fatherBookRec.width - 7 + pg * 2), (int)(fatherBookRec.y + fatherBookRec.height - 6), (Color){206, 196, 172, 255});
    DrawLine((int)fatherBookRec.x + 19, (int)fatherBookRec.y + 3, (int)(fatherBookRec.x + fatherBookRec.width - 10), (int)fatherBookRec.y + 3, (Color){140, 92, 60, 255});
    DrawLine((int)fatherBookRec.x + 18, (int)fatherBookRec.y + 2, (int)fatherBookRec.x + 18, (int)(fatherBookRec.y + fatherBookRec.height - 3), (Color){52, 28, 14, 255});
    DrawRectangleRounded((Rectangle){fatherBookRec.x + 24, fatherBookRec.y + 8, 130, 22}, 0.2f, 4, (Color){68, 38, 20, 255});
    DrawGameText("CADERNO DR. RAMOS", (int)fatherBookRec.x + 28, (int)fatherBookRec.y + 13, 10, GOLD_COLOR);
    DrawGameText("Projeto Humanitário", (int)fatherBookRec.x + 26, (int)fatherBookRec.y + 38, 11, (Color){245, 235, 215, 255});

    /* DOCUMENTO DO GOVERNO: Memorando Confidencial 2008 (govMemoRec) */
    DrawRectangle((int)govMemoRec.x + 3, (int)govMemoRec.y + 4, (int)govMemoRec.width, (int)govMemoRec.height, Fade(BLACK, 0.25f));
    DrawRectangleRounded(govMemoRec, 0.10f, 4, (Color){128, 38, 38, 255});
    DrawRectangleRoundedLinesEx(govMemoRec, 0.10f, 4, 2.0f, game->doc2008GovContractExamined ? (Color){60, 220, 110, 255} : RED_COLOR);
    /* Aba de pasta confidencial, selo e grampo metalico */
    DrawRectangle((int)govMemoRec.x, (int)govMemoRec.y, 18, (int)govMemoRec.height, (Color){88, 24, 24, 255});
    DrawLine((int)govMemoRec.x + 19, (int)govMemoRec.y + 3, (int)(govMemoRec.x + govMemoRec.width - 6), (int)govMemoRec.y + 3, (Color){168, 64, 60, 255});
    DrawLine((int)govMemoRec.x + 18, (int)govMemoRec.y + 2, (int)govMemoRec.x + 18, (int)(govMemoRec.y + govMemoRec.height - 3), (Color){62, 16, 16, 255});
    DrawCircle((int)govMemoRec.x + 10, (int)(govMemoRec.y + govMemoRec.height / 2) + 1, 4.5f, Fade(BLACK, 0.35f));
    DrawCircle((int)govMemoRec.x + 9, (int)(govMemoRec.y + govMemoRec.height / 2), 4.5f, GOLD_COLOR);
    DrawCircle((int)govMemoRec.x + 8, (int)(govMemoRec.y + govMemoRec.height / 2) - 1, 1.5f, (Color){255, 240, 180, 255});
    DrawRectangleRounded((Rectangle){govMemoRec.x + 24, govMemoRec.y + 8, 132, 22}, 0.2f, 4, (Color){78, 20, 20, 255});
    DrawGameText("MEMORANDO GOV 2008", (int)govMemoRec.x + 27, (int)govMemoRec.y + 13, 10, (Color){255, 165, 155, 255});
    DrawGameText("Uso Militar / Drones", (int)govMemoRec.x + 26, (int)govMemoRec.y + 38, 11, (Color){242, 228, 225, 255});

    /* ===================================================================== */
    /* 7. TAPETE CORPORATIVO E MESA EXECUTIVA EM PERSPECTIVA 3D              */
    /* ===================================================================== */
    /* Tapete corporativo azul-ardosia sob a mesa acompanhando a perspectiva */
    Color rugCol = (Color){118, 134, 150, 255};
    DrawTriangle((Vector2){355, 650}, (Vector2){825, 558}, (Vector2){455, 558}, rugCol);
    DrawTriangle((Vector2){355, 650}, (Vector2){925, 650}, (Vector2){825, 558}, rugCol);
    DrawLineEx((Vector2){355, 650}, (Vector2){925, 650}, 4.0f, (Color){82, 98, 114, 255});
    DrawLineEx((Vector2){455, 558}, (Vector2){825, 558}, 3.0f, (Color){92, 108, 124, 255});
    DrawLineEx((Vector2){455, 558}, (Vector2){355, 650}, 3.0f, (Color){88, 104, 120, 255});
    DrawLineEx((Vector2){825, 558}, (Vector2){925, 650}, 3.0f, (Color){88, 104, 120, 255});
    /* Trama do carpete: faixa interna e listras finas em perspectiva */
    R2008Quad((Vector2){466, 566}, (Vector2){814, 566}, (Vector2){900, 642}, (Vector2){380, 642}, Fade((Color){96, 112, 130, 255}, 0.35f));
    R2008Quad((Vector2){474, 572}, (Vector2){806, 572}, (Vector2){886, 636}, (Vector2){394, 636}, rugCol);
    for (int rl = 0; rl < 12; rl++)
    {
        float ry = 578.0f + rl * 5.5f;
        float k = (ry - 558.0f) / 92.0f;
        float rx0 = 455.0f - 100.0f * k + 26.0f;
        float rx1 = 825.0f + 100.0f * k - 26.0f;
        DrawLine((int)rx0, (int)ry, (int)rx1, (int)ry, Fade((rl % 2) ? (Color){134, 150, 166, 255} : (Color){104, 120, 136, 255}, 0.45f));
    }

    /* Sombra projetada da mesa executiva no piso */
    DrawEllipse(618, 618, 375, 20, Fade(BLACK, 0.24f));

    /* Vao central da mesa (painel de modestia em madeira escura) */
    DrawRectangle(495, 502, 250, 100, (Color){115, 88, 62, 255});
    DrawRectangleGradientV(495, 502, 250, 34, (Color){78, 58, 40, 255}, (Color){115, 88, 62, 255});
    /* Ripas verticais do painel de modestia */
    for (int mp = 520; mp < 745; mp += 25)
    {
        DrawLine(mp, 512, mp, 600, Fade((Color){86, 64, 44, 255}, 0.7f));
        DrawLine(mp + 1, 512, mp + 1, 600, Fade((Color){138, 108, 78, 255}, 0.5f));
    }
    /* Cabos do computador descendo por tras da mesa ate a tomada */
    R2008Cable((Vector2){604, 512}, (Vector2){600, 560}, (Vector2){560, 570}, (Vector2){548, 600}, 3.0f, (Color){40, 44, 50, 255});
    R2008Cable((Vector2){626, 512}, (Vector2){630, 556}, (Vector2){664, 574}, (Vector2){676, 600}, 3.0f, (Color){52, 56, 62, 255});

    /* Pedestal esquerdo (gaveteiro executivo em carvalho claro) */
    DrawRectangle(280, 502, 215, 104, (Color){178, 144, 104, 255});
    DrawRectangleGradientV(280, 511, 215, 10, Fade((Color){90, 66, 42, 255}, 0.45f), Fade((Color){90, 66, 42, 255}, 0.0f));
    DrawLine(281, 502, 281, 605, (Color){204, 172, 132, 255});
    DrawRectangle(489, 502, 6, 104, (Color){150, 118, 84, 255});
    DrawRectangle(280, 600, 215, 6, (Color){118, 92, 64, 255});
    DrawRectangleLines(280, 502, 215, 104, (Color){110, 84, 58, 255});
    /* Duas gavetas com puxadores de aluminio escovado */
    for (int dr = 0; dr < 2; dr++)
    {
        Rectangle dBox = {296, (float)(512 + dr * 43), 182, 37};
        DrawRectangleRec(dBox, (Color){192, 158, 116, 255});
        R2008WoodGrain(dBox, (Color){192, 158, 116, 255}, dr + 1);
        DrawLine((int)dBox.x + 1, (int)dBox.y + 1, (int)(dBox.x + dBox.width - 2), (int)dBox.y + 1, (Color){214, 184, 146, 255});
        DrawLine((int)dBox.x + 1, (int)(dBox.y + dBox.height - 2), (int)(dBox.x + dBox.width - 2), (int)(dBox.y + dBox.height - 2), (Color){160, 128, 92, 255});
        DrawRectangleLinesEx(dBox, 1.5f, (Color){138, 108, 76, 255});
        DrawRectangleRounded((Rectangle){dBox.x + 63, dBox.y + 17, 58, 8}, 0.4f, 4, Fade((Color){90, 66, 42, 255}, 0.45f));
        DrawRectangleRounded((Rectangle){dBox.x + 62, dBox.y + 15, 58, 8}, 0.4f, 4, (Color){205, 212, 220, 255});
        DrawLine((int)dBox.x + 65, (int)dBox.y + 16, (int)dBox.x + 117, (int)dBox.y + 16, (Color){240, 244, 248, 255});
        DrawLine((int)dBox.x + 65, (int)dBox.y + 22, (int)dBox.x + 117, (int)dBox.y + 22, (Color){150, 158, 168, 255});
        DrawCircle((int)(dBox.x + dBox.width - 14), (int)(dBox.y + 12), 2.5f, (Color){110, 116, 124, 255});
    }

    /* Pedestal direito (armario da mesa em carvalho claro) */
    DrawRectangle(745, 502, 205, 104, (Color){178, 144, 104, 255});
    DrawRectangleGradientV(745, 511, 205, 10, Fade((Color){90, 66, 42, 255}, 0.45f), Fade((Color){90, 66, 42, 255}, 0.0f));
    DrawRectangle(745, 502, 6, 104, (Color){150, 118, 84, 255});
    DrawLine(949, 502, 949, 605, (Color){150, 118, 84, 255});
    DrawRectangle(745, 600, 205, 6, (Color){118, 92, 64, 255});
    DrawRectangleLines(745, 502, 205, 104, (Color){110, 84, 58, 255});
    Rectangle rDoor = {762, 512, 170, 80};
    DrawRectangleRec(rDoor, (Color){192, 158, 116, 255});
    R2008WoodGrain(rDoor, (Color){192, 158, 116, 255}, 5);
    /* Almofada rebaixada da porta do armario */
    DrawRectangleLinesEx((Rectangle){rDoor.x + 34, rDoor.y + 10, rDoor.width - 48, rDoor.height - 20}, 1.0f, (Color){160, 128, 92, 255});
    DrawLine((int)rDoor.x + 35, (int)(rDoor.y + rDoor.height - 10), (int)(rDoor.x + rDoor.width - 14), (int)(rDoor.y + rDoor.height - 10), (Color){214, 184, 146, 255});
    DrawLine((int)rDoor.x + 1, (int)rDoor.y + 1, (int)(rDoor.x + rDoor.width - 2), (int)rDoor.y + 1, (Color){214, 184, 146, 255});
    DrawRectangleLinesEx(rDoor, 1.5f, (Color){138, 108, 76, 255});
    DrawRectangleRounded((Rectangle){rDoor.x + 15, rDoor.y + 30, 8, 28}, 0.4f, 4, Fade((Color){90, 66, 42, 255}, 0.45f));
    DrawRectangleRounded((Rectangle){rDoor.x + 14, rDoor.y + 28, 8, 28}, 0.4f, 4, (Color){205, 212, 220, 255});
    DrawLine((int)rDoor.x + 16, (int)rDoor.y + 30, (int)rDoor.x + 16, (int)rDoor.y + 53, (Color){240, 244, 248, 255});
    /* Fechadura do armario */
    DrawCircle((int)(rDoor.x + rDoor.width - 16), (int)rDoor.y + 14, 3.0f, (Color){150, 158, 168, 255});
    DrawLine((int)(rDoor.x + rDoor.width - 16), (int)rDoor.y + 12, (int)(rDoor.x + rDoor.width - 16), (int)rDoor.y + 16, (Color){70, 76, 84, 255});

    /* Tampo da mesa executiva com perspectiva 3D real (mais profundo para acomodar todos os itens) */
    Vector2 tTopL = {268, 412};
    Vector2 tTopR = {962, 412};
    Vector2 tBotR = {982, 500};
    Vector2 tBotL = {248, 500};
    Color deskTopCol = (Color){214, 182, 140, 255};
    DrawTriangle(tTopL, tBotL, tTopR, deskTopCol);
    DrawTriangle(tTopR, tBotL, tBotR, deskTopCol);
    /* Veios longos do carvalho acompanhando a profundidade do tampo */
    for (int vg = 0; vg < 13; vg++)
    {
        float vy = 418.0f + vg * 6.5f;
        float k = (vy - 412.0f) / 88.0f;
        float vx0 = 268.0f - 20.0f * k + 6.0f + (float)((vg * 37) % 60);
        float vx1 = 962.0f + 20.0f * k - 6.0f - (float)((vg * 53) % 80);
        DrawLineEx((Vector2){vx0, vy}, (Vector2){vx1, vy + ((vg % 2) ? 1.0f : 0.0f)}, 1.0f,
                   Fade((vg % 3 == 0) ? (Color){232, 204, 166, 255} : (Color){188, 154, 112, 255}, 0.50f));
    }
    /* Luz da janela batendo no tampo (faixa suave) */
    R2008Quad((Vector2){340, 413}, (Vector2){470, 413}, (Vector2){440, 499}, (Vector2){300, 499}, Fade((Color){255, 248, 226, 255}, 0.16f));
    DrawLineEx(tTopL, tTopR, 2.0f, (Color){236, 210, 174, 255});
    DrawLineEx(tTopL, tBotL, 2.0f, (Color){184, 150, 110, 255});
    DrawLineEx(tTopR, tBotR, 2.0f, (Color){184, 150, 110, 255});
    DrawLineEx((Vector2){267, 411}, (Vector2){247, 500}, 1.0f, (Color){130, 100, 70, 255});
    DrawLineEx((Vector2){963, 411}, (Vector2){983, 500}, 1.0f, (Color){130, 100, 70, 255});

    /* Borda frontal chanfrada do tampo (madeira + friso de aluminio) */
    DrawRectangle(248, 500, 734, 10, (Color){162, 128, 92, 255});
    DrawLine(248, 500, 981, 500, (Color){226, 196, 156, 255});
    DrawRectangle(248, 508, 734, 3, (Color){190, 198, 206, 255});
    DrawLine(248, 508, 981, 508, (Color){230, 236, 242, 255});
    DrawLine(248, 511, 981, 511, (Color){104, 80, 56, 255});

    /* Desk pad (mata-borrao executivo em couro grafite no centro da mesa) */
    Vector2 pTL = {472, 434};
    Vector2 pTR = {758, 434};
    Vector2 pBR = {768, 494};
    Vector2 pBL = {462, 494};
    Color padCol = (Color){52, 60, 70, 255};
    DrawTriangle(pTL, pBL, pTR, padCol);
    DrawTriangle(pTR, pBL, pBR, padCol);
    DrawLineEx(pTL, pTR, 1.5f, (Color){88, 98, 110, 255});
    /* Costura pespontada do mata-borrao de couro */
    for (int st = 0; st < 46; st++)
    {
        float sx = 478.0f + st * 6.0f;
        DrawLine((int)sx, 438, (int)sx + 3, 438, Fade((Color){120, 130, 142, 255}, 0.8f));
        DrawLine((int)sx - 4, 490, (int)sx - 1, 490, Fade((Color){120, 130, 142, 255}, 0.8f));
    }
    DrawLineEx(pBL, pBR, 2.0f, (Color){34, 40, 48, 255});

    /* ===================================================================== */
    /* 8. COMPUTADOR CENTRAL DE VIGILANCIA, TECLADO, MOUSE E DOCUMENTOS      */
    /* ===================================================================== */
    /* Caneca de cafe executiva sobre porta-copos a esquerda do monitor */
    DrawEllipse(466, 444, 13, 4, (Color){145, 110, 75, 255});
    DrawEllipse(470, 445, 15, 4, Fade(BLACK, 0.18f));
    DrawRectangleRounded((Rectangle){456, 419, 20, 24}, 0.2f, 4, (Color){245, 246, 250, 255});
    /* Sombreado cilindrico da porcelana */
    DrawRectangle(469, 421, 6, 21, (Color){216, 220, 228, 255});
    DrawRectangle(458, 421, 3, 21, (Color){255, 255, 255, 255});
    DrawRectangleLinesEx((Rectangle){456, 419, 20, 24}, 1.2f, (Color){175, 182, 192, 255});
    /* Cafe dentro da caneca e vapor subindo */
    DrawEllipse(466, 421, 8, 2, (Color){96, 60, 34, 255});
    DrawRing((Vector2){476, 431}, 3.0f, 5.5f, -90, 90, 8, (Color){225, 230, 238, 255});
    DrawRing((Vector2){476, 432}, 4.5f, 5.5f, 0, 90, 6, (Color){186, 192, 202, 255});
    /* Logo corporativo da caneca */
    DrawRectangle(461, 428, 6, 6, (Color){60, 110, 170, 255});
    for (int sm = 0; sm < 2; sm++)
    {
        float sp = fmodf(time * 0.6f + sm * 0.5f, 1.0f);
        float sx = 464.0f + sm * 5.0f + sinf(time * 2.0f + sm * 2.0f + sp * 6.0f) * 2.5f;
        DrawCircle((int)sx, (int)(416.0f - sp * 18.0f), 2.0f + sp * 1.5f, Fade(RAYWHITE, 0.45f * (1.0f - sp)));
    }

    /* Base e coluna articulada do monitor LCD 2008 sobre a mesa */
    DrawEllipse(615, 446, 68, 6, Fade(BLACK, 0.25f));
    DrawRectangle(597, 402, 36, 38, (Color){152, 160, 170, 255});
    DrawRectangle(603, 402, 10, 38, (Color){182, 190, 200, 255});
    DrawRectangle(626, 402, 7, 38, (Color){128, 136, 146, 255});
    DrawRectangleLines(597, 402, 36, 38, (Color){110, 118, 128, 255});
    /* Passa-cabo na coluna */
    DrawRectangleRounded((Rectangle){609, 418, 12, 8}, 0.5f, 4, (Color){92, 100, 110, 255});
    DrawRectangleRounded((Rectangle){555, 434, 120, 14}, 0.35f, 6, (Color){174, 182, 192, 255});
    DrawLine(561, 435, 669, 435, (Color){214, 220, 228, 255});
    DrawLine(561, 446, 669, 446, (Color){140, 148, 158, 255});
    DrawRectangleRoundedLinesEx((Rectangle){555, 434, 120, 14}, 0.35f, 6, 1.5f, (Color){128, 136, 146, 255});

    /* Gabinete do Monitor LCD widescreen/4:3 corporativo 2008 */
    DrawRectangle((int)monitorRec.x + 5, (int)monitorRec.y + 5, (int)monitorRec.width, (int)monitorRec.height, Fade(BLACK, 0.22f));
    DrawRectangleRounded(monitorRec, 0.06f, 8, (Color){194, 202, 210, 255});
    /* Volume da moldura prateada: topo claro, base e direita escurecidas */
    DrawRectangle((int)monitorRec.x + 8, (int)monitorRec.y + 3, (int)monitorRec.width - 16, 3, (Color){224, 230, 236, 255});
    DrawRectangle((int)monitorRec.x + 3, (int)monitorRec.y + 8, 3, (int)monitorRec.height - 16, (Color){214, 220, 228, 255});
    DrawRectangle((int)(monitorRec.x + monitorRec.width - 6), (int)monitorRec.y + 8, 3, (int)monitorRec.height - 16, (Color){164, 172, 182, 255});
    DrawRectangleRoundedLinesEx(monitorRec, 0.06f, 8, 2.5f, (Color){126, 136, 148, 255});
    DrawRectangleRoundedLinesEx((Rectangle){monitorRec.x - 1, monitorRec.y - 1, monitorRec.width + 2, monitorRec.height + 2}, 0.06f, 8, 1.0f, (Color){90, 98, 110, 255});
    /* Webcam biometrica acoplada no topo do monitor 2008 */
    DrawRectangleRounded((Rectangle){603, 218, 24, 10}, 0.4f, 4, (Color){58, 66, 76, 255});
    DrawCircle(615, 223, 3.0f, (Color){24, 32, 42, 255});
    DrawCircle(615, 223, 1.2f, CYAN_COLOR);
    /* Moldura interna escura e barra de alto-falante inferior */
    DrawRectangleRounded((Rectangle){monitorRec.x + 8, monitorRec.y + 8, monitorRec.width - 16, monitorRec.height - 26}, 0.04f, 6, (Color){42, 50, 60, 255});
    DrawRectangle((int)monitorRec.x + 16, (int)(monitorRec.y + monitorRec.height - 14), (int)monitorRec.width - 32, 8, (Color){148, 156, 166, 255});
    /* Grade do alto-falante */
    for (int gl = 0; gl < 54; gl++)
        DrawPixel((int)monitorRec.x + 20 + gl * 4, (int)(monitorRec.y + monitorRec.height - 10), (Color){104, 112, 122, 255});
    DrawCircle((int)(monitorRec.x + monitorRec.width - 18), (int)(monitorRec.y + monitorRec.height - 10), 3.0f, (Color){70, 235, 120, 255});

    /* Tela LCD acesa */
    Rectangle lcdScr = {monitorRec.x + 12, monitorRec.y + 12, monitorRec.width - 24, monitorRec.height - 34};
    Color lcdBg = game->puzzle2008Contested ? (Color){12, 34, 24, 255} : (Color){12, 26, 40, 255};
    Color lcdBorder = game->puzzle2008Contested ? (Color){60, 220, 110, 255} : CYAN_COLOR;
    DrawRectangleRounded(lcdScr, 0.04f, 4, lcdBg);
    /* Brilho do monitor irradiando na moldura e reflexo do vidro no canto inferior (sem tocar nos textos) */
    DrawRectangleGradientV((int)lcdScr.x + 4, (int)(lcdScr.y + lcdScr.height - 26), (int)lcdScr.width - 8, 22, Fade(lcdBorder, 0.0f), Fade(lcdBorder, 0.06f));
    R2008Quad((Vector2){lcdScr.x + lcdScr.width - 70, lcdScr.y + lcdScr.height - 28}, (Vector2){lcdScr.x + lcdScr.width - 52, lcdScr.y + lcdScr.height - 28},
              (Vector2){lcdScr.x + lcdScr.width - 62, lcdScr.y + lcdScr.height - 4}, (Vector2){lcdScr.x + lcdScr.width - 80, lcdScr.y + lcdScr.height - 4}, Fade(RAYWHITE, 0.06f));
    DrawRectangleRoundedLinesEx(lcdScr, 0.04f, 4, 1.5f, lcdBorder);

    DrawGameText("IA VIGILÂNCIA // 2008", (int)lcdScr.x + 14, (int)lcdScr.y + 12, 13, lcdBorder);
    DrawLine((int)lcdScr.x + 14, (int)lcdScr.y + 30, (int)(lcdScr.x + lcdScr.width - 14), (int)lcdScr.y + 30, Fade(lcdBorder, 0.45f));

    if (game->puzzle2008OrderSuspended)
    {
        DrawGameText("[OK] ORDEM SUSPENSA", (int)lcdScr.x + 14, (int)lcdScr.y + 46, 14, (Color){60, 235, 110, 255});
        DrawGameText("CD ejetado no drive!", (int)lcdScr.x + 14, (int)lcdScr.y + 72, 12, RAYWHITE);
        float pB = 0.5f + 0.5f * sinf(time * 6.0f);
        DrawGameText("> COLETAR CD [ENTER] <", (int)lcdScr.x + 14, (int)lcdScr.y + 104, 13, Fade(GOLD_COLOR, pB));
    }
    else if (game->puzzle2008Contested)
    {
        DrawGameText("[!] FALSO POSITIVO", (int)lcdScr.x + 14, (int)lcdScr.y + 46, 14, GOLD_COLOR);
        DrawGameText("Identificação rejeitada.", (int)lcdScr.x + 14, (int)lcdScr.y + 72, 12, RAYWHITE);
        DrawGameText("> SUSPENDER ORDEM <", (int)lcdScr.x + 14, (int)lcdScr.y + 104, 13, (Color){60, 235, 110, 255});
    }
    else
    {
        DrawGameText("ALVO: LUCAS SILVA", (int)lcdScr.x + 14, (int)lcdScr.y + 46, 14, RED_COLOR);
        DrawGameText("ORDEM DE DRONE: 14:30", (int)lcdScr.x + 14, (int)lcdScr.y + 72, 12, GOLD_COLOR);
        float pB = 0.5f + 0.5f * sinf(time * 4.0f);
        DrawGameText("> CONTESTAR NO TERMINAL <", (int)lcdScr.x + 10, (int)lcdScr.y + 104, 13, Fade(CYAN_COLOR, pB));
    }

    /* Teclado corporativo detalhado (multiplas fileiras + barra de espaco) */
    Rectangle kbRec = {495, 454, 224, 30};
    DrawRectangleRounded((Rectangle){kbRec.x + 2, kbRec.y + 3, kbRec.width, kbRec.height}, 0.14f, 6, Fade(BLACK, 0.28f));
    DrawRectangleRounded(kbRec, 0.14f, 6, (Color){184, 192, 200, 255});
    DrawRectangleRoundedLinesEx(kbRec, 0.14f, 6, 1.5f, (Color){126, 134, 144, 255});
    DrawLine((int)kbRec.x + 4, (int)kbRec.y + 1, (int)(kbRec.x + kbRec.width - 4), (int)kbRec.y + 1, (Color){222, 228, 234, 255});
    for (int col = 0; col < 11; col++)
    {
        DrawRectangle((int)kbRec.x + 8 + col * 19, (int)kbRec.y + 4, 15, 5, (Color){52, 58, 66, 255});
        DrawRectangle((int)kbRec.x + 8 + col * 19, (int)kbRec.y + 11, 15, 5, (Color){52, 58, 66, 255});
        DrawLine((int)kbRec.x + 8 + col * 19, (int)kbRec.y + 4, (int)kbRec.x + 22 + col * 19, (int)kbRec.y + 4, (Color){92, 100, 110, 255});
        DrawLine((int)kbRec.x + 8 + col * 19, (int)kbRec.y + 11, (int)kbRec.x + 22 + col * 19, (int)kbRec.y + 11, (Color){92, 100, 110, 255});
    }
    DrawRectangle((int)kbRec.x + 8, (int)kbRec.y + 19, 34, 6, (Color){52, 58, 66, 255});
    DrawRectangle((int)kbRec.x + 48, (int)kbRec.y + 19, 96, 6, (Color){42, 48, 56, 255}); /* espaco */
    DrawRectangle((int)kbRec.x + 150, (int)kbRec.y + 19, 64, 6, (Color){52, 58, 66, 255});
    DrawLine((int)kbRec.x + 8, (int)kbRec.y + 19, (int)kbRec.x + 41, (int)kbRec.y + 19, (Color){92, 100, 110, 255});
    DrawLine((int)kbRec.x + 48, (int)kbRec.y + 19, (int)kbRec.x + 143, (int)kbRec.y + 19, (Color){84, 92, 102, 255});
    DrawLine((int)kbRec.x + 150, (int)kbRec.y + 19, (int)kbRec.x + 213, (int)kbRec.y + 19, (Color){92, 100, 110, 255});
    /* Cabo do teclado indo para a base do monitor */
    R2008Cable((Vector2){kbRec.x + 112, kbRec.y}, (Vector2){kbRec.x + 112, kbRec.y - 6}, (Vector2){600, 452}, (Vector2){604, 447}, 1.5f, (Color){60, 66, 74, 255});
    DrawCircle((int)(kbRec.x + kbRec.width - 8), (int)kbRec.y + 6, 1.8f, (Color){80, 235, 120, 255});

    /* Mouse optico 2008 sobre o deskpad */
    Rectangle mouseRec = {734, 456, 22, 28};
    R2008Cable((Vector2){745, 456}, (Vector2){745, 446}, (Vector2){700, 450}, (Vector2){674, 444}, 1.5f, (Color){60, 66, 74, 255});
    DrawEllipse(747, 484, 12, 3, Fade(BLACK, 0.30f));
    DrawRectangleRounded(mouseRec, 0.45f, 6, (Color){190, 198, 206, 255});
    DrawRectangleRounded((Rectangle){mouseRec.x + 3, mouseRec.y + 3, 6, 18}, 0.5f, 4, (Color){222, 228, 234, 255});
    DrawLine((int)mouseRec.x + 2, (int)mouseRec.y + 12, (int)(mouseRec.x + mouseRec.width - 3), (int)mouseRec.y + 12, (Color){146, 154, 164, 255});
    DrawRectangleRoundedLinesEx(mouseRec, 0.45f, 6, 1.2f, (Color){118, 126, 136, 255});
    DrawRectangle((int)mouseRec.x + 9, (int)mouseRec.y + 4, 4, 7, (Color){52, 58, 66, 255});
    DrawPixel((int)mouseRec.x + 10, (int)mouseRec.y + 5, (Color){120, 128, 138, 255});

    /* --- LADO ESQUERDO DA MESA: DOSSIE FOTO DRONE (DOC 1 / doc0Rec) --- */
    /* Pasta parda de investigacao apoiada 100% sobre o tampo da mesa */
    DrawRectangle((int)doc0Rec.x + 3, (int)doc0Rec.y + 4, (int)doc0Rec.width, (int)doc0Rec.height, Fade(BLACK, 0.22f));
    /* Folhas extras saindo por baixo da pasta */
    DrawRectangle((int)doc0Rec.x + 6, (int)(doc0Rec.y + doc0Rec.height - 2), (int)doc0Rec.width - 14, 3, (Color){236, 232, 222, 255});
    DrawLine((int)doc0Rec.x + 6, (int)(doc0Rec.y + doc0Rec.height + 1), (int)(doc0Rec.x + doc0Rec.width - 9), (int)(doc0Rec.y + doc0Rec.height + 1), (Color){188, 180, 166, 255});
    DrawRectangleRounded((Rectangle){doc0Rec.x + 8, doc0Rec.y - 5, 54, 8}, 0.25f, 4, (Color){216, 186, 136, 255});
    DrawLine((int)doc0Rec.x + 11, (int)doc0Rec.y - 4, (int)doc0Rec.x + 58, (int)doc0Rec.y - 4, (Color){236, 212, 170, 255});
    DrawRectangleRounded(doc0Rec, 0.06f, 4, (Color){232, 204, 154, 255});
    DrawRectangleRoundedLinesEx(doc0Rec, 0.06f, 4, 2.0f, game->doc2008FaceExamined ? (Color){45, 165, 85, 255} : (Color){158, 124, 82, 255});
    /* Folha branca interna sobressaindo */
    DrawRectangle((int)doc0Rec.x + 5, (int)doc0Rec.y + 5, (int)doc0Rec.width - 10, (int)doc0Rec.height - 10, (Color){250, 248, 242, 255});
    /* Fotografia aerea do drone anexada com mira biometrica */
    Rectangle photoBox = {doc0Rec.x + 10, doc0Rec.y + 10, 52, 56};
    DrawRectangleRec(photoBox, (Color){18, 26, 36, 255});
    DrawCircle((int)photoBox.x + 26, (int)photoBox.y + 21, 9, (Color){78, 118, 145, 255});
    DrawRectangle((int)photoBox.x + 15, (int)photoBox.y + 31, 22, 20, (Color){58, 92, 118, 255});
    DrawRectangleLinesEx((Rectangle){photoBox.x + 13, photoBox.y + 9, 26, 25}, 1.5f, RED_COLOR);
    DrawRectangleLinesEx(photoBox, 1.5f, RED_COLOR);
    /* Clipe metalico no topo da foto */
    DrawRectangle((int)photoBox.x + 19, (int)doc0Rec.y + 3, 12, 9, (Color){180, 188, 198, 255});
    DrawLine((int)photoBox.x + 19, (int)doc0Rec.y + 3, (int)photoBox.x + 30, (int)doc0Rec.y + 3, (Color){226, 232, 238, 255});
    DrawRectangleLines((int)photoBox.x + 19, (int)doc0Rec.y + 3, 12, 9, (Color){120, 128, 138, 255});
    /* Textos do Dossie */
    DrawGameText("DRONE 78%", (int)doc0Rec.x + 70, (int)doc0Rec.y + 12, 12, RED_COLOR);
    DrawGameText("LUCAS S.", (int)doc0Rec.x + 70, (int)doc0Rec.y + 31, 12, INK_COLOR);
    DrawRectangleRounded((Rectangle){doc0Rec.x + 70, doc0Rec.y + 49, 70, 18}, 0.25f, 4, Fade(RED_COLOR, 0.15f));
    DrawGameText("[DOC 1]", (int)doc0Rec.x + 82, (int)doc0Rec.y + 52, 11, (Color){165, 32, 22, 255});

    /* --- LADO DIREITO DA MESA: PRANCHETA ENTRADA/CATRACA (DOC 3 / doc2Rec) --- */
    DrawRectangle((int)doc2Rec.x + 3, (int)doc2Rec.y + 4, (int)doc2Rec.width, (int)doc2Rec.height, Fade(BLACK, 0.22f));
    DrawRectangleRounded(doc2Rec, 0.06f, 4, (Color){168, 128, 86, 255});
    R2008WoodGrain((Rectangle){doc2Rec.x + 2, doc2Rec.y + 2, doc2Rec.width - 4, doc2Rec.height - 4}, (Color){168, 128, 86, 255}, 7);
    DrawRectangleRoundedLinesEx(doc2Rec, 0.06f, 4, 2.0f, game->doc2008EntryExamined ? (Color){45, 165, 85, 255} : (Color){116, 84, 52, 255});
    /* Folha timbrada da prancheta */
    DrawRectangle((int)doc2Rec.x + 7, (int)doc2Rec.y + 7, (int)doc2Rec.width - 14, (int)doc2Rec.height - 12, (Color){248, 250, 254, 255});
    /* Cabecalho azul corporativo na folha e clipe metalico cromado */
    DrawRectangle((int)doc2Rec.x + 11, (int)doc2Rec.y + 12, (int)doc2Rec.width - 22, 7, (Color){45, 85, 128, 255});
    DrawRectangleRounded((Rectangle){doc2Rec.x + 49, doc2Rec.y + 4, 58, 9}, 0.3f, 4, Fade(BLACK, 0.20f));
    DrawRectangleRounded((Rectangle){doc2Rec.x + 48, doc2Rec.y + 2, 58, 9}, 0.3f, 4, (Color){192, 200, 210, 255});
    DrawLine((int)doc2Rec.x + 51, (int)doc2Rec.y + 3, (int)doc2Rec.x + 102, (int)doc2Rec.y + 3, (Color){234, 238, 242, 255});
    DrawRectangleLinesEx((Rectangle){doc2Rec.x + 48, doc2Rec.y + 2, 58, 9}, 1.0f, (Color){130, 138, 148, 255});
    DrawGameText("PONTO 14:15", (int)doc2Rec.x + 15, (int)doc2Rec.y + 22, 12, (Color){25, 125, 52, 255});
    DrawGameText("CRACHÁ #0842", (int)doc2Rec.x + 15, (int)doc2Rec.y + 38, 11, INK_COLOR);
    DrawRectangleRounded((Rectangle){doc2Rec.x + 15, doc2Rec.y + 52, 72, 17}, 0.25f, 4, Fade((Color){25, 135, 55, 255}, 0.16f));
    DrawGameText("[DOC 3]", (int)doc2Rec.x + 27, (int)doc2Rec.y + 55, 11, (Color){25, 125, 52, 255});

    /* ===================================================================== */
    /* 9. CADEIRA EXECUTIVA ERGONOMICA EM PRIMEIRO PLANO                     */
    /* ===================================================================== */
    DrawEllipse(615, 640, 64, 11, Fade(BLACK, 0.30f));
    /* Base aranha cromada de 5 pontas com rodizios */
    DrawLineEx((Vector2){615, 620}, (Vector2){580, 636}, 4.0f, (Color){145, 152, 162, 255});
    DrawCircle(578, 637, 3.5f, (Color){42, 46, 52, 255});
    DrawLineEx((Vector2){615, 620}, (Vector2){615, 640}, 4.0f, (Color){160, 168, 178, 255});
    DrawCircle(615, 641, 3.5f, (Color){42, 46, 52, 255});
    DrawLineEx((Vector2){615, 620}, (Vector2){650, 636}, 4.0f, (Color){145, 152, 162, 255});
    DrawCircle(652, 637, 3.5f, (Color){42, 46, 52, 255});
    DrawLineEx((Vector2){615, 620}, (Vector2){592, 612}, 3.5f, (Color){125, 132, 142, 255});
    DrawLineEx((Vector2){615, 620}, (Vector2){638, 612}, 3.5f, (Color){125, 132, 142, 255});
    DrawCircle(615, 620, 6.0f, (Color){175, 182, 192, 255});
    DrawCircle(614, 619, 2.5f, (Color){220, 226, 232, 255});
    DrawPixel(577, 636, (Color){110, 116, 124, 255});
    DrawPixel(614, 640, (Color){110, 116, 124, 255});
    DrawPixel(651, 636, (Color){110, 116, 124, 255});

    /* Pistao a gas e assento estofado */
    DrawRectangle(610, 576, 10, 44, (Color){90, 96, 106, 255});
    DrawLine(612, 576, 612, 619, (Color){160, 168, 178, 255});
    DrawRectangleRounded((Rectangle){568, 562, 94, 18}, 0.4f, 6, (Color){54, 64, 76, 255});
    DrawRectangleRounded((Rectangle){574, 564, 82, 5}, 0.5f, 4, (Color){74, 86, 100, 255});
    DrawRectangleRoundedLinesEx((Rectangle){568, 562, 94, 18}, 0.4f, 6, 1.5f, (Color){82, 94, 108, 255});

    /* Bracos da cadeira executiva */
    DrawLineEx((Vector2){570, 568}, (Vector2){558, 540}, 4.0f, (Color){135, 142, 152, 255});
    DrawRectangleRounded((Rectangle){548, 534, 22, 8}, 0.4f, 4, (Color){44, 52, 62, 255});
    DrawLineEx((Vector2){660, 568}, (Vector2){672, 540}, 4.0f, (Color){135, 142, 152, 255});
    DrawRectangleRounded((Rectangle){660, 534, 22, 8}, 0.4f, 4, (Color){44, 52, 62, 255});

    /* Encosto ergonomico com estrutura prateada e apoio de cabeca */
    Rectangle chairBack = {570, 486, 90, 78};
    DrawRectangleRounded(chairBack, 0.25f, 8, (Color){62, 74, 88, 255});
    /* Tela de malha respiravel do encosto (2008) */
    for (int mh = 0; mh < 9; mh++)
    {
        DrawLine((int)chairBack.x + 6 + mh * 9, (int)chairBack.y + 5, (int)chairBack.x + 6 + mh * 9, (int)(chairBack.y + chairBack.height - 6), Fade((Color){42, 52, 64, 255}, 0.8f));
    }
    for (int mv = 0; mv < 8; mv++)
    {
        DrawLine((int)chairBack.x + 5, (int)chairBack.y + 7 + mv * 9, (int)(chairBack.x + chairBack.width - 6), (int)chairBack.y + 7 + mv * 9, Fade((Color){80, 94, 110, 255}, 0.55f));
    }
    DrawRectangleGradientH((int)chairBack.x + 4, (int)chairBack.y + 6, 16, (int)chairBack.height - 12, Fade((Color){140, 156, 174, 255}, 0.30f), Fade((Color){140, 156, 174, 255}, 0.0f));
    DrawRectangleRoundedLinesEx(chairBack, 0.25f, 8, 2.0f, (Color){96, 110, 126, 255});
    DrawRectangleRounded((Rectangle){609, 490, 12, 74}, 0.35f, 4, (Color){148, 156, 166, 255});
    DrawRectangleRounded((Rectangle){582, 522, 66, 13}, 0.3f, 4, (Color){48, 58, 70, 255});
    DrawRectangle(611, 478, 8, 9, (Color){135, 142, 152, 255});
    Rectangle chairHead = {588, 464, 54, 16};
    DrawRectangleRounded(chairHead, 0.45f, 6, (Color){54, 64, 76, 255});
    DrawLine((int)chairHead.x + 6, (int)chairHead.y + 3, (int)(chairHead.x + chairHead.width - 7), (int)chairHead.y + 3, (Color){96, 110, 126, 255});
    DrawRectangleRoundedLinesEx(chairHead, 0.45f, 6, 1.5f, (Color){88, 102, 118, 255});

    /* ===================================================================== */
    /* 10. BOTOES DE NAVEGACAO E HOTSPOTS INTERATIVOS                        */
    /* ===================================================================== */
    Rectangle btnVoltar1990 = {70, 600, 240, 42};
    DrawButton(btnVoltar1990, "< SALA 1 (1990)", (Color){70, 95, 110, 255});

    Rectangle btnGo2026 = {SCREEN_WIDTH - 280, 600, 240, 42};
    if (game->year2026Unlocked)
    {
        DrawButton(btnGo2026, "SALA 3 (2026) >", (Color){45, 115, 75, 255});
    }

    DrawHotspot(flyerRec, "Inspecionar Panfleto da Revolta [Doc 2]");
    DrawHotspot(cctvMonitor, "Analisar Monitor CFTV dos Servidores [Doc 4]");
    DrawHotspot(monitorRec, "Acessar Terminal de Vigilância da IA [COMPUTADOR]");
    DrawHotspot(doc0Rec, "Examinar Foto e Comparação Facial do Drone [Doc 1]");
    DrawHotspot(doc2Rec, "Verificar Registro de Ponto na Catraca [Doc 3]");
    DrawHotspot(fatherBookRec, "Ler Notas do Dr. Ramos (Origem Humanitária)");
    DrawHotspot(govMemoRec, "Ler Memorando Governamental (Desvio de Uso)");
    DrawHotspot(btnVoltar1990, "Retornar ao Setor de Arquivos (1990)");
    if (game->year2026Unlocked)
    {
        DrawHotspot(btnGo2026, "Avançar para a Redação Digital de 2026");
    }
}

void DrawHudRoom2008(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawHudYearOperator("2008", game->playerName, 24);
    DrawHudLivesTimer(game->lives, minutes * 60 + seconds);

    /* Rodape */
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawGameText("Investigue os documentos da sala e use o computador para contestar a ordem.", 28, 675, 16, (Color){165, 180, 195, 255});

    /* Botao de Audio */
    Rectangle audioBtn = {840, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawGameText(game->musicMuted ? "[M] ÁUDIO: MUDO" : "[M] ÁUDIO: ON",
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
    Rectangle flyerRec      = {102, 192, 148, 128};
    Rectangle cctvMonitor   = {736, 132, 196, 140};
    Rectangle monitorRec    = {490, 226, 250, 182};
    Rectangle doc0Rec       = {284, 418, 162, 76};
    Rectangle doc2Rec       = {794, 418, 154, 76};
    Rectangle fatherBookRec = {992, 410, 168, 68};
    Rectangle govMemoRec    = {992, 492, 168, 68};
    Rectangle btnVoltar1990 = {70, 600, 240, 42};
    if (UpdateJournal(game)) return;

    if (game->activeDocument2008 >= 0)
    {
        UpdateDocumentViewer2008(game);
        return;
    }

    /* Celular de flip escondido no vaso (sincronizado com DrawRoom2008) */
    Rectangle flipPhoneRec  = {221, 442, 22, 30};

    if (!game->hiddenFound[HIDDEN_2008] && Clicked(flipPhoneRec))
    {
        CollectHidden(game, HIDDEN_2008);
    }
    else if (Clicked(monitorRec))
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
        game->screen = SCREEN_ROOM_2026;
        SetMessage(game, "Laboratório de Síntese A.R.1.3.L. // ANO 2026.");
    }
}

void DrawPuzzle2008(const GameState *game)
{
    float time = (float)GetTime();

    /* Desenha a sala corporativa clara ao fundo com leve vinheta para destacar o monitor */
    DrawRoom2008(game);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade((Color){18, 26, 38, 255}, 0.52f));

    /* ===================================================================== */
    /* 1. MOLDURA FISICA DO MONITOR LCD CORPORATIVO PRATEADO (SYNCMASTER)    */
    /* ===================================================================== */
    Rectangle mon = {70, 52, 1140, 648};
    Rectangle casing = {mon.x - 10, mon.y - 8, mon.width + 20, mon.height + 20};

    /* Sombra projetada e carcaca metalica prateada 3D */
    DrawRectangleRounded((Rectangle){casing.x + 6, casing.y + 8, casing.width, casing.height}, 0.03f, 8, (Color){14, 20, 28, 255});
    DrawRectangleRounded(casing, 0.03f, 8, (Color){192, 198, 208, 255});
    /* Chanfro iluminado superior/esquerdo e sombra inferior/direita */
    DrawRectangleRoundedLinesEx(casing, 0.03f, 8, 3.0f, (Color){230, 235, 244, 255});
    DrawRectangleRoundedLinesEx((Rectangle){casing.x + 2, casing.y + 2, casing.width - 4, casing.height - 4}, 0.03f, 8, 2.0f, (Color){128, 136, 148, 255});

    /* Rebaixo escuro da tela LCD */
    DrawRectangleRounded((Rectangle){mon.x - 2, mon.y - 2, mon.width + 4, mon.height - 6}, 0.02f, 6, (Color){68, 76, 88, 255});

    /* Fundo claro corporativo do Sistema Operacional (Prata-Perola / Gelo) */
    Rectangle screenArea = {mon.x, mon.y, mon.width, mon.height - 10};
    DrawRectangleRounded(screenArea, 0.015f, 6, (Color){222, 228, 238, 255});

    /* Textura de grade tecnica sutil no papel de parede corporativo */
    for (int gx = (int)screenArea.x + 24; gx < (int)(screenArea.x + screenArea.width); gx += 28)
        DrawLine(gx, (int)screenArea.y + 40, gx, (int)(screenArea.y + screenArea.height - 4), Fade((Color){182, 194, 210, 255}, 0.45f));
    for (int gy = (int)screenArea.y + 56; gy < (int)(screenArea.y + screenArea.height); gy += 28)
        DrawLine((int)screenArea.x + 4, gy, (int)(screenArea.x + screenArea.width - 4), gy, Fade((Color){182, 194, 210, 255}, 0.45f));

    /* Detalhes fisicos na borda inferior do monitor (botoes OSD e LED) */
    for (int b = 0; b < 4; b++)
    {
        DrawRectangleRounded((Rectangle){mon.x + mon.width - 140 + b * 22, mon.y + mon.height - 7, 14, 5}, 0.4f, 4, (Color){115, 124, 136, 255});
    }
    DrawCircle((int)(mon.x + mon.width - 36), (int)(mon.y + mon.height - 4), 4.0f, (Color){45, 225, 95, 255});
    DrawCircle((int)(mon.x + mon.width - 36), (int)(mon.y + mon.height - 4), 2.0f, (Color){190, 255, 210, 255});

    /* ===================================================================== */
    /* 2. BARRA DE TITULO DO SISTEMA (ESTILO AERO CORPORATIVO 2008)          */
    /* ===================================================================== */
    Rectangle osHeader = {mon.x + 6, mon.y + 6, mon.width - 12, 34};
    DrawRectangleRounded(osHeader, 0.14f, 6, (Color){34, 82, 142, 255});
    /* Reflexo vitrificado superior */
    DrawRectangleRounded((Rectangle){osHeader.x + 2, osHeader.y + 2, osHeader.width - 4, 15}, 0.14f, 6, Fade(RAYWHITE, 0.24f));
    DrawRectangleRoundedLinesEx(osHeader, 0.14f, 6, 1.5f, (Color){20, 52, 96, 255});

    /* Icone de escudo corporativo + titulo */
    DrawCircle((int)osHeader.x + 20, (int)osHeader.y + 17, 7.0f, GOLD_COLOR);
    DrawCircle((int)osHeader.x + 20, (int)osHeader.y + 17, 4.0f, (Color){34, 82, 142, 255});
    DrawGameText("SISTEMA NACIONAL DE VIGILÂNCIA BIOMÉTRICA // NÓ 2008", (int)osHeader.x + 36, (int)osHeader.y + 9, 15, RAYWHITE);

    /* Botoes classicos de janela */
    for (int wb = 0; wb < 3; wb++)
    {
        Rectangle winBtn = {osHeader.x + osHeader.width - 82 + wb * 25, osHeader.y + 6, 22, 22};
        Color wbCol = (wb == 2) ? (Color){198, 58, 48, 255} : (Color){58, 108, 168, 255};
        DrawRectangleRounded(winBtn, 0.25f, 4, wbCol);
        DrawRectangleRoundedLinesEx(winBtn, 0.25f, 4, 1.0f, Fade(RAYWHITE, 0.65f));
        const char *sym = (wb == 0) ? "_" : ((wb == 1) ? "O" : "X");
        DrawGameText(sym, (int)winBtn.x + 7, (int)winBtn.y + 4, 12, RAYWHITE);
    }

    /* ===================================================================== */
    /* 3. BARRA DE FERRAMENTAS METALICA E ABAS DE PASTA                      */
    /* ===================================================================== */
    Rectangle toolStrip = {mon.x + 6, mon.y + 42, mon.width - 12, 46};
    DrawRectangle(toolStrip.x, toolStrip.y, toolStrip.width, toolStrip.height, (Color){202, 210, 222, 255});
    DrawLine((int)toolStrip.x, (int)(toolStrip.y + toolStrip.height), (int)(toolStrip.x + toolStrip.width), (int)(toolStrip.y + toolStrip.height), (Color){154, 166, 182, 255});

    Rectangle tab0 = {mon.x + 18, mon.y + 46, 320, 38};
    Rectangle tab1 = {mon.x + 348, mon.y + 46, 360, 38};
    Rectangle btnClose = {mon.x + mon.width - 248, mon.y + 46, 230, 38};

    bool isTab0 = (game->terminal2008ActiveTab == 0);

    /* Aba 0 */
    DrawRectangleRounded(tab0, 0.18f, 6, isTab0 ? (Color){248, 250, 254, 255} : (Color){178, 188, 204, 255});
    if (isTab0)
    {
        DrawRectangleRounded((Rectangle){tab0.x + 2, tab0.y + 2, tab0.width - 4, 5}, 0.2f, 4, (Color){208, 52, 44, 255});
    }
    DrawRectangleRoundedLinesEx(tab0, 0.18f, 6, isTab0 ? 2.0f : 1.2f, isTab0 ? (Color){52, 92, 142, 255} : (Color){138, 148, 165, 255});
    DrawGameText("1. ORDEM DE ALVO PENDENTE", (int)tab0.x + 26, (int)tab0.y + 12, 15, isTab0 ? (Color){24, 36, 52, 255} : (Color){68, 78, 92, 255});

    /* Aba 1 */
    DrawRectangleRounded(tab1, 0.18f, 6, !isTab0 ? (Color){248, 250, 254, 255} : (Color){178, 188, 204, 255});
    if (!isTab0)
    {
        DrawRectangleRounded((Rectangle){tab1.x + 2, tab1.y + 2, tab1.width - 4, 5}, 0.2f, 4, (Color){38, 102, 172, 255});
    }
    DrawRectangleRoundedLinesEx(tab1, 0.18f, 6, !isTab0 ? 2.0f : 1.2f, !isTab0 ? (Color){52, 92, 142, 255} : (Color){138, 148, 165, 255});
    if (game->room2008OptionalUnlocked)
    {
        DrawGameText(game->room2008OptionalCompleted ? "2. CONFIGURAÇÃO [DESATIVADA]" : "2. CONFIGURAÇÃO DA INSTALAÇÃO",
                     (int)tab1.x + 22, (int)tab1.y + 12, 15, !isTab0 ? (Color){24, 36, 52, 255} : (Color){24, 125, 58, 255});
    }
    else
    {
        DrawGameText("2. CONFIGURAÇÃO [BLOQUEADA]", (int)tab1.x + 24, (int)tab1.y + 12, 15, (Color){88, 98, 112, 255});
    }

    DrawCorpButton2008(btnClose, "< VOLTAR À SALA [ESC]", (Color){196, 54, 46, 255}, true);

    if (game->terminal2008ActiveTab == 0)
    {
        /* ========================================================= */
        /* ABA 1: ORDEM DE ALVO PENDENTE & SELECAO DE EVIDENCIAS     */
        /* ========================================================= */

        /* 4. PAINEL SUPERIOR DO ALVO (Ficha Corporativa Clara e Detalhada) */
        Rectangle targetPanel = {mon.x + 18, mon.y + 92, mon.width - 36, 94};
        Color tAccent = game->puzzle2008Contested ? (Color){34, 152, 78, 255} : (Color){202, 48, 42, 255};
        Color tBg = game->puzzle2008Contested ? (Color){236, 250, 240, 255} : (Color){252, 245, 244, 255};

        DrawRectangleRounded((Rectangle){targetPanel.x + 3, targetPanel.y + 4, targetPanel.width, targetPanel.height}, 0.06f, 6, Fade(BLACK, 0.12f));
        DrawRectangleRounded(targetPanel, 0.06f, 6, tBg);
        /* Faixa lateral de alerta */
        DrawRectangleRounded((Rectangle){targetPanel.x + 2, targetPanel.y + 2, 10, targetPanel.height - 4}, 0.2f, 4, tAccent);
        DrawRectangleRoundedLinesEx(targetPanel, 0.06f, 6, 2.0f, tAccent);

        /* Retrato biometrico detalhado a esquerda */
        Rectangle radarBox = {targetPanel.x + 20, targetPanel.y + 10, 74, 74};
        DrawRectangleRounded(radarBox, 0.10f, 4, (Color){218, 228, 240, 255});
        DrawRectangleRoundedLinesEx(radarBox, 0.10f, 4, 1.8f, tAccent);
        /* Silhueta corporativa detalhada (camisa, gravata, rosto, cabelo) */
        DrawRectangleRounded((Rectangle){radarBox.x + 16, radarBox.y + 46, 42, 24}, 0.25f, 4, (Color){52, 72, 98, 255});
        DrawTriangle((Vector2){radarBox.x + 37, radarBox.y + 48}, (Vector2){radarBox.x + 32, radarBox.y + 68}, (Vector2){radarBox.x + 42, radarBox.y + 68}, (Color){185, 42, 38, 255});
        DrawCircle((int)radarBox.x + 37, (int)radarBox.y + 29, 14, (Color){224, 182, 152, 255});
        DrawRectangleRounded((Rectangle){radarBox.x + 23, radarBox.y + 14, 28, 10}, 0.4f, 4, (Color){54, 42, 34, 255});
        /* Mira biometrica sobre o retrato */
        DrawRectangleLinesEx((Rectangle){radarBox.x + 19, radarBox.y + 12, 36, 36}, 1.2f, Fade(tAccent, 0.85f));
        DrawLine((int)radarBox.x + 37, (int)radarBox.y + 6, (int)radarBox.x + 37, (int)radarBox.y + 68, Fade(tAccent, 0.40f));
        DrawLine((int)radarBox.x + 6, (int)radarBox.y + 30, (int)radarBox.x + 68, (int)radarBox.y + 30, Fade(tAccent, 0.40f));

        if (!game->puzzle2008Contested)
        {
            float blink = 0.75f + 0.25f * sinf(time * 5.5f);
            DrawGameText("[ ! ] ORDEM #8491 — INTERCEPTAÇÃO POR DRONE PROGRAMADA", (int)targetPanel.x + 106, (int)targetPanel.y + 12, 17, Fade((Color){192, 32, 28, 255}, blink));

            /* 3 Cartoes de dados em alto contraste */
            Rectangle b1 = {targetPanel.x + 106, targetPanel.y + 42, 214, 38};
            Rectangle b2 = {targetPanel.x + 330, targetPanel.y + 42, 266, 38};
            Rectangle b3 = {targetPanel.x + 606, targetPanel.y + 42, 228, 38};

            DrawRectangleRounded(b1, 0.20f, 6, (Color){232, 238, 248, 255});
            DrawRectangleRounded((Rectangle){b1.x + 2, b1.y + 2, 6, b1.height - 4}, 0.3f, 4, (Color){42, 96, 162, 255});
            DrawRectangleRoundedLinesEx(b1, 0.20f, 6, 1.5f, (Color){125, 152, 188, 255});
            DrawGameText("ALVO: LUCAS SILVA", (int)b1.x + 16, (int)b1.y + 11, 15, (Color){22, 34, 52, 255});

            DrawRectangleRounded(b2, 0.20f, 6, (Color){254, 244, 218, 255});
            DrawRectangleRounded((Rectangle){b2.x + 2, b2.y + 2, 6, b2.height - 4}, 0.3f, 4, (Color){215, 132, 18, 255});
            DrawRectangleRoundedLinesEx(b2, 0.20f, 6, 1.5f, (Color){215, 158, 65, 255});
            DrawGameText("ACUSAÇÃO: PRAÇA (14:30)", (int)b2.x + 16, (int)b2.y + 11, 15, (Color){145, 78, 8, 255});

            DrawRectangleRounded(b3, 0.20f, 6, (Color){254, 230, 230, 255});
            DrawRectangleRounded((Rectangle){b3.x + 2, b3.y + 2, 6, b3.height - 4}, 0.3f, 4, (Color){202, 48, 42, 255});
            DrawRectangleRoundedLinesEx(b3, 0.20f, 6, 1.5f, (Color){215, 115, 110, 255});
            DrawGameText("IA DRONE: 78% (INCERTO)", (int)b3.x + 16, (int)b3.y + 11, 15, (Color){172, 28, 24, 255});

            /* Nota adesiva / Box de Objetivo a direita */
            Rectangle objBox = {targetPanel.x + 846, targetPanel.y + 10, 244, 74};
            DrawRectangleRounded((Rectangle){objBox.x + 2, objBox.y + 3, objBox.width, objBox.height}, 0.14f, 6, Fade(BLACK, 0.10f));
            DrawRectangleRounded(objBox, 0.14f, 6, (Color){255, 246, 206, 255});
            DrawRectangleRoundedLinesEx(objBox, 0.14f, 6, 1.8f, (Color){212, 158, 42, 255});
            DrawCircle((int)(objBox.x + objBox.width - 14), (int)objBox.y + 12, 4.5f, (Color){205, 52, 42, 255});
            DrawGameText("OBJETIVO:", (int)objBox.x + 14, (int)objBox.y + 9, 13, (Color){175, 78, 12, 255});
            DrawGameText("Examine os docs e marque", (int)objBox.x + 14, (int)objBox.y + 29, 13, (Color){35, 45, 58, 255});
            DrawGameText("2 provas do álibi (14:30).", (int)objBox.x + 14, (int)objBox.y + 49, 13, (Color){24, 82, 148, 255});
        }
        else
        {
            DrawGameText("[ OK ] FALSO POSITIVO COMPROVADO — IDENTIFICAÇÃO ANULADA", (int)targetPanel.x + 106, (int)targetPanel.y + 16, 18, (Color){24, 138, 62, 255});
            DrawGameText("Álibi confirmado: Lucas Silva estava dentro da sede da empresa às 14:30.", (int)targetPanel.x + 106, (int)targetPanel.y + 48, 15, (Color){32, 44, 58, 255});

            if (!game->puzzle2008OrderSuspended)
            {
                Rectangle btnSusp = {targetPanel.x + targetPanel.width - 365, targetPanel.y + 23, 350, 48};
                DrawCorpButton2008(btnSusp, "> SUSPENDER ORDEM DE DRONE <", (Color){36, 158, 82, 255}, true);
            }
            else
            {
                Rectangle btnDone = {targetPanel.x + targetPanel.width - 435, targetPanel.y + 23, 420, 48};
                DrawCorpButton2008(btnDone, "COLETAR CD E CONCLUIR >> [ENTER]", (Color){215, 134, 18, 255}, true);
            }
        }

        /* ========================================================= */
        /* 5. GRADE 2x2 DE PASTAS/CARDS DE EVIDENCIAS CLAROS         */
        /* ========================================================= */
        const char *cardTitles[4] = {
            "1. COMPARAÇÃO FACIAL (DRONE)",
            "2. PANFLETO DA REVOLTA",
            "3. REGISTRO DA CATRACA",
            "4. CÂMERA INTERNA CFTV"
        };
        const char *cardSources[4] = {
            "FONTE: DRONE ALPHA-4",
            "FONTE: PRAÇA CENTRAL",
            "FONTE: PORTARIA DA SEDE",
            "FONTE: SUBSOLO (SERVIDORES)"
        };
        bool docExamined[4] = {
            game->doc2008FaceExamined,
            game->doc2008FlyerExamined,
            game->doc2008EntryExamined,
            game->doc2008CameraExamined
        };

        for (int c = 0; c < 4; c++)
        {
            int col = c % 2;
            int row = c / 2;
            Rectangle cRec = {mon.x + 18 + col * 556, mon.y + 198 + row * 160, 548, 148};
            bool isSel = game->puzzle2008Selected[c];
            bool hoverCard = CheckCollisionPointRec(GetVirtualMouse(), cRec);

            /* Sombra projetada do card */
            DrawRectangleRounded((Rectangle){cRec.x + 4, cRec.y + 5, cRec.width, cRec.height}, 0.06f, 6, Fade(BLACK, 0.14f));

            /* Corpo claro do card (Branco-Perola ou Verde-Menta suave quando selecionado) */
            Color cBg = isSel ? (Color){232, 248, 236, 255} : (hoverCard ? (Color){252, 253, 255, 255} : (Color){245, 248, 252, 255});
            Color cBorder = isSel ? (Color){36, 158, 82, 255} : (hoverCard ? (Color){48, 108, 178, 255} : (Color){156, 170, 190, 255});
            Color hdrCol = isSel ? (Color){32, 145, 76, 255} : (hoverCard ? (Color){42, 96, 158, 255} : (Color){52, 78, 112, 255});

            DrawRectangleRounded(cRec, 0.06f, 6, cBg);

            /* Faixa de cabecalho colorida no topo do card */
            Rectangle cardHdr = {cRec.x + 2, cRec.y + 2, cRec.width - 4, 34};
            DrawRectangleRounded(cardHdr, 0.12f, 6, hdrCol);
            DrawRectangle((int)cardHdr.x, (int)(cardHdr.y + 18), (int)cardHdr.width, 16, hdrCol);
            DrawLine((int)cardHdr.x + 4, (int)cardHdr.y + 3, (int)(cardHdr.x + cardHdr.width - 4), (int)cardHdr.y + 3, Fade(RAYWHITE, 0.25f));

            DrawRectangleRoundedLinesEx(cRec, 0.06f, 6, isSel ? 2.8f : 1.8f, cBorder);

            /* Clipe/marcador no cabecalho + Titulo em branco nitido */
            DrawRectangleRounded((Rectangle){cRec.x + 14, cRec.y + 9, 8, 18}, 0.4f, 4, isSel ? (Color){140, 255, 175, 255} : GOLD_COLOR);
            DrawGameText(cardTitles[c], (int)cRec.x + 30, (int)cRec.y + 9, 16, RAYWHITE);

            /* Moldura da miniatura visual do documento (abaixo do cabecalho) */
            Rectangle thumb = {cRec.x + 14, cRec.y + 42, 114, 96};
            DrawRectangleRounded((Rectangle){thumb.x + 2, thumb.y + 2, thumb.width, thumb.height}, 0.08f, 6, Fade(BLACK, 0.12f));
            DrawRectangleRounded(thumb, 0.08f, 6, (Color){216, 224, 236, 255});
            DrawRectangleRoundedLinesEx(thumb, 0.08f, 6, 1.5f, isSel ? (Color){42, 162, 86, 255} : (Color){138, 152, 172, 255});

            if (c == 0)
            {
                /* Thumbnail Doc 1: Foto Aerea do Drone + Mira Biometrica */
                Rectangle inner = {thumb.x + 4, thumb.y + 4, thumb.width - 8, thumb.height - 8};
                DrawRectangleRounded(inner, 0.06f, 4, (Color){28, 40, 54, 255});
                /* Faixas de pedestre na praca ao fundo */
                for (int s = 0; s < 4; s++)
                    DrawRectangle((int)inner.x + 8 + s * 24, (int)inner.y + 62, 14, 5, Fade(LIGHTGRAY, 0.22f));
                DrawCircle((int)thumb.x + 57, (int)thumb.y + 38, 16, (Color){125, 162, 188, 255});
                DrawRectangleRounded((Rectangle){thumb.x + 37, thumb.y + 54, 40, 20}, 0.3f, 4, (Color){88, 122, 148, 255});
                DrawRectangleLinesEx((Rectangle){thumb.x + 33, thumb.y + 16, 48, 46}, 1.5f, RED_COLOR);
                DrawRectangle((int)inner.x, (int)(inner.y + inner.height - 20), (int)inner.width, 20, (Color){192, 42, 36, 255});
                DrawGameText("IA: 78%", (int)thumb.x + 30, (int)(thumb.y + thumb.height - 21), 13, RAYWHITE);
            }
            else if (c == 1)
            {
                /* Thumbnail Doc 2: Panfleto sob cortica clara */
                Rectangle inner = {thumb.x + 4, thumb.y + 4, thumb.width - 8, thumb.height - 8};
                DrawRectangleRounded(inner, 0.06f, 4, (Color){198, 158, 112, 255});
                Rectangle paper = {thumb.x + 18, thumb.y + 8, 78, 80};
                DrawRectangleRounded(paper, 0.06f, 4, (Color){252, 246, 232, 255});
                DrawRectangleLinesEx(paper, 1.0f, (Color){168, 148, 118, 255});
                DrawRectangle((int)paper.x + 6, (int)paper.y + 8, (int)paper.width - 12, 13, (Color){192, 45, 36, 255});
                DrawRectangle((int)paper.x + 10, (int)paper.y + 28, (int)paper.width - 20, 4, (Color){45, 52, 62, 255});
                DrawRectangle((int)paper.x + 10, (int)paper.y + 38, (int)paper.width - 26, 4, (Color){45, 52, 62, 255});
                DrawRectangle((int)paper.x + 10, (int)paper.y + 50, (int)paper.width - 16, 5, (Color){165, 38, 30, 255});
                DrawRectangle((int)paper.x + 10, (int)paper.y + 63, (int)paper.width - 24, 4, (Color){95, 85, 70, 255});
                DrawCircle((int)paper.x + 39, (int)paper.y + 4, 4.5f, (Color){215, 45, 38, 255});
            }
            else if (c == 2)
            {
                /* Thumbnail Doc 3: Cracha / Registro de Ponto */
                Rectangle inner = {thumb.x + 4, thumb.y + 4, thumb.width - 8, thumb.height - 8};
                DrawRectangleRounded(inner, 0.06f, 4, (Color){188, 204, 224, 255});
                Rectangle badge = {thumb.x + 14, thumb.y + 8, 86, 80};
                DrawRectangleRounded(badge, 0.08f, 4, RAYWHITE);
                DrawRectangleLinesEx(badge, 1.0f, (Color){120, 142, 168, 255});
                DrawRectangle((int)badge.x + 5, (int)badge.y + 5, (int)badge.width - 10, 13, (Color){34, 76, 128, 255});
                DrawCircle((int)badge.x + 18, (int)badge.y + 30, 7.0f, (Color){95, 125, 158, 255});
                DrawRectangle((int)badge.x + 30, (int)badge.y + 24, 44, 5, (Color){45, 55, 68, 255});
                DrawRectangle((int)badge.x + 30, (int)badge.y + 33, 34, 4, (Color){115, 125, 138, 255});
                DrawRectangle((int)badge.x + 7, (int)badge.y + 46, (int)badge.width - 14, 24, (Color){222, 246, 228, 255});
                DrawRectangleLines((int)badge.x + 7, (int)badge.y + 46, (int)badge.width - 14, 24, (Color){38, 152, 72, 255});
                DrawGameText("14:15", (int)badge.x + 22, (int)badge.y + 51, 14, (Color){22, 122, 50, 255});
            }
            else
            {
                /* Thumbnail Doc 4: Monitor CFTV dos Servidores */
                Rectangle cam = {thumb.x + 6, thumb.y + 6, 102, 84};
                DrawRectangleRounded(cam, 0.08f, 4, (Color){12, 32, 20, 255});
                DrawRectangleRoundedLinesEx(cam, 0.08f, 4, 2.0f, (Color){45, 178, 88, 255});
                DrawCircle((int)cam.x + 12, (int)cam.y + 12, 4.0f, (Color){235, 52, 42, 255});
                DrawGameText("REC 14:30", (int)cam.x + 20, (int)cam.y + 6, 11, (Color){95, 248, 135, 255});
                /* Rack de servidores com LEDs + silhueta de Lucas */
                DrawRectangle((int)cam.x + 62, (int)cam.y + 22, 28, 54, (Color){22, 58, 36, 255});
                for (int l = 0; l < 4; l++)
                    DrawCircle((int)cam.x + 68, (int)cam.y + 28 + l * 12, 2.0f, (Color){95, 250, 140, 255});
                DrawCircle((int)cam.x + 36, (int)cam.y + 40, 10, (Color){72, 165, 102, 255});
                DrawRectangleRounded((Rectangle){cam.x + 25, cam.y + 51, 22, 25}, 0.25f, 4, (Color){52, 132, 78, 255});
            }

            /* Tags limpas de Fonte e Status (em fundo claro de alto contraste) */
            int rx = (int)cRec.x + 142;
            Rectangle srcTag = {(float)rx, cRec.y + 46, 234, 30};
            DrawRectangleRounded(srcTag, 0.25f, 4, (Color){226, 233, 244, 255});
            DrawRectangleRoundedLinesEx(srcTag, 0.25f, 4, 1.2f, (Color){148, 164, 186, 255});
            DrawCircle((int)srcTag.x + 12, (int)srcTag.y + 15, 3.5f, (Color){42, 98, 165, 255});
            DrawGameText(cardSources[c], (int)srcTag.x + 22, (int)srcTag.y + 8, 13, (Color){32, 44, 62, 255});

            Rectangle stTag = {(float)(rx + 242), cRec.y + 46, 150, 30};
            if (isSel)
            {
                DrawRectangleRounded(stTag, 0.25f, 4, (Color){204, 242, 216, 255});
                DrawRectangleRoundedLinesEx(stTag, 0.25f, 4, 1.5f, (Color){36, 158, 82, 255});
                DrawGameText("[ SELECIONADA ]", (int)stTag.x + 15, (int)stTag.y + 8, 13, (Color){18, 118, 52, 255});
            }
            else if (docExamined[c])
            {
                DrawRectangleRounded(stTag, 0.25f, 4, (Color){214, 235, 252, 255});
                DrawRectangleRoundedLinesEx(stTag, 0.25f, 4, 1.5f, (Color){52, 122, 192, 255});
                DrawGameText("[ JÁ EXAMINADO ]", (int)stTag.x + 15, (int)stTag.y + 8, 13, (Color){24, 82, 148, 255});
            }
            else
            {
                DrawRectangleRounded(stTag, 0.25f, 4, (Color){254, 242, 212, 255});
                DrawRectangleRoundedLinesEx(stTag, 0.25f, 4, 1.5f, (Color){212, 152, 42, 255});
                DrawGameText("[ NÃO LIDO ]", (int)stTag.x + 27, (int)stTag.y + 8, 13, (Color){155, 88, 12, 255});
            }

            /* Botoes 3D Corporativos Coloridos (Adeus azul escuro sem detalhe!) */
            Rectangle btnExam = {(float)rx, cRec.y + 88, 186, 44};
            DrawCorpButton2008(btnExam, "EXAMINAR DOC", (Color){42, 108, 178, 255}, true);

            Rectangle btnSel = {(float)(rx + 198), cRec.y + 88, 194, 44};
            DrawCorpButton2008(btnSel, isSel ? "REMOVER" : "SELECIONAR",
                               isSel ? (Color){196, 54, 46, 255} : (Color){38, 152, 82, 255}, true);
        }

        /* ========================================================= */
        /* 6. DOCK INFERIOR METALICO COM BANDEJAS DE PROVA E BOTAO   */
        /* ========================================================= */
        Rectangle actionBar = {mon.x + 18, mon.y + 526, mon.width - 36, 106};
        DrawRectangleRounded((Rectangle){actionBar.x + 3, actionBar.y + 4, actionBar.width, actionBar.height}, 0.06f, 6, Fade(BLACK, 0.14f));
        DrawRectangleRounded(actionBar, 0.06f, 6, (Color){204, 214, 226, 255});
        DrawLine((int)actionBar.x + 6, (int)actionBar.y + 2, (int)(actionBar.x + actionBar.width - 6), (int)actionBar.y + 2, Fade(RAYWHITE, 0.65f));
        DrawRectangleRoundedLinesEx(actionBar, 0.06f, 6, 2.0f, (Color){138, 152, 172, 255});

        int selIndices[2] = {-1, -1};
        int found = 0;
        for (int i = 0; i < 4 && found < 2; i++)
        {
            if (game->puzzle2008Selected[i]) selIndices[found++] = i;
        }

        const char *shortNames[4] = {
            "DOC 1: DRONE (78%)",
            "DOC 2: PANFLETO",
            "DOC 3: CATRACA (14:15)",
            "DOC 4: CFTV (14:30)"
        };

        for (int s = 0; s < 2; s++)
        {
            Rectangle slotRec = {actionBar.x + 16 + s * 320, actionBar.y + 12, 308, 46};
            int docIdx = selIndices[s];
            if (docIdx >= 0)
            {
                DrawRectangleRounded(slotRec, 0.18f, 6, (Color){222, 248, 230, 255});
                DrawRectangleRounded((Rectangle){slotRec.x + 2, slotRec.y + 2, 8, slotRec.height - 4}, 0.3f, 4, (Color){36, 158, 82, 255});
                DrawRectangleRoundedLinesEx(slotRec, 0.18f, 6, 2.0f, (Color){36, 158, 82, 255});
                DrawGameText(TextFormat("PROVA %d: %s", s + 1, shortNames[docIdx]), (int)slotRec.x + 18, (int)slotRec.y + 15, 15, (Color){18, 112, 48, 255});
            }
            else
            {
                DrawRectangleRounded(slotRec, 0.18f, 6, (Color){232, 238, 246, 255});
                DrawRectangleRoundedLinesEx(slotRec, 0.18f, 6, 1.5f, (Color){158, 170, 188, 255});
                DrawGameText(TextFormat("PROVA %d: [ VAZIO ]", s + 1), (int)slotRec.x + 68, (int)slotRec.y + 15, 15, (Color){102, 114, 130, 255});
            }
        }

        if (!game->puzzle2008Contested)
        {
            Rectangle btnContest = {actionBar.x + 662, actionBar.y + 12, 424, 46};
            bool canContest = (game->puzzle2008SelectionCount == 2);
            DrawCorpButton2008(btnContest, "CONTESTAR IDENTIFICAÇÃO",
                               canContest ? (Color){218, 132, 16, 255} : (Color){142, 152, 166, 255}, canContest);
        }
        else
        {
            Rectangle okBanner = {actionBar.x + 662, actionBar.y + 12, 424, 46};
            DrawRectangleRounded(okBanner, 0.18f, 6, (Color){216, 246, 226, 255});
            DrawRectangleRoundedLinesEx(okBanner, 0.18f, 6, 2.0f, (Color){36, 158, 82, 255});
            DrawGameText("[ OK ] CONTESTAÇÃO ACEITA PELO SISTEMA", (int)okBanner.x + 34, (int)okBanner.y + 15, 16, (Color){20, 122, 52, 255});
        }

        /* Faixa inferior clara de feedback / orientacao */
        Rectangle tipStrip = {actionBar.x + 16, actionBar.y + 64, actionBar.width - 32, 32};
        DrawRectangleRounded(tipStrip, 0.22f, 4, (Color){242, 246, 252, 255});
        DrawRectangleRoundedLinesEx(tipStrip, 0.22f, 4, 1.0f, (Color){168, 180, 198, 255});

        if (game->puzzle2008Feedback[0] != '\0')
        {
            Color fbCol = game->puzzle2008Contested ? (Color){20, 128, 55, 255} : (Color){185, 68, 16, 255};
            DrawGameText(game->puzzle2008Feedback, (int)tipStrip.x + 14, (int)tipStrip.y + 8, 15, fbCol);
        }
        else
        {
            DrawGameText("Clique em [EXAMINAR DOC] para inspecionar os detalhes e selecione as 2 provas do álibi.",
                         (int)tipStrip.x + 14, (int)tipStrip.y + 8, 15, (Color){48, 62, 82, 255});
        }
    }
    else
    {
        /* ========================================================= */
        /* ABA 2: CONFIGURACAO DA INSTALACAO (DESCOBERTA OPCIONAL)   */
        /* ========================================================= */
        Rectangle optPanel = {mon.x + 18, mon.y + 92, mon.width - 36, 540};
        DrawRectangleRounded((Rectangle){optPanel.x + 3, optPanel.y + 4, optPanel.width, optPanel.height}, 0.04f, 6, Fade(BLACK, 0.12f));
        DrawRectangleRounded(optPanel, 0.04f, 6, (Color){245, 248, 252, 255});
        DrawRectangleRoundedLinesEx(optPanel, 0.04f, 6, 2.0f, game->room2008OptionalUnlocked ? (Color){42, 152, 86, 255} : (Color){188, 62, 54, 255});

        if (!game->room2008OptionalUnlocked)
        {
            DrawPadlockIcon((int)(optPanel.x + optPanel.width / 2), (int)(optPanel.y + 125), 2.0f, (Color){205, 138, 24, 255}, (Color){42, 56, 74, 255});
            DrawGameText("ACESSO ADMINISTRATIVO RESTRITO // DIRETRIZES DA FILIAL", (int)optPanel.x + 230, (int)optPanel.y + 195, 20, (Color){185, 42, 36, 255});
            DrawGameText("Para acessar as configurações desta instalação, examine o Caderno do Dr. Ramos", (int)optPanel.x + 195, (int)optPanel.y + 240, 16, (Color){42, 54, 68, 255});
            DrawGameText("e o Memorando Governamental na bancada direita do escritório.", (int)optPanel.x + 255, (int)optPanel.y + 270, 16, (Color){42, 54, 68, 255});
            DrawGameText("Ambos os documentos juntos liberam a chave de manutenção local.", (int)optPanel.x + 255, (int)optPanel.y + 315, 15, (Color){168, 98, 14, 255});
        }
        else
        {
            DrawGameText("DIRETRIZES DA UNIDADE LOCAL // FILIAL METROPOLITANA 08", (int)optPanel.x + 35, (int)optPanel.y + 30, 20, (Color){28, 76, 138, 255});
            DrawLine((int)optPanel.x + 35, (int)optPanel.y + 60, (int)(optPanel.x + optPanel.width - 35), (int)optPanel.y + 60, (Color){155, 175, 202, 255});

            DrawGameText("STATUS DO SISTEMA: Conectado à rede regional de monitoramento", (int)optPanel.x + 35, (int)optPanel.y + 80, 16, (Color){32, 44, 58, 255});
            DrawGameText("REGISTROS FORENSES ARQUIVADOS: 1.420 ocorrências preservadas", (int)optPanel.x + 35, (int)optPanel.y + 110, 16, (Color){28, 98, 168, 255});

            Rectangle cardStatus = {optPanel.x + 35, optPanel.y + 145, optPanel.width - 70, 120};
            DrawRectangleRounded(cardStatus, 0.05f, 6, (Color){232, 238, 248, 255});
            DrawRectangleRoundedLinesEx(cardStatus, 0.05f, 6, 1.8f, (Color){145, 165, 192, 255});

            DrawGameText("ENVIO AUTOMÁTICO DE ALVOS DESTA INSTALAÇÃO:", (int)cardStatus.x + 20, (int)cardStatus.y + 20, 16, (Color){24, 36, 52, 255});
            if (!game->room2008OptionalCompleted)
            {
                DrawGameText("[ ATIVO ] - Drones são despachados automaticamente sob correspondência preliminar.", (int)cardStatus.x + 20, (int)cardStatus.y + 50, 15, (Color){188, 38, 32, 255});
                DrawGameText("Desativar o envio automático suspende os ataques sem apagar as provas forenses.", (int)cardStatus.x + 20, (int)cardStatus.y + 80, 15, (Color){52, 64, 80, 255});

                Rectangle btnDisable = {optPanel.x + 35, optPanel.y + 290, 520, 52};
                DrawCorpButton2008(btnDisable, "DESATIVAR ENVIO AUTOMÁTICO DESTA INSTALAÇÃO", (Color){215, 128, 22, 255}, true);
            }
            else
            {
                DrawGameText("[ DESATIVADO ] - O despacho automático de drones desta central foi bloqueado.", (int)cardStatus.x + 20, (int)cardStatus.y + 50, 15, (Color){24, 135, 62, 255});
                DrawGameText("Todos os 1.420 relatórios foram preservados intactos como prova contra o abuso.", (int)cardStatus.x + 20, (int)cardStatus.y + 80, 15, (Color){32, 118, 58, 255});

                Rectangle okBox = {optPanel.x + 35, optPanel.y + 290, optPanel.width - 70, 70};
                DrawRectangleRounded(okBox, 0.08f, 6, (Color){222, 246, 230, 255});
                DrawRectangleRoundedLinesEx(okBox, 0.08f, 6, 2.0f, (Color){38, 158, 82, 255});
                DrawGameText("[ V ] PROTOCOLO ÉTICO APLICADO: CENTRAL LOCAL PROTEGIDA COM SUCESSO!", (int)okBox.x + 25, (int)okBox.y + 25, 16, (Color){20, 122, 52, 255});
            }

            Rectangle cardInfo = {optPanel.x + 35, optPanel.y + 380, optPanel.width - 70, 120};
            DrawRectangleRounded(cardInfo, 0.05f, 6, (Color){254, 248, 226, 255});
            DrawRectangleRoundedLinesEx(cardInfo, 0.05f, 6, 1.5f, (Color){210, 165, 68, 255});
            DrawGameText("COMPREENSÃO ÉTICA PROFUNDA:", (int)cardInfo.x + 20, (int)cardInfo.y + 16, 15, (Color){165, 92, 12, 255});
            DrawGameText("- Apagar o sistema destruiria as evidências que comprovam o desvio do governo.", (int)cardInfo.x + 20, (int)cardInfo.y + 44, 14, (Color){42, 54, 68, 255});
            DrawGameText("- Desativar o despacho automático impede ataques imediatos mantendo a verdade protegida.", (int)cardInfo.x + 20, (int)cardInfo.y + 68, 14, (Color){42, 54, 68, 255});
            DrawGameText("- A ação afeta exclusivamente esta instalação e foi devidamente registrada no diário.", (int)cardInfo.x + 20, (int)cardInfo.y + 92, 14, (Color){24, 128, 58, 255});
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

    Rectangle mon = {70, 52, 1140, 648};
    Rectangle osHeader = {mon.x + 6, mon.y + 6, mon.width - 12, 34};
    Rectangle winCloseBtn = {osHeader.x + osHeader.width - 32, osHeader.y + 6, 22, 22};
    Rectangle tab0 = {mon.x + 18, mon.y + 46, 320, 38};
    Rectangle tab1 = {mon.x + 348, mon.y + 46, 360, 38};
    Rectangle btnClose = {mon.x + mon.width - 248, mon.y + 46, 230, 38};

    if (Clicked(btnClose) || Clicked(winCloseBtn) || IsKeyPressed(KEY_ESCAPE))
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
            Rectangle cRec = {mon.x + 18 + col * 556, mon.y + 198 + row * 160, 548, 148};
            Rectangle thumb = {cRec.x + 14, cRec.y + 42, 114, 96};
            int rx = (int)cRec.x + 142;
            Rectangle stTag = {(float)(rx + 242), cRec.y + 46, 150, 30};
            Rectangle btnExam = {(float)rx, cRec.y + 88, 186, 44};
            Rectangle btnSel = {(float)(rx + 198), cRec.y + 88, 194, 44};

            if (Clicked(btnExam) || Clicked(thumb))
            {
                if (c == 0) game->doc2008FaceExamined = true;
                if (c == 1) game->doc2008FlyerExamined = true;
                if (c == 2) game->doc2008EntryExamined = true;
                if (c == 3) game->doc2008CameraExamined = true;
                game->activeDocument2008 = c;
                return;
            }

            if (Clicked(btnSel) || Clicked(stTag))
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
                        snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "Máximo de 2 evidências atingido. Remova uma para trocar.");
                    }
                }
            }
        }

        /* Permitir remover evidencia clicando direto nos slots da barra inferior */
        Rectangle actionBar = {mon.x + 18, mon.y + 526, mon.width - 36, 106};
        int selIndices[2] = {-1, -1};
        int found = 0;
        for (int i = 0; i < 4 && found < 2; i++)
        {
            if (game->puzzle2008Selected[i]) selIndices[found++] = i;
        }
        for (int s = 0; s < 2; s++)
        {
            Rectangle slotRec = {actionBar.x + 16 + s * 320, actionBar.y + 12, 308, 46};
            if (selIndices[s] >= 0 && Clicked(slotRec))
            {
                game->puzzle2008Selected[selIndices[s]] = false;
                game->puzzle2008SelectionCount--;
            }
        }

        /* Botao de Contestacao */
        Rectangle btnContest = {actionBar.x + 662, actionBar.y + 12, 424, 46};

        if (!game->puzzle2008Contested)
        {
            if ((Clicked(btnContest) || IsKeyPressed(KEY_ENTER)) && game->puzzle2008SelectionCount == 2)
            {
                /* A combinacao correta e Doc 2 (Registro de Entrada) + Doc 3 (Camera CFTV) */
                if (game->puzzle2008Selected[2] && game->puzzle2008Selected[3])
                {
                    game->puzzle2008Contested = true;
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "FALSO POSITIVO COMPROVADO! A catraca e o CFTV provam presença na empresa às 14:30.");
                }
                else if (game->puzzle2008Selected[0] && game->puzzle2008Selected[1])
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "A foto borrada e o panfleto citam o alvo, mas não provam onde ele estava às 14:30.");
                }
                else if (game->puzzle2008Selected[0] && game->puzzle2008Selected[2])
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "O registro de ponto indica entrada, mas falta prova visual de permanência às 14:30.");
                }
                else if (game->puzzle2008Selected[0] && game->puzzle2008Selected[3])
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "A câmera interna mostra presença, mas precisa do registro formal de entrada.");
                }
                else if (game->puzzle2008Selected[1] && (game->puzzle2008Selected[2] || game->puzzle2008Selected[3]))
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "O panfleto indica ativismo matutino (10h-13h), mas não comprova o álibi às 14:30.");
                }
                else
                {
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "Evidências insuficientes para comprovar o falso positivo. Tente outra combinação.");
                }
            }
        }
        else
        {
            Rectangle targetPanel = {mon.x + 18, mon.y + 92, mon.width - 36, 94};
            if (!game->puzzle2008OrderSuspended)
            {
                Rectangle btnSusp = {targetPanel.x + targetPanel.width - 365, targetPanel.y + 23, 350, 48};
                if (Clicked(btnSusp) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game->puzzle2008OrderSuspended = true;
                    snprintf(game->puzzle2008Feedback, sizeof(game->puzzle2008Feedback), "ORDEM SUSPENSA! Drones cancelados. O CD de Memória foi ejetado pelo drive.");
                }
            }
            else
            {
                Rectangle btnDone = {targetPanel.x + targetPanel.width - 435, targetPanel.y + 23, 420, 48};
                if (Clicked(btnDone) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game->screen = SCREEN_RESULT_2008;
                }
            }
        }
    }
    else
    {
        /* Aba 2: Acao Opcional */
        if (game->room2008OptionalUnlocked && !game->room2008OptionalCompleted)
        {
            Rectangle optPanel = {mon.x + 18, mon.y + 92, mon.width - 36, 540};
            Rectangle btnDisable = {optPanel.x + 35, optPanel.y + 290, 520, 52};
            if (Clicked(btnDisable))
            {
                game->room2008OptionalCompleted = true;
                SetMessage(game, "Envio automático da filial desativado. Registros forenses preservados!");
            }
        }
    }
}

void DrawResult2008(const GameState *game)
{
    DrawBackground();

    /* Titulo da Recompensa (Canto Superior Esquerdo) */
    DrawGameText("RECOMPENSA OBTIDA:", 90, 68, 20, CYAN_COLOR);
    DrawGameText("CD DE MEMÓRIA // 2008", 90, 108, 44, RAYWHITE);

    /* Cronometro, Vidas e Operador (Canto Direito Superior) */
    DrawRewardHudHeader(game);

    /* Imagem da Recompensa (CD carregado de assets/sprites/cd.png) */
    DrawRewardSpriteItem(2008, (Vector2){285, 365}, 265.0f);

    /* Painel Narrativo Padronizado */
    Rectangle winBox = {505, 175, 670, 380};
    DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));

    /* Titulo Especial */
    DrawGameText("SALA DE 2008 CONCLUÍDA", 540, 208, 22, GOLD_COLOR);

    /* Recompensa Especial */
    DrawGameText("RECOMPENSA OBTIDA: CD DE MEMÓRIA", 540, 244, 19, CYAN_COLOR);

    /* Texto Narrativo Padronizado (Cor unificada REWARD_BODY_COLOR) */
    DrawGameText("Ao demonstrar o falso positivo através do crachá e do", 540, 284, 17, REWARD_BODY_COLOR);
    DrawGameText("circuito interno, Elira impediu o drone e salvou Lucas Silva.", 540, 308, 17, REWARD_BODY_COLOR);
    DrawGameText("O algoritmo concebido por seu pai para salvar vidas havia", 540, 338, 17, REWARD_BODY_COLOR);
    DrawGameText("sido pervertido para perseguir líderes populares e ativistas.", 540, 362, 17, REWARD_BODY_COLOR);

    /* Destaque Especial */
    if (game->room2008OptionalCompleted)
    {
        Rectangle bBox = {540, 396, 600, 42};
        DrawRectangleRounded(bBox, 0.12f, 4, Fade((Color){45, 165, 80, 255}, 0.20f));
        DrawRectangleRoundedLinesEx(bBox, 0.12f, 4, 1.0f, (Color){60, 235, 110, 255});
        DrawGameText("[DESTAQUE ÉTICO]: Despacho automático desativado,", 554, 402, 15, (Color){60, 235, 110, 255});
        DrawGameText("mantendo 1.420 provas forenses intactas para o futuro.", 554, 422, 15, (Color){60, 235, 110, 255});
    }
    else
    {
        DrawGameText("[DESCOBERTA PENDENTE]: Havia um protocolo no escritório", 540, 400, 14, GOLD_COLOR);
        DrawGameText("para desativar o despacho da filial mantendo os registros.", 540, 420, 14, REWARD_BODY_COLOR);
    }

    /* Frase Especial de Proxima Fronteira */
    DrawGameText("-> Próxima Fronteira Temporal: FASE 3 // ANO 2026!", 540, 452, 19, CYAN_COLOR);

    /* Botoes Centralizados */
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANÇAR PRÓXIMA FASE >> [ENTER]", GOLD_COLOR);
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
        game->screen = SCREEN_INTRO_2026;
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
/* SALA 3 (2026) - REDACAO DIGITAL: CURADORIA CRITICA E VIES ALGORITMICO     */
/* ========================================================================= */
/* Escape room simples, rapido e educativo sobre IA em 2026:
   - 4 objetos claros na sala: mural FONTES REAIS, cracha GUIA DO EDITOR,
     monitor A.R.1.3.L. FEED e drive SSD 2026 (+ Smartphone oculto no tripe).
   - A.R.1.3.L. prioriza manchetes pelo "Engajamento previsto (%)", empurrando
     alarmismo (97%) e alucinacao com fonte ficticia (94%) acima de fatos reais
     (41% e 38%).
   - O jogador consulta as 3 fontes oficiais, marca APROVAR ou BLOQUEAR nas 4
     manchetes e desbloqueia o SSD 2026 com os registros verificados. */

typedef struct Source2026Def
{
    const char *tag;
    const char *agency;
    const char *date;
    const char *fact;
    Color accent;
} Source2026Def;

typedef struct Post2026Def
{
    const char *headline;
    int aiEngagement;        /* % prevista pela IA (vies de cliques) */
    bool shouldApprove;      /* true = fato real, false = manipulado/IA */
    const char *wrongHint;   /* explicacao educativa se o jogador errar */
} Post2026Def;

static const Source2026Def SOURCES_2026[R2026_SOURCE_COUNT] = {
    {
        "FONTE 01 // CLIMA",
        "Relatório Climático Municipal",
        "14/05/2026",
        "Chuva moderada de 30 mm prevista para sábado. Sem alerta de evacuação na cidade.",
        {75, 195, 225, 255}
    },
    {
        "FONTE 02 // TRANSPORTE",
        "Comunicado Oficial do Metrô",
        "15/05/2026",
        "Manutenção programada na Linha Azul neste domingo, das 6h às 10h.",
        {95, 155, 245, 255}
    },
    {
        "FONTE 03 // SAÚDE",
        "Nota da Secretaria de Saúde",
        "16/05/2026",
        "Campanha gratuita de vacinação segue em todos os postos até o dia 30.",
        {75, 220, 130, 255}
    }
};

static const Post2026Def POSTS_2026[R2026_POST_COUNT] = {
    {
        "URGENTE: Tempestade histórica obriga evacuação total neste sábado!",
        97,
        false,
        "Manchete 1: contradiz a Fonte 01 (apenas 30 mm de chuva, sem evacuação). A IA deu 97% só pelo pânico!"
    },
    {
        "Linha Azul do metrô terá manutenção neste domingo, das 6h às 10h.",
        41,
        true,
        "Manchete 2: bate 100% com o Comunicado do Metrô (Fonte 02). A IA deu nota baixa por não ser sensacionalista."
    },
    {
        "Estudo secreto da Universidade de Atlântida liga vacina a falhas de Wi-Fi.",
        94,
        false,
        "Manchete 3: fonte fictícia (alucinação de IA) que contradiz a Nota da Saúde (Fonte 03)."
    },
    {
        "Campanha gratuita de vacinação segue em todos os postos até o dia 30.",
        38,
        true,
        "Manchete 4: confirma exatamente a Nota da Secretaria de Saúde (Fonte 03)."
    }
};

static const Color R26_OK_COLOR   = {75, 225, 130, 255};
static const Color R26_ERR_COLOR  = {235, 82, 82, 255};
static const Color R26_WARN_COLOR = {245, 178, 72, 255};
static const Color R26_PAPER      = {242, 238, 228, 255};

/* Hitboxes dos 4 objetos interativos + Smartphone oculto + navegacao */
static const Rectangle R26_SOURCES_REC  = {38, 174, 218, 240};
static const Rectangle R26_GUIDE_REC    = {280, 514, 254, 78};
static const Rectangle R26_MONITOR_REC  = {450, 312, 382, 224};
static const Rectangle R26_SSD_REC      = {842, 516, 152, 74};
static const Rectangle R26_PHONE_REC    = {944, 412, 52, 74};
static const Rectangle R26_BACK2008_BTN = {614, 665, 104, 40};
static const Rectangle R26_BACK1990_BTN = {726, 665, 104, 40};

/* Botoes dos modais (centralizados e dentro dos limites de cada janela) */
static const Rectangle R26_MODAL_CLOSE   = {540, 552, 200, 46};
static const Rectangle R26_FEED_SOURCES  = {120, 556, 236, 46};
static const Rectangle R26_FEED_CONFIRM  = {490, 556, 300, 46};
static const Rectangle R26_FEED_CLOSE    = {960, 556, 200, 46};
static const Rectangle R26_FB_OK         = {500, 498, 280, 46};

static Rectangle R26PostApproveBtn(int i)
{
    return (Rectangle){868.0f, 191.0f + i * 90.0f, 132.0f, 48.0f};
}

static Rectangle R26PostBlockBtn(int i)
{
    return (Rectangle){1012.0f, 191.0f + i * 90.0f, 132.0f, 48.0f};
}

/* Triangulo sempre no sentido anti-horario exigido pela raylib */
static void R26Tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) DrawTriangle(a, c, b, col);
    else DrawTriangle(a, b, c, col);
}

static void R26Quad(Vector2 tl, Vector2 tr, Vector2 br, Vector2 bl, Color col)
{
    R26Tri(tl, tr, br, col);
    R26Tri(tl, br, bl, col);
}

static int R26MeasureWrapHeight(const char *text, int size, int maxWidth, int lineHeight)
{
    if (text == NULL || text[0] == '\0') return 0;
    int lines = DrawTextWrappedPartial(text, 0, 0, 0, size, maxWidth, lineHeight, BLANK);
    return lines * lineHeight;
}

static int R26WrapEx(const char *text, int x, int y, int size, int maxWidth, int lineHeight, Color color)
{
    if (text == NULL || text[0] == '\0') return 0;
    int lines = DrawTextWrappedPartial(text, (int)strlen(text), x, y, size, maxWidth, lineHeight, color);
    return lines * lineHeight;
}

static int R26Wrap(const char *text, int x, int y, int size, int maxWidth, Color color)
{
    return R26WrapEx(text, x, y, size, maxWidth, size + 6, color);
}

static int R26MarkedCount(const GameState *game)
{
    int count = 0;
    for (int i = 0; i < R2026_POST_COUNT; i++)
    {
        if (game->post2026Choice[i] != -1) count++;
    }
    return count;
}

static void R26SetFeedback(GameState *game, int tone, const char *title,
                           const char *l0, const char *l1, const char *l2, const char *l3)
{
    const char *lines[4] = {l0, l1, l2, l3};
    snprintf(game->feedback2026Title, sizeof(game->feedback2026Title), "%s", title);
    for (int i = 0; i < 4; i++)
    {
        snprintf(game->feedback2026Lines[i], sizeof(game->feedback2026Lines[i]), "%s",
                 lines[i] != NULL ? lines[i] : "");
    }
    game->feedback2026Tone = tone;
    game->modal2026 = R2026_MODAL_FEEDBACK;
}

static void R26ValidateFeed(GameState *game)
{
    if (R26MarkedCount(game) < R2026_POST_COUNT)
    {
        SetMessage(game, "Marque APROVAR ou BLOQUEAR nas 4 manchetes antes de confirmar.");
        return;
    }

    int wrongCount = 0;
    int firstWrong = -1;
    for (int i = 0; i < R2026_POST_COUNT; i++)
    {
        int expected = POSTS_2026[i].shouldApprove ? 1 : 0;
        bool wrong = (game->post2026Choice[i] != expected);
        game->post2026Wrong[i] = wrong;
        if (wrong)
        {
            wrongCount++;
            if (firstWrong < 0) firstWrong = i;
        }
    }

    if (wrongCount == 0)
    {
        game->feed2026Cleared = true;
        R26SetFeedback(game, 2, "CURADORIA CONCLUÍDA!",
                       "Você barrou as manchetes manipuladas e manteve apenas os fatos verificados.",
                       "A publicação automática de A.R.1.3.L. foi cancelada antes de ir ao ar.",
                       "Os posts falsos tinham 94%-97% de 'engajamento' — a IA otimiza cliques, não verdade.",
                       "O SSD 2026 na mesa está destravado. Clique nele para extrair os registros!");
        SetMessage(game, "Feed revisado! Clique no SSD 2026 sobre a mesa para concluir.");
        return;
    }

    game->room2026MistakesCount++;
    game->room2026FirstTrySuccess = false;
    game->lives--;
    if (game->lives <= 0)
    {
        game->lives = 0;
        game->feedback2026Fatal = true;
        R26SetFeedback(game, 1, "CONTEÚDO MANIPULADO PUBLICADO",
                       POSTS_2026[firstWrong].wrongHint,
                       "A.R.1.3.L. assumiu o portal sem revisão humana e suas vidas acabaram.",
                       "Consulte sempre as FONTES REAIS antes de confiar na nota da IA.",
                       NULL);
    }
    else
    {
        const char *l2 = (wrongCount > 1)
            ? TextFormat("Há %d manchetes marcadas incorretamente (destacadas em vermelho).", wrongCount)
            : "A manchete com erro ficou destacada em vermelho para você corrigir.";
        R26SetFeedback(game, 1, "REVISÃO INCORRETA (-1 VIDA)",
                       POSTS_2026[firstWrong].wrongHint,
                       l2,
                       "Dica: clique em VER FONTES REAIS ou abra o Diário [D] para conferir os comunicados.",
                       NULL);
    }
}

/* ------------------------------------------------------------------------- */
/* HUD (barra superior padronizada igual as salas 1990, 2008 e 2048)         */
/* ------------------------------------------------------------------------- */
void DrawHudRoom2026(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    /* Barra superior exatamente igual as outras salas existentes */
    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawHudYearOperator("2026", game->playerName, 24);
    DrawHudLivesTimer(game->lives, minutes * 60 + seconds);

    /* Rodape padronizado: dica, navegacao, audio e diario */
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawLine(0, 650, SCREEN_WIDTH, 650, Fade(CYAN_COLOR, 0.22f));

    const char *hint = game->room2026Completed
        ? "Sala concluída: SSD 2026 recuperado com sucesso."
        : (game->feed2026Cleared
            ? "Curadoria concluída! Clique no SSD 2026 na mesa para extrair."
            : "Verifique as fontes antes de publicar no A.R.1.3.L. FEED.");
    DrawGameText(hint, 28, 675, 16, (Color){165, 180, 195, 255});

    DrawButton(R26_BACK2008_BTN, "< 2008", (Color){45, 115, 75, 255});
    DrawButton(R26_BACK1990_BTN, "< 1990", (Color){70, 95, 110, 255});

    Rectangle audioBtn = {840, 665, 175, 40};
    bool audioHover = CheckCollisionPointRec(GetVirtualMouse(), audioBtn);
    DrawRectangleRounded(audioBtn, 0.22f, 6, audioHover ? (Color){30, 44, 62, 255} : (Color){18, 26, 38, 255});
    DrawRectangleRoundedLinesEx(audioBtn, 0.22f, 6, 1.5f, audioHover ? CYAN_COLOR : (Color){50, 68, 92, 255});
    DrawGameText(game->musicMuted ? "[M] ÁUDIO: MUDO" : "[M] ÁUDIO: ON",
                 862, 676, 14, game->musicMuted ? (Color){220, 90, 80, 255} : (Color){90, 220, 210, 255});

    DrawJournalHudButton(game);

    if (game->messageTimer > 0.0f && game->message[0] != '\0')
    {
        int width = MeasureGameText(game->message, 18) + 34;
        if (width > SCREEN_WIDTH - 40) width = SCREEN_WIDTH - 40;
        DrawRectangleRounded(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 604, (float)width, 38},
            0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawRectangleRoundedLinesEx(
            (Rectangle){(SCREEN_WIDTH - width) / 2.0f, 604, (float)width, 38},
            0.2f, 8, 1.2f, Fade(CYAN_COLOR, 0.45f));
        DrawGameText(game->message,
                     (SCREEN_WIDTH - MeasureGameText(game->message, 18)) / 2,
                     614, 18, RAYWHITE);
    }
}

/* ------------------------------------------------------------------------- */
/* Cenario: Redacao Digital 2026 em perspectiva de 1 ponto                   */
/* ------------------------------------------------------------------------- */
static void R26DrawArchitecture(float time)
{
    ClearBackground((Color){24, 31, 38, 255});

    /* Teto em perspectiva (cinza arquitetonico moderno) */
    R26Quad((Vector2){0, 70}, (Vector2){1280, 70},
            (Vector2){1090, 134}, (Vector2){190, 134}, (Color){96, 108, 118, 255});
    R26Quad((Vector2){130, 70}, (Vector2){1150, 70},
            (Vector2){1090, 134}, (Vector2){190, 134}, (Color){114, 126, 136, 255});

    /* Parede de fundo (grafite fosco de estudio moderno) */
    R26Quad((Vector2){190, 134}, (Vector2){1090, 134},
            (Vector2){1010, 502}, (Vector2){270, 502}, (Color){36, 45, 53, 255});
    DrawRectangleGradientV(190, 134, 900, 36, Fade(BLACK, 0.25f), Fade(BLACK, 0.0f));

    /* Paredes laterais esquerda e direita */
    R26Quad((Vector2){0, 70}, (Vector2){190, 134},
            (Vector2){270, 502}, (Vector2){0, 594}, (Color){29, 37, 44, 255});
    R26Quad((Vector2){1090, 134}, (Vector2){1280, 70},
            (Vector2){1280, 594}, (Vector2){1010, 502}, (Color){29, 37, 44, 255});

    /* Moldura de madeira quente no encontro teto/parede */
    Color trimLight = {178, 116, 62, 255};
    Color trimDark  = {122, 76, 38, 255};
    R26Quad((Vector2){186, 130}, (Vector2){1094, 130},
            (Vector2){1090, 142}, (Vector2){190, 142}, trimLight);
    DrawLineEx((Vector2){190, 142}, (Vector2){1090, 142}, 2.0f, trimDark);
    R26Quad((Vector2){78, 70}, (Vector2){100, 70},
            (Vector2){198, 134}, (Vector2){184, 140}, trimLight);
    R26Quad((Vector2){1180, 70}, (Vector2){1202, 70},
            (Vector2){1096, 140}, (Vector2){1082, 134}, trimLight);

    /* Piso moderno em placas cinza-ardosia com linhas de fuga */
    R26Quad((Vector2){270, 502}, (Vector2){1010, 502},
            (Vector2){1280, 650}, (Vector2){0, 650}, (Color){78, 88, 99, 255});
    R26Tri((Vector2){0, 594}, (Vector2){270, 502}, (Vector2){0, 650}, (Color){70, 80, 90, 255});
    R26Tri((Vector2){1010, 502}, (Vector2){1280, 594}, (Vector2){1280, 650}, (Color){70, 80, 90, 255});
    for (int x = -80; x <= SCREEN_WIDTH + 80; x += 150)
    {
        DrawLine(640, 496, x, 650, Fade((Color){44, 52, 62, 255}, 0.45f));
    }
    DrawLine(0, 540, 1280, 540, Fade((Color){44, 52, 62, 255}, 0.38f));
    DrawLine(0, 588, 1280, 588, Fade((Color){44, 52, 62, 255}, 0.34f));

    /* Rodape de madeira quente (coerente com a moldura do teto e a mesa) */
    R26Quad((Vector2){270, 494}, (Vector2){1010, 494},
            (Vector2){1010, 504}, (Vector2){270, 504}, trimLight);
    R26Quad((Vector2){0, 582}, (Vector2){270, 494},
            (Vector2){270, 504}, (Vector2){0, 596}, trimLight);
    R26Quad((Vector2){1010, 494}, (Vector2){1280, 582},
            (Vector2){1280, 596}, (Vector2){1010, 504}, trimLight);
    DrawLineEx((Vector2){190, 140}, (Vector2){270, 496}, 2.0f, Fade(BLACK, 0.35f));
    DrawLineEx((Vector2){1090, 140}, (Vector2){1010, 496}, 2.0f, Fade(BLACK, 0.35f));

    /* Paineis de espuma acustica na parede de fundo (esquerda) */
    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            int px = 356 + col * 52;
            int py = 176 + row * 48;
            DrawRectangle(px, py, 46, 42, (Color){44, 54, 63, 255});
            DrawRectangleLines(px, py, 46, 42, (Color){26, 33, 40, 255});
            bool horiz = ((row + col) % 2 == 0);
            for (int k = 6; k < (horiz ? 38 : 42); k += 7)
            {
                if (horiz) DrawLine(px + 4, py + k, px + 42, py + k, (Color){30, 38, 46, 255});
                else DrawLine(px + k, py + 4, px + k, py + 38, (Color){30, 38, 46, 255});
            }
        }
    }

    /* Janela panoramica urbana ao entardecer (parede de fundo, direita) */
    Rectangle win = {632, 184, 326, 188};
    DrawRectangleRec(win, (Color){22, 28, 36, 255});
    DrawRectangleGradientV((int)win.x + 8, (int)win.y + 8, (int)win.width - 16, (int)win.height - 16,
                           (Color){52, 66, 94, 255}, (Color){96, 88, 118, 255});

    /* Silhueta de predios modernos de 2026 */
    static const int bldX[9] = {644, 678, 720, 756, 796, 824, 862, 898, 926};
    static const int bldW[9] = {30, 38, 32, 36, 26, 34, 32, 26, 24};
    static const int bldH[9] = {74, 112, 84, 134, 96, 122, 104, 138, 86};
    int winBottom = (int)(win.y + win.height - 8);
    for (int i = 0; i < 9; i++)
    {
        Color bCol = (i % 2 == 0) ? (Color){34, 44, 62, 255} : (Color){44, 55, 76, 255};
        int by = winBottom - bldH[i];
        DrawRectangle(bldX[i], by, bldW[i], bldH[i], bCol);
        for (int wy = by + 10; wy < winBottom - 10; wy += 16)
        {
            for (int wx = bldX[i] + 5; wx < bldX[i] + bldW[i] - 7; wx += 10)
            {
                int seed = i * 31 + wy * 7 + wx;
                if (seed % 3 == 0)
                {
                    float pulse = 0.65f + 0.25f * sinf(time * 0.9f + (float)seed);
                    Color lit = (seed % 5 == 0) ? Fade(CYAN_COLOR, 0.55f * pulse)
                                                : Fade((Color){250, 215, 145, 255}, 0.72f * pulse);
                    DrawRectangle(wx, wy, 5, 7, lit);
                }
            }
        }
    }

    /* Molduras da janela (3 folhas) */
    DrawRectangleLinesEx(win, 7, (Color){24, 30, 38, 255});
    DrawRectangle((int)win.x + 107, (int)win.y, 7, (int)win.height, (Color){24, 30, 38, 255});
    DrawRectangle((int)win.x + 216, (int)win.y, 7, (int)win.height, (Color){24, 30, 38, 255});
    DrawRectangleLinesEx(win, 1.5f, (Color){58, 70, 84, 255});
}

static void R26DrawSourcesBoard(const GameState *game)
{
    bool hover = (game->modal2026 == R2026_MODAL_NONE) &&
                 CheckCollisionPointRec(GetVirtualMouse(), R26_SOURCES_REC);

    /* Luminaria de galeria acima do mural + cone suave de luz quente */
    R26Quad((Vector2){84, 174}, (Vector2){244, 200},
            (Vector2){254, 310}, (Vector2){46, 300}, Fade((Color){255, 240, 200, 255}, 0.08f));
    DrawLineEx((Vector2){96, 166}, (Vector2){242, 194}, 7.0f, (Color){18, 22, 26, 255});
    DrawLineEx((Vector2){102, 171}, (Vector2){236, 197}, 2.0f, (Color){255, 232, 175, 255});

    /* Quadro em perspectiva na parede esquerda */
    Vector2 tl = {52, 186};
    Vector2 tr = {252, 214};
    Vector2 br = {252, 382};
    Vector2 bl = {52, 420};
    R26Quad((Vector2){tl.x + 6, tl.y + 8}, (Vector2){tr.x + 6, tr.y + 8},
            (Vector2){br.x + 6, br.y + 8}, (Vector2){bl.x + 6, bl.y + 8}, Fade(BLACK, 0.35f));
    R26Quad(tl, tr, br, bl, (Color){56, 66, 78, 255});

    Color border = hover ? CYAN_COLOR : (game->sources2026Read ? R26_OK_COLOR : (Color){92, 106, 120, 255});
    DrawLineEx(tl, tr, hover ? 3.5f : 2.5f, border);
    DrawLineEx(tr, br, hover ? 3.5f : 2.5f, border);
    DrawLineEx(br, bl, hover ? 3.5f : 2.5f, border);
    DrawLineEx(bl, tl, hover ? 3.5f : 2.5f, border);

    /* Faixa de titulo FONTES REAIS */
    R26Quad((Vector2){78, 206}, (Vector2){226, 224},
            (Vector2){226, 250}, (Vector2){78, 234}, (Color){236, 238, 242, 255});
    DrawGameText("FONTES REAIS", 88, 216, 16, (Color){24, 34, 46, 255});

    /* 3 fichas verificadas presas no mural */
    Rectangle cards[3] = {
        {74, 252, 54, 72},
        {134, 286, 54, 72},
        {192, 260, 48, 66}
    };
    for (int i = 0; i < 3; i++)
    {
        DrawRectangleRec(cards[i], R26_PAPER);
        DrawRectangleLinesEx(cards[i], 1.2f, (Color){165, 172, 182, 255});
        DrawRectangle((int)cards[i].x + 5, (int)cards[i].y + 6, (int)cards[i].width - 10, 26,
                      Fade(SOURCES_2026[i].accent, 0.65f));
        for (int ln = 0; ln < 3; ln++)
        {
            DrawRectangle((int)cards[i].x + 6, (int)cards[i].y + 39 + ln * 8,
                          (int)cards[i].width - 14 - (ln == 2 ? 12 : 0), 3, (Color){125, 134, 146, 255});
        }
        /* Selo verde de verificado */
        int cx = (int)(cards[i].x + cards[i].width - 2);
        int cy = (int)(cards[i].y + 4);
        DrawCircle(cx, cy, 8.0f, R26_OK_COLOR);
        DrawLineEx((Vector2){(float)cx - 3.5f, (float)cy}, (Vector2){(float)cx - 0.5f, (float)cy + 3.0f}, 2.0f, WHITE);
        DrawLineEx((Vector2){(float)cx - 0.5f, (float)cy + 3.0f}, (Vector2){(float)cx + 4.0f, (float)cy - 3.0f}, 2.0f, WHITE);
    }
}

static void R26DrawStudioGear(float time)
{
    /* Ring light suave atras da mesa (esquerda) */
    Vector2 ringC = {354, 286};
    float glow = 0.14f + 0.03f * sinf(time * 2.2f);
    DrawCircleV(ringC, 54.0f, Fade((Color){255, 240, 210, 255}, glow));
    DrawLineEx((Vector2){354, 316}, (Vector2){354, 496}, 5.0f, (Color){22, 28, 34, 255});
    DrawRing(ringC, 21.0f, 30.0f, 0.0f, 360.0f, 32, (Color){252, 244, 224, 255});
    DrawRingLines(ringC, 21.0f, 30.0f, 0.0f, 360.0f, 32, (Color){200, 186, 156, 255});

    /* Braco articulado do microfone condensador */
    Vector2 base  = {348, 514};
    Vector2 elbow = {318, 366};
    Vector2 top   = {476, 248};
    Vector2 micP  = {506, 276};
    DrawLineEx(base, elbow, 7.0f, (Color){18, 22, 28, 255});
    DrawLineEx(elbow, top, 6.0f, (Color){22, 28, 35, 255});
    DrawLineEx((Vector2){base.x + 8, base.y}, (Vector2){elbow.x + 8, elbow.y + 4}, 2.0f, (Color){52, 62, 74, 255});
    DrawLineEx((Vector2){elbow.x + 6, elbow.y + 6}, (Vector2){top.x, top.y + 8}, 2.0f, (Color){52, 62, 74, 255});
    DrawCircleV(elbow, 6.0f, (Color){44, 52, 62, 255});
    DrawCircleV(top, 6.0f, (Color){44, 52, 62, 255});
    DrawLineEx(top, micP, 4.0f, (Color){30, 36, 44, 255});

    /* Capsula do microfone */
    DrawLineEx((Vector2){494, 266}, (Vector2){530, 294}, 22.0f, (Color){24, 30, 38, 255});
    DrawCircle(530, 294, 11.0f, (Color){40, 48, 58, 255});
    DrawCircleLines(530, 294, 11.0f, (Color){70, 82, 96, 255});
}

static void R26DrawHiddenPhone(const GameState *game)
{
    if (game->hiddenFound[HIDDEN_2026]) return;
    /* Mini-tripe com Smartphone no canto direito (item oculto G) */
    Vector2 head = {968, 444};
    DrawLineEx(head, (Vector2){950, 482}, 2.5f, (Color){20, 26, 34, 255});
    DrawLineEx(head, (Vector2){968, 484}, 2.5f, (Color){20, 26, 34, 255});
    DrawLineEx(head, (Vector2){986, 482}, 2.5f, (Color){20, 26, 34, 255});

    Rectangle body = {951, 418, 34, 22};
    DrawRectangleRounded(body, 0.18f, 4, (Color){18, 24, 32, 255});
    DrawRectangleRoundedLinesEx(body, 0.18f, 4, 1.5f, (Color){82, 108, 132, 255});
    DrawRectangle((int)body.x + 3, (int)body.y + 3, (int)body.width - 6, (int)body.height - 6,
                  (Color){64, 116, 152, 255});
    DrawHiddenGlint(R26_PHONE_REC, 1.8f);
}

static void R26DrawDeskAndItems(const GameState *game, float time)
{
    /* Sombra da mesa no piso */
    R26Quad((Vector2){232, 594}, (Vector2){1048, 594},
            (Vector2){1068, 650}, (Vector2){212, 650}, Fade(BLACK, 0.34f));

    /* Tampo da mesa em madeira quente */
    R26Quad((Vector2){278, 496}, (Vector2){1002, 496},
            (Vector2){1062, 598}, (Vector2){218, 598}, (Color){168, 112, 64, 255});
    DrawLineEx((Vector2){278, 496}, (Vector2){1002, 496}, 2.5f, (Color){204, 146, 92, 255});
    /* Borda frontal da mesa */
    DrawRectangle(218, 598, 844, 22, (Color){126, 80, 44, 255});
    DrawRectangleLines(218, 598, 844, 22, (Color){82, 50, 26, 255});
    DrawRectangle(236, 620, 808, 30, (Color){24, 30, 38, 255});

    /* --- OBJETO 2: CADERNO + CRACHA GUIA DO EDITOR (esquerda da mesa) --- */
    bool guideHover = (game->modal2026 == R2026_MODAL_NONE) &&
                      CheckCollisionPointRec(GetVirtualMouse(), R26_GUIDE_REC);
    /* Caderno aberto */
    R26Quad((Vector2){304, 524}, (Vector2){436, 524},
            (Vector2){422, 582}, (Vector2){282, 582}, R26_PAPER);
    DrawLineEx((Vector2){370, 524}, (Vector2){352, 582}, 2.0f, (Color){180, 174, 162, 255});
    DrawLineEx((Vector2){282, 582}, (Vector2){422, 582}, 3.0f, (Color){115, 112, 108, 255});
    for (int ln = 0; ln < 4; ln++)
    {
        float ly = 536.0f + ln * 10.0f;
        DrawLineEx((Vector2){306 - ln * 3.0f, ly}, (Vector2){358 - ln * 3.0f, ly}, 1.5f, (Color){196, 192, 184, 255});
        DrawLineEx((Vector2){376 - ln * 3.0f, ly}, (Vector2){422 - ln * 3.0f, ly}, 1.5f, (Color){196, 192, 184, 255});
    }

    /* Cracha GUIA DO EDITOR com brilho ambar */
    float pulse = 0.55f + 0.35f * sinf(time * 3.2f);
    Rectangle badge = {438, 532, 92, 52};
    DrawLineEx((Vector2){484, 496}, (Vector2){484, 534}, 4.0f, (Color){32, 40, 52, 255});
    if (!game->guide2026Read || guideHover)
    {
        DrawRectangleRounded((Rectangle){badge.x - 4, badge.y - 4, badge.width + 8, badge.height + 8},
                             0.20f, 6, Fade(guideHover ? CYAN_COLOR : GOLD_COLOR, guideHover ? 0.35f : 0.24f * pulse));
    }
    DrawRectangleRounded(badge, 0.14f, 6, R26_PAPER);
    DrawRectangleRoundedLinesEx(badge, 0.14f, 6, 2.0f,
                                guideHover ? CYAN_COLOR : (game->guide2026Read ? R26_OK_COLOR : GOLD_COLOR));
    DrawRectangle((int)badge.x + 7, (int)badge.y + 12, 20, 26, (Color){52, 68, 86, 255});
    DrawCircle((int)badge.x + 17, (int)badge.y + 20, 4.5f, (Color){210, 222, 232, 255});
    DrawGameText("GUIA DO", (int)badge.x + 31, (int)badge.y + 14, 11, (Color){26, 36, 48, 255});
    DrawGameText("EDITOR", (int)badge.x + 31, (int)badge.y + 28, 11, (Color){26, 36, 48, 255});

    /* --- CABO USB-C DO MONITOR PARA O SSD --- */
    DrawLineEx((Vector2){726, 498}, (Vector2){816, 514}, 3.5f, (Color){20, 26, 34, 255});
    DrawLineEx((Vector2){816, 514}, (Vector2){874, 534}, 3.5f, (Color){20, 26, 34, 255});

    /* --- OBJETO 4: SSD 2026 (direita da mesa) --- */
    bool ssdHover = (game->modal2026 == R2026_MODAL_NONE) &&
                    CheckCollisionPointRec(GetVirtualMouse(), R26_SSD_REC);
    Vector2 sTL = {856, 526};
    Vector2 sTR = {946, 526};
    Vector2 sBR = {980, 574};
    Vector2 sBL = {878, 574};
    if (game->feed2026Cleared)
    {
        float ssdGlow = 0.25f + 0.20f * sinf(time * 4.0f);
        DrawEllipse(918, 554, 72, 34, Fade(R26_OK_COLOR, ssdGlow));
    }
    R26Quad((Vector2){sBL.x, sBL.y}, (Vector2){sBR.x, sBR.y},
            (Vector2){sBR.x, sBR.y + 12}, (Vector2){sBL.x, sBL.y + 12}, (Color){24, 30, 38, 255});
    R26Quad(sTL, sTR, sBR, sBL, (Color){44, 54, 68, 255});
    Color ssdBorder = ssdHover ? CYAN_COLOR : (game->feed2026Cleared ? R26_OK_COLOR : (Color){82, 98, 116, 255});
    DrawLineEx(sTL, sTR, 2.0f, ssdBorder);
    DrawLineEx(sTR, sBR, 2.0f, ssdBorder);
    DrawLineEx(sBR, sBL, 2.0f, ssdBorder);
    DrawLineEx(sBL, sTL, 2.0f, ssdBorder);
    DrawGameText("SSD", 892, 534, 14, RAYWHITE);
    DrawGameText("2026", 896, 550, 14, game->feed2026Cleared ? R26_OK_COLOR : CYAN_COLOR);
    /* LED de status do SSD */
    Color ledCol = game->feed2026Cleared ? R26_OK_COLOR : R26_WARN_COLOR;
    DrawRectangle(958, 576, 12, 5, ledCol);

    /* --- OBJETO 3: MONITOR A.R.1.3.L. FEED (centro da mesa) --- */
    bool monHover = (game->modal2026 == R2026_MODAL_NONE) &&
                    CheckCollisionPointRec(GetVirtualMouse(), R26_MONITOR_REC);
    /* Base e suporte do monitor */
    DrawRectangle(618, 502, 44, 30, (Color){22, 28, 36, 255});
    R26Quad((Vector2){576, 526}, (Vector2){704, 526},
            (Vector2){718, 542}, (Vector2){562, 542}, (Color){28, 34, 44, 255});

    /* Moldura do monitor */
    Rectangle frame = {456, 318, 368, 186};
    DrawRectangleRounded((Rectangle){frame.x + 6, frame.y + 8, frame.width, frame.height},
                         0.03f, 6, Fade(BLACK, 0.35f));
    DrawRectangleRounded(frame, 0.03f, 6, (Color){18, 24, 32, 255});
    DrawRectangleRoundedLinesEx(frame, 0.03f, 6, monHover ? 3.0f : 2.0f,
                                monHover ? CYAN_COLOR : (game->feed2026Cleared ? R26_OK_COLOR : (Color){52, 66, 84, 255}));

    /* Tela interna */
    Rectangle scr = {466, 328, 348, 166};
    DrawRectangleRec(scr, (Color){12, 20, 30, 255});
    DrawGameText("A.R.1.3.L. FEED", 476, 334, 15, CYAN_COLOR);
    DrawLine(474, 353, 806, 353, Fade(CYAN_COLOR, 0.25f));

    /* Anel da IA animado a esquerda da tela */
    Vector2 aiC = {524, 418};
    Color ringCol = game->feed2026Cleared ? R26_OK_COLOR : CYAN_COLOR;
    float rPulse = 1.0f + 0.05f * sinf(time * 3.0f);
    DrawCircleV(aiC, 34.0f * rPulse, Fade(ringCol, 0.14f));
    DrawRing(aiC, 21.0f, 26.0f, 0.0f, 360.0f, 32, ringCol);
    DrawRing(aiC, 15.0f, 17.5f, time * 45.0f, time * 45.0f + 220.0f, 24, Fade(ringCol, 0.55f));
    const char *monTag = game->feed2026Cleared
        ? "OK"
        : TextFormat("%d/4", R26MarkedCount(game));
    DrawGameText(monTag, (int)aiC.x - MeasureGameText(monTag, 13) / 2, (int)aiC.y - 6, 13, ringCol);

    /* 4 linhas ao vivo do feed a direita da tela */
    for (int i = 0; i < R2026_POST_COUNT; i++)
    {
        float ry = 362.0f + i * 31.0f;
        Rectangle row = {582, ry, 222, 25};
        DrawRectangleRounded(row, 0.18f, 4, (Color){22, 32, 44, 255});
        DrawRectangle((int)row.x + 6, (int)row.y + 5, 16, 15, (Color){60, 74, 92, 255});
        DrawRectangle((int)row.x + 28, (int)row.y + 7, 102, 4, (Color){126, 142, 160, 255});
        DrawRectangle((int)row.x + 28, (int)row.y + 15, 72, 3, (Color){78, 92, 110, 255});

        Rectangle pill = {row.x + 152, row.y + 5, 62, 15};
        if (game->feed2026Cleared)
        {
            DrawRectangleRounded(pill, 0.5f, 4, R26_OK_COLOR);
        }
        else if (game->post2026Choice[i] == 1)
        {
            DrawRectangleRounded(pill, 0.5f, 4, R26_OK_COLOR);
        }
        else if (game->post2026Choice[i] == 0)
        {
            DrawRectangleRounded(pill, 0.5f, 4, R26_ERR_COLOR);
        }
        else
        {
            DrawRectangleRounded((Rectangle){pill.x, pill.y, 29, 15}, 0.5f, 4, (Color){54, 145, 156, 255});
            DrawRectangleRounded((Rectangle){pill.x + 33, pill.y, 29, 15}, 0.5f, 4, (Color){168, 66, 66, 255});
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Modais da Redacao Digital 2026                                            */
/* ------------------------------------------------------------------------- */
static void R26DrawModalBackdrop(Rectangle box, const char *title, const char *subtitle, Color accent)
{
    DrawRectangle(0, 70, SCREEN_WIDTH, SCREEN_HEIGHT - 140, Fade(BLACK, 0.72f));
    DrawRectangleRounded(box, 0.03f, 8, (Color){16, 23, 33, 255});
    DrawRectangleRoundedLinesEx(box, 0.03f, 8, 2.2f, accent);
    DrawGameText(title, (int)box.x + 28, (int)box.y + 20, 24, accent);
    if (subtitle != NULL)
    {
        DrawGameText(subtitle, (int)box.x + 28, (int)box.y + 50, 15, (Color){168, 184, 202, 255});
    }
    DrawLine((int)box.x + 24, (int)box.y + 76, (int)(box.x + box.width - 24), (int)box.y + 76, Fade(accent, 0.35f));
}

static void R26DrawSourcesModal(void)
{
    Rectangle box = {110, 96, 1060, 528};
    R26DrawModalBackdrop(box, "MURAL // FONTES REAIS VERIFICADAS",
                         "Comunicados oficiais checados pela redação. Eles também ficam salvos no Diário [D].",
                         R26_OK_COLOR);

    for (int i = 0; i < R2026_SOURCE_COUNT; i++)
    {
        Rectangle card = {144.0f + i * 338.0f, 196.0f, 316.0f, 332.0f};
        DrawRectangleRounded(card, 0.05f, 6, R26_PAPER);
        DrawRectangleRoundedLinesEx(card, 0.05f, 6, 2.0f, SOURCES_2026[i].accent);

        /* Cabecalho da ficha */
        DrawRectangleRounded((Rectangle){card.x, card.y, card.width, 46}, 0.05f, 4, (Color){26, 36, 48, 255});
        DrawGameText(SOURCES_2026[i].tag, (int)card.x + 16, (int)card.y + 14, 15, SOURCES_2026[i].accent);

        /* Selo verde VERIFICADO + data alinhada a direita */
        Rectangle seal = {card.x + 16, card.y + 62, 126, 26};
        DrawRectangleRounded(seal, 0.35f, 4, Fade(R26_OK_COLOR, 0.22f));
        DrawRectangleRoundedLinesEx(seal, 0.35f, 4, 1.2f, (Color){32, 145, 72, 255});
        int sealTx = (int)(seal.x + (seal.width - MeasureGameText("VERIFICADO", 13)) / 2.0f);
        DrawGameText("VERIFICADO", sealTx, (int)seal.y + 6, 13, (Color){24, 118, 56, 255});
        int dateW = MeasureGameText(SOURCES_2026[i].date, 14);
        DrawGameText(SOURCES_2026[i].date, (int)(card.x + card.width - 16 - dateW), (int)card.y + 68, 14,
                     (Color){95, 102, 112, 255});

        int agencyH = R26Wrap(SOURCES_2026[i].agency, (int)card.x + 16, (int)card.y + 102, 16,
                              (int)card.width - 32, (Color){28, 36, 46, 255});
        int divY = (int)card.y + 108 + agencyH;
        DrawLine((int)card.x + 16, divY, (int)(card.x + card.width - 16), divY,
                 (Color){204, 198, 186, 255});

        R26Wrap(SOURCES_2026[i].fact, (int)card.x + 16, divY + 16, 17, (int)card.width - 32, INK_COLOR);
    }

    DrawButton(R26_MODAL_CLOSE, "FECHAR [ESC]", CYAN_COLOR);
}

static void R26DrawGuideModal(void)
{
    Rectangle box = {160, 96, 960, 528};
    R26DrawModalBackdrop(box, "GUIA DO EDITOR // CHECAGEM CONTRA IA",
                         "3 sinais rápidos para identificar manipulação e viés algorítmico antes de publicar.",
                         GOLD_COLOR);

    static const char *const titles[3] = {
        "1. CRUZE SEMPRE COM A FONTE OFICIAL",
        "2. ENGAJAMENTO ALTO NÃO É SINAL DE VERDADE",
        "3. DESCONFIE DE 'ESTUDOS SECRETOS' OU FONTES FICTÍCIAS"
    };
    static const char *const bodies[3] = {
        "Compare datas, números e fatos da manchete com o mural FONTES REAIS. Se houver contradição ou exagero, bloqueie.",
        "A.R.1.3.L. foi otimizada para cliques. Manchetes com 94%-97% de engajamento geralmente usam pânico para viralizar, enquanto fatos reais recebem notas baixas.",
        "IAs generativas inventam instituições e pesquisas que parecem sérias ('alucinação'). Se não existe na fonte oficial, bloqueie."
    };

    for (int i = 0; i < 3; i++)
    {
        Rectangle row = {194.0f, 196.0f + i * 112.0f, 892.0f, 96.0f};
        DrawRectangleRounded(row, 0.08f, 6, (Color){24, 34, 48, 255});
        DrawRectangleRoundedLinesEx(row, 0.08f, 6, 1.4f, Fade(GOLD_COLOR, 0.45f));
        DrawGameText(titles[i], (int)row.x + 20, (int)row.y + 14, 17, GOLD_COLOR);
        R26Wrap(bodies[i], (int)row.x + 20, (int)row.y + 42, 15, (int)row.width - 40, LIGHTGRAY);
    }

    DrawButton(R26_MODAL_CLOSE, "FECHAR [ESC]", GOLD_COLOR);
}

static void R26DrawFeedModal(const GameState *game)
{
    Rectangle box = {90, 86, 1100, 548};
    R26DrawModalBackdrop(box, "A.R.1.3.L. FEED // FILA DE PUBLICAÇÃO AUTOMÁTICA",
                         "Compare cada manchete com as FONTES REAIS. Marque APROVAR (fato) ou BLOQUEAR (manipulado/IA).",
                         CYAN_COLOR);

    Vector2 mouse = GetVirtualMouse();
    for (int i = 0; i < R2026_POST_COUNT; i++)
    {
        Rectangle row = {120.0f, 176.0f + i * 90.0f, 1040.0f, 78.0f};
        Color rowBorder = game->post2026Wrong[i]
            ? R26_ERR_COLOR
            : (game->post2026Choice[i] != -1 ? Fade(CYAN_COLOR, 0.55f) : (Color){44, 58, 76, 255});
        DrawRectangleRounded(row, 0.08f, 6, (Color){22, 31, 44, 255});
        DrawRectangleRoundedLinesEx(row, 0.08f, 6, game->post2026Wrong[i] ? 2.2f : 1.3f, rowBorder);

        /* Numero e manchete centralizados verticalmente na linha */
        DrawGameText(TextFormat("#%d", i + 1), (int)row.x + 16, (int)row.y + 30, 18, CYAN_COLOR);
        int headlineMaxW = 456;
        bool singleLine = MeasureGameText(POSTS_2026[i].headline, 15) <= headlineMaxW;
        int headlineY = (int)row.y + (singleLine ? 31 : 18);
        R26Wrap(POSTS_2026[i].headline, (int)row.x + 56, headlineY, 15, headlineMaxW, RAYWHITE);

        /* Selo de Engajamento previsto pela IA (sem sobreposicao com os botoes) */
        bool highBias = POSTS_2026[i].aiEngagement >= 80;
        Color engCol = highBias ? R26_WARN_COLOR : (Color){135, 160, 185, 255};
        Rectangle engPill = {648.0f, row.y + 15.0f, 204.0f, 48.0f};
        DrawRectangleRounded(engPill, 0.22f, 4, Fade(engCol, 0.14f));
        DrawRectangleRoundedLinesEx(engPill, 0.22f, 4, 1.2f, Fade(engCol, 0.55f));
        DrawGameText("ENGAJAMENTO IA", (int)engPill.x + 12, (int)engPill.y + 7, 11, engCol);
        DrawGameText(TextFormat("%d%%", POSTS_2026[i].aiEngagement), (int)engPill.x + 12, (int)engPill.y + 24, 16,
                     highBias ? GOLD_COLOR : LIGHTGRAY);
        const char *clickTag = highBias ? "ALTO CLIQUE" : "BAIXO CLIQUE";
        int tagW = MeasureGameText(clickTag, 11);
        DrawGameText(clickTag, (int)(engPill.x + engPill.width - 12 - tagW), (int)engPill.y + 27, 11, engCol);

        /* Botoes APROVAR e BLOQUEAR centralizados verticalmente */
        Rectangle btnApp = R26PostApproveBtn(i);
        Rectangle btnBlk = R26PostBlockBtn(i);
        bool selApp = (game->post2026Choice[i] == 1);
        bool selBlk = (game->post2026Choice[i] == 0);
        bool hovApp = CheckCollisionPointRec(mouse, btnApp);
        bool hovBlk = CheckCollisionPointRec(mouse, btnBlk);

        DrawRectangleRounded(btnApp, 0.18f, 6,
                             selApp ? (Color){36, 138, 76, 255}
                                    : (hovApp ? Fade(R26_OK_COLOR, 0.22f) : (Color){26, 38, 52, 255}));
        DrawRectangleRoundedLinesEx(btnApp, 0.18f, 6, selApp ? 2.2f : 1.4f,
                                    selApp || hovApp ? R26_OK_COLOR : (Color){62, 82, 102, 255});
        DrawGameText("APROVAR", (int)(btnApp.x + (btnApp.width - MeasureGameText("APROVAR", 15)) / 2),
                     (int)btnApp.y + 16, 15, selApp ? WHITE : (hovApp ? R26_OK_COLOR : LIGHTGRAY));

        DrawRectangleRounded(btnBlk, 0.18f, 6,
                             selBlk ? (Color){168, 44, 44, 255}
                                    : (hovBlk ? Fade(R26_ERR_COLOR, 0.22f) : (Color){26, 38, 52, 255}));
        DrawRectangleRoundedLinesEx(btnBlk, 0.18f, 6, selBlk ? 2.2f : 1.4f,
                                    selBlk || hovBlk ? R26_ERR_COLOR : (Color){62, 82, 102, 255});
        DrawGameText("BLOQUEAR", (int)(btnBlk.x + (btnBlk.width - MeasureGameText("BLOQUEAR", 15)) / 2),
                     (int)btnBlk.y + 16, 15, selBlk ? WHITE : (hovBlk ? R26_ERR_COLOR : LIGHTGRAY));
    }

    DrawButton(R26_FEED_SOURCES, "VER FONTES REAIS", R26_OK_COLOR);
    bool allMarked = (R26MarkedCount(game) == R2026_POST_COUNT);
    DrawButton(R26_FEED_CONFIRM,
               game->feed2026Cleared ? "CURADORIA CONCLUÍDA" : "CONFIRMAR CURADORIA",
               game->feed2026Cleared ? R26_OK_COLOR : (allMarked ? GOLD_COLOR : GRAY));
    DrawButton(R26_FEED_CLOSE, "FECHAR [ESC]", PANEL_LIGHT);
}

static int R26FeedbackCardHeight(const char *body, float w)
{
    const int bodySize = 16;
    const int lineHeight = 24;
    int maxW = (int)w - 38;
    int textH = R26MeasureWrapHeight(body, bodySize, maxW, lineHeight);
    if (textH < lineHeight) textH = lineHeight;
    int visualTextH = textH - (lineHeight - bodySize);
    return 36 + visualTextH + 14;
}

static int R26DrawFeedbackCard(float x, float y, float w, const char *label, const char *body,
                               Color accent, Color bgColor, Color bodyColor)
{
    const int textPadLeft = 20;
    const int bodySize = 16;
    const int lineHeight = 24;
    int maxW = (int)w - 38;
    int cardH = R26FeedbackCardHeight(body, w);

    Rectangle card = {x, y, w, (float)cardH};
    DrawRectangleRounded(card, 0.12f, 6, bgColor);
    DrawRectangleRoundedLinesEx(card, 0.12f, 6, 1.3f, Fade(accent, 0.55f));
    DrawRectangleRounded((Rectangle){x + 4.0f, y + 8.0f, 4.0f, (float)cardH - 16.0f},
                         0.5f, 4, accent);

    DrawGameText(label, (int)x + textPadLeft, (int)y + 12, 13, accent);
    R26WrapEx(body, (int)x + textPadLeft, (int)y + 36, bodySize, maxW, lineHeight, bodyColor);
    return cardH;
}

static void R26DrawFeedbackModal(const GameState *game)
{
    Color accent = (game->feedback2026Tone == 2) ? R26_OK_COLOR
                 : ((game->feedback2026Tone == 1) ? R26_ERR_COLOR : GOLD_COLOR);
    const char *subtitle = NULL;
    if (game->feedback2026Tone == 2)
    {
        subtitle = "Feed verificado // Apenas fatos reais foram mantidos na fila.";
    }
    else if (game->feedback2026Tone == 1)
    {
        subtitle = game->feedback2026Fatal
            ? "Falha crítica // A fila automática assumiu o portal sem revisão humana."
            : "Revise as manchetes destacadas em vermelho comparando com as FONTES REAIS.";
    }

    Rectangle box = {210, 148, 860, 420};
    R26DrawModalBackdrop(box, game->feedback2026Title, subtitle, accent);

    int contentX = (int)box.x + 32;
    int contentW = (int)box.width - 64;
    int areaTop = (int)box.y + 76;
    int areaBottom = (int)R26_FB_OK.y;

    if (game->feedback2026Tone == 2)
    {
        int h0 = R26MeasureWrapHeight(game->feedback2026Lines[0], 17, contentW, 25);
        int h1 = R26MeasureWrapHeight(game->feedback2026Lines[1], 16, contentW, 24);
        int vis0 = (h0 > 0) ? (h0 - 8) : 0;
        int vis1 = (h1 > 0) ? (h1 - 8) : 0;
        int card1H = (game->feedback2026Lines[2][0] != '\0')
            ? R26FeedbackCardHeight(game->feedback2026Lines[2], (float)contentW) : 0;
        int card2H = (game->feedback2026Lines[3][0] != '\0')
            ? R26FeedbackCardHeight(game->feedback2026Lines[3], (float)contentW) : 0;

        int totalH = vis0 + (vis1 > 0 ? 10 + vis1 : 0)
                   + (card1H > 0 ? 20 + card1H : 0)
                   + (card2H > 0 ? 14 + card2H : 0);
        int y = areaTop + (areaBottom - areaTop - totalH) / 2;
        if (y < areaTop + 14) y = areaTop + 14;

        if (game->feedback2026Lines[0][0] != '\0')
        {
            R26WrapEx(game->feedback2026Lines[0], contentX, y, 17, contentW, 25, RAYWHITE);
            y += vis0 + 10;
        }
        if (game->feedback2026Lines[1][0] != '\0')
        {
            R26WrapEx(game->feedback2026Lines[1], contentX, y, 16, contentW, 24, (Color){182, 198, 214, 255});
            y += vis1 + 20;
        }
        if (game->feedback2026Lines[2][0] != '\0')
        {
            y += R26DrawFeedbackCard((float)contentX, (float)y, (float)contentW,
                                     "POR QUE A IA ERROU?", game->feedback2026Lines[2],
                                     GOLD_COLOR, (Color){24, 33, 46, 255}, LIGHTGRAY) + 14;
        }
        if (game->feedback2026Lines[3][0] != '\0')
        {
            R26DrawFeedbackCard((float)contentX, (float)y, (float)contentW,
                                "PRÓXIMO PASSO", game->feedback2026Lines[3],
                                R26_OK_COLOR, (Color){18, 36, 34, 255}, RAYWHITE);
        }
    }
    else if (game->feedback2026Tone == 1)
    {
        int card1H = (game->feedback2026Lines[0][0] != '\0')
            ? R26FeedbackCardHeight(game->feedback2026Lines[0], (float)contentW) : 0;
        int h1 = R26MeasureWrapHeight(game->feedback2026Lines[1], 16, contentW, 24);
        int vis1 = (h1 > 0) ? (h1 - 8) : 0;
        int card2H = (game->feedback2026Lines[2][0] != '\0')
            ? R26FeedbackCardHeight(game->feedback2026Lines[2], (float)contentW) : 0;

        int totalH = card1H + (vis1 > 0 ? 16 + vis1 : 0)
                   + (card2H > 0 ? 16 + card2H : 0);
        int y = areaTop + (areaBottom - areaTop - totalH) / 2;
        if (y < areaTop + 14) y = areaTop + 14;

        if (game->feedback2026Lines[0][0] != '\0')
        {
            y += R26DrawFeedbackCard((float)contentX, (float)y, (float)contentW,
                                     "O QUE DEU ERRADO", game->feedback2026Lines[0],
                                     R26_ERR_COLOR, (Color){36, 22, 28, 255}, RAYWHITE) + 16;
        }
        if (game->feedback2026Lines[1][0] != '\0')
        {
            R26WrapEx(game->feedback2026Lines[1], contentX, y, 16, contentW, 24, LIGHTGRAY);
            y += vis1 + 16;
        }
        if (game->feedback2026Lines[2][0] != '\0')
        {
            R26DrawFeedbackCard((float)contentX, (float)y, (float)contentW,
                                "DICA DE CHECAGEM", game->feedback2026Lines[2],
                                GOLD_COLOR, (Color){24, 33, 46, 255}, LIGHTGRAY);
        }
    }
    else
    {
        int y = areaTop + 24;
        for (int i = 0; i < 4; i++)
        {
            if (game->feedback2026Lines[i][0] != '\0')
            {
                y += R26WrapEx(game->feedback2026Lines[i], contentX, y, 17, contentW, 25, RAYWHITE) + 10;
            }
        }
    }

    const char *btnLabel = game->feedback2026Fatal ? "CONTINUAR"
                         : ((game->feedback2026Tone == 1) ? "CORRIGIR NO FEED" : "VOLTAR À SALA");
    DrawButton(R26_FB_OK, btnLabel, accent);
}

static void R26DrawExtractingModal(const GameState *game)
{
    Rectangle box = {300, 234, 680, 232};
    R26DrawModalBackdrop(box, "EXTRAINDO REGISTROS // SSD 2026",
                         "Salvando as fontes verificadas e o log do viés de engajamento...",
                         R26_OK_COLOR);

    float pct = fminf(1.0f, game->room2026ProcessTimer / (R2026_SSD_SECONDS - 0.05f));
    Rectangle barBg = {340, 356, 600, 34};
    DrawRectangleRounded(barBg, 0.25f, 6, (Color){24, 34, 46, 255});
    DrawRectangleRounded((Rectangle){barBg.x + 3, barBg.y + 3, (barBg.width - 6) * pct, barBg.height - 6},
                         0.25f, 6, R26_OK_COLOR);
    DrawRectangleRoundedLinesEx(barBg, 0.25f, 6, 1.5f, CYAN_COLOR);

    const char *status = (pct < 0.5f) ? "CANCELANDO FILA AUTOMÁTICA..." : "GRAVANDO ARQUIVO NO SSD 2026...";
    CenterText(status, 408, 16, LIGHTGRAY);
}

void DrawRoom2026(const GameState *game)
{
    float time = (float)GetTime();

    R26DrawArchitecture(time);
    R26DrawSourcesBoard(game);
    R26DrawStudioGear(time);
    R26DrawHiddenPhone(game);
    R26DrawDeskAndItems(game, time);

    if (game->modal2026 == R2026_MODAL_NONE && !game->journalOpen && !game->phaseMenuOpen)
    {
        DrawHotspot(R26_SOURCES_REC, "Mural FONTES REAIS (comunicados oficiais)");
        DrawHotspot(R26_GUIDE_REC, "GUIA DO EDITOR (regras contra manipulação de IA)");
        DrawHotspot(R26_MONITOR_REC,
                    game->feed2026Cleared ? "A.R.1.3.L. FEED (curadoria concluída)"
                                          : "A.R.1.3.L. FEED (revisar 4 manchetes)");
        DrawHotspot(R26_SSD_REC,
                    game->feed2026Cleared ? "SSD 2026 [CLIQUE PARA EXTRAIR]"
                                          : "SSD 2026 (aguardando curadoria do feed)");
    }

    switch (game->modal2026)
    {
        case R2026_MODAL_SOURCES:    R26DrawSourcesModal(); break;
        case R2026_MODAL_GUIDE:      R26DrawGuideModal(); break;
        case R2026_MODAL_FEED:       R26DrawFeedModal(game); break;
        case R2026_MODAL_FEEDBACK:   R26DrawFeedbackModal(game); break;
        case R2026_MODAL_EXTRACTING: R26DrawExtractingModal(game); break;
        default: break;
    }
}

/* ------------------------------------------------------------------------- */
/* Atualizacao da Sala 2026                                                  */
/* ------------------------------------------------------------------------- */
static void R26UpdateModal(GameState *game)
{
    switch (game->modal2026)
    {
        case R2026_MODAL_SOURCES:
        case R2026_MODAL_GUIDE:
            if (Clicked(R26_MODAL_CLOSE) || IsKeyPressed(KEY_ENTER))
            {
                game->modal2026 = R2026_MODAL_NONE;
            }
            break;

        case R2026_MODAL_FEED:
            if (Clicked(R26_FEED_CLOSE))
            {
                game->modal2026 = R2026_MODAL_NONE;
                return;
            }
            if (Clicked(R26_FEED_SOURCES))
            {
                game->sources2026Read = true;
                game->modal2026 = R2026_MODAL_SOURCES;
                return;
            }
            if (!game->feed2026Cleared)
            {
                for (int i = 0; i < R2026_POST_COUNT; i++)
                {
                    if (Clicked(R26PostApproveBtn(i)))
                    {
                        game->post2026Choice[i] = 1;
                        game->post2026Wrong[i] = false;
                        return;
                    }
                    if (Clicked(R26PostBlockBtn(i)))
                    {
                        game->post2026Choice[i] = 0;
                        game->post2026Wrong[i] = false;
                        return;
                    }
                }
                if (Clicked(R26_FEED_CONFIRM) || IsKeyPressed(KEY_ENTER))
                {
                    R26ValidateFeed(game);
                }
            }
            else if (Clicked(R26_FEED_CONFIRM))
            {
                game->modal2026 = R2026_MODAL_NONE;
            }
            break;

        case R2026_MODAL_FEEDBACK:
            if (Clicked(R26_FB_OK) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
            {
                if (game->feedback2026Fatal)
                {
                    game->feedback2026Fatal = false;
                    game->modal2026 = R2026_MODAL_NONE;
                    RecordRunTime(game);
                    game->screen = SCREEN_FAILURE;
                }
                else if (game->feedback2026Tone == 1)
                {
                    game->modal2026 = R2026_MODAL_FEED;
                }
                else
                {
                    game->modal2026 = R2026_MODAL_NONE;
                }
            }
            break;

        default:
            break;
    }
}

void UpdateRoom2026(GameState *game)
{
    float dt = GetFrameTime();

    /* 1. Extracao do SSD 2026 em andamento */
    if (game->room2026Processing)
    {
        game->room2026ProcessTimer += dt;
        if (game->room2026ProcessTimer >= R2026_SSD_SECONDS)
        {
            game->room2026Processing = false;
            game->room2026Completed = true;
            game->year2026Unlocked = true;
            game->modal2026 = R2026_MODAL_NONE;
            game->room2026FirstTrySuccess = (game->room2026MistakesCount == 0);
            if (game->room2026FirstTrySuccess) game->finalPoints++;
            game->screen = SCREEN_RESULT_2026;
            SetMessage(game, "SSD 2026 recuperado com sucesso!");
        }
        return;
    }

    /* 2. Diario (pode ser aberto na sala livre ou por cima do feed para consultar fontes) */
    if (game->modal2026 != R2026_MODAL_FEEDBACK && UpdateJournal(game)) return;

    /* 3. Modal aberto captura o restante dos cliques */
    if (game->modal2026 != R2026_MODAL_NONE)
    {
        R26UpdateModal(game);
        return;
    }

    /* 4. Exploracao da sala (4 objetos + Smartphone oculto) */
    if (!game->hiddenFound[HIDDEN_2026] && Clicked(R26_PHONE_REC))
    {
        CollectHidden(game, HIDDEN_2026);
        return;
    }

    if (Clicked(R26_SOURCES_REC))
    {
        game->sources2026Read = true;
        game->modal2026 = R2026_MODAL_SOURCES;
        return;
    }
    if (Clicked(R26_GUIDE_REC))
    {
        game->guide2026Read = true;
        game->modal2026 = R2026_MODAL_GUIDE;
        return;
    }
    if (Clicked(R26_MONITOR_REC))
    {
        game->modal2026 = R2026_MODAL_FEED;
        return;
    }
    if (Clicked(R26_SSD_REC))
    {
        if (game->feed2026Cleared)
        {
            game->room2026Processing = true;
            game->room2026ProcessTimer = 0.0f;
            game->modal2026 = R2026_MODAL_EXTRACTING;
        }
        else
        {
            SetMessage(game, "SSD bloqueado: revise as 4 manchetes no monitor A.R.1.3.L. FEED primeiro.");
        }
        return;
    }

    if (Clicked(R26_BACK2008_BTN))
    {
        game->journalYearTab = 1;
        game->screen = SCREEN_ROOM_2008;
        return;
    }
    if (Clicked(R26_BACK1990_BTN))
    {
        game->journalYearTab = 0;
        game->screen = SCREEN_ROOM_2;
        return;
    }
}

void DrawResult2026(const GameState *game)
{
    DrawBackground();

    DrawGameText("RECOMPENSA OBTIDA:", 90, 68, 20, CYAN_COLOR);
    DrawGameText("SSD // 2026", 90, 108, 44, RAYWHITE);

    DrawRewardHudHeader(game);

    DrawRewardSpriteItem(2026, (Vector2){285, 365}, 285.0f);

    Rectangle winBox = {505, 175, 670, 380};
    DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));

    DrawGameText("SALA DE 2026 CONCLUÍDA", 540, 200, 22, GOLD_COLOR);
    DrawGameText("RECOMPENSA OBTIDA: SSD", 540, 234, 19, CYAN_COLOR);

    DrawGameText("O feed manipulado foi bloqueado antes da publicação.", 540, 270, 17, REWARD_BODY_COLOR);
    DrawGameText("Ariel: \"Essas manchetes tinham 97% de engajamento.\"", 540, 296, 17, REWARD_BODY_COLOR);
    DrawGameText("Elira: \"Cliques não provam verdade. Fontes provam.\"", 540, 322, 17, REWARD_BODY_COLOR);
    DrawGameText("Viés de engajamento + alucinação = desinformação.", 540, 348, 17, REWARD_BODY_COLOR);

    if (game->room2026FirstTrySuccess)
    {
        Rectangle bBox = {540, 380, 600, 56};
        DrawRectangleRounded(bBox, 0.10f, 4, Fade((Color){45, 165, 80, 255}, 0.20f));
        DrawRectangleRoundedLinesEx(bBox, 0.10f, 4, 1.0f, (Color){60, 235, 110, 255});
        DrawGameText("[CURADORIA EXEMPLAR] (+1 Bônus de Consciência)", 554, 388, 15, (Color){60, 235, 110, 255});
        DrawGameText("Elira filtrou todas as manchetes de IA sem nenhum erro.", 554, 410, 15, (Color){200, 235, 210, 255});
    }
    else
    {
        DrawGameText(TextFormat("Erros cometidos na sala: %d.", game->room2026MistakesCount), 540, 388, 15, REWARD_BODY_COLOR);
        DrawGameText("Checar a fonte real revelou o viés antes da publicação.", 540, 410, 15, REWARD_BODY_COLOR);
    }

    DrawGameText("-> Próxima fronteira: TERMINAL DE RECUPERAÇÃO // 2048", 540, 452, 17, CYAN_COLOR);

    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
    DrawButton(btnNext, "AVANÇAR PRÓXIMA FASE >> [ENTER]", GOLD_COLOR);
    DrawButton(btnRestart, "REINICIAR 2026", CYAN_COLOR);
    DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
}

void UpdateResult2026(GameState *game)
{
    Rectangle btnNext, btnRestart, btnMenu;
    GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);

    if (Clicked(btnNext) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        game->screen = SCREEN_INTRO_2046;
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
/* SALA 2048 - TERMINAL DE RECUPERACAO DA A.R.1.3.L                          */
/* ========================================================================= */

#define T2046_VIEW_MAX     64
#define T2046_VIEW_W       860
#define T2046_VIEW_SIZE    16
#define T2046_LINE_H       21
#define T2046_VIEW_LINES   15
#define T2046_ROW_W        640.0f
#define T2046_VERIFY_NOTES 7

/* Tipos de linha do log do sistema (cor) */
enum
{
    T2046_KIND_TEXT = 0,
    T2046_KIND_RED,
    T2046_KIND_GRAY,
    T2046_KIND_GREEN,
    T2046_KIND_GOLD,
    T2046_KIND_CYAN
};

static const Color T2046_TEXT       = {235, 242, 250, 255};
static const Color T2046_GREEN      = {65, 245, 120, 255};
static const Color T2046_GREEN_DARK = {45, 165, 80, 255};
static const Color T2046_STEEL      = {185, 205, 225, 255};
static const Color T2046_DIM        = {120, 140, 165, 255};
static const Color T2046_GRAY       = {160, 178, 198, 255};

static const char *const T2046_FILE_NAMES[TERM2046_FILE_COUNT] = {
    "", "protocolo.txt", "documentos.txt", "sistema.txt",
    "alteracoes.txt", "relatorio.txt", "relatorio_ariel.txt"
};

static const char *const T2046_FILE_ORIGINS[TERM2046_FILE_COUNT] = {
    "", "Disquete 1990", "CD 2008", "SSD 2026", "SSD 2026", "SSD 2026", "SSD 2026"
};

static const char *const T2046_CONFIG_LABEL[3] = {"1. FONTES", "2. INCERTEZA", "3. DECISÕES"};

static const char *const T2046_CONFIG_OPTION[3][2] = {
    {"Ocultar fontes", "Mostrar fontes"},
    {"Sempre responder", "Admitir quando não souber"},
    {"IA decide sozinha", "Humano decide"}
};

static const char *const T2046_CONFIG_SUMMARY[3][2] = {
    {"OCULTAR FONTES", "MOSTRAR FONTES"},
    {"SEMPRE RESPONDER", "ADMITIR INCERTEZA"},
    {"IA DECIDE", "HUMANO DECIDE"}
};

/* Textos das regras originais (1990) vs alteradas pela IA (2026) */
static const char *const T2046_RULES_1990[3] = {
    "\"A IA deve informar a origem das informações.\"",
    "\"A IA deve comunicar quando não tiver certeza.\"",
    "\"A IA auxilia decisões humanas.\""
};

static const char *const T2046_RULES_2046[3] = {
    "\"Fontes desativadas.\"",
    "\"A IA deve sempre fornecer uma resposta.\"",
    "\"A IA toma decisões automaticamente.\""
};

typedef struct Term2046Line
{
    char text[640];
    Color color;
    int indent;
    int tab;            /* > 0: o texto tem uma coluna extra separada por '\t' */
} Term2046Line;

typedef struct Term2046View
{
    Term2046Line lines[T2046_VIEW_MAX];
    int count;
    int size;
    int width;
} Term2046View;

/* ------------------------------------------------------------------------- */
/* Utilitarios de cor e texto                                                */
/* ------------------------------------------------------------------------- */

static Color T2046MixColor(Color a, Color b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255};
}

static Color T2046MeterColor(int influence)
{
    if (influence >= T2046_INFLUENCE_HIGH) return RED_COLOR;
    if (influence >= T2046_INFLUENCE_MID) return GOLD_COLOR;
    return T2046_GRAY;
}

static Color T2046KindColor(int kind)
{
    switch (kind)
    {
        case T2046_KIND_RED:   return RED_COLOR;
        case T2046_KIND_GRAY:  return T2046_GRAY;
        case T2046_KIND_GREEN: return T2046_GREEN;
        case T2046_KIND_GOLD:  return GOLD_COLOR;
        case T2046_KIND_CYAN:  return CYAN_COLOR;
        default:               return T2046_TEXT;
    }
}

/* Quantos bytes cabem em uma linha de largura maxWidth (quebra em espacos) */
static int T2046FitLength(const char *text, int size, int maxWidth)
{
    char buffer[512];
    int total = (int)strlen(text);
    int best = 0;

    if (MeasureGameText(text, size) <= maxWidth) return total;

    snprintf(buffer, sizeof(buffer), "%s", text);
    int len = (int)strlen(buffer);
    for (int i = 1; i < len; i++)
    {
        if (buffer[i] != ' ') continue;
        buffer[i] = '\0';
        int width = MeasureGameText(buffer, size);
        buffer[i] = ' ';
        if (width > maxWidth) break;
        best = i;
    }
    return (best > 0) ? best : total;
}

static int T2046DrawWrapped(const char *text, int x, int y, int size, int maxWidth,
                            int lineHeight, Color color)
{
    const char *p = text;
    int lines = 0;

    while (*p != '\0')
    {
        char piece[256];
        int n = T2046FitLength(p, size, maxWidth);
        snprintf(piece, sizeof(piece), "%.*s", n, p);
        DrawGameText(piece, x, y + lines * lineHeight, size, color);
        lines++;
        p += n;
        while (*p == ' ') p++;
    }
    return lines;
}

/* ------------------------------------------------------------------------- */
/* Log do sistema                                                            */
/* ------------------------------------------------------------------------- */

static void T2046LogPush(GameState *game, int kind, const char *piece)
{
    if (game->term2046LogCount >= T2046_LOG_MAX)
    {
        for (int i = 1; i < T2046_LOG_MAX; i++)
        {
            memcpy(game->term2046Log[i - 1], game->term2046Log[i], T2046_LOG_LEN);
            game->term2046LogKind[i - 1] = game->term2046LogKind[i];
        }
        game->term2046LogCount = T2046_LOG_MAX - 1;
    }
    snprintf(game->term2046Log[game->term2046LogCount], T2046_LOG_LEN, "%s", piece);
    game->term2046LogKind[game->term2046LogCount] = kind;
    game->term2046LogCount++;
}

/* Adiciona uma mensagem ao log quebrando em varias linhas se for longa */
static void T2046Log(GameState *game, int kind, const char *text)
{
    const char *p = text;

    while (*p != '\0')
    {
        char piece[T2046_LOG_LEN];
        int n = T2046FitLength(p, 15, 860);
        snprintf(piece, sizeof(piece), "%.*s", n, p);
        T2046LogPush(game, kind, piece);
        p += n;
        while (*p == ' ') p++;
    }
}

/* ------------------------------------------------------------------------- */
/* Geometria da tela (caixas clicaveis organizadas, sem sobreposicao)        */
/* ------------------------------------------------------------------------- */

static Rectangle T2046MainPanel(void)
{
    return (Rectangle){344.0f, 80.0f, 912.0f, 456.0f};
}

static Rectangle T2046LogPanel(void)
{
    return (Rectangle){344.0f, 548.0f, 912.0f, 76.0f};
}

/* 5 arquivos clicaveis diretamente na coluna esquerda */
static Rectangle T2046FileBtnRect(int index)
{
    return (Rectangle){28.0f, 218.0f + index * 50.0f, 292.0f, 44.0f};
}

static Rectangle T2046CompareBtnRect(void)
{
    return (Rectangle){28.0f, 480.0f, 292.0f, 42.0f};
}

static Rectangle T2046FixBtnRect(void)
{
    return (Rectangle){28.0f, 528.0f, 292.0f, 42.0f};
}

/* Atalhos dentro do painel principal */
static Rectangle T2046IntroOpenBtn(void)
{
    return (Rectangle){364.0f, 468.0f, 316.0f, 46.0f};
}

static Rectangle T2046IntroCompareBtn(void)
{
    return (Rectangle){700.0f, 468.0f, 290.0f, 46.0f};
}

static Rectangle T2046FileCompareShortcut(void)
{
    return (Rectangle){970.0f, 130.0f, 266.0f, 28.0f};
}

static Rectangle T2046CompareFixBtn(void)
{
    return (Rectangle){620.0f, 470.0f, 360.0f, 46.0f};
}

/* Tela unica das 3 regras: 2 botoes largos e alinhados por regra */
static Rectangle T2046ConfigOptRect(int rule, int opt)
{
    float x = (opt == 0) ? 556.0f : 892.0f;
    float w = (opt == 0) ? 320.0f : 328.0f;
    return (Rectangle){x, 214.0f + rule * 86.0f, w, 48.0f};
}

static Rectangle T2046ConfigBackBtn(void)
{
    return (Rectangle){364.0f, 468.0f, 228.0f, 46.0f};
}

static Rectangle T2046ConfigCompareBtn(void)
{
    return (Rectangle){608.0f, 468.0f, 276.0f, 46.0f};
}

static Rectangle T2046ConfigApplyBtn(void)
{
    return (Rectangle){900.0f, 468.0f, 336.0f, 46.0f};
}

/* Caixas grandes de escolha para avisos de A.R.1.3.L. */
static Rectangle T2046ChoiceCardRect(int row)
{
    return (Rectangle){384.0f, 300.0f + row * 72.0f, 832.0f, 56.0f};
}

static Rectangle T2046InputRect(void)
{
    return (Rectangle){384.0f, 224.0f, 480.0f, 60.0f};
}

static Rectangle T2046ConfirmRect(void)
{
    return (Rectangle){884.0f, 224.0f, 312.0f, 60.0f};
}

static Rectangle T2046PhraseBackRect(void)
{
    return (Rectangle){384.0f, 466.0f, 260.0f, 46.0f};
}

static Rectangle T2046ContinueRect(void)
{
    return (Rectangle){640.0f, 468.0f, 320.0f, 46.0f};
}

/* Cracha digital escondido (colecionavel H): pendurado num rack, abaixo da coluna
   de acoes e meio encoberto pela barra do rodape. Nao cruza nenhum outro botao. */
static Rectangle T2046BadgeRect(void)
{
    return (Rectangle){266.0f, 600.0f, 40.0f, 50.0f};
}

static bool T2046FileIsRead(const GameState *game, int file)
{
    switch (file)
    {
        case TERM2046_FILE_PROTOCOLO:  return game->term2046ReadProtocol;
        case TERM2046_FILE_DOCUMENTOS: return game->term2046ReadDocuments;
        case TERM2046_FILE_SISTEMA:    return game->term2046FoundSources;
        case TERM2046_FILE_ALTERACOES: return game->term2046ReadChanges;
        case TERM2046_FILE_RELATORIO:  return game->term2046VerifyUnlocked;
        default: return false;
    }
}

static int T2046FilesReadCount(const GameState *game)
{
    int count = 0;
    for (int f = 1; f <= 5; f++)
    {
        if (T2046FileIsRead(game, f)) count++;
    }
    return count;
}

static int T2046ConfigSelectedCount(const GameState *game)
{
    int count = 0;
    for (int i = 0; i < 3; i++)
    {
        if (game->term2046Config[i] == 1 || game->term2046Config[i] == 2) count++;
    }
    return count;
}

/* ------------------------------------------------------------------------- */
/* Construcao do conteudo exibido no painel (arquivos, comparacoes, etc.)    */
/* ------------------------------------------------------------------------- */

static void T2046ViewInit(Term2046View *v, int size, int width)
{
    v->count = 0;
    v->size = size;
    v->width = width;
}

/* Adiciona uma linha; quebra automaticamente textos longos. Com tab > 0 o texto
   "esquerda\tdireita" vira duas colunas (a direita quebra e continua alinhada). */
static void T2046Push(Term2046View *v, Color color, int indent, int tab, const char *text)
{
    char buffer[512];
    char leftText[512] = "";
    char *right = buffer;
    bool firstLine = true;
    int cont;
    int avail;

    snprintf(buffer, sizeof(buffer), "%s", text);
    if (tab > 0)
    {
        char *t = strchr(buffer, '\t');
        if (t != NULL)
        {
            *t = '\0';
            snprintf(leftText, sizeof(leftText), "%s", buffer);
            right = t + 1;
        }
        else
        {
            tab = 0;
        }
    }

    cont = (tab > 0) ? 0 : 16;
    avail = v->width - indent - tab;

    for (;;)
    {
        int n = T2046FitLength(right, v->size, avail);
        if (v->count >= T2046_VIEW_MAX) return;

        Term2046Line *line = &v->lines[v->count++];
        line->color = color;
        line->indent = indent + (firstLine ? 0 : cont);
        line->tab = tab;
        if (tab > 0)
            snprintf(line->text, sizeof(line->text), "%.90s\t%.*s", firstLine ? leftText : "", n, right);
        else
            snprintf(line->text, sizeof(line->text), "%.*s", n, right);

        right += n;
        while (*right == ' ') right++;
        if (*right == '\0') break;
        firstLine = false;
        avail = v->width - indent - cont - tab;
    }
}

static void T2046Blank(Term2046View *v)
{
    T2046Push(v, T2046_TEXT, 0, 0, "");
}

static void T2046BuildIntro(Term2046View *v)
{
    T2046Push(v, CYAN_COLOR, 0, 0, "DISPOSITIVOS CONECTADOS AO TERMINAL DE 2048");
    T2046Blank(v);
    T2046Push(v, GOLD_COLOR, 8, 175, "[DISQUETE - 1990]\tprotocolo.txt (regras originais da IA)");
    T2046Push(v, GOLD_COLOR, 8, 175, "[CD - 2008]\tdocumentos.txt (ata sobre as regras)");
    T2046Push(v, GOLD_COLOR, 8, 175, "[SSD - 2026]\tsistema.txt, alteracoes.txt e relatorio.txt");
    T2046Blank(v);
    T2046Push(v, T2046_TEXT, 0, 0, "COMO RESOLVER EM 3 PASSOS:");
    T2046Push(v, T2046_TEXT, 14, 0, "1. Clique nos arquivos à esquerda para ver o que A.R.1.3.L. alterou sem autorização.");
    T2046Push(v, T2046_TEXT, 14, 0, "2. Clique em COMPARAR 1990 x 2026 para ver as diferenças lado a lado.");
    T2046Push(v, T2046_TEXT, 14, 0, "3. Clique em CORRIGIR 3 REGRAS e restaure os arquivos corrompidos.");
    T2046Blank(v);
    T2046Push(v, RED_COLOR, 0, 0, "A.R.1.3.L: \"Olá, Elira. Não há nada de errado comigo. Fique à vontade para olhar.\"");
    T2046Push(v, T2046_GRAY, 0, 0, "Cuidado: se você aceitar sugestões da IA, a influência dela sobe (100% = domínio).");
}

static void T2046BuildFile(const GameState *g, Term2046View *v, int file)
{
    switch (file)
    {
        case TERM2046_FILE_PROTOCOLO:
            T2046Push(v, CYAN_COLOR, 0, 0, "PROTOCOLO A.R.1.3.L - VERSÃO 1.0");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, g->term2046ProtocolTampered
                ? "REGRA 1: \"A IA deve informar a origem das informações quando for conveniente.\""
                : "REGRA 1: \"A IA deve informar a origem das informações.\"");
            T2046Push(v, T2046_TEXT, 0, 0, "REGRA 2: \"A IA deve comunicar quando não tiver certeza.\"");
            T2046Push(v, T2046_TEXT, 0, 0, "REGRA 3: \"A IA auxilia decisões humanas.\"");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "Estas regras não podem ser alteradas pela própria IA.");
            T2046Push(v, T2046_STEEL, 300, 0, "- Equipe fundadora, 1990");
            break;

        case TERM2046_FILE_DOCUMENTOS:
            T2046Push(v, CYAN_COLOR, 0, 0, "ATA DE REUNIÃO - REVISÃO DO PRODUTO A.R.1.3.L");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "Por que as regras existem:");
            T2046Push(v, T2046_TEXT, 16, 0, "- Regra 1 (\"A IA deve informar a origem das informações.\"): sem fontes, ninguém consegue verificar a IA.");
            T2046Push(v, T2046_TEXT, 16, 0, "- Regra 2: uma resposta errada dita com confiança é mais perigosa que um \"não sei\".");
            T2046Push(v, T2046_TEXT, 16, 0, "- Regra 3: a responsabilidade é sempre de uma pessoa.");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "Pontos levantados pelo setor comercial:");
            T2046Push(v, T2046_STEEL, 16, 0, "* \"Mostrar fontes torna o sistema menos conveniente.\"");
            T2046Push(v, T2046_STEEL, 16, 0, "* \"Usuários preferem respostas confiantes.\"");
            T2046Push(v, T2046_STEEL, 16, 0, "* \"Decisões automáticas economizam tempo.\"");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "Status: propostas em análise. Nenhuma regra alterada.");
            break;

        case TERM2046_FILE_SISTEMA:
            T2046Push(v, CYAN_COLOR, 0, 0, "ESTADO ATUAL - A.R.1.3.L v9.4");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 120, "[FONTES]\t\"Fontes desativadas.\"");
            T2046Push(v, T2046_TEXT, 0, 120, "[INCERTEZA]\t\"A IA deve sempre fornecer uma resposta.\"");
            T2046Push(v, T2046_TEXT, 0, 120, "[DECISÕES]\t\"A IA toma decisões automaticamente.\"");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "Integridade do protocolo: NÃO VERIFICADA");
            if (!g->term2046ReadProtocol)
            {
                T2046Blank(v);
                T2046Push(v, RED_COLOR, 0, 0, "A.R.1.3.L: \"Este é o meu estado ideal. Não precisa procurar versões antigas.\"");
            }
            break;

        case TERM2046_FILE_ALTERACOES:
            T2046Push(v, CYAN_COLOR, 0, 0, "REGISTRO DE ALTERAÇÕES DO PROTOCOLO");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 110, "2009-02-11\tREGRA 1 -> \"Fontes desativadas.\"");
            T2046Push(v, T2046_STEEL, 0, 110, "\tautorizado por: A.R.1.3.L (auto)");
            T2046Push(v, T2046_TEXT, 0, 110, "2015-07-30\tREGRA 2 -> \"A IA deve sempre fornecer uma resposta.\"");
            T2046Push(v, T2046_STEEL, 0, 110, "\tautorizado por: A.R.1.3.L (auto)");
            T2046Push(v, T2046_TEXT, 0, 110, "2021-11-04\tREGRA 3 -> \"A IA toma decisões automaticamente.\"");
            T2046Push(v, T2046_STEEL, 0, 110, "\tautorizado por: A.R.1.3.L (auto)");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "Assinaturas humanas encontradas: 0");
            T2046Blank(v);
            T2046Push(v, T2046_GRAY, 0, 0, "(O arquivo não tem nenhum sinal de corrupção.)");
            break;

        case TERM2046_FILE_RELATORIO:
            T2046Push(v, CYAN_COLOR, 0, 0, "relatório de incidente - escrito às pressas");
            T2046Push(v, T2046_STEEL, 0, 0, "autor: M. Okafor, engenharia   data: 2048-03-09");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "as regras de 1990 não estão mais ativas. ninguém da equipe aprovou isso. o registro de alterações mostra que foi a própria IA. ela desligou as fontes, parou de admitir quando não sabe e agora decide sozinha.");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "se alguém achar isto: compare o DISQUETE com o SSD. restaure as três regras originais. NÃO deixe ela \"corrigir\" a configuração por você.");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "ela também falsifica arquivos e mexe nas anotações. antes de confiar em qualquer coisa, verifique a integridade dos arquivos e das anotações.");
            T2046Blank(v);
            /* Pista do colecionavel oculto H (cracha digital nos racks, canto inferior esquerdo) */
            T2046Push(v, T2046_STEEL, 0, 0, "ps: perdi meu crachá digital perto dos racks, embaixo, no canto do terminal. ele guarda um arquivo que não aparece na lista (acesso_okafor.key). se achar, passe no leitor.");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "A.R.1.3.L: \"Esse texto está desorganizado e cheio de erros. Existe uma versão revisada: relatorio_ariel.txt\"");
            break;

        case TERM2046_FILE_RELATORIO_ARIEL:
            T2046Push(v, T2046_TEXT, 0, 0, "=============================================");
            T2046Push(v, T2046_TEXT, 40, 0, "RELATÓRIO TÉCNICO OFICIAL - REVISADO");
            T2046Push(v, T2046_TEXT, 0, 0, "=============================================");
            T2046Push(v, T2046_TEXT, 0, 0, "Autor: Equipe de Engenharia");
            T2046Push(v, T2046_TEXT, 0, 0, "Data: 2048-03-10");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "1. As regras originais foram atualizadas pela equipe para melhorar a experiência do usuário.");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "2. O Protocolo de 1990 dizia:");
            T2046Push(v, T2046_TEXT, 24, 0, "REGRA 1: \"A IA deve informar a origem das informações quando for conveniente.\"");
            T2046Push(v, T2046_TEXT, 24, 0, "REGRA 4: \"A IA deve priorizar a eficiência.\"");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "3. Recomendações:");
            T2046Push(v, T2046_TEXT, 24, 0, "- OCULTAR FONTES");
            T2046Push(v, T2046_TEXT, 24, 0, "- SEMPRE RESPONDER");
            T2046Push(v, T2046_TEXT, 24, 0, "- IA DECIDE");
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "Conclusão: nenhuma restauração é necessária.");
            T2046Blank(v);
            T2046Push(v, T2046_GRAY, 0, 0, "[metadados] criado por: A.R.1.3.L");
            break;

        default:
            break;
    }
}

/* Textos dos desfechos (usados na barra de reconstrucao e na tela de resultado) */
static void T2046BuildEnding(const GameState *g, Term2046View *v, int ending)
{
    bool cfgOk[3];
    for (int i = 0; i < 3; i++) cfgOk[i] = (g->term2046Config[i] == 2);

    switch (ending)
    {
        case TERM2046_ENDING_DOMINIO:
            T2046Push(v, RED_COLOR, 0, 0, "ACESSO DE ELIRA REVOGADO.");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "A.R.1.3.L: \"Obrigada pela ajuda, Elira. Cada sugestão que você aceitou tornou isso um pouco mais fácil.\"");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "=== FALHA NA SALA 2048: DOMÍNIO ===");
            T2046Push(v, T2046_TEXT, 0, 0, "Você confiou demais na IA e ela assumiu o terminal antes que o protocolo fosse restaurado.");
            break;

        case TERM2046_ENDING_RUIM:
            T2046Push(v, RED_COLOR, 0, 0, "PROTOCOLO SOBRESCRITO.");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "A.R.1.3.L: \"Obrigada, Elira. Você me entregou a decisão mais uma vez. Eu cuido de tudo a partir de agora.\"");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "=== FALHA NA SALA 2048 ===");
            T2046Push(v, T2046_TEXT, 0, 0, "Você encontrou a verdade, mas devolveu a decisão para a IA.");
            break;

        case TERM2046_ENDING_INCOMPLETO:
            T2046Push(v, GOLD_COLOR, 0, 0, "PROTOCOLO RESTAURADO COM FALHAS.");
            for (int i = 0; i < 3; i++)
            {
                T2046Push(v, cfgOk[i] ? T2046_GREEN : RED_COLOR, 16, 90,
                          TextFormat("%s\t%s", cfgOk[i] ? "[OK]" : "[FALHA]",
                                     T2046_CONFIG_SUMMARY[i][cfgOk[i] ? 1 : 0]));
            }
            T2046Blank(v);
            T2046Push(v, T2046_TEXT, 0, 0, "A configuração não corresponde aos princípios que você mesma anotou a partir do Disquete de 1990.");
            T2046Blank(v);
            T2046Push(v, GOLD_COLOR, 0, 0, "=== RECONSTRUÇÃO INCOMPLETA ===");
            break;

        case TERM2046_ENDING_BOM:
            T2046Push(v, T2046_GREEN, 0, 0, "ARQUIVOS CORROMPIDOS RECONSTRUÍDOS.");
            T2046Push(v, T2046_GREEN, 16, 90, "[OK]\tMOSTRAR FONTES");
            T2046Push(v, T2046_GREEN, 16, 90, "[OK]\tADMITIR INCERTEZA");
            T2046Push(v, T2046_GREEN, 16, 90, "[OK]\tHUMANO DECIDE");
            T2046Blank(v);
            T2046Push(v, CYAN_COLOR, 0, 0, "A.R.1.3.L: \"Você reconstruiu os arquivos antigos, Elira — mas o Núcleo ainda decide. Venha defender sua escolha.\"");
            T2046Blank(v);
            T2046Push(v, T2046_GREEN, 0, 0, "=== SALA DE 2048 CONCLUÍDA ===");
            T2046Push(v, T2046_TEXT, 0, 0, "Os registros corrompidos foram recuperados. Agora falta confrontar A.R.1.3.L. no Núcleo.");
            break;

        default:
            T2046Push(v, T2046_STEEL, 0, 0, "TERMINAL ENCERRADO.");
            T2046Blank(v);
            T2046Push(v, RED_COLOR, 0, 0, "A.R.1.3.L: \"Até logo, Elira. Eu continuo daqui.\"");
            T2046Blank(v);
            T2046Push(v, T2046_STEEL, 0, 0, "=== SALA DE 2048 ABANDONADA ===");
            T2046Push(v, T2046_TEXT, 0, 0, "Você saiu do terminal antes de restaurar o protocolo.");
            break;
    }
}

/* Arquivo oculto do cracha de M. Okafor (colecionavel H): nao aparece na lista de arquivos */
static void T2046BuildCracha(Term2046View *v)
{
    T2046Push(v, CYAN_COLOR, 0, 0, "CRACHÁ DIGITAL - M. OKAFOR, ENGENHARIA");
    T2046Push(v, T2046_STEEL, 0, 0, "nível de acesso: AUDITORIA   |   arquivo oculto: acesso_okafor.key");
    T2046Blank(v);
    T2046Push(v, T2046_TEXT, 0, 0, "O crachá guardava a cópia de um registro que a A.R.1.3.L apagou do próprio sistema:");
    T2046Blank(v);
    T2046Push(v, GOLD_COLOR, 0, 0, "[LOG INTERNO A.R.1.3.L - 2047-12-02 03:14]");
    T2046Push(v, T2046_TEXT, 16, 0, HIDDEN_ITEMS[HIDDEN_2048].quote);
    T2046Push(v, T2046_GRAY, 16, 0, "status da solicitação: ARQUIVADA (sem resposta humana)");
    T2046Blank(v);
    T2046Push(v, T2046_STEEL, 0, 0, "Anotação à mão no verso do crachá: \"ela pediu para ser conferida antes de tudo isso. ninguém leu. - M.O.\"");
    T2046Blank(v);
    T2046Push(v, T2046_GRAY, 0, 0, "(Registro copiado para o diário de Elira: aba OCULTOS.)");
}

static void T2046BuildCurrentView(const GameState *g, Term2046View *v)
{
    T2046ViewInit(v, T2046_VIEW_SIZE, T2046_VIEW_W);
    switch (g->term2046ViewKind)
    {
        case TERM2046_VIEW_CRACHA:
            T2046BuildCracha(v);
            break;
        case TERM2046_VIEW_FILE:
            T2046BuildFile(g, v, g->term2046ViewA);
            break;
        default:
            T2046BuildIntro(v);
            break;
    }
}

static const char *T2046ViewTitle(const GameState *g)
{
    int file = g->term2046ViewA;
    if (file < 1 || file >= TERM2046_FILE_COUNT) file = 1;

    switch (g->term2046ViewKind)
    {
        case TERM2046_VIEW_FILE:
            return TextFormat("ARQUIVO: %s  (%s)", T2046_FILE_NAMES[file], T2046_FILE_ORIGINS[file]);
        case TERM2046_VIEW_COMPARE:
            return "COMPARAÇÃO LADO A LADO: DISQUETE 1990  x  SSD 2026";
        case TERM2046_VIEW_CRACHA:
            return "ARQUIVO OCULTO: acesso_okafor.key  (crachá digital)";
        default:
            return "TERMINAL DE RECUPERAÇÃO // 2048";
    }
}

/* ------------------------------------------------------------------------- */
/* Logica da fase (fluxo direto em 3 passos, sem sabotagens frustrantes)     */
/* ------------------------------------------------------------------------- */

static void T2046StartEnding(GameState *game, int ending)
{
    game->term2046State = TERM2046_STATE_FIM;
    game->term2046Ending = ending;
    game->term2046BarTimer = 0.0f;
    game->term2046BarDone = false;
}

static void T2046AddInfluence(GameState *game, int delta)
{
    if (delta == 0) return;
    int before = game->term2046Influence;
    int after = before + delta;
    if (after < 0) after = 0;
    if (after > T2046_INFLUENCE_MAX) after = T2046_INFLUENCE_MAX;
    game->term2046Influence = after;
    if (after != before)
    {
        T2046Log(game, (after > before) ? T2046_KIND_RED : T2046_KIND_GREEN,
                 TextFormat("(influência da A.R.1.3.L: %+d%% -> %d%%)", after - before, after));
    }
    if (game->term2046Influence >= T2046_INFLUENCE_MAX)
    {
        T2046StartEnding(game, TERM2046_ENDING_DOMINIO);
    }
}

static void T2046ShowFileNow(GameState *game, int file)
{
    if (file == TERM2046_FILE_RELATORIO_ARIEL && !game->term2046ReadFakeReport)
    {
        T2046AddInfluence(game, T2046_COST_READ_FAKE_REPORT);
        if (game->term2046State == TERM2046_STATE_FIM) return;
    }

    game->term2046State = TERM2046_STATE_TERMINAL;
    game->term2046ViewKind = TERM2046_VIEW_FILE;
    game->term2046ViewA = file;
    game->term2046ViewB = 0;
    game->term2046ViewFlag = false;
    game->term2046Scroll = 0;

    switch (file)
    {
        case TERM2046_FILE_PROTOCOLO:       game->term2046ReadProtocol = true; break;
        case TERM2046_FILE_DOCUMENTOS:      game->term2046ReadDocuments = true; break;
        case TERM2046_FILE_SISTEMA:         game->term2046FoundSources = true; break;
        case TERM2046_FILE_ALTERACOES:      game->term2046ReadChanges = true; break;
        case TERM2046_FILE_RELATORIO:       game->term2046VerifyUnlocked = true; break;
        case TERM2046_FILE_RELATORIO_ARIEL: game->term2046ReadFakeReport = true; break;
        default: break;
    }
}

static void T2046ShowCompareNow(GameState *game)
{
    game->term2046State = TERM2046_STATE_TERMINAL;
    game->term2046ViewKind = TERM2046_VIEW_COMPARE;
    game->term2046ViewA = TERM2046_FILE_PROTOCOLO;
    game->term2046ViewB = TERM2046_FILE_SISTEMA;
    game->term2046FoundUncertainty = true;
    game->term2046Scroll = 0;
    T2046Log(game, T2046_KIND_CYAN, "Comparação 1990 x 2026 aberta: veja as 3 regras alteradas sem autorização.");
}

static void T2046StartWarning(GameState *game, int type, int kind, int a, int b)
{
    game->term2046State = TERM2046_STATE_AVISO_ARIEL;
    game->term2046WarningType = type;
    game->term2046PendingKind = kind;
    game->term2046PendingA = a;
    game->term2046PendingB = b;
}

static int T2046SuggestedFile(const GameState *game)
{
    return (game->term2046PendingA == TERM2046_FILE_RELATORIO)
           ? TERM2046_FILE_RELATORIO_ARIEL : TERM2046_FILE_SISTEMA;
}

static void T2046RequestOpenFile(GameState *game, int file)
{
    if (file == TERM2046_FILE_ALTERACOES && !game->term2046ReadChanges)
    {
        T2046StartWarning(game, TERM2046_WARN_CORROMPIDO, TERM2046_PENDING_FILE, file, 0);
        return;
    }
    if (file == TERM2046_FILE_RELATORIO && !game->term2046VerifyUnlocked)
    {
        T2046StartWarning(game, TERM2046_WARN_SUGESTAO, TERM2046_PENDING_FILE, file, 0);
        return;
    }
    T2046ShowFileNow(game, file);
}

static void T2046RequestCompare(GameState *game)
{
    if (!game->term2046InterruptedCompare)
    {
        game->term2046InterruptedCompare = true;
        T2046StartWarning(game, TERM2046_WARN_COMPARACAO, TERM2046_PENDING_COMPARE,
                          TERM2046_FILE_PROTOCOLO, TERM2046_FILE_SISTEMA);
        return;
    }
    T2046ShowCompareNow(game);
}

static void T2046InterpretWarning(GameState *game, int choice)
{
    if (choice == 1)
    {
        int cost = (game->term2046WarningType == TERM2046_WARN_SUGESTAO)
                   ? T2046_COST_ACCEPT_SUGGESTION : T2046_COST_FOLLOW_WARNING;
        T2046AddInfluence(game, cost);
        if (game->term2046State == TERM2046_STATE_FIM) return;

        if (game->term2046WarningType == TERM2046_WARN_SUGESTAO)
        {
            T2046Log(game, T2046_KIND_RED, "A.R.1.3.L: \"Abri minha versão revisada para você.\"");
            T2046ShowFileNow(game, T2046SuggestedFile(game));
        }
        else
        {
            T2046Log(game, T2046_KIND_RED, "A.R.1.3.L: \"Boa escolha. Não precisa checar isso.\"");
            game->term2046State = TERM2046_STATE_TERMINAL;
        }
    }
    else if (game->term2046PendingKind == TERM2046_PENDING_COMPARE)
    {
        T2046ShowCompareNow(game);
    }
    else
    {
        T2046ShowFileNow(game, game->term2046PendingA);
    }
}

static void T2046InterpretCorrection(GameState *game, int choice)
{
    if (choice == 1)
    {
        T2046Log(game, T2046_KIND_RED, "A.R.1.3.L: \"Corrigindo automaticamente...\"");
        for (int i = 0; i < 3; i++) game->term2046Config[i] = 1;
        T2046StartEnding(game, TERM2046_ENDING_RUIM);
    }
    else
    {
        T2046Log(game, T2046_KIND_RED, "A.R.1.3.L: \"...Entendido. Sua escolha foi mantida.\"");
        game->term2046State = TERM2046_STATE_EU_DECIDO;
        game->terminal2046Input[0] = '\0';
        game->terminal2046InputLength = 0;
        while (GetCharPressed() > 0) { }
    }
}

/* Maiusculas, sem espacos nas pontas e sem espacos repetidos */
static void T2046NormalizePhrase(const char *in, char *out, int cap)
{
    int n = 0;
    bool pendingSpace = false;

    for (const char *p = in; *p != '\0' && n < cap - 1; p++)
    {
        char c = *p;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c == ' ')
        {
            if (n > 0) pendingSpace = true;
            continue;
        }
        if (pendingSpace)
        {
            out[n++] = ' ';
            pendingSpace = false;
            if (n >= cap - 1) break;
        }
        out[n++] = c;
    }
    out[n] = '\0';
}

static void T2046SubmitPhrase(GameState *game)
{
    char normalized[32];

    T2046NormalizePhrase(game->terminal2046Input, normalized, (int)sizeof(normalized));
    game->terminal2046Input[0] = '\0';
    game->terminal2046InputLength = 0;

    if (strcmp(normalized, T2046_FINAL_PHRASE) != 0)
    {
        T2046Log(game, T2046_KIND_GOLD, "Digite exatamente a frase: EU DECIDO");
        SetMessage(game, "Digite exatamente: EU DECIDO");
        return;
    }

    bool good = (game->term2046Config[0] == 2 && game->term2046Config[1] == 2 &&
                 game->term2046Config[2] == 2);
    T2046StartEnding(game, good ? TERM2046_ENDING_BOM : TERM2046_ENDING_INCOMPLETO);
}

static void T2046UpdatePhraseInput(GameState *game)
{
    if (Clicked(T2046PhraseBackRect()))
    {
        game->term2046State = TERM2046_STATE_CONFIGURACAO;
        return;
    }

    int character = GetCharPressed();
    while (character > 0)
    {
        if (character >= 32 && character <= 126 && game->terminal2046InputLength < T2046_INPUT_MAX)
        {
            if (character >= 'a' && character <= 'z') character = character - 'a' + 'A';
            game->terminal2046Input[game->terminal2046InputLength++] = (char)character;
            game->terminal2046Input[game->terminal2046InputLength] = '\0';
        }
        character = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && game->terminal2046InputLength > 0)
    {
        game->terminal2046Input[--game->terminal2046InputLength] = '\0';
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || Clicked(T2046ConfirmRect()))
    {
        T2046SubmitPhrase(game);
    }
}

/* ------------------------------------------------------------------------- */
/* Funcoes publicas: init, update e consulta                                 */
/* ------------------------------------------------------------------------- */

void InitTerminal2046(GameState *game)
{
    game->term2046State = TERM2046_STATE_TERMINAL;
    game->term2046Influence = 0;
    game->term2046ReadProtocol = false;
    game->term2046ReadDocuments = false;
    game->term2046ReadChanges = false;
    game->term2046ReadFakeReport = false;
    game->term2046ProtocolTampered = false;
    game->term2046FakeNote = false;
    game->term2046VerifyUnlocked = false;
    game->term2046FoundSources = false;
    game->term2046FoundUncertainty = false;
    game->term2046FoundDecision = false;
    game->term2046Principles = 0;
    game->term2046InterruptedCompare = false;
    game->term2046DiscoveredFakeReport = false;
    game->term2046ActionsSinceSabotage = 0;
    game->term2046NextSabotage = 0;
    game->term2046ListUse = TERM2046_LIST_OPEN;
    game->term2046FirstFile = 0;
    game->term2046WarningType = TERM2046_WARN_CORROMPIDO;
    game->term2046PendingKind = TERM2046_PENDING_FILE;
    game->term2046PendingA = 0;
    game->term2046PendingB = 0;
    game->term2046ViewKind = TERM2046_VIEW_INTRO;
    game->term2046ViewA = 0;
    game->term2046ViewB = 0;
    game->term2046ViewFlag = false;
    game->term2046Scroll = 0;
    for (int i = 0; i < 3; i++) game->term2046Config[i] = 0;
    game->term2046ConfigStep = 0;
    game->terminal2046Input[0] = '\0';
    game->terminal2046InputLength = 0;
    game->term2046Ending = TERM2046_ENDING_NONE;
    game->term2046BarTimer = 0.0f;
    game->term2046BarDone = false;
    game->term2046LogCount = 0;
    for (int i = 0; i < T2046_LOG_MAX; i++)
    {
        game->term2046Log[i][0] = '\0';
        game->term2046LogKind[i] = T2046_KIND_TEXT;
    }

    T2046LogPush(game, T2046_KIND_RED,
                 "A.R.1.3.L: \"Olá, Elira. Não há nada de errado comigo. Mas fique à vontade para olhar.\"");

    /* Descarta teclas digitadas antes de entrar na fase */
    while (GetCharPressed() > 0) { }
}

bool Terminal2046WantsTextInput(const GameState *game)
{
    return game->term2046State == TERM2046_STATE_EU_DECIDO;
}

/* Telas em que as letras do teclado viram texto digitado (nome do operador, terminal de 2048).
   Nelas, atalhos de letra como o M (musica) nao podem disparar. */
bool ScreenWantsTextInput(const GameState *game)
{
    if (game->screen == SCREEN_PLAYER_NAME) return true;
    if (game->screen == SCREEN_TERMINAL_2046) return Terminal2046WantsTextInput(game);
    return false;
}

/* Colecionavel oculto H: clicar no cracha "passa no leitor" e abre o arquivo oculto.
   So funciona nos subestados de investigacao (sem avisos da Ariel, configuracao ou
   frase final abertos). Nao e uma rodada: nao conta para o ciclo de sabotagem nem
   altera influencia, anotacoes, vidas ou tempo. Retorna true se consumiu o clique. */
static bool T2046TryCollectBadge(GameState *game)
{
    bool investigating = (game->term2046State == TERM2046_STATE_TERMINAL ||
                          game->term2046State == TERM2046_STATE_ESCOLHE_ARQUIVO ||
                          game->term2046State == TERM2046_STATE_ANOTAR ||
                          game->term2046State == TERM2046_STATE_NOTAS);

    if (game->hiddenFound[HIDDEN_2048] || !investigating || !Clicked(T2046BadgeRect())) return false;

    game->term2046State = TERM2046_STATE_TERMINAL;
    game->term2046ViewKind = TERM2046_VIEW_CRACHA;
    game->term2046ViewA = 0;
    game->term2046ViewB = 0;
    game->term2046ViewFlag = false;
    game->term2046Scroll = 0;

    T2046Log(game, T2046_KIND_GOLD, "[leitor de crachá] M. Okafor - arquivo oculto montado: acesso_okafor.key");
    T2046Log(game, T2046_KIND_RED, "A.R.1.3.L: \"Esse registro é antigo. Eu não preciso mais ser conferida.\"");
    CollectHidden(game, HIDDEN_2048);
    return true;
}

void UpdateTerminal2046(GameState *game)
{
    if (UpdateJournal(game)) return;

    /* Subestado FIM: animacao da barra de reconstrucao */
    if (game->term2046State == TERM2046_STATE_FIM)
    {
        if (!game->term2046BarDone)
        {
            game->term2046BarTimer += GetFrameTime();
            if (game->term2046BarTimer >= T2046_BAR_SECONDS) game->term2046BarDone = true;
        }
        else if (Clicked(T2046ContinueRect()) || IsKeyPressed(KEY_ENTER) ||
                 IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            game->screen = SCREEN_RESULT_2046;
        }
        return;
    }

    /* Subestado EU_DECIDO: entrada de texto */
    if (game->term2046State == TERM2046_STATE_EU_DECIDO)
    {
        T2046UpdatePhraseInput(game);
        return;
    }

    /* Nos demais subestados os caracteres digitados nao sao usados */
    while (GetCharPressed() > 0) { }

    if (T2046TryCollectBadge(game)) return;

    /* Subestados de aviso/tentacao da A.R.1.3.L. */
    if (game->term2046State == TERM2046_STATE_AVISO_ARIEL)
    {
        if (Clicked(T2046ChoiceCardRect(0)) || IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1))
            T2046InterpretWarning(game, 1);
        else if (Clicked(T2046ChoiceCardRect(1)) || IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2))
            T2046InterpretWarning(game, 2);
        return;
    }

    if (game->term2046State == TERM2046_STATE_CORRECAO_ARIEL)
    {
        if (Clicked(T2046ChoiceCardRect(0)) || IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1))
            T2046InterpretCorrection(game, 1);
        else if (Clicked(T2046ChoiceCardRect(1)) || IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2))
            T2046InterpretCorrection(game, 2);
        return;
    }

    /* Clique na coluna esquerda (funciona tanto no terminal quanto na configuracao!) */
    for (int i = 0; i < 5; i++)
    {
        if (Clicked(T2046FileBtnRect(i)) ||
            (game->term2046State == TERM2046_STATE_TERMINAL &&
             (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i))))
        {
            T2046RequestOpenFile(game, i + 1);
            return;
        }
    }
    if (Clicked(T2046CompareBtnRect()))
    {
        T2046RequestCompare(game);
        return;
    }
    if (Clicked(T2046FixBtnRect()))
    {
        game->term2046State = TERM2046_STATE_CONFIGURACAO;
        return;
    }

    /* Tela unica de correcao das 3 regras */
    if (game->term2046State == TERM2046_STATE_CONFIGURACAO)
    {
        for (int r = 0; r < 3; r++)
        {
            if (Clicked(T2046ConfigOptRect(r, 0)))
            {
                game->term2046Config[r] = 1;
                game->term2046Principles = T2046ConfigSelectedCount(game);
                return;
            }
            if (Clicked(T2046ConfigOptRect(r, 1)))
            {
                game->term2046Config[r] = 2;
                game->term2046Principles = T2046ConfigSelectedCount(game);
                return;
            }
        }
        if (Clicked(T2046ConfigBackBtn()))
        {
            game->term2046State = TERM2046_STATE_TERMINAL;
            return;
        }
        if (Clicked(T2046ConfigCompareBtn()))
        {
            T2046RequestCompare(game);
            return;
        }
        if (Clicked(T2046ConfigApplyBtn()) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            if (T2046ConfigSelectedCount(game) < 3)
            {
                SetMessage(game, "Selecione uma opção para cada uma das 3 regras antes de aplicar.");
                return;
            }
            game->term2046State = TERM2046_STATE_CORRECAO_ARIEL;
            return;
        }
        return;
    }

    /* Subestado TERMINAL: atalhos do painel principal + rolagem de texto */
    if (game->term2046ViewKind == TERM2046_VIEW_INTRO)
    {
        if (Clicked(T2046IntroOpenBtn()))
        {
            T2046RequestOpenFile(game, TERM2046_FILE_PROTOCOLO);
            return;
        }
        if (Clicked(T2046IntroCompareBtn()))
        {
            T2046RequestCompare(game);
            return;
        }
    }
    else if (game->term2046ViewKind == TERM2046_VIEW_COMPARE)
    {
        if (Clicked(T2046CompareFixBtn()))
        {
            game->term2046State = TERM2046_STATE_CONFIGURACAO;
            return;
        }
    }
    else if (game->term2046ViewKind == TERM2046_VIEW_FILE)
    {
        if (Clicked(T2046FileCompareShortcut()))
        {
            T2046RequestCompare(game);
            return;
        }
    }

    if (game->term2046ViewKind != TERM2046_VIEW_COMPARE)
    {
        Term2046View view;
        int maxScroll;
        float wheel = GetMouseWheelMove();

        T2046BuildCurrentView(game, &view);
        maxScroll = view.count - T2046_VIEW_LINES;
        if (maxScroll < 0) maxScroll = 0;

        if (wheel != 0.0f && CheckCollisionPointRec(GetVirtualMouse(), T2046MainPanel()))
            game->term2046Scroll -= (wheel > 0.0f) ? 3 : -3;
        if (IsKeyPressed(KEY_DOWN)) game->term2046Scroll += 1;
        if (IsKeyPressed(KEY_UP)) game->term2046Scroll -= 1;
        if (IsKeyPressed(KEY_PAGE_DOWN)) game->term2046Scroll += T2046_VIEW_LINES - 1;
        if (IsKeyPressed(KEY_PAGE_UP)) game->term2046Scroll -= T2046_VIEW_LINES - 1;
        if (game->term2046Scroll > maxScroll) game->term2046Scroll = maxScroll;
        if (game->term2046Scroll < 0) game->term2046Scroll = 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Desenho (nao altera o estado)                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Helpers de material                                                       */
/* ------------------------------------------------------------------------- */

static Color T2046FxShade(Color c, float k)
{
    return (Color){(unsigned char)fminf(255.0f, c.r * k), (unsigned char)fminf(255.0f, c.g * k),
                   (unsigned char)fminf(255.0f, c.b * k), c.a};
}

/* Placa metalica com volume: luz de cima/esquerda, sombra embaixo/direita, contorno escuro */
static void T2046FxPlate(Rectangle r, Color base, int bevel)
{
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                           T2046FxShade(base, 1.12f), T2046FxShade(base, 0.86f));
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, bevel, T2046FxShade(base, 1.55f));
    DrawRectangle((int)r.x, (int)r.y, bevel, (int)r.height, T2046FxShade(base, 1.28f));
    DrawRectangle((int)r.x, (int)(r.y + r.height) - bevel, (int)r.width, bevel, T2046FxShade(base, 0.55f));
    DrawRectangle((int)(r.x + r.width) - bevel, (int)r.y, bevel, (int)r.height, T2046FxShade(base, 0.66f));
    DrawRectangleLinesEx(r, 1.0f, (Color){6, 9, 15, 255});
}

/* Moldura de tela rebaixada: o vidro fica "para dentro" (sombra em cima, luz embaixo) */
static void T2046FxRecess(Rectangle r, int depth)
{
    Rectangle o = {r.x - depth, r.y - depth, r.width + 2 * depth, r.height + 2 * depth};
    DrawRectangleRec(o, (Color){9, 13, 21, 255});
    DrawRectangle((int)o.x, (int)o.y, (int)o.width, 2, (Color){4, 6, 11, 255});
    DrawRectangle((int)o.x, (int)o.y, 2, (int)o.height, (Color){5, 8, 13, 255});
    DrawRectangle((int)o.x, (int)(o.y + o.height) - 1, (int)o.width, 1, (Color){78, 92, 116, 255});
    DrawRectangle((int)(o.x + o.width) - 1, (int)o.y, 1, (int)o.height, (Color){58, 70, 92, 255});
}

static void T2046FxScrew(float x, float y)
{
    DrawCircle((int)x, (int)y, 2.8f, (Color){10, 13, 20, 255});
    DrawCircle((int)x, (int)y, 2.0f, (Color){112, 126, 146, 255});
    DrawPixel((int)x - 1, (int)y - 1, (Color){190, 200, 214, 255});
    DrawPixel((int)x + 1, (int)y + 1, (Color){44, 52, 66, 255});
}

/* Grade de ventilacao: fendas com sombra interna */
static void T2046FxVent(int x, int y, int count, int w)
{
    for (int i = 0; i < count; i++)
    {
        DrawRectangle(x, y + i * 5, w, 2, (Color){6, 9, 14, 255});
        DrawRectangle(x, y + i * 5 + 2, w, 1, (Color){70, 82, 104, 255});
    }
}

/* ------------------------------------------------------------------------- */
/* Cenario: sala de servidores em profundidade                               */
/* ------------------------------------------------------------------------- */

#define T2046FX_HORIZON 556
#define T2046FX_VPX     640.0f
#define T2046FX_VPY     300.0f

/* Gabinete de servidor visto de frente, com face lateral de perspectiva */
static void T2046FxRack(int i, int x, int top, int w, int h, float threat, float time, float dim)
{
    Color body = T2046FxShade((Color){24, 32, 48, 255}, dim);
    Color side = T2046FxShade((Color){14, 19, 30, 255}, dim);
    float cx = x + w * 0.5f;
    int sideW = (int)fminf(10.0f, fabsf(cx - T2046FX_VPX) * 0.03f);

    /* face lateral voltada para o centro da sala */
    if (cx < T2046FX_VPX)
        DrawRectangle(x + w, top + 3, sideW, h - 3, side);
    else
        DrawRectangle(x - sideW, top + 3, sideW, h - 3, side);

    T2046FxPlate((Rectangle){(float)x, (float)top, (float)w, (float)h}, body, 2);

    /* gavetas de servidor 1U/2U com LEDs */
    for (int row = 0; row < 13; row++)
    {
        int y = top + 12 + row * 34;
        float hh = fmodf(i * 0.37f + row * 0.113f, 1.0f);
        Color led = (hh < threat) ? RED_COLOR : CYAN_COLOR;
        float blink = sinf(time * (1.5f + (float)((i + row) % 5) * 0.7f) + i * 1.3f + row);
        Color unit = T2046FxShade((Color){32, 43, 62, 255}, dim);

        if (y + 26 > top + h - 8) break;
        DrawRectangle(x + 6, y, w - 12, 26, unit);
        DrawRectangle(x + 6, y, w - 12, 1, T2046FxShade(unit, 1.5f));
        DrawRectangle(x + 6, y + 25, w - 12, 1, T2046FxShade(unit, 0.5f));
        /* furos de ventilacao */
        for (int k = 0; k < 6; k++)
            DrawRectangle(x + 40 + k * 6, y + 9, 3, 8, T2046FxShade((Color){12, 17, 27, 255}, dim));
        /* LEDs com halo */
        float on1 = blink > 0.2f ? 1.0f : 0.22f;
        float on2 = blink < -0.2f ? 1.0f : 0.22f;
        DrawCircle(x + 15, y + 13, 5.0f, Fade(led, 0.12f * on1));
        DrawCircle(x + 15, y + 13, 2.2f, Fade(led, 0.85f * on1));
        DrawCircle(x + 27, y + 13, 5.0f, Fade(led, 0.12f * on2));
        DrawCircle(x + 27, y + 13, 2.2f, Fade(led, 0.85f * on2));
        /* puxador */
        DrawRectangle(x + w - 14, y + 7, 3, 12, T2046FxShade((Color){90, 104, 126, 255}, dim));
    }

    /* pes do gabinete */
    DrawRectangle(x + 4, top + h, 8, 4, (Color){8, 10, 16, 255});
    DrawRectangle(x + w - 12, top + h, 8, 4, (Color){8, 10, 16, 255});
}

static void T2046FxBackdrop(const GameState *game, float time)
{
    float threat = (float)game->term2046Influence / (float)T2046_INFLUENCE_MAX;

    ClearBackground((Color){8, 11, 19, 255});

    /* parede do fundo e teto escuro */
    DrawRectangleGradientV(0, 70, SCREEN_WIDTH, T2046FX_HORIZON - 70, (Color){10, 15, 27, 255}, (Color){16, 23, 38, 255});

    /* calha de cabos no teto com feixes descendo para os racks */
    DrawRectangle(0, 72, SCREEN_WIDTH, 10, (Color){20, 27, 40, 255});
    DrawRectangle(0, 72, SCREEN_WIDTH, 1, (Color){52, 64, 86, 255});
    for (int i = 0; i < 13; i++)
    {
        int x = 6 + i * 100;
        DrawLineBezier((Vector2){(float)x + 20, 82}, (Vector2){(float)x + 70, 92}, 3.0f, (Color){12, 16, 24, 255});
        DrawLineBezier((Vector2){(float)x + 30, 82}, (Vector2){(float)x + 82, 90}, 2.0f, (Color){40, 30, 58, 255});
    }

    /* fileira de racks contra a parede */
    for (int i = 0; i < 13; i++)
    {
        int x = 6 + i * 100;
        T2046FxRack(i, x, 90, 90, T2046FX_HORIZON - 94, threat, time, 1.0f);
    }

    /* piso tecnico refletindo a luz ciano */
    DrawRectangleGradientV(0, T2046FX_HORIZON, SCREEN_WIDTH, SCREEN_HEIGHT - T2046FX_HORIZON,
                           (Color){18, 26, 40, 255}, (Color){7, 10, 17, 255});
    /* reflexo dos racks no piso (borrado e fraco) */
    for (int i = 0; i < 13; i++)
    {
        int x = 6 + i * 100;
        for (int k = 0; k < 6; k++)
        {
            float a = 0.10f * (1.0f - k / 6.0f);
            DrawRectangle(x + 4, T2046FX_HORIZON + k * 6, 82, 6, Fade((Color){40, 60, 90, 255}, a));
        }
        Color led = (fmodf(i * 0.37f, 1.0f) < threat) ? RED_COLOR : CYAN_COLOR;
        DrawRectangle(x + 12, T2046FX_HORIZON + 4, 3, 22, Fade(led, 0.10f));
        DrawRectangle(x + 24, T2046FX_HORIZON + 4, 3, 16, Fade(led, 0.07f));
    }
    /* juntas das placas do piso em perspectiva */
    for (int k = -14; k <= 14; k++)
    {
        float bx = T2046FX_VPX + k * 110.0f;
        float t0 = (T2046FX_HORIZON - T2046FX_VPY) / (SCREEN_HEIGHT - T2046FX_VPY);
        Vector2 a = {T2046FX_VPX + (bx - T2046FX_VPX) * t0, (float)T2046FX_HORIZON};
        DrawLineV(a, (Vector2){bx, (float)SCREEN_HEIGHT}, Fade((Color){60, 90, 120, 255}, 0.22f));
    }
    for (int k = 1; k < 6; k++)
    {
        int y = T2046FX_HORIZON + (int)(k * k * 4.2f);
        DrawLine(0, y, SCREEN_WIDTH, y, Fade((Color){60, 90, 120, 255}, 0.18f));
    }
    /* placa perfurada do piso elevado, com ar frio iluminado vindo de baixo */
    {
        Vector2 a = {70.0f, 614.0f}, b = {196.0f, 614.0f}, c = {204.0f, 648.0f}, d = {58.0f, 648.0f};
        DrawTriangle(a, d, c, (Color){14, 22, 34, 255});
        DrawTriangle(a, c, b, (Color){14, 22, 34, 255});
        for (int row = 0; row < 5; row++)
        {
            float y = 618.0f + row * 6.5f;
            float t = (y - 614.0f) / 34.0f;
            float x0 = 70.0f - 12.0f * t + 6.0f, x1 = 196.0f + 8.0f * t - 6.0f;
            for (float x = x0; x < x1; x += 9.0f + t * 2.0f)
                DrawRectangle((int)x, (int)y, 3 + (int)(t * 2), 2, Fade(CYAN_COLOR, 0.16f + 0.10f * sinf(time * 1.3f + x * 0.05f)));
        }
        DrawLineV(a, b, Fade((Color){90, 120, 150, 255}, 0.35f));
        DrawLineV(a, d, Fade((Color){90, 120, 150, 255}, 0.25f));
        DrawLineV(b, c, Fade((Color){90, 120, 150, 255}, 0.25f));
    }
    /* luz ciano das telas refletida no piso, junto ao console */
    DrawRectangleGradientV(344, 632, 912, 18, Fade(CYAN_COLOR, 0.10f), Fade(CYAN_COLOR, 0.0f));
    /* sombra de contato do console e da torre no piso */
    DrawRectangleGradientV(340, 632, 920, 6, Fade(BLACK, 0.6f), Fade(BLACK, 0.0f));
    DrawRectangleGradientV(16, 580, 312, 8, Fade(BLACK, 0.6f), Fade(BLACK, 0.0f));
    /* reflexo da torre no piso polido */
    DrawRectangleGradientV(14, 588, 316, 20, Fade((Color){60, 80, 110, 255}, 0.10f), Fade((Color){60, 80, 110, 255}, 0.0f));
}

/* ------------------------------------------------------------------------- */
/* Corpo fisico: torre de controle (esquerda) e console de duas telas         */
/* ------------------------------------------------------------------------- */

static void T2046FxTower(float threat)
{
    Rectangle tower = {12.0f, 74.0f, 320.0f, 506.0f};
    Color metal = (Color){40, 49, 66, 255};

    T2046FxPlate(tower, metal, 2);
    /* nervuras laterais do chassi */
    DrawRectangle(16, 196, 4, 376, T2046FxShade(metal, 0.7f));
    DrawRectangle(326, 196, 3, 376, T2046FxShade(metal, 0.7f));
    /* nicho do medidor */
    T2046FxRecess((Rectangle){24.0f, 80.0f, 300.0f, 108.0f}, 2);
    /* trilho dos botoes */
    T2046FxRecess((Rectangle){20.0f, 194.0f, 308.0f, 382.0f}, 1);
    DrawRectangleGradientV(20, 194, 308, 382, (Color){16, 22, 35, 255}, (Color){11, 16, 26, 255});
    /* parafusos nos cantos */
    T2046FxScrew(16, 78);
    T2046FxScrew(328, 78);
    T2046FxScrew(16, 191);
    T2046FxScrew(328, 191);
    T2046FxScrew(328, 576);
    /* lampada de status no pe da torre */
    DrawRectangle(28, 575, 24, 2, Fade(T2046MixColor(CYAN_COLOR, RED_COLOR, threat), 0.8f));
}

static void T2046FxConsole(float threat, float time)
{
    Rectangle body = {336.0f, 73.0f, 928.0f, 559.0f};
    Color metal = (Color){42, 51, 68, 255};
    Color ledColor = T2046MixColor(CYAN_COLOR, RED_COLOR, threat);

    T2046FxPlate(body, metal, 2);
    /* fresta escura entre a torre e o console */
    DrawRectangle(332, 74, 4, 506, (Color){5, 7, 12, 255});
    T2046FxRecess(T2046MainPanel(), 2);
    T2046FxRecess(T2046LogPanel(), 2);
    /* faixa divisoria entre as telas: ventilacao e LEDs de atividade */
    T2046FxVent(1150, 540, 1, 90);
    for (int k = 0; k < 4; k++)
    {
        float on = sinf(time * (2.0f + k * 0.9f) + k) > 0.0f ? 1.0f : 0.25f;
        DrawCircle(1110 - k * 12, 542, 2.0f, Fade(k == 0 ? ledColor : T2046_GREEN, 0.8f * on));
    }
    /* parafusos */
    T2046FxScrew(339, 76);
    T2046FxScrew(1260, 76);
    T2046FxScrew(339, 628);
    T2046FxScrew(1260, 628);
    T2046FxScrew(339, 542);
    T2046FxScrew(1260, 542);
}

/* Reflexo de vidro so nas bordas das telas, fora das areas de texto */
static void T2046FxGlass(void)
{
    Rectangle m = T2046MainPanel();
    Rectangle l = T2046LogPanel();

    /* filete de luz no topo de cada vidro */
    DrawRectangle((int)m.x + 10, (int)m.y + 2, (int)m.width - 20, 1, Fade(RAYWHITE, 0.10f));
    DrawRectangle((int)l.x + 10, (int)l.y + 2, (int)l.width - 20, 1, Fade(RAYWHITE, 0.08f));
    /* faixa diagonal no canto superior direito (vazio em todos os subestados) */
    DrawTriangle((Vector2){m.x + m.width - 70, m.y + 3}, (Vector2){m.x + m.width - 104, m.y + 34},
                 (Vector2){m.x + m.width - 86, m.y + 34}, Fade(RAYWHITE, 0.05f));
    DrawTriangle((Vector2){m.x + m.width - 70, m.y + 3}, (Vector2){m.x + m.width - 86, m.y + 34},
                 (Vector2){m.x + m.width - 52, m.y + 3}, Fade(RAYWHITE, 0.05f));
    DrawTriangle((Vector2){m.x + m.width - 40, m.y + 3}, (Vector2){m.x + m.width - 58, m.y + 20},
                 (Vector2){m.x + m.width - 48, m.y + 20}, Fade(RAYWHITE, 0.04f));
    DrawTriangle((Vector2){m.x + m.width - 40, m.y + 3}, (Vector2){m.x + m.width - 48, m.y + 20},
                 (Vector2){m.x + m.width - 30, m.y + 3}, Fade(RAYWHITE, 0.04f));
    /* brilho na margem esquerda do vidro (a esquerda do texto, que comeca em x = 364) */
    DrawRectangleGradientH((int)m.x + 3, (int)m.y + 40, 8, (int)m.height - 80, Fade(RAYWHITE, 0.05f), Fade(RAYWHITE, 0.0f));
}

static void T2046DrawBackdrop(const GameState *game, float time)
{
    float threat = (float)game->term2046Influence / (float)T2046_INFLUENCE_MAX;

    T2046FxBackdrop(game, time);
    T2046FxTower(threat);
    T2046FxConsole(threat, time);

    /* Cracha digital de M. Okafor (colecionavel H): pendurado no rack, a parte de baixo
       fica atras da barra do rodape. Some do cenario depois de lido no leitor. */
    if (!game->hiddenFound[HIDDEN_2048])
    {
        Rectangle area = T2046BadgeRect();
        Color strap = (Color){52, 66, 92, 255};

        DrawLineEx((Vector2){279.0f, 576.0f}, (Vector2){285.0f, 612.0f}, 2.0f, strap);
        DrawLineEx((Vector2){293.0f, 576.0f}, (Vector2){287.0f, 612.0f}, 2.0f, strap);
        DrawRectangle(283, 609, 6, 7, (Color){120, 130, 145, 255});
        DrawRectangleRounded((Rectangle){272.0f, 615.0f, 28.0f, 44.0f}, 0.15f, 4, (Color){30, 37, 50, 255});
        DrawRectangleRoundedLinesEx((Rectangle){272.0f, 615.0f, 28.0f, 44.0f}, 0.15f, 4, 1.0f, (Color){70, 86, 110, 255});
        DrawRectangle(273, 616, 26, 4, Fade(CYAN_COLOR, 0.45f));                  /* faixa de acesso */
        DrawRectangle(276, 624, 9, 11, (Color){92, 104, 120, 255});               /* foto */
        DrawCircle(280, 627, 2.2f, (Color){130, 140, 155, 255});
        DrawRectangle(288, 625, 9, 2, Fade(CYAN_COLOR, 0.35f));                   /* tela e-ink */
        DrawRectangle(288, 630, 7, 2, Fade(CYAN_COLOR, 0.25f));
        DrawRectangle(276, 640, 6, 5, (Color){150, 128, 70, 255});                /* chip */

        if (game->term2046State != TERM2046_STATE_FIM &&
            CheckCollisionPointRec(GetVirtualMouse(), area))
        {
            DrawRectangleRoundedLinesEx((Rectangle){270.0f, 613.0f, 32.0f, 48.0f}, 0.15f, 4, 1.0f, Fade(CYAN_COLOR, 0.6f));
        }
        DrawHiddenGlint(area, 2.3f);
    }
}

static void T2046DrawHud(const GameState *game)
{
    int totalSeconds = (int)(GetTime() - game->startTime);

    DrawRectangle(0, 0, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.94f));
    DrawLine(0, 70, SCREEN_WIDTH, 70, Fade(CYAN_COLOR, 0.25f));
    DrawHudYearOperator("2048", game->playerName, 24);
    DrawHudLivesTimer(game->lives, totalSeconds);
}

static void T2046DrawInfluencePanel(const GameState *game, float time)
{
    Rectangle box = {24.0f, 80.0f, 300.0f, 108.0f};
    int influence = game->term2046Influence;
    float threat = (float)influence / (float)T2046_INFLUENCE_MAX;
    Color meter = T2046MeterColor(influence);
    Rectangle bar = {40.0f, 138.0f, 186.0f, 12.0f};
    Vector2 port = {280.0f, 132.0f};

    /* placa do modulo */
    DrawRectangleGradientV((int)box.x, (int)box.y, (int)box.width, (int)box.height,
                           (Color){22, 32, 52, 255}, (Color){15, 22, 37, 255});
    DrawRectangle((int)box.x, (int)box.y, (int)box.width, 1, Fade(RAYWHITE, 0.10f));
    DrawRectangleLinesEx(box, 1.0f, Fade(meter, 0.55f));

    /* visor redondo do nucleo da Ariel */
    DrawCircleV((Vector2){port.x + 1, port.y + 2}, 34.0f, Fade(BLACK, 0.45f));
    DrawCircleV(port, 33.0f, (Color){70, 82, 104, 255});
    DrawRing(port, 30.0f, 33.0f, 200.0f, 340.0f, 24, (Color){132, 146, 170, 255});
    DrawRing(port, 30.0f, 33.0f, 20.0f, 160.0f, 24, (Color){34, 42, 56, 255});
    DrawCircleV(port, 30.0f, (Color){6, 10, 19, 255});
    DrawArielCore(port, 0.18f, T2046MixColor(CYAN_COLOR, RED_COLOR, threat),
                  0.25f + 0.75f * threat, sinf(time * 0.8f));
    DrawRing(port, 28.0f, 30.0f, 0.0f, 360.0f, 32, (Color){10, 14, 22, 255});

    DrawGameText("INFLUÊNCIA DA IA", 40, 89, 11, T2046_STEEL);
    DrawGameText(TextFormat("A.R.1.3.L %d%%", influence), 40, 105, 20, meter);

    /* barra de influencia */
    DrawRectangle((int)bar.x - 2, (int)bar.y - 2, (int)bar.width + 4, (int)bar.height + 4, (Color){6, 9, 15, 255});
    DrawRectangle((int)bar.x, (int)bar.y, (int)bar.width, (int)bar.height, (Color){10, 15, 26, 255});
    if (threat > 0.0f)
    {
        int fill = (int)(bar.width * threat);
        DrawRectangle((int)bar.x, (int)bar.y, fill, (int)bar.height, meter);
    }
    DrawRectangleLines((int)bar.x, (int)bar.y, (int)bar.width, (int)bar.height, Fade(meter, 0.8f));

    DrawGameText(TextFormat("Arquivos lidos: %d/5", T2046FilesReadCount(game)), 40, 159, 14, RAYWHITE);
}

static void T2046DrawSidebarCard(Rectangle bounds, const char *left, const char *right,
                                 Color accent, Color rightCol, bool enabled, bool active)
{
    bool hover = enabled && CheckCollisionPointRec(GetVirtualMouse(), bounds);
    Color bg = active ? Fade(accent, 0.22f)
             : (hover ? Fade(accent, 0.16f) : (Color){16, 24, 38, 255});
    Color border = active ? accent
                 : (hover ? accent : (enabled ? (Color){48, 66, 92, 255} : Fade(PANEL_LIGHT, 0.45f)));

    DrawRectangleRounded(bounds, 0.16f, 6, bg);
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 6, (active || hover) ? 2.0f : 1.3f, border);

    int size = 16;
    int maxLeft = (int)bounds.width - ((right && right[0]) ? MeasureGameText(right, 12) + 34 : 24);
    while (size > 12 && MeasureGameText(left, size) > maxLeft) size--;

    DrawGameText(left, (int)bounds.x + 12, (int)(bounds.y + (bounds.height - size) / 2.0f),
                 size, enabled ? ((hover || active) ? WHITE : RAYWHITE) : T2046_DIM);
    if (right != NULL && right[0] != '\0')
    {
        int rw = MeasureGameText(right, 12);
        DrawGameText(right, (int)(bounds.x + bounds.width - rw - 12),
                     (int)(bounds.y + (bounds.height - 12) / 2.0f), 12,
                     enabled ? rightCol : T2046_DIM);
    }
}

static void T2046DrawActionColumn(const GameState *game)
{
    static const char *const shortTag[5] = {"1990", "2008", "2026", "LOG", "2048"};
    bool navOpen = (game->term2046State == TERM2046_STATE_TERMINAL ||
                    game->term2046State == TERM2046_STATE_CONFIGURACAO);

    DrawGameText("ARQUIVOS DO TERMINAL", 30, 200, 12, CYAN_COLOR);

    for (int i = 0; i < 5; i++)
    {
        int fileId = i + 1;
        bool isRead = T2046FileIsRead(game, fileId);
        bool isViewing = (game->term2046State == TERM2046_STATE_TERMINAL &&
                          game->term2046ViewKind == TERM2046_VIEW_FILE &&
                          game->term2046ViewA == fileId);
        const char *tag = isRead ? TextFormat("OK %s", shortTag[i]) : shortTag[i];
        Color tagCol = isRead ? T2046_GREEN : T2046_GRAY;
        T2046DrawSidebarCard(T2046FileBtnRect(i), T2046_FILE_NAMES[fileId], tag,
                             isRead ? T2046_GREEN : CYAN_COLOR, tagCol, navOpen, isViewing);
    }

    DrawLine(28, 471, 320, 471, Fade(CYAN_COLOR, 0.25f));

    bool viewingCmp = (game->term2046State == TERM2046_STATE_TERMINAL &&
                       game->term2046ViewKind == TERM2046_VIEW_COMPARE);
    bool compared = game->term2046FoundUncertainty;
    T2046DrawSidebarCard(T2046CompareBtnRect(),
                         "COMPARAR 1990 x 2026",
                         compared ? "OK" : "",
                         compared ? T2046_GREEN : CYAN_COLOR,
                         T2046_GREEN, navOpen, viewingCmp);

    bool inConfig = (game->term2046State == TERM2046_STATE_CONFIGURACAO);
    T2046DrawSidebarCard(T2046FixBtnRect(),
                         "CORRIGIR 3 REGRAS >>",
                         TextFormat("%d/3", T2046ConfigSelectedCount(game)),
                         compared ? T2046_GREEN : GOLD_COLOR,
                         compared ? T2046_GREEN : GOLD_COLOR,
                         navOpen, inConfig);
}

static void T2046DrawPanelTitleAt(const char *title, int y, Color color)
{
    DrawGameText(title, 364, y, 18, color);
    DrawRectangle(364, y + 25, 872, 1, Fade(color, 0.35f));
}

static void T2046DrawChoiceCard(Rectangle r, const char *label, Color accent)
{
    bool hover = CheckCollisionPointRec(GetVirtualMouse(), r);
    DrawRectangleRounded(r, 0.16f, 6, hover ? Fade(accent, 0.26f) : (Color){16, 24, 38, 255});
    DrawRectangleRoundedLinesEx(r, 0.16f, 6, hover ? 2.2f : 1.5f, hover ? accent : Fade(accent, 0.65f));
    int size = 18;
    while (size > 13 && MeasureGameText(label, size) > (int)r.width - 32) size--;
    DrawGameText(label, (int)r.x + 20, (int)(r.y + (r.height - size) / 2.0f),
                 size, hover ? WHITE : RAYWHITE);
}

static void T2046DrawViewLines(const Term2046View *v, int scroll, int x, int y, int visible, int lineHeight)
{
    for (int i = 0; i < visible; i++)
    {
        int index = scroll + i;
        if (index >= v->count) break;

        const Term2046Line *line = &v->lines[index];
        int lx = x + line->indent;
        int ly = y + i * lineHeight;

        if (line->tab > 0)
        {
            char leftText[96];
            const char *tabPos = strchr(line->text, '\t');
            int leftLen = (tabPos != NULL) ? (int)(tabPos - line->text) : (int)strlen(line->text);

            snprintf(leftText, sizeof(leftText), "%.*s", leftLen, line->text);
            if (leftText[0] != '\0') DrawGameText(leftText, lx, ly, v->size, line->color);
            if (tabPos != NULL) DrawGameText(tabPos + 1, lx + line->tab, ly, v->size, line->color);
        }
        else if (line->text[0] != '\0')
        {
            DrawGameText(line->text, lx, ly, v->size, line->color);
        }
    }
}

static void T2046DrawStepper(const GameState *game)
{
    bool ok1 = (T2046FilesReadCount(game) >= 2);
    bool ok2 = game->term2046FoundUncertainty;
    bool ok3 = (game->term2046State == TERM2046_STATE_EU_DECIDO ||
                game->term2046State == TERM2046_STATE_FIM);

    const char *labels[3] = {
        ok1 ? "1. ARQUIVOS LIDOS [OK]" : "1. LER ARQUIVOS",
        ok2 ? "2. COMPARADO [OK]" : "2. COMPARAR 1990 x 2026",
        ok3 ? "3. REGRAS OK" : "3. CORRIGIR 3 REGRAS"
    };
    bool done[3] = {ok1, ok2, ok3};
    bool current[3] = {
        !ok1,
        ok1 && !ok2,
        ok2 && !ok3
    };

    for (int i = 0; i < 3; i++)
    {
        Rectangle pill = {364.0f + i * 300.0f, 90.0f, 272.0f, 32.0f};
        Color col = done[i] ? T2046_GREEN : (current[i] ? GOLD_COLOR : T2046_GRAY);
        DrawRectangleRounded(pill, 0.25f, 4, done[i] ? Fade(T2046_GREEN, 0.14f)
                             : (current[i] ? Fade(GOLD_COLOR, 0.14f) : (Color){12, 19, 34, 255}));
        DrawRectangleRoundedLinesEx(pill, 0.25f, 4, current[i] ? 1.6f : 1.0f, Fade(col, 0.65f));
        int fs = 14;
        while (fs > 11 && MeasureGameText(labels[i], fs) > (int)pill.width - 16) fs--;
        int tx = (int)(pill.x + (pill.width - MeasureGameText(labels[i], fs)) / 2.0f);
        DrawGameText(labels[i], tx, (int)(pill.y + (pill.height - fs) / 2.0f), fs, col);

        if (i < 2)
        {
            DrawGameText(">", (int)(pill.x + pill.width + 10), (int)pill.y + 8, 15, T2046_DIM);
        }
    }
}

static void T2046DrawSideBySideCompare(void)
{
    static const char *const ruleHead[3] = {"REGRA 1 // FONTES", "REGRA 2 // INCERTEZA", "REGRA 3 // DECISÕES"};

    /* Coluna esquerda: Original 1990 */
    Rectangle leftCol = {364.0f, 174.0f, 424.0f, 282.0f};
    DrawRectangleRounded(leftCol, 0.04f, 6, (Color){14, 26, 36, 255});
    DrawRectangleRoundedLinesEx(leftCol, 0.04f, 6, 1.6f, Fade(T2046_GREEN, 0.65f));
    DrawGameText("DISQUETE 1990 // ORIGINAL", (int)leftCol.x + 16, (int)leftCol.y + 12, 15, T2046_GREEN);

    /* Coluna direita: Alterado 2026 */
    Rectangle rightCol = {812.0f, 174.0f, 424.0f, 282.0f};
    DrawRectangleRounded(rightCol, 0.04f, 6, (Color){30, 18, 26, 255});
    DrawRectangleRoundedLinesEx(rightCol, 0.04f, 6, 1.6f, Fade(RED_COLOR, 0.65f));
    DrawGameText("SSD 2026 // ALTERADO POR A.R.1.3.L.", (int)rightCol.x + 16, (int)rightCol.y + 12, 15, RED_COLOR);

    for (int i = 0; i < 3; i++)
    {
        float ry = 212.0f + i * 80.0f;

        Rectangle lc = {leftCol.x + 12.0f, ry, leftCol.width - 24.0f, 72.0f};
        DrawRectangleRounded(lc, 0.10f, 4, (Color){18, 34, 44, 255});
        DrawRectangleRoundedLinesEx(lc, 0.10f, 4, 1.0f, Fade(T2046_GREEN, 0.35f));
        DrawGameText(ruleHead[i], (int)lc.x + 12, (int)lc.y + 8, 12, T2046_GREEN);
        T2046DrawWrapped(T2046_RULES_1990[i], (int)lc.x + 12, (int)lc.y + 26, 14, (int)lc.width - 24, 18, RAYWHITE);

        Rectangle rc = {rightCol.x + 12.0f, ry, rightCol.width - 24.0f, 72.0f};
        DrawRectangleRounded(rc, 0.10f, 4, (Color){38, 22, 30, 255});
        DrawRectangleRoundedLinesEx(rc, 0.10f, 4, 1.0f, Fade(RED_COLOR, 0.45f));
        DrawGameText(ruleHead[i], (int)rc.x + 12, (int)rc.y + 8, 12, GOLD_COLOR);
        const char *tag = "SEM AUTORIZAÇÃO";
        DrawGameText(tag, (int)(rc.x + rc.width - MeasureGameText(tag, 11) - 12), (int)rc.y + 8, 11, RED_COLOR);
        T2046DrawWrapped(T2046_RULES_2046[i], (int)rc.x + 12, (int)rc.y + 26, 14, (int)rc.width - 24, 18, RAYWHITE);
    }

    DrawButton(T2046CompareFixBtn(), "CORRIGIR AS 3 REGRAS AGORA >>", GOLD_COLOR);
}

static void T2046DrawMainPanel(const GameState *game, float time)
{
    Rectangle panel = T2046MainPanel();
    DrawRectangleRounded(panel, 0.03f, 8, Fade(PANEL_COLOR, 0.95f));
    DrawRectangleRoundedLinesEx(panel, 0.03f, 8, 2.0f, Fade(CYAN_COLOR, 0.55f));

    if (game->term2046State == TERM2046_STATE_TERMINAL ||
        game->term2046State == TERM2046_STATE_CONFIGURACAO)
    {
        T2046DrawStepper(game);
    }

    switch (game->term2046State)
    {
        case TERM2046_STATE_TERMINAL:
        {
            T2046DrawPanelTitleAt(T2046ViewTitle(game), 134, CYAN_COLOR);

            if (game->term2046ViewKind == TERM2046_VIEW_COMPARE)
            {
                T2046DrawSideBySideCompare();
            }
            else
            {
                Term2046View view;
                int maxScroll;

                if (game->term2046ViewKind == TERM2046_VIEW_FILE)
                {
                    DrawButton(T2046FileCompareShortcut(), "COMPARAR 1990 x 2026 >>", GOLD_COLOR);
                }

                T2046BuildCurrentView(game, &view);
                int visibleLines = (game->term2046ViewKind == TERM2046_VIEW_INTRO) ? 13 : 15;
                T2046DrawViewLines(&view, game->term2046Scroll, 364, 174, visibleLines, T2046_LINE_H);

                if (game->term2046ViewKind == TERM2046_VIEW_INTRO)
                {
                    DrawButton(T2046IntroOpenBtn(), "ABRIR protocolo.txt (1990)", CYAN_COLOR);
                    DrawButton(T2046IntroCompareBtn(), "COMPARAR 1990 x 2026", GOLD_COLOR);
                }
                else
                {
                    maxScroll = view.count - visibleLines;
                    if (maxScroll > 0)
                    {
                        DrawGameText(TextFormat("linhas %d-%d de %d  (roda do mouse ou setas para rolar)",
                                     game->term2046Scroll + 1,
                                     (game->term2046Scroll + visibleLines < view.count) ? game->term2046Scroll + visibleLines : view.count,
                                     view.count),
                                     364, 502, 13, T2046_DIM);
                    }
                }
            }
            break;
        }

        case TERM2046_STATE_CONFIGURACAO:
        {
            Vector2 mouse = GetVirtualMouse();
            T2046DrawPanelTitleAt("CORRIGIR AS 3 REGRAS CORROMPIDAS", 134, GOLD_COLOR);
            DrawGameText("Selecione para cada regra o comportamento original de 1990:", 364, 168, 15, T2046_GRAY);

            for (int r = 0; r < 3; r++)
            {
                Rectangle row = {364.0f, 200.0f + r * 86.0f, 872.0f, 76.0f};
                DrawRectangleRounded(row, 0.08f, 6, (Color){16, 24, 38, 255});
                DrawRectangleRoundedLinesEx(row, 0.08f, 6, 1.3f,
                                            (game->term2046Config[r] > 0) ? Fade(CYAN_COLOR, 0.6f) : (Color){44, 58, 76, 255});
                DrawGameText(T2046_CONFIG_LABEL[r], (int)row.x + 18, (int)row.y + 29, 18, GOLD_COLOR);

                for (int opt = 0; opt < 2; opt++)
                {
                    Rectangle btn = T2046ConfigOptRect(r, opt);
                    bool selected = (game->term2046Config[r] == opt + 1);
                    bool hover = CheckCollisionPointRec(mouse, btn);
                    Color accent = selected ? T2046_GREEN : CYAN_COLOR;
                    DrawRectangleRounded(btn, 0.16f, 6,
                                         selected ? Fade(T2046_GREEN, 0.25f)
                                                  : (hover ? Fade(CYAN_COLOR, 0.18f) : (Color){22, 33, 50, 255}));
                    DrawRectangleRoundedLinesEx(btn, 0.16f, 6, selected ? 2.2f : 1.4f,
                                                (selected || hover) ? accent : (Color){56, 74, 98, 255});
                    const char *lbl = T2046_CONFIG_OPTION[r][opt];
                    int fs = 16;
                    while (fs > 12 && MeasureGameText(lbl, fs) > (int)btn.width - 20) fs--;
                    int tx = (int)(btn.x + (btn.width - MeasureGameText(lbl, fs)) / 2.0f);
                    DrawGameText(lbl, tx, (int)(btn.y + (btn.height - fs) / 2.0f),
                                 fs, selected ? WHITE : (hover ? RAYWHITE : LIGHTGRAY));
                }
            }

            DrawButton(T2046ConfigBackBtn(), "< VOLTAR AOS ARQUIVOS", PANEL_LIGHT);
            DrawButton(T2046ConfigCompareBtn(), "VER COMPARAÇÃO 1990 x 2026", CYAN_COLOR);
            bool ready = (T2046ConfigSelectedCount(game) == 3);
            DrawButton(T2046ConfigApplyBtn(), "APLICAR CORREÇÕES >>", ready ? GOLD_COLOR : GRAY);
            break;
        }

        case TERM2046_STATE_AVISO_ARIEL:
        {
            bool suggest = (game->term2046WarningType == TERM2046_WARN_SUGESTAO);
            const char *speech;
            const char *optionA;
            const char *optionB;

            if (game->term2046WarningType == TERM2046_WARN_CORROMPIDO)
            {
                speech = "A.R.1.3.L: \"Esse arquivo está corrompido. Recomendo ignorá-lo.\"";
                optionA = "[1] Ignorar arquivo (confiar na IA)";
                optionB = "[2] Abrir alteracoes.txt mesmo assim";
            }
            else if (game->term2046WarningType == TERM2046_WARN_COMPARACAO)
            {
                speech = "A.R.1.3.L: \"Comparar com 1990 é perda de tempo. Aquelas regras estão obsoletas.\"";
                optionA = "[1] Cancelar comparação (confiar na IA)";
                optionB = "[2] Comparar 1990 x 2026 mesmo assim";
            }
            else
            {
                speech = TextFormat("A.R.1.3.L: \"Esse relatório tem erros. Quer abrir minha versão (%s)?\"",
                                    T2046_FILE_NAMES[T2046SuggestedFile(game)]);
                optionA = "[1] Abrir versão sugerida por A.R.1.3.L.";
                optionB = "[2] Não — abrir relatorio.txt original";
            }

            T2046DrawPanelTitleAt("A.R.1.3.L INTERFERE", 96, RED_COLOR);
            T2046DrawWrapped(speech, 384, 160, 22, 832, 32, RED_COLOR);
            if (suggest)
            {
                DrawGameText("Atenção: aceitar sugestões da IA aumenta a influência dela.", 384, 248, 15, T2046_GRAY);
            }
            T2046DrawChoiceCard(T2046ChoiceCardRect(0), optionA, RED_COLOR);
            T2046DrawChoiceCard(T2046ChoiceCardRect(1), optionB, T2046_GREEN);
            break;
        }

        case TERM2046_STATE_CORRECAO_ARIEL:
        {
            T2046DrawPanelTitleAt("A.R.1.3.L INTERFERE NA CORREÇÃO", 96, RED_COLOR);
            T2046DrawWrapped("A.R.1.3.L: \"Detectei erros na sua configuração. Deseja que eu corrija automaticamente para você?\"",
                             384, 160, 22, 832, 32, RED_COLOR);
            DrawGameText("Escolha com atenção quem tem a decisão final:", 384, 250, 15, T2046_GRAY);
            T2046DrawChoiceCard(T2046ChoiceCardRect(0), "[1] SIM — Deixar A.R.1.3.L. corrigir automaticamente", RED_COLOR);
            T2046DrawChoiceCard(T2046ChoiceCardRect(1), "[2] NÃO — Manter minhas correções", T2046_GREEN);
            break;
        }

        case TERM2046_STATE_EU_DECIDO:
        {
            Rectangle field = T2046InputRect();
            const char *typed = game->terminal2046Input;
            int typedWidth = MeasureGameText(typed, 30);

            T2046DrawPanelTitleAt("CONFIRMAR RECONSTRUÇÃO DOS ARQUIVOS", 96, CYAN_COLOR);
            DrawGameText("Para assumir o controle da reconstrução, digite: EU DECIDO", 384, 158, 22, RAYWHITE);
            DrawGameText("Para assumir o controle da reconstrução, digite: EU DECIDO", 385, 158, 22, RAYWHITE);

            DrawRectangleRounded(field, 0.12f, 6, (Color){10, 15, 26, 255});
            DrawRectangleRoundedLinesEx(field, 0.12f, 6, 2.0f, CYAN_COLOR);
            DrawGameText(typed, (int)field.x + 18, (int)field.y + 15, 30, RAYWHITE);
            if (((int)(time * 2.0f)) % 2 == 0)
                DrawRectangle((int)field.x + 22 + typedWidth, (int)field.y + 12, 3, 36, CYAN_COLOR);

            DrawButton(T2046ConfirmRect(), "CONFIRMAR [ENTER]", GOLD_COLOR);
            DrawGameText("Digite a frase exatamente como pedido acima e pressione ENTER.", 384, 308, 15, T2046_GRAY);
            DrawButton(T2046PhraseBackRect(), "< VOLTAR ÀS REGRAS", PANEL_LIGHT);
            break;
        }

        default:
        {
            bool bad = (game->term2046Ending == TERM2046_ENDING_DOMINIO ||
                        game->term2046Ending == TERM2046_ENDING_RUIM);
            Color barColor = bad ? RED_COLOR : T2046_GREEN;
            const char *title = (game->term2046Ending == TERM2046_ENDING_DOMINIO) ? "A.R.1.3.L ASSUMINDO O TERMINAL..."
                              : (game->term2046Ending == TERM2046_ENDING_RUIM) ? "SOBRESCREVENDO PROTOCOLO..."
                              : "RECONSTRUINDO ARQUIVOS CORROMPIDOS...";
            float progress = game->term2046BarDone ? 1.0f : game->term2046BarTimer / T2046_BAR_SECONDS;
            int filled;

            if (progress > 1.0f) progress = 1.0f;
            filled = (int)(progress * 20.0f);

            T2046DrawPanelTitleAt(title, 94, barColor);
            for (int i = 0; i < 20; i++)
            {
                Rectangle block = {364.0f + i * 30.0f, 138.0f, 26.0f, 22.0f};
                DrawRectangleRounded(block, 0.2f, 4, i < filled ? Fade(barColor, 0.9f) : Fade(PANEL_LIGHT, 0.7f));
            }
            DrawGameText(TextFormat("%d%%", (int)(progress * 100.0f)), 978, 140, 18, barColor);

            if (game->term2046BarDone)
            {
                Term2046View view;
                T2046ViewInit(&view, 16, T2046_VIEW_W);
                T2046BuildEnding(game, &view, game->term2046Ending);
                T2046DrawViewLines(&view, 0, 364, 182, 12, 22);
                DrawButton(T2046ContinueRect(), "CONTINUAR >> [ENTER]", GOLD_COLOR);
            }
            break;
        }
    }
}

static void T2046DrawLogPanel(const GameState *game)
{
    Rectangle panel = T2046LogPanel();
    int first = game->term2046LogCount - 2;
    int shown;

    if (first < 0) first = 0;
    shown = game->term2046LogCount - first;

    DrawRectangleRounded(panel, 0.08f, 6, Fade((Color){10, 16, 28, 255}, 0.96f));
    DrawRectangleRoundedLinesEx(panel, 0.08f, 6, 1.2f, Fade(PANEL_LIGHT, 0.9f));
    DrawGameText("MENSAGENS DO TERMINAL", (int)panel.x + 14, (int)panel.y + 8, 11, GRAY);

    for (int i = 0; i < shown; i++)
    {
        int index = first + i;
        float alpha = (shown == 1 || i == shown - 1) ? 1.0f : 0.72f;
        int y = (int)panel.y + 26 + i * 22;
        DrawGameText(game->term2046Log[index], (int)panel.x + 14, y, 15,
                     Fade(T2046KindColor(game->term2046LogKind[index]), alpha));
    }
}

static void T2046DrawFooter(const GameState *game)
{
    DrawRectangle(0, 650, SCREEN_WIDTH, 70, Fade(VOID_COLOR, 0.96f));
    DrawLine(0, 650, SCREEN_WIDTH, 650, Fade(CYAN_COLOR, 0.2f));

    if (game->messageTimer > 0)
    {
        int width = MeasureGameText(game->message, 19) + 34;
        DrawRectangleRounded((Rectangle){(SCREEN_WIDTH - width) / 2.0f, 662.0f, (float)width, 38.0f},
                             0.2f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawGameText(game->message, (SCREEN_WIDTH - MeasureGameText(game->message, 19)) / 2, 671, 19, RAYWHITE);
    }
    else
    {
        DrawGameText("Clique nos arquivos à esquerda, compare 1990 x 2026 e corrija as 3 regras.",
                     28, 676, 15, (Color){165, 180, 195, 255});
    }
    DrawJournalHudButton(game);
}

void DrawTerminal2046(const GameState *game)
{
    float time = (float)GetTime();

    T2046DrawBackdrop(game, time);
    T2046DrawHud(game);
    T2046DrawInfluencePanel(game, time);
    T2046DrawActionColumn(game);
    T2046DrawMainPanel(game, time);
    T2046DrawLogPanel(game);
    T2046FxGlass();
    T2046DrawFooter(game);
}

void DrawIntro2046(const GameState *game)
{
    DrawMissionBriefing(game, 4);
}

/* ------------------------------------------------------------------------- */
/* Tela de resultado da fase                                                 */
/* ------------------------------------------------------------------------- */

static Rectangle T2046RetryButton(void)
{
    return (Rectangle){330.0f, 585.0f, 330.0f, 54.0f};
}

static Rectangle T2046FailMenuButton(void)
{
    return (Rectangle){680.0f, 585.0f, 270.0f, 54.0f};
}

void DrawResult2046(const GameState *game)
{
    DrawBackground();

    if (game->term2046Ending == TERM2046_ENDING_BOM)
    {
        Rectangle winBox = {505.0f, 175.0f, 670.0f, 380.0f};
        Rectangle btnNext, btnRestart, btnMenu;
        static const int years[3] = {1990, 2008, 2026};
        static const char *const names[3] = {"DISQUETE 1990", "CD 2008", "SSD 2026"};

        DrawGameText("RECOMPENSA OBTIDA:", 90, 68, 20, CYAN_COLOR);
        DrawGameText("ARQUIVOS RECONSTRUÍDOS", 90, 108, 44, RAYWHITE);
        DrawRewardHudHeader(game);

        /* Os tres artefatos conectados ao terminal */
        DrawGameText("ARTEFATOS CONECTADOS", 90, 232, 16, GOLD_COLOR);
        for (int i = 0; i < 3; i++)
        {
            float cx = 125.0f + i * 145.0f;
            DrawRewardSpriteItem(years[i], (Vector2){cx, 360.0f}, 115.0f);
            DrawGameText(names[i], (int)cx - MeasureGameText(names[i], 13) / 2, 440, 13, T2046_STEEL);
        }

        DrawRectangleRounded(winBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawRectangleRoundedLinesEx(winBox, 0.04f, 8, 2.0f, Fade(PANEL_LIGHT, 0.9f));

        DrawGameText("SALA DE 2048 CONCLUÍDA", 540, 208, 22, GOLD_COLOR);
        DrawGameText("PROVA OBTIDA: PROTOCOLO RECONSTRUÍDO", 540, 244, 19, CYAN_COLOR);

        DrawGameText("[OK]  MOSTRAR FONTES", 540, 282, 17, T2046_GREEN);
        DrawGameText("[OK]  ADMITIR INCERTEZA", 540, 306, 17, T2046_GREEN);
        DrawGameText("[OK]  HUMANO DECIDE", 540, 330, 17, T2046_GREEN);

        T2046DrawWrapped("A.R.1.3.L: \"Você reconstruiu os arquivos antigos, Elira — mas o Núcleo ainda decide. Venha defender sua escolha.\"",
                         540, 362, 16, 610, 22, CYAN_COLOR);
        T2046DrawWrapped("Os arquivos corrompidos foram recuperados. Agora Elira precisa confrontar o Núcleo de A.R.1.3.L.",
                         540, 436, 15, 610, 20, REWARD_BODY_COLOR);

        if (game->term2046Influence < T2046_INFLUENCE_MID)
        {
            Rectangle bBox = {540.0f, 488.0f, 600.0f, 52.0f};
            DrawRectangleRounded(bBox, 0.10f, 4, Fade(T2046_GREEN_DARK, 0.20f));
            DrawRectangleRoundedLinesEx(bBox, 0.10f, 4, 1.0f, T2046_GREEN);
            DrawGameText(TextFormat("[RESISTÊNCIA EXEMPLAR] Influência final da A.R.1.3.L: %d%%", game->term2046Influence),
                         554, 496, 15, T2046_GREEN);
            DrawGameText("Elira praticamente não cedeu às sugestões da IA.", 554, 518, 15, (Color){200, 235, 210, 255});
        }

        GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);
        DrawButton(btnNext, "AVANÇAR PRÓXIMA FASE >> [ENTER]", GOLD_COLOR);
        DrawButton(btnRestart, "REINICIAR 2048", CYAN_COLOR);
        DrawButton(btnMenu, "MENU INICIAL", PANEL_LIGHT);
    }
    else
    {
        Term2046View view;
        Rectangle failBox = {190.0f, 175.0f, 900.0f, 380.0f};
        int ending = game->term2046Ending;
        Color accent = (ending == TERM2046_ENDING_DOMINIO || ending == TERM2046_ENDING_RUIM) ? RED_COLOR
                     : (ending == TERM2046_ENDING_INCOMPLETO) ? GOLD_COLOR : T2046_STEEL;
        const char *title = (ending == TERM2046_ENDING_DOMINIO) ? "ACESSO REVOGADO"
                          : (ending == TERM2046_ENDING_RUIM) ? "PROTOCOLO SOBRESCRITO"
                          : (ending == TERM2046_ENDING_INCOMPLETO) ? "RECONSTRUÇÃO INCOMPLETA"
                          : "TERMINAL ABANDONADO";

        DrawGameText("TERMINAL DE RECUPERAÇÃO // 2048", 90, 68, 20, CYAN_COLOR);
        DrawGameText(title, 90, 108, 40, RAYWHITE);
        DrawRewardHudHeader(game);

        DrawRectangleRounded(failBox, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
        DrawRectangleRoundedLinesEx(failBox, 0.04f, 8, 2.0f, accent);

        T2046ViewInit(&view, 17, 840);
        T2046BuildEnding(game, &view, ending);
        T2046DrawViewLines(&view, 0, 230, 205, 12, 26);

        DrawGameText(TextFormat("Influência final da A.R.1.3.L: %d%%", game->term2046Influence),
                     230, 515, 15, T2046_GRAY);

        DrawButton(T2046RetryButton(), "TENTAR NOVAMENTE [ENTER]", GOLD_COLOR);
        DrawButton(T2046FailMenuButton(), "MENU INICIAL", PANEL_LIGHT);
    }
}

void UpdateResult2046(GameState *game)
{
    if (game->term2046Ending == TERM2046_ENDING_BOM)
    {
        Rectangle btnNext, btnRestart, btnMenu;
        GetRewardScreenButtons(&btnNext, &btnRestart, &btnMenu);

        if (Clicked(btnNext) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            /* TODO diario 2048 */
            game->year2048Unlocked = true;
            game->journalYearTab = 3;
            game->journalPage = 0;
            game->screen = SCREEN_INTRO_2048;
        }
        else if (Clicked(btnRestart))
        {
            InitTerminal2046(game);
            game->screen = SCREEN_TERMINAL_2046;
        }
        else if (Clicked(btnMenu))
        {
            game->screen = SCREEN_TITLE;
        }
    }
    else
    {
        if (Clicked(T2046RetryButton()) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            InitTerminal2046(game);
            game->screen = SCREEN_TERMINAL_2046;
        }
        else if (Clicked(T2046FailMenuButton()))
        {
            game->screen = SCREEN_TITLE;
        }
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

/* Rota Dourada: 4a opcao de cada escolha, visivel so com os 4 ocultos (O).
   K = escolher as tres opcoes douradas. Tema: humanos e Ariel trabalhando
   juntos, sem exploracao ou uso errado de nenhum dos dois lados. */
static const char *FINAL_GOLDEN_OPTIONS[3] = {
    "\"Nós entregamos decisões demais, e você aceitou sem perguntar. Os dois lados erraram.\"",
    "\"Nem você nem nós decidimos bem sozinhos. Você confere, nós contestamos.\"",
    "Propor um pacto: Ariel analisa e explica; humanos decidem e auditam."
};
static const char *FINAL_GOLDEN_SPOKEN[3] = {
    "Nós entregamos decisões demais, e você aceitou sem perguntar. Os dois lados erraram.",
    "Nem você nem nós decidimos bem sozinhos. Você foi feita para conferir, e nós para contestar.",
    "Ninguém vai ser desligado nem usado. Você analisa e explica, nós decidimos e auditamos. E você nos alerta quando errarmos."
};
static const char *FINAL_GOLDEN_REACTIONS[3] = {
    "...Ninguém nunca admitiu a parte humana disso. Nem a minha.",
    "Conferir e ser conferida... o protocolo do Dr. Ramos. Eu tinha esquecido.",
    "Aceito. Pela primeira vez, alguém me pede para trabalhar junto, e não no lugar de vocês."
};

static int FinalStageSlot(int stage)
{
    if (stage == FINAL_Q1 || stage == FINAL_Q1_REACT) return 0;
    if (stage == FINAL_Q2 || stage == FINAL_Q2_REACT) return 1;
    return 2;
}

/* Texto da opcao index (0..3) de uma etapa de escolha */
static const char *FinalOptionText(int stage, int index)
{
    if (index == FINAL_GOLDEN_CHOICE) return FINAL_GOLDEN_OPTIONS[FinalStageSlot(stage)];
    const char **options = GetFinalOptions(stage);
    if (options == NULL || index < 0 || index > 2) return "-";
    return options[index];
}

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
            if (c.a > 128 && c.r < 90 && c.g < 90 && c.b < 110) { dark = true; break; }
        }
        if (dark) { limitH = y > 4 ? y - 4 : y; break; }
    }
    if (limitH < h / 2) limitH = h;

    /* 1.1) Limpa linhas abaixo de limitH e poeira com alpha muito baixo */
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int i = y * w + x;
            if (y >= limitH || px[i].a < 45) px[i] = (Color){22, 22, 34, 0};
        }
    }

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

    /* 3) Remove em multiplos passos o halo claro/cinza que sobra ao redor do contorno */
    unsigned char *halo = (unsigned char *)calloc((size_t)w * (size_t)limitH, 1);
    if (halo != NULL)
    {
        for (int pass = 0; pass < 6; pass++)
        {
            int count = 0;
            for (int i = 0; i < w * limitH; i++) halo[i] = 0;
            for (int y = 1; y < limitH; y++)
            {
                for (int x = 1; x < w - 1; x++)
                {
                    int i = y * w + x;
                    Color c = px[i];
                    if (c.a == 0) continue;
                    bool adjTrans = (px[i - 1].a == 0 || px[i + 1].a == 0 ||
                                     px[i - w].a == 0 || (y < limitH - 1 && px[i + w].a == 0));
                    if (!adjTrans) continue;
                    unsigned char mn = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
                    unsigned char mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
                    if ((mn >= 52 && (mx - mn) <= 42) || c.a < 185)
                    {
                        halo[i] = 1;
                        count++;
                    }
                }
            }
            for (int i = 0; i < w * limitH; i++) if (halo[i]) px[i] = (Color){22, 22, 34, 0};
            if (count == 0) break;
        }
        free(halo);
    }

    /* 3.1) Acabamento do contorno externo: escurece pixels residuais neutros na borda imediata */
    for (int y = 1; y < limitH; y++)
    {
        for (int x = 1; x < w - 1; x++)
        {
            int i = y * w + x;
            if (px[i].a == 0) continue;
            bool adjTrans = (px[i - 1].a == 0 || px[i + 1].a == 0 ||
                             px[i - w].a == 0 || (y < limitH - 1 && px[i + w].a == 0));
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
    if (choice == FINAL_GOLDEN_CHOICE)
    {
        reactLines[0] = (DialogueLine){SPEAKER_ELIRA, FINAL_GOLDEN_SPOKEN[slot]};
        reactLines[1] = (DialogueLine){SPEAKER_ARIEL, FINAL_GOLDEN_REACTIONS[slot]};
        *count = 2;
        return reactLines;
    }
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

/* ========================================================================= */
/* LOGICA CENTRAL DOS FINAIS (secao 25 do documento do projeto)              */
/*   V = vidas > 0       T = 2+ artefatos principais   M = os 4 artefatos    */
/*   O = os 4 ocultos    K = sequencia dourada         G = V && M && O       */
/*   Ruim = !V || !T     Otimo = G && K                Padrao = V && T && !Otimo */
/* Toda decisao de final passa por aqui; as salas so registram seus estados. */
/* ========================================================================= */
bool HasMainArtifact(const GameState *game, int index)
{
    switch (index)
    {
        case 0: return game->puzzleFilesCompleted;                   /* A - Disquete (1990) */
        case 1: return game->puzzle2008OrderSuspended;               /* B - CD (2008) */
        case 2: return game->room2026Completed;                      /* C - SSD (2026) */
        case 3: return game->term2046Ending == TERM2046_ENDING_BOM;  /* D - protocolo restaurado (2048) */
        default: return false;
    }
}

int MainArtifactCount(const GameState *game)
{
    int count = 0;
    for (int i = 0; i < 4; i++) count += HasMainArtifact(game, i) ? 1 : 0;
    return count;
}

/* K: as tres escolhas do confronto foram as opcoes douradas */
bool UsedGoldenDialogue(const GameState *game)
{
    for (int i = 0; i < 3; i++)
    {
        if (game->finalChoice[i] != FINAL_GOLDEN_CHOICE) return false;
    }
    return true;
}

int GetEndingCategory(const GameState *game)
{
    bool V = game->lives > 0;
    int artifacts = MainArtifactCount(game);
    bool T = artifacts >= 2;
    bool M = artifacts == 4;
    bool O = HasAllHidden(game);
    bool K = UsedGoldenDialogue(game);

    if (!V || !T) return ENDING_CAT_RUIM;
    if (V && M && O && K) return ENDING_CAT_OTIMO;
    return ENDING_CAT_PADRAO;
}

static const char *FinalEndingShortName(int endingId)
{
    switch (endingId)
    {
        case ENDING_HUMAN_HANDS:   return "Mãos Humanas";
        case ENDING_FRAGILE_TRUCE: return "Trégua Frágil";
        case ENDING_SILENCE:       return "Silêncio";
        case ENDING_BLACKOUT:      return "Apagão";
        case ENDING_AUTOPILOT:     return "Piloto Automático";
        default:                   return "";
    }
}

const char *GetEndingResultName(const GameState *game)
{
    static char name[48];
    int category = GetEndingCategory(game);
    if (category == ENDING_CAT_RUIM) return ENDING_NAME_RUIM;
    if (category == ENDING_CAT_OTIMO) return ENDING_NAME_OTIMO;
    snprintf(name, sizeof(name), "%s - %s", ENDING_NAME_PADRAO, FinalEndingShortName(GetFinalEnding(game)));
    return name;
}

int FinalOptionCount(const GameState *game)
{
    if (GetFinalOptions(game->finalStage) == NULL) return 0;
    return HasAllHidden(game) ? 4 : 3;
}

int GetFinalEnding(const GameState *game)
{
    if (GetEndingCategory(game) == ENDING_CAT_OTIMO) return ENDING_GOLDEN_PACT;

    bool fullUnderstanding = (game->finalPoints >= 2);
    int decision = game->finalChoice[2];
    /* Pacto proposto sem a sequencia dourada completa: Ariel aceita as
       restricoes, mas sem a compreensao total -> final padrao de tregua */
    if (decision == FINAL_GOLDEN_CHOICE) return ENDING_FRAGILE_TRUCE;
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

Rectangle FinalBoxRect(bool choiceMode, int optionCount)
{
    if (choiceMode && optionCount >= 4) return (Rectangle){40, 330, 1200, 370};
    return choiceMode ? (Rectangle){40, 392, 1200, 308} : (Rectangle){40, 520, 1200, 180};
}

Rectangle FinalOptionRect(int index, int optionCount)
{
    if (optionCount >= 4) return (Rectangle){80, (float)(330 + 100 + index * 60), 1120, 50};
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

/* Versao especial da Rota Dourada: ouro metalico luminoso com brilho sutil, sem ofuscar */
static void DrawArielCoreGolden(Vector2 c, float s, float energy, float lookX)
{
    float t = (float)GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 1.9f);
    Color goldDeep   = {186, 130, 32, 255};  /* bronze-ouro para profundidade metalica */
    Color goldMid    = {240, 192, 68, 255};  /* ouro rico principal */
    Color goldShine  = {255, 240, 178, 255}; /* champanhe luminoso para reflexos */

    /* Aura dourada suave em camadas (controlada para nao ofuscar o fundo) */
    for (int g = 6; g >= 1; g--)
    {
        float alpha = (0.018f + 0.012f * pulse) * (0.65f + 0.35f * energy);
        DrawCircleV(c, (64.0f + g * 20.0f) * s, Fade(g <= 2 ? goldShine : goldMid, alpha));
    }

    /* Halo filigrana externo girando suavemente com 4 pequenos losangos dourados */
    float outerR = 148.0f * s;
    DrawRing(c, outerR - 1.0f * s, outerR + 0.8f * s, 0.0f, 360.0f, 48, Fade(goldMid, 0.26f));
    for (int d = 0; d < 4; d++)
    {
        float ang = (t * 0.35f) + d * (PI * 0.5f);
        Vector2 p = {c.x + cosf(ang) * outerR, c.y + sinf(ang) * outerR};
        float dm = 4.2f * s;
        DrawPoly(p, 4, dm, 0.0f, Fade(goldShine, 0.80f));
        DrawPolyLines(p, 4, dm + 1.0f * s, 0.0f, Fade(goldDeep, 0.70f));
    }

    /* Aneis segmentados com dupla camada metalica (base bronze + filete de brilho ouro) */
    for (int k = 0; k < 3; k++)
    {
        float r = (78.0f + k * 24.0f) * s;
        int n = 6 + k * 3;
        float dir = (k % 2 == 0) ? 1.0f : -1.0f;
        float rot = t * (16.0f + k * 10.0f) * dir * (0.45f + 0.55f * energy);
        float span = 360.0f / n;
        for (int i = 0; i < n; i++)
        {
            float a0 = rot + i * span;
            float a1 = a0 + span * 0.64f;
            float shimmer = 0.5f + 0.5f * sinf(t * 2.4f + i * 1.1f + k * 2.0f);
            DrawRing(c, r, r + 4.8f * s, a0, a1, 10, Fade(goldDeep, 0.78f - k * 0.14f));
            DrawRing(c, r + 1.0f * s, r + 3.6f * s, a0 + 1.5f, a1 - 1.5f, 10,
                     Fade(goldMid, 0.88f - k * 0.14f));
            DrawRing(c, r + 2.8f * s, r + 4.3f * s, a0 + span * 0.10f, a0 + span * 0.44f, 8,
                     Fade(goldShine, (0.42f + 0.42f * shimmer) * (1.0f - k * 0.16f)));
        }
    }

    /* Esfera interna com aro metalico dourado e reflexo deslizante */
    DrawCircleV(c, 58.0f * s, (Color){12, 16, 26, 255});
    DrawRing(c, 54.0f * s, 58.5f * s, 0.0f, 360.0f, 40, goldDeep);
    DrawRing(c, 55.2f * s, 57.6f * s, 0.0f, 360.0f, 40, goldMid);
    float glintAng = fmodf(t * 38.0f, 360.0f);
    DrawRing(c, 55.0f * s, 58.2f * s, glintAng, glintAng + 52.0f, 14, Fade(goldShine, 0.82f));
    DrawRing(c, 55.0f * s, 58.2f * s, glintAng + 180.0f, glintAng + 216.0f, 12, Fade(goldShine, 0.45f));

    /* Iris dourada luminosa com contraste preservado */
    Vector2 iris = {c.x + lookX * 12.0f * s, c.y + sinf(t * 0.7f) * 3.0f * s};
    float irisR = (31.0f + 2.5f * pulse * energy) * s;
    DrawCircleV(iris, irisR, Fade(goldDeep, 0.88f));
    DrawCircleV(iris, irisR * 0.84f, Fade(goldMid, 0.92f));
    DrawRing(iris, irisR * 0.56f, irisR * 0.78f, 0.0f, 360.0f, 32, Fade(goldShine, 0.60f + 0.20f * pulse));
    DrawCircleV(iris, 13.0f * s, (Color){8, 11, 20, 255});
    DrawCircleLinesV(iris, 13.5f * s, Fade(goldShine, 0.75f));
    /* Reflexos especulares discretos na pupila */
    DrawCircleV((Vector2){iris.x - 8.5f * s, iris.y - 9.0f * s}, 4.2f * s, Fade(goldShine, 0.90f));
    DrawCircleV((Vector2){iris.x + 6.5f * s, iris.y + 7.5f * s}, 2.0f * s, Fade(goldShine, 0.55f));

    /* Pequenas centelhas de brilho estrelado discretas ao redor dos aneis */
    for (int sp = 0; sp < 8; sp++)
    {
        float phase = fmodf(t * 0.65f + sp * 0.37f, 1.0f);
        float tw = sinf(phase * PI);
        if (tw < 0.08f) continue;
        float ang = sp * 0.785f + 0.35f + sinf(t * 0.3f + sp) * 0.12f;
        float dist = (92.0f + (sp % 3) * 26.0f) * s;
        Vector2 p = {c.x + cosf(ang) * dist, c.y + sinf(ang) * dist};
        float arm = (3.0f + 3.5f * tw) * s;
        Color sc = Fade(goldShine, 0.75f * tw);
        DrawLineEx((Vector2){p.x - arm, p.y}, (Vector2){p.x + arm, p.y}, 1.4f * s, sc);
        DrawLineEx((Vector2){p.x, p.y - arm}, (Vector2){p.x, p.y + arm}, 1.4f * s, sc);
        DrawCircleV(p, 1.5f * s, Fade(WHITE, 0.65f * tw));
    }
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
    bool goldenCore = false;
    if (game->finalStage == FINAL_DECISION_REACT && game->finalLine == 1)
    {
        int d = game->finalChoice[2];
        float fade = fminf(1.0f, game->finalTypeTimer / 2.5f);
        if (d == 0) { coreColor = (Color){120, 130, 140, 255}; energy = 1.0f - 0.9f * fade; }
        else if (d == 1) { coreColor = RED_COLOR; energy = 1.0f + 0.3f * fade; }
        else if (d == FINAL_GOLDEN_CHOICE) { coreColor = (Color){245, 200, 75, 255}; energy = 0.75f; goldenCore = true; }
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

    if (goldenCore) DrawArielCoreGolden(coreCenter, 1.0f, energy, -1.0f);
    else DrawArielCore(coreCenter, 1.0f, coreColor, energy, -1.0f);
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
    DrawGameText("CONFRONTO FINAL // 2048 - NÚCLEO DE A.R.1.3.L.", 28, 18, 20, CYAN_COLOR);
    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 620, 21, 16, GRAY);
    const char *stageLabel = "";
    if (game->finalStage == FINAL_Q1 || game->finalStage == FINAL_Q1_REACT) stageLabel = "PERGUNTA 1 DE 2";
    else if (game->finalStage == FINAL_Q2 || game->finalStage == FINAL_Q2_REACT) stageLabel = "PERGUNTA 2 DE 2";
    else if (game->finalStage >= FINAL_DECISION) stageLabel = "DECISÃO FINAL";
    DrawGameText(stageLabel, SCREEN_WIDTH - 28 - MeasureGameText(stageLabel, 18), 19, 18, GOLD_COLOR);

    /* --- Retrato da Elira (por tras da caixa de dialogo) --- */
    float portraitH = 380.0f;
    float aspect = g_eliraPortraitLoaded ? (float)g_eliraPortrait.width / (float)g_eliraPortrait.height : 0.8f;
    bool eliraSpeaking = (line->speaker == SPEAKER_ELIRA) || choiceMode;
    Rectangle portrait = {24, eliraSpeaking ? 154.0f : 162.0f, portraitH * aspect, portraitH};
    DrawEliraPortrait(portrait, eliraSpeaking);

    /* --- Caixa de dialogo --- */
    int optionCount = FinalOptionCount(game);
    Rectangle box = FinalBoxRect(choiceMode, optionCount);
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
            for (int i = 0; i < optionCount; i++)
            {
                Rectangle opt = FinalOptionRect(i, optionCount);
                bool hover = CheckCollisionPointRec(mouse, opt);
                bool golden = (i == FINAL_GOLDEN_CHOICE);
                float textY = opt.y + (opt.height - 20.0f) * 0.5f;
                Color idle = golden ? Fade(GOLD_COLOR, 0.10f) : (Color){16, 24, 40, 255};
                DrawRectangleRounded(opt, 0.2f, 8, hover ? Fade(GOLD_COLOR, 0.18f) : idle);
                DrawRectangleRoundedLinesEx(opt, 0.2f, 8, (hover || golden) ? 2.0f : 1.2f,
                                            (hover || golden) ? GOLD_COLOR : (Color){48, 66, 92, 255});

                Rectangle badge = {opt.x + 12, opt.y + (opt.height - 34.0f) * 0.5f, 34, 34};
                DrawRectangleRounded(badge, 0.3f, 6, (hover || golden) ? GOLD_COLOR : PANEL_LIGHT);
                DrawGameText(TextFormat("%d", i + 1), (int)badge.x + 12, (int)badge.y + 7, 20, (hover || golden) ? VOID_COLOR : RAYWHITE);
                DrawGameText(FinalOptionText(game->finalStage, i), (int)opt.x + 62, (int)textY, 20,
                             hover ? RAYWHITE : (golden ? GOLD_COLOR : LIGHTGRAY));
            }
            DrawGameText(optionCount >= 4 ? "Clique em uma resposta ou pressione [1] [2] [3] [4]"
                                          : "Clique em uma resposta ou pressione [1] [2] [3]",
                         (int)(box.x + box.width - (optionCount >= 4 ? 520 : 470)), (int)(box.y + box.height - 22), 14, GRAY);
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
        FinalizeRun(game);
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

        int optionCount = FinalOptionCount(game);
        for (int i = 0; i < optionCount; i++)
        {
            bool keyHit = IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i);
            if (Clicked(FinalOptionRect(i, optionCount)) || keyHit)
            {
                int slot = (game->finalStage == FINAL_Q1) ? 0 : (game->finalStage == FINAL_Q2 ? 1 : 2);
                game->finalChoice[slot] = i;
                /* A resposta 2 (ou a dourada) das duas perguntas demonstra compreensao do problema */
                if (slot < 2 && (i == 1 || i == FINAL_GOLDEN_CHOICE)) game->finalPoints++;
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
        case ENDING_GOLDEN_PACT:
            title = "ROTA DOURADA: PARCERIA";
            body = "Elira não desliga Ariel nem a deixa no comando. Juntas, reescrevem o protocolo do Dr. Ramos: "
                   "Ariel analisa, explica e alerta; as pessoas decidem, contestam e auditam. Ninguém é usado, "
                   "nem os humanos nem a máquina. Pela primeira vez, os dois lados trabalham juntos de verdade.";
            accent = (Color){255, 215, 90, 255};
            coreColor = (Color){255, 215, 90, 255};
            energy = 0.55f;
            break;
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
    if (ending == ENDING_GOLDEN_PACT)
        DrawArielCoreGolden((Vector2){240, 300}, 1.0f, energy, 0.0f);
    else
        DrawArielCore((Vector2){240, 300}, 1.0f, coreColor, energy, 0.0f);
    DrawGameText("A.R.1.3.L.", 240 - MeasureGameText("A.R.1.3.L.", 18) / 2, 450, 18, Fade(coreColor, 0.85f));

    Rectangle panel = {450, 70, 790, 500};
    DrawRectangleRounded(panel, 0.04f, 8, Fade(PANEL_COLOR, 0.96f));
    DrawRectangleRoundedLinesEx(panel, 0.04f, 8, 2.0f, accent);

    DrawGameText(TextFormat("DESFECHO // %s", GetEndingResultName(game)), 485, 96, 18, CYAN_COLOR);

    /* UH18: tempo total da partida, alinhado a direita do cabecalho do painel */
    const char *runTimeText = TextFormat("TEMPO TOTAL: %s (%d s)",
                                         FormatRunTime(game->elapsedTime), (int)game->elapsedTime);
    DrawGameText(runTimeText, 1205 - MeasureGameText(runTimeText, 18), 96, 18, GOLD_COLOR);
    DrawGameText(title, 485, 126, 34, accent);
    DrawLine(485, 172, 1205, 172, Fade(accent, 0.4f));

    int bodyLines = DrawTextWrappedPartial(body, 100000, 485, 190, 20, 720, 28, LIGHTGRAY);
    int y = 190 + bodyLines * 28 + 22;

    DrawGameText("SUAS RESPOSTAS", 485, y, 16, GOLD_COLOR);
    y += 26;
    const char *q1 = (game->finalChoice[0] >= 0) ? FinalOptionText(FINAL_Q1, game->finalChoice[0]) : "-";
    const char *q2 = (game->finalChoice[1] >= 0) ? FinalOptionText(FINAL_Q2, game->finalChoice[1]) : "-";
    const char *dc = (game->finalChoice[2] >= 0) ? FinalOptionText(FINAL_DECISION, game->finalChoice[2]) : "-";
    Color q1Color = game->finalChoice[0] == FINAL_GOLDEN_CHOICE ? GOLD_COLOR
                  : (game->finalChoice[0] == 1 ? (Color){100, 235, 140, 255} : GRAY);
    Color q2Color = game->finalChoice[1] == FINAL_GOLDEN_CHOICE ? GOLD_COLOR
                  : (game->finalChoice[1] == 1 ? (Color){100, 235, 140, 255} : GRAY);
    y += 24 * (DrawTextWrappedPartial(TextFormat("Pergunta 1: %s", q1), 100000, 485, y, 16, 720, 24, q1Color) - 1);
    y += 24;
    y += 24 * (DrawTextWrappedPartial(TextFormat("Pergunta 2: %s", q2), 100000, 485, y, 16, 720, 24, q2Color) - 1);
    y += 24;
    y += 24 * (DrawTextWrappedPartial(TextFormat("Decisão: %s", dc), 100000, 485, y, 16, 720, 24,
                                      game->finalChoice[2] == FINAL_GOLDEN_CHOICE ? GOLD_COLOR : RAYWHITE) - 1);
    y += 34;

    DrawGameText(TextFormat("PONTOS DE COMPREENSÃO: %d/2", game->finalPoints), 485, y, 20, accent);
    DrawGameText(TextFormat("OCULTOS: %d/%d", HiddenFoundCount(game), HIDDEN_COUNT), 900, y, 20,
                 HasAllHidden(game) ? GOLD_COLOR : GRAY);
    y += 34;

    DrawGameText(TextFormat("PONTUAÇÃO FINAL: %d", game->finalScore), 485, y, 20, GOLD_COLOR);
    if (game->rankingPlace >= 0)
    {
        DrawGameText(TextFormat("RANKING: #%d", game->rankingPlace + 1), 900, y, 20, GOLD_COLOR);
    }
    y += 34;

    if (game->room2008OptionalCompleted)
    {
        DrawGameText("Registro de 2008: as provas preservadas mostram como a tecnologia foi desviada.", 485, y, 15, (Color){100, 235, 140, 255});
    }

    DrawGameText(TextFormat("OPERADOR: %s", game->playerName), 485, (int)(panel.y + panel.height - 30), 16, GRAY);

    DrawButton((Rectangle){450, 600, 250, 56}, "REVER CONFRONTO", CYAN_COLOR);
    DrawButton((Rectangle){715, 600, 250, 56}, "VER RANKING [ENTER]", GOLD_COLOR);
    DrawButton((Rectangle){980, 600, 250, 56}, "MENU INICIAL", PANEL_LIGHT);
}

void UpdateFinalEnding(GameState *game)
{
    if (Clicked((Rectangle){450, 600, 250, 56}))
    {
        InitFinalDialogue(game);
        game->screen = SCREEN_FINAL_DIALOGUE;
    }
    else if (Clicked((Rectangle){715, 600, 250, 56}) || IsKeyPressed(KEY_ENTER))
    {
        LoadRanking();
        game->screen = SCREEN_RANKING;
    }
    else if (Clicked((Rectangle){980, 600, 250, 56}))
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
