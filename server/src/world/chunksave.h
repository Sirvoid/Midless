/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CHUNK_SAVE_H
#define MIDLESS_CHUNK_SAVE_H

#include "entitypersistence.h"
#define CHUNK_SAVE_JOBS 64

bool ChunkSave_Init(void);
bool ChunkSave_Queue(Chunk *chunk);
bool ChunkSave_Autosave(Chunk *chunk);
bool ChunkSave_PathPending(const char *path);
// Takes ownership of snapshot only on success; never waits for disk I/O.
bool ChunkSave_QueueSnapshot(const char *path, BinaryWriter *snapshot);
void ChunkSave_Poll(void (*completed)(Chunk *, bool, const BinaryWriter *));
void ChunkSave_PollUntil(void (*completed)(Chunk *, bool, const BinaryWriter *), double deadline);
void ChunkSave_Shutdown(void);
void ChunkSave_Flush(void (*completed)(Chunk *, bool, const BinaryWriter *));

#endif
