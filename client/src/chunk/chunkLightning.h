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

void Chunk_LightQueueAdd(int index, Chunk *chunk);
void Chunk_LightDelQueueAdd(int index, int val, Chunk *chunk);
void Chunk_LightQueuePop(void);
void Chunk_LightDelQueuePop(void);

void Chunk_DoSunlight(Chunk *chunk);
void Chunk_DoLightSources(Chunk *srcChunk);

void Chunk_UpdateLight(bool sunlight);
void Chunk_SpreadLight(bool sunlight);

void Chunk_AddLightSource(Chunk *srcChunk, Vector3 srcPos, int intensity, bool sunlight);
void Chunk_RemoveLightSource(Chunk *srcChunk, Vector3 srcPos);
void Chunk_RemoveSunlight(Chunk *srcChunk, Vector3 srcPos);

#endif