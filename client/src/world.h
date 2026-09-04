/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_WORLD_H
#define MIDLESS_CLIENT_WORLD_H

#include "raylib.h"
#include "chunk.h"
#include "entity.h"
#include "worldtime.h"

#define WORLD_MAX_ENTITIES 1028

typedef struct World{
    Entity *entities;
    struct { long int key; Chunk* value; } *chunks;
    Chunk* *generateChunksQueue;
    Material material;
    int drawDistance;
    float time;
    bool loadChunks;
} World;

extern World world;

//Initialize the world.
void World_Init(void);
//Load multiplayer world.
void World_LoadMultiplayer(void);
//Load singleplayer world.
void World_LoadSingleplayer(void);
//Update World
void World_Update(void);
//Build Chunks mesh in queue
void World_UpdateChunks(void);
//Load & Unload Chunks around players.
void World_LoadChunks(void);
//Read Queue to generate chunks.
void World_ReadChunksQueues(void);
//Queue a chunk to build it.
void World_QueueChunk(Chunk *chunk, bool immediate);
//Get chunk at a chunk position.
Chunk* World_GetChunkAt(Vector3 position);
//Get closest chunk from position in array.
int World_GetClosestChunkIndex(Chunk* *array, Vector3 pos);
//Add a chunk.
void World_AddChunk(Vector3 position);
//Remove a chunk
void World_RemoveChunk(Chunk *currentChunk);
//Remove all world objects while keeping the world initialized.
void World_Clear(void);
//Shutdown the world system.
void World_Shutdown(void);
//Reload chunks.
void World_Reload(void);
//Draw the world.
void World_Draw(Vector3 camPosition);
//Apply terrain texture to the world.
void World_ApplyTexture(Texture2D texture);
//Apply a shader to the world.
void World_ApplyShader(Shader shader);
//Set block at a given position and reload affected meshes.
void World_SetBlock(Vector3 blockPos, int blockId, bool immediate);
//Get block id at a given position.
int World_GetBlock(Vector3 blockPos);
//Get strength of sunlight based on time.
float World_GetSunlightStrength(void);
//Get a Chunk at a given position.
Chunk* World_GetChunkAt(Vector3 pos);
//Teleport an Entity in the world
void World_TeleportEntity(int id, Vector3 position, Vector3 rotation);
//Add an Entity to the world
void World_AddEntity(int id, int type, int modelId, Vector3 position, Vector3 rotation);
//Remove an Entity from the world
void World_RemoveEntity(int id);

#endif
