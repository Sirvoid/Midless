#include "raylib.h" 
#include "chunk.h"
#include <stdio.h>
#include <stdlib.h>

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
        chunk->data[i] = 1;
    }
}

void Chunk_BuildMesh(Chunk *chunk) {
    Chunk_AllocateMeshData(&chunk->mesh, 2 * 6 * CHUNK_SIZE);
    
    BFH_ResetIndexes();
    Chunk_facesCounter = 0;
    for(int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 pos = Chunk_IndexToPos(i);
        Chunk_AddCube(chunk, &chunk->mesh, pos, chunk->data[i]);
    }
    
    Chunk_ReAllocateMeshData(&chunk->mesh, Chunk_facesCounter * 2);
    
    UploadMesh(&chunk->mesh, false);
}

void Chunk_AddCube(Chunk *chunk, Mesh *mesh, Vector3 pos, int block_id) {
    
    if(block_id == 0) {
        return;
    }
    
    for(int i = 0; i < 6; i++) {
        Chunk_AddFace(chunk, mesh, pos, (BlockFace)i);
    }
}

void Chunk_AddFace(Chunk *chunk, Mesh *mesh, Vector3 pos, BlockFace face) {
    Vector3 faceDir = BFH_GetDirection(face);
    Vector3 nextPos = (Vector3){ pos.x + faceDir.x, pos.y + faceDir.y, pos.z + faceDir.z };
    int nextIndex = Chunk_PosToIndex(nextPos);
    if(Chunk_IsValidPos(nextPos) && chunk->data[nextIndex] != 0) {
        return;
    }
    BFH_AddFace(mesh, face, pos);
    Chunk_facesCounter++;
}

void Chunk_Unload(Chunk *chunk) {
    UnloadMesh(chunk->mesh);
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
