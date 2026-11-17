/* =============================================================================
 * invasao.h - Interface publica do MODO INVASAO (implementado em invasao.c).
 * O estado do modo e privado ao modulo; o main.c so roteia SCREEN_INVASION.
 * ============================================================================= */
#ifndef INVASAO_H
#define INVASAO_H

#include "funcoes.h"

/* Carrega os recursos do modo (so na primeira vez) e comeca um jogo novo em 1990. */
void InvEnter(GameState *game);

/* Le o teclado e avanca o modo; volta game->screen para SCREEN_TITLE quando o jogador sai. */
void InvUpdate(GameState *game, float dt);

/* Desenha o modo no render target que o main.c ja abriu (1280x720). */
void InvDraw(void);

/* Libera texturas e fontes do modo (chamar antes de CloseWindow). */
void InvUnload(void);

#endif
