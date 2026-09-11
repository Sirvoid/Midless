#include "worldgen.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#define __clang__ true
#include "stb_ds.h"

#define SKY_CACHE_COLUMNS 2048
typedef struct SkyColumn {
    int highest, lowest;
    bool initialized;
} SkyColumn;
typedef struct SkyColumns { SkyColumn cells[CHUNK_SIZE_XZ]; } SkyColumns;
typedef struct SkyCacheEntry { long int key; SkyColumns *value; } SkyCacheEntry;
static SkyCacheEntry *cache;
static int evictionCursor;
static bool checked, allowed;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void Worldgen_ClearSkyCache(void) {
    // Worldgen configuration changes only after generation workers are joined.
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < hmlen(cache); i++) free(cache[i].value);
    hmfree(cache);
    cache = NULL;
    evictionCursor = 0;
    checked = allowed = false;
    pthread_mutex_unlock(&mutex);
}

static bool CanCache(void) {
    if (worldgen.skyField < 0) return false;
    if (worldgen.ceiling >= 0 && !worldgen.fields[worldgen.ceiling].isColumnConstant) return false;
    bool usesOriginY[WG_MAX_FIELDS] = {0};
    // Fields refer to earlier fields. The chunk's starting Y must not affect
    // the sky function: such custom generators keep the original scan path.
    for (int i = 0; i < worldgen.fieldCount; i++) {
        WGField *field = &worldgen.fields[i];
        usesOriginY[i] = field->op == WG_ORIGIN_Y;
        int inputs[3] = {field->firstInput, field->secondInput, field->thirdInput};
        for (int j = 0; j < 3; j++) {
            if (inputs[j] >= 0 && inputs[j] < i && usesOriginY[inputs[j]]) usesOriginY[i] = true;
        }
    }
    return !usesOriginY[worldgen.skyField] &&
           (worldgen.ceiling < 0 || !usesOriginY[worldgen.ceiling]);
}

bool Worldgen_CachedSkyMask(Chunk *chunk) {
    if (!worldgen.frozen) return false;
    long int key = ServerChunk_GetPackedPos((Vector3){chunk->position.x, 0, chunk->position.z});
    SkyColumns columns = {0};
    pthread_mutex_lock(&mutex);
    if (!checked) { allowed = CanCache(); checked = true; }
    if (!allowed) { pthread_mutex_unlock(&mutex); return false; }
    int index = hmgeti(cache, key);
    if (index >= 0) columns = *cache[index].value;
    pthread_mutex_unlock(&mutex);

    int first = chunk->blockPosition.y + CHUNK_SIZE_Y;
    memset(chunk->skyMask, 255, sizeof(chunk->skyMask));
    for (int z = 0; z < CHUNK_SIZE_Z; z++) {
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            int cell = z * CHUNK_SIZE_X + x;
            SkyColumn *column = &columns.cells[cell];
            if (!column->initialized || (column->highest == INT_MIN && first < column->lowest)) {
                WGEval context;
                Vector3 position = {chunk->blockPosition.x + x, first, chunk->blockPosition.z + z};
                Worldgen_EvalInit(&context, position, position);
                if (!column->initialized) {
                    int ceiling = worldgen.ceiling < 0 ? worldgen.maxY :
                        (int)fmaxf(-4096, fminf(4096, Worldgen_Eval(&context, worldgen.ceiling)));
                    column->highest = INT_MIN;
                    column->lowest = ceiling + 1;
                    column->initialized = true;
                }
                // Scan downward only through levels not previously examined.
                // The first obstruction is the highest one, valid for every
                // lower chunk in this horizontal column.
                for (int y = column->lowest - 1; y >= first; y--) {
                    Worldgen_EvalY(&context, y);
                    column->lowest = y;
                    if (Worldgen_Eval(&context, worldgen.skyField) > 0) {
                        column->highest = y;
                        break;
                    }
                }
            }
            if (column->highest >= first) chunk->skyMask[cell >> 3] &= ~(1u << (cell & 7));
        }
    }

    pthread_mutex_lock(&mutex);
    index = hmgeti(cache, key);
    if (index >= 0) {
        // Concurrent vertical chunks may have examined different depths.
        for (int i = 0; i < CHUNK_SIZE_XZ; i++) {
            SkyColumn *old = &cache[index].value->cells[i], *next = &columns.cells[i];
            if (next->highest > old->highest) old->highest = next->highest;
            if (next->lowest < old->lowest) old->lowest = next->lowest;
        }
    } else {
        SkyColumns *copy = malloc(sizeof(*copy));
        if (copy) {
            *copy = columns;
            int count = hmlen(cache);
            if (count >= SKY_CACHE_COLUMNS) {
                int victim = evictionCursor++ % count;
                free(cache[victim].value);
                long int oldKey = cache[victim].key;
                (void)hmdel(cache, oldKey);
                evictionCursor %= SKY_CACHE_COLUMNS;
            }
            hmput(cache, key, copy);
        }
    }
    pthread_mutex_unlock(&mutex);
    return true;
}
