/**
 * Copyright (c) 2026 Sirvoid
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
#include "../../server/src/savedatabase.h"
#include "runtimepaths.h"
#include "platform.h"


void Game_RunLoop(void);

static void DrawInsideBlock(void) {
    Vector3 eye = player.camera.position;
    Vector3 cell = {floorf(eye.x), floorf(eye.y), floorf(eye.z)};
    int id = World_GetBlock(cell);
    if (id <= 0) return;
    const Block *block = Block_GetDefinition(id);
    if (block->colliderType != BLOCK_COLLIDER_SOLID) return;
    bool inside = false;
    for (int i = 0; i < Block_BoxCount(block, false); i++) {
        BoundingBox bounds = Block_GetBox(block, i, cell, false);
        if (eye.x > bounds.min.x && eye.x < bounds.max.x &&
            eye.y > bounds.min.y && eye.y < bounds.max.y &&
            eye.z > bounds.min.z && eye.z < bounds.max.z) { inside = true; break; }
    }
    if (!inside) return;
    Vector3 view = Vector3Subtract(player.camera.target, eye);
    BlockFace face;
    if (fabsf(view.y) > fmaxf(fabsf(view.x), fabsf(view.z)))
        face = view.y > 0 ? BLOCK_FACE_TOP : BLOCK_FACE_BOTTOM;
    else if (fabsf(view.x) > fabsf(view.z))
        face = view.x > 0 ? BLOCK_FACE_RIGHT : BLOCK_FACE_LEFT;
    else face = view.z > 0 ? BLOCK_FACE_FRONT : BLOCK_FACE_BACK;
    int tile = block->textures[face];
    Texture2D terrain = ClientTextures_Get(1);
    float width = terrain.width / 16.0f, height = terrain.height / 16.0f;
    Rectangle source = {(tile % 16) * width, (tile / 16) * height, width, height};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), BLACK);
    if (terrain.id) DrawTexturePro(terrain, source,
        (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, (Vector2){0}, 0,
        (Color){150, 150, 150, 255});
}

int main(void) {
    if (!RuntimePaths_Init()) return 1;
    Platform_BeginTiming();

    int screenWidth = 1280;
    int screenHeight = 720;

    // Initialization
    InitWindow(screenWidth, screenHeight, "Midless");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(640, 560);
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
    SaveDatabase_SyncBrowser(false);
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

        DrawInsideBlock();
        Screen_Draw();

    EndDrawing();
}
