#include "digging.h"
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
#include "rotation.h"
#include "player.h"
#include "chunkmeshgeneration.h"
#include "chunkmeshworker.h"
#include "chunklightning.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "entitymodel.h"
#include "nametag.h"
#include "localserver.h"
#include "particle.h"
#include "cloud.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

World world;

static Vector3 meshQueueCenter;
static bool meshQueueNeedsRebuild = true;
static Chunk **retiredChunks;

static void World_FreeRetiredChunks(double deadline) {
    while (arrlen(retiredChunks) && GetTime() < deadline) {
        Chunk *chunk = arrpop(retiredChunks);
        // These came from a server-owned world. They are already detached,
        // so no neighbor updates or client-side saves are needed.
        Chunk_Unload(chunk);
        Chunk_Destroy(chunk);
    }
    if (!arrlen(retiredChunks)) arrfree(retiredChunks);
}

bool World_CleanupChunks(void) {
    double deadline = GetTime() + 0.016;
    while (arrlen(retiredChunks) && GetTime() < deadline) {
        Chunk *chunks[16];
        ChunkMesh *meshes[32];
        int count = 0;
        while (count < 16 && arrlen(retiredChunks)) {
            Chunk *chunk = arrpop(retiredChunks);
            chunks[count] = chunk;
            meshes[count * 2] = &chunk->mesh;
            meshes[count * 2 + 1] = &chunk->meshTransparent;
            count++;
        }
        ChunkMesh_UnloadBatch(meshes, count * 2);
        for (int i = 0; i < count; i++) Chunk_Destroy(chunks[i]);
    }
    if (!arrlen(retiredChunks)) arrfree(retiredChunks);
    return arrlen(retiredChunks) == 0;
}

int World_RemainingCleanupChunks(void) {
    return arrlen(retiredChunks);
}

static bool MeshQueueCloser(Chunk *a, Chunk *b) {
    return Vector3DistanceSqr(a->position, meshQueueCenter) <
           Vector3DistanceSqr(b->position, meshQueueCenter);
}

static void MeshQueueDown(int index) {
    int count = arrlen(world.generateChunksQueue);
    Chunk *chunk = world.generateChunksQueue[index];
    while (index * 2 + 1 < count) {
        int child = index * 2 + 1;
        if (child + 1 < count && MeshQueueCloser(world.generateChunksQueue[child + 1],
                                                world.generateChunksQueue[child])) child++;
        if (!MeshQueueCloser(world.generateChunksQueue[child], chunk)) break;
        world.generateChunksQueue[index] = world.generateChunksQueue[child];
        index = child;
    }
    world.generateChunksQueue[index] = chunk;
}

static void MeshQueuePrepare(void) {
    Vector3 center = Player_GetChunkPosition();
    if (!Vector3Equals(center, meshQueueCenter)) meshQueueNeedsRebuild = true;
    if (!meshQueueNeedsRebuild) return;
    meshQueueCenter = center;
    for (int i = (int)arrlen(world.generateChunksQueue) / 2 - 1; i >= 0; i--) MeshQueueDown(i);
    meshQueueNeedsRebuild = false;
}

void World_Init(void) {
    world.material = LoadMaterialDefault();
    world.loadChunks = false;
    world.drawDistance = 8;
    world.time = 0;

    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0; //type 0 = none

    ChunkMeshWorker_Init();
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
    double cleanupDeadline = fmin(endTime, GetTime() + 0.001);
    World_FreeRetiredChunks(cleanupDeadline);

    // Refill before uploads, then alternate completions and submissions so
    // workers do not sit idle while the main thread spends its upload budget.
    while (world.loadChunks && arrlen(world.generateChunksQueue) > 0 &&
           GetTime() < endTime) {
        if (ChunkMeshWorker_HasSpace()) {
            int count = arrlen(world.generateChunksQueue);
            World_ReadChunksQueues();
            if (arrlen(world.generateChunksQueue) == count) break;
        } else if (!ChunkMeshWorker_ReceiveOne()) break;
    }
    if (GetTime() < endTime) ChunkMeshWorker_Receive(endTime);
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
        entity->rotation.y = Rotation_Interpolate(entity->rotation.y, entity->targetRotation.y, interpolationAmount);
        entity->rotation.z = Rotation_Interpolate(entity->rotation.z, entity->targetRotation.z, interpolationAmount);
        if (entity->type != 1)
            entity->rotation.x = Rotation_Interpolate(entity->rotation.x, entity->targetRotation.x, interpolationAmount);
        for (int partIndex = 0; partIndex < entity->model.partCount; partIndex++) {
            EntityModelPart *part = &entity->model.parts[partIndex];
            if (entity->type == 1 && part->type == PART_TYPE_HEAD) {
                part->rotation.x = Rotation_Interpolate(part->rotation.x, entity->targetHeadPitch, interpolationAmount);
            }
        }

        EntityAnimation_Update(&entity->animation, entity->position, deltaTime);
    }
    
}

void World_ReadChunksQueues(void) {
    if (!world.loadChunks) return;
    if (!arrlen(world.generateChunksQueue)) return;
    MeshQueuePrepare();
    Chunk *chunk = world.generateChunksQueue[0];
    if (!ChunkMeshWorker_Submit(chunk)) return;
    arrdelswap(world.generateChunksQueue, 0);
    if (arrlen(world.generateChunksQueue)) MeshQueueDown(0);
    chunk->isGenerating = false;
}

void World_QueueChunk(Chunk *chunk, bool immediate) {
    // A queued chunk has no snapshot yet, so it already includes later edits.
    if (chunk->isGenerating) return;
    chunk->meshRevision++;
    if (!world.loadChunks || !chunk->isLightGenerated || chunk->meshPending) return;

    if (chunk->isGenerating == false) {
        MeshQueuePrepare();
        arrput(world.generateChunksQueue, chunk);
        int index = arrlen(world.generateChunksQueue) - 1;
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (!MeshQueueCloser(chunk, world.generateChunksQueue[parent])) break;
            world.generateChunksQueue[index] = world.generateChunksQueue[parent];
            index = parent;
        }
        world.generateChunksQueue[index] = chunk;
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
                meshQueueNeedsRebuild = true;
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

void World_ClearChunks(void) {
    ChunkMeshWorker_CancelQueued();
    bool loading = world.loadChunks;
    world.loadChunks = false;
    arrfree(world.generateChunksQueue);
    world.generateChunksQueue = NULL;
    meshQueueNeedsRebuild = true;

    if (networkConnectedToServer) {
        int oldCount = hmlen(world.chunks);
        int retiredCount = arrlen(retiredChunks);
        if (oldCount) {
            (void)arrsetlen(retiredChunks, retiredCount + oldCount);
            for (int i = 0; i < oldCount; i++)
                retiredChunks[retiredCount + i] = world.chunks[i].value;
        }
        hmfree(world.chunks);
        world.chunks = NULL;
        world.loadChunks = loading;
        return;
    }

    for (int i = hmlen(world.chunks) - 1; i >= 0; i--) {
        World_RemoveChunk(world.chunks[i].value);
    }

    world.loadChunks = loading;
}

void World_Clear(void) {
    world.loadChunks = false;
    Particle_Clear();
    Player_ClearEntityModel();

    World_ClearChunks();
    // Shutdown/disconnect must release everything before the graphics context.
    World_FreeRetiredChunks(INFINITY);

    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        World_RemoveEntity(i);
    }

    hmfree(world.chunks);
    world.chunks = NULL;

}

void World_Shutdown(void) {
    Cloud_Shutdown();
    World_Clear();
    world.material.maps[MATERIAL_MAP_DIFFUSE].texture.id = 0; // Texture owner releases it.
    UnloadMaterial(world.material);
    MemFree(world.entities);
    world.entities = NULL;
    ChunkMeshWorker_Shutdown();
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

    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    ChunkMesh_PrepareDrawing(world.material);

    int amountChunks = hmlen(world.chunks);
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } sortedChunks[amountChunks > 0 ? amountChunks : 1];

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
        }
        {
            Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                0, 1, 0, chunk->blockPosition.y,
                0, 0, 1, chunk->blockPosition.z,
                0, 0, 0, 1 };
        
            ChunkMesh_Draw(&chunk->mesh, world.material, matrix);
        }
    }
    
    ChunkMesh_FinishDrawing();

    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        if (world.entities[i].type == 0) continue;
        Entity_Draw(&world.entities[i]);
    }
    if (player.cameraMode != PLAYER_CAMERA_FIRST_PERSON) Player_Draw();
    Digging_Draw();
    Cloud_Draw(camPosition, World_GetSunlightStrength());
    Particle_Draw(player.camera, world.material.maps[MATERIAL_MAP_DIFFUSE].texture);
    rlDrawRenderBatchActive();

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

    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    for (int i = 0; i < sortedLength; i++) {
        Chunk *chunk = sortedChunks[i].chunk;

        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };
        
        ChunkMesh_Draw(&chunk->meshTransparent, world.material, matrix);
    }

    ChunkMesh_FinishDrawing();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    Nametags_Draw(player.camera);
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

    World_QueueChunk(chunk, immediate);
    bool boundary[6] = {blockPosInChunk.x == 0, blockPosInChunk.x == 15,
                        blockPosInChunk.y == 15, blockPosInChunk.y == 0,
                        blockPosInChunk.z == 15, blockPosInChunk.z == 0};
    for (int face = 0; face < 6; face++) {
        if (boundary[face] && chunk->neighbours[face])
            World_QueueChunk(chunk->neighbours[face], immediate);
    }

}

float World_GetSunlightStrength(void) {
    return WorldTime_Sunlight(world.time);
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
        entity->rotation = (Vector3) {entity->type == 1 ? 0 : rotation.x, rotation.y, rotation.z};
        entity->targetPosition = position;
        entity->targetRotation = entity->rotation;
        entity->targetHeadPitch = rotation.x;
        entity->animation.lastPosition = position;
        for (int i = 0; i < entity->model.partCount; i++) {
            if (entity->type == 1 && entity->model.parts[i].type == PART_TYPE_HEAD) {
                entity->model.parts[i].rotation.x = rotation.x;
            }
        }
        return;
    }

    entity->targetPosition = position;
    entity->targetRotation = (Vector3) {entity->type == 1 ? 0 : rotation.x, rotation.y, rotation.z};
    entity->targetHeadPitch = rotation.x;
}

void World_AddEntity(int id, int type, int modelId, Vector3 position, Vector3 rotation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    if (modelId < 0 || modelId >= 256) return;

    if (world.entities[id].type != 0) Entity_Destroy(&world.entities[id]);
    world.entities[id] = (Entity){0};
    world.entities[id].type = type;
    world.entities[id].modelId = (unsigned char)modelId;
    world.entities[id].position = position;
    world.entities[id].rotation = rotation;
    world.entities[id].targetPosition = position;
    world.entities[id].targetRotation = rotation;
    world.entities[id].targetHeadPitch = rotation.x;
    EntityAnimation_Init(&world.entities[id].animation, position);
    
    if (type != ENTITY_TYPE_DROPPED_ITEM)
        EntityModel_Create(&world.entities[id].model, *EntityModel_GetDefinition(modelId));
}

void World_RemoveEntity(int id) {
    if (!world.entities || id < 0 || id >= WORLD_MAX_ENTITIES) return;
    Entity_Destroy(&world.entities[id]);
}

void World_PlayEntityAnimation(int id, EntityAnimationType animation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    if (world.entities[id].type == 0) return;
    EntityAnimation_Start(&world.entities[id].animation, animation);
}

void World_InvalidateBlockDefinitions(bool relight) {
    if (relight) {
        for (int i = 0; i < hmlen(world.chunks); i++) {
            Chunk *chunk = world.chunks[i].value;
            if (!chunk->isLightGenerated) continue;
            memset(chunk->lightData, 0, sizeof(chunk->lightData));
            memset(chunk->sunlightData, 0, sizeof(chunk->sunlightData));
            chunk->isLightDirty = true;
        }
        for (int i = 0; i < hmlen(world.chunks); i++) {
            Chunk *chunk = world.chunks[i].value;
            if (!chunk->isLightGenerated) continue;
            Chunk_DoSunlight(chunk);
            Chunk_DoLightSources(chunk);
        }
    }
    for (int i = 0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;
        World_QueueChunk(chunk, false);
    }
}
