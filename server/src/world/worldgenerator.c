#if !defined(MIDLESS_FNL_EXTERNAL)
#define FNL_IMPL
#endif
#define __clang__ true
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "FastNoiseLite.h"
#include "worldgenerator.h"
#include "world.h"

static fnl_state heightNoise, terrainNoise, caveNoise, offsetNoise;
static int generatorSeed;

typedef struct TreeOriginCacheEntry {
    long int key;
    Vector3 *value;
} TreeOriginCacheEntry;

static TreeOriginCacheEntry *treeOriginCache;

static int RandomFromPosition(Vector3 p, int salt) {
    srand(((int)(p.x * 1135 + p.y * 1307 + p.z * 1479) % 2048)
          + salt * 1024 + generatorSeed * 1024);
    return rand();
}

static void PlaceStructureBlock(Chunk *target, Vector3 worldPos, int blockId) {
    Vector3 localPos = {
        floorf(worldPos.x) - target->blockPosition.x,
        floorf(worldPos.y) - target->blockPosition.y,
        floorf(worldPos.z) - target->blockPosition.z
    };
    if (!ServerChunk_IsValidPos(localPos)) return;
    target->data[ServerChunk_PosToIndex(localPos)] = (unsigned short)blockId;
}

static Vector3 GenerateBranch(Chunk *target, Vector3 p, float thickness, float upwardness, float length, int salt) {
    float angle = (RandomFromPosition(p, 10 + salt) % 360) * DEG2RAD;
    float dx = cosf(angle) / upwardness, dz = sinf(angle) / upwardness;
    int steps = (int)(length * 4);
    for (int i = 0; i < steps; i++) {
        for (int x = (int)-thickness; x < thickness; x++)
            for (int y = (int)-thickness; y < thickness; y++)
                for (int z = (int)-thickness; z < thickness; z++) {
                    float radius = thickness / (2 + i / (steps / 1.5f));
                    if (x * x + y * y + z * z < radius * radius)
                        PlaceStructureBlock(target, (Vector3){p.x + x, p.y + y, p.z + z}, 10);
                }
        p.x += dx; p.z += dz; p.y += 0.25f;
    }
    return p;
}

static void GenerateLeaves(Chunk *target, Vector3 p, float thickness) {
    float radiusSquared = thickness * thickness / 4;
    for (int x = (int)-thickness; x < thickness; x++)
        for (int y = (int)-thickness; y < thickness; y++)
            for (int z = (int)-thickness; z < thickness; z++)
                if (x * x + y * y + z * z < radiusSquared)
                    PlaceStructureBlock(target, (Vector3){p.x + x, p.y + y, p.z + z}, 11);
}

static void GenerateTree(Chunk *target, Vector3 p) {
    Vector3 top = GenerateBranch(target, p, 5, 16, 8 + RandomFromPosition(p, 1) % 3, 10);
    int count = 4 + RandomFromPosition(p, 2) % 2;
    for (int i = 0; i < count; i++) {
        Vector3 branch = GenerateBranch(target, top, 3, 4, 5 + RandomFromPosition(p, 3*i) % 2, i);
        GenerateLeaves(target, branch, 9 + RandomFromPosition(p, 4*i) % 3);
    }
}

static float GetBiomeElevation(float x, float z) {
    return fnlGetNoise2D(&heightNoise, x * 0.05f, z * 0.05f) + 1;
}

static int GetTerrainPoint(Vector3 p, float elevation) {
    if (p.y >= 16) {
        float off = fnlGetNoise3D(&offsetNoise, p.x * 2, p.y * 2, p.z * 2) * 12 * (elevation - 1);
        float terrain = (fnlGetNoise3D(&terrainNoise, (p.x + off) * 2.5f, off, (p.z + off) * 2.5f) * off + 1) * 32 * elevation;
        if (p.y + terrain >= 96 * elevation) return 0;
    }
    float a = fnlGetNoise3D(&caveNoise, p.x * 1.5f, p.y * 1.5f, p.z * 1.5f);
    float b = fnlGetNoise3D(&caveNoise, p.x * 1.5f, (p.y + 2048) * 1.5f, p.z * 1.5f);
    return a * b > 0.6f ? 2 : 1;
}

void ServerWorldGenerator_GenerateSkyMask(Chunk *chunk) {
    int firstYAboveChunk = (int)chunk->blockPosition.y + CHUNK_SIZE_Y;
    memset(chunk->skyMask, 0, sizeof(chunk->skyMask));

    for (int z = 0; z < CHUNK_SIZE_Z; z++) {
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            float worldX = chunk->blockPosition.x + x;
            float worldZ = chunk->blockPosition.z + z;
            float elevation = GetBiomeElevation(worldX, worldZ);

            // Above this height GetTerrainPoint is guaranteed to return air,
            int terrainCeiling = (int)ceilf(64.0f * elevation +
                384.0f * elevation * fabsf(elevation - 1.0f)) + 1;
            bool openToSky = true;

            // Scan upward and stop at the first obstruction.
            for (int y = firstYAboveChunk; y <= terrainCeiling; y++) {
                if (GetTerrainPoint((Vector3){worldX, y, worldZ}, elevation) == 1) {
                    openToSky = false;
                    break;
                }
            }

            if (openToSky) {
                int column = z * CHUNK_SIZE_X + x;
                chunk->skyMask[column >> 3] |= (unsigned char)(1u << (column & 7));
            }
        }
    }
}

static bool HasSurfaceAbove(int i, const float *map) {
    return map[i] == 1 && map[i + CHUNK_SIZE_XZ] == 0;
}

void ServerWorldGenerator_Init(int seed) {
    for (int i = 0; i < hmlen(treeOriginCache); i++) arrfree(treeOriginCache[i].value);
    hmfree(treeOriginCache);
    treeOriginCache = NULL;

    generatorSeed = seed;
    heightNoise = fnlCreateState(); heightNoise.seed = seed; heightNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    heightNoise.fractal_type = FNL_FRACTAL_FBM; heightNoise.octaves = 1; heightNoise.lacunarity = 1; heightNoise.gain = .75f;
    terrainNoise = fnlCreateState(); terrainNoise.seed = seed; terrainNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    terrainNoise.fractal_type = FNL_FRACTAL_FBM; terrainNoise.octaves = 3; terrainNoise.lacunarity = 1; terrainNoise.gain = .75f;
    caveNoise = fnlCreateState(); caveNoise.seed = seed; caveNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    caveNoise.fractal_type = FNL_FRACTAL_RIDGED; caveNoise.octaves = 1; caveNoise.lacunarity = 2; caveNoise.gain = 1;
    offsetNoise = fnlCreateState(); offsetNoise.seed = seed; offsetNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    offsetNoise.fractal_type = FNL_FRACTAL_FBM; offsetNoise.octaves = 2; offsetNoise.lacunarity = 2; offsetNoise.gain = 1;
}

float *ServerWorldGenerator_Generate(Chunk *chunk) {
    static float map[CHUNK_SIZE + CHUNK_SIZE_XZ];
    for (int z = 0; z < CHUNK_SIZE_Z; z++) for (int x = 0; x < CHUNK_SIZE_X; x++) {
        float wx = chunk->blockPosition.x + x, wz = chunk->blockPosition.z + z, elevation = GetBiomeElevation(wx,wz);
        for (int y = 0; y <= CHUNK_SIZE_Y; y++) {
            int i = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
            map[i] = GetTerrainPoint((Vector3){wx, chunk->blockPosition.y + y,wz}, elevation);
        }
    }
    for (int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 p = Vector3Add(ServerChunk_IndexToPos(i), chunk->blockPosition);
        if (map[i] == 2) continue;
        int block = map[i] == 1 ? 1 : 0;
        if (HasSurfaceAbove(i, map)) {
            block = p.y > 48 ? 3 : 6;
            if (block == 3 && i - CHUNK_SIZE_XZ > 0) chunk->data[i - CHUNK_SIZE_XZ] = 2;
        } else if (map[i] == 0 && p.y < 48) block = 5;
        if (chunk->data[i] == 0) chunk->data[i] = block;
    }
    for (int i = CHUNK_SIZE - 1; i >= 0; i--) if (chunk->data[i] == 3 && i + CHUNK_SIZE_XZ < CHUNK_SIZE) {
        int r = RandomFromPosition(Vector3Add(ServerChunk_IndexToPos(i), chunk->blockPosition),6);
        if (r % 128 == 0) chunk->data[i + CHUNK_SIZE_XZ] = 12; 
        else if (r % 128 == 1) chunk->data[i + CHUNK_SIZE_XZ] = 13;
    }
    return map;
}

static Vector3 *GetTreeOrigins(Vector3 originChunkPos) {
    long int key = ServerChunk_GetPackedPos(originChunkPos);
    int cacheIndex = hmgeti(treeOriginCache, key);
    if (cacheIndex >= 0) return treeOriginCache[cacheIndex].value;

    Vector3 *origins = NULL;
    if (originChunkPos.y < 3 || originChunkPos.x == 0 || originChunkPos.z == 0) {
        hmput(treeOriginCache, key, origins);
        return origins;
    }

    Vector3 origin = Vector3Multiply(originChunkPos, CHUNK_SIZE_VEC3);
    for (int z = CHUNK_SIZE_Z - 1; z >= 0; z--) {
        for (int x = CHUNK_SIZE_X - 1; x >= 0; x--) {
            float worldX = origin.x + x;
            float worldZ = origin.z + z;
            float elevation = GetBiomeElevation(worldX, worldZ);

            // Match the original descending index order exactly.
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                Vector3 p = {worldX, origin.y + y, worldZ};
                if (GetTerrainPoint(p, elevation) != 1 ||
                    GetTerrainPoint((Vector3){p.x, p.y + 1, p.z}, elevation) != 0) continue;

                if (RandomFromPosition(p, 5) % 512 == 0) arrput(origins, p);
            }
        }
    }

    hmput(treeOriginCache, key, origins);
    return origins;
}

static bool GenerateStructuresFromOriginChunk(Chunk *target, Vector3 originChunkPos) {
    Vector3 *origins = GetTreeOrigins(originChunkPos);
    bool generated = arrlen(origins) > 0;
    for (int i = 0; i < arrlen(origins); i++) GenerateTree(target, origins[i]);
    return generated;
}

bool ServerWorldGenerator_GenerateStructures(Chunk *chunk) {
    bool generated = false;

    for (int oy = -2; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            for (int oz = -1; oz <= 1; oz++) {
                Vector3 originChunkPos = {
                    chunk->position.x + ox,
                    chunk->position.y + oy,
                    chunk->position.z + oz
                };
                if (GenerateStructuresFromOriginChunk(chunk, originChunkPos)) generated = true;
            }
        }
    }
    return generated;
}
