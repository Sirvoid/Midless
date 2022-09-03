/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include "packet.h"
#include "networkhandler.h"
#include "world.h"
#include "chunk/chunk.h"
#include "entity.h"

#define PACKET_STRING_SIZE 64

unsigned char *Packet_data;
Player *Packet_player;
int Packet_LastDynamicLength;

int Packet_Lengths[256] = {
    1,  //map init
    0, //load chunk
    14,  //setblock
    16, //spawnEntity
    17, //teleportEntity
    65, //Message
    3, //despawnEntity
    13 //unload chunk
};

int Packet_GetLength(unsigned char opcode) {
    return Packet_Lengths[opcode];
}

//Packet Readers

int PacketReader_index = 1;

unsigned char Packet_ReadByte(void) {
    return Packet_data[PacketReader_index++];
}

short Packet_ReadShort(void) {
    short value = (short)(Packet_data[PacketReader_index] << 8 | Packet_data[PacketReader_index + 1]); 
    PacketReader_index += 2;
    return value;
}

unsigned short Packet_ReadUShort(void) {
    unsigned short value = (unsigned short)(Packet_data[PacketReader_index] << 8 | Packet_data[PacketReader_index + 1]); 
    PacketReader_index += 2;
    return value;
}

int Packet_ReadInt(void) {
    int value = (int)(Packet_data[PacketReader_index] << 24 | Packet_data[PacketReader_index + 1] << 16 | Packet_data[PacketReader_index + 2] << 8 | Packet_data[PacketReader_index + 3]); 
    PacketReader_index += 4;
    return value;
}

char* Packet_ReadString(void) {
    char *string = MemAlloc(PACKET_STRING_SIZE + 1);
    
    for(int i = 0; i < PACKET_STRING_SIZE; i++) {
        string[i] = Packet_data[PacketReader_index++];
    }
    
    string[PACKET_STRING_SIZE] = 0;
    
    return string;
}

unsigned char* Packet_ReadArray(int size) {
    unsigned char *arr = MemAlloc(size);
    memcpy(arr, &Packet_data[PacketReader_index], size);
    PacketReader_index += size;
    return arr;
}

//Packet Writers

int PacketWriter_index = 0;

void Packet_WriteByte(unsigned char* packet, unsigned char value) {
    packet[PacketWriter_index++] = value;
}

void Packet_WriteShort(unsigned char* packet, short value) {
    packet[PacketWriter_index++] = (char)(value >> 8);
	packet[PacketWriter_index++] = (char)(value);
}

void Packet_WriteUShort(unsigned char* packet, unsigned short value) {
    packet[PacketWriter_index++] = (char)(value >> 8);
	packet[PacketWriter_index++] = (char)(value);
}

void Packet_WriteInt(unsigned char* packet, int value) {
    packet[PacketWriter_index++] = (char)(value >> 24);
	packet[PacketWriter_index++] = (char)(value >> 16);
    packet[PacketWriter_index++] = (char)(value >> 8);
    packet[PacketWriter_index++] = (char)(value);
}

void Packet_WriteString(unsigned char *packet, const char *string) {
    int length = TextLength(string);
    for(int i = 0; i < PACKET_STRING_SIZE; i++) {
        if(i < length) {
            packet[PacketWriter_index++] = string[i];
        } else {
            packet[PacketWriter_index++] = 0;
        }  
    }
}

void Packet_WriteArray(unsigned char* packet, unsigned char* array, int size) {
    for(int i = 0; i < size; i++) {
        packet[PacketWriter_index++] = array[i];
    }
}

/* Packets Received */

void Packet_H_Identification(void) {
    int protocolVersion = Packet_ReadUShort();
    Packet_player->name = Packet_ReadString();
    printf("%s connected. Protocol version: %i\n", Packet_player->name, protocolVersion);
    World_AddPlayer(Packet_player);
    Network_Send(Packet_player, Packet_MapInit());
}

void Packet_H_SetBlock(void) {
    int BlockID = Packet_ReadByte();
    Vector3 position = (Vector3) { Packet_ReadInt(), Packet_ReadInt(), Packet_ReadInt() };
    World_SetBlock(position, BlockID, true);
}

void Packet_H_PlayerPosition(void) {
    Vector3 position = (Vector3) { Packet_ReadInt() / 64.0f, Packet_ReadInt() / 64.0f, Packet_ReadInt() / 64.0f };
    Vector3 rotation = (Vector3) {Packet_ReadByte(), Packet_ReadByte(), 0};
    Player_UpdatePositionRotation(Packet_player, position, rotation);
}

void Packet_H_Message(void) {
    char *message = Packet_ReadString();
    
    int nameLen = TextLength(Packet_player->name);
    char* sentMessage = MemAlloc(nameLen + 3 + 64 + 1);
    
    //username
    sentMessage[0] = '<';
    memcpy(&sentMessage[1], Packet_player->name, nameLen);
    sentMessage[nameLen + 1] = '>';
    sentMessage[nameLen + 2] = ' ';
    
    //message
    memcpy(&sentMessage[nameLen + 3], message, TextLength(message));
    
    //end string
    sentMessage[nameLen + 3 + 64] = 0;
    
    World_SendMessage(sentMessage);
    MemFree(sentMessage);
}

/* Packets sent */

unsigned char* Packet_MapInit(void) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[0]);
    Packet_WriteByte(packet, 0);
    
    return packet;
}

unsigned char* Packet_LoadChunk(unsigned short* chunkArray, unsigned short length, Vector3 chunkPosition) {
    PacketWriter_index = 0;
    Packet_LastDynamicLength = (length * 2) + 15;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_LastDynamicLength);
    Packet_WriteByte(packet, 1);
    Packet_WriteInt(packet, (int)chunkPosition.x);
    Packet_WriteInt(packet, (int)chunkPosition.y);
    Packet_WriteInt(packet, (int)chunkPosition.z);
    Packet_WriteUShort(packet, length);
    Packet_WriteArray(packet, (unsigned char*)chunkArray, length * 2);
    return packet;
}

unsigned char* Packet_UnloadChunk(Vector3 chunkPosition) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[7]);
    Packet_WriteByte(packet, 7);
    Packet_WriteInt(packet, (int)chunkPosition.x);
    Packet_WriteInt(packet, (int)chunkPosition.y);
    Packet_WriteInt(packet, (int)chunkPosition.z);
    return packet;
}

unsigned char* Packet_SetBlock(unsigned char blockID, Vector3 position) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[2]);
    Packet_WriteByte(packet, 2);
    Packet_WriteByte(packet, blockID);
    Packet_WriteInt(packet, (int)position.x);
    Packet_WriteInt(packet, (int)position.y);
    Packet_WriteInt(packet, (int)position.z);
    return packet;
}

unsigned char* Packet_SpawnEntity(Entity *entity) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[3]);
    Packet_WriteByte(packet, 3);
    Packet_WriteUShort(packet, entity->ID);
    Packet_WriteByte(packet, entity->type);
    Packet_WriteInt(packet, (int)(entity->position.x * 64));
    Packet_WriteInt(packet, (int)(entity->position.y * 64));
    Packet_WriteInt(packet, (int)(entity->position.z * 64));
    return packet;
}

unsigned char* Packet_DespawnEntity(Entity *entity) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[6]);
    Packet_WriteByte(packet, 6);
    Packet_WriteUShort(packet, entity->ID);
    return packet;
}

unsigned char* Packet_TeleportEntity(Entity *entity, Vector3 position, Vector3 rotation) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[4]);
    Packet_WriteByte(packet, 4);
    Packet_WriteUShort(packet, entity->ID);
    Packet_WriteInt(packet, (int)(position.x * 64));
    Packet_WriteInt(packet, (int)(position.y * 64));
    Packet_WriteInt(packet, (int)(position.z * 64));
    Packet_WriteByte(packet, rotation.x);
    Packet_WriteByte(packet, rotation.y);
    return packet;
}

unsigned char* Packet_SendMessage(const char* message) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[5]);
    Packet_WriteByte(packet, 5);
    Packet_WriteString(packet, message);
    return packet;
}