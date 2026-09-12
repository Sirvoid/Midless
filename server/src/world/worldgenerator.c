/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if !defined(MIDLESS_FNL_EXTERNAL)
#define FNL_IMPL
#endif
#include "FastNoiseLite.h"
#include "worldgenerator.h"
#include "worldgen.h"

void ServerWorldGenerator_Init(int seed) {
    Worldgen_Reset(seed);
}

void ServerWorldGenerator_Generate(Chunk *chunk) {
    Worldgen_Generate(chunk);
}

bool ServerWorldGenerator_GenerateStructures(Chunk *chunk) {
    (void)chunk;
    return worldgen.structureCount > 0 || worldgen.featureCount > 0;
}

void ServerWorldGenerator_GenerateSkyMask(Chunk *chunk) {
    Worldgen_SkyMask(chunk);
}
