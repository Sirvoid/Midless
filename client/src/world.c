/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define __clang__ true
#define STB_DS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include "raylib.h"
#include "stb_ds.h"
#include "rlgl.h"
#include "raymath.h"
#include "chunkmeshgeneration.h"
#include "world.h"
#include "worldgenerator.h"
#include "player.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"

World world;
pthread_t chunkThread_id;

void World_Init(void) {
    world.mat = LoadMaterialDefault();
    world.loadChunks = false;
    world.drawDistance = 8;
    world.time = 0;

    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0; //type 0 = none

    Chunk_MeshGenerationInit();

    int seed = rand();

    //Create world directory
    struct stat st = {0};
    if (stat("./world", &st) == -1) {
        mkdir("./world");
    }
    
    if(FileExists("./world/seed.dat")) {
        unsigned int bytesRead = 0;
        unsigned char *data = LoadFileData("./world/seed.dat", &bytesRead);
        seed = (int)(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]); 
        UnloadFileData(data);
    } else {
        char data[4] = {(char)(seed >> 24), (char)(seed >> 16), (char)(seed >> 8), (char)(seed)};
        SaveFileData("./world/seed.dat", data, 4);
    }

    WorldGenerator_Init(seed);

    pthread_create(&chunkThread_id, NULL, World_ReadChunksQueues, NULL);
}

void World_LoadMultiplayer(void) {
    player.position = (Vector3) { 0, 80, 0 };
    Screen_Switch(SCREEN_GAME);
    world.loadChunks = true;
}

void World_LoadSingleplayer(void) {

    //Prevent multiplayer chunks from being unloaded during a singleplayer game which causes them to be saved locally.
    if(hmlen(world.chunks) != 0) return;

    player.position = (Vector3) { 0, 80, 0 };
    Network_connectedToServer = false;
    Screen_Switch(SCREEN_GAME);
    world.loadChunks = true;
    World_LoadChunks();
}

clock_t updateClock;
void World_Update(void) { 
    
    clock_t newClock = clock();
    float time_spent = (float)(newClock - updateClock) / CLOCKS_PER_SEC;
    updateClock = newClock;

    world.time += time_spent;
    if(world.time >= WORLD_DAY_LENGTH_SECONDS) world.time = 0;
    World_UpdateChunks();
}

pthread_mutex_t chunk_mutex;
pthread_mutex_t genChunk_mutex;
void *World_ReadChunksQueues(void *state) {
    while(true) {

        pthread_mutex_lock(&chunk_mutex);
        
        if(world.loadChunks == true) {

            Vector3 pos = (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};
            int index = World_GetClosestChunkIndex(world.generateChunksQueue, pos);

            if(index != -1) {
                Chunk_Generate(world.generateChunksQueue[index]);
                pthread_mutex_lock(&genChunk_mutex);
                arrdel(world.generateChunksQueue, index);
                pthread_mutex_unlock(&genChunk_mutex);
            }
            
        }
        
        pthread_mutex_unlock(&chunk_mutex);
        
    }
    return NULL;
}

void World_QueueChunk(Chunk *chunk) {

    if(chunk->hasStartedGenerating == false) {
        pthread_mutex_lock(&genChunk_mutex);
        arrput(world.generateChunksQueue, chunk);
        pthread_mutex_unlock(&genChunk_mutex);

    }
    chunk->hasStartedGenerating = true;

    if(chunk->isBuilding == false) {
        arrput(world.buildChunksQueue, chunk);
        chunk->isBuilding = true;
    }
}


Chunk* World_GetChunkAt(Vector3 position) {
    long int p = (long)((int)(position.x)&4095)<<20 | (long)((int)(position.z)&4095)<<8 | (long)((int)(position.y)&255);
    int index = hmgeti(world.chunks, p);
    if(index >= 0) {
        return world.chunks[index].value;
    }
    
    return NULL;
}


int World_GetClosestChunkIndex(Chunk* *array, Vector3 pos) {
    int arrLength = arrlen(array);
    if(arrLength > 0) {
        Chunk* queuedChunk = array[0];
        int index = 0;
        for(int i = 0; i < arrLength; i++) {
            if(Vector3Distance(array[i]->position, pos) < Vector3Distance(queuedChunk->position, pos)) {
                queuedChunk = array[i];
                index = i;
            }
        }
        return index;
    }

    return -1;
}

void World_AddChunk(Vector3 position) {

    long int p = (long)((int)(position.x)&4095)<<20 | (long)((int)(position.z)&4095)<<8 | (long)((int)(position.y)&255);
    int index = hmgeti(world.chunks, p);
    if(index == -1) {
        //Add chunk to list
        Chunk *newChunk = MemAlloc(sizeof(Chunk));
        hmput(world.chunks, p, newChunk);

        Chunk_Init(newChunk, position);

        World_QueueChunk(newChunk);
        Chunk_RefreshBorderingChunks(newChunk, true);
    }
}

void World_RemoveChunk(Chunk *curChunk) {
    if(curChunk->beingDeleted) return;

    long int p = (long)((int)(curChunk->position.x)&4095)<<20 | (long)((int)(curChunk->position.z)&4095)<<8 | (long)((int)(curChunk->position.y)&255);
    
    int index = hmgeti(world.chunks, p);
    if(index >= 0) {
        curChunk->beingDeleted = true;
        arrput(world.deleteChunksQueue, curChunk);
    }
    
}

void World_UpdateChunks(void) {

        int meshUpdatesCount = 4;

        for (int i = 0; i < meshUpdatesCount; i++) {
            Vector3 pos = (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};
            int index = World_GetClosestChunkIndex(world.buildChunksQueue, pos);
            if(index == -1) continue;
            Chunk *chunk = world.buildChunksQueue[index];
            if(chunk->isLightGenerated == true) {
                if(Chunk_AreNeighbourGenerated(chunk) == true) {
                    Chunk_BuildMesh(chunk);
                    chunk->isBuilding = false;
                    arrdel(world.buildChunksQueue, index);
                    continue;
                }
            }
        }

        for(int i = arrlen(world.deleteChunksQueue) - 1; i >= 0 ; i--) {
            Chunk* chunk = world.deleteChunksQueue[i];
            if(chunk->isBuilt == true && chunk->isBuilding == false) {
                if(Chunk_AreNeighbourBuilding(chunk) == false) {
                    long int p = (long)((int)(chunk->position.x)&4095)<<20 | (long)((int)(chunk->position.z)&4095)<<8 | (long)((int)(chunk->position.y)&255);
                    Chunk_Unload(chunk);
                    hmdel(world.chunks, p);
                    arrdel(world.deleteChunksQueue, i);
                    continue;
                }
            }
        }
}

void World_LoadChunks(void) {

    if(!world.loadChunks || Network_connectedToServer) return;

    Vector3 pos = (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};

    //Create chunks or prepare array of chunks to be sorted
    int loadingHeight = fmin(world.drawDistance, 4);
    for(int y = loadingHeight; y >= -loadingHeight; y--) {
        for(int x = -world.drawDistance ; x <= world.drawDistance; x++) {
            for(int z = -world.drawDistance ; z <= world.drawDistance; z++) {
                Vector3 chunkPos = (Vector3) {pos.x + x, pos.y + y, pos.z + z};
                if(Vector3Distance(chunkPos, pos) < world.drawDistance + 3) {
                    World_AddChunk(chunkPos);
                }
            }
        }
    }
    
    //destroy far chunks
    for (int i = hmlen(world.chunks) - 1; i >= 0 ; i--) {
        Chunk *chunk = world.chunks[i].value;
        if(Vector3Distance(chunk->position, pos) >= world.drawDistance + 3) {
            World_RemoveChunk(chunk);
        }
    }
    
}

void World_Reload(void) {
    if(!Network_connectedToServer) World_Unload();
    world.loadChunks = true;
}

void World_Unload(void) {
    world.loadChunks = false;

    pthread_mutex_lock(&chunk_mutex);

    arrfree(world.generateChunksQueue);
    arrfree(world.buildChunksQueue);
    world.generateChunksQueue = NULL;
    world.buildChunksQueue = NULL;

    for(int i = hmlen(world.chunks) - 1; i >= 0; i--) {
        World_RemoveChunk(world.chunks[i].value);
    }

    world.chunks = NULL;

    pthread_mutex_unlock(&chunk_mutex);
    
}

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.mat, MATERIAL_MAP_DIFFUSE, texture);
}

void World_ApplyShader(Shader shader) {
    world.mat.shader = shader;
}

void World_Draw(Vector3 camPosition) {

    ChunkMesh_PrepareDrawing(world.mat);

    int amountChunks = hmlen(world.chunks);
    float frustumAngle = DEG2RAD * player.camera.fovy + 0.3f;
    Vector3 dirVec = Player_GetForwardVector();
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } sortedChunks[amountChunks];

    int sortedLength = 0;
    for (int i=0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;
        if(chunk->onlyAir) continue;
        if(chunk->hasTransparency) {
            Vector3 centerChunk = Vector3Add(chunk->blockPosition, chunkLocalCenter);
            float distFromCam = Vector3Distance(centerChunk, camPosition);

            //Don't draw chunks behind the player
            Vector3 toChunkVec = Vector3Normalize(Vector3Subtract(centerChunk, camPosition));
        
            if(distFromCam > CHUNK_SIZE_X && Vector3Distance(toChunkVec, dirVec) > frustumAngle) {
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
        
            ChunkMesh_Draw(&chunk->mesh, world.mat, matrix);
        }
    }
    
    //Sort chunks back to front
    for(int i = 1; i < sortedLength; i++) {
        int j = i;
        while(j > 0 && sortedChunks[j-1].dist <= sortedChunks[j].dist) {
            struct { Chunk *chunk; float dist; } tempC;
            tempC.chunk = sortedChunks[j].chunk;
            tempC.dist = sortedChunks[j].dist;

            sortedChunks[j] = sortedChunks[j - 1];
            sortedChunks[j - 1].chunk = tempC.chunk;
            sortedChunks[j - 1].dist = tempC.dist;
            j = j - 1;
        }
    }
    
    ChunkMesh_PrepareDrawing(world.mat);

    //Draw sorted chunks
    for(int i = 0; i < sortedLength; i++) {
        Chunk *chunk = sortedChunks[i].chunk;

        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };
        
        ChunkMesh_Draw(&chunk->mesh, world.mat, matrix);
        rlDisableBackfaceCulling();
        ChunkMesh_Draw(&chunk->meshTransparent, world.mat, matrix);
        rlEnableBackfaceCulling();
    }

    ChunkMesh_FinishDrawing();

    //Draw entities
    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        if(world.entities[i].type == 0) continue;
        Entity_Draw(&world.entities[i]);
    }

}

int World_GetBlock(Vector3 blockPos) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunk->blockPosition.x,
                                floor(blockPos.y) - chunk->blockPosition.y, 
                                floor(blockPos.z) - chunk->blockPosition.z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_SetBlock(Vector3 blockPos, int blockID, bool immediate) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if(chunk == NULL) return;
    if(chunk->isLightGenerated == false) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };
    
    Chunk_SetBlock(chunk, blockPosInChunk, blockID);

    if(blockID == 0) {
        //Refresh mesh of neighbour chunks.
        Chunk_RefreshBorderingChunks(chunk, false);

        if(immediate == true) {
            Chunk_BuildMesh(chunk);
        } else {
            //Refresh current chunk.
            World_QueueChunk(chunk);
        }
    } else {

        if(immediate == true) {
            Chunk_BuildMesh(chunk);
        } else {
            //Refresh current chunk.
            World_QueueChunk(chunk);
        }

        //Refresh mesh of neighbour chunks.
        Chunk_RefreshBorderingChunks(chunk, false);
    }

}

float World_GetSunlightStrength(void) {
    return fmax(abs(world.time - WORLD_DAY_LENGTH_SECONDS / 2.0f) / (WORLD_DAY_LENGTH_SECONDS / 2.0f), 2/16.0f);
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Entities-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_TeleportEntity(int ID, Vector3 position, Vector3 rotation) {
    Entity *entity = &world.entities[ID];
    entity->position = position;
    entity->rotation = (Vector3) { 0, rotation.y, 0 };
    
    for(int i = 0; i < entity->model.amountParts; i++) {
        if(entity->model.parts[i].type == PartType_Head) {
            entity->model.parts[i].rotation.x = rotation.x;
        }
    }
}

void World_AddEntity(int ID, int type, Vector3 position, Vector3 rotation) {
    world.entities[ID].type = type;
    world.entities[ID].position = position;
    world.entities[ID].rotation = rotation;
    
    EntityModel_Build(&world.entities[ID].model, entityModels[0]);
}

void World_RemoveEntity(int ID) {
    Entity_Remove(&world.entities[ID]);
}
