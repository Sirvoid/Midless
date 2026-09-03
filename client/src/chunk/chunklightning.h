/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_CHUNK_LIGHTING_H
#define MIDLESS_CLIENT_CHUNK_LIGHTING_H

#include "chunk.h"

typedef struct LightQueue {
    LightNode *nodes;
    size_t head;
} LightQueue;

typedef struct LightRemovalQueue {
    LightRemovalNode *nodes;
    size_t head;
} LightRemovalQueue;

int Chunk_GetLight(Chunk* chunk, Vector3 pos, bool sunlight);

void Chunk_DoSunlight(Chunk *chunk);
void Chunk_DoLightSources(Chunk *sourceChunk);

void Chunk_UpdateLight(LightRemovalQueue *delQueue, LightQueue *spreadQueue, bool sunlight);
void Chunk_SpreadLight(LightQueue *queue, bool sunlight);

void Chunk_AddLightSource(Chunk *sourceChunk, Vector3 sourcePosition, int intensity, bool sunlight);
void Chunk_RemoveLightSource(Chunk *sourceChunk, Vector3 sourcePosition);
void Chunk_RemoveSunlight(Chunk *sourceChunk, Vector3 sourcePosition);

#endif