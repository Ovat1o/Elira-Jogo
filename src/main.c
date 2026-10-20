#include <math.h>
#include <string.h>
#include "funcoes.h"

int main(void)
{
    GameState game = {0};

    /* Os caminhos "assets/..." passam a valer a partir da pasta do executavel,
       mesmo quando o jogo e aberto pelo Finder/Explorer ou de outra pasta */
    ChangeDirectory(GetApplicationDirectory());

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ELIRA - Escape Run Temporal");
    InitAudioDevice();

    LoadEliraPortrait();

    EnsureAmbientMusicFile();

    Music ambientBgm = {0};
    bool bgmLoaded = false;
    if (FileExists("assets/ambient.wav"))
    {
        ambientBgm = LoadMusicStream("assets/ambient.wav");
        if (ambientBgm.stream.buffer != NULL)
        {
            ambientBgm.looping = true;
            SetMusicVolume(ambientBgm, 0.45f);
            PlayMusicStream(ambientBgm);
            bgmLoaded = true;
        }
    }

    /* Buffer virtual de renderizacao para escalonamento perfeito em qualquer resolucao/fullscreen */
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    SetTargetFPS(60);
    game.screen = SCREEN_TITLE;
    ResetDemo(&game);

    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();
        if (game.messageTimer > 0) game.messageTimer -= delta;

        if (bgmLoaded)
        {
            UpdateMusicStream(ambientBgm);
        }

        /* Alternar tela cheia globalmente com F11 ou Alt+Enter */
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)))
        {
            ToggleFullscreen();
        }

        /* Alternar audio com tecla M ou clique no HUD */
        Rectangle audioHudBtn = {840, 665, 175, 40};
        bool toggleAudio = IsKeyPressed(KEY_M) || ((game.screen == SCREEN_ROOM || game.screen == SCREEN_ROOM_2 || game.screen == SCREEN_ROOM_2008 || game.screen == SCREEN_ROOM_2026) && Clicked(audioHudBtn));
        if (toggleAudio)
        {
            game.musicMuted = !game.musicMuted;
            if (bgmLoaded)
            {
                SetMusicVolume(ambientBgm, game.musicMuted ? 0.0f : g_musicVolume);
            }
            SetMessage(&game, game.musicMuted ? "Musica ambiente desativada [Mudo]."
                                              : "Musica ambiente ativada.");
        }

        switch (game.screen)
        {
            case SCREEN_TITLE:
                if (Clicked((Rectangle){86, 436, 350, 56}) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game.screen = SCREEN_PLAYER_NAME;
                }
                else if (Clicked((Rectangle){86, 506, 350, 52}) || IsKeyPressed(KEY_C))
                {
                    game.screen = SCREEN_SETTINGS;
                }
                break;
            case SCREEN_SETTINGS:
                UpdateSettings(&game, &ambientBgm, bgmLoaded);
                break;
            case SCREEN_PLAYER_NAME:
                UpdatePlayerName(&game);
                if (Clicked((Rectangle){490, 470, 300, 58}) || IsKeyPressed(KEY_ENTER))
                {
                    if (game.playerNameLength == 0)
                    {
                        strcpy(game.playerName, "OPERADOR");
                        game.playerNameLength = (int)strlen(game.playerName);
                    }
                    game.message[0] = '\0';
                    game.messageTimer = 0.0f;
                    InitCinematicIntro(&game);
                    game.screen = SCREEN_CINEMATIC_INTRO;
                }
                break;
            case SCREEN_CINEMATIC_INTRO:
                UpdateCinematicIntro(&game);
                break;
            case SCREEN_INTRO:
                if (Clicked((Rectangle){490, 575, 300, 58}) ||
                    IsKeyPressed(KEY_ENTER))
                {
                    ResetDemo(&game);
                    game.screen = SCREEN_ROOM_2;
                }
                break;
            case SCREEN_ROOM:
                UpdateRoom(&game);
                break;
            case SCREEN_TERMINAL:
                UpdateTerminal(&game);
                break;
            case SCREEN_RESULT:
                if (Clicked((Rectangle){800, 466, 350, 48}) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    InitFinalDialogue(&game);
                    game.screen = SCREEN_FINAL_DIALOGUE;
                }
                else if (Clicked((Rectangle){120, 588, 250, 56}))
                {
                    game.journalYearTab = 2;
                    game.screen = SCREEN_ROOM_2026;
                    SetMessage(&game, "Retornando a Fabrica de Materiais // 2026.");
                }
                else if (Clicked((Rectangle){390, 588, 230, 56}) || IsKeyPressed(KEY_BACKSPACE))
                {
                    game.journalYearTab = 1;
                    game.screen = SCREEN_ROOM_2008;
                    SetMessage(&game, "Retornando ao Escritorio // 2008.");
                }
                else if (Clicked((Rectangle){640, 588, 260, 56}))
                {
                    game.foundCalendar = false;
                    game.foundBooks = false;
                    game.foundDrawer = false;
                    game.foundBlueprint = false;
                    game.foundTape = false;
                    game.codeLength = 0;
                    game.code[0] = '\0';
                    game.screen = SCREEN_ROOM;
                }
                else if (Clicked((Rectangle){920, 588, 220, 56}))
                {
                    game.screen = SCREEN_TITLE;
                }
                break;
            case SCREEN_ROOM_2:
                UpdateRoom2(&game);
                break;
            case SCREEN_MEMORY_1990:
                UpdateMemory1990(&game);
                break;
            case SCREEN_PUZZLE_FILES:
                UpdatePuzzleFiles(&game);
                break;
            case SCREEN_RESULT_2:
                UpdateResult2(&game);
                break;
            case SCREEN_ROOM_2008:
                UpdateRoom2008(&game);
                break;
            case SCREEN_PUZZLE_2008:
                UpdatePuzzle2008(&game);
                break;
            case SCREEN_RESULT_2008:
                UpdateResult2008(&game);
                break;
            case SCREEN_ROOM_2026:
                UpdateRoom2026(&game);
                break;
            case SCREEN_RESULT_2026:
                UpdateResult2026(&game);
                break;
            case SCREEN_FINAL_DIALOGUE:
                UpdateFinalDialogue(&game);
                break;
            case SCREEN_FINAL_ENDING:
                UpdateFinalEnding(&game);
                break;
            case SCREEN_FAILURE:
                if (Clicked((Rectangle){490, 460, 300, 60}) || IsKeyPressed(KEY_ENTER))
                {
                    ResetDemo(&game);
                    game.screen = SCREEN_ROOM_2;
                }
                else if (Clicked((Rectangle){520, 550, 240, 48}) || IsKeyPressed(KEY_ESCAPE))
                    game.screen = SCREEN_TITLE;
                break;
        }

        /* 1. Renderiza o jogo na textura virtual (1280x720) */
        BeginTextureMode(target);
        ClearBackground(VOID_COLOR);
        switch (game.screen)
        {
            case SCREEN_TITLE: DrawTitle(); break;
            case SCREEN_SETTINGS: DrawSettings(&game); break;
            case SCREEN_PLAYER_NAME: DrawPlayerName(&game); break;
            case SCREEN_CINEMATIC_INTRO: DrawCinematicIntro(&game); break;
            case SCREEN_INTRO: DrawIntro(&game); break;
            case SCREEN_ROOM:
                DrawRoom(&game);
                DrawHud(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_TERMINAL: DrawTerminal(&game); break;
            case SCREEN_RESULT: DrawResult(&game); break;
            case SCREEN_ROOM_2:
                DrawRoom2(&game);
                DrawHudRoom2(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_MEMORY_1990:
                DrawRoom2(&game);
                DrawMemory1990(&game);
                break;
            case SCREEN_PUZZLE_FILES:
                DrawRoom2(&game);
                DrawPuzzleFiles(&game);
                break;
            case SCREEN_RESULT_2:
                DrawResult2(&game);
                break;
            case SCREEN_ROOM_2008:
                DrawRoom2008(&game);
                DrawHudRoom2008(&game);
                if (game.activeDocument2008 >= 0) DrawDocumentViewer2008(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_PUZZLE_2008:
                DrawPuzzle2008(&game);
                if (game.activeDocument2008 >= 0) DrawDocumentViewer2008(&game);
                break;
            case SCREEN_RESULT_2008:
                DrawResult2008(&game);
                break;
            case SCREEN_ROOM_2026:
                DrawRoom2026(&game);
                DrawHudRoom2026(&game);
                if (game.journalOpen) DrawJournal(&game);
                break;
            case SCREEN_RESULT_2026:
                DrawResult2026(&game);
                break;
            case SCREEN_FINAL_DIALOGUE:
                DrawFinalDialogue(&game);
                break;
            case SCREEN_FINAL_ENDING:
                DrawFinalEnding(&game);
                break;
            case SCREEN_FAILURE: DrawFailure(); break;
        }
        EndTextureMode();

        /* 2. Desenha a textura escalonada para a janela atual / tela cheia */
        BeginDrawing();
        ClearBackground(BLACK);
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        float scale = fminf((float)screenW / SCREEN_WIDTH, (float)screenH / SCREEN_HEIGHT);
        Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, -(float)target.texture.height };
        Rectangle destRec = {
            (screenW - (SCREEN_WIDTH * scale)) * 0.5f,
            (screenH - (SCREEN_HEIGHT * scale)) * 0.5f,
            SCREEN_WIDTH * scale,
            SCREEN_HEIGHT * scale
        };
        DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    UnloadGameFonts();
    if (g_eliraPortraitLoaded)
    {
        UnloadTexture(g_eliraPortrait);
    }
    if (bgmLoaded)
    {
        UnloadMusicStream(ambientBgm);
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
