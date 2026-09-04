/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if !defined(PLATFORM_WEB)
    #define __clang__ true
#endif
#define STB_DS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "raylib.h"
#include "stb_ds.h"
#include "rlgl.h"
#include "raymath.h"
#include "world.h"
#include "player.h"
#include "chunkmeshgeneration.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "entitymodel.h"
#include "localserver.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

World world;

void World_Init(void) {
    world.material = LoadMaterialDefault();
    world.loadChunks = false;
    world.drawDistance = 8;
    world.time = 0;

    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0; //type 0 = none

    ChunkMeshGeneration_Init();
}

void World_LoadMultiplayer(void) {
    player.position = (Vector3) { 0, 80, 0 };
    Screen_Switch(SCREEN_GAME);
    world.loadChunks = true;
}

void World_LoadSingleplayer(void) {
    LocalServer_Start();
}

void World_UpdateChunksWithBudget(double budgetMs) {
    double endTime = GetTime() + budgetMs / 1000.0;

    while (arrlen(world.generateChunksQueue) > 0) {
        World_ReadChunksQueues();

        if (GetTime() >= endTime)
            break;
    }
}

void World_Update(void) { 
    world.time += GetFrameTime();
    while (world.time >= WORLD_DAY_LENGTH_SECONDS) {
        world.time -= WORLD_DAY_LENGTH_SECONDS;
    }

    World_UpdateChunksWithBudget(4.0);
    
}

void World_ReadChunksQueues(void) {

        if (world.loadChunks == true) {

            int index = World_GetClosestChunkIndex(world.generateChunksQueue, Player_GetChunkPosition());

            if (index != -1) {
                Chunk *chunk = world.generateChunksQueue[index];

                if(!chunk->isBuilt) {
                    for (int i = 0; i < 6; i++) {
                        if (chunk->neighbours[i] == NULL) continue;
                        World_QueueChunk(chunk->neighbours[i], false);
                    }
                }

                Chunk_Generate(chunk);
                ChunkMeshGeneration_Build(chunk);

                arrdel(world.generateChunksQueue, index);

                chunk->isGenerating = false;

                for (int i = 0; i < hmlen(world.chunks); i++) {
                    Chunk *lightDirtyChunk = world.chunks[i].value;
                    if (lightDirtyChunk->isBuilt && lightDirtyChunk->isLightDirty)
                        World_QueueChunk(lightDirtyChunk, false);
                }
            }
            
        }  
}

void World_QueueChunk(Chunk *chunk, bool immediate) {

    if (chunk->isGenerating == false) {
        if(!immediate) {
            arrput(world.generateChunksQueue, chunk);
        } else {
            arrins(world.generateChunksQueue, 0, chunk);
        }
    }
    chunk->isGenerating = true;
    
}


Chunk* World_GetChunkAt(Vector3 position) {
    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index >= 0) {
        return world.chunks[index].value;
    }
    
    return NULL;
}

int World_GetClosestChunkIndex(Chunk* *array, Vector3 pos) {
    int arrLength = arrlen(array);
    if (arrLength > 0) {
        Chunk* queuedChunk = array[0];
        int index = 0;
        for (int i = 0; i < arrLength; i++) {
            if (Vector3Distance(array[i]->position, pos) < Vector3Distance(queuedChunk->position, pos)) {
                queuedChunk = array[i];
                index = i;
            }
        }
        return index;
    }

    return -1;
}

void World_AddChunk(Vector3 position) {

    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index == -1) {
        Chunk *newChunk = Chunk_Create(position);
        if (newChunk == NULL) return;

        hmput(world.chunks, p, newChunk);
        World_QueueChunk(newChunk, false);
        
    }
}

void World_RemoveChunk(Chunk *currentChunk) {

    if(currentChunk->isGenerating == true) {
        for(int i = 0; i < arrlen(world.generateChunksQueue); i++) {
            if(world.generateChunksQueue[i] == currentChunk) {
                arrdel(world.generateChunksQueue, i);
            }
        }
    }

    long int p = Chunk_GetPackedPos(currentChunk->position);
    hmdel(world.chunks, p);

    Chunk_UpdateNeighbours(currentChunk, true);
    if (currentChunk->modified) Chunk_SaveFile(currentChunk);
    Chunk_Unload(currentChunk);
    Chunk_Destroy(currentChunk);
}

void World_LoadChunks(void) {

    if (!world.loadChunks || networkConnectedToServer) return;

    Vector3 pos = Player_GetChunkPosition();

    //Create chunks or prepare array of chunks to be sorted
    int loadingHeight = fmin(world.drawDistance, 4);
    for (int y = loadingHeight; y >= -loadingHeight; y--) {
        for (int x = -world.drawDistance ; x <= world.drawDistance; x++) {
            for (int z = -world.drawDistance ; z <= world.drawDistance; z++) {
                Vector3 chunkPos = (Vector3) {pos.x + x, pos.y + y, pos.z + z};

                if (Vector3Distance(chunkPos, pos) < world.drawDistance) {
                    World_AddChunk(chunkPos);
                }
            }
        }
    }
    
    //destroy far chunks
    for (int i = hmlen(world.chunks) - 1; i >= 0 ; i--) {
        Chunk *chunk = world.chunks[i].value;

        if (Vector3Distance(chunk->position, pos) >= world.drawDistance) {
            World_RemoveChunk(chunk);
        }
    }
    
}

void World_Reload(void) {
    if (!networkConnectedToServer) World_Clear();
    world.loadChunks = true;
}

void World_Clear(void) {
    world.loadChunks = false;

    arrfree(world.generateChunksQueue);
    world.generateChunksQueue = NULL;

    for (int i = hmlen(world.chunks) - 1; i >= 0; i--) {
        World_RemoveChunk(world.chunks[i].value);
    }

    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        World_RemoveEntity(i);
    }

    hmfree(world.chunks);
    world.chunks = NULL;

}

void World_Shutdown(void) {
    World_Clear();
    UnloadMaterial(world.material);
    MemFree(world.entities);
    world.entities = NULL;
    ChunkMeshGeneration_Shutdown();
}

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.material, MATERIAL_MAP_DIFFUSE, texture);
}

void World_ApplyShader(Shader shader) {
    world.material.shader = shader;
}

void World_Draw(Vector3 camPosition) {

    ChunkMesh_PrepareDrawing(world.material);

    int amountChunks = hmlen(world.chunks);
    float frustumAngle = DEG2RAD * player.camera.fovy + 0.3f;
    Vector3 dirVec = Player_GetForwardVector();
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } sortedChunks[amountChunks];

    int sortedLength = 0;
    for (int i=0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;

        if (chunk->onlyAir) continue;

        if (chunk->hasTransparency) {
            Vector3 centerChunk = Vector3Add(chunk->blockPosition, chunkLocalCenter);
            float distFromCam = Vector3Distance(centerChunk, camPosition);

            //Don't draw chunks behind the player
            Vector3 toChunkVec = Vector3Normalize(Vector3Subtract(centerChunk, camPosition));
        
            if (distFromCam > CHUNK_SIZE_X && Vector3Distance(toChunkVec, dirVec) > frustumAngle) {
                continue;
            }

            sortedChunks[sortedLength].dist = distFromCam;
            sortedChunks[sortedLength].chunk = chunk;
            sortedLength++;
        } else {
            Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                0, 1, 0, chunk->blockPosition.y,
                0, 0, 1, chunk->blockPosition.z,
                0, 0, 0, 1 };
        
            ChunkMesh_Draw(&chunk->mesh, world.material, matrix);
        }
    }
    
    //Sort chunks back to front
    for (int i = 1; i < sortedLength; i++) {
        int j = i;
        while (j > 0 && sortedChunks[j-1].dist <= sortedChunks[j].dist) {
            struct { Chunk *chunk; float dist; } tempC;
            tempC.chunk = sortedChunks[j].chunk;
            tempC.dist = sortedChunks[j].dist;

            sortedChunks[j] = sortedChunks[j - 1];
            sortedChunks[j - 1].chunk = tempC.chunk;
            sortedChunks[j - 1].dist = tempC.dist;
            j = j - 1;
        }
    }
    
    ChunkMesh_PrepareDrawing(world.material);

    //Draw sorted chunks
    for (int i = 0; i < sortedLength; i++) {
        Chunk *chunk = sortedChunks[i].chunk;

        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };
        
        ChunkMesh_Draw(&chunk->mesh, world.material, matrix);
        rlDisableBackfaceCulling();
        ChunkMesh_Draw(&chunk->meshTransparent, world.material, matrix);
        rlEnableBackfaceCulling();
    }

    ChunkMesh_FinishDrawing();

    //Draw entities
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        if (world.entities[i].type == 0) continue;
        Entity_Draw(&world.entities[i]);
    }

}

int World_GetBlock(Vector3 blockPos) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunk->blockPosition.x,
                                floor(blockPos.y) - chunk->blockPosition.y, 
                                floor(blockPos.z) - chunk->blockPosition.z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_SetBlock(Vector3 blockPos, int blockId, bool immediate) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };

    if (!chunk->isLightGenerated) {
        if (Chunk_IsValidPos(blockPosInChunk)) {
            chunk->data[Chunk_PosToIndex(blockPosInChunk)] = blockId;
        }
        return;
    }
    
    Chunk_SetBlock(chunk, blockPosInChunk, blockId);

    if (blockId == 0) {
        World_QueueChunk(chunk, immediate);
        for (int i = 0; i < 26; i++) {
            if (chunk->neighbours[i] == NULL) continue;
            World_QueueChunk(chunk->neighbours[i], immediate);
        }
    } else {
        for (int i = 0; i < 26; i++) {
            if (chunk->neighbours[i] == NULL) continue;
            World_QueueChunk(chunk->neighbours[i], immediate);
        }
        World_QueueChunk(chunk, immediate); 
    }

}

float World_GetSunlightStrength(void) {
    return fmax(abs((int)(world.time - WORLD_DAY_LENGTH_SECONDS / 2.0f)) / (WORLD_DAY_LENGTH_SECONDS / 2.0f), 2/16.0f);
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Entities-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    Entity *entity = &world.entities[id];
    entity->position = position;
    entity->rotation = (Vector3) { 0, rotation.y, 0 };
    
    for (int i = 0; i < entity->model.partCount; i++) {
        if (entity->model.parts[i].type == PART_TYPE_HEAD) {
            entity->model.parts[i].rotation.x = rotation.x;
        }
    }
}

void World_AddEntity(int id, int type, Vector3 position, Vector3 rotation) {
    world.entities[id].type = type;
    world.entities[id].position = position;
    world.entities[id].rotation = rotation;
    
    EntityModel_Create(&world.entities[id].model, entityModels[0]);
}

void World_RemoveEntity(int id) {
    Entity_Destroy(&world.entities[id]);
}
