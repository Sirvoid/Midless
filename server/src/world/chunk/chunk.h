/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_CHUNK_H
#define MIDLESS_SERVER_CHUNK_H

#include "raylib.h"
#include "../../player.h"
#include "chunkdata.h"
#include "chunkmetadata.h"
#include "blocktimer.h"

#define CHUNK_SIZE_VEC3 CLITERAL(Vector3){ CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z }

typedef struct Chunk{
    unsigned char lightData[CHUNK_SIZE];
    unsigned char lightFlags[CHUNK_SIZE];
    bool lightDirty, lightReady, lightPriority;
    unsigned int lightRevision;
    struct Chunk *lightPrevious, *lightNext;
    struct Chunk *lightColumnNext;
    unsigned short data[CHUNK_SIZE];
    unsigned char states[CHUNK_SIZE]; // Derived cache; never saved.
    unsigned char skyMask[CHUNK_SKY_MASK_SIZE];
    Vector3 position; //Position of the chunk in chunk unit
    Vector3 blockPosition; //Position of the chunk in block unit
    bool fromFile;
    Player* *players;
    BlockMetadata *metadata;
    int metadataCount;
    BlockTimer *timers;
    int timerCount;
    unsigned char *savedEntities; // Pending records; only unavailable types remain after activation.
    unsigned int savedEntitiesSize;
    bool entitiesActivated; // Main-thread guard against loading the same records twice.
    bool loadFailed;
    bool savePending;
    bool savedOnShutdown;
} Chunk;

//Allocate and initialize a chunk.
Chunk *ServerChunk_Create(Vector3 pos);
//Unload the chunk.
void ServerChunk_Destroy(Chunk *chunk);

void ServerChunk_Decompress(Chunk *chunk, unsigned short *compressed, int compressedLength);
//Create compressed chunk data.
unsigned short* ServerChunk_CreateCompressedData(Chunk *chunk, int *compressedLength);
bool ServerChunk_SaveFile(Chunk *chunk);
bool ServerChunk_LoadFile(Chunk *chunk);
void ServerChunk_Generate(Chunk *chunk);

bool ServerChunk_PlayerInChunk(Chunk* chunk, Player* player);
void ServerChunk_AddPlayer(Chunk* chunk, Player* player);
void ServerChunk_RemovePlayer(Chunk* chunk, int index);

void ServerChunk_SetBlock(Chunk *chunk, Vector3 pos, int blockId);
int ServerChunk_GetBlock(Chunk *chunk, Vector3 pos);

bool ServerChunk_IsValidPos(Vector3 pos);
Vector3 ServerChunk_IndexToPos(int index);
int ServerChunk_PosToIndex(Vector3 pos);
long int ServerChunk_GetPackedPos(Vector3 pos);

#endif
