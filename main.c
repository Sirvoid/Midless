#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "raylib.h"
#include "rlgl.h"
#include "player.h"
#include "world.h"
#include "screens.h"
#include "client.h"
#include "networkhandler.h"
#include "packet.h"

#define GLSL_VERSION 330

int main(void) {
    // Initialization
    int screenWidth = 800;
    int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Game");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetExitKey(0);
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(60);
    
    Block_BuildDefinition();
    
    // World Initialization
    World_Init();
    
    char *chunkShaderVs = 
        #include "chunk_shader.vs"
    ;
    
    char *chunkShaderFs = 
        #include "chunk_shader.fs"
    ;
    
    Shader chunkShader = LoadShaderFromMemory(chunkShaderVs, chunkShaderFs);
    
    Image terrainTex = LoadImage("textures/terrain.png"); 
    Texture2D texture = LoadTextureFromImage(terrainTex);
    UnloadImage(terrainTex);
    World_ApplyTexture(texture);
    World_ApplyShader(chunkShader);

    //Player Initialization
    Player_Init();
    
    bool exitProgram = false;
    Screens_init(texture, &exitProgram);
    
    // Game loop
    while (!WindowShouldClose() && !exitProgram) {
        
        Network_ReadQueue();
      
        // Update
        Player_Update();
        World_LoadChunks();
        
        Vector3 selectionBoxPos = (Vector3) { (int)player.rayResult.hitPos.x + 0.5f, (int)player.rayResult.hitPos.y + 0.5f, (int)player.rayResult.hitPos.z + 0.5f};
        
        // Draw
        BeginDrawing();

            ClearBackground((Color) { 140, 210, 240});
            
            BeginMode3D(player.camera);
                World_Draw(player.camera.position);
                if(player.rayResult.hitBlockID != -1) 
                    DrawCube(selectionBoxPos, 1.01f, 1.01f, 1.01f, (Color){255, 255, 255, 40});
            EndMode3D();

            Screen_Make();
        
        EndDrawing();
    }
    
    Network_threadState = -1;
    
    UnloadShader(chunkShader);
    UnloadTexture(texture);
    World_Unload();

    CloseWindow();

    return 0;
}