/**
 * Copyright (c) 2022 Sirvoid
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
#include "world/worldgen.h"
#include "stb_ds.h"
#include "networkhandler.h"
#include "packet.h"
#include "scripthooks.h"
#include "logger.h"
#include "utils.h"
#include "runtimepaths.h"
#include "platform.h"
#include "servertiming.h"
#include "serverconfig.h"

int main(int argc, char **argv) {
    bool acceptGeneratorChange = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--accept-generator-change")) {
            acceptGeneratorChange = true;
        } else {
            fprintf(stderr, "Usage: %s [--accept-generator-change]\n", argv[0]);
            return 1;
        }
    }
    if (!RuntimePaths_Init()) return 1;
    if (!ServerConfig_Load("server.cfg")) return 1;
    Platform_BeginTiming();

    #if !defined(SERVER_HEADLESS)
        InitWindow(400, 400, "Server");
        SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
        SetTargetFPS(0);
    #endif

    SetTraceLogLevel(LOG_WARNING);

    ServerLogger_Log("Started Server.");

    ScriptRuntime_Init();
    ScriptHooks_Init();

    if (!ServerWorld_Init()) {
        ScriptHooks_Shutdown();
        ScriptRuntime_Stop();
        Platform_EndTiming();
        return 1;
    }
    serverWorld.maxPlayers = serverConfig.maxPlayers;
    serverWorld.maxDrawDistance = serverConfig.maxRenderDistance;
    ServerNetwork_Init();
    if (!ScriptRuntime_Run(acceptGeneratorChange)) {
        SavedGenerator previous, current;
        if (Worldgen_GetChange(&previous, &current)) {
            fprintf(stderr, "To approve this generator update, restart with --accept-generator-change.\n"
                            "Saved terrain stays intact; new terrain may have seams.\n"
                            "Otherwise restore the world's previous generator and mods.\n");
        }
        ServerNetwork_Shutdown();
        ServerWorld_Shutdown();
        ScriptHooks_Shutdown();
        ScriptRuntime_Stop();
        Platform_EndTiming();
        return 1;
    }

    int serverThreadState = 0;
    pthread_t serverThreadId;
    pthread_create(&serverThreadId, NULL, Server_Init, (void*)&serverThreadState);
    
    #if defined(SERVER_WEB_SUPPORT)
    ServerWss_Init();
    #endif

    ScriptHooks_Ready();
    #if !defined(SERVER_HEADLESS)
    double nextDrawTime = 0;
    #endif
    
    #if !defined(SERVER_HEADLESS)
    while (!WindowShouldClose()) {
    #else
    while(true) {
    #endif

        #if defined(SERVER_WEB_SUPPORT)
        ServerWss_Poll();
        #endif

        ServerNetwork_ProcessIncomingPackets();


        ServerWorld_Update();

        #if !defined(SERVER_HEADLESS)
        double now = GetTime();
        if (now >= nextDrawTime) {
            nextDrawTime = now + 1.0 / 60.0;
            BeginDrawing();
                ClearBackground(BLACK);
                DrawText("Server Running", 16, 16, 20, WHITE);
                DrawText(TextFormat("Chunks: %i", hmlen(serverWorld.chunks)), 200, 48, 12, WHITE);
                DrawText("Players:", 16, 48, 12, WHITE);
                for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
                    if (serverWorld.players[i]) {
                        DrawText(TextFormat("%s (ping: %2i ms)", serverWorld.players[i]->name, 0), 16, 64 + (i * 16), 12, WHITE);
                    }
                }
            EndDrawing();
        }
        #endif
        WaitTime(SERVER_SERVICE_WAIT_SECONDS);
    }

    serverThreadState = -1;
    pthread_join(serverThreadId, NULL);

    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();

    ScriptHooks_Shutdown();
    ScriptRuntime_Stop();

    #if !defined(SERVER_HEADLESS)
    CloseWindow();
    #endif
    
    Platform_EndTiming();
    return 0;
}
