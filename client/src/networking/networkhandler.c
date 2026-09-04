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

PacketHandlerEntry packets[256];
int networkConnectedToServer = 0;
void (*networkClientSend)(unsigned char*, int);
void (*networkClientDisconnect)(void);

unsigned char* *queuedData = NULL;
unsigned char* *terrainQueuedData = NULL;
int packetCount;

int networkPing = 0;
int networkThreadState = 0;
char *networkName = "Player";
char *networkIp = "127.0.0.1";
char *networkFullAddress = "127.0.0.1:25565";
int networkPort = 25565;

void Network_Init(void) {
    packetCount = 0;
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMapInit}; //0
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleLoadChunk}; //1
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSetBlock}; //2
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleSpawnEntity}; //3
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleTeleportEntity}; //4
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMessage}; //5
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleDespawnEntity}; //6
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleUnloadChunk}; //7
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleBlockBatch}; //8
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleWorldTime}; //9
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleMessageContinuation}; //10
    packets[packetCount++] = (PacketHandlerEntry) {&Packet_HandleEntityAnimation}; //11
}

void Network_Connect(void) {
    networkConnectedToServer = true;
    Network_Send(Packet_CreateIdentification(1, networkName));
    Network_Send(Packet_CreateSetDrawDistance(world.drawDistance));
}

void Network_Disconnect(void) {
    bool wasLocal = LocalServer_IsRunning();
    if (wasLocal) {
        LocalServer_Stop();
    }
    Screen_Switch(SCREEN_LOGIN);
    if (!wasLocal) {
        World_Clear();
        Network_ClearQueue();
    }
    networkThreadState = -1; //End network thread
    screenCursorEnabled = false;

    #if defined(PLATFORM_WEB)
    networkClientDisconnect();
    #endif
}

pthread_mutex_t networkQueueMutex;

static void Network_ExecutePacket(unsigned char *packet) {
    packetData = packet;
    packetReaderIndex = 1;
    if (packet[0] < packetCount) (*packets[packet[0]].handler)();
    MemFree(packet);
}

//Executed on the main thread
void Network_ProcessIncomingPackets(void) {
    const int maxPacketsPerFrame = 1024;
    const double terrainPacketBudgetSeconds = 0.002;
    unsigned char *gameplayPackets[maxPacketsPerFrame];
    unsigned char *terrainPackets[maxPacketsPerFrame];
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
}

//Receive data and list it for the main thread to execute
void Network_Receive(unsigned char *data, int dataLength) {

    //Copy received data before enet clears it later.
    unsigned char* nextData = MemAlloc(dataLength);
    memcpy(nextData, data, dataLength);

    pthread_mutex_lock(&networkQueueMutex);
    unsigned char opcode = nextData[0];
    bool modifiesTerrain = opcode == 1 || opcode == 2 || opcode == 7 || opcode == 8;
    if (modifiesTerrain) arrput(terrainQueuedData, nextData);
    else arrput(queuedData, nextData);
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
    for (int i = 0; i < arrlen(queuedData); i++) MemFree(queuedData[i]);
    arrfree(queuedData);
    queuedData = NULL;
    for (int i = 0; i < arrlen(terrainQueuedData); i++) MemFree(terrainQueuedData[i]);
    arrfree(terrainQueuedData);
    terrainQueuedData = NULL;
    pthread_mutex_unlock(&networkQueueMutex);
}
