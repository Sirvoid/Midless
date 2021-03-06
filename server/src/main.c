/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "raylib.h"
#include "server.h"
#include "world.h"

int main(void) {
    
    InitWindow(320, 320, "Server");
    SetTargetFPS(60);
    
    World_Init();
    
    int serverThread_state = 0;
    pthread_t serverThread_id;
    pthread_create(&serverThread_id, NULL, Server_Init, (void*)&serverThread_state);
    
    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Server Running", 16, 16, 20, WHITE);
            DrawText("Players:", 16, 48, 12, WHITE);
            for(int i = 0; i < 256; i++) {
                if(world.players[i]) {
                    DrawText(TextFormat("%s (ping: %2i ms)", world.players[i]->name, 0), 16, 64 + (i * 16), 12, WHITE);
                }
            }
        EndDrawing();
    }

    serverThread_state = -1;
    CloseWindow();
    
    return 0;
}