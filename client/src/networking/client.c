/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if !defined(PLATFORM_WEB)

#define ENET_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "enet.h"
#include "stb_ds.h"
#include "client.h"
#include "networkhandler.h"

#define CLIENT_TIMEOUT 5000

ENetPeer* peer = { 0 };

typedef struct ClientOutgoingPacket {
    unsigned char *data;
    int length;
} ClientOutgoingPacket;

static ClientOutgoingPacket *clientOutgoingPackets;
static pthread_mutex_t clientOutgoingMutex = PTHREAD_MUTEX_INITIALIZER;

static void Client_FlushOutgoing(ENetHost *client) {
    pthread_mutex_lock(&clientOutgoingMutex);
    ClientOutgoingPacket *packets = clientOutgoingPackets;
    clientOutgoingPackets = NULL;
    pthread_mutex_unlock(&clientOutgoingMutex);

    for (int i = 0; i < arrlen(packets); i++) {
        ENetPacket *packet = enet_packet_create(
            packets[i].data, packets[i].length, ENET_PACKET_FLAG_RELIABLE);
        if (packet != NULL && enet_peer_send(peer, 0, packet) < 0) {
            enet_packet_destroy(packet);
        }
        free(packets[i].data);
    }
    if (arrlen(packets) > 0) enet_host_flush(client);
    arrfree(packets);
}

static void Client_ClearOutgoing(void) {
    pthread_mutex_lock(&clientOutgoingMutex);
    for (int i = 0; i < arrlen(clientOutgoingPackets); i++) {
        free(clientOutgoingPackets[i].data);
    }
    arrfree(clientOutgoingPackets);
    clientOutgoingPackets = NULL;
    pthread_mutex_unlock(&clientOutgoingMutex);
}

void *Client_Init(void *state) {

    enet_initialize();
    
    networkClientSend = &Client_Send;
    Client_Do((int*)state);
    
    enet_deinitialize();
    
    return NULL;
}

void Client_Do(int *state) {
    
    ENetHost* client = { 0 };
    client = enet_host_create(NULL, 1, 1, 0, 0);
    if (client == NULL) {
        puts("Couldn't create client.");
        return;
    }
    
    ENetAddress address = { 0 };
    ENetEvent event = { 0 };
    
    enet_address_set_host(&address, networkIp);
    address.port = networkPort;
    peer = enet_host_connect(client, &address, 1, 0);
    Network_Init();

    if (enet_host_service(client, &event, CLIENT_TIMEOUT) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        Network_Connect();
        puts("Connection succeeded.");
    } else {
        enet_peer_reset(peer);
        puts("Connection failed.");
        Network_Disconnect();
        return;
    }

    uint8_t disconnected = false;
    
    //read events
    while (*state != -1) {
        Client_FlushOutgoing(client);
        while (enet_host_service(client, &event, 5) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE:
                    Network_Receive((unsigned char*)event.packet->data, event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                    puts("disconnected.");
                    Network_Disconnect();
                    disconnected = true;
                    break;

                default:
                    break;
            }
            Client_FlushOutgoing(client);
        }
        Client_FlushOutgoing(client);
    }

    if (!disconnected) {
        enet_peer_disconnect_now(peer, 0);
        enet_peer_reset(peer);
    }

    enet_host_destroy(client);
    Client_ClearOutgoing();

    *state = 0;
    networkConnectedToServer = false;
}

void Client_Send(unsigned char* packet, int packetLength) {
    if (packet == NULL || packetLength <= 0) return;

    ClientOutgoingPacket outgoing;
    outgoing.data = malloc(packetLength);
    if (outgoing.data == NULL) return;
    memcpy(outgoing.data, packet, packetLength);
    outgoing.length = packetLength;

    pthread_mutex_lock(&clientOutgoingMutex);
    arrput(clientOutgoingPackets, outgoing);
    pthread_mutex_unlock(&clientOutgoingMutex);
}

#endif
