#ifndef TIMELINE_H
#define TIMELINE_H

#include "raylib.h"
#include "raymath.h"


typedef enum { INTRO, ROOM_1990, ENIGMA, WIN, DEFEAT } GameState;

typedef struct {
    GameState currentState;
    float timer;
    Rectangle windowSpace;
    Rectangle terminal;
    Rectangle diary;
} Room1990;

void InitRoom1990(Room1990 *room);
void UpdateRoom1990(Room1990 *room, float deltaTime);

#endif