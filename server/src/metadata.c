/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "metadatainternal.h"
#include "world/world.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

MetadataSchema *serverMetadataSchemas[METADATA_MAX_SCHEMAS];
int serverMetadataSchemaCount, serverBlockSchemas[256];
int serverItemSchemas[ITEM_LIMIT];

bool MetadataCodec_ReadField(const MetadataField *field, BinaryReader *in, MetadataBits *bits) {
    if (field->type <= FIELD_BOOL) {
        MetadataCodec_ReadBits(in, bits, field->bits);

    } else {
        if (bits->byte)
            return false;
        *bits = (MetadataBits){0};
        if (field->type == FIELD_FLOAT) {
            float number = Binary_ReadFloat(in);
            if (!isfinite(number))
                return false;

        } else if (field->type == FIELD_STRING) {
            uint32_t size = Binary_ReadVarUInt(in);
            Binary_Read(in, size);
            if (in->failed || size > (unsigned)field->limit)
                return false;

        } else {
            int count = Binary_ReadU8(in), previous = -1;
            if (count > field->limit)
                return false;

            for (int slot = 0; slot < count; slot++) {
                int index = Binary_ReadU8(in);
                ItemStack stack = ItemStack_Read(in);
                if (in->failed || index <= previous || index >= field->limit || !stack.count)
                    return false;
                previous = index;
            }
        }
    }
    return !in->failed;
}

void MetadataCodec_WriteBits(BinaryWriter *out, MetadataBits *bits, uint32_t value, int count) {
    for (int i = 0; i < count; i++) {
        bits->byte |= ((value >> i) & 1) << bits->count;
        if (++bits->count == 8) {
            Binary_U8(out, bits->byte);
            *bits = (MetadataBits){0};
        }
    }
}

void MetadataCodec_FlushBits(BinaryWriter *out, MetadataBits *bits) {
    if (bits->count)
        Binary_U8(out, bits->byte);
    *bits = (MetadataBits){0};
}

uint32_t MetadataCodec_ReadBits(BinaryReader *in, MetadataBits *bits, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; i++) {
        if (!bits->count) {
            bits->byte = Binary_ReadU8(in);
            bits->count = 8;
        }
        value |= (uint32_t)(bits->byte & 1) << i;
        bits->byte >>= 1;
        bits->count--;
    }
    return value;
}

void MetadataCodec_WriteDefault(BinaryWriter *out, MetadataBits *bits, const MetadataField *field) {
    if (field->type <= FIELD_BOOL)
        MetadataCodec_WriteBits(out, bits, (uint32_t)(int64_t)field->defaultNumber, field->bits);
    else {
        MetadataCodec_FlushBits(out, bits);
        if (field->type == FIELD_FLOAT)
            Binary_Float(out, field->defaultNumber);
        else if (field->type == FIELD_STRING) {
            size_t size = strlen(field->defaultString);
            Binary_VarUInt(out, size);
            Binary_Write(out, field->defaultString, size);
        } else
            Binary_U8(out, 0);
    }
}

bool MetadataCodec_BuildDefaults(MetadataSchema *schema) {
    BinaryWriter out = {0};
    MetadataBits bits = {0};
    for (int i = 0; i < schema->count; i++)
        MetadataCodec_WriteDefault(&out, &bits, &schema->fields[i]);
    MetadataCodec_FlushBits(&out, &bits);
    if (out.failed || out.size > 65535) {
        free(out.data);
        return false;
    }
    schema->defaults = (Metadata){out.data, out.size, schema->version};
    return true;
}

bool ServerMetadata_CanRead(int id, const Metadata *value) {
    return !value->size || (id >= 0 && id < serverMetadataSchemaCount && value->version == serverMetadataSchemas[id]->version);
}

bool ServerMetadata_Validate(int id, const Metadata *value) {
    // Unknown layouts stay opaque. Known layouts are checked without invoking Lua.
    if (!value->size || !ServerMetadata_CanRead(id, value))
        return true;
    MetadataSchema *schema = serverMetadataSchemas[id];
    BinaryReader in = {value->data, value->size};
    MetadataBits bits = {0};
    for (int i = 0; i < schema->count; i++)
        if (!MetadataCodec_ReadField(&schema->fields[i], &in, &bits))
            return false;
    return Binary_End(&in) && !bits.byte;
}

bool ServerMetadata_ValidateChunk(Chunk *chunk) {
    for (int i = 0; i < chunk->metadataCount; i++) {
        BlockMetadata *record = &chunk->metadata[i];
        int type = chunk->data[record->index];
        if (type < 256 && !ServerMetadata_Validate(serverBlockSchemas[type], &record->value))
            return false;
    }
    return true;
}

int ServerMetadata_CollectBlockItems(Vector3 position, ItemStack *stacks, int capacity) {
    Vector3 chunkPosition = {floorf(position.x / 16), floorf(position.y / 16),
                             floorf(position.z / 16)};
    Chunk *chunk = ServerWorld_GetChunkAt(chunkPosition);
    if (!chunk)
        return -1;
    int index = ChunkData_PositionToIndex((int)(position.x - chunkPosition.x * 16),
                                          (int)(position.y - chunkPosition.y * 16),
                                          (int)(position.z - chunkPosition.z * 16));
    Metadata *value = ChunkMetadata_Get(chunk, index);
    if (!value || !value->size)
        return 0; // Inventories default to empty.
    int id = chunk->data[index] < 256 ? serverBlockSchemas[chunk->data[index]] : -1;
    if (!ServerMetadata_CanRead(id, value))
        return -1;
    MetadataSchema *schema = serverMetadataSchemas[id];
    BinaryReader in = {value->data, value->size};
    MetadataBits bits = {0};
    int total = 0;
    for (int i = 0; i < schema->count; i++) {
        const MetadataField *field = &schema->fields[i];
        BinaryReader inventory = in;
        if (!MetadataCodec_ReadField(field, &in, &bits))
            return -1;
        if (field->type != FIELD_INVENTORY)
            continue;
        int count = Binary_ReadU8(&inventory);
        if (count > capacity - total)
            return -1;
        for (int slot = 0; slot < count; slot++) {
            Binary_ReadU8(&inventory);
            stacks[total++] = ItemStack_Read(&inventory);
        }
    }
    return Binary_End(&in) && !bits.byte ? total : -1;
}

bool MetadataCodec_SkipField(const MetadataField *field, BinaryReader *in, MetadataBits *bits) {
    if (field->type <= FIELD_BOOL)
        MetadataCodec_ReadBits(in, bits, field->bits);
    else {
        *bits = (MetadataBits){0};
        if (field->type == FIELD_FLOAT)
            Binary_Read(in, 4);
        else if (field->type == FIELD_STRING) {
            uint32_t length = Binary_ReadVarUInt(in);
            Binary_Read(in, length);
        } else {
            unsigned count = Binary_ReadU8(in);
            for (unsigned i = 0; i < count; i++) {
                Binary_ReadU8(in);
                ItemStack_Read(in);
            }
        }
    }
    return !in->failed;
}

bool ServerMetadata_StateField(int id, const char *name, bool *boolean, int *bits) {
    if (id < 0 || id > 255 || serverBlockSchemas[id] < 0 || serverBlockSchemas[id] >= serverMetadataSchemaCount)
        return false;
    MetadataSchema *schema = serverMetadataSchemas[serverBlockSchemas[id]];
    for (int i = 0; i < schema->count; i++)
        if (!strcmp(schema->fields[i].name, name)) {
            MetadataField *f = &schema->fields[i];
            if (f->type > FIELD_BOOL)
                return false;
            *boolean = f->type == FIELD_BOOL;
            *bits = f->bits;
            return true;
        }
    return false;
}

bool ServerMetadata_StateValue(int id, const Metadata *value, const char *name, int64_t *out) {
    if (id < 0 || id > 255 || serverBlockSchemas[id] < 0 || serverBlockSchemas[id] >= serverMetadataSchemaCount)
        return false;
    MetadataSchema *schema = serverMetadataSchemas[serverBlockSchemas[id]];
    MetadataBits bits = {0};
    const Metadata *source =
        value && value->size && value->version == schema->version ? value : &schema->defaults;
    BinaryReader in = {source->data, source->size};
    for (int i = 0; i < schema->count; i++) {
        MetadataField *f = &schema->fields[i];
        if (!strcmp(f->name, name)) {
            if (f->type > FIELD_BOOL)
                return false;
            uint32_t number = MetadataCodec_ReadBits(&in, &bits, f->bits);
            if (in.failed) {
                *out = (int64_t)f->defaultNumber;
                return true;
            }
            int64_t decoded = number;
            if (f->type == FIELD_INT && (number & (1u << (f->bits - 1))))
                decoded -= (int64_t)1 << f->bits;
            *out = decoded;
            return true;
        }
        if (!MetadataCodec_SkipField(f, &in, &bits))
            return false;
    }
    return false;
}

int ServerMetadata_Register(const MetadataSchema *layout) {
    if (serverMetadataSchemaCount == METADATA_MAX_SCHEMAS)
        return -1;
    MetadataSchema *schema = calloc(1, sizeof(*schema));
    if (!schema)
        return -1;
    *schema = *layout;
    if (!MetadataCodec_BuildDefaults(schema)) {
        free(schema);
        return -1;
    }
    serverMetadataSchemas[serverMetadataSchemaCount] = schema;
    return serverMetadataSchemaCount++;
}

void ServerMetadata_Reset(void) {
    for (int i = 0; i < serverMetadataSchemaCount; i++) {
        Metadata_Free(&serverMetadataSchemas[i]->defaults);
        free(serverMetadataSchemas[i]);
        serverMetadataSchemas[i] = NULL;
    }
    serverMetadataSchemaCount = 0;
    for (int i = 0; i < 256; i++)
        serverBlockSchemas[i] = -1;
    for (int i = 0; i < ITEM_LIMIT; i++)
        serverItemSchemas[i] = -1;
}
