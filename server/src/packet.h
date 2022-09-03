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

extern unsigned char *Packet_data;
extern Player *Packet_player;
extern int Packet_LastDynamicLength;
extern int PacketReader_index;

int Packet_GetLength(unsigned char opcode);

unsigned char Packet_ReadByte(void);
unsigned short Packet_ReadUShort(void);
char* Packet_ReadString(void);
unsigned char* Packet_ReadArray(int size);

void Packet_WriteString(unsigned char* packet, const char* string);
void Packet_WriteByte(unsigned char* packet, unsigned char value);
void Packet_WriteShort(unsigned char* packet, short value);
void Packet_WriteUShort(unsigned char* packet, unsigned short value);
void Packet_WriteInt(unsigned char* packet, int value);
void Packet_WriteArray(unsigned char* packet, unsigned char* array, int size);

void Packet_H_Identification(void);
void Packet_H_SetBlock(void);
void Packet_H_PlayerPosition(void);
void Packet_H_Message(void);
void Packet_H_SetDrawDistance(void);

unsigned char* Packet_MapInit(void);
unsigned char* Packet_LoadChunk(unsigned short* chunkArray, unsigned short length, Vector3 chunkPosition);
unsigned char* Packet_UnloadChunk(Vector3 chunkPosition);
unsigned char* Packet_SetBlock(unsigned char blockID, Vector3 position);
unsigned char* Packet_SpawnEntity(Entity *entity);
unsigned char* Packet_DespawnEntity(Entity *entity);
unsigned char* Packet_TeleportEntity(Entity *entity, Vector3 position, Vector3 rotation);
unsigned char* Packet_SendMessage(const char* message);

#endif