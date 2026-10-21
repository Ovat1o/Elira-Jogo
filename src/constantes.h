#ifndef CONSTANTES_H
#define CONSTANTES_H

/* Dimensoes da tela e configuracoes principais */
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ACCESS_CODE "1950"
#define PLAYER_NAME_MAX 20

/* ========================================================================= */
/* FONTE GERAL DO JOGO                                                       */
/* ========================================================================= */
/* COMO TROCAR A FONTE:
     1. Coloque o arquivo da fonte (.ttf ou .otf) na pasta assets/fonts/
     2. Altere GAME_FONT_FILE abaixo para o nome do arquivo
     3. Recompile (make clean && make run)
   Use GAME_FONT_FILE "" para usar a fonte padrao (pixelada) embutida na raylib.
   Exemplos:
     #define GAME_FONT_FILE "GeistPixel-Square.ttf"   -> Geist Pixel (Square)
     #define GAME_FONT_FILE "MinhaFonte.otf"          -> qualquer outra fonte
     #define GAME_FONT_FILE ""                        -> fonte padrao da raylib
   Cada tamanho e rasterizado sob demanda (cache por tamanho em pixels). Se o
   arquivo nao for encontrado, o jogo volta automaticamente para a fonte padrao. */
#define GAME_FONT_DIR      "assets/fonts/"
#ifndef GAME_FONT_FILE
#define GAME_FONT_FILE     "GeistPixel-Square.ttf"
#endif
#define GAME_FONT_PATH     GAME_FONT_DIR GAME_FONT_FILE
#define GAME_FONT_MAX_SIZE 192

/* Filtro da textura da fonte: 1 = pixel art (POINT, bordas nitidas),
   0 = fonte suave/vetorial (BILINEAR, bordas suavizadas) */
#define GAME_FONT_PIXEL_ART 1

/* Metricas: por padrao a escala e o deslocamento vertical sao calibrados
   AUTOMATICAMENTE lendo a altura da letra 'H' da fonte escolhida, para que ela
   ocupe a mesma caixa da fonte padrao da raylib (preservando todos os layouts).
   Para ajustar manualmente, mude GAME_FONT_MANUAL_METRICS para 1 e edite os
   valores abaixo (ex.: Geist Pixel = 1.3f e 0.19f). */
#define GAME_FONT_MANUAL_METRICS 0
#define GAME_FONT_SCALE          1.3f
#define GAME_FONT_OFFSET_Y       0.19f

/* ========================================================================= */
/* PARAMETROS DA ABERTURA CINEMATOGRAFICA (AJUSTAVEIS)                       */
/* ========================================================================= */
#define CRAWL_SPEED             34.0f   /* Velocidade de subida acelerada para leitura agradavel */
#define CRAWL_PERSPECTIVE_K     1.15f   /* Inclinacao 3D em perspectiva estilo Star Wars */
#define CRAWL_FONT_SIZE_TITLE   64.0f   /* Titulo ELIRA imponente */
#define CRAWL_FONT_SIZE_BODY    30.0f   /* Tamanho base do texto narrativo bem legivel */
#define CRAWL_FONT_SIZE_YEAR    44.0f   /* Ano final '1990' em destaque */
#define CRAWL_LINE_SPACING      44.0f   /* Espacamento vertical entre linhas normais */
#define CRAWL_PARAGRAPH_SPACING 34.0f   /* Espacamento extra entre paragrafos */
#define CRAWL_MIN_SCALE         0.34f   /* Escala minima no horizonte (profundidade Star Wars) */
#define CRAWL_MAX_SCALE         1.15f   /* Escala maxima na base da tela */
#define CRAWL_HORIZON_Y         100.0f  /* Posicao Y do horizonte de fuga */
#define CRAWL_BOTTOM_Y          675.0f  /* Posicao Y da base de entrada */

/* ========================================================================= */
/* CONFRONTO FINAL (2048) - SALA DE DIALOGO ELIRA x ARIEL                    */
/* ========================================================================= */
#define FINAL_TYPE_SPEED 48.0f /* bytes por segundo no efeito maquina de escrever */

/* Macro auxiliar para remocao do fundo do retrato (flood fill) */
#define PORTRAIT_PUSH(X, Y) do { int _i = (Y) * w + (X); \
    if (px[_i].a != 0 && IsPortraitBackground(px[_i])) { px[_i].a = 0; stack[sp++] = _i; } } while (0)

#endif /* CONSTANTES_H */
