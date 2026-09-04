#if defined(OS_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
#endif

#include <pthread.h>
#include <unistd.h>
#include "localserver.h"
#include "../../server/src/world/world.h"
#include "../../server/src/player.h"
#include "../../server/src/networkhandler.h"
#include "../../server/src/scripting/luaengine.h"
#include "../../server/src/scripting/luabindings.h"

extern int networkConnectedToServer;
extern void (*networkClientSend)(unsigned char *, int);
void Network_Init(void);
void Network_Connect(void);
void Network_Receive(unsigned char *data, int dataLength);
void Network_ClearQueue(void);
void World_Clear(void);

static Player *localPlayer;
static bool localServerRunning;
static bool localServerThreadCreated;
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
    while (LocalServer_IsRunning()) {
        ServerNetwork_ProcessIncomingPackets();
        ServerWorld_Update();
        usleep(1000);
    }
    return NULL;
}

void Server_Send(void *peer, unsigned char *packet, int length) {
    (void)peer;
    Network_Receive(packet, length);
}

static void LocalServer_Send(unsigned char *packet, int length) {
    ServerNetwork_Receive(localPlayer, packet, length);
}

bool LocalServer_Start(void) {
    if (LocalServer_IsRunning()) return true;

    Lua_Init();
    LuaBindings_Init();
    Lua_Run();
    ServerWorld_Init();
    ServerNetwork_Init();
    localPlayer = ServerPlayer_Create(NULL, false);
    if (localPlayer == NULL) {
        ServerWorld_Shutdown();
        LuaBindings_Shutdown();
        Lua_Stop();
        return false;
    }
    localPlayer->peer = localPlayer;

    networkClientSend = LocalServer_Send;
    networkConnectedToServer = true;
    Network_Init();
    LocalServer_SetRunning(true);
    if (pthread_create(&localServerThread, NULL, LocalServer_Run, NULL) != 0) {
        LocalServer_SetRunning(false);
        networkConnectedToServer = false;
        ServerNetwork_Shutdown();
        ServerWorld_Shutdown();
        LuaBindings_Shutdown();
        Lua_Stop();
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
    if (localServerThreadCreated) {
        pthread_join(localServerThread, NULL);
        localServerThreadCreated = false;
    }
    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();
    LuaBindings_Shutdown();
    Lua_Stop();
    localPlayer = NULL;
    World_Clear();
    Network_ClearQueue();
    networkConnectedToServer = false;
}
