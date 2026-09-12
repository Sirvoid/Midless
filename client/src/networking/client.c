/**
 * Copyright (c) 2021 Sirvoid
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


static pthread_t clientThread;
static pthread_mutex_t clientStateMutex = PTHREAD_MUTEX_INITIALIZER;
static bool clientThreadCreated, clientFinished, clientStopRequested;

static bool Client_Stopping(void) {
    pthread_mutex_lock(&clientStateMutex);
    bool stopping = clientStopRequested;
    pthread_mutex_unlock(&clientStateMutex);
    return stopping;
}

void Client_Stop(void) {
    pthread_mutex_lock(&clientStateMutex);
    clientStopRequested = true;
    pthread_mutex_unlock(&clientStateMutex);
}

bool Client_IsBusy(void) {
    if (!clientThreadCreated) return false;
    pthread_mutex_lock(&clientStateMutex);
    bool finished = clientFinished;
    pthread_mutex_unlock(&clientStateMutex);
    if (!finished) return true;
    pthread_join(clientThread, NULL);
    clientThreadCreated = false;
    return false;
}

bool Client_Start(void) {
    if (Client_IsBusy()) return false;
    pthread_mutex_lock(&clientStateMutex);
    clientStopRequested = clientFinished = false;
    pthread_mutex_unlock(&clientStateMutex);
    networkThreadState = 0;
    if (pthread_create(&clientThread, NULL, Client_Init, NULL) != 0) return false;
    clientThreadCreated = true;
    return true;
}

void Client_Shutdown(void) {
    Client_Stop();
    if (clientThreadCreated) pthread_join(clientThread, NULL);
    clientThreadCreated = false;
}

void *Client_Init(void *state) {
    (void)state;
    if (enet_initialize() != 0) Network_Disconnect();
    else {
        networkClientSend = &Client_Send;
        Client_Do(NULL);
        enet_deinitialize();
    }
    pthread_mutex_lock(&clientStateMutex);
    clientFinished = true;
    pthread_mutex_unlock(&clientStateMutex);
    return NULL;
}

void Client_Do(int *state) {
    (void)state;
    ENetHost *client = enet_host_create(NULL, 1, 1, 0, 0);
    if (!client) { Network_Disconnect(); return; }
    ENetAddress address = {0};
    ENetEvent event = {0};
    bool connected = false;
    if (enet_address_set_host(&address, networkIp) < 0 || Client_Stopping()) goto cleanup;
    address.port = networkPort;
    peer = enet_host_connect(client, &address, 1, 0);
    if (!peer) goto cleanup;
    Network_Init();
    enet_uint32 started = enet_time_get();
    while (!Client_Stopping() && enet_time_get() - started < CLIENT_TIMEOUT) {
        int result = enet_host_service(client, &event, 50);
        if (result < 0) break;
        if (result > 0 && event.type == ENET_EVENT_TYPE_CONNECT) { connected = true; break; }
        if (result > 0 && event.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(event.packet);
    }
    if (!connected || Client_Stopping()) goto cleanup;
    // Serialize connection publication with cancellation so a late connect cannot
    // restore the session after the main thread has returned to the menu.
    pthread_mutex_lock(&clientStateMutex);
    if (!clientStopRequested) Network_Connect();
    pthread_mutex_unlock(&clientStateMutex);
    while (!Client_Stopping()) {
        Client_FlushOutgoing(client);
        int result = enet_host_service(client, &event, 5);
        if (result < 0) break;
        if (result <= 0) continue;
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            Network_Receive((unsigned char *)event.packet->data, event.packet->dataLength);
            enet_packet_destroy(event.packet);
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT || event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT) break;
    }
cleanup:
    if (peer) { enet_peer_disconnect_now(peer, 0); enet_peer_reset(peer); peer = NULL; }
    enet_host_destroy(client);
    Client_ClearOutgoing();
    if (!Client_Stopping()) Network_Disconnect();
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
