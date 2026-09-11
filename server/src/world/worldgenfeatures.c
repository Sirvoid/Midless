#include "worldgen.h"
#include <math.h>
#include <pthread.h>
#include <string.h>
#define __clang__ true
#include "stb_ds.h"

typedef struct OriginCache {
    long int key;
    Vector3 *value;
} OriginCache;
static OriginCache *originCaches[WG_MAX_FEATURES];
static int evictionCursor[WG_MAX_FEATURES];
static pthread_mutex_t originCacheMutex = PTHREAD_MUTEX_INITIALIZER;

// The caller holds originCacheMutex while freeing or using an origin array.
static void ClearOriginCache(int featureIndex) {
    for (int i = 0; i < hmlen(originCaches[featureIndex]); i++) {
        arrfree(originCaches[featureIndex][i].value);
    }
    hmfree(originCaches[featureIndex]);
    originCaches[featureIndex] = NULL;
    evictionCursor[featureIndex] = 0;
}

void Worldgen_ClearFeatures(void) {
    pthread_mutex_lock(&originCacheMutex);
    for (int featureIndex = 0; featureIndex < WG_MAX_FEATURES; featureIndex++) {
        ClearOriginCache(featureIndex);
    }
    pthread_mutex_unlock(&originCacheMutex);
}

static void WriteFeatureBlock(Chunk *chunk, Vector3 position, int blockId) {
    Vector3 localPosition = {floorf(position.x) - chunk->blockPosition.x,
                             floorf(position.y) - chunk->blockPosition.y,
                             floorf(position.z) - chunk->blockPosition.z};
    if (ServerChunk_IsValidPos(localPosition))
        chunk->data[ServerChunk_PosToIndex(localPosition)] = blockId;
}

static void PaintSphere(Chunk *chunk, Vector3 center, int bounds, float radius, int blockId) {
    if (radius <= 0)
        return;
    for (int x = -bounds; x < bounds; x++)
        for (int y = -bounds; y < bounds; y++)
            for (int z = -bounds; z < bounds; z++)
                if (x * x + y * y + z * z < radius * radius)
                    WriteFeatureBlock(chunk, (Vector3){center.x + x, center.y + y, center.z + z},
                                      blockId);
}

static void ExecuteFeatureCommands(Chunk *chunk, WGFeature *feature, Vector3 origin) {
    Vector3 positions[8] = {0};
    positions[0] = origin;
    // Slot zero is the feature anchor; later commands can reuse earlier endpoints.
    bool positionInitialized[8] = {true};
    for (int i = 0; i < feature->count; i++) {
        WGCommand *command = &feature->commands[i];
        if (!positionInitialized[command->from])
            continue;
        Vector3 cursor = positions[command->from];
        WGEval context;
        Worldgen_EvalInit(&context, cursor, origin);
        if (command->when >= 0 && Worldgen_Eval(&context, command->when) == 0)
            continue;
        int bounds = (int)fmaxf(0, fminf(32, Worldgen_Eval(&context, command->bounds)));
        int steps = command->op == WG_COMMAND_STROKE
                        ? (int)fmaxf(0, fminf(256, Worldgen_Eval(&context, command->steps)))
                        : 1;
        float dx = Worldgen_Eval(&context, command->dx), dy = Worldgen_Eval(&context, command->dy),
              dz = Worldgen_Eval(&context, command->dz);
        for (int step = 0; step < steps; step++) {
            Worldgen_EvalInit(&context, cursor, origin);
            context.step = step;
            context.steps = steps;
            PaintSphere(chunk, cursor, bounds, Worldgen_Eval(&context, command->radius),
                        command->block);
            cursor.x += dx;
            cursor.y += dy;
            cursor.z += dz;
        }
        positions[command->to] = cursor;
        positionInitialized[command->to] = true;
    }
}

static Vector3 *GenerateFeatureOrigins(int featureIndex, Vector3 chunkPosition) {
    Vector3 *origins = NULL;
    Vector3 chunkOrigin = {chunkPosition.x * CHUNK_SIZE_X, chunkPosition.y * CHUNK_SIZE_Y,
                           chunkPosition.z * CHUNK_SIZE_Z};
    for (int z = CHUNK_SIZE_Z - 1; z >= 0; z--)
        for (int x = CHUNK_SIZE_X - 1; x >= 0; x--) {
            WGEval context;
            Vector3 position = {chunkOrigin.x + x, chunkOrigin.y, chunkOrigin.z + z};
            Worldgen_EvalInit(&context, position, position);
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                Worldgen_EvalY(&context, chunkOrigin.y + y);
                if (Worldgen_Eval(&context, worldgen.features[featureIndex].when) != 0)
                    arrput(origins, context.position);
            }
        }
    return origins;
}

static void CopyFeatureOrigins(int featureIndex, Vector3 position, Vector3 **anchors) {
    long int key = ServerChunk_GetPackedPos(position);
    pthread_mutex_lock(&originCacheMutex);
    int index = hmgeti(originCaches[featureIndex], key);
    Vector3 *generated = NULL;
    if (index < 0) {
        pthread_mutex_unlock(&originCacheMutex);
        generated = GenerateFeatureOrigins(featureIndex, position);
        pthread_mutex_lock(&originCacheMutex);
        // Another worker may have produced these same anchors in the meantime.
        index = hmgeti(originCaches[featureIndex], key);
        if (index < 0) {
            int count = hmlen(originCaches[featureIndex]);
            if (count >= 4096) {
                int victim = evictionCursor[featureIndex]++ % count;
                arrfree(originCaches[featureIndex][victim].value);
                long int oldKey = originCaches[featureIndex][victim].key;
                (void)hmdel(originCaches[featureIndex], oldKey);
                evictionCursor[featureIndex] %= 4096;
            }
            hmput(originCaches[featureIndex], key, generated);
            generated = NULL; // Ownership transferred to the cache.
            index = hmgeti(originCaches[featureIndex], key);
        }
    }
    Vector3 *origins = originCaches[featureIndex][index].value;
    int count = arrlen(origins);
    (void)arrsetlen(*anchors, count);
    if (count) memcpy(*anchors, origins, count * sizeof(Vector3));
    pthread_mutex_unlock(&originCacheMutex);
    arrfree(generated);
}

void Worldgen_Features(Chunk *chunk) {
    if (!worldgen.featureCount)
        return;
    // Copy anchors while locked. Placement only touches this worker's chunk,
    // and another worker may safely evict the cache while we place features.
    Vector3 *anchors = NULL;
    for (int featureIndex = 0; featureIndex < worldgen.featureCount; featureIndex++) {
        WGFeature *feature = &worldgen.features[featureIndex];
        for (int y = feature->paddingMin[1]; y <= feature->paddingMax[1]; y++)
            for (int x = feature->paddingMin[0]; x <= feature->paddingMax[0]; x++)
                for (int z = feature->paddingMin[2]; z <= feature->paddingMax[2]; z++) {
                    Vector3 originChunkPosition = {chunk->position.x + x, chunk->position.y + y,
                                                   chunk->position.z + z};
                    CopyFeatureOrigins(featureIndex, originChunkPosition, &anchors);
                    int count = arrlen(anchors);
                    for (int i = 0; i < count; i++)
                        ExecuteFeatureCommands(chunk, feature, anchors[i]);
                }
    }
    arrfree(anchors);
}
