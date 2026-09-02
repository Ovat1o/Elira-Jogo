#ifndef ELIRA_H
#define ELIRA_H

#include "raylib.h"
#include "raymath.h"

typedef struct {
    int lives;
    bool hasFloppy1990;
    Vector2 position;
} Elira;

void InitElira(Elira *player);
void CheckPointAndClick(Elira *player, Vector2 mousePos, bool clicked, char *dialogBuffer);

#endif