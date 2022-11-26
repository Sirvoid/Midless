/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_NETWORK_H
#define G_NETWORK_H

typedef struct PacketDefinition {
    void (*handler)(void);
} PacketDefinition;

extern int Network_ping;
extern int Network_threadState;
extern char *Network_name;
extern char *Network_ip;
extern char *Network_fullAddress;
extern int Network_port;
extern int Network_connectedToServer;
extern void (*Network_Internal_Client_Send)(unsigned char*, int);
extern void (*Network_Internal_Client_Disconnect)(void);

void Network_Init(void);
void Network_Connect(void);
void Network_Disconnect(void);
void Network_ReadQueue(void);
void Network_Receive(unsigned char* data, int dataLength);
void Network_Send(unsigned char* packet);

#endif