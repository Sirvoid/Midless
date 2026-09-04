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
#define ENET_IMPLEMENTATION
#include "enet.h"
#include "stb_ds.h"
#include "server.h"
#include "networkhandler.h"

struct Player;
struct Player *ServerPlayer_Create(void *peer, bool isWeb);

#define MAX_CLIENTS 64

typedef struct ServerOutgoingPacket {
    ENetPeer *peer;
    unsigned char *data;
    int length;
} ServerOutgoingPacket;

static ServerOutgoingPacket *serverOutgoingPackets;
static pthread_mutex_t serverOutgoingMutex = PTHREAD_MUTEX_INITIALIZER;

static void Server_FlushOutgoing(ENetHost *server) {
    pthread_mutex_lock(&serverOutgoingMutex);
    ServerOutgoingPacket *packets = serverOutgoingPackets;
    serverOutgoingPackets = NULL;
    pthread_mutex_unlock(&serverOutgoingMutex);

    for (int i = 0; i < arrlen(packets); i++) {
        ENetPacket *packet = enet_packet_create(
            packets[i].data, packets[i].length, ENET_PACKET_FLAG_RELIABLE);
        if (packet != NULL && enet_peer_send(packets[i].peer, 0, packet) < 0) {
            enet_packet_destroy(packet);
        }
        free(packets[i].data);
    }
    if (arrlen(packets) > 0) enet_host_flush(server);
    arrfree(packets);
}

static void Server_ClearOutgoing(void) {
    pthread_mutex_lock(&serverOutgoingMutex);
    for (int i = 0; i < arrlen(serverOutgoingPackets); i++) {
        free(serverOutgoingPackets[i].data);
    }
    arrfree(serverOutgoingPackets);
    serverOutgoingPackets = NULL;
    pthread_mutex_unlock(&serverOutgoingMutex);
}

void *Server_Init(void *state) {
    
    enet_initialize();
    
    Server_Do((int*)state);
    
    enet_deinitialize();
    
    return NULL;
}

void Server_Do(int *state) {
    
    enet_initialize();
    ENetAddress address = {0};
    address.host = ENET_HOST_ANY;
    address.port = 25565;

    ENetHost * server = enet_host_create(&address, MAX_CLIENTS, 1, 0, 0);
    ENetEvent event;
    
    while (*state != -1) {
        Server_FlushOutgoing(server);
        while (enet_host_service(server, &event, 5) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    event.peer->data = ServerPlayer_Create(event.peer, false);
                    ServerNetwork_Connect(event.peer->data);
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    ServerNetwork_Receive(event.peer->data, (unsigned char*)event.packet->data, event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                    ServerNetwork_Disconnect(event.peer->data);
                    break;
                    
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
            Server_FlushOutgoing(server);
        }
        Server_FlushOutgoing(server);
    }
    

    Server_ClearOutgoing();
    enet_host_destroy(server);
    enet_deinitialize();
}

void Server_Send(void *peer, unsigned char* packet, int length) {
    if (peer == NULL || packet == NULL || length <= 0) return;

    ServerOutgoingPacket outgoing;
    outgoing.peer = peer;
    outgoing.data = malloc(length);
    if (outgoing.data == NULL) return;
    memcpy(outgoing.data, packet, length);
    outgoing.length = length;

    pthread_mutex_lock(&serverOutgoingMutex);
    arrput(serverOutgoingPackets, outgoing);
    pthread_mutex_unlock(&serverOutgoingMutex);
}
