/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_NETWORK_H
#define G_NETWORK_H

typedef struct PacketDefinition {
    void (*handler)(void);
} PacketDefinition;

typedef struct QueuedData {
    void* next;
    void* prev;
    unsigned char* data;
} QueuedData;

extern int Network_ping;
extern int Network_threadState;
extern char *Network_name;
extern char *Network_ip;
extern int Network_port;
extern int Network_connectedToServer;
extern void (*Network_Internal_Client_Send)(unsigned char*, int);

void Network_Init(void);
void Network_Connect(void);
void Network_Disconnect(void);
void Network_ReadQueue(void);
void Network_Receive(unsigned char* data, int dataLength);
void Network_Send(unsigned char* packet);

#endif