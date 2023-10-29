/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define FNL_IMPL

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"
#include "FastNoiseLite.h"
#include "worldgenerator.h"
#include "world.h"

fnl_state heightNoise;
fnl_state noise;
fnl_state noiseCave;
fnl_state offsetNoise;
fnl_state riverNoise;

int genWorldSeed = 0;

static int randFromPos(Vector3 pos, int seed) {
    srand(((int)((pos.x) * 1135 + (pos.y) * 1307 + (pos.z) * 1479) % 2048) + seed * 1024 + genWorldSeed * 1024);
    return rand();
}

static Vector3 makeBranch(Chunk *chunk, Vector3 position, float thickness, float upwardness, float length, int seedValue) {
    
    int randN = randFromPos(position, 10 + seedValue);

    float angle = (randN % 360) * (PI / 180.0f);
    
    float dirX = cosf(angle) / upwardness;
    float dirZ = sinf(angle) / upwardness;
    
    int steps = length * 4;
    
    for(int i = 0; i < steps; i++) {
        
        for(int x = -thickness; x < thickness; x++) {
            for(int y = -thickness; y < thickness; y++) {
                for(int z = -thickness; z < thickness; z++) {

                    float distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
                    if(distance >= (thickness / (2 + (i / (steps / 1.5f))))) continue;

                    Vector3 v = (Vector3) { position.x + x, position.y + y, position.z + z};

                    World_FastBlock(v, 10);
                }
            }
        }
        
        position.x += dirX;
        position.z += dirZ;
        position.y += 0.25f;
    }
    
    return position;
}

static void makeLeaves(Chunk *chunk, Vector3 position, float thickness) {
    for(int x = -thickness; x < thickness; x++) {
        for(int y = -thickness; y < thickness; y++) {
            for(int z = -thickness; z < thickness; z++) {

                float distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
                if(distance >= (thickness / 2)) continue;

                Vector3 v = (Vector3) { position.x + x, position.y + y, position.z + z};

                World_FastBlock(v, 11);
                
            }
        }
    }
}

static void makeTree(Chunk *chunk, Vector3 pos) {

    Vector3 top = makeBranch(chunk, pos, 5, 16, 8 + (randFromPos(pos, 1) % 3), 10);
    
    for(int i = 0; i < 4 + randFromPos(pos, 2) % 2; i++) {
        Vector3 top2 = makeBranch(chunk, top, 3, 4, 5 + (randFromPos(pos, 3 * i) % 2), i);
        makeLeaves(chunk, top2, 9 + (randFromPos(pos, 4 * i) % 3));
    }
}

static int getHeightMapPoint(Vector3 pos) {
    if(pos.y >= 16) {    
        float biomeElevation = (fnlGetNoise2D(&heightNoise, pos.x * 0.05f, pos.z * 0.05f) + 1.0f);
        float nsOff = fnlGetNoise3D(&offsetNoise, pos.x * 2.0f, pos.y * 2.0f, pos.z * 2.0f) * 12.0f * (biomeElevation - 1);
        float ns = (fnlGetNoise3D(&noise, (pos.x + nsOff) * 2.5f, nsOff, (pos.z + nsOff) * 2.5f) * nsOff + 1.0f) / 2.0f * 64 * biomeElevation;

        if (pos.y + ns >= 96 * biomeElevation) {
            return 0; //Normal Air
        } 
    }

    float nsCave = fnlGetNoise3D(&noiseCave, pos.x * 1.5f, pos.y * 1.5f, pos.z * 1.5f);
    float nsCave2 = fnlGetNoise3D(&noiseCave, pos.x * 1.5f, (pos.y + 2048) * 1.5f, pos.z * 1.5f);

    if (nsCave * nsCave2 > 0.6f) {
        return 2; //Cave Air
    } else {
        return 1; //Ground
    }
        
}

static bool checkForGrass(int index, float* heightMap, Chunk* chunk) {
    int blockID = heightMap[index];
    if(blockID == 1) {
        Vector3 pos = Vector3Add(Chunk_IndexToPos(index), chunk->blockPosition);
        if(getHeightMapPoint((Vector3) {pos.x, pos.y + 1, pos.z} ) == 0) {
            return true;
        }
    }
    return false;
}

void WorldGenerator_Init(int worldSeed) {

    genWorldSeed = worldSeed;

    heightNoise = fnlCreateState();
    heightNoise.seed = worldSeed;
    heightNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    heightNoise.fractal_type = FNL_FRACTAL_FBM;
    heightNoise.octaves = 1;
    heightNoise.lacunarity = 1.0f;
    heightNoise.gain = 0.75f;

    noise = fnlCreateState();
    noise.seed = worldSeed;
    noise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    noise.fractal_type = FNL_FRACTAL_FBM;
    noise.octaves = 3;
    noise.lacunarity = 1.0f;
    noise.gain = 0.75f;

    noiseCave = fnlCreateState();
    noiseCave.seed = worldSeed;
    noiseCave.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    noiseCave.fractal_type = FNL_FRACTAL_RIDGED;
    noiseCave.octaves = 1;
    noiseCave.lacunarity = 2.0f;
    noiseCave.gain = 1.0f;

    offsetNoise = fnlCreateState();
    offsetNoise.seed = worldSeed;
    offsetNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    offsetNoise.fractal_type = FNL_FRACTAL_FBM;
    offsetNoise.octaves = 2;
    offsetNoise.lacunarity = 2.0f;
    offsetNoise.gain = 1.0f;
}

bool WorldGenerator_GenerateStructures(Chunk *chunk, float* heightMap) {

    bool generatedSomething = false;

    if(chunk->position.y >= 3 && (chunk->position.x != 0 && chunk->position.z != 0)) {
        for (int i = CHUNK_SIZE - 1; i >= 0; i--) {
            Vector3 localPos = Chunk_IndexToPos(i);
            Vector3 pos = Vector3Add(localPos, chunk->blockPosition);
            if(checkForGrass(i, heightMap, chunk)) {
                int randN = randFromPos(pos, 5);
                if(randN % 512 == 0) { 
                    makeTree(chunk, pos);
                    generatedSomething = true;
                }
            }
        }
    }

    return generatedSomething;
}

float* WorldGenerator_Generate(Chunk *chunk) {

    int one_up = CHUNK_SIZE_XZ;

    static float heightMap[CHUNK_SIZE];

    //Ground
    for (int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 localPos = Chunk_IndexToPos(i);
        Vector3 pos = Vector3Add(localPos, chunk->blockPosition);
        heightMap[i] = getHeightMapPoint(pos);

        if(heightMap[i] == 2) continue; //Skip everything when cave air.

        int blockID = 0;
        if(heightMap[i] == 1) blockID = 1;

        if(checkForGrass(i, heightMap, chunk)) {
            if(pos.y > 48) {
                blockID = 3;
                if(i - one_up > 0) {
                    chunk->data[i - one_up] = 2;
                }
            } else {
                blockID = 6;
            }
        }

        if(heightMap[i] == 0 && pos.y < 48) {
            blockID = 5;
        } 

        if(chunk->data[i] == 0) chunk->data[i] = blockID;
    }

    //Plants
    for (int i = CHUNK_SIZE - 1; i >= 0; i--) {
        if(chunk->data[i] == 3 && i + one_up < CHUNK_SIZE) {
            Vector3 localPos = Chunk_IndexToPos(i);
            Vector3 pos = Vector3Add(localPos, chunk->blockPosition);
            int randN = randFromPos(pos, 6);
            if(randN % 128 == 0) {
                chunk->data[i + one_up] = 12;
            } else if(randN % 128 == 1) {
                chunk->data[i + one_up] = 13;
            }
        }
    }

    return heightMap;
}