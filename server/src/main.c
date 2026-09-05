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
#include "world/world.h"
#include "stb_ds.h"
#include "networkhandler.h"
#include "packet.h"
#include "luaengine.h"
#include "luabindings.h"
#include "logger.h"
#include "utils.h"

int main(void) {

    #if !defined(SERVER_HEADLESS)
        InitWindow(400, 400, "Server");
        SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
        SetTargetFPS(60);
    #endif

    SetTraceLogLevel(LOG_WARNING);

    ServerLogger_Log("Started Server.");

    Lua_Init();
    LuaBindings_Init();

    ServerWorld_Init();
    ServerNetwork_Init();
    Lua_Run();

    int serverThreadState = 0;
    pthread_t serverThreadId;
    pthread_create(&serverThreadId, NULL, Server_Init, (void*)&serverThreadState);
    
    #if defined(SERVER_WEB_SUPPORT)
    ServerWss_Init();
    #endif

    LuaBindings_InvokeReady();
    
    #if !defined(SERVER_HEADLESS)
    while (!WindowShouldClose()) {
    #else
    while(true) {
        usleep(16 * 1000);
    #endif

        #if defined(SERVER_WEB_SUPPORT)
        ServerWss_Poll();
        #endif

        ServerNetwork_ProcessIncomingPackets();


        ServerWorld_Update();

        #if !defined(SERVER_HEADLESS)
        BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Server Running", 16, 16, 20, WHITE);
            DrawText(TextFormat("Chunks: %i", hmlen(serverWorld.chunks)), 200, 48, 12, WHITE);
            DrawText("Players:", 16, 48, 12, WHITE);
            for (int i = 0; i < 256; i++) {
                if (serverWorld.players[i]) {
                    DrawText(TextFormat("%s (ping: %2i ms)", serverWorld.players[i]->name, 0), 16, 64 + (i * 16), 12, WHITE);
                }
            }
        EndDrawing();
        #endif
    }

    serverThreadState = -1;
    pthread_join(serverThreadId, NULL);

    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();

    LuaBindings_Shutdown();
    Lua_Stop();

    #if !defined(SERVER_HEADLESS)
    CloseWindow();
    #endif
    
    return 0;
}
