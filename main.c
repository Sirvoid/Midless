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
    
    World world;
    World_Init(&world);

    Image terrainTex = LoadImage("textures/terrain.png"); 
    Texture2D texture = LoadTextureFromImage(terrainTex);
    UnloadImage(terrainTex);
    World_ApplyTexture(&world, texture);

    SetTargetFPS(60);    
    
    //Game loop
    while (!WindowShouldClose())
    {
        // Update
        UpdateCamera(&player.camera);                  
        
        // Draw
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(player.camera);
                
                World_Draw(&world);

            EndMode3D();

        EndDrawing();
    }
    
    UnloadTexture(texture);
    World_Unload(&world);

    CloseWindow();

    return 0;
}