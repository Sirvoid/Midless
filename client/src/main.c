/**
 * Copyright (c) 2021 Sirvoid
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
#include "textures.h"
#include "screens.h"
#include "block.h"
#include "networkhandler.h"
#include "chat.h"
#include "localserver.h"
#include "runtimepaths.h"
#include "platform.h"


void Game_RunLoop(void);

int main(void) {
    if (!RuntimePaths_Init()) return 1;
    Platform_BeginTiming();

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
    UnloadImage(midlessLogo);

    EntityModelDefinitions_Init();
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
    Screen_Init(texture, &exitProgram);
    ClientTextures_Init(texture);


    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(Game_RunLoop, 0, 1);
    #else
        while (!WindowShouldClose() && !exitProgram) {
            Game_RunLoop();
        }
        
        networkThreadState = -1;

        LocalServer_Stop();
        EntityModel_ResetDefinitions();
        ClientTextures_Reset();
        Screen_Shutdown();
        UnloadShader(shader);
        UnloadTexture(texture);
        World_Shutdown();
        EntityModelDefinitions_Shutdown();
        Chat_Shutdown();

        CloseWindow();
        Platform_EndTiming();
    #endif

    return 0;
}

void Game_RunLoop(void) {
    Network_ProcessIncomingPackets();
    
    // Update
    Player_Update();
    World_Update();
    
    Vector3 selectionBoxPos = (Vector3) { floor(player.rayResult.hitPos.x), floor(player.rayResult.hitPos.y), floor(player.rayResult.hitPos.z)};
    
    // Draw
    BeginDrawing();

        float sunlightStrength = World_GetSunlightStrength();
        ClearBackground((Color) { 140 * sunlightStrength, 210 * sunlightStrength, 240 * sunlightStrength, 255});

        BeginMode3D(player.camera);
            World_Draw(player.camera.position);
            if (player.cameraMode == PLAYER_CAMERA_FIRST_PERSON) Player_Draw();
            if (player.rayResult.hitblockId != -1) {
                const Block *block = Block_GetDefinition(World_GetBlock(selectionBoxPos));
                for(int box=0;box<Block_BoxCount(block,true);box++) {
                    BoundingBox bounds=Block_GetBox(block,box,selectionBoxPos,true);
                    Vector3 size=Vector3Subtract(bounds.max,bounds.min);
                    Vector3 center=Vector3Scale(Vector3Add(bounds.min,bounds.max),0.5f);
                    DrawCube(center,size.x+0.02f,size.y+0.02f,size.z+0.02f,(Color){255,255,255,40});
                }
            }
                
        EndMode3D();

        Color liquidTint;
        if (Player_GetCameraLiquidTint(&liquidTint)) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), liquidTint);
        }

        Screen_Draw();

    EndDrawing();
}
