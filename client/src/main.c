/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "player.h"
#include "world.h"
#include "resource.h"
#include "screens.h"
#include "block.h"
#include "networkhandler.h"
#include "chat.h"


void GameLoop(void);

int main(void) {

    int screenWidth = 1280;
    int screenHeight = 720;

    // Initialization
    InitWindow(screenWidth, screenHeight, "Midless");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
    SetExitKey(0);
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(60); 

    #if defined(PLATFORM_WEB)
        char *chunkShaderVs = 
            #include "chunk/shaders/chunk_shader_gl100.vs"
        ;
        char *chunkShaderFs = 
            #include "chunk/shaders/chunk_shader_gl100.fs"
        ;
    #else
        char *chunkShaderVs = 
            #include "chunk/shaders/chunk_shader.vs"
        ;
        char *chunkShaderFs = 
            #include "chunk/shaders/chunk_shader.fs"
        ;
    #endif

    Image midlessLogo = Resource_LoadImage("midless.png"); 


    SetWindowIcon(midlessLogo);

    EntityModel_DefineAll();
    Block_BuildDefinition();

    // World Initialization
    World_Init();

    
    Shader shader = LoadShaderFromMemory(chunkShaderVs, chunkShaderFs);
    Texture2D texture = Resource_LoadTexture("terrain.png"); 
    
    World_ApplyTexture(texture);
    World_ApplyShader(shader);

    //Player Initialization
    Player_Init();
    
    bool exitProgram = false;
    Screens_init(texture, &exitProgram);


    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(GameLoop, 0, 1);
    #else
        while (!WindowShouldClose() && !exitProgram) {
            GameLoop();
        }
        
        Network_threadState = -1;

        UnloadShader(shader);
        UnloadTexture(texture);
        World_Unload();

        CloseWindow();
    #endif

    return 0;
}

void GameLoop(void) {
    Network_ReadQueue();
    
    // Update
    Player_Update();
    World_Update();
    
    Vector3 selectionBoxPos = (Vector3) { floor(player.rayResult.hitPos.x) + 0.5f, floor(player.rayResult.hitPos.y), floor(player.rayResult.hitPos.z) + 0.5f};
    
    // Draw
    BeginDrawing();

        float sunlightStrength = World_GetSunlightStrength();
        ClearBackground((Color) { 140 * sunlightStrength, 210 * sunlightStrength, 240 * sunlightStrength, 255});

        BeginMode3D(player.camera);
            World_Draw(player.camera.position);
            if (player.rayResult.hitBlockID != -1) {
                Block block = Block_GetDefinition(player.rayResult.hitBlockID);
                Vector3 blockSize = Vector3Subtract(block.maxBB, block.minBB);
                blockSize = Vector3Scale(blockSize, 1.0f / 16);
                selectionBoxPos.y += blockSize.y / 2;
                DrawCube(selectionBoxPos, blockSize.x + 0.02f, blockSize.y + 0.02f, blockSize.z + 0.02f, (Color){255, 255, 255, 40});
            }
                
        EndMode3D();
        
        Screen_Make();

    EndDrawing();
}