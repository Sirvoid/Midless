/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_WORLDGEN_H
#define S_WORLDGEN_H

#include "raylib.h"
#include "chunk/chunk.h"

void ServerWorldGenerator_Init(int worldSeed);
float *ServerWorldGenerator_Generate(Chunk *chunk);
bool ServerWorldGenerator_GenerateStructures(Chunk *chunk, const float *heightMap);

#endif
