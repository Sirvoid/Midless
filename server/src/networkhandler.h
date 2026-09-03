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

void ServerNetwork_Init(void);
void ServerNetwork_Shutdown(void);
void ServerNetwork_Connect(void *playerPtr);
void ServerNetwork_Disconnect(void *playerPtr);
void ServerNetwork_ReadIncomingPackets(void);
void ServerNetwork_Receive(void *playerPtr, unsigned char* data, int dataLength);
//Send a packet and take ownership.
void ServerNetwork_Send(void *playerPtr, unsigned char* packet);
void ServerNetwork_ExecutePacket(unsigned char* packet);

#endif
