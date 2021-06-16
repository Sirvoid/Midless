#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include "rlgl.h"
#include "chunkmesh.h"

void ChunkMesh_Upload(ChunkMesh *mesh) {

    mesh->vboId = (unsigned int*)RL_CALLOC(MAX_CHUNKMESH_VERTEX_BUFFERS, sizeof(unsigned int));

    mesh->vaoId = 0;        
    mesh->vboId[0] = 0;
    mesh->vboId[1] = 0;
    mesh->vboId[3] = 0;

    mesh->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(mesh->vaoId);

    mesh->vboId[0] = rlLoadVertexBuffer(mesh->vertices, mesh->vertexCount * 3 * sizeof(float), false);
    rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(0);

    mesh->vboId[1] = rlLoadVertexBuffer(mesh->texcoords, mesh->vertexCount * 2 * sizeof(float), false);
    rlSetVertexAttribute(1, 2, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(1);

    mesh->vboId[3] = rlLoadVertexBuffer(mesh->colors, mesh->vertexCount * sizeof(unsigned char), false);
    rlSetVertexAttribute(3, 1, RL_UNSIGNED_BYTE, 1, 0, 0);
    rlEnableVertexAttribute(3);

    rlDisableVertexArray();
}

void ChunkMesh_Unload(ChunkMesh mesh) {
    
    rlUnloadVertexArray(mesh.vaoId);

    for (int i = 0; i < MAX_CHUNKMESH_VERTEX_BUFFERS; i++) rlUnloadVertexBuffer(mesh.vboId[i]);
    RL_FREE(mesh.vboId);

    RL_FREE(mesh.vertices);
    RL_FREE(mesh.texcoords);
    RL_FREE(mesh.colors);
}

void ChunkMesh_Draw(ChunkMesh mesh, Material material, Matrix transform) {

    rlEnableShader(material.shader.id);

    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = matView;
    Matrix matProjection = rlGetMatrixProjection();

    matModelView = MatrixMultiply(transform, MatrixMultiply(rlGetMatrixTransform(), matView));
    
    rlEnableTexture(material.maps[0].texture.id);

    if (!rlEnableVertexArray(mesh.vaoId)) {
        rlEnableVertexBuffer(mesh.vboId[0]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        rlEnableVertexBuffer(mesh.vboId[1]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1) {
            rlEnableVertexBuffer(mesh.vboId[3]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 1, RL_UNSIGNED_BYTE, 1, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
        }

    }

    Matrix matMVP = MatrixIdentity();
    matMVP = MatrixMultiply(matModelView, matProjection);

    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matMVP);

    rlDrawVertexArray(0, mesh.vertexCount);

    rlDisableTexture();

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    rlDisableShader();

    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}