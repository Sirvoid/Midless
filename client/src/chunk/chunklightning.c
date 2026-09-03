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

static bool Block_BlocksLight(const Block *block) {
    return block->renderType == BLOCK_RENDER_OPAQUE && block->fullCube;
}

int Chunk_GetLight(Chunk* chunk, Vector3 pos, bool sunlight) {
    if (!Chunk_IsValidPos(pos)) return 15;
    int index = Chunk_PosToIndex(pos);
    if (sunlight) {
        return chunk->sunlightData[index];
    } else {
        return chunk->lightData[index];
    }
}

static void Chunk_LightQueueAdd(LightQueue *queue, int index, Chunk *chunk) {
    LightNode node = { .index = index, .chunk = chunk };
    arrput(queue->nodes, node);
}

static void Chunk_LightRemovalQueueAdd(LightRemovalQueue *queue, int index, int val, Chunk *chunk) {
    LightRemovalNode node = { .index = index, .val = val, .chunk = chunk };
    arrput(queue->nodes, node);
}

static bool Chunk_LightQueuePop(LightQueue *queue, LightNode *node) {
    if (queue->head >= (size_t)arrlen(queue->nodes)) return false;
    *node = queue->nodes[queue->head++];
    return true;
}

static bool Chunk_LightRemovalQueuePop(LightRemovalQueue *queue, LightRemovalNode *node) {
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

void Chunk_DoLightSources(Chunk *sourceChunk) {
    if (sourceChunk == NULL) return;
    LightQueue queue = {0};

    for (int i = 0; i < CHUNK_SIZE; i++) { 
        const Block *blockDefinition = Block_GetDefinition(sourceChunk->data[i]);
        if (blockDefinition->lightType != BLOCK_LIGHT_EMIT) continue;

        
        Chunk_SetLightLevel(sourceChunk, i, 15, false);
        Chunk_LightQueueAdd(&queue, i, sourceChunk);
    }

    Chunk_SpreadLight(&queue, false);
    arrfree(queue.nodes);
}

void Chunk_DoSunlight(Chunk *sourceChunk) {
    if (sourceChunk == NULL) return;
    LightQueue queue = {0};
 
    bool isTopLoaded = sourceChunk->neighbours[2] != NULL;
    if (isTopLoaded) isTopLoaded = sourceChunk->neighbours[2]->isLightGenerated == true;

    if (isTopLoaded) {
        Chunk* topChunk = sourceChunk->neighbours[2];
        for (int i = 0; i < CHUNK_SIZE_XZ; i++) {
            if (topChunk->sunlightData[i] != 0) {
                Chunk_LightQueueAdd(&queue, i, topChunk);
            }
        }
    } else {
        // This is the top of the currently loaded vertical column, so its top
        // face uses the server-provided per-column sky visibility mask.
        for (int i = CHUNK_SIZE - CHUNK_SIZE_XZ; i < CHUNK_SIZE; i++) {
            int column = i - (CHUNK_SIZE - CHUNK_SIZE_XZ);
            if ((sourceChunk->skyMask[column >> 3] & (1u << (column & 7))) == 0) continue;

            const Block *blockDefinition = Block_GetDefinition(sourceChunk->data[i]);

            if (!Block_BlocksLight(blockDefinition)) {
                int sunlight = blockDefinition->renderType == BLOCK_RENDER_TRANSPARENT ? 15 : 14;
                Chunk_SetLightLevel(sourceChunk, i, sunlight, true);
                Chunk_LightQueueAdd(&queue, i, sourceChunk);
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
            
            const Block *blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
            if (Block_BlocksLight(blockDefinition)) continue;

            //Sunlight goes infinitely down
            int subVal = 1;
            if (sunlight && d == 3 && blockDefinition->renderType == BLOCK_RENDER_TRANSPARENT) subVal = 0;

            if (nextLight + 1 + subVal <= lightLevel) {
                Chunk_SetLightLevel(nextChunk, nextIndex, lightLevel - subVal, sunlight);
                Chunk_LightQueueAdd(queue, nextIndex, nextChunk);
            }
        }
    }

}

void Chunk_UpdateLight(LightRemovalQueue *delQueue, LightQueue *spreadQueue, bool sunlight) {
    LightRemovalNode node;
    while (Chunk_LightRemovalQueuePop(delQueue, &node)) {
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
                Chunk_LightRemovalQueueAdd(delQueue, nextIndex, neighborLevel, nextChunk);
            } else if (neighborLevel != 0 && neighborLevel >= lightLevel) {
                const Block *blockDefinition = Block_GetDefinition(nextChunk->data[nextIndex]);
                if (Block_BlocksLight(blockDefinition)) continue;

                Chunk_LightQueueAdd(spreadQueue, nextIndex, nextChunk);
            } 
            
        }
    }
}

void Chunk_AddLightSource(Chunk *sourceChunk, Vector3 sourcePosition, int intensity, bool sunlight) {
    if (sourceChunk == NULL) return;
    LightQueue queue = {0};

    int srcIndex = Chunk_PosToIndex(sourcePosition);
    
    Chunk_SetLightLevel(sourceChunk, srcIndex, intensity, sunlight);
    Chunk_LightQueueAdd(&queue, srcIndex, sourceChunk);
    Chunk_SpreadLight(&queue, sunlight);
    arrfree(queue.nodes);
}

void Chunk_RemoveLightSource(Chunk *sourceChunk, Vector3 sourcePosition) {
    if (sourceChunk == NULL) return;
    LightQueue spreadQueue = {0};
    LightRemovalQueue delQueue = {0};

    int srcIndex = Chunk_PosToIndex(sourcePosition);

    int srcVal = Chunk_GetLightLevel(sourceChunk, srcIndex, false);
    Chunk_LightRemovalQueueAdd(&delQueue, srcIndex, srcVal, sourceChunk);
    Chunk_SetLightLevel(sourceChunk, srcIndex, 0, false);
    Chunk_UpdateLight(&delQueue, &spreadQueue, false);
    Chunk_SpreadLight(&spreadQueue, false);
    arrfree(delQueue.nodes);
    arrfree(spreadQueue.nodes);
}

void Chunk_RemoveSunlight(Chunk *sourceChunk, Vector3 sourcePosition) {
    if (sourceChunk == NULL) return;
    LightQueue spreadQueue = {0};
    LightRemovalQueue delQueue = {0};

    int srcIndex = Chunk_PosToIndex(sourcePosition);

    int srcVal = Chunk_GetLightLevel(sourceChunk, srcIndex, true);
    Chunk_LightRemovalQueueAdd(&delQueue, srcIndex, srcVal, sourceChunk);
    Chunk_SetLightLevel(sourceChunk, srcIndex, 0, true);
    Chunk_UpdateLight(&delQueue, &spreadQueue, true);
    Chunk_SpreadLight(&spreadQueue, true);
    arrfree(delQueue.nodes);
    arrfree(spreadQueue.nodes);
}
