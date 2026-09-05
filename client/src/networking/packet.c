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
#include <limits.h>
#include "raylib.h"
#include "packet.h"
#include "networkhandler.h"
#include "world.h"
#include "chat.h"
#include "particle.h"
#include "block.h"

#define PACKET_STRING_SIZE 64

unsigned char *packetData;
int packetDataLength;
int Packet_Lengths[256] = {
    67, //0
    14, //1
    15, //2
    65, //3
    2, //4
    2, //5
};
int pingCalculationPreviousTime = 0;

int Packet_GetLength(unsigned char opcode) {
    return Packet_Lengths[opcode];
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Packets Readers----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
int packetReaderIndex = 1;

unsigned char Packet_ReadByte(void) {
    return packetData[packetReaderIndex++];
}

char Packet_ReadSByte(void) {
    return packetData[packetReaderIndex++];
}

short Packet_ReadShort(void) {
    short value = (short)(packetData[packetReaderIndex] << 8 | packetData[packetReaderIndex + 1]); 
    packetReaderIndex += 2;
    return value;
}

unsigned short Packet_ReadUShort(void) {
    unsigned short value = (unsigned short)(packetData[packetReaderIndex] << 8 | packetData[packetReaderIndex + 1]); 
    packetReaderIndex += 2;
    return value;
}

int Packet_ReadInt(void) {
    int value = (int)(packetData[packetReaderIndex] << 24 | packetData[packetReaderIndex + 1] << 16 | packetData[packetReaderIndex + 2] << 8 | packetData[packetReaderIndex + 3]); 
    packetReaderIndex += 4;
    return value;
}

char *Packet_ReadString(void) {
    char *string = MemAlloc(PACKET_STRING_SIZE + 1);
    if (!string) return NULL;
    
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        string[i] = packetData[packetReaderIndex++];
    }
    
    string[PACKET_STRING_SIZE] = 0;

    return string;
}


unsigned char* Packet_ReadArray(int size) {
    unsigned char *arr = MemAlloc(size);
    memcpy(arr, &packetData[packetReaderIndex], size);
    packetReaderIndex += size;
    return arr;
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Packets Writer-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
int packetWriterIndex = 0;

void Packet_WriteByte(unsigned char *packet, unsigned char value) {
    packet[packetWriterIndex++] = value;
}

void Packet_WriteSByte(unsigned char *packet, char value) {
    packet[packetWriterIndex++] = value;
}

void Packet_WriteShort(unsigned char *packet, short value) {
    packet[packetWriterIndex++] = (char)(value >> 8);
	packet[packetWriterIndex++] = (char)(value);
}

void Packet_WriteUShort(unsigned char *packet, unsigned short value) {
    packet[packetWriterIndex++] = (char)(value >> 8);
	packet[packetWriterIndex++] = (char)(value);
}

void Packet_WriteInt(unsigned char *packet, int value) {
    packet[packetWriterIndex++] = (char)(value >> 24);
	packet[packetWriterIndex++] = (char)(value >> 16);
    packet[packetWriterIndex++] = (char)(value >> 8);
    packet[packetWriterIndex++] = (char)(value);
}

void Packet_WriteString(unsigned char *packet, char *string) {
    int length = TextLength(string);
    for (int i = 0; i < PACKET_STRING_SIZE; i++) {
        if (i < length) {
            packet[packetWriterIndex++] = string[i];
        } else {
            packet[packetWriterIndex++] = 0;
        }  
    }
    
}

/*-------------------------------------------------------------------------------------------------------*
*------------------------------------------Packets Received----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void Packet_HandleMapInit(void) {
    if (packetDataLength != 3 || Packet_ReadUShort() != GAME_PROTOCOL_VERSION) {
        TraceLog(LOG_WARNING, "Incompatible server protocol; expected version %d", GAME_PROTOCOL_VERSION);
        Network_Disconnect();
        return;
    }
    World_LoadMultiplayer();
}


void Packet_HandleLoadChunk(void) {
    // Opcode + three coordinates + compressed-array length.
    const int headerLength = 1 + 3 * 4 + 2;
    if (packetDataLength < headerLength + CHUNK_SKY_MASK_SIZE) return;
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();
    int length = Packet_ReadUShort();
    // Compressed data consists of (block ID, run length) ushort pairs.
    if (length == 0 || length > CHUNK_SIZE * 2 || length % 2 != 0) return;
    int expectedLength = headerLength + length * 2 + CHUNK_SKY_MASK_SIZE;
    if (packetDataLength != expectedLength) return;

    unsigned short* chunkData = (unsigned short*)Packet_ReadArray(length * 2);
    unsigned char* skyMask = Packet_ReadArray(CHUNK_SKY_MASK_SIZE);

    int blockCount = 0;
    for (int i = 0; i < length; i += 2) {
        if (chunkData[i + 1] == 0 ||
            blockCount + chunkData[i + 1] > CHUNK_SIZE) {
            MemFree(chunkData);
            MemFree(skyMask);
            return;
        }
        // Unknown block types must not prevent the rest of the chunk loading.
        if (!Block_IsDefined(chunkData[i])) chunkData[i] = 0;
        blockCount += chunkData[i + 1];
    }
    if (blockCount != CHUNK_SIZE) {
        MemFree(chunkData);
        MemFree(skyMask);
        return;
    }
    Vector3 position = (Vector3) {x,y,z};
    World_AddChunk(position);
    Chunk* chunk = World_GetChunkAt(position);
    if (!chunk) { MemFree(chunkData); MemFree(skyMask); return; }
    Chunk_Decompress(chunk, chunkData, length);
    memcpy(chunk->skyMask, skyMask, CHUNK_SKY_MASK_SIZE);
    MemFree(chunkData);
    MemFree(skyMask);
}

void Packet_HandleUnloadChunk(void) {
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();

    Vector3 position = (Vector3) {x,y,z};
    Chunk* chunk = World_GetChunkAt(position);
    if (chunk != NULL) {
        World_RemoveChunk(chunk);
    }
}

void Packet_HandleSetBlock(void) {
    int blockId = Packet_ReadByte();
    Vector3 position = (Vector3) { Packet_ReadInt(), Packet_ReadInt(), Packet_ReadInt() };
    if (!Block_IsDefined(blockId)) return;
    int oldBlockId = World_GetBlock(position);
    if (blockId == 0 && oldBlockId != 0) Particle_SpawnBlockBreak(position, oldBlockId);
    World_SetBlock(position, blockId, false);
}

void Packet_HandleSpawnEntity(void) {
    int id = Packet_ReadUShort();
    int type = Packet_ReadByte();
    int modelId = Packet_ReadByte();
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();
    Vector3 position = (Vector3) { x / 64.0f, y / 64.0f, z / 64.0f };
    if (id == USHRT_MAX) {
        Player_SetEntityModel(type, modelId);
        Player_Teleport(position);
        return;
    }
    World_AddEntity(id, type, modelId, position, (Vector3) {0, 0, 0});
}

void Packet_HandleDespawnEntity(void) {
    int id = Packet_ReadUShort();
    World_RemoveEntity(id);
}

void Packet_HandleTeleportEntity(void) {
    int id = Packet_ReadUShort();
    int x = Packet_ReadInt();
    int y = Packet_ReadInt();
    int z = Packet_ReadInt();
    int yaw = Packet_ReadSByte();
    int pitch = Packet_ReadSByte();
    Vector3 position = (Vector3) { x / 64.0f, y / 64.0f, z / 64.0f };
    if (id == USHRT_MAX) {
        Player_Teleport(position);
        return;
    }
    World_TeleportEntity(id, position, (Vector3) {pitch / 128.0f * PI, yaw / 128.0f * PI, 0});
}

void Packet_HandleMessage(void) {
    char *message = Packet_ReadString();
    Chat_AddOwnedLine(message);
}

void Packet_HandleMessageContinuation(void) {
    char *message = Packet_ReadString();
    Chat_AppendOwnedLine(message);
}

void Packet_HandleBlockBatch(void) {
    const int headerLength = 1 + 2; // Opcode + update count.
    const int updateLength = 1 + 3 * 4; // Block ID + three coordinates.
    if (packetDataLength < headerLength) return;
    int count = Packet_ReadUShort();
    if (packetDataLength != headerLength + count * updateLength) return;
    for (int i = 0; i < count; i++) {
        int blockId = Packet_ReadByte();
        Vector3 position = {
            Packet_ReadInt(), Packet_ReadInt(), Packet_ReadInt()
        };
        if (!Block_IsDefined(blockId)) continue;
        int oldBlockId = World_GetBlock(position);
        if (blockId == 0 && oldBlockId != 0) Particle_SpawnBlockBreak(position, oldBlockId);
        World_SetBlock(position, blockId, false);
    }
}

void Packet_HandleWorldTime(void) {
    int timeMilliseconds = Packet_ReadInt();
    world.time = timeMilliseconds / 1000.0f;
}

void Packet_HandleEntityAnimation(void) {
    int id = Packet_ReadUShort();
    EntityAnimationType animation = (EntityAnimationType)Packet_ReadByte();
    World_PlayEntityAnimation(id, animation);
}

/*-------------------------------------------------------------------------------------------------------*
*--------------------------------------------Packets Sent------------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/
unsigned char *Packet_CreateIdentification(unsigned short version, char *name) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[0]);
    Packet_WriteByte(packet, 0);
    Packet_WriteUShort(packet, version);
    Packet_WriteString(packet, name);
    return packet;
}

unsigned char *Packet_CreateSetBlock(unsigned char blockId, Vector3 position) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[1]);
    Packet_WriteByte(packet, 1);
    Packet_WriteByte(packet, blockId);
    Packet_WriteInt(packet, floor(position.x));
    Packet_WriteInt(packet, floor(position.y));
    Packet_WriteInt(packet, floor(position.z));
    return packet;
}

unsigned char *Packet_CreatePlayerPosition(Vector3 position, Vector2 rotation) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[2]);
    Packet_WriteByte(packet, 2);
    Packet_WriteInt(packet, (int)(position.x * 64));
    Packet_WriteInt(packet, (int)(position.y * 64));
    Packet_WriteInt(packet, (int)(position.z * 64));
    Packet_WriteSByte(packet, round(rotation.x / PI * 128));
    Packet_WriteSByte(packet, round(rotation.y / PI * 128));
    return packet;
}

unsigned char *Packet_CreateMessage(char *message) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[3]);
    Packet_WriteByte(packet, 3);
    Packet_WriteString(packet, message);
    return packet;
}

unsigned char *Packet_CreateSetDrawDistance(unsigned char distance) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[4]);
    Packet_WriteByte(packet, 4);
    Packet_WriteByte(packet, distance);
    return packet;
}

unsigned char *Packet_CreatePlayerClick(unsigned char button) {
    packetWriterIndex = 0;
    unsigned char *packet = (unsigned char*)MemAlloc(Packet_Lengths[5]);
    Packet_WriteByte(packet, 5);
    Packet_WriteByte(packet, button);
    return packet;
}

void Packet_HandleDefineBlock(void) {
    if (packetDataLength != DEFINE_BLOCK_PACKET_SIZE) return;
    BlockDefinition definition = {0};
    int id = Packet_ReadByte();
    char *name = Packet_ReadString();
    if (!name) return;
    memcpy(definition.name, name, sizeof(definition.name));
    MemFree(name);
    for (int i = 0; i < 6; i++) definition.textures[i] = Packet_ReadByte();
    definition.modelType = Packet_ReadByte();
    definition.renderType = Packet_ReadByte();
    definition.colliderType = Packet_ReadByte();
    definition.lightType = Packet_ReadByte();
    for (int i = 0; i < 3; i++) definition.min[i] = Packet_ReadByte();
    for (int i = 0; i < 3; i++) definition.max[i] = Packet_ReadByte();
    if (!Block_ApplyDefinition(id, &definition)) {
        TraceLog(LOG_WARNING, "Rejected invalid block definition");
    }
}

void Packet_HandleRemoveBlockDefinition(void) {
    if (packetDataLength == 2) Block_RemoveDefinition(Packet_ReadByte());
}
