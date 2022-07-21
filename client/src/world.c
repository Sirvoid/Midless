/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <raylib.h>
#include <sys/stat.h>
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
    world.chunks = NULL;
    world.generateChunksQueue = NULL;
    world.buildChunksQueue = NULL;

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

pthread_mutex_t generateChunk_mutex;
pthread_mutex_t chunk_mutex;
void *World_ReadChunksQueues(void *state) {
    while(true) {

        pthread_mutex_lock(&chunk_mutex);
        
        QueuedChunk *queuedChunk = world.generateChunksQueue;
        
        if(world.loadChunks == true && queuedChunk != NULL) {
            Chunk_Generate(queuedChunk->chunk);

            pthread_mutex_lock(&generateChunk_mutex);
            world.generateChunksQueue = Chunk_PopFromQueue(queuedChunk);
            pthread_mutex_unlock(&generateChunk_mutex);
        }
        
        pthread_mutex_unlock(&chunk_mutex);
        
    }
    return NULL;
}

void World_QueueChunk(Chunk *chunk) {

    if(chunk->hasStartedGenerating == false) {
        pthread_mutex_lock(&generateChunk_mutex);
        world.generateChunksQueue = Chunk_AddToQueue(world.generateChunksQueue, chunk);  
        pthread_mutex_unlock(&generateChunk_mutex);
    }
    chunk->hasStartedGenerating = true;

    if(chunk->isBuilding == false) {
        world.buildChunksQueue = Chunk_AddToQueue(world.buildChunksQueue, chunk);
        chunk->isBuilding = true;
    }
}

void World_AddChunk(Vector3 position) {

    Chunk *curChunk = world.chunks;

    while(curChunk != NULL) {
        if(curChunk->position.x == position.x && curChunk->position.y == position.y && curChunk->position.z == position.z) { 
            return; //Already exists
        }

        if(curChunk->nextChunk != NULL) {
            curChunk = curChunk->nextChunk;
        } else {
            break;
        }
    }
    
    Chunk *newChunk = MemAlloc(sizeof(Chunk));
    newChunk->nextChunk = NULL;

    if(curChunk != NULL) {
        if(curChunk->nextChunk != NULL) {
            curChunk->nextChunk->previousChunk = newChunk;
            newChunk->nextChunk = curChunk->nextChunk;
        }
        curChunk->nextChunk = newChunk;
        newChunk->previousChunk = curChunk;
    } else {
        world.chunks = newChunk;
        newChunk->previousChunk = NULL;
    }

    Chunk_Init(newChunk, position);

    if(!Network_connectedToServer) {
        World_QueueChunk(newChunk);
        Chunk_RefreshBorderingChunks(newChunk, true);
    }
}

Chunk* World_GetChunkAt(Vector3 pos) {
    Chunk *chunk = world.chunks;
    while(chunk != NULL) {
        if(chunk->position.x == pos.x && chunk->position.y == pos.y && chunk->position.z == pos.z) {
            return chunk;
        }
        
        chunk = chunk->nextChunk;
    }
    
    return NULL;
}

void World_RemoveChunk(Chunk *curChunk) {

    Chunk* prevChunk = curChunk->previousChunk;
    Chunk *nextChunk = curChunk->nextChunk;

    if(curChunk == world.chunks) world.chunks = curChunk->nextChunk;
    if(prevChunk != NULL) prevChunk->nextChunk = nextChunk;
    if(nextChunk != NULL) nextChunk->previousChunk = prevChunk;
    
    Chunk_Unload(curChunk);
    MemFree(curChunk);
}

void World_UpdateChunks(void) {
        QueuedChunk* curQueued = world.buildChunksQueue;
        QueuedChunk* prevQueued = world.buildChunksQueue;

        int meshUpdatesCount = 4;

        while(curQueued != NULL) {
            QueuedChunk *nextQueued = curQueued->next;
            Chunk *chunk = curQueued->chunk;
            if(chunk->isLightGenerated == true) {
                if(Chunk_AreNeighbourGenerated(chunk) == true) {
                    Chunk_BuildMesh(chunk);
                    chunk->isBuilding = false;
                    world.buildChunksQueue = Chunk_RemoveFromQueue(world.buildChunksQueue, prevQueued, curQueued);

                    if(--meshUpdatesCount == 0) return;

                    curQueued = nextQueued;
                    continue;
                }
            }
            prevQueued = curQueued;
            curQueued = nextQueued;
        }
}

void World_LoadChunks(bool loadEdges) {

    if(!world.loadChunks) return;

    //Create chunks around
    Vector3 pos = (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};
    
    int loadingHeight = fmin(world.drawDistance, 4);
    for(int x = -world.drawDistance ; x <= world.drawDistance; x++) {
        for(int z = -world.drawDistance ; z <= world.drawDistance; z++) {
            for(int y = -loadingHeight ; y <= loadingHeight; y++) {
                Vector3 chunkPos = (Vector3) {pos.x + x, pos.y + y, pos.z + z};
                if(!loadEdges) {
                    World_AddChunk(chunkPos);
                } else if(Vector3Distance(chunkPos, pos) >= world.drawDistance) {
                    World_AddChunk(chunkPos);
                }
            }
        }
    }
    

    //destroy far chunks
    Chunk *chunk = world.chunks;
    while(chunk != NULL) {
        Chunk *nextChunk = chunk->nextChunk;

        if(chunk->isBuilt == true && chunk->isBuilding == false) {
            if(Vector3Distance(chunk->position, pos) >= world.drawDistance + 2) {
                if(Chunk_AreNeighbourBuilding(chunk) == false) {
                   World_RemoveChunk(chunk);
                }
            }
        }

        chunk = nextChunk;
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
    while(world.generateChunksQueue != NULL) {
        world.generateChunksQueue = Chunk_PopFromQueue(world.generateChunksQueue);
    }

    while(world.buildChunksQueue != NULL) {
        world.buildChunksQueue = Chunk_PopFromQueue(world.buildChunksQueue);
    }

    Chunk *curChunk = world.chunks;
    while(curChunk != NULL) {
        World_RemoveChunk(curChunk);
        curChunk = world.chunks;
    }
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

    int amountChunks = 0;
    float frustumAngle = DEG2RAD * player.camera.fovy + 0.3f;
    Vector3 dirVec = Player_GetForwardVector();

    Chunk *chunk = world.chunks;
    while(chunk != NULL) {
        amountChunks++;
        chunk = chunk->nextChunk;
    }
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } *sortedChunks = MemAlloc(amountChunks * (sizeof(Chunk*) + sizeof(float)));

    int sortedLength = 0;
    chunk = world.chunks;
    while(chunk != NULL) {
        Vector3 centerChunk = Vector3Add(chunk->blockPosition, chunkLocalCenter);
        float distFromCam = Vector3Distance(centerChunk, camPosition);

        //Don't draw chunks behind the player
        Vector3 toChunkVec = Vector3Normalize(Vector3Subtract(centerChunk, camPosition));
       
        if(distFromCam > CHUNK_SIZE_X && Vector3Distance(toChunkVec, dirVec) > frustumAngle) {
            chunk = chunk->nextChunk;
            continue;
        }

        sortedChunks[sortedLength].dist = distFromCam;
        sortedChunks[sortedLength].chunk = chunk;
        sortedLength++;
        chunk = chunk->nextChunk;
    }
    
    //Sort chunks back to front
    for(int i = 1; i < sortedLength; i++) {
        int j = i;
        while(j > 0 && sortedChunks[j-1].dist <= sortedChunks[j].dist) {

            static struct { Chunk *chunk; float dist; } tempC;
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
        
        ChunkMesh_Draw(chunk->mesh, world.mat, matrix);
        rlDisableBackfaceCulling();
        ChunkMesh_Draw(chunk->meshTransparent, world.mat, matrix);
        rlEnableBackfaceCulling();
    }

    ChunkMesh_FinishDrawing();

    MemFree(sortedChunks);

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
