/**
 * Copyright (c) 2022 Sirvoid
 * This software is released under the MIT License.
 */

#include <stddef.h>
#include <stdlib.h>
#include "raylib.h"
#include "chunkmeshgeneration.h"
#include "blockmeshgeneration.h"

MeshBuffers *ChunkMeshGeneration_CreateBuffers(void) {
    return calloc(1, sizeof(MeshBuffers));
}

static bool ReserveBank(MeshBank *bank, int vertices) {
    if (vertices <= bank->capacity) return true;
    unsigned char *positions = malloc(vertices * 3);
    unsigned short *texcoords = malloc(vertices * 2 * sizeof(unsigned short));
    unsigned char *colors = malloc(vertices);
    unsigned short *indices = malloc(vertices / 4 * 6 * sizeof(unsigned short));
    if (!positions || !texcoords || !colors || !indices) {
        free(positions);
        free(texcoords);
        free(colors);
        free(indices);
        return false;
    }
    free(bank->vertices);
    free(bank->texcoords);
    free(bank->colors);
    free(bank->indices);
    bank->vertices = positions;
    bank->texcoords = texcoords;
    bank->colors = colors;
    bank->indices = indices;
    bank->capacity = vertices;
    return true;
}

void ChunkMeshGeneration_FreeBuffers(MeshBuffers *buffers) {
    if (!buffers) return;
    for (int i = 0; i < 2; i++) {
        free(buffers->banks[i].vertices);
        free(buffers->banks[i].texcoords);
        free(buffers->banks[i].colors);
        free(buffers->banks[i].indices);
    }
    free(buffers);
}

static bool SameLiquidOccludes(const Block *block, const Block *next) {
    return block == next && block->colliderType == BLOCK_COLLIDER_LIQUID &&
           block->modelType == BLOCK_MODEL_SOLID && block->fullCube;
}

static bool FaceVisible(const Block *block, const Block *next) {
    if (SameLiquidOccludes(block, next)) return false;
    if (next->colliderType != BLOCK_COLLIDER_SOLID) return true;
    if (block->fastOpaqueCube) {
        return !next->fastOpaqueCube;
    }
    if (block->renderType == BLOCK_RENDER_OPAQUE) return ChunkMeshGeneration_IsOpaqueFaceVisible(block, next);
    if (block->renderType == BLOCK_RENDER_TRANSLUCENT) return ChunkMeshGeneration_IsTranslucentFaceVisible(block, next);
    return true;
}

static void AddFace(MeshBuffers *buffers, MeshSnapshot *chunk, const Block *definitions,
                    const BlockMeshTemplate *templates, int blockIndex, int x, int y, int z,
                    BlockFace face, const Block *block) {
    const BlockMeshTemplate *model=&templates[block - definitions];
    int templateFace=(int)face;
    face=(BlockFace)model->directions[templateFace];
    static const int indexOffsets[6] = {-1, 1, CHUNK_SIZE_XZ, -CHUNK_SIZE_XZ, CHUNK_SIZE_X, -CHUNK_SIZE_X};
    int nx = x, ny = y, nz = z;
    if (face == BLOCK_FACE_LEFT) nx--;
    else if (face == BLOCK_FACE_RIGHT) nx++;
    else if (face == BLOCK_FACE_TOP) ny++;
    else if (face == BLOCK_FACE_BOTTOM) ny--;
    else if (face == BLOCK_FACE_FRONT) nz++;
    else nz--;


    int nextIndex;
    if ((unsigned)nx < CHUNK_SIZE_X && (unsigned)ny < CHUNK_SIZE_Y && (unsigned)nz < CHUNK_SIZE_Z) {
        nextIndex = blockIndex + indexOffsets[(int)face];
    } else {
        if (!chunk->neighbors[(int)face]) return;
        int cell;
        if (face == BLOCK_FACE_LEFT || face == BLOCK_FACE_RIGHT) cell = y * 16 + z;
        else if (face == BLOCK_FACE_TOP || face == BLOCK_FACE_BOTTOM) cell = z * 16 + x;
        else cell = y * 16 + x;
        nextIndex = CHUNK_SIZE + (int)face * CHUNK_SIZE_XZ + cell;
    }

    const Block *next = &definitions[chunk->cells[nextIndex].block];
    bool sprite = block->modelType == BLOCK_MODEL_SPRITE;
    if (!sprite && model->boundary[templateFace]) {
        if (next->geometry.enabled) { if(next->fastOpaqueCube) return; }
        else if(block->geometry.enabled) { if(next->fastOpaqueCube) return; }
        else if(!FaceVisible(block,next)) return;
    }

    int light;
    int sunlight;
    if (sprite || block->renderType == BLOCK_RENDER_TRANSPARENT) {
        light = chunk->cells[blockIndex].light;
        sunlight = chunk->cells[blockIndex].sky;
    } else if (!block->fullCube) {
        int ownLight = chunk->cells[blockIndex].light;
        int ownSunlight = chunk->cells[blockIndex].sky;
        int neighborLight = chunk->cells[nextIndex].light;
        int neighborSunlight = chunk->cells[nextIndex].sky;
        light = ownLight > neighborLight ? ownLight : neighborLight;
        sunlight = ownSunlight > neighborSunlight ? ownSunlight : neighborSunlight;
    } else {
        light = chunk->cells[nextIndex].light;
        sunlight = chunk->cells[nextIndex].sky;
    }

    int bankIndex = block->renderType == BLOCK_RENDER_TRANSLUCENT ? 1 : 0;
    MeshBank *bank = &buffers->banks[bankIndex];
    BlockMesh_WriteFace(bank->vertices, bank->indices, bank->texcoords, bank->colors,
        bank->vertexCount, model, templateFace, x, y, z, sprite, light, sunlight);
    bank->vertexCount += 4;
}

void ChunkMeshGeneration_Compute(MeshBuffers *buffers, MeshSnapshot *chunk,
                                 const Block *definitions, const BlockMeshTemplate *templates) {
    buffers->valid = false;
    // Bound output by the faces present in this snapshot, not the worst possible chunk.
    int vertices[2] = {0};
    for (int i = 0; i < CHUNK_SIZE; i++) {
        int id = chunk->cells[i].block;
        if (definitions[id].modelType == BLOCK_MODEL_GAS) continue;
        int bank = definitions[id].renderType == BLOCK_RENDER_TRANSLUCENT ? 1 : 0;
        vertices[bank] += templates[id].faceCount * 4;
    }
    if (!ReserveBank(&buffers->banks[0], vertices[0]) || !ReserveBank(&buffers->banks[1], vertices[1])) return;
    buffers->banks[0].vertexCount = buffers->banks[1].vertexCount = 0;
    buffers->onlyAir = true;
    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X;
            for (int x = 0; x < CHUNK_SIZE_X; x++, index++) {
                int id = chunk->cells[index].block;
                const Block *block = &definitions[id];
                if (block->modelType == BLOCK_MODEL_GAS) continue;
                buffers->onlyAir = false;
                for (int face = 0; face < templates[id].faceCount; face++) {
                    AddFace(buffers, chunk, definitions, templates, index, x, y, z, (BlockFace)face, block);
                }
            }
        }
    }
    buffers->valid = true;
}

void ChunkMeshGeneration_Upload(MeshBuffers *buffers, Chunk *chunk) {
    ChunkMesh *meshes[2] = {&chunk->mesh, &chunk->meshTransparent};
    for (int i = 0; i < 2; i++) {
        MeshBank *bank = &buffers->banks[i];
        meshes[i]->vertexCount = bank->vertexCount;
        meshes[i]->triangleCount = bank->vertexCount / 2;
        if (bank->vertexCount) ChunkMesh_Upload(meshes[i], bank->vertices, bank->indices, bank->texcoords, bank->colors);
        else ChunkMesh_Clear(meshes[i]);
    }
    chunk->onlyAir = buffers->onlyAir;
    chunk->hasTransparency = buffers->banks[1].vertexCount > 0;
    chunk->isBuilt = true;
}

bool ChunkMeshGeneration_IsOpaqueFaceVisible(const Block *block, const Block *next) {
    if (SameLiquidOccludes(block, next)) return false;
    if (next->colliderType != BLOCK_COLLIDER_SOLID) return true;
    if (next->modelType == BLOCK_MODEL_GAS) return true;
    if (next->renderType != BLOCK_RENDER_OPAQUE) return true;
    if (next->modelType == BLOCK_MODEL_SPRITE) return true;
    return !block->fullCube || !next->fullCube;
}

bool ChunkMeshGeneration_IsTranslucentFaceVisible(const Block *block, const Block *next) {
    if (SameLiquidOccludes(block, next)) return false;
    if (next->colliderType != BLOCK_COLLIDER_SOLID) return true;
    if (next->modelType == BLOCK_MODEL_GAS) return true;
    if (next->renderType == BLOCK_RENDER_TRANSPARENT) return true;
    return !block->fullCube || !next->fullCube;
}
