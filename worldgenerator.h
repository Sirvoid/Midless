/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_WORLDGEN_H
#define G_WORLDGEN_H

#include "raylib.h"

unsigned char *WorldGenerator_Generate(void);
Vector3 WorldGenerator_MakeBranch(unsigned char * worldData, Vector3 position, float thickness, float upwardness, float length);
void WorldGenerator_MakeLeaves(unsigned char *worldData, Vector3 position, float thickness);
void WorldGenerator_MakeTree(unsigned char *worldData, Vector3 position);

#endif