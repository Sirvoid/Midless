#include "raylib.h"
#include "player.h"
#include "chunk.h"
#include "world.h"
#include "block.h"
#include <string.h>

int main(void)
{
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Game");
    SetTraceLogLevel(LOG_WARNING);
    
    Block_Define(1, "stone", 1, 1, 1);
    Block_Define(2, "dirt", 2, 2, 2);
    Block_Define(3, "grass", 0, 2, 3);
    Block_Define(4, "wood", 4, 4, 4);
    
    Player player;
    Player_Init(&player);
    
    World_Init();

    Image terrainTex = LoadImage("textures/terrain.png"); 
    Texture2D texture = LoadTextureFromImage(terrainTex);
    UnloadImage(terrainTex);
    World_ApplyTexture(texture);

    SetTargetFPS(60);
    
    //Game loop
    while (!WindowShouldClose())
    {
        
        // Update
        Player_Update(&player);
        
        // Draw
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawFPS(16,16);
            BeginMode3D(player.camera);
                
                World_Draw();

            EndMode3D();

        EndDrawing();
    }
    
    UnloadTexture(texture);
    World_Unload();

    CloseWindow();

    return 0;
}