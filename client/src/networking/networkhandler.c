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

PacketDefinition packets[256];
int Network_connectedToServer = 0;
void (*Network_Internal_Client_Send)(unsigned char*, int);
void (*Network_Internal_Client_Disconnect)(void);

unsigned char* *queuedData = NULL;
unsigned char* *terrainQueuedData = NULL;
int packetsNb;

int Network_ping = 0;
int Network_threadState = 0;
char *Network_name = "Player";
char *Network_ip = "127.0.0.1";
char *Network_fullAddress = "127.0.0.1:25565";
int Network_port = 25565;

void Network_Init(void) {
    packetsNb = 0;
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_MapInit}; //0
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_LoadChunk}; //1
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SetBlock}; //2
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SpawnEntity}; //3
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_TeleportEntity}; //4
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Message}; //5
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_DespawnEntity}; //6
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_UnloadChunk}; //7
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_BlockBatch}; //8
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_WorldTime}; //9
}

void Network_Connect(void) {
    Network_connectedToServer = true;
    Network_Send(Packet_CreateIdentification(1, Network_name));
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
    Network_threadState = -1; //End network thread
    Screen_cursorEnabled = false;

    #if defined(PLATFORM_WEB)
    Network_Internal_Client_Disconnect();
    #endif
}

pthread_mutex_t queue_mutex;

static void Network_ExecutePacket(unsigned char *packet) {
    Packet_data = packet;
    PacketReader_index = 1;
    if (packet[0] < packetsNb) (*packets[packet[0]].handler)();
    MemFree(packet);
}

//Executed on the main thread
void Network_ReadQueue(void) {
    const int maxPacketsPerFrame = 1024;
    const double terrainPacketBudgetSeconds = 0.002;
    unsigned char *gameplayPackets[maxPacketsPerFrame];
    unsigned char *terrainPackets[maxPacketsPerFrame];
    int gameplayPacketCount = 0;
    int terrainPacketCount = 0;

    pthread_mutex_lock(&queue_mutex);
    gameplayPacketCount = arrlen(queuedData);
    if (gameplayPacketCount > maxPacketsPerFrame) gameplayPacketCount = maxPacketsPerFrame;
    for (int i = 0; i < gameplayPacketCount; i++) gameplayPackets[i] = queuedData[i];
    if (gameplayPacketCount > 0) arrdeln(queuedData, 0, gameplayPacketCount);

    terrainPacketCount = arrlen(terrainQueuedData);
    if (terrainPacketCount > maxPacketsPerFrame) terrainPacketCount = maxPacketsPerFrame;
    for (int i = 0; i < terrainPacketCount; i++) terrainPackets[i] = terrainQueuedData[i];
    if (terrainPacketCount > 0) arrdeln(terrainQueuedData, 0, terrainPacketCount);
    pthread_mutex_unlock(&queue_mutex);

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
        pthread_mutex_lock(&queue_mutex);
        arrinsn(terrainQueuedData, 0, remainingCount);
        for (int i = 0; i < remainingCount; i++) {
            terrainQueuedData[i] = terrainPackets[terrainProcessedCount + i];
        }
        pthread_mutex_unlock(&queue_mutex);
    }
}

//Receive data and list it for the main thread to execute
void Network_Receive(unsigned char *data, int dataLength) {

    //Copy received data before enet clears it later.
    unsigned char* nextData = MemAlloc(dataLength);
    memcpy(nextData, data, dataLength);

    pthread_mutex_lock(&queue_mutex);
    unsigned char opcode = nextData[0];
    bool modifiesTerrain = opcode == 1 || opcode == 2 || opcode == 7 || opcode == 8;
    if (modifiesTerrain) arrput(terrainQueuedData, nextData);
    else arrput(queuedData, nextData);
    pthread_mutex_unlock(&queue_mutex);
    
}

void Network_Send(unsigned char *packet) {
     if (packet == NULL) return;

    if (Network_connectedToServer) {
        int packetLength = Packet_GetLength(packet[0]);
        Network_Internal_Client_Send(packet, packetLength);
    }

    MemFree(packet);
}

void Network_ClearQueue(void) {
    pthread_mutex_lock(&queue_mutex);
    for (int i = 0; i < arrlen(queuedData); i++) MemFree(queuedData[i]);
    arrfree(queuedData);
    queuedData = NULL;
    for (int i = 0; i < arrlen(terrainQueuedData); i++) MemFree(terrainQueuedData[i]);
    arrfree(terrainQueuedData);
    terrainQueuedData = NULL;
    pthread_mutex_unlock(&queue_mutex);
}
