#include <stdio.h>
#include <stddef.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "world.h"
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "chunk.h"
#include "chunkmesh.h"
#include "screens.h"
#include "block.h"
#include "networkhandler.h"

World world;

#define WORLD_MAX_ENTITIES 1028

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Loading------------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_Init(void) {
    world.size = (Vector3) {8, 8, 8};
    world.mat = LoadMaterialDefault();
    world.loaded = INT_MAX;
    world.drawDistance = 6;

    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0;

    pthread_t clientThread_id;
    pthread_create(&clientThread_id, NULL, World_ReadChunksQueue, NULL);
}

void World_LoadSingleplayer(void) {
    
    if(World_LoadFile("world.dat")) return;

    world.size = (Vector3) {8, 8, 8};
    unsigned char *worldData = MemAlloc((world.size.x * world.size.y * world.size.z) * CHUNK_SIZE);

    for(int i = 0; i < World_GetFlatSize() * CHUNK_SIZE; i++) {
        Vector3 npos = World_BlockIndexToPos(i);

        if(npos.y < 3) worldData[i] = 3;
        if(npos.y < 2) worldData[i] = 2;
        if(npos.y < 1) worldData[i] = 1;
    }
    
    World_Load(worldData);
}

unsigned char* World_Decompress(unsigned char *data, int currentLength, int *newLength) {
    
    unsigned char *decompressed = (unsigned char*)MemAlloc(World_GetFlatSize() * CHUNK_SIZE);
    for(int i = 0; i < currentLength; i+=2) {
        for(int j = 0; j < data[i + 1]; j++) {
            decompressed[*newLength] = data[i];
            *newLength += 1;
        } 
    }
    
    return decompressed;
}

void World_StartLoading(void) {
    world.loaded = 0;
    Screen_Switch(SCREEN_LOADING);
}

void World_LoadChunks(void) {
    if(world.loaded == INT_MAX) return;
    
    if(world.loaded == World_GetFlatSize()) {
        Screen_Switch(SCREEN_GAME);
        world.loaded++;
    }

    for(int i = 0; i < World_GetFlatSize(); i++) {
        if(world.chunksToRebuild[i] == 2) {
            world.chunksToRebuild[i] = 0;
            ChunkMesh_Upload(world.chunks[i].mesh);
            ChunkMesh_Upload(world.chunks[i].meshTransparent);

            if(world.loaded < World_GetFlatSize()) {
                world.loaded++;
            }
        }
    }
}

void World_SaveFile(const char* fileName) {
    if(Network_connectedToServer) return;
    unsigned char *saveFile = MemAlloc(World_GetFlatSize() * CHUNK_SIZE + 3);
    saveFile[0] = (unsigned char)world.size.x;
    saveFile[1] = (unsigned char)world.size.y;
    saveFile[2] = (unsigned char)world.size.z;
    memcpy(&saveFile[3], world.data, World_GetFlatSize() * CHUNK_SIZE);
    SaveFileData(fileName, saveFile, World_GetFlatSize() * CHUNK_SIZE);
    MemFree(saveFile);
}

bool World_LoadFile(const char* fileName) {
    if(FileExists(fileName)) {

        World_Unload();

        unsigned int length = 0;
        unsigned char *saveFile = LoadFileData(fileName, &length);
        world.size.x = saveFile[0];
        world.size.y = saveFile[1];
        world.size.z = saveFile[2];

        unsigned char *wData = MemAlloc(World_GetFlatSize() * CHUNK_SIZE);
        memcpy(wData, &saveFile[3], World_GetFlatSize() * CHUNK_SIZE);

        World_Load(wData);

        MemFree(saveFile);
        return true;
    }
    return false;
}

void World_Unload(void) {

    if(world.loaded == INT_MAX) return;

    for(int i = 0; i < World_GetFlatSize(); i++) {
        Chunk_Unload(&world.chunks[i]);
    }

    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0;
    MemFree(world.data);
    MemFree(world.lightData);
    MemFree(world.chunks);
    MemFree(world.chunksToRebuild);
    world.loaded = INT_MAX;

}

void World_Load(unsigned char *worldData) {
    world.data = worldData;
    
    world.chunks = MemAlloc(World_GetFlatSize() * sizeof(Chunk));
    world.lightData = MemAlloc(World_GetFlatSize() * CHUNK_SIZE);

    world.chunksToRebuild = MemAlloc(World_GetFlatSize());
    memset(world.chunksToRebuild, 0, World_GetFlatSize());

    World_BuildLightMap();

    //Create Chunks
    for(int i = 0; i < World_GetFlatSize(); i++) {
        Vector3 pos = World_ChunkIndexToPos(i);
        Chunk_Init(&world.chunks[i], pos);
        World_QueueChunk(&world.chunks[i], false);
    }

    player.position = (Vector3) { (world.size.x * CHUNK_SIZE_X) / 2, 128.0f, (world.size.z * CHUNK_SIZE_Z) / 2 };
    World_StartLoading();
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Drawing------------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.mat, MATERIAL_MAP_DIFFUSE, texture);
}

void World_ApplyShader(Shader shader) {
    world.mat.shader = shader;
}

void World_Draw(Vector3 camPosition) {
    if(world.loaded == INT_MAX) return;

    //Create the sorted chunk list
    Chunk* sortedChunks = MemAlloc(World_GetFlatSize() * sizeof(Chunk));
    int sortedLength = 0;
    for(int i = 0; i < World_GetFlatSize(); i++) {
        Chunk* chunk = &world.chunks[i];

        //Don't draw chunks further than the drawing distance
        if(Vector3Distance(chunk->blockPosition, camPosition) > world.drawDistance * 16) continue;

        //Don't draw chunks behind the player
        Vector3 centerChunk = Vector3Add(chunk->blockPosition, (Vector3){8, 8, 8});
        Vector3 toChunkVec = Vector3Normalize(Vector3Subtract(centerChunk, camPosition));
        Vector3 dirVec = Player_GetForwardVector();
        if(Vector3Distance(centerChunk, camPosition) > 16 && Vector3Distance(toChunkVec, dirVec) > 1.5f) continue;

        sortedChunks[sortedLength] = world.chunks[i];
        sortedLength++;
    }
    
    //Sort chunks back to front
    for(int i = 1; i < sortedLength; i++) {
        int j = i;
        while(j > 0 && Vector3Distance(Vector3Add(sortedChunks[j-1].blockPosition, (Vector3){8, 8, 8}), camPosition) <= Vector3Distance(Vector3Add(sortedChunks[j].blockPosition, (Vector3){8, 8, 8}), camPosition)) {
            Chunk tempC = sortedChunks[j];
            sortedChunks[j] = sortedChunks[j - 1];
            sortedChunks[j - 1] = tempC;
            j = j - 1;
        }
    }
    
    //Draw sorted chunks
    for(int i = 0; i < sortedLength; i++) {
        if(i >= world.loaded) break;
        Chunk* chunk = &sortedChunks[i];
        
        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };
        
        ChunkMesh_Draw(*chunk->mesh, world.mat, matrix);
        rlDisableBackfaceCulling();
        ChunkMesh_Draw(*chunk->meshTransparent, world.mat, matrix);
        rlEnableBackfaceCulling();
    }

    MemFree(sortedChunks);

    //Draw entities
    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        if(world.entities[i].type == 0) continue;
        Entity_Draw(&world.entities[i]);
    }

}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Entities-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_TeleportEntity(int ID, Vector3 position) {
    world.entities[ID].position = position;
}

void World_AddEntity(int ID, int type, Vector3 position) {
    world.entities[ID].type = type;
    world.entities[ID].position = position;
}

void World_RemoveEntity(int ID) {
    world.entities[ID].type = 0;
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Lightning----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

int World_GetLightLevel(Vector3 blockPos) {
    
    if(!World_IsValidBlockPos(blockPos)) return 255;
    int index = World_BlockPosToIndex(blockPos);
    int lightLvl = 255 - world.lightData[index];

    return lightLvl;
}

void World_AddLight(Vector3 pos, int intensity, bool isSunLight) {
    if(!World_IsValidBlockPos(pos)) return;
    int index = World_BlockPosToIndex(pos);
    world.lightData[index] = intensity;

    Vector3 lightDirections[6] = {
        {0, 1, 0},
        {0, -1, 0},
        {1, 0, 0},
        {-1, 0, 0},
        {0, 0, 1},
        {0, 0, -1}
    };

    for(int d = 0; d < 6; d++) {

        Vector3 dirs[5];
        int cnt = 0;
        for(int d2 = 0; d2 < 6; d2++) {
            if(d2 == d) continue;
            dirs[cnt++] = Vector3Multiply(lightDirections[d2], (Vector3) {-1, -1, -1});
        }

        int subVal = 32;
        if(lightDirections[d].y == 1 && isSunLight) subVal = 0;

        World_PropagateLight(Vector3Add(pos, lightDirections[d]), dirs, intensity - subVal, isSunLight);
    }
}

void World_PropagateLight(Vector3 pos, Vector3 *directions, int intensity, bool isSunLight) {
    if(!World_IsValidBlockPos(pos) || intensity <= 0) return;
    int index = World_BlockPosToIndex(pos);
    if(world.lightData[index] >= intensity) return;
    if(Block_definition[world.data[index]].renderType == BlockRenderType_Opaque) return;
    
    world.lightData[index] = intensity;

    for(int d = 0; d < 5; d++) {
        int subVal = 32;
        if(directions[d].y == 1 && isSunLight) subVal = 0;
        World_PropagateLight(Vector3Add(pos, directions[d]), directions, intensity - subVal, isSunLight);
    }
}

void World_UpdateLightMap(Vector3 pos) {

    for(int x = pos.x - 15; x <= pos.x + 15; x++) {
        for(int y = pos.y - 15; y <= pos.y + 15; y++) {
            for(int z = pos.z - 15; z < pos.z + 15; z++) {
                Vector3 newPos = (Vector3) {x, y, z};
                if(!World_IsValidBlockPos(newPos)) continue;
                int index = World_BlockPosToIndex(newPos);
                world.lightData[index] = 0;
            }
        }
    }

    for(int x = pos.x - 23; x <= pos.x + 23; x++) {
        for(int z = pos.z - 23; z < pos.z + 23; z++) {

            for(int y = world.size.y * CHUNK_SIZE_Y - 1; y >= 0; y--) {
                Vector3 dPos = (Vector3) {x, y, z};

                Vector3 nPos = Vector3Add(dPos, (Vector3) {0, -1, 0});
                if(!World_IsValidBlockPos(nPos)) continue;
                int index = World_BlockPosToIndex(nPos);
                int blockID = world.data[index];

                if(Block_definition[blockID].renderType != BlockRenderType_Transparent) {
                    World_AddLight(dPos, 255, true);
                    break;
                } else if(Block_definition[blockID].modelType != BlockModelType_Gas) {
                    World_AddLight(dPos, 255, true);
                }
                
            }

            for(int y = pos.y - 23; y <= pos.y + 23; y++) {
                Vector3 newPos = (Vector3) {x, y, z};
                if(!World_IsValidBlockPos(newPos)) continue;
                int index = World_BlockPosToIndex(newPos);
                if(Block_definition[world.data[index]].lightType != BlockLightType_Emit) continue;
                world.lightData[index] = 255;
                World_AddLight(newPos, 255, false);
            }
        }
    }
}

void World_BuildLightMap(void) {

    memset(world.lightData, 0, (world.size.x * world.size.y * world.size.z) * CHUNK_SIZE);
    int one_up = world.size.x * world.size.z * CHUNK_SIZE_XZ;
    for(int i = 0; i < World_GetFlatSize() * CHUNK_SIZE; i++) {
        
        if(i >= ( World_GetFlatSize() * CHUNK_SIZE ) - one_up) {
            for(int j = i; j >= one_up; j -= one_up) {

                int index = j - one_up;
                int blockID = world.data[index];
                if(Block_definition[blockID].renderType != BlockRenderType_Transparent) {
                    Vector3 dPos = World_BlockIndexToPos(j);
                    World_AddLight(dPos, 255, true);
                    break;
                } else if(Block_definition[blockID].modelType != BlockModelType_Gas) {
                    Vector3 dPos = World_BlockIndexToPos(j);
                    World_AddLight(dPos, 255, true);
                }

            }
        }

        if(Block_definition[world.data[i]].lightType != BlockLightType_Emit) continue;
        Vector3 pos = World_BlockIndexToPos(i);
        world.lightData[i] = 255;
        World_AddLight(pos, 255, false);
    }

}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------Blocks & Chunks----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

int World_GetBlock(Vector3 blockPos) {
    if(!World_IsValidBlockPos(blockPos)) return 0;
    int index = World_BlockPosToIndex(blockPos);

    return world.data[index];
}

void World_SetBlock(Vector3 blockPos, int blockID) { 
    if(!World_IsValidBlockPos(blockPos)) return;
    int index = World_BlockPosToIndex(blockPos);
    
    if(world.data[index] == blockID) return;

    world.data[index] = blockID;
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { (int)blockPos.x / CHUNK_SIZE_X, (int)blockPos.y / CHUNK_SIZE_Y, (int)blockPos.z / CHUNK_SIZE_Z };
    Chunk* chunk = World_GetChunkAt(chunkPos);

    if(chunk == NULL) return;
    
    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                (int)blockPos.x - chunkPos.x * CHUNK_SIZE_X, 
                                (int)blockPos.y - chunkPos.y * CHUNK_SIZE_Y, 
                                (int)blockPos.z - chunkPos.z * CHUNK_SIZE_Z 
                               };
     
    //Refresh current chunk.
    World_QueueChunk(chunk, true);

    //Refresh mesh of neighbour chunks.
    Chunk_RefreshBorderingChunks(chunk, blockPosInChunk);
}

void *World_ReadChunksQueue(void *state) {
    while(true) {
        for(int i = 0; i < World_GetFlatSize(); i++) {
            if(world.loaded == INT_MAX) continue;
            if(world.chunksToRebuild[i] == 11) {
                Chunk *chunk = &world.chunks[i];
                World_UpdateLightMap(Vector3Add(chunk->blockPosition, (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2}));
            }

            if(world.chunksToRebuild[i] == 1 || world.chunksToRebuild[i] == 11) {
                Chunk *chunk = &world.chunks[i];
                Chunk_BuildMesh(chunk);
            }
        }
    }

    return NULL;
}

void World_QueueChunk(Chunk *chunk, bool updateLight) {
    if(world.chunksToRebuild[chunk->index] != 0) return;

    if(chunk->loaded == 1) {
        ChunkMesh_Unload(*chunk->mesh, false);
        ChunkMesh_Unload(*chunk->meshTransparent, false);
    }

    if(updateLight) {
        world.chunksToRebuild[chunk->index] = 11;
    } else {
        world.chunksToRebuild[chunk->index] = 1;
    }
    
}

Chunk* World_GetChunkAt(Vector3 pos) {
    int index = World_ChunkPosToIndex(pos);
    
    if(World_IsValidChunkPos(pos)) {
        return &world.chunks[index];
    }
    
    return NULL;
}

int World_IsValidChunkPos(Vector3 chunkPos) {
    return chunkPos.x >= 0 && chunkPos.x < world.size.x && chunkPos.y >= 0 && chunkPos.y < world.size.y && chunkPos.z >= 0 && chunkPos.z < world.size.z;
}

int World_IsValidBlockPos(Vector3 blockPos) {
    return blockPos.x >= 0 && blockPos.x < (world.size.x * CHUNK_SIZE_X) && blockPos.y >= 0 && blockPos.y < (world.size.y * CHUNK_SIZE_Y) && blockPos.z >= 0 && blockPos.z < (world.size.z * CHUNK_SIZE_Z);
}

int World_GetFlatSize(void) {
    return world.size.x * world.size.y * world.size.z;
}

Vector3 World_ChunkIndexToPos(int chunkIndex) {
    int x = (int)(chunkIndex % (int)world.size.x);
	int y = (int)(chunkIndex / (world.size.x * world.size.z));
	int z = (int)(chunkIndex / world.size.x) % (int)world.size.z;
    return (Vector3){x, y, z};
}

int World_ChunkPosToIndex(Vector3 chunkPos) {
    return (int)(((int)chunkPos.y * world.size.z + (int)chunkPos.z) * world.size.x + (int)chunkPos.x);
}

Vector3 World_BlockIndexToPos(int blockIndex) {
    int x = (int)(blockIndex % (int)(world.size.x * CHUNK_SIZE_X));
	int y = (int)(blockIndex / ((world.size.x * CHUNK_SIZE_X) * (world.size.z * CHUNK_SIZE_Z)));
	int z = (int)(blockIndex / (world.size.x * CHUNK_SIZE_X)) % (int)(world.size.z * CHUNK_SIZE_Z);
    return (Vector3){x, y, z};
}

int World_BlockPosToIndex(Vector3 blockPos) {
    return ((int)blockPos.y * (world.size.z * CHUNK_SIZE_Z) + (int)blockPos.z) * (world.size.x * CHUNK_SIZE_X) + (int)blockPos.x;
}