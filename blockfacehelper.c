#include "raylib.h"
#include "blockfacehelper.h"
#include "block.h"
#include "math.h"

int BFH_verticesI = 0;
int BFH_normalsI = 0;
int BFH_texI = 0;
int BFH_colorsI = 0;

float texCoords[12] = {
    1, 1,  0, 0,  1, 0,
    0, 0,  1, 1,  0, 1
    
};

Vector3 BFH_facesPosition[36] = {
    //left
    (Vector3) {0, 0, 0}, (Vector3) {0, 1, 1},
    (Vector3) {0, 1, 0}, (Vector3) {0, 1, 1},
    (Vector3) {0, 0, 0}, (Vector3) {0, 0, 1},
    //right
    (Vector3) {1, 0, 1}, (Vector3) {1, 1, 0},
    (Vector3) {1, 1, 1}, (Vector3) {1, 1, 0}, 
    (Vector3) {1, 0, 1}, (Vector3) {1, 0, 0},
    //top
    (Vector3) {0, 1, 1}, (Vector3) {1, 1, 0},
    (Vector3) {0, 1, 0}, (Vector3) {1, 1, 0},
    (Vector3) {0, 1, 1}, (Vector3) {1, 1, 1},
    //bottom
    (Vector3) {0, 0, 0}, (Vector3) {1, 0, 1},
    (Vector3) {0, 0, 1}, (Vector3) {1, 0, 1},
    (Vector3) {0, 0, 0}, (Vector3) {1, 0, 0},
    //front
    (Vector3) {0, 0, 1}, (Vector3) {1, 1, 1},
    (Vector3) {0, 1, 1}, (Vector3) {1, 1, 1},
    (Vector3) {0, 0, 1}, (Vector3) {1, 0, 1},
    //back
    (Vector3) {1, 0, 0}, (Vector3) {0, 1, 0},
    (Vector3) {1, 1, 0}, (Vector3) {0, 1, 0},
    (Vector3) {1, 0, 0}, (Vector3) {0, 0, 0}
};

Vector3 BFH_GetDirection(BlockFace face) {
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

void BFH_ResetIndexes() {
    BFH_verticesI = 0;
    BFH_normalsI = 0;
    BFH_texI = 0;
    BFH_colorsI = 0;
}

void BFH_AddFace(Mesh *mesh, BlockFace face, Vector3 pos, int blockID) {
    
    int texID = Block_definition[blockID].textures[(int)face];
    
    int texI = 0;
    int textureX = texID % 16;
    int textureY = texID / 16;
    
    for(int i = 0; i < 6; i++) {
        int faceIndex = i + ((int)face * 6);
        mesh->vertices[BFH_verticesI++] =  BFH_facesPosition[faceIndex].x + pos.x;
        mesh->vertices[BFH_verticesI++] =  BFH_facesPosition[faceIndex].y + pos.y;
        mesh->vertices[BFH_verticesI++] =  BFH_facesPosition[faceIndex].z + pos.z;
        
        mesh->normals[BFH_normalsI++] = 0;
        mesh->normals[BFH_normalsI++] = 1;
        mesh->normals[BFH_normalsI++] = 0;
        
        switch(face) {
            case BlockFace_Bottom:
                mesh->colors[BFH_colorsI++] = 100;
                mesh->colors[BFH_colorsI++] = 100;
                mesh->colors[BFH_colorsI++] = 100;
                mesh->colors[BFH_colorsI++] = 255;
                break;
            case BlockFace_Left:
            case BlockFace_Right:
                mesh->colors[BFH_colorsI++] = 150;
                mesh->colors[BFH_colorsI++] = 150;
                mesh->colors[BFH_colorsI++] = 150;
                mesh->colors[BFH_colorsI++] = 255;
                break;
            case BlockFace_Front:
            case BlockFace_Back:
                mesh->colors[BFH_colorsI++] = 200;
                mesh->colors[BFH_colorsI++] = 200;
                mesh->colors[BFH_colorsI++] = 200;
                mesh->colors[BFH_colorsI++] = 255;
                break;
            default:
                mesh->colors[BFH_colorsI++] = 255;
                mesh->colors[BFH_colorsI++] = 255;
                mesh->colors[BFH_colorsI++] = 255;
                mesh->colors[BFH_colorsI++] = 255;
                break;
        }
    }
    
    for(int i = 0; i < 6; i++) {
        mesh->texcoords[BFH_texI++] = (texCoords[texI++] + textureX) / 16.0f;
        mesh->texcoords[BFH_texI++] = (texCoords[texI++] + textureY) / 16.0f;
    }
}