#ifndef MIDLESS_CHUNK_SAVE_H
#define MIDLESS_CHUNK_SAVE_H

#include "entitypersistence.h"
#define CHUNK_SAVE_JOBS 64

bool ChunkSave_Init(void);
bool ChunkSave_Queue(Chunk *chunk);
void ChunkSave_Poll(void (*completed)(Chunk *, bool, const BinaryWriter *));
void ChunkSave_PollUntil(void (*completed)(Chunk *, bool, const BinaryWriter *), double deadline);
void ChunkSave_Shutdown(void);
void ChunkSave_Flush(void (*completed)(Chunk *, bool, const BinaryWriter *));

#endif
