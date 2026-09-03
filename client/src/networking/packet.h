/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_PACKET_H
#define MIDLESS_CLIENT_PACKET_H

#include "player.h"

extern unsigned char *packetData;
extern int packetReaderIndex;

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

void Packet_HandleMapInit(void);
void Packet_HandleLoadChunk(void);
void Packet_HandleUnloadChunk(void);
void Packet_HandleSetBlock(void);
void Packet_HandleSpawnEntity(void);
void Packet_HandleTeleportEntity(void);
void Packet_HandleDespawnEntity(void);
void Packet_HandleMessage(void);
void Packet_HandleMessageContinuation(void);
void Packet_HandleBlockBatch(void);
void Packet_HandleWorldTime(void);

unsigned char* Packet_CreateIdentification(unsigned short version, char* name);
unsigned char* Packet_CreateSetBlock(unsigned char blockId, Vector3 position);
unsigned char* Packet_CreatePlayerPosition(Vector3 position, Vector2 rotation);
unsigned char* Packet_CreateMessage(char* message);
unsigned char *Packet_CreateSetDrawDistance(unsigned char distance);

#endif
