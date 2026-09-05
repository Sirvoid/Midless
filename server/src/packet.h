/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_PACKET_H
#define MIDLESS_SERVER_PACKET_H

#include "player.h"
#include "blockdefinition.h"
#include "entity.h"
#include "entityanimation.h"

typedef struct ServerBlockUpdate {
    Vector3 position;
    unsigned char blockId;
} ServerBlockUpdate;

extern unsigned char *serverPacketData;
extern Player *serverPacketPlayer;
extern int serverPacketLastDynamicLength;
extern int serverPacketReaderIndex;
extern int serverPacketDataLength;

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

void ServerPacket_HandleIdentification(void);
void ServerPacket_HandleSetBlock(void);
void ServerPacket_HandlePlayerPosition(void);
void ServerPacket_HandleMessage(void);
void ServerPacket_HandleSetDrawDistance(void);
void ServerPacket_HandlePlayerClick(void);

unsigned char* ServerPacket_CreateMapInit(void);
unsigned char *ServerPacket_CreateDefineBlock(int id, const BlockDefinition *definition);
unsigned char *ServerPacket_CreateRemoveBlockDefinition(int id);
unsigned char* ServerPacket_CreateLoadChunk(unsigned short* chunkArray, unsigned short length,
                                            Vector3 chunkPosition, const unsigned char *skyMask);
unsigned char* ServerPacket_CreateUnloadChunk(Vector3 chunkPosition);
unsigned char* ServerPacket_CreateSetBlock(unsigned char blockId, Vector3 position);
unsigned char* ServerPacket_CreateBlockBatch(const ServerBlockUpdate *updates, unsigned short count);
unsigned char* ServerPacket_CreateSpawnEntity(Entity *entity);
unsigned char* ServerPacket_CreateDespawnEntity(Entity *entity);
unsigned char* ServerPacket_CreateTeleportEntity(Entity *entity, Vector3 position, Vector3 rotation);
unsigned char* ServerPacket_CreateMessage(const char* message);
unsigned char* ServerPacket_CreateMessageContinuation(const char* message);
unsigned char* ServerPacket_CreateWorldTime(float timeSeconds);
unsigned char* ServerPacket_CreateEntityAnimation(unsigned short entityId, EntityAnimationType animation);

#endif
