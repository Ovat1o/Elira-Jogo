#include "elira.h"
#include <stdio.h>
#include <string.h>

void InitElira(Elira *player) {
    player->lives = 3;
    player->hasFloppy1990 = false;
    player->position = (Vector2){ 0, 0 };
}

void CheckPointAndClick(Elira *player, Vector2 mousePos, bool clicked, char *dialogBuffer) {
    if (!clicked) return;

    // Hitbox de teste do diário (900, 450, 60, 40)
    Rectangle diaryHitbox = { 900, 450, 60, 40 };
    if (CheckCollisionPointRec(mousePos, diaryHitbox)) {
        strcpy(dialogBuffer, "DIARIO: Para contornar a seguranca, aperte ESPACO no terminal.");
    }
}