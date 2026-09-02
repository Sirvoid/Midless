/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHUNKLIGHTNING_H
#define G_CHUNKLIGHTNING_H

#include "chunk.h"

typedef struct LightQueue {
    LightNode *nodes;
    size_t head;
} LightQueue;

typedef struct LightDelQueue {
    LightDelNode *nodes;
    size_t head;
} LightDelQueue;

int Chunk_GetLight(Chunk* chunk, Vector3 pos, bool sunLight);

void Chunk_DoSunlight(Chunk *chunk);
void Chunk_DoLightSources(Chunk *srcChunk);

void Chunk_UpdateLight(LightDelQueue *delQueue, LightQueue *spreadQueue, bool sunlight);
void Chunk_SpreadLight(LightQueue *queue, bool sunlight);

void Chunk_AddLightSource(Chunk *srcChunk, Vector3 srcPos, int intensity, bool sunlight);
void Chunk_RemoveLightSource(Chunk *srcChunk, Vector3 srcPos);
void Chunk_RemoveSunlight(Chunk *srcChunk, Vector3 srcPos);

#endif