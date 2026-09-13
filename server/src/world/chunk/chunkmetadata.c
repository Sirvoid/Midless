/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "chunkmetadata.h"
#include "chunk.h"
#include <stdlib.h>
#include <string.h>

void Metadata_Free(Metadata *value) {
    free(value->data);
    *value = (Metadata){0};
}
bool Metadata_Copy(Metadata *target, const Metadata *source) {
    uint8_t *copy = source->size ? malloc(source->size) : NULL;
    if (source->size && !copy)
        return false;
    if (source->size)
        memcpy(copy, source->data, source->size);
    Metadata_Free(target);
    *target = (Metadata){copy, source->size, source->version};
    return true;
}
Metadata *ChunkMetadata_Get(Chunk *chunk, int index) {
    if (index < 0 || index >= CHUNK_SIZE || !chunk->metadataSlots)
        return NULL;
    int slot = chunk->metadataSlots[index];
    return slot ? &chunk->metadata[slot - 1].value : NULL;
}
void ChunkMetadata_Clear(Chunk *chunk, int index) {
    if (!ChunkMetadata_Get(chunk, index))
        return;
    int slot = chunk->metadataSlots[index] - 1;
    Metadata_Free(&chunk->metadata[slot].value);
    chunk->metadata[slot] = chunk->metadata[--chunk->metadataCount];
    if (slot < chunk->metadataCount)
        chunk->metadataSlots[chunk->metadata[slot].index] = slot + 1;
    chunk->metadataSlots[index] = 0;
}
bool ChunkMetadata_Reserve(Chunk *chunk) {
    if (!chunk->metadataSlots) {
        chunk->metadataSlots = calloc(CHUNK_SIZE, sizeof(*chunk->metadataSlots));
        if (!chunk->metadataSlots)
            return false;
    }
    if (chunk->metadataCount == chunk->metadataCapacity) {
        int capacity = chunk->metadataCapacity ? chunk->metadataCapacity * 2 : 8;
        BlockMetadata *grown = realloc(chunk->metadata, capacity * sizeof(*grown));
        if (!grown)
            return false;
        chunk->metadata = grown;
        chunk->metadataCapacity = capacity;
    }
    return true;
}
void ChunkMetadata_Assign(Chunk *chunk, int index, Metadata *value) {
    ChunkMetadata_Clear(chunk, index);
    if (value->size) {
        chunk->metadata[chunk->metadataCount++] = (BlockMetadata){index, *value};
        chunk->metadataSlots[index] = chunk->metadataCount;
        *value = (Metadata){0};
    }
}
bool ChunkMetadata_Set(Chunk *chunk, int index, const Metadata *value) {
    if (index < 0 || index >= CHUNK_SIZE)
        return false;
    if (!value->size) {
        ChunkMetadata_Clear(chunk, index);
        return true;
    }
    Metadata *existing = ChunkMetadata_Get(chunk, index);
    if (existing) {
        if (existing->version == value->version && existing->size == value->size &&
            !memcmp(existing->data, value->data, value->size))
            return true;
        if (!Metadata_Copy(existing, value))
            return false;
    } else {
        if (!ChunkMetadata_Reserve(chunk))
            return false;
        BlockMetadata *record = &chunk->metadata[chunk->metadataCount];
        *record = (BlockMetadata){.index = index};
        if (!Metadata_Copy(&record->value, value))
            return false;
        chunk->metadataCount++;
        chunk->metadataSlots[index] = chunk->metadataCount;
    }
    return true;
}
void ChunkMetadata_Free(Chunk *chunk) {
    for (int i = 0; i < chunk->metadataCount; i++)
        Metadata_Free(&chunk->metadata[i].value);
    free(chunk->metadata);
    free(chunk->metadataSlots);
    chunk->metadata = NULL;
    chunk->metadataSlots = NULL;
    chunk->metadataCount = chunk->metadataCapacity = 0;
}
