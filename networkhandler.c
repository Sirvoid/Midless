#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include "networkhandler.h"
#include "packet.h"
#include "raylib.h"
#include "screens.h"
#include "world.h"

PacketDefinition packets[256];
int Network_connectedToServer = 0;
void (*Network_Internal_Client_Send)(unsigned char*, int);

QueuedData *endQueuedData = NULL;
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
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_Pong}; //3
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_SpawnEntity}; //4
    packets[packetsNb++] = (PacketDefinition) {&Packet_H_TeleportEntity}; //5
}

void Network_Connect(void) {
    Network_connectedToServer = true;
    Network_Send(Packet_Identification(1, Network_name));
    Network_Send(&(unsigned char){1}); //ping
}

void Network_Disconnect(void) {
    Screen_Switch(SCREEN_LOGIN);
    Network_connectedToServer = false;
    Network_threadState = -1; //End network thread
    Screen_cursorEnabled = false;
    World_Unload();
}

//Executed on the main thread
void Network_ReadQueue(void) {
    while(true) {

        if(!endQueuedData) return;
        
        //Find start of queue.
        QueuedData* startQueuedData = endQueuedData;
        while(startQueuedData->prev) { 
            startQueuedData = startQueuedData->prev;
        }
        
        //Handle packet
        Packet_data = startQueuedData->data;
        PacketReader_index = 1;
        if(startQueuedData->data[0] < packetsNb) (*packets[startQueuedData->data[0]].handler)();


        //Free queue node
        if(startQueuedData->next) {
            ((QueuedData*)startQueuedData->next)->prev = NULL;
        } else if(!startQueuedData->next) {
            endQueuedData = NULL;
        }
   
        MemFree(startQueuedData->data);
        MemFree(startQueuedData);
        
    }
}

//Receive data and list it for the main thread to execute
void Network_Receive(unsigned char *data, int dataLength) {
    
    if(data[0] == 3) { //Execute ping packet right away for precision
        (*packets[data[0]].handler)(); 
        return;
    }

    //Copy received data before enet clears it later.
    QueuedData* nextData = MemAlloc(sizeof(QueuedData));
    nextData->data = MemAlloc(dataLength);
    memcpy(nextData->data, data, dataLength);
    
    if(endQueuedData) {
        nextData->prev = endQueuedData;
        endQueuedData->next = nextData;
        endQueuedData = endQueuedData->next;
    } else {
        nextData->prev = NULL;
        nextData->next = NULL;
        endQueuedData = nextData;
    }
    
}

void Network_Send(unsigned char *packet) {
    if(!Network_connectedToServer) return;
    Network_Internal_Client_Send(packet, Packet_GetLength(packet[0]));
}