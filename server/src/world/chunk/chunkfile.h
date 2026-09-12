/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CHUNK_FILE_H
#define MIDLESS_CHUNK_FILE_H
#include "chunk.h"
#include "binarydata.h"
typedef enum ChunkFileResult { CHUNK_FILE_MISSING, CHUNK_FILE_OK, CHUNK_FILE_CORRUPT, CHUNK_FILE_UNSUPPORTED } ChunkFileResult;
ChunkFileResult ChunkFile_Decode(Chunk *chunk, const void *data, size_t size);
bool ChunkFile_Encode(const Chunk *chunk, BinaryWriter *out);
ChunkFileResult ChunkFile_Load(Chunk *chunk);
bool ChunkFile_Save(Chunk *chunk);
#endif
