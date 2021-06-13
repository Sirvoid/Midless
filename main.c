#include "raylib.h"
#include "player.h"
#include "chunk.h"
#include "world.h"

int main(void)
{
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Game");
    
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