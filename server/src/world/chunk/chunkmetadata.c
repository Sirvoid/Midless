#include "chunkmetadata.h"
#include "chunk.h"
#include <stdlib.h>
#include <string.h>

void Metadata_Free(Metadata *value) { free(value->data); *value = (Metadata){0}; }
bool Metadata_Copy(Metadata *target, const Metadata *source) {
    uint8_t *copy = source->size ? malloc(source->size) : NULL;
    if (source->size && !copy) return false;
    if (source->size) memcpy(copy, source->data, source->size);
    Metadata_Free(target);
    *target = (Metadata){copy, source->size, source->version};
    return true;
}
Metadata *ChunkMetadata_Get(Chunk *chunk, int index) {
    for (int i = 0; i < chunk->metadataCount; i++)
        if (chunk->metadata[i].index == index) return &chunk->metadata[i].value;
    return NULL;
}
void ChunkMetadata_Clear(Chunk *chunk, int index) {
    for (int i = 0; i < chunk->metadataCount; i++) {
        if (chunk->metadata[i].index != index) continue;
        Metadata_Free(&chunk->metadata[i].value);
        chunk->metadata[i] = chunk->metadata[--chunk->metadataCount];
        return;
    }
}
bool ChunkMetadata_Set(Chunk *chunk, int index, const Metadata *value) {
    if (index < 0 || index >= CHUNK_SIZE) return false;
    if (!value->size) { ChunkMetadata_Clear(chunk, index); return true; }
    Metadata *existing = ChunkMetadata_Get(chunk, index);
    if (existing) {
        if (existing->version == value->version && existing->size == value->size &&
            !memcmp(existing->data, value->data, value->size)) return true;
        if (!Metadata_Copy(existing, value)) return false;
    } else {
        BlockMetadata *grown = realloc(chunk->metadata, (chunk->metadataCount + 1) * sizeof(*grown));
        if (!grown) return false;
        chunk->metadata = grown;
        BlockMetadata *record = &grown[chunk->metadataCount];
        *record = (BlockMetadata){.index = index};
        if (!Metadata_Copy(&record->value, value)) return false;
        chunk->metadataCount++;
    }
    return true;
}
void ChunkMetadata_Free(Chunk *chunk) {
    for (int i = 0; i < chunk->metadataCount; i++) Metadata_Free(&chunk->metadata[i].value);
    free(chunk->metadata);
    chunk->metadata = NULL;
    chunk->metadataCount = 0;
}
