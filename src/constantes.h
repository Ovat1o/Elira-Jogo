#ifndef CONSTANTES_H
#define CONSTANTES_H

/* Dimensoes da tela e configuracoes principais */
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ACCESS_CODE "1950"
#define PLAYER_NAME_MAX 20

/* Fonte geral do jogo: Geist Pixel (Square). Cada tamanho e rasterizado sob demanda
   para manter os pixels nitidos; sem o arquivo, volta para a fonte padrao da raylib. */
#define GAME_FONT_PATH "assets/fonts/GeistPixel-Square.ttf"
#define GAME_FONT_MAX_SIZE 192
/* A raylib dimensiona TTFs por ascendente+descendente (1300/1000 do em na Geist).
   Com 1.3x e subindo 0.19x, a Geist ocupa a mesma caixa da fonte padrao
   (altura de maiuscula e largura praticamente iguais), preservando os layouts. */
#define GAME_FONT_SCALE    1.3f
#define GAME_FONT_OFFSET_Y 0.19f

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
