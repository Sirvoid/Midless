/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_NETWORK_HANDLER_H
#define MIDLESS_CLIENT_NETWORK_HANDLER_H

typedef struct PacketHandlerEntry {
    void (*handler)(void);
} PacketHandlerEntry;

extern int networkPing;
extern int networkThreadState;
extern char *networkName;
extern char *networkIp;
extern char *networkFullAddress;
extern int networkPort;
extern int networkConnectedToServer;
extern void (*networkClientSend)(unsigned char*, int);
extern void (*networkClientDisconnect)(void);

void Network_Init(void);
void Network_Connect(void);
void Network_Disconnect(void);
void Network_ProcessIncomingPackets(void);
void Network_ClearQueue(void);
void Network_Receive(unsigned char* data, int dataLength);
//Send a packet and take ownership.
void Network_Send(unsigned char* packet);

#endif
