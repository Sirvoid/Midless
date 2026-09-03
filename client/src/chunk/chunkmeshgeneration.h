/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */


#ifndef G_CHUNKMESHGEN_H
#define G_CHUNKMESHGEN_H

#include "chunk.h"
#include "block.h"

void Chunk_MeshGenerationInit(void);
void Chunk_MeshGenerationShutdown(void);
//Build/Refresh a chunk's mesh.
void Chunk_BuildMesh(Chunk *chunk);
bool Chunk_TestOpaque(const Block *blockDef, const Block *nextDef);
bool Chunk_TestTranslucent(const Block *blockDef, const Block *nextDef);

#endif
