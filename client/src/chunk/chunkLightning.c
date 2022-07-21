/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include "chunkLightning.h"
#include "../block/block.h"
#include "world.h"
#include "player.h"

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

int Chunk_GetLight(Chunk* chunk, Vector3 pos) {
    if(!Chunk_IsValidPos(pos)) return 255;
    int index = Chunk_PosToIndex(pos);
    return 255 - fmin(chunk->lightData[index] + chunk->sunlightData[index], 15) * 17;
}

LightNode *Chunk_LightQueueAdd(LightNode *queue, int index, Chunk *chunk) {

    LightNode *head = queue;

    if(queue != NULL) {
        while(queue->next != NULL) {
            queue = queue->next;
        }
        queue->next = MemAlloc(sizeof(LightNode));
        queue = queue->next;
    } else {
        queue = MemAlloc(sizeof(LightNode));
        head = queue;
    }

    queue->chunk = chunk;
    queue->index = index;
    queue->next = NULL;

    return head;
}

LightDelNode *Chunk_LightDelQueueAdd(LightDelNode *queue, int index, int val, Chunk *chunk) {

    LightDelNode *head = queue;

    if(queue != NULL) {
        while(queue->next != NULL) {
            queue = queue->next;
        }
        queue->next = MemAlloc(sizeof(LightDelNode));
        queue = queue->next;
    } else {
        queue = MemAlloc(sizeof(LightDelNode));
        head = queue;
    }

    queue->chunk = chunk;
    queue->index = index;
    queue->val = val;
    queue->next = NULL;

    return head;
}

LightNode *Chunk_LightQueuePop(LightNode *lightQueue) {
    if(lightQueue == NULL) return NULL;

    LightNode *node = lightQueue->next;
    MemFree(lightQueue);
    return node;
}

LightDelNode *Chunk_LightDelQueuePop(LightDelNode *lightDelQueue) {
    if(lightDelQueue == NULL) return NULL;

    LightDelNode *node = lightDelQueue->next;
    MemFree(lightDelQueue);
    return node;
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

    LightNode *lightQueue = NULL;

    for(int i = 0; i < CHUNK_SIZE; i++) { 
        Block blockDefinition = Block_GetDefinition(srcChunk->data[i]);
        if(blockDefinition.lightType != BlockLightType_Emit) continue;

        
        Chunk_SetLightLevel(srcChunk, i, 15, false);
        lightQueue = Chunk_LightQueueAdd(lightQueue, i, srcChunk);
    }

    lightQueue = Chunk_SpreadLight(lightQueue, false);
}

void Chunk_DoSunlight(Chunk *srcChunk) {
    if(srcChunk == NULL) return;
 
    LightNode *lightQueue = NULL;

    bool isTopLoaded = srcChunk->neighbours[2] != NULL;
    if(isTopLoaded) isTopLoaded = srcChunk->neighbours[2]->isLightGenerated == true;

    if(isTopLoaded) {
        Chunk* topChunk = srcChunk->neighbours[2];
        for(int i = 0; i < CHUNK_SIZE; i++) {
            if(topChunk->sunlightData[i] != 0) {
                lightQueue = Chunk_LightQueueAdd(lightQueue, i, topChunk);
            }
        }
    } else {
        if(srcChunk->position.y >= fmax(floorf(player.position.y / CHUNK_SIZE_X) + fmin(world.drawDistance, 4), 5)) {
            for(int i = CHUNK_SIZE - CHUNK_SIZE_XZ; i < CHUNK_SIZE; i++) {
                Block blockDefinition = Block_GetDefinition(srcChunk->data[i]);

                if(blockDefinition.renderType == BlockRenderType_Transparent) {
                    Chunk_SetLightLevel(srcChunk, i, 15, true);
                    lightQueue = Chunk_LightQueueAdd(lightQueue, i, srcChunk);
                }
            }
        }
    }
    
    lightQueue = Chunk_SpreadLight(lightQueue, true);
    
}

LightNode *Chunk_SpreadLight(LightNode *lightQueue, bool sunlight) {
    while(lightQueue != NULL) {

        //Get and remove first item in Queue
        int index = lightQueue->index;
        Chunk *chunk = lightQueue->chunk;
        lightQueue = Chunk_LightQueuePop(lightQueue);
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
                lightQueue = Chunk_LightQueueAdd(lightQueue, nextIndex, nextChunk);
            }
        }
    }

    return lightQueue;
}

void Chunk_UpdateLight(LightNode *lightQueue, LightDelNode *lightDelQueue, bool sunlight) {
    while(lightDelQueue != NULL) {

        //Get and remove first item in Queue
        Chunk *chunk = lightDelQueue->chunk;
        int index = lightDelQueue->index;
        int lightLevel = lightDelQueue->val;
        lightDelQueue = Chunk_LightDelQueuePop(lightDelQueue);
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
                lightDelQueue = Chunk_LightDelQueueAdd(lightDelQueue, nextIndex, neighborLevel, nextChunk);
            } else if(neighborLevel >= lightLevel) {
                
                Block blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
                if(blockDefinition.renderType == BlockRenderType_Opaque) continue;

                lightQueue = Chunk_LightQueueAdd(lightQueue, nextIndex, nextChunk);
                lightQueue = Chunk_SpreadLight(lightQueue, sunlight);
            } 
            
        }
    }
}

void Chunk_AddLightSource(Chunk *srcChunk, Vector3 srcPos, int intensity, bool sunlight) {
    if(srcChunk == NULL) return;

    LightNode *lightQueue = NULL;
    int srcIndex = Chunk_PosToIndex(srcPos);
    
    Chunk_SetLightLevel(srcChunk, srcIndex, intensity, sunlight);
    lightQueue = Chunk_LightQueueAdd(lightQueue, srcIndex, srcChunk);
    lightQueue = Chunk_SpreadLight(lightQueue, sunlight);
}

void Chunk_RemoveLightSource(Chunk *srcChunk, Vector3 srcPos) {
    if(srcChunk == NULL) return;

    LightNode *lightQueue = NULL;
    LightDelNode *lightDelQueue = NULL;
    int srcIndex = Chunk_PosToIndex(srcPos);

    int srcVal = Chunk_GetLightLevel(srcChunk, srcIndex, false);
    lightDelQueue = Chunk_LightDelQueueAdd(lightDelQueue, srcIndex, srcVal, srcChunk);
    Chunk_SetLightLevel(srcChunk, srcIndex, 0, false);
    Chunk_UpdateLight(lightQueue, lightDelQueue, false);
}

void Chunk_RemoveSunlight(Chunk *srcChunk, Vector3 srcPos) {
    if(srcChunk == NULL) return;

    LightNode *lightQueue = NULL;
    LightDelNode *lightDelQueue = NULL;
    int srcIndex = Chunk_PosToIndex(srcPos);

    int srcVal = srcVal = Chunk_GetLightLevel(srcChunk, srcIndex, true);
    lightDelQueue = Chunk_LightDelQueueAdd(lightDelQueue, srcIndex, srcVal, srcChunk);
    Chunk_SetLightLevel(srcChunk, srcIndex, 0, true);
    Chunk_UpdateLight(lightQueue, lightDelQueue, true);
}