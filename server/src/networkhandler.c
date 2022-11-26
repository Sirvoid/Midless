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
#include "stb_ds.h"
#include "raylib.h"
#include "player.h"
#include "networkhandler.h"
#include "packet.h"
#include "server.h"
#include "serverwss.h"
#include "world.h"
#include "logger.h"

PacketDefinition packets[256];
int packetsNb = 0;
pthread_mutex_t network_mutex;
pthread_mutex_t snetwork_mutex;
IncomingPacket *incomingPackets = NULL;

void Network_Init(void) {
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Identification};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SetBlock};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_PlayerPosition};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Message};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SetDrawDistance};
}

void* Network_InitPlayer(void* peerPtr, bool isWeb) {
    Player *player = MemAlloc(sizeof(Player));
    player->peerPtr = peerPtr;
    player->drawDistance = 3;
    player->isWeb = isWeb;
    player->disconnected = false;
    return (void*)player;
}

void Network_Connect(void *playerPtr) {
}

void Network_Disconnect(void *playerPtr) {
    Player *player = (Player*)playerPtr;
    Logger_Log(TextFormat("%s disconnected.\n", player->name));
    player->disconnected = true;
}

void Network_ReadIncomingPackets(void) {
    while(arrlen(incomingPackets) > 0) {
        Packet_player = (Player*)incomingPackets[0].playerPtr;
        Packet_data = incomingPackets[0].data;
        PacketReader_index = 1;
        if (Packet_data[0] < packetsNb) {
            (*packets[Packet_data[0]].handler)();
        }
        free(incomingPackets[0].data);

        pthread_mutex_lock(&network_mutex);
        arrdel(incomingPackets, 0);
        pthread_mutex_unlock(&network_mutex);
    }
}

void Network_Receive(void *playerPtr, unsigned char* data, int dataLength) {
    pthread_mutex_lock(&network_mutex);

    IncomingPacket packet;
    packet.data = malloc(dataLength);
    memcpy(packet.data, data, dataLength);
    packet.playerPtr = playerPtr;
    arrput(incomingPackets, packet);

    pthread_mutex_unlock(&network_mutex);
}

void Network_Send(void *playerPtr, unsigned char* packet) {
    Player *player = (Player*)playerPtr;
    int packetLength = Packet_GetLength(packet[0]);
    if (packetLength == 0) packetLength = Packet_LastDynamicLength;

    if(player->isWeb == false) {
        Server_Send(player->peerPtr, packet, packetLength);
    } else {
        #if defined(SERVER_WEB_SUPPORT)
        ServerWSS_Send(player->peerPtr, packet, packetLength);
        #endif
    }

}

