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
#include <raylib.h>
#include <sys/stat.h>
#include <stb_ds.h>
#include "chunkMeshGeneration.h"
#include "world.h"
#include "rlgl.h"
#include "raymath.h"
#include "worldgenerator.h"
#include "player.h"
#include "screens.h"
#include "networkhandler.h"

World world;
pthread_t chunkThread_id;

void World_Init(void) {
    world.mat = LoadMaterialDefault();
    world.loadChunks = false;
    world.drawDistance = 4;

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

void World_LoadSingleplayer(void) {

    Screen_Switch(SCREEN_GAME);

    world.loadChunks = true;
    World_LoadChunks(false);
}

clock_t begin;
pthread_mutex_t chunk_mutex;
void *World_ReadChunksQueues(void *state) {
    while(true) {

        pthread_mutex_lock(&chunk_mutex);
        
        QueuedChunk *queuedChunk = world.generateChunksQueue;
        
        if(world.loadChunks == true && queuedChunk != NULL) {
            Chunk_Generate(queuedChunk->chunk);

            if(arrlen(world.generateChunksQueue) > 0) arrdel(world.generateChunksQueue, 0);
        }
        
        pthread_mutex_unlock(&chunk_mutex);
        
    }
    return NULL;
}

void World_QueueChunk(Chunk *chunk) {

    if(chunk->hasStartedGenerating == false) {
        QueuedChunk queued;
        queued.chunk = chunk;
        queued.state = 0;
        arrput(world.generateChunksQueue, queued);

    }
    chunk->hasStartedGenerating = true;

    if(chunk->isBuilding == false) {
        QueuedChunk queued;
        queued.chunk = chunk;
        queued.state = 0;
        arrput(world.buildChunksQueue, queued);
        chunk->isBuilding = true;
    }
}

void World_AddChunk(Vector3 position) {

    long int p = (long)((int)(position.x)&4095)<<20 | (long)((int)(position.z)&4095)<<8 | ((int)(position.y)&255);
    int index = hmgeti(world.chunks, p);
    if(index == -1) {
        //Add chunk to list
        Chunk *newChunk = MemAlloc(sizeof(Chunk));
        hmput(world.chunks, p, newChunk);

        Chunk_Init(newChunk, position);

        if(!Network_connectedToServer) {
            World_QueueChunk(newChunk);
            Chunk_RefreshBorderingChunks(newChunk, true);
        }
    }
}

Chunk* World_GetChunkAt(Vector3 position) {
    long int p = (long)((int)(position.x)&4095)<<20 | (long)((int)(position.z)&4095)<<8 | ((int)(position.y)&255);
    int index = hmgeti(world.chunks, p);
    if(index >= 0) {
        return world.chunks[index].value;
    }
    
    return NULL;
}

void World_RemoveChunk(Chunk *curChunk) {
    long int p = (long)((int)(curChunk->position.x)&4095)<<20 | (long)((int)(curChunk->position.z)&4095)<<8 | ((int)(curChunk->position.y)&255);

    int index = hmgeti(world.chunks, p);
    if(index >= 0) {
        Chunk_Unload(curChunk);
        hmdel(world.chunks, p);
    }
    
}

void World_UpdateChunks(void) {

        int meshUpdatesCount = 4;

        for (int i = arrlen(world.buildChunksQueue) - 1; i >= 0; i--) {
            Chunk *chunk = world.buildChunksQueue[i].chunk;
            if(chunk->isLightGenerated == true) {
                if(Chunk_AreNeighbourGenerated(chunk) == true) {
                    Chunk_BuildMesh(chunk);
                    chunk->isBuilding = false;
                    arrdel(world.buildChunksQueue, i);
                    if(--meshUpdatesCount == 0) return;

                    continue;
                }
            }
        }
}

void World_LoadChunks(bool loadEdges) {

    if(!world.loadChunks) return;

    Vector3 pos = (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};
    
    //Create array of chunks to be loaded
    int loadingHeight = fmin(world.drawDistance, 4);
    int sortedLength = 0;
    struct { Vector3 chunkPos; float dist; } sortedChunks[(world.drawDistance + 1) * 2 * (world.drawDistance + 1) * 2 * (loadingHeight + 1) * 2];
    for(int x = -world.drawDistance ; x <= world.drawDistance; x++) {
        for(int z = -world.drawDistance ; z <= world.drawDistance; z++) {
            for(int y = -loadingHeight ; y <= loadingHeight; y++) {
                Vector3 chunkPos = (Vector3) {pos.x + x, pos.y + y, pos.z + z};
                if(!loadEdges) {
                    sortedChunks[sortedLength].chunkPos = chunkPos;
                    sortedChunks[sortedLength].dist = Vector3Distance(chunkPos, pos);
                    sortedLength++;
                } else if(Vector3Distance(chunkPos, pos) >= world.drawDistance - 1) {
                    sortedChunks[sortedLength].chunkPos = chunkPos;
                    sortedChunks[sortedLength].dist = Vector3Distance(chunkPos, pos);
                    sortedLength++;
                }
            }
        }
    }

    //Sort array of chunks front to back
    for(int i = 1; i < sortedLength; i++) {
        int j = i;
        while(j > 0 && sortedChunks[j-1].dist > sortedChunks[j].dist) {

            struct { Vector3 chunkPos; float dist; } tempC;
            tempC.chunkPos = sortedChunks[j].chunkPos;
            tempC.dist = sortedChunks[j].dist;

            sortedChunks[j] = sortedChunks[j - 1];
            sortedChunks[j - 1].chunkPos = tempC.chunkPos;
            sortedChunks[j - 1].dist = tempC.dist;
            j = j - 1;
        }
    }

    //Create the chunks
    for(int i = 0; i < sortedLength; i++) {
        World_AddChunk(sortedChunks[i].chunkPos);
    }
    
    //destroy far chunks
    for (int i = hmlen(world.chunks) - 1; i >= 0 ; i--) {
        Chunk *chunk = world.chunks[i].value;

        if(chunk->isBuilt == true && chunk->isBuilding == false) {
            if(Vector3Distance(chunk->position, pos) >= world.drawDistance + 2) {
                if(Chunk_AreNeighbourBuilding(chunk) == false) {
                   World_RemoveChunk(chunk);
                }
            }
        }

    }


}

void World_Reload(void) {
    World_Unload();
    world.loadChunks = true;
    World_LoadChunks(false);
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
