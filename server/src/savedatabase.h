/** Copyright (c) 2026 Sirvoid. SPDX-License-Identifier: MIT */
#ifndef MIDLESS_SAVE_DATABASE_H
#define MIDLESS_SAVE_DATABASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum SaveResult { SAVE_ERROR = -1, SAVE_MISSING = 0, SAVE_OK = 1 } SaveResult;
typedef struct SavePosition {
    int x, y, z;
} SavePosition;
typedef struct SavedItem {
    int id;
    char identifier[65];
} SavedItem;
typedef struct SavedGenerator {
    char name[65];
    int version;
    uint32_t fingerprint;
} SavedGenerator;

typedef enum SaveKind { SAVE_CHUNK, SAVE_PLAYER } SaveKind;
typedef struct SaveWrite {
    SaveKind kind;
    SavePosition position;
    const char *player;
    const void *data;
    size_t size;
} SaveWrite;

bool SaveDatabase_Open(void);
void SaveDatabase_Close(void);
SaveResult SaveDatabase_LoadChunk(SavePosition position, unsigned char **data, size_t *size);
bool SaveDatabase_SaveChunk(SavePosition position, const void *data, size_t size);
SaveResult SaveDatabase_LoadPlayer(const char *name, unsigned char **data, size_t *size);
bool SaveDatabase_SavePlayer(const char *name, const void *data, size_t size);
// The whole batch commits or rolls back. Payloads remain owned by the caller.
bool SaveDatabase_WriteBatch(const SaveWrite *writes, int count);
bool SaveDatabase_LoadItems(SavedItem *items, int capacity, int *count);
bool SaveDatabase_SaveItem(int id, const char *identifier);
SaveResult SaveDatabase_LoadSeed(int *seed);
bool SaveDatabase_SaveSeed(int seed);
SaveResult SaveDatabase_LoadTime(float *time);
bool SaveDatabase_SaveTime(float time);
SaveResult SaveDatabase_LoadGenerator(SavedGenerator *generator);
bool SaveDatabase_SaveGenerator(const SavedGenerator *generator);
// Called on the browser main thread, between database transactions.
void SaveDatabase_SyncBrowser(bool force);

#endif
