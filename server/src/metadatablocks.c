/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "metadatainternal.h"
#include "blockstates.h"
#include "scripthooks.h"
#include "world/world.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static Chunk *FindBlock(Vector3 position, int *index) {
    Vector3 chunkPosition = {floorf(position.x / 16), floorf(position.y / 16),
                             floorf(position.z / 16)};
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (!chunk)
        return NULL;
    *index = ChunkData_PositionToIndex((int)(position.x - chunkPosition.x * 16),
                                       (int)(position.y - chunkPosition.y * 16),
                                       (int)(position.z - chunkPosition.z * 16));
    return chunk->data[*index] < 256 ? chunk : NULL;
}

static const MetadataField *LocateField(Chunk *chunk, int index, const char *name, MetadataSchema **schema,
                                BinaryReader *reader, MetadataBits *bits) {
    int id = serverBlockSchemas[chunk->data[index]];
    if (id < 0 || id >= serverMetadataSchemaCount)
        return NULL;
    *schema = serverMetadataSchemas[id];
    const Metadata *value = ChunkMetadata_Get(chunk, index);
    if (value && value->size && value->version != (*schema)->version)
        return NULL;
    if (!value || !value->size)
        value = &(*schema)->defaults;
    *reader = (BinaryReader){value->data, value->size};
    *bits = (MetadataBits){0};
    for (int i = 0; i < (*schema)->count; i++) {
        const MetadataField *field = &(*schema)->fields[i];
        if (!strcmp(field->name, name))
            return field;
        if (!MetadataCodec_SkipField(field, reader, bits))
            return NULL;
    }
    return NULL;
}

bool ServerMetadata_BlockInventory(Vector3 position, const char *name, ItemStack *slots, int count,
                                   bool write) {
    if (!slots || count < 1 || count > 255)
        return false;
    int index;
    Chunk *chunk = FindBlock(position, &index);
    if (!chunk)
        return false;
    MetadataSchema *schema;
    BinaryReader reader;
    MetadataBits bits;
    const MetadataField *field = LocateField(chunk, index, name, &schema, &reader, &bits);
    if (!field || field->type != FIELD_INVENTORY || field->limit != count)
        return false;
    size_t start = reader.offset;
    if (!MetadataCodec_ReadField(field, &reader, &bits))
        return false;
    if (!write) {
        memset(slots, 0, count * sizeof(*slots));
        BinaryReader inventory = {reader.data + start, reader.offset - start};
        int occupied = Binary_ReadU8(&inventory);
        for (int i = 0; i < occupied; i++) {
            int slot = Binary_ReadU8(&inventory);
            slots[slot] = ItemStack_Read(&inventory);
        }
        return true;
    }

    BinaryWriter output = {0};
    Binary_Write(&output, reader.data, start);
    int occupied = 0;
    for (int i = 0; i < count; i++) {
        if (slots[i].count)
            occupied++;
    }
    Binary_U8(&output, occupied);
    for (int i = 0; i < count; i++) {
        if (!slots[i].count)
            continue;
        if (!slots[i].itemId || slots[i].count > Item_GetMaxStack(slots[i].itemId)) {
            free(output.data);
            return false;
        }
        Binary_U8(&output, i);
        ItemStack_Write(&output, slots[i]);
    }
    Binary_Write(&output, reader.data + reader.offset, reader.size - reader.offset);
    if (output.failed || output.size > 65535) {
        free(output.data);
        return false;
    }
    Metadata value = {output.data, output.size, schema->version};
    bool defaults = output.size == schema->defaults.size &&
                    !memcmp(output.data, schema->defaults.data, output.size);
    chunk->states[index] = ServerBlockStates_Resolve(chunk, index);
    bool success = true;
    if (defaults)
        ChunkMetadata_Clear(chunk, index);
    else
        success = ChunkMetadata_Set(chunk, index, &value);
    free(output.data);
    if (!success)
        return false;
    ServerBlockStates_Changed(chunk, index);
    ScriptHooks_MetadataInventoryChanged(position, name);
    return true;
}

bool ServerMetadata_Progress(Vector3 position, const char *name, float *value) {
    int index;
    Chunk *chunk = FindBlock(position, &index);
    if (!chunk)
        return false;
    MetadataSchema *schema;
    BinaryReader reader;
    MetadataBits bits;
    const MetadataField *field = LocateField(chunk, index, name, &schema, &reader, &bits);
    if (!field || field->type == FIELD_BOOL || field->type > FIELD_FLOAT)
        return false;
    if (field->type == FIELD_FLOAT) {
        *value = Binary_ReadFloat(&reader);
    } else {
        uint32_t number = MetadataCodec_ReadBits(&reader, &bits, field->bits);
        int64_t decoded = number;
        if (field->type == FIELD_INT && (number & (1u << (field->bits - 1))))
            decoded -= (int64_t)1 << field->bits;
        *value = decoded;
    }
    return !reader.failed && isfinite(*value);
}
