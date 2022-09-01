/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include "stb_ds.h"
#include "raylib.h"
#include "networkhandler.h"
#include "packet.h"
#include "screens.h"
#include "world.h"

PacketDefinition packets[256];
int Network_connectedToServer = 0;
void (*Network_Internal_Client_Send)(unsigned char*, int);

unsigned char* *queuedData = NULL;
int packetsNb;

int Network_ping = 0;
int Network_threadState = 0;
char *Network_name = "Player";
char *Network_ip = "127.0.0.1";
int Network_port = 25565;

void Network_Init(void) {
    packetsNb = 0;
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_MapInit}; //0
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_MapChunk}; //1
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SetBlock}; //2
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SpawnEntity}; //3
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_TeleportEntity}; //4
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Message}; //5
}

void Network_Connect(void) {
    Network_connectedToServer = true;
    Network_Send(Packet_Identification(1, Network_name));
}

void Network_Disconnect(void) {
    Screen_Switch(SCREEN_LOGIN);
    Network_connectedToServer = false;
    Network_threadState = -1; //End network thread
    Screen_cursorEnabled = false;
    World_Unload();
}

pthread_mutex_t queue_mutex;

//Executed on the main thread
void Network_ReadQueue(void) {
    while(true) {

        if(arrlen(queuedData) == 0) return;
        
        //Find start of queue.
        unsigned char* startQueuedData = queuedData[0];
        
        //Handle packet
        Packet_data = startQueuedData;
        PacketReader_index = 1;
        if(startQueuedData[0] < packetsNb) (*packets[startQueuedData[0]].handler)();

        MemFree(startQueuedData);

        pthread_mutex_lock(&queue_mutex);
        arrdel(queuedData, 0);
        pthread_mutex_unlock(&queue_mutex);
        
    }
}

//Receive data and list it for the main thread to execute
void Network_Receive(unsigned char *data, int dataLength) {

    //Copy received data before enet clears it later.
    unsigned char* nextData = MemAlloc(dataLength);
    memcpy(nextData, data, dataLength);

    pthread_mutex_lock(&queue_mutex);
    arrput(queuedData, nextData);
    pthread_mutex_unlock(&queue_mutex);
    
}

void Network_Send(unsigned char *packet) {
    if(!Network_connectedToServer) return;
    Network_Internal_Client_Send(packet, Packet_GetLength(packet[0]));
}