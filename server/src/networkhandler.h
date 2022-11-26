/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_NETWORK_H
#define S_NETWORK_H

typedef struct PacketDefinition {
    void (*handler)(void);
} PacketDefinition;

typedef struct IncomingPacket {
    unsigned char *data;
    void *playerPtr;
} IncomingPacket;

void Network_Init(void);
void* Network_InitPlayer(void* peerPtr, bool isWeb);
void Network_Connect(void *playerPtr);
void Network_Disconnect(void *playerPtr);
void Network_ReadIncomingPackets(void);
void Network_Receive(void *playerPtr, unsigned char* data, int dataLength);
void Network_Send(void *playerPtr, unsigned char* packet);
void Network_ExecutePacket(unsigned char* packet);

#endif