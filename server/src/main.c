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
#include <sys/time.h>
#include <unistd.h>
#include "raylib.h"
#include "server.h"
#include "serverwss.h"
#include "world.h"
#include "stb_ds.h"
#include "networkhandler.h"
#include "luaengine.h"
#include "luadefinition.h"
#include "logger.h"
#include "utils.h"

int main(void) {

    #if !defined(SERVER_HEADLESS)
        InitWindow(400, 400, "Server");
        SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
        SetTargetFPS(60);
    #endif

    SetTraceLogLevel(LOG_WARNING);

    Logger_Log("Started Server.");

    Lua_Init();
    LuaDefinition_Init();
    Lua_Run();

    World_Init();
    Network_Init();

    int serverThread_state = 0;
    pthread_t serverThread_id;
    pthread_create(&serverThread_id, NULL, Server_Init, (void*)&serverThread_state);
    
    #if defined(SERVER_WEB_SUPPORT)
    ServerWSS_Init();
    #endif
    
    #if !defined(SERVER_HEADLESS)
    while (!WindowShouldClose()) {
    #else
    while(true) {
        usleep(16 * 1000);
    #endif

        #if defined(SERVER_WEB_SUPPORT)
        ServerWSS_Poll();
        #endif

        Network_ReadIncomingPackets();

        World_Update();

        #if !defined(SERVER_HEADLESS)
        BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Server Running", 16, 16, 20, WHITE);
            DrawText(TextFormat("Chunks: %i", hmlen(world.chunks)), 200, 48, 12, WHITE);
            DrawText("Players:", 16, 48, 12, WHITE);
            for (int i = 0; i < 256; i++) {
                if (world.players[i]) {
                    DrawText(TextFormat("%s (ping: %2i ms)", world.players[i]->name, 0), 16, 64 + (i * 16), 12, WHITE);
                }
            }
        EndDrawing();
        #endif
    }

    serverThread_state = -1;

    World_Unload();

    Lua_Stop();

    #if !defined(SERVER_HEADLESS)
    CloseWindow();
    #endif
    
    return 0;
}