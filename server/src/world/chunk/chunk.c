/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "stb_ds.h"
#include "raylib.h"
#include "raymath.h"
#include "chunk.h"
#include "chunkfile.h"
#include "../worldgenerator.h"

static void ServerChunk_Init(Chunk *chunk, Vector3 pos) {
    *chunk = (Chunk){0};
    chunk->position = pos;
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->fromFile = false;
    chunk->players = NULL;
    memset(chunk->data, 0, sizeof(chunk->data));
    memset(chunk->skyMask, 0, sizeof(chunk->skyMask));

    if (ServerChunk_LoadFile(chunk)) {
        chunk->fromFile = true;
    }

}

Chunk *ServerChunk_Create(Vector3 pos) {
    Chunk *chunk = MemAlloc(sizeof(*chunk));
    if (chunk == NULL) return NULL;

    ServerChunk_Init(chunk, pos);
    if (chunk->loadFailed) { ServerChunk_Destroy(chunk); return NULL; }
    return chunk;
}

void ServerChunk_Destroy(Chunk *chunk) {
    if (chunk == NULL) return;

    arrfree(chunk->players);
    ChunkMetadata_Free(chunk);
    free(chunk->savedEntities);
    free(chunk->timers);
    MemFree(chunk);
}

bool ServerChunk_SaveFile(Chunk *chunk) { return ChunkFile_Save(chunk); }

bool ServerChunk_LoadFile(Chunk *chunk) {
    ChunkFileResult result = ChunkFile_Load(chunk);
    chunk->loadFailed = result == CHUNK_FILE_CORRUPT || result == CHUNK_FILE_UNSUPPORTED;
    if (chunk->loadFailed) TraceLog(LOG_ERROR, "Chunk (%g,%g,%g) is corrupt or unsupported; it will not be regenerated",
        chunk->position.x, chunk->position.y, chunk->position.z);
    return result == CHUNK_FILE_OK;
}

void ServerChunk_Generate(Chunk *chunk) {
    if (!chunk->fromFile) {
        ServerWorldGenerator_Generate(chunk);
        ServerWorldGenerator_GenerateStructures(chunk);
    }
    ServerWorldGenerator_GenerateSkyMask(chunk);
}

void ServerChunk_Decompress(Chunk *chunk, unsigned short *compressed, int compressedLength) {
    ChunkData_Decompress(chunk->data, compressed, compressedLength);
}

unsigned short* ServerChunk_CreateCompressedData(Chunk *chunk, int *compressedLength) {
    return ChunkData_CreateCompressed(chunk->data, compressedLength);
}

bool ServerChunk_PlayerInChunk(Chunk* chunk, Player* player) {
    for (int i = 0; i < arrlen(chunk->players); i++) {
        if (chunk->players[i] == player) return true;
    }
    return false;
}

void ServerChunk_AddPlayer(Chunk* chunk, Player* player) {
    arrput(chunk->players, player);
}

void ServerChunk_RemovePlayer(Chunk* chunk, int index) {
    arrdel(chunk->players, index);
}

void ServerChunk_SetBlock(Chunk *chunk, Vector3 pos, int blockId) {
    if (ServerChunk_IsValidPos(pos)) {
        int index = ServerChunk_PosToIndex(pos);

        if (chunk->data[index] != blockId) {
            ChunkMetadata_Clear(chunk, index);
            BlockTimer_Stop(chunk, index);
        }
        chunk->data[index] = blockId;
    }
}

int ServerChunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if (ServerChunk_IsValidPos(pos)) {
        return chunk->data[ServerChunk_PosToIndex(pos)];
    }
    return 0;
}

bool ServerChunk_IsValidPos(Vector3 pos) {
    return ChunkData_IsValidPosition((int)pos.x, (int)pos.y, (int)pos.z);
}

Vector3 ServerChunk_IndexToPos(int index) {
    int x, y, z;
    ChunkData_IndexToPosition(index, &x, &y, &z);
    return (Vector3){x, y, z};
}

int ServerChunk_PosToIndex(Vector3 pos) {
    return ChunkData_PositionToIndex((int)pos.x, (int)pos.y, (int)pos.z);
}

long int ServerChunk_GetPackedPos(Vector3 pos) {
    return ChunkData_PackPosition((int)pos.x, (int)pos.y, (int)pos.z);
}
