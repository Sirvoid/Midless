/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CHUNK_MESH_WORKER_H
#define MIDLESS_CHUNK_MESH_WORKER_H

#include "chunk.h"

void ChunkMeshWorker_Init(void);
void ChunkMeshWorker_Shutdown(void);
void ChunkMeshWorker_CancelQueued(void);
bool ChunkMeshWorker_HasSpace(void);
bool ChunkMeshWorker_Submit(Chunk *chunk);
void ChunkMeshWorker_Receive(double deadline);
bool ChunkMeshWorker_ReceiveOne(void);

#endif
