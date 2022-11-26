/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_WORLDGEN_H
#define S_WORLDGEN_H

#include "raylib.h"
#include "chunk.h"

void WorldGenerator_Init(int worldSeed);
int WorldGenerator_Generate(Chunk *chunk, Vector3 blockPos, int index);

#endif