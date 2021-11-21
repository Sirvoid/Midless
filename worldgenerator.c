/**
 * Copyright (c) 2021 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "worldgenerator.h"
#include "world.h"
#include "raylib.h"

#define FNL_IMPL
#include "FastNoiseLite.h"

unsigned char *WorldGenerator_Generate(void) {
    int worldSize = World_GetFlatSize();
    unsigned char *worldData = MemAlloc(worldSize);

    int seed = rand() / 2;

    fnl_state noise = fnlCreateState();
    noise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    noise.fractal_type = FNL_FRACTAL_FBM;
    noise.octaves = 5;
    noise.lacunarity = 1.0f;
    noise.gain = 0.75f;

    fnl_state noiseCave = fnlCreateState();
    noiseCave.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    noiseCave.fractal_type = FNL_FRACTAL_FBM;
    noiseCave.octaves = 1;
    noiseCave.lacunarity = 2.0f;
    noiseCave.gain = 1.0f;

    fnl_state offsetNoise = fnlCreateState();
    offsetNoise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    offsetNoise.fractal_type = FNL_FRACTAL_FBM;
    offsetNoise.octaves = 2;
    offsetNoise.lacunarity = 2.0f;
    offsetNoise.gain = 1.0f;

    float worldSizeHalfX = world.size.x / 2;
    float worldSizeHalfZ = world.size.z / 2;
    float worldSizeHalfChunkX = world.size.x / (CHUNK_SIZE_X / 2);

    int one_up = world.size.x * world.size.z;

    for(int i = 0; i < worldSize; i++) {
        Vector3 npos = World_BlockIndexToPos(i);
        float distanceCenter = sqrt(pow(npos.x - worldSizeHalfX, 2) + pow(npos.z - worldSizeHalfZ, 2));
        float isleHeight = fmax(distanceCenter / worldSizeHalfChunkX, 0.8f);

        float nsCave = fnlGetNoise3D(&noiseCave, npos.x * 2.5f, (npos.y + seed) * 2.5f, npos.z * 2.5f);
        float nsCave2 = fnlGetNoise3D(&noiseCave, npos.x * 2.5f, (npos.y + 2048 + seed) * 2.5f, npos.z * 2.5f);

        float nsOff = fnlGetNoise3D(&offsetNoise, npos.x * 2.5f, (npos.y + seed) * 2.5f, npos.z * 2.5f) * 4.0f;
        float ns = (fnlGetNoise3D(&noise, (npos.x + nsOff) * 3, seed + nsOff, (npos.z + nsOff) * 3) * nsOff + 1.0f) / 2.0f * 42 / isleHeight;
        float ny = npos.y - 32;

        if(npos.y < 45) worldData[i] = 5; //Water

        if(ny < ns && npos.y < 45) worldData[i] = 6; //Sand
        if(ny + 1 < ns) worldData[i] = 3; //Grass
        if(ny + 3 < ns) worldData[i] = 1; //Stone

        if( ny < ns &&  fabs(nsCave) + fabs(nsCave2) < 0.3f) worldData[i] = 0; //Caves

        if(npos.y == 0) worldData[i] = 16; //Lava
    }

    //Turn grass to dirt if no sunlight
    for(int i = 0; i < worldSize; i++) {
        if(worldData[i] == 3) {
            for(int j = i + one_up; j <= worldSize - one_up; j += one_up) {
                if(worldData[j] != 0) {
                    worldData[i] = 2; //dirt
                    break;
                }
            }
        }
    }

    //Plant stuff
    for(int i = 0; i < worldSize; i++) {
        if(worldData[i] == 3) {
             if(rand() % 12 == 0) { 
                Vector3 npos = World_BlockIndexToPos(i);
                if( (int) npos.x % 10 == 0 && (int) npos.z % 10 == 0) WorldGenerator_MakeTree(worldData, npos);
            } else if(rand() % 128 == 0) {
                worldData[i + one_up] = 12;
            } else if(rand() % 128 == 1) {
                worldData[i + one_up] = 13;
            }
        }
    }

    return worldData;
}

Vector3 WorldGenerator_MakeBranch(unsigned char * worldData, Vector3 position, float thickness, float upwardness, float length) {
    
    float angle = (rand() % 360) * (PI / 180.0f);
    
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
                    if(!World_IsValidBlockPos(v)) continue;
                    int index = World_BlockPosToIndex(v);
                    worldData[index] = 10;
                }
            }
        }
        
        position.x += dirX;
        position.z += dirZ;
        position.y += 0.25f;
    }
    
    return position;
}

void WorldGenerator_MakeLeaves(unsigned char *worldData, Vector3 position, float thickness) {
    for(int x = -thickness; x < thickness; x++) {
        for(int y = -thickness; y < thickness; y++) {
            for(int z = -thickness; z < thickness; z++) {

                float distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
                if(distance >= (thickness / 2)) continue;

                Vector3 v = (Vector3) { position.x + x, position.y + y, position.z + z};
                if(!World_IsValidBlockPos(v)) continue;
                int index = World_BlockPosToIndex(v);
                if(worldData[index] != 10) worldData[index] = 11;
                
            }
        }
    }
}

void WorldGenerator_MakeTree(unsigned char *worldData, Vector3 position) {

    Vector3 top = WorldGenerator_MakeBranch(worldData, position, 5, 16, 8 + (rand() % 3));
    
    for(int i = 0; i < 4 + rand() % 2; i++) {
        Vector3 top2 = WorldGenerator_MakeBranch(worldData, top, 3, 4, 5 + (rand() % 2));
        WorldGenerator_MakeLeaves(worldData, top2, 9 + (rand() % 3));
    }
}