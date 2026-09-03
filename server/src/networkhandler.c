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

PacketHandlerEntry serverPacketHandlers[256];
int serverPacketHandlerCount = 0;
pthread_mutex_t serverNetworkMutex;
pthread_mutex_t serverSendMutex;
IncomingPacket *serverIncomingPackets = NULL;

#define SERVER_MAX_QUEUED_PACKETS 256 * 64
#define SERVER_MAX_QUEUED_PACKETS_PER_PLAYER 256

static const int serverIncomingPacketLengths[] = {
    67, 
    14, 
    15, 
    65, 
    2
};

void ServerNetwork_Init(void) {
    pthread_mutex_init(&serverNetworkMutex, NULL);
    serverPacketHandlerCount = 0;
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleIdentification};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleSetBlock};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandlePlayerPosition};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleMessage};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleSetDrawDistance};
}

void ServerNetwork_Shutdown(void) {
    pthread_mutex_lock(&serverNetworkMutex);
    for (int i = 0; i < arrlen(serverIncomingPackets); i++) {
        Player *player = serverIncomingPackets[i].player;
        if (player != NULL && player->pendingPackets > 0) player->pendingPackets--;
        free(serverIncomingPackets[i].data);
    }
    arrfree(serverIncomingPackets);
    serverIncomingPackets = NULL;
    pthread_mutex_unlock(&serverNetworkMutex);
    pthread_mutex_destroy(&serverNetworkMutex);
}

void ServerNetwork_Connect(void *playerData) {
}

void ServerNetwork_Disconnect(void *playerData) {
    Player *player = (Player*)playerData;
    if (player == NULL) return;

    pthread_mutex_lock(&serverNetworkMutex);

    if (player->disconnected) {
        pthread_mutex_unlock(&serverNetworkMutex);
        return;
    }

    ServerLogger_Log(TextFormat("%s disconnected.\n",
        player->name != NULL ? player->name : "Unidentified player"));
    player->disconnected = true;

    for (int i = arrlen(serverIncomingPackets) - 1; i >= 0; i--) {
        if (serverIncomingPackets[i].player != player) continue;
        free(serverIncomingPackets[i].data);
        arrdel(serverIncomingPackets, i);
        if (player->pendingPackets > 0) player->pendingPackets--;
    }

    bool destroyUnidentified = player->name == NULL && player->pendingPackets == 0;
   
    pthread_mutex_unlock(&serverNetworkMutex);

    if (destroyUnidentified) ServerPlayer_Destroy(player);
}

int ServerNetwork_PlayerReadyForRemoval(void *playerData) {
    Player *player = playerData;
    if (player == NULL) return 0;

    pthread_mutex_lock(&serverNetworkMutex);
    int ready = player->disconnected && player->pendingPackets == 0;
    pthread_mutex_unlock(&serverNetworkMutex);

    return ready;
}

void ServerNetwork_ProcessIncomingPackets(void) {
    while (true) {
        IncomingPacket packet = {0};

        pthread_mutex_lock(&serverNetworkMutex);
        if (arrlen(serverIncomingPackets) > 0) {
            packet = serverIncomingPackets[0];
            arrdel(serverIncomingPackets, 0);
        }
        pthread_mutex_unlock(&serverNetworkMutex);

        if (packet.data == NULL) return;

        serverPacketPlayer = (Player*)packet.player;
        serverPacketData = packet.data;
        serverPacketDataLength = packet.length;
        serverPacketReaderIndex = 1;
        if (!serverPacketPlayer->disconnected &&
            (serverPacketData[0] == 0 || serverPacketPlayer->name != NULL)) {
            (*serverPacketHandlers[serverPacketData[0]].handler)();
        }
        free(packet.data);

        pthread_mutex_lock(&serverNetworkMutex);

        if (serverPacketPlayer->pendingPackets > 0) serverPacketPlayer->pendingPackets--;

        bool destroyUnidentified = serverPacketPlayer->disconnected &&
            serverPacketPlayer->name == NULL && serverPacketPlayer->pendingPackets == 0;

        pthread_mutex_unlock(&serverNetworkMutex);

        if (destroyUnidentified) ServerPlayer_Destroy(serverPacketPlayer);
    }
}

void ServerNetwork_Receive(void *playerData, unsigned char* data, int dataLength) {
    if (playerData == NULL || data == NULL || dataLength < 1) return;

    unsigned char opcode = data[0];
    if (opcode >= serverPacketHandlerCount ||
        dataLength != serverIncomingPacketLengths[opcode]) return;

    pthread_mutex_lock(&serverNetworkMutex);

    Player *player = playerData;
    if (player->disconnected ||
        arrlen(serverIncomingPackets) >= SERVER_MAX_QUEUED_PACKETS ||
        player->pendingPackets >= SERVER_MAX_QUEUED_PACKETS_PER_PLAYER) {
        pthread_mutex_unlock(&serverNetworkMutex);
        return;
    }

    IncomingPacket packet;
    packet.data = malloc(dataLength);
    if (packet.data == NULL) {
        pthread_mutex_unlock(&serverNetworkMutex);
        return;
    }
    memcpy(packet.data, data, dataLength);
    packet.player = playerData;
    packet.length = dataLength;
    arrput(serverIncomingPackets, packet);
    player->pendingPackets++;

    pthread_mutex_unlock(&serverNetworkMutex);
}

void ServerNetwork_Send(void *playerData, unsigned char* packet) {

    if(packet == NULL) return;

    Player *player = (Player*)playerData;
    int packetLength = ServerPacket_GetLength(packet[0]);
    if (packetLength == 0) packetLength = serverPacketLastDynamicLength;

    if(player->isWeb == false) {
        Server_Send(player->peer, packet, packetLength);
    } else {
        #if defined(SERVER_WEB_SUPPORT)
        ServerWss_Send(player->peer, packet, packetLength);
        #endif
    }

    MemFree(packet);

}
