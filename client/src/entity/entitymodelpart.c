/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <math.h>
#include "raylib.h"
#include "entitymodelpart.h"

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

    // The original UV orientation maps U from vertex 2 to 1, V from 2 to 0.
    Vector3 origin = facesPosition[face * 6 + 2];
    Vector3 uEnd = facesPosition[face * 6 + 1];
    Vector3 vEnd = facesPosition[face * 6];
    float width = fabsf(uEnd.x-origin.x) + fabsf(uEnd.y-origin.y) + fabsf(uEnd.z-origin.z);
    float height = fabsf(vEnd.x-origin.x) + fabsf(vEnd.y-origin.y) + fabsf(vEnd.z-origin.z);
    float scale = fminf(width / fabsf(uvs.width), height / fabsf(uvs.height));
    float repeatU = width / (fabsf(uvs.width) * scale);
    float repeatV = height / (fabsf(uvs.height) * scale);

    int texI = 0;

    unsigned char lightning = 255;

    switch (face) {
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

    for (int i = 0; i < 6; i++) {
        int faceIndex = i + face * 6;
        
        mesh->vertices[vCount++] =  facesPosition[faceIndex].x / 16;
        mesh->vertices[vCount++] =  facesPosition[faceIndex].y / 16;
        mesh->vertices[vCount++] =  facesPosition[faceIndex].z / 16;
        
        mesh->colors[cCount++] = lightning;
        mesh->colors[cCount++] = lightning;
        mesh->colors[cCount++] = lightning;
        mesh->colors[cCount++] = 255;

        float minX = 0;
        float minY = 0;
        float maxX = repeatU;
        float maxY = repeatV;

        float texCoords[12] = {
            maxX, maxY,  minX, minY,  maxX, minY,
            minX, minY,  maxX, maxY,  minX, maxY
        };

        // Tangents carry the signed atlas rectangle for the entity shader.
        int rectIndex = (vCount / 3 - 1) * 4;
        mesh->tangents[rectIndex] = uvs.x / textureSize.x;
        mesh->tangents[rectIndex + 1] = uvs.y / textureSize.y;
        mesh->tangents[rectIndex + 2] = uvs.width / textureSize.x;
        mesh->tangents[rectIndex + 3] = uvs.height / textureSize.y;
        mesh->texcoords[tCount++] = texCoords[texI++];
        mesh->texcoords[tCount++] = texCoords[texI++];
    }
}

void EntityModelPart_Build(EntityModelPart *part, BoundingBox box, Rectangle *uvs, Vector2 textureSize, Vector3 position) {
    part->mesh = (Mesh) {0};
    Mesh *mesh = &part->mesh;

    int triangles = 12;

    mesh->vertexCount = triangles * 3;
    mesh->triangleCount = triangles;

    mesh->vertices = (float*)MemAlloc(mesh->vertexCount * sizeof(float) * 3);
    mesh->texcoords = (float*)MemAlloc(mesh->vertexCount * sizeof(float) * 2);
    mesh->tangents = (float*)MemAlloc(mesh->vertexCount * sizeof(float) * 4);
    mesh->colors = (unsigned char*)MemAlloc(mesh->vertexCount * 4);

    vCount = 0;
    tCount = 0;
    cCount = 0;
    for (int i = 0; i < 6; i++) {
        EntityModelPart_AddFace(mesh, i, box, uvs[i], textureSize);
    }

    UploadMesh(mesh, false);

    part->grip = (Vector3){(box.min.x + box.max.x) / 32.0f, box.min.y / 16.0f, (box.min.z + box.max.z) / 32.0f};
    part->position = position;
    part->rotation = (Vector3) {0, 0, 0};
}
