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
#include "../worldgenerator.h"

static void ServerChunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->fromFile = false;
    chunk->modified = false;
    chunk->players = NULL;
    memset(chunk->data, 0, sizeof(chunk->data));

    if (ServerChunk_LoadFile(chunk)) {
        chunk->fromFile = true;
    }

}

Chunk *ServerChunk_Create(Vector3 pos) {
    Chunk *chunk = MemAlloc(sizeof(*chunk));
    if (chunk == NULL) return NULL;

    ServerChunk_Init(chunk, pos);
    return chunk;
}

void ServerChunk_Destroy(Chunk *chunk) {
    if (chunk == NULL) return;

    arrfree(chunk->players);
    MemFree(chunk);
}

void ServerChunk_SaveFile(Chunk *chunk) {
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    int compressedLength;
    unsigned short* compressed = ServerChunk_CreateCompressedData(chunk, &compressedLength);
    SaveFileData(fileName, compressed, compressedLength * 2);
    MemFree(compressed);
}

bool ServerChunk_LoadFile(Chunk *chunk) {
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    if (FileExists(fileName)) {
        unsigned int length = 0;
        unsigned char *saveFile = LoadFileData(fileName, &length);
        ServerChunk_Decompress(chunk, (unsigned short*)saveFile, length / 2);
        UnloadFileData(saveFile);
        return true;
    }
    return false;
}

void ServerChunk_Generate(Chunk *chunk) {
    if (!chunk->fromFile) {
        float *heightMap = ServerWorldGenerator_Generate(chunk);
        ServerWorldGenerator_GenerateStructures(chunk, heightMap);
    }
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

        chunk->data[index] = blockId;
        chunk->modified = true;
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
