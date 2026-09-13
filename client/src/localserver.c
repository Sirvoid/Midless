/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(OS_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
#endif

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#if defined(PLATFORM_WEB)
#include <emscripten/threading.h>
#endif
#include "localserver.h"
#include "../../server/src/savedatabase.h"
#include "../../server/src/world/world.h"
#include "../../server/src/world/worldgen.h"
#include "gui/screens.h"
#include "../../server/src/player.h"
#include "../../server/src/networkhandler.h"
#include "../../server/src/scripthooks.h"
#include "../../server/src/servertiming.h"

extern int networkConnectedToServer;
extern void (*networkClientSend)(unsigned char *, int);
void Network_Init(void);
void Network_Connect(void);
void Network_Receive(unsigned char *data, int dataLength);
void Network_ClearQueue(void);
void World_Clear(void);
void World_ClearChunks(void);
bool World_CleanupChunks(void);
int World_RemainingCleanupChunks(void);

static Player *localPlayer;
static bool generatorChanged;
static SavedGenerator previousGenerator;
static SavedGenerator currentGenerator;
static bool localServerRunning;
static bool localServerThreadCreated;
static bool localServerFinished;
static pthread_t localServerThread;
static pthread_mutex_t localServerStateMutex = PTHREAD_MUTEX_INITIALIZER;

static void LocalServer_SetRunning(bool running) {
    pthread_mutex_lock(&localServerStateMutex);
    localServerRunning = running;
    pthread_mutex_unlock(&localServerStateMutex);
}

bool LocalServer_IsRunning(void) {
    pthread_mutex_lock(&localServerStateMutex);
    bool running = localServerRunning;
    pthread_mutex_unlock(&localServerStateMutex);
    return running;
}

static void *LocalServer_Run(void *unused) {
    (void)unused;

    ScriptHooks_Ready();

    while (LocalServer_IsRunning()) {
        ServerNetwork_ProcessIncomingPackets();
        ServerWorld_Update();

        WaitTime(SERVER_SERVICE_WAIT_SECONDS);
    }

    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();
    ScriptHooks_Shutdown();
    ScriptRuntime_Stop();
    pthread_mutex_lock(&localServerStateMutex);
    localServerFinished = true;
    pthread_mutex_unlock(&localServerStateMutex);
    return NULL;
}

void Server_Send(void *peer, unsigned char *packet, int length) {
    (void)peer;
    Network_Receive(packet, length);
}

static void LocalServer_Send(unsigned char *packet, int length) {
    ServerNetwork_Receive(localPlayer, packet, length);
}

bool LocalServer_GetGeneratorChange(SavedGenerator *previous, SavedGenerator *current) {
    if (!generatorChanged) return false;
    *previous = previousGenerator;
    *current = currentGenerator;
    return true;
}

bool LocalServer_Start(bool acceptGeneratorChange) {
    if (LocalServer_IsRunning()) return true;
    generatorChanged = false;

    ScriptRuntime_Init();
    ScriptHooks_Init();
    if (!ServerWorld_Init()) {
        ScriptHooks_Shutdown();
        ScriptRuntime_Stop();
        return false;
    }
    ServerNetwork_Init();
    if (!ScriptRuntime_Run(acceptGeneratorChange)) {
        generatorChanged = Worldgen_GetChange(&previousGenerator, &currentGenerator);
        ServerNetwork_Shutdown();
        ServerWorld_Shutdown();
        ScriptHooks_Shutdown();
        ScriptRuntime_Stop();
        return false;
    }
    localPlayer = ServerPlayer_Create(NULL, false);
    if (localPlayer == NULL) {
        ServerWorld_Shutdown();
        ScriptHooks_Shutdown();
        ScriptRuntime_Stop();
        return false;
    }
    localPlayer->peer = localPlayer;

    networkClientSend = LocalServer_Send;
    networkConnectedToServer = true;
    Network_Init();
    localServerFinished = false;
    LocalServer_SetRunning(true);
    if (pthread_create(&localServerThread, NULL, LocalServer_Run, NULL) != 0) {
        LocalServer_SetRunning(false);
        networkConnectedToServer = false;
        ServerNetwork_Shutdown();
        ServerWorld_Shutdown();
        ScriptHooks_Shutdown();
        ScriptRuntime_Stop();
        localPlayer = NULL;
        return false;
    }
    localServerThreadCreated = true;
    Network_Connect();
    return true;
}

void LocalServer_Stop(void) {
    if (!LocalServer_IsRunning()) return;
    LocalServer_SetRunning(false);
    World_ClearChunks();
    if (localServerThreadCreated) {
        double started = GetTime();
        double serverSeconds = -1, cleanupSeconds = 0;
        int chunks = World_RemainingCleanupChunks();
        for (;;) {
#if defined(PLATFORM_WEB)
            emscripten_current_thread_process_queued_calls();
#endif
            pthread_mutex_lock(&localServerStateMutex);
            bool finished = localServerFinished;
            pthread_mutex_unlock(&localServerStateMutex);
            if (finished && serverSeconds < 0) serverSeconds = GetTime() - started;
            double cleanupStarted = GetTime();
            bool cleaned = World_CleanupChunks();
            cleanupSeconds += GetTime() - cleanupStarted;
            if (finished && cleaned) break;
            Screen_DrawSavingWorld(finished, World_RemainingCleanupChunks());
        }
        fprintf(stderr, "Shutdown: server %.2f s, client cleanup %.2f s of work for %d chunks, total %.2f s\n",
                 serverSeconds, cleanupSeconds, chunks, GetTime() - started);
        pthread_join(localServerThread, NULL);
        localServerThreadCreated = false;
    }
    SaveDatabase_SyncBrowser(true);
    localPlayer = NULL;
    World_Clear();
    Network_ClearQueue();
    networkConnectedToServer = false;
}
