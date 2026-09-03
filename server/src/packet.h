/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_PACKET_H
#define S_PACKET_H

#include "player.h"
#include "entity.h"

typedef struct ServerBlockUpdate {
    Vector3 position;
    unsigned char blockID;
} ServerBlockUpdate;

extern unsigned char *serverPacketData;
extern Player *serverPacketPlayer;
extern int serverPacketLastDynamicLength;
extern int serverPacketReaderIndex;

int ServerPacket_GetLength(unsigned char opcode);

unsigned char ServerPacket_ReadByte(void);
unsigned short ServerPacket_ReadUShort(void);
char* ServerPacket_ReadString(void);
unsigned char* ServerPacket_ReadArray(int size);

void ServerPacket_WriteString(unsigned char* packet, const char* string);
void ServerPacket_WriteByte(unsigned char* packet, unsigned char value);
void ServerPacket_WriteShort(unsigned char* packet, short value);
void ServerPacket_WriteUShort(unsigned char* packet, unsigned short value);
void ServerPacket_WriteInt(unsigned char* packet, int value);
void ServerPacket_WriteArray(unsigned char* packet, unsigned char* array, int size);

void ServerPacket_H_Identification(void);
void ServerPacket_H_SetBlock(void);
void ServerPacket_H_PlayerPosition(void);
void ServerPacket_H_Message(void);
void ServerPacket_H_SetDrawDistance(void);

unsigned char* ServerPacket_CreateMapInit(void);
unsigned char* ServerPacket_CreateLoadChunk(unsigned short* chunkArray, unsigned short length, Vector3 chunkPosition);
unsigned char* ServerPacket_CreateUnloadChunk(Vector3 chunkPosition);
unsigned char* ServerPacket_CreateSetBlock(unsigned char blockID, Vector3 position);
unsigned char* ServerPacket_CreateBlockBatch(const ServerBlockUpdate *updates, unsigned short count);
unsigned char* ServerPacket_CreateSpawnEntity(Entity *entity);
unsigned char* ServerPacket_CreateDespawnEntity(Entity *entity);
unsigned char* ServerPacket_CreateTeleportEntity(Entity *entity, Vector3 position, Vector3 rotation);
unsigned char* ServerPacket_CreateMessage(const char* message);
unsigned char* ServerPacket_CreateWorldTime(float timeSeconds);

#endif
