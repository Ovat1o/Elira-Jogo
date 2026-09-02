#include "raylib.h"
#include "raymath.h"
#include "elira.h"
#include "timeline.h"
#include "ariel.h"

#define CREME (Color){253, 250, 239, 255}
#define BRANCO (Color){255, 254, 254, 255}
#define ESCURO (Color){40, 40, 40, 255}

int main(void) {
    InitWindow(1280, 720, "Escape Run Temporal");
    SetTargetFPS(60);

    Elira player;
    Room1990 room;
    Ariel ai;

    InitElira(&player);
    InitRoom1990(&room);
    InitAriel(&ai);

    char dialogBuffer[256] = "Clique nos objetos da sala para investigar.";

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // Atualização Lógica
        UpdateRoom1990(&room, dt);

        if (room.currentState == INTRO && clicked) {
            room.currentState = ROOM_1990;
        } else if (room.currentState == ROOM_1990) {
            CheckPointAndClick(&player, mouse, clicked, dialogBuffer);
            if (clicked && CheckCollisionPointRec(mouse, room.terminal)) {
                room.currentState = ENIGMA;
            }
        } else if (room.currentState == ENIGMA) {
            if (IsKeyPressed(KEY_SPACE)) {
                player.hasFloppy1990 = true;
                room.currentState = WIN;
            }
        }

        // Renderização
        BeginDrawing();
        ClearBackground(CREME);

        if (room.currentState == INTRO) {
            DrawRectangle(100, 100, 1080, 520, BRANCO);
            DrawText("Escape Run Temporal - 1990", 150, 150, 30, ESCURO);
            DrawText("Clique para iniciar...", 150, 550, 20, DARKGRAY);
        } 
        else if (room.currentState == ROOM_1990) {
            DrawRectangleRec(room.diary, BLUE);
            DrawRectangleRec(room.terminal, BLACK);
            
            // HUD
            DrawRectangle(20, 600, 1240, 100, BRANCO);
            DrawText(dialogBuffer, 40, 620, 20, ESCURO);
            DrawText(GetArielDialogue(&ai, player.lives), 40, 650, 18, RED);
        } 
        else if (room.currentState == ENIGMA) {
            DrawRectangle(400, 200, 480, 320, BRANCO);
            DrawText("TERMINAL: Aperte ESPACO para decifrar", 430, 300, 20, ESCURO);
        } 
        else if (room.currentState == WIN) {
            DrawText("ARTEFATO COLETADO: DISQUETE 1990", 350, 350, 30, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}