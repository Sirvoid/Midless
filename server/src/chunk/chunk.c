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
#include "worldgenerator.h"

void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    chunk->blockPosition = Vector3Multiply(chunk->position, CHUNK_SIZE_VEC3);
    chunk->fromFile = false;
    chunk->modified = false;

    if (Chunk_LoadFile(chunk)) {
        chunk->fromFile = true;
    } else {
        Chunk_Generate(chunk);
    }

}

void Chunk_Unload(Chunk *chunk) {
    if (chunk->modified) Chunk_SaveFile(chunk);
    MemFree(chunk);
}

void Chunk_SaveFile(Chunk *chunk) {
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    int newLength;
    unsigned short* compressed = Chunk_Compress(chunk, CHUNK_SIZE, &newLength);
    SaveFileData(fileName, compressed, newLength * 2);
}

bool Chunk_LoadFile(Chunk *chunk) {
    const char* fileName = TextFormat("world/%i.%i.%i.dat", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z);
    if (FileExists(fileName)) {
        unsigned int length = 0;
        unsigned char *saveFile = LoadFileData(fileName, &length);
        Chunk_Decompress(chunk, (unsigned short*)saveFile, length / 2);
        UnloadFileData(saveFile);
        return true;
    }
    return false;
}

void Chunk_Generate(Chunk *chunk) {
    if (!chunk->fromFile) {
        //Map Generation
        for (int i = CHUNK_SIZE - 1; i >= 0; i--) {
            Vector3 npos = Vector3Add(Chunk_IndexToPos(i), chunk->blockPosition);
            chunk->data[i] = WorldGenerator_Generate(chunk, npos, i);
        }
    }
}

void Chunk_Decompress(Chunk *chunk, unsigned short *compressed, int currentLength) {
    int newLength = 0;

    for (int i = 0; i < currentLength; i+=2) {
        for (int j = 0; j < compressed[i + 1]; j++) {
            chunk->data[newLength] = compressed[i];
            newLength += 1;
        } 
    }
    
}

unsigned short* Chunk_Compress(Chunk *chunk, int currentLength, int *newLength) {
    
    //BlockID:UShort, Amount:UShort, ...
    
    unsigned short *compressed = MemAlloc(currentLength * 2 * 2);
    
    int oldID = chunk->data[0];
    int bCount = 1;
    int len = 0;
    for (int i = 1; i <= currentLength; i++) {
        
        int curID = 0;
        if (i != currentLength) curID = chunk->data[i];
        
        if (oldID != curID || bCount >= USHRT_MAX || i == currentLength) {
            compressed[len++] = (unsigned short)oldID;
            compressed[len++] = (unsigned short)bCount;

            bCount = 0;
            oldID = curID;
        }
        
        bCount++;
        
    }
    
    *newLength = len;
    
    compressed = MemRealloc(compressed, *newLength * 2);
    return compressed;
}

bool Chunk_PlayerInChunk(Chunk* chunk, Player* player) {
    for (int i = 0; i < arrlen(chunk->players); i++) {
        if (chunk->players[i] == player) return true;
    }
    return false;
}

void Chunk_AddPlayer(Chunk* chunk, Player* player) {
    arrput(chunk->players, player);
}

void Chunk_RemovePlayer(Chunk* chunk, int index) {
    arrdel(chunk->players, index);
}

void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID) {
    if (Chunk_IsValidPos(pos)) {
        int index = Chunk_PosToIndex(pos);

        chunk->data[index] = blockID;
        chunk->modified = true;
    }
}

int Chunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if (Chunk_IsValidPos(pos)) {
        return chunk->data[Chunk_PosToIndex(pos)];
    }
    return 0;
}

bool Chunk_IsValidPos(Vector3 pos) {
    return pos.x >= 0 && pos.x < CHUNK_SIZE_X && pos.y >= 0 && pos.y < CHUNK_SIZE_Y && pos.z >= 0 && pos.z < CHUNK_SIZE_Z;
}

Vector3 Chunk_IndexToPos(int index) {
    int x = (index % CHUNK_SIZE_X);
	int y = (index / CHUNK_SIZE_XZ);
	int z = (index / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
    return (Vector3){x, y, z};
}

int Chunk_PosToIndex(Vector3 pos) {
    return ((int)pos.y * CHUNK_SIZE_Z + (int)pos.z) * CHUNK_SIZE_X + (int)pos.x;
}

long int Chunk_GetPackedPos(Vector3 pos) {
    return (long)((int)(pos.x)&4095)<<20 | (long)((int)(pos.z)&4095)<<8 | (long)((int)(pos.y)&255);
}