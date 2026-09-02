/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stddef.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "chunklightning.h"
#include "block.h"

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
    if (!Chunk_IsValidPos(pos)) return 15;
    int index = Chunk_PosToIndex(pos);
    if (sunLight) {
        return chunk->sunlightData[index];
    } else {
        return chunk->lightData[index];
    }
}

static void Chunk_LightQueueAdd(LightQueue *queue, int index, Chunk *chunk) {
    LightNode node = { .index = index, .chunk = chunk };
    arrput(queue->nodes, node);
}

static void Chunk_LightDelQueueAdd(LightDelQueue *queue, int index, int val, Chunk *chunk) {
    LightDelNode node = { .index = index, .val = val, .chunk = chunk };
    arrput(queue->nodes, node);
}

static bool Chunk_LightQueuePop(LightQueue *queue, LightNode *node) {
    if (queue->head >= (size_t)arrlen(queue->nodes)) return false;
    *node = queue->nodes[queue->head++];
    return true;
}

static bool Chunk_LightDelQueuePop(LightDelQueue *queue, LightDelNode *node) {
    if (queue->head >= (size_t)arrlen(queue->nodes)) return false;
    *node = queue->nodes[queue->head++];
    return true;
}

void Chunk_SetLightLevel(Chunk *chunk, int index, int level, bool sunlight) {
    if (sunlight) {
        chunk->sunlightData[index] = level;
    } else {
        chunk->lightData[index] = level;
    }
}

int Chunk_GetLightLevel(Chunk *chunk, int index, bool sunlight) {
    if (sunlight) {
        return chunk->sunlightData[index];
    } else {
        return chunk->lightData[index];
    }
}

void Chunk_DoLightSources(Chunk *srcChunk) {
    if (srcChunk == NULL) return;
    LightQueue queue = {0};

    for (int i = 0; i < CHUNK_SIZE; i++) { 
        Block blockDefinition = Block_GetDefinition(srcChunk->data[i]);
        if (blockDefinition.lightType != BlockLightType_Emit) continue;

        
        Chunk_SetLightLevel(srcChunk, i, 15, false);
        Chunk_LightQueueAdd(&queue, i, srcChunk);
    }

    Chunk_SpreadLight(&queue, false);
    arrfree(queue.nodes);
}

void Chunk_DoSunlight(Chunk *srcChunk) {
    if (srcChunk == NULL) return;
    LightQueue queue = {0};
 
    bool isTopLoaded = srcChunk->neighbours[2] != NULL;
    if (isTopLoaded) isTopLoaded = srcChunk->neighbours[2]->isLightGenerated == true;

    if (isTopLoaded) {
        Chunk* topChunk = srcChunk->neighbours[2];
        for (int i = 0; i < CHUNK_SIZE_XZ; i++) {
            if (topChunk->sunlightData[i] != 0) {
                Chunk_LightQueueAdd(&queue, i, topChunk);
            }
        }
    } else {
        if (srcChunk->position.y >= 3) {
            for (int i = CHUNK_SIZE - CHUNK_SIZE_XZ; i < CHUNK_SIZE; i++) {
                Block blockDefinition = Block_GetDefinition(srcChunk->data[i]);

                if (blockDefinition.renderType == BlockRenderType_Transparent) {
                    Chunk_SetLightLevel(srcChunk, i, 15, true);
                    Chunk_LightQueueAdd(&queue, i, srcChunk);
                }
            }
        }
    }
    
    Chunk_SpreadLight(&queue, true);
    arrfree(queue.nodes);
}

void Chunk_SpreadLight(LightQueue *queue, bool sunlight) {
    LightNode node;
    while (Chunk_LightQueuePop(queue, &node)) {
        int index = node.index;
        Chunk *chunk = node.chunk;
        if (chunk == NULL) continue;

        int lightLevel = Chunk_GetLightLevel(chunk, index, sunlight);
        Vector3 pos = Chunk_IndexToPos(index);

        for (int d = 0; d < 6; d++) {
            Vector3 nextPos = Vector3Add(pos, lightDirections[d]);
            Chunk *nextChunk = chunk;

            //Goto neighbour chunk if out of bounds
            if (!Chunk_IsValidPos(nextPos)) {
                nextChunk = chunk->neighbours[d];
                nextPos = Vector3Subtract(nextPos, lightDirectionsXChunk[d]); 
                if (nextChunk == NULL) continue;
                if (!nextChunk->isMapGenerated) continue;
            }

            int nextIndex = Chunk_PosToIndex(nextPos);
            int nextLight = Chunk_GetLightLevel(nextChunk, nextIndex, sunlight);
            
            Block blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
            if (blockDefinition.renderType == BlockRenderType_Opaque && Block_IsFullSize(&blockDefinition)) continue;

            //Sunlight goes infinitely down
            int subVal = 1;
            if (sunlight && d == 3 && blockDefinition.renderType == BlockRenderType_Transparent) subVal = 0;

            if (nextLight + 1 + subVal <= lightLevel) {
                Chunk_SetLightLevel(nextChunk, nextIndex, lightLevel - subVal, sunlight);
                Chunk_LightQueueAdd(queue, nextIndex, nextChunk);
            }
        }
    }

}

void Chunk_UpdateLight(LightDelQueue *delQueue, LightQueue *spreadQueue, bool sunlight) {
    LightDelNode node;
    while (Chunk_LightDelQueuePop(delQueue, &node)) {
        Chunk *chunk = node.chunk;
        int index = node.index;
        int lightLevel = node.val;
        if (chunk == NULL) continue;

        Vector3 pos = Chunk_IndexToPos(index);
        
        for (int d = 0; d < 6; d++) {
            Vector3 nextPos = Vector3Add(pos, lightDirections[d]);
            Chunk *nextChunk = chunk;

            //Goto neighbour chunk if out of bounds
            if (!Chunk_IsValidPos(nextPos)) {
                nextChunk = chunk->neighbours[d];
                nextPos = Vector3Subtract(nextPos, lightDirectionsXChunk[d]); 
            }
            if (nextChunk == NULL) continue;

            int nextIndex = Chunk_PosToIndex(nextPos);
            int neighborLevel = Chunk_GetLightLevel(nextChunk, nextIndex, sunlight);

            if (neighborLevel != 0 &&
                (neighborLevel < lightLevel || (lightLevel != 0 && d == 3 && sunlight))) {
                Chunk_SetLightLevel(nextChunk, nextIndex, 0, sunlight);
                Chunk_LightDelQueueAdd(delQueue, nextIndex, neighborLevel, nextChunk);
            } else if (neighborLevel != 0 && neighborLevel >= lightLevel) {
                Block blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
                if (blockDefinition.renderType == BlockRenderType_Opaque) continue;

                Chunk_LightQueueAdd(spreadQueue, nextIndex, nextChunk);
            } 
            
        }
    }
}

void Chunk_AddLightSource(Chunk *srcChunk, Vector3 srcPos, int intensity, bool sunlight) {
    if (srcChunk == NULL) return;
    LightQueue queue = {0};

    int srcIndex = Chunk_PosToIndex(srcPos);
    
    Chunk_SetLightLevel(srcChunk, srcIndex, intensity, sunlight);
    Chunk_LightQueueAdd(&queue, srcIndex, srcChunk);
    Chunk_SpreadLight(&queue, sunlight);
    arrfree(queue.nodes);
}

void Chunk_RemoveLightSource(Chunk *srcChunk, Vector3 srcPos) {
    if (srcChunk == NULL) return;
    LightQueue spreadQueue = {0};
    LightDelQueue delQueue = {0};

    int srcIndex = Chunk_PosToIndex(srcPos);

    int srcVal = Chunk_GetLightLevel(srcChunk, srcIndex, false);
    Chunk_LightDelQueueAdd(&delQueue, srcIndex, srcVal, srcChunk);
    Chunk_SetLightLevel(srcChunk, srcIndex, 0, false);
    Chunk_UpdateLight(&delQueue, &spreadQueue, false);
    Chunk_SpreadLight(&spreadQueue, false);
    arrfree(delQueue.nodes);
    arrfree(spreadQueue.nodes);
}

void Chunk_RemoveSunlight(Chunk *srcChunk, Vector3 srcPos) {
    if (srcChunk == NULL) return;
    LightQueue spreadQueue = {0};
    LightDelQueue delQueue = {0};

    int srcIndex = Chunk_PosToIndex(srcPos);

    int srcVal = Chunk_GetLightLevel(srcChunk, srcIndex, true);
    Chunk_LightDelQueueAdd(&delQueue, srcIndex, srcVal, srcChunk);
    Chunk_SetLightLevel(srcChunk, srcIndex, 0, true);
    Chunk_UpdateLight(&delQueue, &spreadQueue, true);
    Chunk_SpreadLight(&spreadQueue, true);
    arrfree(delQueue.nodes);
    arrfree(spreadQueue.nodes);
}