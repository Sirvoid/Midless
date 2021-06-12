#include "world.h"
#include "raylib.h" 
#include "chunk.h"

void World_Init(World *world) {
    world->mat = LoadMaterialDefault();
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk* chunk = &world->chunks[i];
        Vector3 pos = World_ChunkIndexToPos(i);
        Chunk_Init(chunk, pos);
        Chunk_BuildMesh(chunk);
    }
}

Vector3 World_ChunkIndexToPos(int chunkIndex) {
    int x = (int)(chunkIndex % WORLD_SIZE_X);
	int y = (int)(chunkIndex / (WORLD_SIZE_X * WORLD_SIZE_Z));
	int z = (int)((int)(chunkIndex / WORLD_SIZE_X) % WORLD_SIZE_Z);
    return (Vector3){x, y, z};
}

int World_ChunkPosToIndex(Vector3 chunkPos) {
    return (int)(((int)chunkPos.y * WORLD_SIZE_Z + (int)chunkPos.z) * WORLD_SIZE_X + (int)chunkPos.x);
}

void World_Unload(World *world) {
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk_Unload(&world->chunks[i]);
    }
}

void World_ApplyTexture(World *world, Texture2D texture) {
    SetMaterialTexture(&world->mat, MATERIAL_MAP_DIFFUSE, texture);
}

void World_Draw(World *world) {
    for(int i = 0; i < WORLD_SIZE; i++) {
        Chunk* chunk = &world->chunks[i];
        Vector3 cpos = chunk->position;
        Vector3 pos = (Vector3) { cpos.x * CHUNK_SIZE_X, cpos.y * CHUNK_SIZE_Y, cpos.z * CHUNK_SIZE_Z };
        Matrix matrix = (Matrix) { 1, 0, 0, pos.x, 
                                   0, 1, 0, pos.y,
                                   0, 0, 1, pos.z,
                                   0, 0, 0, 1 };
        DrawMesh(chunk->mesh, world->mat, matrix);
    }
}