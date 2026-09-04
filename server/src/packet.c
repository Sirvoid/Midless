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
#include "logger.h"
#include "luabindings.h"

#define PACKET_STRING_SIZE 64

unsigned char *serverPacketData;
Player *serverPacketPlayer;
int serverPacketLastDynamicLength;
int serverPacketDataLength;

int serverPacketLengths[256] = {
    1,  //map init
    0, //load chunk
    14,  //setblock
    17, //spawnEntity
    17, //teleportEntity
    65, //Message
    3, //despawnEntity
    13, //unload chunk
    0, //block batch
    5, //world time
    65 //message continuation
};

int ServerPacket_GetLength(unsigned char opcode) {
    return serverPacketLengths[opcode];
}

//Packet Readers

int serverPacketReaderIndex = 1;

unsigned char ServerPacket_ReadByte(void) {
    if (serverPacketReaderIndex >= serverPacketDataLength) return 0;
    return serverPacketData[serverPacketReaderIndex++];
}

short ServerPacket_ReadShort(void) {
    if (serverPacketReaderIndex > serverPacketDataLength - 2) return 0;
    short value = (short)(serverPacketData[serverPacketReaderIndex] << 8 | serverPacketData[serverPacketReaderIndex + 1]);
    serverPacketReaderIndex += 2;
    return value;
}

unsigned short ServerPacket_ReadUShort(void) {
    if (serverPacketReaderIndex > serverPacketDataLength - 2) return 0;
    unsigned short value = (unsigned short)(serverPacketData[serverPacketReaderIndex] << 8 | serverPacketData[serverPacketReaderIndex + 1]);
    serverPacketReaderIndex += 2;
    return value;
}

int ServerPacket_ReadInt(void) {
    if (serverPacketReaderIndex > serverPacketDataLength - 4) return 0;
    int value = (int)(serverPacketData[serverPacketReaderIndex] << 24 | serverPacketData[serverPacketReaderIndex + 1] << 16 | serverPacketData[serverPacketReaderIndex + 2] << 8 | serverPacketData[serverPacketReaderIndex + 3]);
    serverPacketReaderIndex += 4;
    return value;
}

char* ServerPacket_ReadString(void) {
    char *string = MemAlloc(PACKET_STRING_SIZE + 1);
    if (string == NULL) return NULL;
    if (serverPacketReaderIndex > serverPacketDataLength - PACKET_STRING_SIZE) {
        string[0] = 0;
        return string;
    }
    
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        string[i] = serverPacketData[serverPacketReaderIndex++];
    }
    
    string[PACKET_STRING_SIZE] = 0;
    
    return string;
}

unsigned char* ServerPacket_ReadArray(int size) {
    if (size < 0 || serverPacketReaderIndex > serverPacketDataLength - size) return NULL;
    unsigned char *arr = MemAlloc(size);
    if (arr == NULL) return NULL;
    memcpy(arr, &serverPacketData[serverPacketReaderIndex], size);
    serverPacketReaderIndex += size;
    return arr;
}

//Packet Writers

int serverPacketWriterIndex = 0;

void ServerPacket_WriteByte(unsigned char* packet, unsigned char value) {
    packet[serverPacketWriterIndex++] = value;
}

void ServerPacket_WriteShort(unsigned char* packet, short value) {
    packet[serverPacketWriterIndex++] = (char)(value >> 8);
	packet[serverPacketWriterIndex++] = (char)(value);
}

void ServerPacket_WriteUShort(unsigned char* packet, unsigned short value) {
    packet[serverPacketWriterIndex++] = (char)(value >> 8);
	packet[serverPacketWriterIndex++] = (char)(value);
}

void ServerPacket_WriteInt(unsigned char* packet, int value) {
    packet[serverPacketWriterIndex++] = (char)(value >> 24);
	packet[serverPacketWriterIndex++] = (char)(value >> 16);
    packet[serverPacketWriterIndex++] = (char)(value >> 8);
    packet[serverPacketWriterIndex++] = (char)(value);
}

void ServerPacket_WriteString(unsigned char *packet, const char *string) {
    int length = TextLength(string);
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        if (i < length) {
            packet[serverPacketWriterIndex++] = string[i];
        } else {
            packet[serverPacketWriterIndex++] = 0;
        }  
    }
}

void ServerPacket_WriteArray(unsigned char* packet, unsigned char* array, int size) {
    for (int i = 0; i < size; i++) {
        packet[serverPacketWriterIndex++] = array[i];
    }
}

/* Packets Received */

void ServerPacket_HandleIdentification(void) {
    if(serverPacketPlayer->name != NULL) return;
    int protocolVersion = ServerPacket_ReadUShort();
    serverPacketPlayer->name = ServerPacket_ReadString();
    ServerLogger_Log(TextFormat("%s connected. Protocol version: %i\n", serverPacketPlayer->name, protocolVersion));
    ServerWorld_AddPlayer(serverPacketPlayer);
    ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateMapInit());
    ServerNetwork_Send(serverPacketPlayer, ServerPacket_CreateWorldTime(serverWorld.time));
}

void ServerPacket_HandleSetBlock(void) {
    int blockId = ServerPacket_ReadByte();
    Vector3 position = (Vector3) { ServerPacket_ReadInt(), ServerPacket_ReadInt(), ServerPacket_ReadInt() };
    ServerWorld_SetBlock(position, blockId, true);
}

void ServerPacket_HandlePlayerPosition(void) {
    Vector3 position = (Vector3) { ServerPacket_ReadInt() / 64.0f, ServerPacket_ReadInt() / 64.0f, ServerPacket_ReadInt() / 64.0f };
    Vector3 rotation = (Vector3) {ServerPacket_ReadByte(), ServerPacket_ReadByte(), 0};
    ServerPlayer_UpdatePositionRotation(serverPacketPlayer, position, rotation);
}

void ServerPacket_HandleMessage(void) {
    char *message = ServerPacket_ReadString();
    
    int nameLen = TextLength(serverPacketPlayer->name);
    char* sentMessage = MemAlloc(nameLen + 3 + 64 + 1);
    
    //username
    sentMessage[0] = '<';
    memcpy(&sentMessage[1], serverPacketPlayer->name, nameLen);
    sentMessage[nameLen + 1] = '>';
    sentMessage[nameLen + 2] = ' ';
    
    //message
    memcpy(&sentMessage[nameLen + 3], message, TextLength(message));
    
    //end string
    sentMessage[nameLen + 3 + 64] = 0;
    
    ServerWorld_SendMessage(sentMessage);
    LuaBindings_InvokeChatMessage(serverPacketPlayer->name, message);
    MemFree(sentMessage);
    MemFree(message);
}

void ServerPacket_HandleSetDrawDistance(void) {
    unsigned char distance = ServerPacket_ReadByte();
    if (distance > serverWorld.maxDrawDistance) distance = serverWorld.maxDrawDistance;
    if (distance < 2) distance = 2;
    serverPacketPlayer->drawDistance = distance;
}

/* Packets sent */

unsigned char* ServerPacket_CreateMapInit(void) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[0]);
    ServerPacket_WriteByte(packet, 0);
    
    return packet;
}

unsigned char* ServerPacket_CreateLoadChunk(unsigned short* chunkArray, unsigned short length,
                                            Vector3 chunkPosition, const unsigned char *skyMask) {
    serverPacketWriterIndex = 0;
    serverPacketLastDynamicLength = (length * 2) + 15 + CHUNK_SKY_MASK_SIZE;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLastDynamicLength);
    ServerPacket_WriteByte(packet, 1);
    ServerPacket_WriteInt(packet, (int)chunkPosition.x);
    ServerPacket_WriteInt(packet, (int)chunkPosition.y);
    ServerPacket_WriteInt(packet, (int)chunkPosition.z);
    ServerPacket_WriteUShort(packet, length);
    ServerPacket_WriteArray(packet, (unsigned char*)chunkArray, length * 2);
    ServerPacket_WriteArray(packet, (unsigned char*)skyMask, CHUNK_SKY_MASK_SIZE);
    return packet;
}

unsigned char* ServerPacket_CreateUnloadChunk(Vector3 chunkPosition) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[7]);
    ServerPacket_WriteByte(packet, 7);
    ServerPacket_WriteInt(packet, (int)chunkPosition.x);
    ServerPacket_WriteInt(packet, (int)chunkPosition.y);
    ServerPacket_WriteInt(packet, (int)chunkPosition.z);
    return packet;
}

unsigned char* ServerPacket_CreateSetBlock(unsigned char blockId, Vector3 position) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[2]);
    ServerPacket_WriteByte(packet, 2);
    ServerPacket_WriteByte(packet, blockId);
    ServerPacket_WriteInt(packet, (int)position.x);
    ServerPacket_WriteInt(packet, (int)position.y);
    ServerPacket_WriteInt(packet, (int)position.z);
    return packet;
}

unsigned char* ServerPacket_CreateBlockBatch(const ServerBlockUpdate *updates, unsigned short count) {
    serverPacketWriterIndex = 0;
    serverPacketLastDynamicLength = 3 + count * 13;
    unsigned char *packet = MemAlloc(serverPacketLastDynamicLength);
    ServerPacket_WriteByte(packet, 8);
    ServerPacket_WriteUShort(packet, count);
    for (int i = 0; i < count; i++) {
        ServerPacket_WriteByte(packet, updates[i].blockId);
        ServerPacket_WriteInt(packet, (int)updates[i].position.x);
        ServerPacket_WriteInt(packet, (int)updates[i].position.y);
        ServerPacket_WriteInt(packet, (int)updates[i].position.z);
    }
    return packet;
}

unsigned char* ServerPacket_CreateSpawnEntity(Entity *entity) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[3]);
    ServerPacket_WriteByte(packet, 3);
    ServerPacket_WriteUShort(packet, entity->id);
    ServerPacket_WriteByte(packet, entity->type);
    ServerPacket_WriteByte(packet, entity->model);
    ServerPacket_WriteInt(packet, (int)(entity->position.x * 64));
    ServerPacket_WriteInt(packet, (int)(entity->position.y * 64));
    ServerPacket_WriteInt(packet, (int)(entity->position.z * 64));
    return packet;
}

unsigned char* ServerPacket_CreateDespawnEntity(Entity *entity) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[6]);
    ServerPacket_WriteByte(packet, 6);
    ServerPacket_WriteUShort(packet, entity->id);
    return packet;
}

unsigned char* ServerPacket_CreateTeleportEntity(Entity *entity, Vector3 position, Vector3 rotation) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[4]);
    ServerPacket_WriteByte(packet, 4);
    ServerPacket_WriteUShort(packet, entity->id);
    ServerPacket_WriteInt(packet, (int)(position.x * 64));
    ServerPacket_WriteInt(packet, (int)(position.y * 64));
    ServerPacket_WriteInt(packet, (int)(position.z * 64));
    ServerPacket_WriteByte(packet, rotation.x);
    ServerPacket_WriteByte(packet, rotation.y);
    return packet;
}

unsigned char* ServerPacket_CreateMessage(const char* message) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[5]);
    ServerPacket_WriteByte(packet, 5);
    ServerPacket_WriteString(packet, message);
    return packet;
}

unsigned char* ServerPacket_CreateMessageContinuation(const char* message) {
    serverPacketWriterIndex = 0;
    unsigned char* packet = (unsigned char*)MemAlloc(serverPacketLengths[10]);
    ServerPacket_WriteByte(packet, 10);
    ServerPacket_WriteString(packet, message);
    return packet;
}

unsigned char* ServerPacket_CreateWorldTime(float timeSeconds) {
    serverPacketWriterIndex = 0;
    unsigned char *packet = MemAlloc(serverPacketLengths[9]);
    ServerPacket_WriteByte(packet, 9);
    ServerPacket_WriteInt(packet, (int)(timeSeconds * 1000.0f));
    return packet;
}
