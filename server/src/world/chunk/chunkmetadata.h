/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CHUNK_METADATA_H
#define MIDLESS_CHUNK_METADATA_H
#include <stdint.h>
#include <stdbool.h>

typedef struct Metadata { uint8_t *data; uint32_t size; uint16_t version; } Metadata;
typedef struct BlockMetadata { uint16_t index; Metadata value; } BlockMetadata;
struct Chunk;
Metadata *ChunkMetadata_Get(struct Chunk *chunk, int index);
bool ChunkMetadata_Set(struct Chunk *chunk, int index, const Metadata *value);
void ChunkMetadata_Clear(struct Chunk *chunk, int index);
void ChunkMetadata_Free(struct Chunk *chunk);
bool Metadata_Copy(Metadata *target, const Metadata *source);
void Metadata_Free(Metadata *value);
#endif
