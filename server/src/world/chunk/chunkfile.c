#include "version.h"
#include "chunkfile.h"
#include "../../savefile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

enum { SECTION_BLOCKS = 1, SECTION_METADATA = 2, SECTION_ENTITIES = 3, SECTION_TIMERS = 4 };

static void Section(BinaryWriter *out, int type, BinaryWriter *payload) {
    if (payload->failed) out->failed = true;
    Binary_U16(out, type);
    Binary_U16(out, CHUNK_SECTION_VERSION);
    Binary_U32(out, payload->size);
    Binary_Write(out, payload->data, payload->size);
    free(payload->data);
    *payload = (BinaryWriter){0};
}

bool ChunkFile_Encode(const Chunk *chunk, BinaryWriter *out) {
    Binary_Write(out, "MDCH", 4);
    Binary_U16(out, CHUNK_FILE_VERSION);
    Binary_U16(out, 1 + (chunk->metadataCount > 0) + (chunk->savedEntitiesSize > 0) + (chunk->timerCount > 0));
    BinaryWriter section = {0};
    int count = 0;
    unsigned short *runs = ChunkData_CreateCompressed(chunk->data, &count);
    if (!runs) return false;
    for (int i = 0; i < count; i++) Binary_U16(&section, runs[i]);
    free(runs);
    Section(out, SECTION_BLOCKS, &section);
    if (chunk->metadataCount) {
        // Store each (block type, schema version) once, including unmigrated records.
        uint16_t types[CHUNK_SIZE], versions[CHUNK_SIZE], counts[CHUNK_SIZE] = {0};
        int typeCount = 0;
        for (int i = 0; i < chunk->metadataCount; i++) {
            const BlockMetadata *record = &chunk->metadata[i];
            int type = chunk->data[record->index], entry = 0;
            while (entry < typeCount && (types[entry] != type || versions[entry] != record->value.version)) entry++;
            if (entry == typeCount) { types[entry] = type; versions[entry] = record->value.version; typeCount++; }
            counts[entry]++;
        }
        Binary_VarUInt(&section, typeCount);
        for (int entry = 0; entry < typeCount; entry++) {
            Binary_U16(&section, types[entry]);
            Binary_U16(&section, versions[entry]);
            Binary_VarUInt(&section, counts[entry]);
            for (int i = 0; i < chunk->metadataCount; i++) {
                const BlockMetadata *record = &chunk->metadata[i];
                if (chunk->data[record->index] != types[entry] || record->value.version != versions[entry]) continue;
                Binary_U16(&section, record->index);
                Binary_VarUInt(&section, record->value.size);
                Binary_Write(&section, record->value.data, record->value.size);
            }
        }
        Section(out, SECTION_METADATA, &section);
    }
    if (chunk->savedEntitiesSize) {
        Binary_Write(&section, chunk->savedEntities, chunk->savedEntitiesSize);
        Section(out, SECTION_ENTITIES, &section);
    }
    if (chunk->timerCount) {
        Binary_U16(&section, chunk->timerCount);
        for (int i = 0; i < chunk->timerCount; i++) {
            Binary_U16(&section, chunk->timers[i].index);
            Binary_Float(&section, chunk->timers[i].interval);
            Binary_Float(&section, chunk->timers[i].elapsed);
        }
        Section(out, SECTION_TIMERS, &section);
    }
    return !out->failed;
}

static bool ReadBlocks(Chunk *chunk, BinaryReader *in) {
    int output = 0;
    if (in->size % 4) return false;
    while (in->offset < in->size && !in->failed) {
        uint16_t block = Binary_ReadU16(in), count = Binary_ReadU16(in);
        if (!count || count > CHUNK_SIZE - output) return false;
        for (int i = 0; i < count; i++) chunk->data[output++] = block;
    }
    return output == CHUNK_SIZE && Binary_End(in);
}
static bool ReadMetadata(Chunk *chunk, BinaryReader *in) {
    uint32_t typeCount = Binary_ReadVarUInt(in);
    if (typeCount > CHUNK_SIZE) return false;
    for (unsigned entry = 0; entry < typeCount; entry++) {
        uint16_t type = Binary_ReadU16(in), version = Binary_ReadU16(in);
        uint32_t count = Binary_ReadVarUInt(in);
        if (in->failed || !version || !count || count > CHUNK_SIZE - chunk->metadataCount) return false;
        for (unsigned i = 0; i < count; i++) {
            int index = Binary_ReadU16(in);
            uint32_t size = Binary_ReadVarUInt(in);
            const uint8_t *data = Binary_Read(in, size);
            if (in->failed || !size || size > 65535 || index >= CHUNK_SIZE ||
                type != chunk->data[index] || ChunkMetadata_Get(chunk, index)) return false;
            Metadata value = {(uint8_t *)data, size, version};
            if (!ChunkMetadata_Set(chunk, index, &value)) return false;
        }
    }
    return Binary_End(in);
}
ChunkFileResult ChunkFile_Decode(Chunk *chunk, const void *data, size_t size) {
    // Replacing disk data beneath live entities would make the next activation
    // spawn duplicates. Unload first; the main-thread activation owns this flag.
    if (chunk->entitiesActivated) return CHUNK_FILE_CORRUPT;
    Chunk parsed = {0};
    BinaryReader in = {data, size};
    ChunkFileResult result = CHUNK_FILE_CORRUPT;
    if (size < 4 || memcmp(data, "MDCH", 4)) {
        if (!ReadBlocks(&parsed, &in)) goto done;
    } else {
        Binary_Read(&in, 4);
        if (Binary_ReadU16(&in) != CHUNK_FILE_VERSION) { result = CHUNK_FILE_UNSUPPORTED; goto done; }
        int sections = Binary_ReadU16(&in);
        bool seen[5] = {0};
        for (int i = 0; i < sections; i++) {
            int type = Binary_ReadU16(&in), version = Binary_ReadU16(&in);
            uint32_t length = Binary_ReadU32(&in);
            const uint8_t *payload = Binary_Read(&in, length);
            if (in.failed) goto done;
            // Refuse unknown sections: an older writer must never discard newer data.
            if (type < 1 || type > 4 || version != CHUNK_SECTION_VERSION) {
                result = CHUNK_FILE_UNSUPPORTED; goto done;
            }
            if (seen[type]) goto done;
            seen[type] = true;
            BinaryReader section = {payload, length};
            if (type == SECTION_BLOCKS && !ReadBlocks(&parsed, &section)) goto done;
            if (type == SECTION_METADATA && (!seen[SECTION_BLOCKS] || !ReadMetadata(&parsed, &section))) goto done;
            if (type == SECTION_TIMERS) {
                int count = Binary_ReadU16(&section);
                if (!count || count > CHUNK_SIZE) goto done;
                parsed.timers = calloc(count, sizeof(BlockTimer));
                if (!parsed.timers) goto done;
                bool occupied[CHUNK_SIZE] = {0};
                for (int t = 0; t < count; t++) {
                    BlockTimer *timer = &parsed.timers[t];
                    timer->index = Binary_ReadU16(&section);
                    timer->interval = Binary_ReadFloat(&section);
                    timer->elapsed = Binary_ReadFloat(&section);
                    if (timer->index >= CHUNK_SIZE || occupied[timer->index] ||
                        !isfinite(timer->interval) || timer->interval < 0.05f ||
                        !isfinite(timer->elapsed) || timer->elapsed < 0) goto done;
                    occupied[timer->index] = true;
                }
                if (!Binary_End(&section)) goto done;
                parsed.timerCount = count;
            }
            if (type == SECTION_ENTITIES) {
                if (!length) goto done;
                parsed.savedEntities = malloc(length);
                if (!parsed.savedEntities) goto done;
                memcpy(parsed.savedEntities, payload, length);
                parsed.savedEntitiesSize = length;
            }
        }
        if (!seen[SECTION_BLOCKS] || !Binary_End(&in)) goto done;
    }
    memcpy(chunk->data, parsed.data, sizeof(chunk->data));
    ChunkMetadata_Free(chunk);
    free(chunk->savedEntities);
    chunk->metadata = parsed.metadata;
    chunk->metadataCount = parsed.metadataCount;
    parsed.metadata = NULL; parsed.metadataCount = 0;
    result = CHUNK_FILE_OK;
done:
    if (result == CHUNK_FILE_OK) {
        free(chunk->timers);
        chunk->timers = parsed.timers; chunk->timerCount = parsed.timerCount;
        parsed.timers = NULL;
        chunk->savedEntities = parsed.savedEntities;
        chunk->savedEntitiesSize = parsed.savedEntitiesSize;
        parsed.savedEntities = NULL;
    }
    ChunkMetadata_Free(&parsed);
    free(parsed.savedEntities);
    free(parsed.timers);
    return result;
}

static void Filename(const Chunk *chunk, char path[160], const char *suffix) {
    snprintf(path, 160, "world/%i.%i.%i.dat%s", (int)chunk->position.x, (int)chunk->position.y, (int)chunk->position.z, suffix);
}
ChunkFileResult ChunkFile_Load(Chunk *chunk) {
    char path[160]; Filename(chunk, path, "");
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? CHUNK_FILE_MISSING : CHUNK_FILE_CORRUPT;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return CHUNK_FILE_CORRUPT; }
    long size = ftell(file);
    if (size <= 0 || size > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET)) { fclose(file); return CHUNK_FILE_CORRUPT; }
    void *data = malloc(size);
    bool read = data && fread(data, 1, size, file) == (size_t)size;
    fclose(file);
    ChunkFileResult result = read ? ChunkFile_Decode(chunk, data, size) : CHUNK_FILE_CORRUPT;
    free(data);
    return result;
}
bool ChunkFile_Save(Chunk *chunk) {
    BinaryWriter out = {0};
    char path[160]; Filename(chunk, path, "");
    bool ok = !chunk->loadFailed && ChunkFile_Encode(chunk, &out) && SaveFile_WriteAtomic(path, out.data, out.size);
    free(out.data);
    if (!ok) TraceLog(LOG_ERROR, "Could not save chunk %s; original file retained", path);
    return ok;
}
