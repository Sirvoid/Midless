/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "chunkmesh.h"
#include "world.h"

void ChunkMesh_Upload(ChunkMesh *mesh, unsigned char *vertices, unsigned short *indices, unsigned short *texcoords, unsigned char *colors) {

    mesh->drawVertexCount = mesh->vertexCount;
    mesh->drawTriangleCount = mesh->triangleCount;

    mesh->vboId = (unsigned int*)RL_CALLOC(MAX_CHUNKMESH_VERTEX_BUFFERS, sizeof(unsigned int));

    mesh->vaoId = 0;        
    mesh->vboId[0] = 0;
    mesh->vboId[1] = 0;
    mesh->vboId[2] = 0;
    mesh->vboId[3] = 0;

    mesh->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(mesh->vaoId);

    int vertXchar = mesh->vertexCount * sizeof(unsigned char);
    int vertXShort = mesh->vertexCount * sizeof(unsigned short);

    mesh->vboId[0] = rlLoadVertexBuffer(vertices, vertXchar * 3, false);
    rlSetVertexAttribute(0, 3, RL_UNSIGNED_BYTE, 0, 0, 0);
    rlEnableVertexAttribute(0);

    mesh->vboId[1] = rlLoadVertexBuffer(texcoords, vertXShort * 2, false);
    rlSetVertexAttribute(1, 2, 0x1403, 0, 0, 0);
    rlEnableVertexAttribute(1);

    mesh->vboId[2] = rlLoadVertexBuffer(colors, vertXchar, false);
    rlSetVertexAttribute(3, 1, RL_UNSIGNED_BYTE, 0, 0, 0);
    rlEnableVertexAttribute(3);

    mesh->vboId[3] = rlLoadVertexBufferElement(indices, mesh->drawTriangleCount*3*sizeof(unsigned short), false);

    rlDisableVertexArray();
}

void ChunkMesh_Unload(ChunkMesh *mesh) {
    
    rlUnloadVertexArray(mesh->vaoId);
    for (int i = 0; i < MAX_CHUNKMESH_VERTEX_BUFFERS; i++) rlUnloadVertexBuffer(mesh->vboId[i]);
    
    RL_FREE(mesh->vboId);
    
}

void ChunkMesh_PrepareDrawing(Material mat) {
    rlEnableShader(mat.shader.id);
    rlEnableTexture(mat.maps[0].texture.id);

    float drawDistance = (world.drawDistance + 2) * 16.0f;
    rlSetUniform(rlGetLocationUniform(mat.shader.id, "drawDistance"), &drawDistance, RL_SHADER_UNIFORM_FLOAT, 1);
}

void ChunkMesh_FinishDrawing(void) {
    rlDisableShader();
    rlDisableTexture();
}

void ChunkMesh_Draw(ChunkMesh *mesh, Material material, Matrix transform) {

    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = matView;
    Matrix matProjection = rlGetMatrixProjection();

    matModelView = MatrixMultiply(transform, MatrixMultiply(rlGetMatrixTransform(), matView));
    
    if (!rlEnableVertexArray(mesh->vaoId)) {
        rlEnableVertexBuffer(mesh->vboId[0]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_UNSIGNED_BYTE, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        rlEnableVertexBuffer(mesh->vboId[1]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, 0x1403, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        rlEnableVertexBuffer(mesh->vboId[2]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 1, RL_UNSIGNED_BYTE, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);

        rlEnableVertexBufferElement(mesh->vboId[3]);
    }

    Matrix matMVP = MatrixIdentity();
    matMVP = MatrixMultiply(matModelView, matProjection);

    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matMVP);
    
    rlDrawVertexArrayElements(0, mesh->drawTriangleCount * 3, 0);

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}