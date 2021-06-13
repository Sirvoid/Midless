#include <stdio.h>
#include <stdlib.h>
#include "raylib.h" 
#include "chunk.h"
#include "world.h"

int Chunk_facesCounter = 0;

void Chunk_AllocateMeshData(Mesh *mesh, int triangleCount)
{
    mesh->vertexCount = triangleCount * 3;
    mesh->triangleCount = triangleCount;

    mesh->vertices = (float*)malloc(mesh->vertexCount * 3 * sizeof(float));
    mesh->texcoords = (float*)malloc(mesh->vertexCount * 2 * sizeof(float));
    mesh->normals = (float*)malloc(mesh->vertexCount * 3 * sizeof(float));
}

void Chunk_ReAllocateMeshData(Mesh *mesh, int triangleCount)
{
   mesh->vertexCount = triangleCount * 3;
   mesh->triangleCount = triangleCount;

   mesh->vertices = (float*)realloc(mesh->vertices, mesh->vertexCount * 3 * sizeof(float));
   mesh->texcoords = (float*)realloc(mesh->texcoords, mesh->vertexCount * 2 * sizeof(float));
   mesh->normals = (float*)realloc(mesh->normals, mesh->vertexCount * 3 * sizeof(float));
}

void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    for(int i = 0; i < CHUNK_SIZE; i++) {
        if(i < CHUNK_SIZE_X * CHUNK_SIZE_Z) chunk->data[i] = rand() % 4;
    }
}

void Chunk_BuildMesh(Chunk *chunk) {
    
    if(chunk->loaded == 1) UnloadMesh(*chunk->mesh);
    chunk->loaded = 1;
    
    chunk->mesh = (Mesh*)malloc(sizeof(Mesh));
    Chunk_AllocateMeshData(chunk->mesh, 2 * 6 * CHUNK_SIZE);
    
    BFH_ResetIndexes();
    Chunk_facesCounter = 0;
    for(int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 cpos = Chunk_IndexToPos(i);
        Vector3 wpos = (Vector3) { chunk->position.x * CHUNK_SIZE_X, chunk->position.y * CHUNK_SIZE_Y, chunk->position.z * CHUNK_SIZE_Z };
        Vector3 pos = (Vector3) { cpos.x + wpos.x, cpos.y + wpos.y, cpos.z + wpos.z };
        Chunk_AddCube(chunk, chunk->mesh, pos, chunk->data[i]);
    }
    
    Chunk_ReAllocateMeshData(chunk->mesh, Chunk_facesCounter * 2);
    
    UploadMesh(chunk->mesh, true);
}

void Chunk_AddCube(Chunk *chunk, Mesh *mesh, Vector3 pos, int blockID) {
    
    if(blockID == 0) return;
    
    for(int i = 0; i < 6; i++) {
        Chunk_AddFace(chunk, mesh, pos, (BlockFace)i, blockID);
    }
}

void Chunk_AddFace(Chunk *chunk, Mesh *mesh, Vector3 pos, BlockFace face, int blockID) {
    Vector3 faceDir = BFH_GetDirection(face);
    Vector3 nextPos = (Vector3){ pos.x + faceDir.x, pos.y + faceDir.y, pos.z + faceDir.z };
    if(World_GetBlock(nextPos) != 0) {
        return;
    }
    BFH_AddFace(mesh, face, pos, blockID);
    Chunk_facesCounter++;
}

void Chunk_Unload(Chunk *chunk) {
    UnloadMesh(*chunk->mesh);
}

void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID) {
    if(Chunk_IsValidPos(pos)) {
        chunk->data[Chunk_PosToIndex(pos)] = blockID;
        Chunk_BuildMesh(chunk);
    }
}

int Chunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if(Chunk_IsValidPos(pos)) {
        return chunk->data[Chunk_PosToIndex(pos)];
    }
    return 0;
}

int Chunk_IsValidPos(Vector3 pos) {
    return pos.x >= 0 && pos.x < CHUNK_SIZE_X && pos.y >= 0 && pos.y < CHUNK_SIZE_Y && pos.z >= 0 && pos.z < CHUNK_SIZE_Z;
}

Vector3 Chunk_IndexToPos(int index) {
    int x = (int)(index % CHUNK_SIZE_X);
	int y = (int)(index / (CHUNK_SIZE_X * CHUNK_SIZE_Z));
	int z = (int)((int)(index / CHUNK_SIZE_X) % CHUNK_SIZE_Z);
    return (Vector3){x, y, z};
}

int Chunk_PosToIndex(Vector3 pos) {
    return (int)(((int)pos.y * CHUNK_SIZE_Z + (int)pos.z) * CHUNK_SIZE_X + (int)pos.x);
    
}
