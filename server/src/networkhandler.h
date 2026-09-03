/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_NETWORK_HANDLER_H
#define MIDLESS_SERVER_NETWORK_HANDLER_H

typedef struct PacketHandlerEntry {
    void (*handler)(void);
} PacketHandlerEntry;

typedef struct IncomingPacket {
    unsigned char *data;
    void *player;
    int length;
} IncomingPacket;

void ServerNetwork_Init(void);
void ServerNetwork_Shutdown(void);
void ServerNetwork_Connect(void *playerData);
void ServerNetwork_Disconnect(void *playerData);
int ServerNetwork_PlayerReadyForRemoval(void *playerData);
void ServerNetwork_ProcessIncomingPackets(void);
void ServerNetwork_Receive(void *playerData, unsigned char* data, int dataLength);
//Send a packet and take ownership.
void ServerNetwork_Send(void *playerData, unsigned char* packet);
void ServerNetwork_ExecutePacket(unsigned char* packet);

#endif
