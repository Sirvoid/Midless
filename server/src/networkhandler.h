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

void Network_Init(void);
void* Network_InitPlayer(void* peerPtr);
void Network_Connect(void *playerPtr);
void Network_Disconnect(void *playerPtr);
void Network_Receive(void *playerPtr, unsigned char* data);
void Network_Send(void *playerPtr, unsigned char* packet);

#endif