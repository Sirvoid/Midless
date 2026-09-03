/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_PACKET_H
#define G_PACKET_H

#include "player.h"

extern unsigned char *Packet_data;
extern int PacketReader_index;

int Packet_GetLength(unsigned char opcode);

unsigned char Packet_ReadByte(void);
char Packet_ReadSByte(void);
unsigned short Packet_ReadUShort(void);
int Packet_ReadInt(void);
char* Packet_ReadString(void);
unsigned char* Packet_ReadArray(int size);

void Packet_WriteString(unsigned char* packet, char* string);
void Packet_WriteByte(unsigned char* packet, unsigned char value);
void Packet_WriteSByte(unsigned char* packet, char value);
void Packet_WriteShort(unsigned char* packet, short value);
void Packet_WriteUShort(unsigned char* packet, unsigned short value);
void Packet_WriteInt(unsigned char* packet, int value);

void Packet_H_MapInit(void);
void Packet_H_LoadChunk(void);
void Packet_H_UnloadChunk(void);
void Packet_H_SetBlock(void);
void Packet_H_SpawnEntity(void);
void Packet_H_TeleportEntity(void);
void Packet_H_DespawnEntity(void);
void Packet_H_Message(void);
void Packet_H_BlockBatch(void);
void Packet_H_WorldTime(void);

unsigned char* Packet_CreateIdentification(unsigned short version, char* name);
unsigned char* Packet_CreateSetBlock(unsigned char blockID, Vector3 position);
unsigned char* Packet_CreatePlayerPosition(Vector3 position, Vector2 rotation);
unsigned char* Packet_CreateMessage(char* message);
unsigned char *Packet_CreateSetDrawDistance(unsigned char distance);

#endif
