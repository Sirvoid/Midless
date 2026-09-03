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

PacketDefinition serverPacketHandlers[256];
int serverPacketHandlerCount = 0;
pthread_mutex_t serverNetworkMutex;
pthread_mutex_t serverSendMutex;
IncomingPacket *serverIncomingPackets = NULL;

void ServerNetwork_Init(void) {
    serverPacketHandlerCount = 0;
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketDefinition) {&ServerPacket_H_Identification};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketDefinition) {&ServerPacket_H_SetBlock};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketDefinition) {&ServerPacket_H_PlayerPosition};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketDefinition) {&ServerPacket_H_Message};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketDefinition) {&ServerPacket_H_SetDrawDistance};
}

void ServerNetwork_Shutdown(void) {
    for (int i = 0; i < arrlen(serverIncomingPackets); i++) {
        free(serverIncomingPackets[i].data);
    }
    arrfree(serverIncomingPackets);
    serverIncomingPackets = NULL;
}

void ServerNetwork_Connect(void *playerPtr) {
}

void ServerNetwork_Disconnect(void *playerPtr) {
    Player *player = (Player*)playerPtr;
    ServerLogger_Log(TextFormat("%s disconnected.\n", player->name));
    player->disconnected = true;
}

void ServerNetwork_ReadIncomingPackets(void) {
    while (true) {
        IncomingPacket packet = {0};

        pthread_mutex_lock(&serverNetworkMutex);
        if (arrlen(serverIncomingPackets) > 0) {
            packet = serverIncomingPackets[0];
            arrdel(serverIncomingPackets, 0);
        }
        pthread_mutex_unlock(&serverNetworkMutex);

        if (packet.data == NULL) return;

        serverPacketPlayer = (Player*)packet.playerPtr;
        serverPacketData = packet.data;
        serverPacketReaderIndex = 1;
        if (serverPacketData[0] < serverPacketHandlerCount) {
            (*serverPacketHandlers[serverPacketData[0]].handler)();
        }
        free(packet.data);
    }
}

void ServerNetwork_Receive(void *playerPtr, unsigned char* data, int dataLength) {
    pthread_mutex_lock(&serverNetworkMutex);

    IncomingPacket packet;
    packet.data = malloc(dataLength);
    memcpy(packet.data, data, dataLength);
    packet.playerPtr = playerPtr;
    arrput(serverIncomingPackets, packet);

    pthread_mutex_unlock(&serverNetworkMutex);
}

void ServerNetwork_Send(void *playerPtr, unsigned char* packet) {

    if(packet == NULL) return;

    Player *player = (Player*)playerPtr;
    int packetLength = ServerPacket_GetLength(packet[0]);
    if (packetLength == 0) packetLength = serverPacketLastDynamicLength;

    if(player->isWeb == false) {
        Server_Send(player->peerPtr, packet, packetLength);
    } else {
        #if defined(SERVER_WEB_SUPPORT)
        ServerWSS_Send(player->peerPtr, packet, packetLength);
        #endif
    }

    MemFree(packet);

}
