/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_PACKET_H
#define MIDLESS_CLIENT_PACKET_H

#include "packetsizes.h"
#include <stdint.h>

#include "player.h"

extern unsigned char *packetData;
extern int packetReaderIndex;
extern int packetDataLength;

int Packet_GetLength(unsigned char opcode);

unsigned char Packet_ReadByte(void);
uint32_t Packet_ReadUInt(void);
const unsigned char *Packet_ReadBytes(int size);
short Packet_ReadShort(void);
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

// Packet handlers, in opcode order.
void Packet_HandleMapInit(void);
void Packet_HandleLoadChunk(void);
void Packet_HandleSetBlock(void);
void Packet_HandleSpawnEntity(void);
void Packet_HandleTextColor(void);
void Packet_HandleNametag(void);
void Packet_HandleTeleportEntity(void);
void Packet_HandleMessage(void);
void Packet_HandleDespawnEntity(void);
void Packet_HandleUnloadChunk(void);
void Packet_HandleBlockBatch(void);
void Packet_HandleWorldTime(void);
void Packet_HandleMessageContinuation(void);
void Packet_HandleEntityAnimation(void);
void Packet_HandleDefineBlock(void);
void Packet_HandleRemoveBlockDefinition(void);
void Packet_HandleDefineEntityModel(void);
void Packet_HandleRemoveEntityModel(void);
void Packet_HandleSetEntityModel(void);
void Packet_HandleTextureBegin(void);
void Packet_HandleTextureData(void);
void Packet_HandleTerrainTexture(void);
void Packet_HandleHeldBlock(void);
void Packet_HandleInventoryState(void);
void Packet_HandleDroppedItem(void);
void Packet_HandleInventoryView(void);
void Packet_HandleDefineItem(void);
void Packet_HandleDigProgress(void);
void Packet_HandleBreakingTexture(void);
void Packet_HandleDefineHudBar(void);
void Packet_HandleSetHudBar(void);
void Packet_HandleRemoveHudBar(void);

unsigned char* Packet_CreateIdentification(unsigned short version, char* name);
unsigned char* Packet_CreatePlayerPosition(Vector3 position, Vector3 rotation);
unsigned char* Packet_CreateMessage(char* message);
unsigned char *Packet_CreateSetDrawDistance(unsigned char distance);
unsigned char *Packet_CreatePlayerClick(unsigned char button);

#endif
