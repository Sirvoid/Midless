#include "raylib.h"
#include "player.h"
#include "world.h"
#include <string.h>

#define GLSL_VERSION 330

int main(void) {
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;
    Color uiColBg = (Color){ 0, 0, 0, 80 };

    InitWindow(screenWidth, screenHeight, "Game");
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(60);
    
    Block_Define(1, "stone", 1, 1, 1);
    Block_Define(2, "dirt", 2, 2, 2);
    Block_Define(3, "grass", 0, 2, 3);
    Block_Define(4, "wood", 4, 4, 4);
    
    Player player;
    Player_Init(&player);
    
    // World Initialization
    World_Init();
    
    char *chunkShaderVs = 
        #include "chunk_shader.vs"
    ;
    
    char *chunkShaderFs = 
        #include "chunk_shader.fs"
    ;
    
    Shader shader = LoadShaderFromMemory(chunkShaderVs, chunkShaderFs);
    
    Image terrainTex = LoadImage("textures/terrain.png"); 
    Texture2D texture = LoadTextureFromImage(terrainTex);
    UnloadImage(terrainTex);
    
    World_ApplyTexture(texture);
    World_ApplyShader(shader);

    // Game loop
    while (!WindowShouldClose()) {
        
        // Update
        Player_Update(&player);
        
        // Draw
        BeginDrawing();

            ClearBackground((Color) { 140, 210, 240});

            BeginMode3D(player.camera);
                World_Draw(player.camera.position);
            EndMode3D();
            
            
            const char* coordText = TextFormat("X: %i Y: %i Z: %i", (int)player.position.x, (int)player.position.y, (int)player.position.z);
            
            DrawRectangle(13, 15, MeasureText(coordText, 20) + 6, 39, uiColBg);
            DrawText(TextFormat("%2i FPS", GetFPS()), 16, 16, 20, WHITE);
            DrawText(coordText, 16, 36, 20, WHITE);
            
            DrawRectangle(screenWidth / 2 - 8, screenHeight / 2 - 2, 16, 4, uiColBg);
            DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 + 2, 4, 6, uiColBg);
            DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 - 8, 4, 6, uiColBg);

        EndDrawing();
    }
    
    UnloadShader(shader);
    UnloadTexture(texture);
    World_Unload();

    CloseWindow();

    return 0;
}