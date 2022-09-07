/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "player.h"
#include "networkhandler.h"
#include "packet.h"
#include "server.h"
#include "world.h"

PacketDefinition packets[256];
int packetsNb = 0;

void Network_Init(void) {
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Identification};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SetBlock};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_PlayerPosition};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Message};
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SetDrawDistance};
}

void* Network_InitPlayer(void* peerPtr) {
    Player *player = MemAlloc(sizeof(Player));
    player->peerPtr = peerPtr;
    player->drawDistance = 3;
    return (void*)player;
}

void Network_Connect(void *playerPtr) {
}

void Network_Disconnect(void *playerPtr) {
    Player *player = (Player*)playerPtr;
    printf("%s disconnected.\n", player->name);
    World_RemovePlayer(player);
}

void Network_Receive(void *playerPtr, unsigned char* data) {
    Packet_player = (Player*)playerPtr;
    Packet_data = data;
    PacketReader_index = 1;
    if (data[0] < packetsNb) {
        (*packets[data[0]].handler)();
    }
}

void Network_Send(void *playerPtr, unsigned char* packet) {
    Player *player = (Player*)playerPtr;
    int packetLength = Packet_GetLength(packet[0]);
    if (packetLength == 0) packetLength = Packet_LastDynamicLength;
    Server_Send(player->peerPtr, packet, packetLength);
}

