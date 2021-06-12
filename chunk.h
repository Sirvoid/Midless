#ifndef G_CHUNK_H
#define G_CHUNK_H

#include "blockfacehelper.h"
#include "raylib.h"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z)

typedef struct {
    Mesh mesh;
    Vector3 position;
    int data[CHUNK_SIZE]; //16x16x16
} Chunk;


void Chunk_Init(Chunk *chunk, Vector3 pos);
void Chunk_AllocateMeshData(Mesh *mesh, int triangleCount);
void Chunk_ReAllocateMeshData(Mesh *mesh, int triangleCount);
void Chunk_BuildMesh(Chunk *chunk);
void Chunk_AddCube(Chunk *chunk, Mesh *mesh, Vector3 pos, int block_id);
void Chunk_AddFace(Chunk *chunk, Mesh *mesh, Vector3 pos, BlockFace face);
void Chunk_Unload(Chunk *chunk);

int Chunk_IsValidPos(Vector3 pos);
int Chunk_PosToIndex(Vector3 pos);

Vector3 Chunk_IndexToPos(int index);

#endif