/**
 * Copyright (c) 2022 Sirvoid
 * This software is released under the MIT License.
 */

#include <stddef.h>
#include "raylib.h"
#include "chunkmeshgeneration.h"
#include "blockmeshgeneration.h"

int chunkTriangleCount = 0;
int chunkTransparentTriangleCount = 0;

static unsigned char *vertices, *colors, *verticesT, *colorsT;
static unsigned short *indices, *texcoords, *indicesT, *texcoordsT;

void ChunkMeshGeneration_Init(void) {
    int vertexCount = 2 * 6 * CHUNK_SIZE * 2;
    int triangleCount = 2 * 6 * CHUNK_SIZE;
    vertices = MemAlloc(vertexCount * 3);
    texcoords = MemAlloc(vertexCount * 2 * sizeof(unsigned short));
    colors = MemAlloc(vertexCount);
    indices = MemAlloc(triangleCount * 3 * sizeof(unsigned short));
    verticesT = MemAlloc(vertexCount * 3);
    texcoordsT = MemAlloc(vertexCount * 2 * sizeof(unsigned short));
    colorsT = MemAlloc(vertexCount);
    indicesT = MemAlloc(triangleCount * 3 * sizeof(unsigned short));
}

void ChunkMeshGeneration_Shutdown(void) {
    MemFree(vertices);
    MemFree(texcoords);
    MemFree(colors);
    MemFree(indices);
    MemFree(verticesT);
    MemFree(texcoordsT);
    MemFree(colorsT);
    MemFree(indicesT);

    vertices = NULL;
    texcoords = NULL;
    colors = NULL;
    indices = NULL;
    verticesT = NULL;
    texcoordsT = NULL;
    colorsT = NULL;
    indicesT = NULL;
}

static bool FaceVisible(const Block *block, const Block *next) {
    if (block->fastOpaqueCube) {
        return !next->fastOpaqueCube;
    }
    if (block->renderType == BLOCK_RENDER_OPAQUE) return ChunkMeshGeneration_IsOpaqueFaceVisible(block, next);
    if (block->renderType == BLOCK_RENDER_TRANSLUCENT) return ChunkMeshGeneration_IsTranslucentFaceVisible(block, next);
    return true;
}

static void AddFace(Chunk *chunk, int blockIndex, int x, int y, int z,
                    BlockFace face, const Block *block) {
    static const int indexOffsets[6] = {-1, 1, CHUNK_SIZE_XZ, -CHUNK_SIZE_XZ, CHUNK_SIZE_X, -CHUNK_SIZE_X};
    int nx = x, ny = y, nz = z;
    if (face == BLOCK_FACE_LEFT) nx--;
    else if (face == BLOCK_FACE_RIGHT) nx++;
    else if (face == BLOCK_FACE_TOP) ny++;
    else if (face == BLOCK_FACE_BOTTOM) ny--;
    else if (face == BLOCK_FACE_FRONT) nz++;
    else nz--;

    Chunk *nextChunk = chunk;
    int nextIndex;
    if ((unsigned)nx < CHUNK_SIZE_X && (unsigned)ny < CHUNK_SIZE_Y && (unsigned)nz < CHUNK_SIZE_Z) {
        nextIndex = blockIndex + indexOffsets[(int)face];
    } else {
        nextChunk = chunk->neighbours[(int)face];
        if (nextChunk == NULL) return;
        if (nx < 0) nx = CHUNK_SIZE_X - 1; else if (nx == CHUNK_SIZE_X) nx = 0;
        if (ny < 0) ny = CHUNK_SIZE_Y - 1; else if (ny == CHUNK_SIZE_Y) ny = 0;
        if (nz < 0) nz = CHUNK_SIZE_Z - 1; else if (nz == CHUNK_SIZE_Z) nz = 0;
        nextIndex = (ny * CHUNK_SIZE_Z + nz) * CHUNK_SIZE_X + nx;
    }

    const Block *next = &blockDefinitions[nextChunk->data[nextIndex]];
    bool sprite = block->modelType == BLOCK_MODEL_SPRITE;
    if (!sprite && !FaceVisible(block, next)) return;

    int lightIndex = (sprite || block->renderType == BLOCK_RENDER_TRANSPARENT) ? blockIndex : nextIndex;
    Chunk *lightChunk = (sprite || block->renderType == BLOCK_RENDER_TRANSPARENT) ? chunk : nextChunk;
    int light = lightChunk->lightData[lightIndex];
    int sunlight = lightChunk->sunlightData[lightIndex];

    if (block->renderType == BLOCK_RENDER_TRANSLUCENT) {
        chunkTransparentTriangleCount += 2;
        BlockMesh_AddFace(verticesT, indicesT, texcoordsT, colorsT, face, x, y, z, block, 1, light, sunlight);
    } else {
        chunkTriangleCount += 2;
        BlockMesh_AddFace(vertices, indices, texcoords, colors, face, x, y, z, block, 0, light, sunlight);
    }
}

void ChunkMeshGeneration_Build(Chunk *chunk) {
    BlockMesh_ResetIndexes();
    chunkTriangleCount = 0;
    chunkTransparentTriangleCount = 0;
    chunk->hasTransparency = false;
    chunk->onlyAir = true;

    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X;
            for (int x = 0; x < CHUNK_SIZE_X; x++, index++) {
                unsigned int blockId = chunk->data[index];
                const Block *block = &blockDefinitions[blockId];
                if (block->modelType == BLOCK_MODEL_GAS) continue;
                chunk->onlyAir = false;
                if (block->renderType == BLOCK_RENDER_TRANSLUCENT) chunk->hasTransparency = true;
                int faceCount = block->modelType == BLOCK_MODEL_SPRITE ? 4 : 6;
                for (int face = 0; face < faceCount; face++) AddFace(chunk, index, x, y, z, (BlockFace)face, block);
            }
        }
    }

    chunk->mesh.vertexCount = chunkTriangleCount * 2;
    chunk->mesh.triangleCount = chunkTriangleCount;
    chunk->meshTransparent.vertexCount = chunkTransparentTriangleCount * 2;
    chunk->meshTransparent.triangleCount = chunkTransparentTriangleCount;

    if (chunk->mesh.triangleCount > 0) ChunkMesh_Upload(&chunk->mesh, vertices, indices, texcoords, colors);
    else ChunkMesh_Clear(&chunk->mesh);
    if (chunk->meshTransparent.triangleCount > 0) ChunkMesh_Upload(&chunk->meshTransparent, verticesT, indicesT, texcoordsT, colorsT);
    else ChunkMesh_Clear(&chunk->meshTransparent);
    chunk->isBuilt = true;
}

bool ChunkMeshGeneration_IsOpaqueFaceVisible(const Block *block, const Block *next) {
    if (next->modelType == BLOCK_MODEL_GAS) return true;
    if (next->renderType != BLOCK_RENDER_OPAQUE) return true;
    if (next->modelType == BLOCK_MODEL_SPRITE) return true;
    return !block->fullCube || !next->fullCube;
}

bool ChunkMeshGeneration_IsTranslucentFaceVisible(const Block *block, const Block *next) {
    if (next->modelType == BLOCK_MODEL_GAS) return true;
    if (next->renderType == BLOCK_RENDER_TRANSPARENT) return true;
    return !block->fullCube || !next->fullCube;
}
