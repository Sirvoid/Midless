/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include "raylib.h"
#include "entityModelPart.h"

int vCount = 0;
int tCount = 0;
int cCount = 0;

void EntityModelPart_GetFacesPosition(BoundingBox BB, Vector3 *facesPosition) {
        
        Vector3 nFacesPosition[36] = {
            //left
            (Vector3) {BB.min.x, BB.min.y, BB.min.z}, (Vector3) {BB.min.x, BB.max.y, BB.max.z},
            (Vector3) {BB.min.x, BB.max.y, BB.min.z}, (Vector3) {BB.min.x, BB.max.y, BB.max.z},
            (Vector3) {BB.min.x, BB.min.y, BB.min.z}, (Vector3) {BB.min.x, BB.min.y, BB.max.z},
            //right
            (Vector3) {BB.max.x, BB.min.y, BB.max.z}, (Vector3) {BB.max.x, BB.max.y, BB.min.z},
            (Vector3) {BB.max.x, BB.max.y, BB.max.z}, (Vector3) {BB.max.x, BB.max.y, BB.min.z}, 
            (Vector3) {BB.max.x, BB.min.y, BB.max.z}, (Vector3) {BB.max.x, BB.min.y, BB.min.z},
            //top
            (Vector3) {BB.min.x, BB.max.y, BB.max.z}, (Vector3) {BB.max.x, BB.max.y, BB.min.z},
            (Vector3) {BB.min.x, BB.max.y, BB.min.z}, (Vector3) {BB.max.x, BB.max.y, BB.min.z},
            (Vector3) {BB.min.x, BB.max.y, BB.max.z}, (Vector3) {BB.max.x, BB.max.y, BB.max.z},
            //bottom
            (Vector3) {BB.min.x, BB.min.y, BB.min.z}, (Vector3) {BB.max.x, BB.min.y, BB.max.z},
            (Vector3) {BB.min.x, BB.min.y, BB.max.z}, (Vector3) {BB.max.x, BB.min.y, BB.max.z},
            (Vector3) {BB.min.x, BB.min.y, BB.min.z}, (Vector3) {BB.max.x, BB.min.y, BB.min.z},
            //front
            (Vector3) {BB.min.x, BB.min.y, BB.max.z}, (Vector3) {BB.max.x, BB.max.y, BB.max.z},
            (Vector3) {BB.min.x, BB.max.y, BB.max.z}, (Vector3) {BB.max.x, BB.max.y, BB.max.z},
            (Vector3) {BB.min.x, BB.min.y, BB.max.z}, (Vector3) {BB.max.x, BB.min.y, BB.max.z},
            //back
            (Vector3) {BB.max.x, BB.min.y, BB.min.z}, (Vector3) {BB.min.x, BB.max.y, BB.min.z},
            (Vector3) {BB.max.x, BB.max.y, BB.min.z}, (Vector3) {BB.min.x, BB.max.y, BB.min.z},
            (Vector3) {BB.max.x, BB.min.y, BB.min.z}, (Vector3) {BB.min.x, BB.min.y, BB.min.z}
        };

        memcpy(facesPosition, nFacesPosition, 36 * sizeof(Vector3));
    
}

void EntityModelPart_AddFace(Mesh *mesh, int face, BoundingBox box, Rectangle uvs, Vector2 textureSize) {
    Vector3 facesPosition[36] = {0};
    EntityModelPart_GetFacesPosition(box, facesPosition);

    int texI = 0;

    unsigned char lightning = 255;

    switch(face) {
        case 0: //left
        case 1: //right
            lightning = 150;
            break;
        case 2: //top
            lightning = 255;
            break;
        case 3: //bottom
            lightning = 100;
            break;
        case 4: //front
        case 5: //back
            lightning = 200;
            break;
        default:
            break;
    }

    for(int i = 0; i < 6; i++) {
        int faceIndex = i + face * 6;
        
        mesh->vertices[vCount++] =  facesPosition[faceIndex].x / 16;
        mesh->vertices[vCount++] =  facesPosition[faceIndex].y / 16;
        mesh->vertices[vCount++] =  facesPosition[faceIndex].z / 16;
        
        mesh->colors[cCount++] = lightning;
        mesh->colors[cCount++] = lightning;
        mesh->colors[cCount++] = lightning;
        mesh->colors[cCount++] = 255;

        float minX = uvs.x;
        float minY = uvs.y;
        float maxX = uvs.x + uvs.width;
        float maxY = uvs.y + uvs.height;

        float texCoords[12] = {
            maxX, maxY,  minX, minY,  maxX, minY,
            minX, minY,  maxX, maxY,  minX, maxY
        };

        mesh->texcoords[tCount++] = texCoords[texI++] / textureSize.x;
        mesh->texcoords[tCount++] = texCoords[texI++] / textureSize.y;
    }
}

void EntityModelPart_Build(EntityModelPart *part, BoundingBox box, Rectangle *uvs, Vector2 textureSize, Vector3 position) {
    Mesh *mesh = &part->mesh;

    int triangles = 12;

    mesh->vertexCount = triangles * 3;
    mesh->triangleCount = triangles;

    mesh->vertices = (float*)MemAlloc(mesh->vertexCount * sizeof(float) * 3);
    mesh->texcoords = (float*)MemAlloc(mesh->vertexCount * sizeof(float) * 2);
    mesh->colors = (unsigned char*)MemAlloc(mesh->vertexCount * 4);

    vCount = 0;
    tCount = 0;
    cCount = 0;
    for(int i = 0; i < 6; i++) {
        EntityModelPart_AddFace(mesh, i, box, uvs[i], textureSize);
    }

    UploadMesh(mesh, false);

    part->position = position;
    part->rotation = (Vector3) {0, 0, 0};
}