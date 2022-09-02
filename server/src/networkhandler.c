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

void Network_Init(void) {
    packets[0] = (PacketDefinition) {&Packet_H_Identification};
    packets[1] = (PacketDefinition) {&Packet_H_SetBlock};
    packets[2] = (PacketDefinition) {&Packet_H_PlayerPosition};
    packets[3] = (PacketDefinition) {&Packet_H_Message};
}

void* Network_InitPlayer(void* peerPtr) {
    Player *player = MemAlloc(sizeof(Player));
    player->peerPtr = peerPtr;
    return (void*)player;
}

void Network_Connect(void *playerPtr) {
}

void Network_Disconnect(void *playerPtr) {
    Player *player = (Player*)playerPtr;
    printf("%s disconnected.\n", player->name);
    World_RemovePlayer(player);
    MemFree(playerPtr);
}

void Network_Receive(void *playerPtr, unsigned char* data) {
    Packet_player = (Player*)playerPtr;
    Packet_data = data;
    PacketReader_index = 1;
    (*packets[data[0]].handler)();
}

void Network_Send(void *playerPtr, unsigned char* packet) {
    Player *player = (Player*)playerPtr;
    int packetLength = Packet_GetLength(packet[0]);
    if(packetLength == 0) packetLength = Packet_LastDynamicLength;
    Server_Send(player->peerPtr, packet, packetLength);
}

