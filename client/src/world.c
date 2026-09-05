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
#include <string.h>
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
#include "chunklightning.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "entitymodel.h"
#include "localserver.h"
#include "particle.h"
#include "cloud.h"

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
    Particle_Clear();
    Cloud_Init();
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
    float deltaTime = GetFrameTime();
    world.time += deltaTime;
    while (world.time >= WORLD_DAY_LENGTH_SECONDS) {
        world.time -= WORLD_DAY_LENGTH_SECONDS;
    }

    World_UpdateChunksWithBudget(4.0);
    Particle_Update(deltaTime);
    Cloud_Update(deltaTime);
    float interpolationAmount = 1.0f - expf(-20.0f * deltaTime);
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *entity = &world.entities[i];
        if (entity->type == 0) continue;

        entity->position = Vector3Lerp(entity->position, entity->targetPosition, interpolationAmount);
        float yawDifference = atan2f(sinf(entity->targetRotation.y - entity->rotation.y),
                                     cosf(entity->targetRotation.y - entity->rotation.y));
        entity->rotation.y += yawDifference * interpolationAmount;
        for (int partIndex = 0; partIndex < entity->model.partCount; partIndex++) {
            EntityModelPart *part = &entity->model.parts[partIndex];
            if (part->type == PART_TYPE_HEAD) {
                part->rotation.x = Lerp(part->rotation.x, entity->targetHeadPitch, interpolationAmount);
            }
        }

        EntityAnimation_Update(&entity->animation, entity->position, deltaTime);
    }
    
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
        int index = 0;
        float closestDistanceSquared = Vector3DistanceSqr(array[0]->position, pos);
        for (int i = 1; i < arrLength; i++) {
            float distanceSquared = Vector3DistanceSqr(array[i]->position, pos);
            if (distanceSquared < closestDistanceSquared) {
                closestDistanceSquared = distanceSquared;
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
    Particle_Clear();
    Player_ClearEntityModel();

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
    Cloud_Shutdown();
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

static Vector4 World_TransformVector4(Vector4 vector, Matrix matrix) {
    return (Vector4) {
        matrix.m0 * vector.x + matrix.m4 * vector.y + matrix.m8  * vector.z + matrix.m12 * vector.w,
        matrix.m1 * vector.x + matrix.m5 * vector.y + matrix.m9  * vector.z + matrix.m13 * vector.w,
        matrix.m2 * vector.x + matrix.m6 * vector.y + matrix.m10 * vector.z + matrix.m14 * vector.w,
        matrix.m3 * vector.x + matrix.m7 * vector.y + matrix.m11 * vector.z + matrix.m15 * vector.w
    };
}

static bool World_IsChunkInFrustum(const Chunk *chunk, Matrix view, Matrix projection) {
    Vector3 min = chunk->blockPosition;
    Vector3 max = Vector3Add(min, CHUNK_SIZE_VEC3);
    unsigned char outsideAllCorners = 0x3F;

    for (int i = 0; i < 8; i++) {
        Vector4 corner = {
            (i & 1) ? max.x : min.x,
            (i & 2) ? max.y : min.y,
            (i & 4) ? max.z : min.z,
            1.0f
        };
        Vector4 clip = World_TransformVector4(World_TransformVector4(corner, view), projection);
        unsigned char outside = 0;

        if (clip.x < -clip.w) outside |= 1u << 0;
        if (clip.x >  clip.w) outside |= 1u << 1;
        if (clip.y < -clip.w) outside |= 1u << 2;
        if (clip.y >  clip.w) outside |= 1u << 3;
        if (clip.z < -clip.w) outside |= 1u << 4;
        if (clip.z >  clip.w) outside |= 1u << 5;

        outsideAllCorners &= outside;
    }

    return outsideAllCorners == 0;
}

void World_Draw(Vector3 camPosition) {

    ChunkMesh_PrepareDrawing(world.material);

    int amountChunks = hmlen(world.chunks);
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } sortedChunks[amountChunks];

    int sortedLength = 0;
    for (int i=0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;

        if (chunk->onlyAir) continue;
        if (!World_IsChunkInFrustum(chunk, view, projection)) continue;

        if (chunk->hasTransparency) {
            Vector3 centerChunk = Vector3Add(chunk->blockPosition, chunkLocalCenter);
            float distFromCam = Vector3Distance(centerChunk, camPosition);

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

    Cloud_Draw(camPosition, World_GetSunlightStrength());

    Particle_Draw(player.camera, world.material.maps[MATERIAL_MAP_DIFFUSE].texture);

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

float World_GetBrightness(Vector3 position) {
    Vector3 chunkPosition = {
        floorf(position.x / CHUNK_SIZE_X),
        floorf(position.y / CHUNK_SIZE_Y),
        floorf(position.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = World_GetChunkAt(chunkPosition);
    if (!chunk || !chunk->isLightGenerated) return 1.0f;

    Vector3 localPosition = {
        floorf(position.x) - chunk->blockPosition.x,
        floorf(position.y) - chunk->blockPosition.y,
        floorf(position.z) - chunk->blockPosition.z
    };
    float blockLight = Chunk_GetLight(chunk, localPosition, false) / 15.0f;
    float sunlight = Chunk_GetLight(chunk, localPosition, true) / 15.0f;
    sunlight *= World_GetSunlightStrength();
    return Clamp(fmaxf(blockLight, sunlight), 0.1f, 1.0f);
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Entities-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    Entity *entity = &world.entities[id];
    if (entity->type == 0) return;
    if (Vector3DistanceSqr(entity->position, position) > 64.0f) {
        entity->position = position;
        entity->rotation = (Vector3) {0, rotation.y, 0};
        entity->targetPosition = position;
        entity->targetRotation = entity->rotation;
        entity->targetHeadPitch = rotation.x;
        entity->animation.lastPosition = position;
        for (int i = 0; i < entity->model.partCount; i++) {
            if (entity->model.parts[i].type == PART_TYPE_HEAD) {
                entity->model.parts[i].rotation.x = rotation.x;
            }
        }
        return;
    }

    entity->targetPosition = position;
    entity->targetRotation = (Vector3) {0, rotation.y, 0};
    entity->targetHeadPitch = rotation.x;
}

void World_AddEntity(int id, int type, int modelId, Vector3 position, Vector3 rotation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    if (modelId < 0 || modelId >= 256 || entityModels[modelId].boxCount == 0) return;

    if (world.entities[id].type != 0) Entity_Destroy(&world.entities[id]);
    world.entities[id].type = type;
    world.entities[id].modelId = (unsigned char)modelId;
    world.entities[id].position = position;
    world.entities[id].rotation = rotation;
    world.entities[id].targetPosition = position;
    world.entities[id].targetRotation = rotation;
    world.entities[id].targetHeadPitch = rotation.x;
    EntityAnimation_Init(&world.entities[id].animation, position);
    
    EntityModel_Create(&world.entities[id].model, entityModels[modelId]);
}

void World_RemoveEntity(int id) {
    Entity_Destroy(&world.entities[id]);
}

void World_PlayEntityAnimation(int id, EntityAnimationType animation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    if (world.entities[id].type == 0) return;
    EntityAnimation_Start(&world.entities[id].animation, animation);
}

void World_InvalidateBlockDefinitions(bool relight) {
    for (int i = 0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;
        if (relight) {
            memset(chunk->lightData, 0, sizeof(chunk->lightData));
            memset(chunk->sunlightData, 0, sizeof(chunk->sunlightData));
            chunk->isLightGenerated = false;
            chunk->incompleteLightFaces = 0;
            chunk->incompleteSunlightFaces = 0;
            chunk->isLightDirty = true;
        }
        World_QueueChunk(chunk, false);
    }
}
