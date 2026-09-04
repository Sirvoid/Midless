/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define __clang__ true
#if !defined(MIDLESS_STB_DS_EXTERNAL)
    #define STB_DS_IMPLEMENTATION
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "raylib.h"
#include "stb_ds.h"
#include "world.h"
#include "chunkmanager.h"
#include "playermanager.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "worldgenerator.h"
#include "../utils.h"

World serverWorld;
static long long lastUpdateMilliseconds;
static long long lastTimeSyncMilliseconds;

static void CreateWorldDirectory(void) {
    struct stat status = {0};
    if (stat("./world", &status) != -1) return;
#if defined(OS_LINUX) || defined(PLATFORM_WEB)
    mkdir("./world", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#else
    mkdir("./world");
#endif
}

static int LoadWorldSeed(void) {
    int seed = rand();
    if (FileExists("./world/seed.dat")) {
        unsigned int bytesRead = 0;
        unsigned char *data = LoadFileData("./world/seed.dat", &bytesRead);
        if (data != NULL && bytesRead >= 4) {
            seed = (int)(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
        }
        UnloadFileData(data);
    } else {
        char data[4] = {(char)(seed >> 24), (char)(seed >> 16), (char)(seed >> 8), (char)seed};
        SaveFileData("./world/seed.dat", data, 4);
    }
    return seed;
}

void ServerWorld_Init(void) {
    serverWorld = (World){0};
    serverWorld.players = MemAlloc(sizeof(Player *) * WORLD_MAX_PLAYERS);
    memset(serverWorld.players, 0, sizeof(Player *) * WORLD_MAX_PLAYERS);
    serverWorld.entities = MemAlloc(sizeof(Entity) * WORLD_MAX_ENTITIES);
    memset(serverWorld.entities, 0, sizeof(Entity) * WORLD_MAX_ENTITIES);
    serverWorld.maxDrawDistance = 8;

    lastUpdateMilliseconds = GetTimeMilliseconds();
    lastTimeSyncMilliseconds = lastUpdateMilliseconds;
    CreateWorldDirectory();
    ServerWorldGenerator_Init(LoadWorldSeed());
    ServerChunkManager_Init();
}

void ServerWorld_Shutdown(void) {
    ServerChunkManager_Shutdown();
    ServerPlayerManager_Shutdown();
    MemFree(serverWorld.players);
    MemFree(serverWorld.entities);
    serverWorld.players = NULL;
    serverWorld.entities = NULL;
}

void ServerWorld_Update(void) {
    ServerChunkManager_Update();

    long long nowMilliseconds = GetTimeMilliseconds();
    long long elapsedMilliseconds = nowMilliseconds - lastUpdateMilliseconds;
    lastUpdateMilliseconds = nowMilliseconds;
    if (elapsedMilliseconds > 0) {
        serverWorld.time += elapsedMilliseconds / 1000.0f;
        while (serverWorld.time >= WORLD_DAY_LENGTH_SECONDS) {
            serverWorld.time -= WORLD_DAY_LENGTH_SECONDS;
        }
    }

    if (nowMilliseconds - lastTimeSyncMilliseconds >= WORLD_TIME_SYNC_INTERVAL_MILLISECONDS) {
        ServerWorld_Broadcast(ServerPacket_CreateWorldTime(serverWorld.time));
        lastTimeSyncMilliseconds = nowMilliseconds;
    }

    ServerPlayerManager_Update();
}

void ServerWorld_SendMessage(const char *message) {
    int messageLength = TextLength(message);
    int parts = messageLength > 0 ? (messageLength + 63) / 64 : 1;
    for (int i = 0; i < parts; i++) {
        const char *messageChunk = TextSubtext(message, i * 64, 64);
        ServerWorld_Broadcast(i == 0
            ? ServerPacket_CreateMessage(messageChunk)
            : ServerPacket_CreateMessageContinuation(messageChunk));
    }
}

void ServerWorld_Broadcast(unsigned char *packet) {
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] == NULL) continue;
        int packetLength = ServerPacket_GetLength(packet[0]);
        unsigned char *copy = MemAlloc(packetLength);
        memcpy(copy, packet, packetLength);
        ServerNetwork_Send(serverWorld.players[i], copy);
    }
    MemFree(packet);
}

void ServerWorld_BroadcastExcluding(unsigned char *packet, int excludedPlayerId) {
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player == NULL || player->id == excludedPlayerId) continue;
        int packetLength = ServerPacket_GetLength(packet[0]);
        unsigned char *copy = MemAlloc(packetLength);
        memcpy(copy, packet, packetLength);
        ServerNetwork_Send(player, copy);
    }
    MemFree(packet);
}
