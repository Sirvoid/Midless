/**
 * Copyright (c) 2021 Sirvoid
 * This software is released under the MIT License.
 */

#include <string.h>
#include "block.h"
#include "blockmeshgeneration.h"

static BlockMeshTemplate templates[BLOCK_RUNTIME_COUNT];
static int verticesIndex[2];

static const unsigned char spriteVertices[4][12] = {
    {0,0,0, 16,16,16, 0,16,0, 16,0,16},
    {16,0,16, 0,16,0, 16,16,16, 0,0,0},
    {0,0,16, 16,16,0, 0,16,16, 16,0,0},
    {16,0,0, 0,16,16, 16,16,0, 0,0,16}
};

static void BuildSolidVertices(const Block *block, BlockMeshTemplate *out) {
    unsigned char minX = (unsigned char)block->minBB.x, minY = (unsigned char)block->minBB.y, minZ = (unsigned char)block->minBB.z;
    unsigned char maxX = (unsigned char)block->maxBB.x, maxY = (unsigned char)block->maxBB.y, maxZ = (unsigned char)block->maxBB.z;
    unsigned char v[6][12] = {
        {minX,minY,minZ, minX,maxY,maxZ, minX,maxY,minZ, minX,minY,maxZ},
        {maxX,minY,maxZ, maxX,maxY,minZ, maxX,maxY,maxZ, maxX,minY,minZ},
        {minX,maxY,maxZ, maxX,maxY,minZ, minX,maxY,minZ, maxX,maxY,maxZ},
        {minX,minY,minZ, maxX,minY,maxZ, minX,minY,maxZ, maxX,minY,minZ},
        {minX,minY,maxZ, maxX,maxY,maxZ, minX,maxY,maxZ, maxX,minY,maxZ},
        {maxX,minY,minZ, minX,maxY,minZ, maxX,maxY,minZ, minX,minY,minZ}
    };
    memcpy(out->vertices, v, sizeof(v));
}

static void BuildBoxTemplate(const Block *block, BlockMeshTemplate *out) {
    if (block->modelType == BLOCK_MODEL_SPRITE) memcpy(out->vertices, spriteVertices, sizeof(spriteVertices));
    else BuildSolidVertices(block, out);

    int faceCount = block->modelType == BLOCK_MODEL_SPRITE ? 4 : 6;
    for (int face = 0; face < faceCount; face++) {
        int textureX = (block->textures[face] % 16) * 16;
        int textureY = (block->textures[face] / 16) * 16;
        int minX = textureX, minY = textureY, maxX = textureX + 16, maxY = textureY + 16;
        if (block->modelType != BLOCK_MODEL_SPRITE) {
            if (face == BLOCK_FACE_FRONT || face == BLOCK_FACE_BACK) {
                maxY -= 16 - (int)block->maxBB.y; minY += (int)block->minBB.y;
                maxX -= 16 - (int)block->maxBB.x; minX += (int)block->minBB.x;
            } else if (face == BLOCK_FACE_LEFT || face == BLOCK_FACE_RIGHT) {
                maxX -= 16 - (int)block->maxBB.z; minX += (int)block->minBB.z;
                maxY -= 16 - (int)block->maxBB.y; minY += (int)block->minBB.y;
            } else {
                maxX -= 16 - (int)block->maxBB.x; minX += (int)block->minBB.x;
                maxY -= 16 - (int)block->maxBB.z; minY += (int)block->minBB.z;
            }
        }
        unsigned short uv[8] = {minX,maxY, maxX,minY, minX,minY, maxX,maxY};
        memcpy(out->texcoords[face], uv, sizeof(uv));
    }
}

void BlockMesh_BuildTemplate(int id) {
    if(id<0 || id>=BLOCK_RUNTIME_COUNT) return;
    const Block *block=&blockDefinitions[id]; BlockMeshTemplate *out=&templates[id];
    memset(out,0,sizeof(*out));
    int boxes=block->geometry.enabled?block->geometry.boxCount:1;
    static const int rotatedFace[6]={5,4,2,3,0,1};
    for(int box=0;box<boxes;box++) {
        Block part=*block;
        if(block->geometry.enabled) {
            const BlockModelBox *b=&block->geometry.boxes[box];
            part.minBB=(Vector3){b->bounds.min[0],b->bounds.min[1],b->bounds.min[2]};
            part.maxBB=(Vector3){b->bounds.max[0],b->bounds.max[1],b->bounds.max[2]};
            for(int f=0;f<6;f++) part.textures[f]=b->textures[f];
        }
        BlockMeshTemplate temp={0}; BuildBoxTemplate(&part,&temp);
        int faces=part.modelType==BLOCK_MODEL_SPRITE?4:6;
        for(int face=0;face<faces;face++) {
            int dest=out->faceCount++, direction=face;
            memcpy(out->vertices[dest],temp.vertices[face],12);
            memcpy(out->texcoords[dest],temp.texcoords[face],sizeof(temp.texcoords[face]));
            for(int turn=0;turn<block->geometry.rotation;turn++) {
                direction=rotatedFace[direction];
                for(int v=0;v<4;v++) { int x=out->vertices[dest][v*3]; out->vertices[dest][v*3]=16-out->vertices[dest][v*3+2]; out->vertices[dest][v*3+2]=x; }
            }
            out->directions[dest]=direction;
            int axis=direction<2?0:direction<4?1:2;
            int edge=direction==0 || direction==3 || direction==5?0:16;
            bool boundary=part.modelType!=BLOCK_MODEL_SPRITE;
            for(int v=0;v<4;v++) boundary &= out->vertices[dest][v*3+axis]==edge;
            out->boundary[dest]=boundary;
        }
    }
}

void BlockMesh_BuildTemplates(void) {
    for (int id = 0; id < 256; id++) BlockMesh_BuildTemplate(id);
}

const BlockMeshTemplate *BlockMesh_GetTemplate(int blockId) {
    if (blockId < 0 || blockId >= BLOCK_RUNTIME_COUNT) return NULL;
    return &templates[blockId];
}

Vector3 BlockMesh_GetDirection(BlockFace face) {
    static const Vector3 directions[6] = {{-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    return directions[(int)face];
}

void BlockMesh_ResetIndexes(void) {
    memset(verticesIndex, 0, sizeof(verticesIndex));
}

static unsigned char FaceColor(BlockFace face, bool sprite, int light, int sunlight) {
    int shade = 0;
    if (!sprite) {
        if (face == BLOCK_FACE_BOTTOM) shade = 8;
        else if (face == BLOCK_FACE_LEFT || face == BLOCK_FACE_RIGHT) shade = 5;
        else if (face == BLOCK_FACE_FRONT || face == BLOCK_FACE_BACK) shade = 3;
    }
    light -= shade; sunlight -= shade;
    if (light < 0) light = 0;
    if (sunlight < 0) sunlight = 0;
    return (unsigned char)((light << 4) | sunlight);
}

void BlockMesh_AddFace(unsigned char *vertices, unsigned short *indices, unsigned short *texcoords,
                       unsigned char *colors, BlockFace face, int x, int y, int z,
                       const Block *block, int translucent, int light, int sunlight) {
    const BlockMeshTemplate *meshTemplate = &templates[block - blockDefinitions];
    int vertex = verticesIndex[translucent] / 3;
    BlockMesh_WriteFace(vertices, indices, texcoords, colors, vertex, meshTemplate,
        face, x, y, z, block->modelType == BLOCK_MODEL_SPRITE, light, sunlight);
    verticesIndex[translucent] += 12;
}

void BlockMesh_WriteFace(unsigned char *vertices, unsigned short *indices, unsigned short *texcoords,
                        unsigned char *colors, int vertex, const BlockMeshTemplate *meshTemplate,
                        int face, int x, int y, int z, bool sprite, int light, int sunlight) {
    const unsigned char *source = meshTemplate->vertices[face];
    int baseVertex = vertex % 65536;
    int index = (vertex / 4) * 6;
    static const unsigned short faceIndices[6] = {0, 1, 2, 1, 0, 3};
    for (int i = 0; i < 6; i++) indices[index + i] = (unsigned short)(baseVertex + faceIndices[i]);
    unsigned char color = FaceColor(meshTemplate->directions[face], sprite, light, sunlight);
    for (int i = 0; i < 4; i++) {
        vertices[(vertex + i)*3] = (unsigned char)(x*15 + source[i*3]*15/16);
        vertices[(vertex + i)*3 + 1] = (unsigned char)(y*15 + source[i*3 + 1]*15/16);
        vertices[(vertex + i)*3 + 2] = (unsigned char)(z*15 + source[i*3 + 2]*15/16);
        colors[vertex + i] = color;
    }
    memcpy(texcoords + vertex*2, meshTemplate->texcoords[face], 8 * sizeof(unsigned short));
}
