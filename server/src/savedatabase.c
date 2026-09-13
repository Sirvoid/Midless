/** Copyright (c) 2026 Sirvoid. SPDX-License-Identifier: MIT */
#include "savedatabase.h"
#include "sqlite3.h"
#include "raylib.h"
#include "worldtime.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

static sqlite3 *database;
static sqlite3_stmt *loadChunk, *saveChunk, *loadPlayer, *savePlayer;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool dirty;

static bool Execute(const char *sql) {
    if (sqlite3_exec(database, sql, NULL, NULL, NULL) == SQLITE_OK) return true;
    TraceLog(LOG_ERROR, "World database: %s", sqlite3_errmsg(database));
    return false;
}

static bool Prepare(const char *sql, sqlite3_stmt **statement) {
    return database && sqlite3_prepare_v2(database, sql, -1, statement, NULL) == SQLITE_OK;
}

static void Reset(sqlite3_stmt *statement) {
    if (!statement) return;
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
}

static void CloseDatabase(void) {
    sqlite3_finalize(loadChunk);
    sqlite3_finalize(saveChunk);
    sqlite3_finalize(loadPlayer);
    sqlite3_finalize(savePlayer);
    loadChunk = saveChunk = loadPlayer = savePlayer = NULL;
    if (database) sqlite3_close(database);
    database = NULL;
}

bool SaveDatabase_Open(void) {
    pthread_mutex_lock(&mutex);
    if (database) {
        pthread_mutex_unlock(&mutex);
        return true;
    }
#if defined(PLATFORM_WEB)
    if (!EM_ASM_INT({ return Module.worldStorage && Module.worldStorage.ready ? 1 : 0; })) {
        TraceLog(LOG_ERROR, "Browser world storage is unavailable");
        pthread_mutex_unlock(&mutex);
        return false;
    }
#endif
    bool ok = sqlite3_open_v2("world/world.sqlite", &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) == SQLITE_OK;
    if (ok) {
        sqlite3_busy_timeout(database, 5000);
#if defined(PLATFORM_WEB)
        ok = Execute("PRAGMA journal_mode=MEMORY; PRAGMA synchronous=OFF;");
#else
        ok = Execute("PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL;");
#endif
    }
    sqlite3_stmt *version = NULL;
    int schema = -1;
    if (ok && Prepare("PRAGMA user_version", &version) && sqlite3_step(version) == SQLITE_ROW)
        schema = sqlite3_column_int(version, 0);
    sqlite3_finalize(version);
    if (schema != 0 && schema != 2) {
        TraceLog(LOG_ERROR, "Unsupported world database version: %d", schema);
        ok = false;
    }
    if (ok && schema == 0) {
        ok = Execute("BEGIN IMMEDIATE;"
            "CREATE TABLE chunks(x INTEGER NOT NULL,y INTEGER NOT NULL,z INTEGER NOT NULL,"
            "data BLOB NOT NULL,PRIMARY KEY(x,y,z));"
            "CREATE TABLE players(name TEXT PRIMARY KEY NOT NULL,data BLOB NOT NULL);"
            "CREATE TABLE items(id INTEGER PRIMARY KEY CHECK(id>0 AND id<65536),"
            "identifier TEXT NOT NULL UNIQUE);"
            "CREATE TABLE world(id INTEGER PRIMARY KEY CHECK(id=1),seed INTEGER,time REAL,"
            "generator TEXT,generator_version INTEGER,generator_fingerprint INTEGER);");
        if (ok) ok = Execute("PRAGMA user_version=2; COMMIT;");
        if (!ok && !sqlite3_get_autocommit(database)) Execute("ROLLBACK;");
    }
    if (ok) ok = Prepare("SELECT data FROM chunks WHERE x=?1 AND y=?2 AND z=?3", &loadChunk) &&
        Prepare("INSERT INTO chunks VALUES(?1,?2,?3,?4) ON CONFLICT(x,y,z) "
                "DO UPDATE SET data=excluded.data WHERE data!=excluded.data", &saveChunk) &&
        Prepare("SELECT data FROM players WHERE name=?1", &loadPlayer) &&
        Prepare("INSERT INTO players VALUES(?1,?2) ON CONFLICT(name) "
                "DO UPDATE SET data=excluded.data WHERE data!=excluded.data", &savePlayer);
    if (!ok) {
        TraceLog(LOG_ERROR, "Cannot open world database: %s", database ? sqlite3_errmsg(database) : "out of memory");
        CloseDatabase();
    } else dirty = true;
    pthread_mutex_unlock(&mutex);
    return ok;
}

void SaveDatabase_Close(void) {
    pthread_mutex_lock(&mutex);
    CloseDatabase();
    pthread_mutex_unlock(&mutex);
}

static bool BindPosition(sqlite3_stmt *statement, SavePosition position) {
    return sqlite3_bind_int(statement, 1, position.x) == SQLITE_OK &&
           sqlite3_bind_int(statement, 2, position.y) == SQLITE_OK &&
           sqlite3_bind_int(statement, 3, position.z) == SQLITE_OK;
}

static bool ValidName(const char *name) {
    return name && name[0] && strlen(name) <= 64;
}

static SaveResult ReadBlob(sqlite3_stmt *statement, unsigned char **data, size_t *size, size_t limit) {
    SaveResult result = SAVE_ERROR;
    int step = sqlite3_step(statement);
    if (step == SQLITE_DONE) result = SAVE_MISSING;
    if (step == SQLITE_ROW && sqlite3_column_type(statement, 0) == SQLITE_BLOB) {
        size_t length = sqlite3_column_bytes(statement, 0);
        const void *source = sqlite3_column_blob(statement, 0);
        if (length <= limit && (!length || source)) {
            unsigned char *copy = malloc(length + 1);
            if (copy) {
                if (length) memcpy(copy, source, length);
                copy[length] = 0;
                *data = copy;
                *size = length;
                result = SAVE_OK;
            }
        }
    }
    return result;
}

SaveResult SaveDatabase_LoadChunk(SavePosition position, unsigned char **data, size_t *size) {
    *data = NULL;
    *size = 0;
    pthread_mutex_lock(&mutex);
    SaveResult result = SAVE_ERROR;
    if (loadChunk && BindPosition(loadChunk, position))
        result = ReadBlob(loadChunk, data, size, 16 * 1024 * 1024);
    Reset(loadChunk);
    pthread_mutex_unlock(&mutex);
    return result;
}

SaveResult SaveDatabase_LoadPlayer(const char *name, unsigned char **data, size_t *size) {
    *data = NULL;
    *size = 0;
    pthread_mutex_lock(&mutex);
    SaveResult result = SAVE_ERROR;
    if (loadPlayer && ValidName(name) && sqlite3_bind_text(loadPlayer, 1, name, -1, SQLITE_STATIC) == SQLITE_OK)
        result = ReadBlob(loadPlayer, data, size, 1400000);
    Reset(loadPlayer);
    pthread_mutex_unlock(&mutex);
    return result;
}

bool SaveDatabase_WriteBatch(const SaveWrite *writes, int count) {
    if (!writes || count < 1 || count > 64) return false;
    pthread_mutex_lock(&mutex);
    bool ok = database && Execute("BEGIN IMMEDIATE;");
    bool changed = false;
    for (int i = 0; ok && i < count; i++) {
        const SaveWrite *write = &writes[i];
        sqlite3_stmt *statement = NULL;
        int parameter = 0;
        if (write->kind == SAVE_CHUNK && write->size <= 16 * 1024 * 1024) {
            statement = saveChunk;
            parameter = 4;
            ok = BindPosition(statement, write->position);
        } else if (write->kind == SAVE_PLAYER && ValidName(write->player) && write->size <= 1400000) {
            statement = savePlayer;
            parameter = 2;
            ok = sqlite3_bind_text(statement, 1, write->player, -1, SQLITE_STATIC) == SQLITE_OK;
        } else ok = false;
        if (ok) ok = (!write->size || write->data) && sqlite3_bind_blob(statement, parameter,
            write->size ? write->data : "", (int)write->size, SQLITE_STATIC) == SQLITE_OK;
        if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
        if (ok && sqlite3_changes(database)) changed = true;
        Reset(statement);
    }
    if (ok) ok = Execute("COMMIT;");
    if (!ok && database && !sqlite3_get_autocommit(database)) Execute("ROLLBACK;");
    if (ok && changed) dirty = true;
    pthread_mutex_unlock(&mutex);
    return ok;
}

bool SaveDatabase_SaveChunk(SavePosition position, const void *data, size_t size) {
    SaveWrite write = {.kind = SAVE_CHUNK, .position = position, .data = data, .size = size};
    return SaveDatabase_WriteBatch(&write, 1);
}

bool SaveDatabase_SavePlayer(const char *name, const void *data, size_t size) {
    SaveWrite write = {.kind = SAVE_PLAYER, .player = name, .data = data, .size = size};
    return SaveDatabase_WriteBatch(&write, 1);
}

// Small world and registry queries are prepared on demand; chunk/player queries
// above are reused because streaming calls them continuously.
static bool FinishWrite(sqlite3_stmt *statement, bool bound) {
    bool ok = bound && sqlite3_step(statement) == SQLITE_DONE;
    if (ok && sqlite3_changes(database)) dirty = true;
    sqlite3_finalize(statement);
    return ok;
}

bool SaveDatabase_SaveItem(int id, const char *identifier) {
    if (id < 1 || id >= 65536 || !ValidName(identifier)) return false;
    pthread_mutex_lock(&mutex);
    sqlite3_stmt *statement = NULL;
    bool ok = Prepare("INSERT INTO items VALUES(?1,?2) ON CONFLICT(id) "
                      "DO UPDATE SET identifier=excluded.identifier WHERE identifier=excluded.identifier", &statement);
    if (ok) ok = sqlite3_bind_int(statement, 1, id) == SQLITE_OK &&
                 sqlite3_bind_text(statement, 2, identifier, -1, SQLITE_STATIC) == SQLITE_OK;
    // IDs are permanent: an existing different identifier must fail.
    if (ok) {
        ok = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(database) == 1;
        if (ok) dirty = true;
    }
    sqlite3_finalize(statement);
    pthread_mutex_unlock(&mutex);
    return ok;
}

bool SaveDatabase_LoadItems(SavedItem *items, int capacity, int *count) {
    *count = 0;
    pthread_mutex_lock(&mutex);
    sqlite3_stmt *statement = NULL;
    bool ok = Prepare("SELECT id,identifier FROM items ORDER BY id", &statement);
    int step = SQLITE_ERROR;
    while (ok && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        int length = sqlite3_column_bytes(statement, 1);
        const char *name = (const char *)sqlite3_column_text(statement, 1);
        if (*count >= capacity || !name || length < 1 || length > 64 || memchr(name, 0, length)) {
            ok = false;
            break;
        }
        items[*count].id = sqlite3_column_int(statement, 0);
        memcpy(items[*count].identifier, name, length);
        items[*count].identifier[length] = 0;
        (*count)++;
    }
    ok = ok && step == SQLITE_DONE;
    sqlite3_finalize(statement);
    pthread_mutex_unlock(&mutex);
    return ok;
}

static SaveResult ReadNumber(const char *sql, double *value) {
    sqlite3_stmt *statement = NULL;
    SaveResult result = SAVE_ERROR;
    if (Prepare(sql, &statement)) {
        int step = sqlite3_step(statement);
        if (step == SQLITE_DONE || (step == SQLITE_ROW && sqlite3_column_type(statement, 0) == SQLITE_NULL))
            result = SAVE_MISSING;
        else if (step == SQLITE_ROW) {
            int type = sqlite3_column_type(statement, 0);
            if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
                *value = sqlite3_column_double(statement, 0);
                result = SAVE_OK;
            }
        }
    }
    sqlite3_finalize(statement);
    return result;
}

SaveResult SaveDatabase_LoadSeed(int *seed) {
    pthread_mutex_lock(&mutex);
    double value = 0;
    SaveResult result = ReadNumber("SELECT seed FROM world WHERE id=1", &value);
    if (result == SAVE_OK) {
        if (!isfinite(value) || value < INT32_MIN || value > INT32_MAX || floor(value) != value) result = SAVE_ERROR;
        else *seed = (int)value;
    }
    pthread_mutex_unlock(&mutex);
    return result;
}

bool SaveDatabase_SaveSeed(int seed) {
    pthread_mutex_lock(&mutex);
    sqlite3_stmt *statement = NULL;
    bool ok = Prepare("INSERT INTO world(id,seed) VALUES(1,?1) ON CONFLICT(id) DO UPDATE SET seed=excluded.seed", &statement);
    if (ok) ok = sqlite3_bind_int(statement, 1, seed) == SQLITE_OK;
    ok = FinishWrite(statement, ok);
    pthread_mutex_unlock(&mutex);
    return ok;
}

SaveResult SaveDatabase_LoadTime(float *time) {
    pthread_mutex_lock(&mutex);
    double value = 0;
    SaveResult result = ReadNumber("SELECT time FROM world WHERE id=1", &value);
    if (result == SAVE_OK) {
        if (!isfinite(value) || value < 0 || value >= WORLD_DAY_LENGTH_SECONDS) result = SAVE_ERROR;
        else *time = (float)value;
    }
    pthread_mutex_unlock(&mutex);
    return result;
}

bool SaveDatabase_SaveTime(float time) {
    if (!isfinite(time) || time < 0 || time >= WORLD_DAY_LENGTH_SECONDS) return false;
    pthread_mutex_lock(&mutex);
    sqlite3_stmt *statement = NULL;
    bool ok = Prepare("INSERT INTO world(id,time) VALUES(1,?1) ON CONFLICT(id) DO UPDATE SET time=excluded.time", &statement);
    if (ok) ok = sqlite3_bind_double(statement, 1, time) == SQLITE_OK;
    ok = FinishWrite(statement, ok);
    pthread_mutex_unlock(&mutex);
    return ok;
}

SaveResult SaveDatabase_LoadGenerator(SavedGenerator *generator) {
    pthread_mutex_lock(&mutex);
    sqlite3_stmt *statement = NULL;
    SaveResult result = SAVE_ERROR;
    if (Prepare("SELECT generator,generator_version,generator_fingerprint FROM world WHERE id=1", &statement)) {
        int step = sqlite3_step(statement);
        if (step == SQLITE_DONE || (step == SQLITE_ROW && sqlite3_column_type(statement, 0) == SQLITE_NULL))
            result = SAVE_MISSING;
        else if (step == SQLITE_ROW) {
            const char *name = (const char *)sqlite3_column_text(statement, 0);
            int length = sqlite3_column_bytes(statement, 0);
            sqlite3_int64 version = sqlite3_column_int64(statement, 1);
            sqlite3_int64 fingerprint = sqlite3_column_int64(statement, 2);
            if (name && length > 0 && length <= 64 && !memchr(name, 0, length) &&
                sqlite3_column_type(statement, 1) == SQLITE_INTEGER && version >= 0 && version <= INT32_MAX &&
                sqlite3_column_type(statement, 2) == SQLITE_INTEGER && fingerprint >= 0 && fingerprint <= UINT32_MAX) {
                memcpy(generator->name, name, length);
                generator->name[length] = 0;
                generator->version = (int)version;
                generator->fingerprint = (uint32_t)fingerprint;
                result = SAVE_OK;
            }
        }
    }
    sqlite3_finalize(statement);
    pthread_mutex_unlock(&mutex);
    return result;
}

bool SaveDatabase_SaveGenerator(const SavedGenerator *generator) {
    if (!ValidName(generator->name) || generator->version < 0) return false;
    pthread_mutex_lock(&mutex);
    sqlite3_stmt *statement = NULL;
    bool ok = Prepare("INSERT INTO world(id,generator,generator_version,generator_fingerprint) VALUES(1,?1,?2,?3) "
        "ON CONFLICT(id) DO UPDATE SET generator=excluded.generator,generator_version=excluded.generator_version,"
        "generator_fingerprint=excluded.generator_fingerprint", &statement);
    if (ok) ok = sqlite3_bind_text(statement, 1, generator->name, -1, SQLITE_STATIC) == SQLITE_OK &&
        sqlite3_bind_int(statement, 2, generator->version) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 3, generator->fingerprint) == SQLITE_OK;
    ok = FinishWrite(statement, ok);
    pthread_mutex_unlock(&mutex);
    return ok;
}

void SaveDatabase_SyncBrowser(bool force) {
#if defined(PLATFORM_WEB)
    static double lastSync;
    double now = emscripten_get_now();
    if (!force && now - lastSync < 5000) return;
    // Never wait for a worker which might be proxying filesystem calls to us.
    if (pthread_mutex_trylock(&mutex)) return;
    if (dirty) {
        bool accepted = EM_ASM_INT({
            return Module.worldStorage.save() ? 1 : 0;
        });
        if (accepted) dirty = false;
    }
    lastSync = now;
    pthread_mutex_unlock(&mutex);
#else
    (void)force;
#endif
}
