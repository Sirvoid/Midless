/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#define ENET_IMPLEMENTATION
#include "enet.h"
#include "server.h"
#include "networkhandler.h"


#define MAX_CLIENTS 64

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
    
    puts("Started server.");
    
    ENetEvent event;
    
    Network_Init();
    
    while(*state != -1) {
        while (enet_host_service(server, &event, 33) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    event.peer->data = Network_InitPlayer(event.peer);
                    Network_Connect(event.peer->data);
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    Network_Receive(event.peer->data, (unsigned char*)event.packet->data);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                    Network_Disconnect(event.peer->data);
                    break;
                    
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
            enet_host_flush(server);
        }
    }
    

    enet_host_destroy(server);
    enet_deinitialize();
}

void Server_Send(void *peer, unsigned char* packet, int length) {
    ENetPacket * epacket = enet_packet_create (packet, length, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, epacket);
}