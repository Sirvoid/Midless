#include <stdio.h>
#include <stdlib.h>
#include "raylib.h" 
#include "raymath.h"
#include "chunk.h"
#include "world.h"
#include "blockfacehelper.h"
#include "block.h"
#include "chunkmesh.h"
#include "stb_perlin.h"

int Chunk_facesCounter = 0;

void Chunk_Init(Chunk *chunk, Vector3 pos) {
    chunk->position = pos;
    
    if(chunk->position.y > 0) return;
    
    for(int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 npos = Vector3Add(Chunk_IndexToPos(i), Vector3Multiply(chunk->position, (Vector3) {CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z}) );
        float noise = (stb_perlin_fbm_noise3( npos.x / WORLD_SIZE_X / 4.0f, 1.0f,  npos.z / WORLD_SIZE_Z / 4.0f, 2.0f, 0.5f, 3) + 1.0f) / 2.0f * WORLD_SIZE_Y;
        float ny = npos.y - 8;
        
        if(ny < noise) chunk->data[i] = 3;
        if(ny + 1 < noise) chunk->data[i] = 2;
        if(ny + 2 < noise) chunk->data[i] = 1;
    }
    
}

void Chunk_Unload(Chunk *chunk) {
    ChunkMesh_Unload(*chunk->mesh);
}

void Chunk_AllocateMeshData(ChunkMesh *mesh, int triangleCount) {
    mesh->vertexCount = triangleCount * 3;
    mesh->triangleCount = triangleCount;

    int vertFX3 = mesh->vertexCount * 3 * sizeof(float);

    mesh->vertices = (float*)MemAlloc(vertFX3);
    mesh->texcoords = (float*)MemAlloc(mesh->vertexCount * 2 * sizeof(float));
    mesh->colors = (unsigned char*)MemAlloc(mesh->vertexCount * sizeof(unsigned char));
}

void Chunk_ReAllocateMeshData(ChunkMesh *mesh, int triangleCount) {
    mesh->vertexCount = triangleCount * 3;
    mesh->triangleCount = triangleCount;
    
    int vertFX3 = mesh->vertexCount * 3 * sizeof(float);
    
    mesh->vertices = (float*)MemRealloc(mesh->vertices, vertFX3);
    mesh->texcoords = (float*)MemRealloc(mesh->texcoords, mesh->vertexCount * 2 * sizeof(float));
    mesh->colors = (unsigned char*)MemRealloc(mesh->colors, mesh->vertexCount * sizeof(unsigned char));
}

void Chunk_BuildMesh(Chunk *chunk) {
    
    if(chunk->loaded == 1) ChunkMesh_Unload(*chunk->mesh);
    chunk->loaded = 1;
    
    chunk->mesh = (ChunkMesh*)MemAlloc(sizeof(ChunkMesh));
    Chunk_AllocateMeshData(chunk->mesh, 2 * 6 * CHUNK_SIZE);
    
    BFH_ResetIndexes();
    Chunk_facesCounter = 0;
    
    Vector3 chunkWorldPos = (Vector3) { chunk->position.x * CHUNK_SIZE_X, chunk->position.y * CHUNK_SIZE_Y, chunk->position.z * CHUNK_SIZE_Z };
    
    for(int i = 0; i < CHUNK_SIZE; i++) {
        Vector3 pos = Chunk_IndexToPos(i);
        Vector3 worldPos = Vector3Add(pos, chunkWorldPos);
        
        Chunk_AddCube(chunk, chunk->mesh, pos, worldPos, chunk->data[i]);
    }
    
    Chunk_ReAllocateMeshData(chunk->mesh, Chunk_facesCounter * 2);
    
    ChunkMesh_Upload(chunk->mesh);
}

void Chunk_AddCube(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Vector3 worldPos, int blockID) {
    
    if(blockID == 0) return;
    
    for(int i = 0; i < 6; i++) {
        Chunk_AddFace(chunk, mesh, pos, worldPos, (BlockFace)i, blockID);
    }
}

void Chunk_AddFace(Chunk *chunk, ChunkMesh *mesh, Vector3 pos, Vector3 worldPos, BlockFace face, int blockID) {
    Vector3 faceDir = BFH_GetDirection(face);
    Vector3 nextPos = Vector3Add(worldPos, faceDir);
    
    if(World_GetBlock(nextPos) != 0) return;
    
    BFH_AddFace(mesh, face, pos, blockID);
    Chunk_facesCounter++;
}

void Chunk_SetBlock(Chunk *chunk, Vector3 pos, int blockID) {
    if(Chunk_IsValidPos(pos)) {
        chunk->data[Chunk_PosToIndex(pos)] = blockID;
    }
}

int Chunk_GetBlock(Chunk *chunk, Vector3 pos) {
    if(Chunk_IsValidPos(pos)) {
        return chunk->data[Chunk_PosToIndex(pos)];
    }
    return 0;
}

void Chunk_GetBorderingChunks(Chunk *chunk, Vector3 pos, Chunk *(*dest)[3]) {
    int i = 0;
    
    if(pos.x == 0) {
        *dest[i++] = World_GetChunkAt(Vector3Add(chunk->position, (Vector3){-1, 0, 0}));
    } else if(pos.x == CHUNK_SIZE_X - 1) {
        *dest[i++] = World_GetChunkAt(Vector3Add(chunk->position, (Vector3){1, 0, 0}));
    } 

    if(pos.y == 0) {
        *dest[i++] = World_GetChunkAt(Vector3Add(chunk->position, (Vector3){0, -1, 0}));
    } else if(pos.y == CHUNK_SIZE_Y - 1) {
        *dest[i++] = World_GetChunkAt(Vector3Add(chunk->position, (Vector3){0, 1, 0}));
    } 

    if(pos.z == 0) {
        *dest[i++] = World_GetChunkAt(Vector3Add(chunk->position, (Vector3){0, 0, -1}));
    } else if(pos.z == CHUNK_SIZE_Z - 1) {
        *dest[i++] = World_GetChunkAt(Vector3Add(chunk->position, (Vector3){0, 0, 1}));
    }
}

int Chunk_IsValidPos(Vector3 pos) {
    return pos.x >= 0 && pos.x < CHUNK_SIZE_X && pos.y >= 0 && pos.y < CHUNK_SIZE_Y && pos.z >= 0 && pos.z < CHUNK_SIZE_Z;
}

Vector3 Chunk_IndexToPos(int index) {
    int x = (int)(index % CHUNK_SIZE_X);
	int y = (int)(index / (CHUNK_SIZE_X * CHUNK_SIZE_Z));
	int z = (int)(index / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
    return (Vector3){x, y, z};
}

int Chunk_PosToIndex(Vector3 pos) {
    return ((int)pos.y * CHUNK_SIZE_Z + (int)pos.z) * CHUNK_SIZE_X + (int)pos.x;
    
}
