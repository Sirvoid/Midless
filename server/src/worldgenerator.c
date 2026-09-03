#if !defined(MIDLESS_FNL_EXTERNAL)
#define FNL_IMPL
#endif
#include <math.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"
#include "FastNoiseLite.h"
#include "worldgenerator.h"
#include "world.h"

static fnl_state heightNoise, terrainNoise, caveNoise, offsetNoise;
static int generatorSeed;

static int RandomFromPosition(Vector3 p, int salt) {
    srand(((int)(p.x * 1135 + p.y * 1307 + p.z * 1479) % 2048)
          + salt * 1024 + generatorSeed * 1024);
    return rand();
}

static Vector3 GenerateBranch(Vector3 p, float thickness, float upwardness, float length, int salt) {
    float angle = (RandomFromPosition(p, 10 + salt) % 360) * DEG2RAD;
    float dx = cosf(angle) / upwardness, dz = sinf(angle) / upwardness;
    int steps = (int)(length * 4);
    for (int i = 0; i < steps; i++) {
        for (int x = (int)-thickness; x < thickness; x++)
            for (int y = (int)-thickness; y < thickness; y++)
                for (int z = (int)-thickness; z < thickness; z++) {
                    float radius = thickness / (2 + i / (steps / 1.5f));
                    if (x * x + y * y + z * z < radius * radius)
                        ServerWorld_SetBlockFast((Vector3){p.x + x, p.y + y, p.z + z}, 10);
                }
        p.x += dx; p.z += dz; p.y += 0.25f;
    }
    return p;
}

static void GenerateLeaves(Vector3 p, float thickness) {
    float radiusSquared = thickness * thickness / 4;
    for (int x = (int)-thickness; x < thickness; x++)
        for (int y = (int)-thickness; y < thickness; y++)
            for (int z = (int)-thickness; z < thickness; z++)
                if (x * x + y * y + z * z < radiusSquared)
                    ServerWorld_SetBlockFast((Vector3){p.x + x, p.y + y, p.z + z}, 11);
}

static void GenerateTree(Vector3 p) {
    Vector3 top = GenerateBranch(p, 5, 16, 8 + RandomFromPosition(p, 1) % 3, 10);
    int count = 4 + RandomFromPosition(p, 2) % 2;
    for (int i = 0; i < count; i++) {
        Vector3 branch = GenerateBranch(top, 3, 4, 5 + RandomFromPosition(p, 3*i) % 2, i);
        GenerateLeaves(branch, 9 + RandomFromPosition(p, 4*i) % 3);
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

static bool HasSurfaceAbove(int i, const float *map) {
    return map[i] == 1 && map[i + CHUNK_SIZE_XZ] == 0;
}

void ServerWorldGenerator_Init(int seed) {
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

bool ServerWorldGenerator_GenerateStructures(Chunk *chunk, const float *map) {
    bool generated = false;
    if (chunk->position.y < 3 || chunk->position.x == 0 || chunk->position.z == 0) return false;
    for (int i = CHUNK_SIZE - 1; i >= 0; i--) if (HasSurfaceAbove(i, map)) {
        Vector3 p = Vector3Add(ServerChunk_IndexToPos(i), chunk->blockPosition);
        if (RandomFromPosition(p,5)%512 == 0) { 
            GenerateTree(p); 
            generated = true; 
        }
    }
    return generated;
}
