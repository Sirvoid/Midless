/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHUNKLIGHTNING_H
#define G_CHUNKLIGHTNING_H

#include "chunk.h"

int Chunk_GetLight(Chunk* chunk, Vector3 pos);

LightNode *Chunk_LightQueueAdd(LightNode *queue, int index, Chunk *chunk);
LightDelNode *Chunk_LightDelQueueAdd(LightDelNode *queue, int index, int val, Chunk *chunk);
LightNode *Chunk_LightQueuePop(LightNode *lightQueue);
LightDelNode *Chunk_LightDelQueuePop(LightDelNode *lightDelQueue);

void Chunk_DoSunlight(Chunk *chunk);
void Chunk_DoLightSources(Chunk *srcChunk);

void Chunk_UpdateLight(LightNode *lightQueue, LightDelNode *lightDelQueue, bool sunlight);
LightNode *Chunk_SpreadLight(LightNode *lightQueue, bool sunlight);

void Chunk_AddLightSource(Chunk *srcChunk, Vector3 srcPos, int intensity, bool sunlight);
void Chunk_RemoveLightSource(Chunk *srcChunk, Vector3 srcPos);
void Chunk_RemoveSunlight(Chunk *srcChunk, Vector3 srcPos);

#endif