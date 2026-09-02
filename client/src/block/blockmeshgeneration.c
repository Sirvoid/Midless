/**
 * Copyright (c) 2021-2022 Sirvoid
 * This software is released under the MIT License.
 */

#include <string.h>
#include "block.h"
#include "blockmeshgeneration.h"

typedef struct BlockMeshTemplate {
    unsigned char vertices[6][12];
    unsigned short texcoords[6][8];
} BlockMeshTemplate;

static BlockMeshTemplate templates[256];
static int verticesIndex[2], texIndex[2], colorsIndex[2], indicesIndex[2];

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

void BlockMesh_BuildTemplates(void) {
    for (int id = 0; id < 256; id++) {
        const Block *block = &Block_definition[id];
        BlockMeshTemplate *out = &templates[id];
        if (block->modelType == BlockModelType_Sprite) memcpy(out->vertices, spriteVertices, sizeof(spriteVertices));
        else BuildSolidVertices(block, out);

        int faceCount = block->modelType == BlockModelType_Sprite ? 4 : 6;
        for (int face = 0; face < faceCount; face++) {
            int textureX = (block->textures[face] % 16) * 16;
            int textureY = (block->textures[face] / 16) * 16;
            int minX = textureX, minY = textureY, maxX = textureX + 16, maxY = textureY + 16;
            if (block->modelType != BlockModelType_Sprite) {
                if (face == BlockFace_Front || face == BlockFace_Back) {
                    maxY -= 16 - (int)block->maxBB.y; minY += (int)block->minBB.y;
                    maxX -= 16 - (int)block->maxBB.x; minX += (int)block->minBB.x;
                } else if (face == BlockFace_Left || face == BlockFace_Right) {
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
}

Vector3 BlockMesh_GetDirection(BlockFace face) {
    static const Vector3 directions[6] = {{-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    return directions[(int)face];
}

void BlockMesh_ResetIndexes(void) {
    memset(verticesIndex, 0, sizeof(verticesIndex));
    memset(texIndex, 0, sizeof(texIndex));
    memset(colorsIndex, 0, sizeof(colorsIndex));
    memset(indicesIndex, 0, sizeof(indicesIndex));
}

static unsigned char FaceColor(BlockFace face, bool sprite, int light, int sunlight) {
    int shade = 0;
    if (!sprite) {
        if (face == BlockFace_Bottom) shade = 8;
        else if (face == BlockFace_Left || face == BlockFace_Right) shade = 5;
        else if (face == BlockFace_Front || face == BlockFace_Back) shade = 3;
    }
    light -= shade; sunlight -= shade;
    if (light < 0) light = 0;
    if (sunlight < 0) sunlight = 0;
    return (unsigned char)((light << 4) | sunlight);
}

void BlockMesh_AddFace(unsigned char *vertices, unsigned short *indices, unsigned short *texcoords,
                       unsigned char *colors, BlockFace face, int x, int y, int z,
                       const Block *block, int translucent, int light, int sunlight) {
    const BlockMeshTemplate *meshTemplate = &templates[block - Block_definition];
    const unsigned char *source = meshTemplate->vertices[(int)face];
    int baseVertex = verticesIndex[translucent] / 3;
    static const unsigned short faceIndices[6] = {0, 1, 2, 1, 0, 3};
    for (int i = 0; i < 6; i++) indices[indicesIndex[translucent]++] = (unsigned short)(baseVertex + faceIndices[i]);

    unsigned char color = FaceColor(face, block->modelType == BlockModelType_Sprite, light, sunlight);
    int offsetX = x * 15, offsetY = y * 15, offsetZ = z * 15;
    for (int i = 0; i < 4; i++) {
        vertices[verticesIndex[translucent]++] = (unsigned char)(offsetX + source[i*3] * 15 / 16);
        vertices[verticesIndex[translucent]++] = (unsigned char)(offsetY + source[i*3+1] * 15 / 16);
        vertices[verticesIndex[translucent]++] = (unsigned char)(offsetZ + source[i*3+2] * 15 / 16);
        colors[colorsIndex[translucent]++] = color;
    }
    memcpy(&texcoords[texIndex[translucent]], meshTemplate->texcoords[(int)face], 8 * sizeof(unsigned short));
    texIndex[translucent] += 8;
}
