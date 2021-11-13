#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include "packet.h"
#include "world.h"
#include "networkhandler.h"

#define PACKET_STRING_SIZE 64

unsigned char *Packet_data;
int Packet_Lengths[256] = {
    66, //0
    1, //1
    8, //2
    7 //3
};
int PingCalculation_oldTime = 0;

int Packet_GetLength(unsigned char opcode) {
    return Packet_Lengths[opcode];
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Packets Readers----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
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
    char *string = MemAlloc(PACKET_STRING_SIZE);
    
    for(int i = 0; i < PACKET_STRING_SIZE; i++) {
        string[i] = Packet_data[PacketReader_index++];
    }
    
    return string;
}


unsigned char* Packet_ReadArray(int size) {
    unsigned char *arr = MemAlloc(size);
    memcpy(arr, &Packet_data[PacketReader_index], size);
    PacketReader_index += size;
    return arr;
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Packets Writer-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
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

void Packet_WriteString(unsigned char* packet, char* string) {
    int length = TextLength(string);
    for(int i = 0; i < 64; i++) {
        if(i < length) {
            packet[PacketWriter_index++] = string[i];
        } else {
            packet[PacketWriter_index++] = 0;
        }  
    }
    
}

/*-------------------------------------------------------------------------------------------------------*
*------------------------------------------Packets Received----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
int mapLoadingChunkCnt = 0;
int compressedLength = 0;
unsigned char* compressedMap;
void Packet_H_MapInit(void) {

    int nbChunksX = Packet_ReadByte();
    int nbChunksY = Packet_ReadByte();
    int nbChunksZ = Packet_ReadByte();
    compressedLength = Packet_ReadInt();
    compressedMap = MemAlloc(compressedLength);

    world.size.x = nbChunksX;
    world.size.y = nbChunksY;
    world.size.z = nbChunksZ;
    
    World_Unload();
    
    mapLoadingChunkCnt = 0;
    
}


void Packet_H_MapChunk(void) {
    
    int dataLength = Packet_ReadShort();

    unsigned char* compressedChunk = Packet_ReadArray(1024);
    
    memcpy(&compressedMap[mapLoadingChunkCnt], compressedChunk, dataLength);
    mapLoadingChunkCnt += dataLength;
    
    if(mapLoadingChunkCnt >= compressedLength) {
        int decompressedLength = 0;
        
        World_Load(World_Decompress(compressedMap, compressedLength, &decompressedLength));
        MemFree(compressedMap);
        
    }
}

void Packet_H_SetBlock(void) {
    int BlockID = Packet_ReadByte();
    Vector3 position = (Vector3) { Packet_ReadUShort(), Packet_ReadUShort(), Packet_ReadUShort() };
    World_SetBlock(position, BlockID);
}

void Packet_H_Pong(void) {
    Network_ping = fmax(0, (int)(GetTime() * 1000) - PingCalculation_oldTime);
    PingCalculation_oldTime = (int)(GetTime() * 1000);
    Network_Send(&(unsigned char){1}); //ping
}

void Packet_H_SpawnEntity(void) {
    int ID = Packet_ReadUShort();
    int type = Packet_ReadByte();
    int x = Packet_ReadUShort();
    int y = Packet_ReadUShort();
    int z = Packet_ReadUShort();
    World_AddEntity(ID, type, (Vector3) { x / 64.0f, y / 64.0f, z / 64.0f });
}

void Packet_H_TeleportEntity(void) {
    int ID = Packet_ReadUShort();
    int x = Packet_ReadUShort();
    int y = Packet_ReadUShort();
    int z = Packet_ReadUShort();
    World_TeleportEntity(ID, (Vector3) { x / 64.0f, y / 64.0f, z / 64.0f });
}


/*-------------------------------------------------------------------------------------------------------*
*--------------------------------------------Packets Sent------------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
unsigned char* Packet_Identification(char version, char* name) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[0]);
    Packet_WriteByte(packet, 0);
    Packet_WriteByte(packet, version);
    Packet_WriteString(packet, name);
    return packet;
}

unsigned char* Packet_SetBlock(unsigned char blockID, Vector3 position) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[2]);
    Packet_WriteByte(packet, 2);
    Packet_WriteByte(packet, blockID);
    Packet_WriteUShort(packet, (unsigned short)position.x);
    Packet_WriteUShort(packet, (unsigned short)position.y);
    Packet_WriteUShort(packet, (unsigned short)position.z);
    return packet;
}

unsigned char* Packet_PlayerPosition(Vector3 position) {
    PacketWriter_index = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(Packet_Lengths[3]);
    Packet_WriteByte(packet, 3);
    Packet_WriteUShort(packet, (unsigned short)(position.x * 64));
    Packet_WriteUShort(packet, (unsigned short)(position.y * 64));
    Packet_WriteUShort(packet, (unsigned short)(position.z * 64));
    return packet;
}