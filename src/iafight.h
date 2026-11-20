/* =============================================================================
 * iafight.h - Interface publica do MODO IA FIGHT (implementado em iafight.c).
 * Luta 1x1 em visao lateral entre ELIRA, CLAUDE, CODEX e ANTIGRAVITY.
 * O estado do modo e privado ao modulo; o main.c so roteia SCREEN_IAFIGHT.
 * ============================================================================= */
#ifndef IAFIGHT_H
#define IAFIGHT_H

#include "funcoes.h"

/* Carrega os recursos do modo (so na primeira vez) e abre a selecao de lutador. */
void IafEnter(GameState *game);

/* Le o teclado e avanca o modo; volta game->screen para SCREEN_TITLE quando o jogador sai. */
void IafUpdate(GameState *game, float dt);

/* Desenha o modo no render target que o main.c ja abriu (1280x720). */
void IafDraw(void);

/* Libera a textura do modo (chamar antes de CloseWindow). */
void IafUnload(void);

#endif
