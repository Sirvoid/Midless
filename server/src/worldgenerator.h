/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_WORLD_GENERATOR_H
#define MIDLESS_SERVER_WORLD_GENERATOR_H

#include "raylib.h"
#include "chunk/chunk.h"

void ServerWorldGenerator_Init(int worldSeed);
float *ServerWorldGenerator_Generate(Chunk *chunk);
bool ServerWorldGenerator_GenerateStructures(Chunk *chunk, const float *heightMap);

#endif
