#include "version.h"
#include "../digging.h"
/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include "stb_ds.h"
#include "raylib.h"
#include "networkhandler.h"
#include "../items.h"
#include "packet.h"
#include "screens.h"
#include "world.h"
#include "localserver.h"
#include "block.h"
#include "entitymodel.h"
#include "../textures.h"
#include "textureprotocol.h"
#include "inventoryprotocol.h"
#include "hudbarprotocol.h"
#include "../gui/hudbars.h"
#include "inventoryclient.h"
#include "formattedtext.h"

PacketHandlerEntry packets[256];
int networkConnectedToServer = 0;
void (*networkClientSend)(unsigned char*, int);
void (*networkClientDisconnect)(void);

typedef struct IncomingPacket {
    unsigned char *data;
    int length;
    unsigned int terrainGeneration;
} IncomingPacket;
static IncomingPacket *queuedData, *terrainQueuedData;
static int queuedTextureBytes;
static unsigned int terrainGeneration;
static bool resetDefinitionsPending, disconnectPending, acceptingIncoming;
static pthread_mutex_t networkQueueMutex = PTHREAD_MUTEX_INITIALIZER;
int packetCount;

int networkPing = 0;
int networkThreadState = 0;
char *networkName = "Player";
char *networkIp = "127.0.0.1";
char *networkFullAddress = "127.0.0.1:25565";
int networkPort = 25565;

void Network_Init(void) {
    pthread_mutex_lock(&networkQueueMutex);
    acceptingIncoming = true;
    pthread_mutex_unlock(&networkQueueMutex);
    packetCount = 0;
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMapInit, MAP_INIT_PACKET_SIZE}; //0
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleLoadChunk, PACKET_VARIABLE_SIZE}; //1
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSetBlock, SET_BLOCK_PACKET_SIZE}; //2
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSpawnEntity, SPAWN_ENTITY_PACKET_SIZE}; //3
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTeleportEntity, TELEPORT_ENTITY_PACKET_SIZE}; //4
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMessage, MESSAGE_PACKET_SIZE}; //5
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDespawnEntity, DESPAWN_ENTITY_PACKET_SIZE}; //6
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleUnloadChunk, UNLOAD_CHUNK_PACKET_SIZE}; //7
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleBlockBatch, PACKET_VARIABLE_SIZE}; //8
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleWorldTime, WORLD_TIME_PACKET_SIZE}; //9
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMessageContinuation, MESSAGE_CONTINUATION_PACKET_SIZE}; //10
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleEntityAnimation, ENTITY_ANIMATION_PACKET_SIZE}; //11
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDefineBlock, DEFINE_BLOCK_PACKET_SIZE}; //12
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleRemoveBlockDefinition, REMOVE_BLOCK_DEFINITION_PACKET_SIZE}; //13
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDefineEntityModel, PACKET_VARIABLE_SIZE}; //14
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleRemoveEntityModel, REMOVE_ENTITY_MODEL_PACKET_SIZE}; //15
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSetEntityModel, SET_ENTITY_MODEL_PACKET_SIZE}; //16
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTextureBegin, TEXTURE_BEGIN_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTextureData, TEXTURE_DATA_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTerrainTexture, TERRAIN_TEXTURE_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleHeldBlock, HELD_BLOCK_PACKET_SIZE}; //20
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleInventoryState, INVENTORY_STATE_PACKET_SIZE}; //21
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDroppedItem, DROPPED_ITEM_PACKET_SIZE}; //22
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleInventoryView, PACKET_VARIABLE_SIZE}; //23
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDefineItem, ITEM_DEFINITION_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDigProgress, DIG_PROGRESS_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleBreakingTexture, BREAKING_TEXTURE_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDefineHudBar, HUD_BAR_DEFINE_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSetHudBar, HUD_BAR_STATE_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleRemoveHudBar, HUD_BAR_REMOVE_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTextColor, TEXT_COLOR_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleNametag, NAMETAG_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandlePlayerImpulse, PLAYER_IMPULSE_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleEntityTexture, SET_ENTITY_TEXTURE_PACKET_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleChunkLight, PACKET_VARIABLE_SIZE};
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleResetChunks, RESET_CHUNKS_PACKET_SIZE};
}

void Network_Connect(void) {
    networkConnectedToServer = true;
    pthread_mutex_lock(&networkQueueMutex);
    resetDefinitionsPending = true;
    disconnectPending = false;
    acceptingIncoming = true;
    pthread_mutex_unlock(&networkQueueMutex);
    Network_Send(Packet_CreateIdentification(GAME_PROTOCOL_VERSION, networkName));
    Network_Send(Packet_CreateSetDrawDistance(world.drawDistance));
}

void Network_Disconnect(void) {
    pthread_mutex_lock(&networkQueueMutex);
    disconnectPending = true;
    acceptingIncoming = false;
    pthread_mutex_unlock(&networkQueueMutex);
    networkThreadState = -1;
}

static void Network_PerformDisconnect(void) {
    memset(textColors, 0, sizeof(textColors));
    ClientHudBars_Reset();
    ClientInventory_Reset();
    for (int i = 0; i < hmlen(world.chunks); i++) world.chunks[i].value->modified = false;
    bool wasLocal = LocalServer_IsRunning();
    if (wasLocal) {
        LocalServer_Stop();
    }
    Screen_Switch(SCREEN_LOGIN);
    if (!wasLocal) {
        World_Clear();
        Network_ClearQueue();
    }
    EntityModel_ResetDefinitions();
    ClientTextures_Reset();
    Block_ResetDefinitions();
    Block_FlushDefinitionChanges();
    ClientTextures_UpdateLiquidTints();
    networkConnectedToServer = false;
    networkThreadState = -1; //End network thread
    screenCursorEnabled = false;

    #if defined(PLATFORM_WEB)
    networkClientDisconnect();
    #endif
}

static bool IsChunkPacket(unsigned char opcode) {
    return opcode == 1 || opcode == 2 || opcode == 7 || opcode == 8 || opcode == 34 || opcode == 35;
}

static void Network_ExecutePacket(IncomingPacket packet) {
    pthread_mutex_lock(&networkQueueMutex);
    bool stopping = disconnectPending;
    if (IsChunkPacket(packet.data[0]) && packet.terrainGeneration != terrainGeneration) stopping = true;
    if(packet.data[0]==PACKET_TEXTURE_BEGIN || packet.data[0]==PACKET_TEXTURE_DATA) {
        queuedTextureBytes-=packet.length;
        if(queuedTextureBytes<0) queuedTextureBytes=0;
    }
    pthread_mutex_unlock(&networkQueueMutex);
    if (stopping) { MemFree(packet.data); return; }
    packetData = packet.data;
    packetDataLength = packet.length;
    packetReaderIndex = 1;
    if (packet.data[0] < packetCount) (*packets[packet.data[0]].handler)();
    MemFree(packet.data);
}

//Executed on the main thread
void Network_ProcessIncomingPackets(void) {
    pthread_mutex_lock(&networkQueueMutex);
    bool stopping = disconnectPending;
    bool reset = resetDefinitionsPending;
    disconnectPending = resetDefinitionsPending = false;
    pthread_mutex_unlock(&networkQueueMutex);
    if (stopping) {
        Network_PerformDisconnect();
        return;
    }
    if (reset) { memset(textColors, 0, sizeof(textColors)); ClientHudBars_Reset(); ClientInventory_Reset(); EntityModel_ResetDefinitions(); ClientTextures_Reset(); Block_ResetDefinitions(); }
    const int maxPacketsPerFrame = 1024;
    const int maxTerrainPacketsPerFrame = 64;
    const int maxTerrainBytesPerFrame = 256 * 1024;
    const double terrainPacketBudgetSeconds = 0.004;
    IncomingPacket gameplayPackets[maxPacketsPerFrame];
    IncomingPacket terrainPackets[maxPacketsPerFrame];
    int gameplayPacketCount = 0;
    int terrainPacketCount = 0;

    pthread_mutex_lock(&networkQueueMutex);
    gameplayPacketCount = arrlen(queuedData);
    if (gameplayPacketCount > maxPacketsPerFrame) gameplayPacketCount = maxPacketsPerFrame;
    for (int i = 0; i < gameplayPacketCount; i++) gameplayPackets[i] = queuedData[i];
    if (gameplayPacketCount > 0) arrdeln(queuedData, 0, gameplayPacketCount);

    terrainPacketCount = arrlen(terrainQueuedData);
    if (terrainPacketCount > maxTerrainPacketsPerFrame) terrainPacketCount = maxTerrainPacketsPerFrame;
    for (int i = 0; i < terrainPacketCount; i++) terrainPackets[i] = terrainQueuedData[i];
    if (terrainPacketCount > 0) arrdeln(terrainQueuedData, 0, terrainPacketCount);
    pthread_mutex_unlock(&networkQueueMutex);

    for (int i = 0; i < gameplayPacketCount; i++) {
        Network_ExecutePacket(gameplayPackets[i]);
    }

    int terrainProcessedCount = 0;
    int terrainBytes = 0;
    double terrainDeadline = GetTime() + terrainPacketBudgetSeconds;
    for (; terrainProcessedCount < terrainPacketCount; terrainProcessedCount++) {
        if (terrainProcessedCount > 0 && (GetTime() >= terrainDeadline ||
            terrainBytes + terrainPackets[terrainProcessedCount].length > maxTerrainBytesPerFrame)) break;
        terrainBytes += terrainPackets[terrainProcessedCount].length;
        Network_ExecutePacket(terrainPackets[terrainProcessedCount]);
    }

    int remainingCount = terrainPacketCount - terrainProcessedCount;
    if (remainingCount > 0) {
        pthread_mutex_lock(&networkQueueMutex);
        arrinsn(terrainQueuedData, 0, remainingCount);
        for (int i = 0; i < remainingCount; i++) {
            terrainQueuedData[i] = terrainPackets[terrainProcessedCount + i];
        }
        pthread_mutex_unlock(&networkQueueMutex);
    }
    Block_FlushDefinitionChanges();
    ClientTextures_UpdateLiquidTints();
}

//Receive data and list it for the main thread to execute
void Network_Receive(unsigned char *data, int dataLength) {
    if (!data || dataLength < 1) return;
    unsigned char opcode = data[0];
    if (opcode >= packetCount) return;
    int fixedLength = packets[opcode].fixedLength;
    if (fixedLength != 0 && dataLength != fixedLength) return;

    //Copy received data before enet clears it later.
    unsigned char* nextData = MemAlloc(dataLength);
    if (!nextData) return;
    memcpy(nextData, data, dataLength);

    pthread_mutex_lock(&networkQueueMutex);
    if (!acceptingIncoming || disconnectPending) {
        pthread_mutex_unlock(&networkQueueMutex);
        MemFree(nextData);
        return;
    }
    if(opcode==PACKET_TEXTURE_BEGIN || opcode==PACKET_TEXTURE_DATA) {
        if(queuedTextureBytes+dataLength>TEXTURE_MAX_BYTES) {
            pthread_mutex_unlock(&networkQueueMutex); MemFree(nextData); return;
        }
        queuedTextureBytes+=dataLength;
    }
    // Keep light maps behind their chunk data, including across the terrain budget.
    if (opcode == 35) {
        terrainGeneration++;
        // Remove obsolete terrain even when it is waiting behind the frame budget.
        // Generation tags also invalidate packets already extracted by the main thread.
        int kept = 0;
        for (int i = 0; i < arrlen(terrainQueuedData); i++) {
            if (IsChunkPacket(terrainQueuedData[i].data[0]))
                MemFree(terrainQueuedData[i].data);
            else terrainQueuedData[kept++] = terrainQueuedData[i];
        }
        if (terrainQueuedData) (void)arrsetlen(terrainQueuedData, kept);
    }
    bool modifiesTerrain = opcode == 35 || opcode == 0 || opcode == 1 || opcode == 2 || opcode == 7 || opcode == 8 || opcode == 34 ||
                           opcode == PACKET_DEFINE_BLOCK || opcode == PACKET_REMOVE_BLOCK_DEFINITION;
    IncomingPacket packet = {nextData, dataLength, terrainGeneration};
    if (modifiesTerrain) arrput(terrainQueuedData, packet);
    else arrput(queuedData, packet);
    pthread_mutex_unlock(&networkQueueMutex);
    
}

void Network_Send(unsigned char *packet) {
     if (packet == NULL) return;

    if (networkConnectedToServer) {
        int packetLength = Packet_GetLength(packet[0]);
        networkClientSend(packet, packetLength);
    }

    MemFree(packet);
}

void Network_ClearQueue(void) {
    pthread_mutex_lock(&networkQueueMutex);
    for (int i = 0; i < arrlen(queuedData); i++) MemFree(queuedData[i].data);
    arrfree(queuedData);
    queuedData = NULL;
    queuedTextureBytes=0;
    for (int i = 0; i < arrlen(terrainQueuedData); i++) MemFree(terrainQueuedData[i].data);
    arrfree(terrainQueuedData);
    terrainQueuedData = NULL;
    pthread_mutex_unlock(&networkQueueMutex);
}
