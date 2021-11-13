#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "block.h"
#include "chunkmesh.h"
#include "blockfacehelper.h"

int BFH_verticesI[] = {0, 0};
int BFH_texI[] = {0, 0};
int BFH_colorsI[] = {0, 0};

float BFH_texCoords[12] = {
    1, 1,  0, 0,  1, 0,
    0, 0,  1, 1,  0, 1
    
};

void BFH_GetFacesPosition(Block b, Vector3 *facesPosition) {
    
    if(b.modelType == BlockModelType_Sprite) {

        Vector3 nFacesPosition[24] = {
            //left
            (Vector3) {0, 0, 0}, (Vector3) {1, 1, 1},
            (Vector3) {0, 1, 0}, (Vector3) {1, 1, 1},
            (Vector3) {0, 0, 0}, (Vector3) {1, 0, 1},
            //right
            (Vector3) {1, 0, 1}, (Vector3) {0, 1, 0},
            (Vector3) {1, 1, 1}, (Vector3) {0, 1, 0}, 
            (Vector3) {1, 0, 1}, (Vector3) {0, 0, 0},
            //front
            (Vector3) {0, 0, 1}, (Vector3) {1, 1, 0},
            (Vector3) {0, 1, 1}, (Vector3) {1, 1, 0},
            (Vector3) {0, 0, 1}, (Vector3) {1, 0, 0},
            //back
            (Vector3) {1, 0, 0}, (Vector3) {0, 1, 1},
            (Vector3) {1, 1, 0}, (Vector3) {0, 1, 1},
            (Vector3) {1, 0, 0}, (Vector3) {0, 0, 1}
        };

        memcpy(facesPosition, nFacesPosition, 24 * sizeof(Vector3));

    } else {

        Vector3 nFacesPosition[36] = {
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

        memcpy(facesPosition, nFacesPosition, 36 * sizeof(Vector3));
        
    }
    
}

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

void BFH_ResetIndexes(void) {
    BFH_verticesI[0] = 0;
    BFH_texI[0] = 0;
    BFH_colorsI[0] = 0;
    
    BFH_verticesI[1] = 0;
    BFH_texI[1] = 0;
    BFH_colorsI[1] = 0;
}

void BFH_AddFace(ChunkMesh *mesh, BlockFace face, Vector3 pos, Block blockDef, int translucent, int light) {
    
    Vector3 facesPosition[36] = {0};
    BFH_GetFacesPosition(blockDef, facesPosition);
    int texID = blockDef.textures[(int)face];
    
    int texI = 0;
    int textureX = texID % 16;
    int textureY = texID / 16;
    
    int faceX6 = ((int)face * 6);
    
    for(int i = 0; i < 6; i++) {
        int faceIndex = i + faceX6;
        
        mesh->vertices[BFH_verticesI[translucent]++] =  (unsigned char)((facesPosition[faceIndex].x + pos.x) * 15.99f);
        mesh->vertices[BFH_verticesI[translucent]++] =  (unsigned char)((facesPosition[faceIndex].y + pos.y) * 15.99f);
        mesh->vertices[BFH_verticesI[translucent]++] =  (unsigned char)((facesPosition[faceIndex].z + pos.z) * 15.99f);
        
        if(blockDef.modelType != BlockModelType_Sprite) {
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

