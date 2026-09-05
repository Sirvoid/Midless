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
#include "packet.h"
#include "screens.h"
#include "world.h"
#include "localserver.h"
#include "block.h"

PacketHandlerEntry packets[256];
int networkConnectedToServer = 0;
void (*networkClientSend)(unsigned char*, int);
void (*networkClientDisconnect)(void);

typedef struct IncomingPacket {
    unsigned char *data;
    int length;
} IncomingPacket;
static IncomingPacket *queuedData, *terrainQueuedData;
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
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMapInit, 0}; //0
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleLoadChunk, 0}; //1
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSetBlock, 14}; //2
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSpawnEntity, 17}; //3
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTeleportEntity, 17}; //4
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMessage, 65}; //5
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDespawnEntity, 3}; //6
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleUnloadChunk, 13}; //7
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleBlockBatch, 0}; //8
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleWorldTime, 5}; //9
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMessageContinuation, 65}; //10
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleEntityAnimation, 4}; //11
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDefineBlock, DEFINE_BLOCK_PACKET_SIZE}; //12
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleRemoveBlockDefinition, 2}; //13
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
    Block_ResetDefinitions();
    Block_FlushDefinitionChanges();
    networkConnectedToServer = false;
    networkThreadState = -1; //End network thread
    screenCursorEnabled = false;

    #if defined(PLATFORM_WEB)
    networkClientDisconnect();
    #endif
}

static void Network_ExecutePacket(IncomingPacket packet) {
    pthread_mutex_lock(&networkQueueMutex);
    bool stopping = disconnectPending;
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
    if (reset) Block_ResetDefinitions();
    const int maxPacketsPerFrame = 1024;
    const double terrainPacketBudgetSeconds = 0.002;
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
    if (terrainPacketCount > maxPacketsPerFrame) terrainPacketCount = maxPacketsPerFrame;
    for (int i = 0; i < terrainPacketCount; i++) terrainPackets[i] = terrainQueuedData[i];
    if (terrainPacketCount > 0) arrdeln(terrainQueuedData, 0, terrainPacketCount);
    pthread_mutex_unlock(&networkQueueMutex);

    for (int i = 0; i < gameplayPacketCount; i++) {
        Network_ExecutePacket(gameplayPackets[i]);
    }

    int terrainProcessedCount = 0;
    double terrainDeadline = GetTime() + terrainPacketBudgetSeconds;
    for (; terrainProcessedCount < terrainPacketCount; terrainProcessedCount++) {
        if (terrainProcessedCount > 0 && GetTime() >= terrainDeadline) break;
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
    bool modifiesTerrain = opcode == 0 || opcode == 1 || opcode == 2 || opcode == 7 || opcode == 8 ||
                           opcode == PACKET_DEFINE_BLOCK || opcode == PACKET_REMOVE_BLOCK_DEFINITION;
    IncomingPacket packet = {nextData, dataLength};
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
    for (int i = 0; i < arrlen(terrainQueuedData); i++) MemFree(terrainQueuedData[i].data);
    arrfree(terrainQueuedData);
    terrainQueuedData = NULL;
    pthread_mutex_unlock(&networkQueueMutex);
}
