/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <pthread.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "chunkLightning.h"
#include "../block/block.h"
#include "world.h"
#include "player.h"

pthread_mutex_t light_mutex;
LightNode *lightQueue = NULL;
LightDelNode *lightDelQueue = NULL;

Vector3 lightDirections[6] = {
    {-1, 0, 0},
    {1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {0, 0, 1},
    {0, 0, -1}
};

Vector3 lightDirectionsXChunk[6] = {
    {-CHUNK_SIZE_X, 0, 0},
    {CHUNK_SIZE_X, 0, 0},
    {0, CHUNK_SIZE_Y, 0},
    {0, -CHUNK_SIZE_Y, 0},
    {0, 0, CHUNK_SIZE_Z},
    {0, 0, -CHUNK_SIZE_Z }
};

int Chunk_GetLight(Chunk* chunk, Vector3 pos, bool sunLight) {
    if(!Chunk_IsValidPos(pos)) return 15;
    int index = Chunk_PosToIndex(pos);
    if(sunLight) {
        return chunk->sunlightData[index];
    } else {
        return chunk->lightData[index];
    }
}

void Chunk_LightQueueAdd(int index, Chunk *chunk) {

    pthread_mutex_lock(&light_mutex);

    LightNode node;
    node.index = index;
    node.chunk = chunk;
    arrput(lightQueue, node);

    pthread_mutex_unlock(&light_mutex);
}

void Chunk_LightDelQueueAdd(int index, int val, Chunk *chunk) {

    pthread_mutex_lock(&light_mutex);

    LightDelNode node;
    node.index = index;
    node.chunk = chunk;
    node.val = val;
    arrput(lightDelQueue, node);

    pthread_mutex_unlock(&light_mutex);
}

void Chunk_LightQueuePop(void) {

    pthread_mutex_lock(&light_mutex);

    if(arrlen(lightQueue) > 0) arrdel(lightQueue, 0);

    pthread_mutex_unlock(&light_mutex);
}

void Chunk_LightDelQueuePop(void) {
    pthread_mutex_lock(&light_mutex);

    if(arrlen(lightDelQueue) > 0) arrdel(lightDelQueue, 0);
    
    pthread_mutex_unlock(&light_mutex);
}

void Chunk_SetLightLevel(Chunk *chunk, int index, int level, bool sunlight) {
    if(sunlight) {
        chunk->sunlightData[index] = level;
    } else {
        chunk->lightData[index] = level;
    }
}

int Chunk_GetLightLevel(Chunk *chunk, int index, bool sunlight) {
    if(sunlight) {
        return chunk->sunlightData[index];
    } else {
        return chunk->lightData[index];
    }
}

void Chunk_DoLightSources(Chunk *srcChunk) {
    if(srcChunk == NULL) return;

    for(int i = 0; i < CHUNK_SIZE; i++) { 
        Block blockDefinition = Block_GetDefinition(srcChunk->data[i]);
        if(blockDefinition.lightType != BlockLightType_Emit) continue;

        
        Chunk_SetLightLevel(srcChunk, i, 15, false);
        Chunk_LightQueueAdd(i, srcChunk);
    }

    Chunk_SpreadLight(false);
}

void Chunk_DoSunlight(Chunk *srcChunk) {
    if(srcChunk == NULL) return;
 
    bool isTopLoaded = srcChunk->neighbours[2] != NULL;
    if(isTopLoaded) isTopLoaded = srcChunk->neighbours[2]->isLightGenerated == true;

    if(isTopLoaded) {
        Chunk* topChunk = srcChunk->neighbours[2];
        for(int i = 0; i < CHUNK_SIZE; i++) {
            if(topChunk->sunlightData[i] != 0) {
                Chunk_LightQueueAdd(i, topChunk);
            }
        }
    } else {
        if(srcChunk->position.y >= 5) {
            for(int i = CHUNK_SIZE - CHUNK_SIZE_XZ; i < CHUNK_SIZE; i++) {
                Block blockDefinition = Block_GetDefinition(srcChunk->data[i]);

                if(blockDefinition.renderType == BlockRenderType_Transparent) {
                    Chunk_SetLightLevel(srcChunk, i, 15, true);
                    Chunk_LightQueueAdd(i, srcChunk);
                }
            }
        } else {
            for(int d = 0; d < 6; d++) { 
                if(srcChunk->neighbours[d] != NULL) {
                    Chunk* neighbor = srcChunk->neighbours[d];
                    for(int i = 0; i < CHUNK_SIZE; i++) {
                        Vector3 pos = Chunk_IndexToPos(i);
                        if(pos.x == 0 || pos.x == 15 || pos.y == 0 || pos.y == 15 || pos.z == 0 || pos.z == 15) {
                            if(neighbor->sunlightData[i] != 0) {
                                Chunk_LightQueueAdd(i, neighbor);
                            }
                        }
                    }
                }
            }
        }
    }
    
    Chunk_SpreadLight(true);
    
}

void Chunk_SpreadLight(bool sunlight) {
    int limit = 100000;
    while(arrlen(lightQueue) > 0  && limit-- > 0) {
        //Get and remove first item in Queue
        int index = lightQueue[0].index;
        Chunk *chunk = lightQueue[0].chunk;
        Chunk_LightQueuePop();
        if(chunk == NULL) continue;

        int lightLevel = Chunk_GetLightLevel(chunk, index, sunlight);
        Vector3 pos = Chunk_IndexToPos(index);

        for(int d = 0; d < 6; d++) {
            Vector3 nextPos = Vector3Add(pos, lightDirections[d]);
            Chunk *nextChunk = chunk;

            //Goto neighbour chunk if out of bounds
            if(!Chunk_IsValidPos(nextPos)) {
                nextChunk = chunk->neighbours[d];
                nextPos = Vector3Subtract(nextPos, lightDirectionsXChunk[d]); 
                if(nextChunk == NULL) continue;
                if(!nextChunk->isMapGenerated) continue;
            }

            int nextIndex = Chunk_PosToIndex(nextPos);
            int nextLight = Chunk_GetLightLevel(nextChunk, nextIndex, sunlight);
            
            Block blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
            if(blockDefinition.renderType == BlockRenderType_Opaque && Block_IsFullSize(&blockDefinition)) continue;

            //Sunlight goes infinitely down
            int subVal = 1;
            if(sunlight && d == 3 && blockDefinition.renderType == BlockRenderType_Transparent) subVal = 0;

            if(nextLight + 1 + subVal <= lightLevel) {
                Chunk_SetLightLevel(nextChunk, nextIndex, lightLevel - subVal, sunlight);
                Chunk_LightQueueAdd(nextIndex, nextChunk);
            }
        }
    }

}

void Chunk_UpdateLight(bool sunlight) {
   int limit = 20000;
   while(arrlen(lightDelQueue) > 0 && limit-- > 0) {

        //Get and remove first item in Queue
        Chunk *chunk = lightDelQueue[0].chunk;
        int index = lightDelQueue[0].index;
        int lightLevel = lightDelQueue[0].val;
        Chunk_LightDelQueuePop();
        if(chunk == NULL) continue;

        Vector3 pos = Chunk_IndexToPos(index);
        
        for(int d = 0; d < 6; d++) {
            Vector3 nextPos = Vector3Add(pos, lightDirections[d]);
            Chunk *nextChunk = chunk;

            //Goto neighbour chunk if out of bounds
            if(!Chunk_IsValidPos(nextPos)) {
                nextChunk = chunk->neighbours[d];
                nextPos = Vector3Subtract(nextPos, lightDirectionsXChunk[d]); 
            }
            if(nextChunk == NULL) continue;

            int nextIndex = Chunk_PosToIndex(nextPos);
            int neighborLevel = Chunk_GetLightLevel(nextChunk, nextIndex, sunlight);

            if((neighborLevel != 0 && neighborLevel < lightLevel) || (lightLevel != 0 && d == 3 && sunlight == true)) {
                Chunk_SetLightLevel(nextChunk, nextIndex, 0, sunlight);
                Chunk_LightDelQueueAdd(nextIndex, neighborLevel, nextChunk);
            } else if(neighborLevel >= lightLevel) {
                
                Block blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
                if(blockDefinition.renderType == BlockRenderType_Opaque) continue;

                Chunk_LightQueueAdd(nextIndex, nextChunk);
                Chunk_SpreadLight(sunlight);
            } 
            
        }
    }
}

void Chunk_AddLightSource(Chunk *srcChunk, Vector3 srcPos, int intensity, bool sunlight) {
    if(srcChunk == NULL) return;

    int srcIndex = Chunk_PosToIndex(srcPos);
    
    Chunk_SetLightLevel(srcChunk, srcIndex, intensity, sunlight);
    Chunk_LightQueueAdd(srcIndex, srcChunk);
    Chunk_SpreadLight(sunlight);
}

void Chunk_RemoveLightSource(Chunk *srcChunk, Vector3 srcPos) {
    if(srcChunk == NULL) return;

    int srcIndex = Chunk_PosToIndex(srcPos);

    int srcVal = Chunk_GetLightLevel(srcChunk, srcIndex, false);
    Chunk_LightDelQueueAdd(srcIndex, srcVal, srcChunk);
    Chunk_SetLightLevel(srcChunk, srcIndex, 0, false);
    Chunk_UpdateLight(false);
}

void Chunk_RemoveSunlight(Chunk *srcChunk, Vector3 srcPos) {
    if(srcChunk == NULL) return;

    int srcIndex = Chunk_PosToIndex(srcPos);

    int srcVal = srcVal = Chunk_GetLightLevel(srcChunk, srcIndex, true);
    Chunk_LightDelQueueAdd(srcIndex, srcVal, srcChunk);
    Chunk_SetLightLevel(srcChunk, srcIndex, 0, true);
    Chunk_UpdateLight(true);
}