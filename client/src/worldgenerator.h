/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_WORLDGEN_H
#define G_WORLDGEN_H

#include "raylib.h"
#include "chunk.h"

void WorldGenerator_Init(int worldSeed);
float* WorldGenerator_Generate(Chunk *chunk);
bool WorldGenerator_GenerateStructures(Chunk *chunk, float *heightMap);

#endif