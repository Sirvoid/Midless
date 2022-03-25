/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include "block.h"
#include "../chunk/chunkMesh.h"
#include "blockMeshGeneration.h"

int BFH_verticesI[] = {0, 0};
int BFH_texI[] = {0, 0};
int BFH_colorsI[] = {0, 0};
int BFH_indicesI[] = {0, 0};

float BFH_texCoords[12] = {
    1, 1,  0, 0,  1, 0,
    0, 1,  1, 1,  0, 0
};

Vector3 BlockMesh_GetDirection(BlockFace face) {
    switch (face) {
        case BlockFace_Left:
            return (Vector3){ -1, 0, 0 };
        case BlockFace_Right:
            return (Vector3){ 1, 0, 0 };
        case BlockFace_Bottom:
            return (Vector3){ 0, -1, 0 };
        case BlockFace_Top:
            return (Vector3){ 0, 1, 0 };
        case BlockFace_Back:
            return (Vector3){ 0, 0, -1 };
        case BlockFace_Front:
            return (Vector3){ 0, 0, 1 };
    }
    return (Vector3){ 0, 0, 0 };
}

void BlockMesh_ResetIndexes(void) {
    BFH_verticesI[0] = 0;
    BFH_texI[0] = 0;
    BFH_colorsI[0] = 0;
    BFH_indicesI[0] = 0;
    
    BFH_verticesI[1] = 0;
    BFH_texI[1] = 0;
    BFH_colorsI[1] = 0;
    BFH_indicesI[1] = 0;
}

void BlockMesh_AddFace(ChunkMesh *mesh, BlockFace face, Vector3 pos, Block b, int translucent, int light) {
    
    Vector3 *facesPosition;

    Vector3 facesPositionBlock[24] = {
        //left
        (Vector3) {b.minBB.x, b.minBB.y, b.minBB.z}, (Vector3) {b.minBB.x, b.maxBB.y, b.maxBB.z},
        (Vector3) {b.minBB.x, b.maxBB.y, b.minBB.z}, (Vector3) {b.minBB.x, b.minBB.y, b.maxBB.z},
        //right
        (Vector3) {b.maxBB.x, b.minBB.y, b.maxBB.z}, (Vector3) {b.maxBB.x, b.maxBB.y, b.minBB.z},
        (Vector3) {b.maxBB.x, b.maxBB.y, b.maxBB.z}, (Vector3) {b.maxBB.x, b.minBB.y, b.minBB.z},
        //top
        (Vector3) {b.minBB.x, b.maxBB.y, b.maxBB.z}, (Vector3) {b.maxBB.x, b.maxBB.y, b.minBB.z},
        (Vector3) {b.minBB.x, b.maxBB.y, b.minBB.z}, (Vector3) {b.maxBB.x, b.maxBB.y, b.maxBB.z},
        //bottom
        (Vector3) {b.minBB.x, b.minBB.y, b.minBB.z}, (Vector3) {b.maxBB.x, b.minBB.y, b.maxBB.z},
        (Vector3) {b.minBB.x, b.minBB.y, b.maxBB.z}, (Vector3) {b.maxBB.x, b.minBB.y, b.minBB.z},
        //front
        (Vector3) {b.minBB.x, b.minBB.y, b.maxBB.z}, (Vector3) {b.maxBB.x, b.maxBB.y, b.maxBB.z},
        (Vector3) {b.minBB.x, b.maxBB.y, b.maxBB.z}, (Vector3) {b.maxBB.x, b.minBB.y, b.maxBB.z},
        //back
        (Vector3) {b.maxBB.x, b.minBB.y, b.minBB.z}, (Vector3) {b.minBB.x, b.maxBB.y, b.minBB.z},
        (Vector3) {b.maxBB.x, b.maxBB.y, b.minBB.z}, (Vector3) {b.minBB.x, b.minBB.y, b.minBB.z}
    };

    Vector3 facesPositionSprite[16] = {
        //left
        (Vector3) {0, 0, 0}, (Vector3) {16, 16, 16},
        (Vector3) {0, 16, 0}, (Vector3) {16, 0, 16},
        //right
        (Vector3) {16, 0, 16}, (Vector3) {0, 16, 0},
        (Vector3) {16, 16, 16}, (Vector3) {0, 0, 0},
        //front
        (Vector3) {0, 0, 16}, (Vector3) {16, 16, 0},
        (Vector3) {0, 16, 16}, (Vector3) {16, 0, 0},
        //back
        (Vector3) {16, 0, 0}, (Vector3) {0, 16, 16},
        (Vector3) {16, 16, 0}, (Vector3) {0, 0, 16}
    };


    if(b.modelType == BlockModelType_Sprite) {
        facesPosition = facesPositionSprite;
    } else {
        facesPosition = facesPositionBlock;
    }

    int texID = b.textures[(int)face];
    
    int texI = 0;
    int textureX = texID % 16;
    int textureY = texID / 16;
    
    int faceX4 = ((int)face * 4);
    
    int verticeIndexD3 =  BFH_verticesI[translucent] / 3;

    mesh->indices[BFH_indicesI[translucent]++] = verticeIndexD3;
    mesh->indices[BFH_indicesI[translucent]++] = verticeIndexD3 + 1;
    mesh->indices[BFH_indicesI[translucent]++] = verticeIndexD3 + 2;
    mesh->indices[BFH_indicesI[translucent]++] = verticeIndexD3 + 1;
    mesh->indices[BFH_indicesI[translucent]++] = verticeIndexD3;
    mesh->indices[BFH_indicesI[translucent]++] = verticeIndexD3 + 3;

    for(int i = 0; i < 4; i++) {
        int faceIndex = i + faceX4;
        
        mesh->vertices[BFH_verticesI[translucent]++] =  (unsigned char)((facesPosition[faceIndex].x / 16 + pos.x) * 15);
        mesh->vertices[BFH_verticesI[translucent]++] =  (unsigned char)((facesPosition[faceIndex].y / 16 + pos.y) * 15);
        mesh->vertices[BFH_verticesI[translucent]++] =  (unsigned char)((facesPosition[faceIndex].z / 16 + pos.z) * 15);
        
        if(b.modelType != BlockModelType_Sprite) {
            switch(face) {
                case BlockFace_Bottom:
                    mesh->colors[BFH_colorsI[translucent]++] = fmin(100, fmax(16, 100 - light));
                    break;
                case BlockFace_Left:
                case BlockFace_Right:
                    mesh->colors[BFH_colorsI[translucent]++] = fmin(150, fmax(16, 150 - light));
                    break;
                case BlockFace_Front:
                case BlockFace_Back:
                    mesh->colors[BFH_colorsI[translucent]++] = fmin(200, fmax(16, 200 - light));
                    break;
                default:
                    mesh->colors[BFH_colorsI[translucent]++] = fmin(255, fmax(16, 255 - light));
                    break;
            }
        } else {
            mesh->colors[BFH_colorsI[translucent]++] = fmax(16, 255 - light);
        }
        
        mesh->texcoords[BFH_texI[translucent]++] = (unsigned char)((BFH_texCoords[texI++] + textureX));
        mesh->texcoords[BFH_texI[translucent]++] = (unsigned char)((BFH_texCoords[texI++] + textureY));
    }


}

