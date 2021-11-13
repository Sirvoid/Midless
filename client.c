#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define ENET_IMPLEMENTATION
#include "enet.h"
#include "client.h"
#include "networkhandler.h"

#define CLIENT_TIMEOUT 3000

ENetPeer* peer = { 0 };

void *Client_Init(void *state) {
    
    enet_initialize();
    
    Network_Internal_Client_Send = &Client_Send;
    Client_Do((int*)state);
    
    enet_deinitialize();
    
    return NULL;
}

void Client_Do(int *state) {
    
    ENetHost* client = { 0 };
    client = enet_host_create(NULL, 1, 2, 0, 0);
    if (client == NULL) {
        puts("Couldn't create client.");
        return;
    }
    
    ENetAddress address = { 0 };
    ENetEvent event = { 0 };
    
    enet_address_set_host(&address, Network_ip);
    address.port = Network_port;
    peer = enet_host_connect(client, &address, 2, 0);

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
    while(*state != -1) {
        while (enet_host_service(client, &event, 1) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE:
                    Network_Receive((unsigned char*)event.packet->data, event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    puts("disconnected.");
                    Network_Disconnect();
                    disconnected = true;
                    break;
                default:
                    break;
            }
        }
    }

    if (!disconnected) {
        enet_peer_reset(peer);
    }

    enet_host_destroy(client);

    *state = 0;
    Network_connectedToServer = false;
}

void Client_Send(unsigned char* packet, int packetLength) {
    ENetPacket *epacket = enet_packet_create(packet, packetLength, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, epacket);
}