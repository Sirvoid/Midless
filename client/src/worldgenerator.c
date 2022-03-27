/**
 * Copyright (c) 2021-2022 Sirvoid
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

fnl_state heightNoise;
fnl_state noise;
fnl_state noiseCave;
fnl_state offsetNoise;
fnl_state riverNoise;

void WorldGenerator_Init(int worldSeed) {

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

int WorldGenerator_Generate(Chunk *chunk, Vector3 pos, int index) {

    int blockID = 0;

    float nsCave = fnlGetNoise3D(&noiseCave, pos.x * 1.5f, pos.y * 1.5f, pos.z * 1.5f);
    float nsCave2 = fnlGetNoise3D(&noiseCave, pos.x * 1.5f, (pos.y + 2048) * 1.5f, pos.z * 1.5f);

    float biomeElevation = (fnlGetNoise2D(&heightNoise, pos.x * 0.05f, pos.z * 0.05f) + 1.0f);

    if(pos.y >= 32 * biomeElevation) {
        float nsOff = fnlGetNoise3D(&offsetNoise, pos.x * 2.0f, pos.y * 2.0f, pos.z * 2.0f) * 12.0f * (biomeElevation - 1);
        float ns = (fnlGetNoise3D(&noise, (pos.x + nsOff) * 2.5f, nsOff, (pos.z + nsOff) * 2.5f) * nsOff + 1.0f) / 2.0f * 64 * biomeElevation;
        float ny = pos.y - 32;
        
        float waterLevel = 58;

        if(ny + 4 < ns) blockID = 1; //Stone
        else if(ny + 1 < ns) {
            if(chunk->data[(index + CHUNK_SIZE_XZ) % CHUNK_SIZE] == 0) {
                blockID = 3; //Grass
            } else {
                blockID = 2; //Dirt
            }
        } 
        else if(ny < ns && pos.y < waterLevel) blockID= 6; //Sand
        else if(pos.y < waterLevel) blockID = 5; //Water

        if( ny < ns &&  nsCave * nsCave2 > 0.6f) blockID = 0; //Caves
    } else {
        blockID = 1;
        if(nsCave * nsCave2 > 0.6f) blockID = 0; //Caves
    }

    return blockID;
}