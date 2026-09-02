#include "timeline.h"

void InitRoom1990(Room1990 *room) {
    room->currentState = INTRO;
    room->timer = 300.0f; // 5 minutos
    room->windowSpace = (Rectangle){ 100, 100, 200, 150 };
    room->diary = (Rectangle){ 900, 450, 60, 40 };
    room->terminal = (Rectangle){ 1000, 200, 150, 250 };
}

void UpdateRoom1990(Room1990 *room, float deltaTime) {
    if (room->currentState == ROOM_1990 || room->currentState == ENIGMA) {
        room->timer -= deltaTime;
        if (room->timer <= 0) room->currentState = DEFEAT;
    }
}