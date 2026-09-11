/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#if defined(PLATFORM_DESKTOP)
#include "external/glad.h"
#endif
#include "chunkmesh.h"
#include "world.h"
#include "player.h"

static unsigned int cachedShaderId = 0;
static int matModelViewLocation = -1;

void ChunkMesh_Upload(ChunkMesh *mesh, unsigned char *vertices, unsigned short *indices, unsigned short *texcoords, unsigned char *colors) {
    mesh->drawVertexCount = mesh->vertexCount;
    mesh->drawTriangleCount = mesh->triangleCount;

    int requiredVertices = mesh->vertexCount;
    int requiredIndices = mesh->triangleCount * 3;
    bool canReuse = mesh->vboId != NULL && requiredVertices <= mesh->vertexCapacity && requiredIndices <= mesh->indexCapacity;

    if (canReuse) {
        rlUpdateVertexBuffer(mesh->vboId[0], vertices, requiredVertices * 3 * sizeof(unsigned char), 0);
        rlUpdateVertexBuffer(mesh->vboId[1], texcoords, requiredVertices * 2 * sizeof(unsigned short), 0);
        rlUpdateVertexBuffer(mesh->vboId[2], colors, requiredVertices * sizeof(unsigned char), 0);
        rlUpdateVertexBuffer(mesh->vboId[3], indices, requiredIndices * sizeof(unsigned short), 0);
        return;
    }

    if (mesh->vboId != NULL) ChunkMesh_Unload(mesh);

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
    int positionLocation = world.material.shader.locs[SHADER_LOC_VERTEX_POSITION];
    rlSetVertexAttribute(positionLocation, 3, RL_UNSIGNED_BYTE, 0, 0, 0);
    rlEnableVertexAttribute(positionLocation);

    mesh->vboId[1] = rlLoadVertexBuffer(texcoords, vertXShort * 2, false);
    int texcoordLocation = world.material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01];
    rlSetVertexAttribute(texcoordLocation, 2, 0x1403, 0, 0, 0);
    rlEnableVertexAttribute(texcoordLocation);

    mesh->vboId[2] = rlLoadVertexBuffer(colors, vertXchar, false);
    int colorLocation = world.material.shader.locs[SHADER_LOC_VERTEX_COLOR];
    rlSetVertexAttribute(colorLocation, 1, RL_UNSIGNED_BYTE, 0, 0, 0);
    rlEnableVertexAttribute(colorLocation);

    mesh->vboId[3] = rlLoadVertexBufferElement(indices, mesh->drawTriangleCount*3*sizeof(unsigned short), false);

    mesh->vertexCapacity = requiredVertices;
    mesh->indexCapacity = requiredIndices;

    rlDisableVertexArray();
}

void ChunkMesh_Clear(ChunkMesh *mesh) {
    mesh->vertexCount = 0;
    mesh->triangleCount = 0;
    mesh->drawVertexCount = 0;
    mesh->drawTriangleCount = 0;
}

void ChunkMesh_Unload(ChunkMesh *mesh) {
    if (mesh->vboId == NULL) return;
    rlUnloadVertexArray(mesh->vaoId);
    for (int i = 0; i < MAX_CHUNKMESH_VERTEX_BUFFERS; i++) rlUnloadVertexBuffer(mesh->vboId[i]);

    RL_FREE(mesh->vboId);
    mesh->vboId = NULL;
    mesh->vaoId = 0;
    mesh->vertexCapacity = 0;
    mesh->indexCapacity = 0;
}

void ChunkMesh_UnloadBatch(ChunkMesh **meshes, int count) {
#if defined(PLATFORM_DESKTOP)
    if (count <= 32 && glDeleteVertexArrays && glDeleteBuffers) {
        unsigned int arrays[32], buffers[32 * MAX_CHUNKMESH_VERTEX_BUFFERS];
        int arrayCount = 0, bufferCount = 0;
        for (int i = 0; i < count; i++) {
            ChunkMesh *mesh = meshes[i];
            if (!mesh->vboId) continue;
            if (mesh->vaoId) arrays[arrayCount++] = mesh->vaoId;
            for (int j = 0; j < MAX_CHUNKMESH_VERTEX_BUFFERS; j++) {
                if (mesh->vboId[j]) buffers[bufferCount++] = mesh->vboId[j];
            }
        }
        // Drop references first, then release the buffers in one driver call.
        rlDisableVertexArray();
        if (arrayCount) glDeleteVertexArrays(arrayCount, arrays);
        if (bufferCount) glDeleteBuffers(bufferCount, buffers);
        for (int i = 0; i < count; i++) {
            ChunkMesh *mesh = meshes[i];
            RL_FREE(mesh->vboId);
            mesh->vboId = NULL;
            mesh->vaoId = 0;
            mesh->vertexCapacity = mesh->indexCapacity = 0;
        }
        return;
    }
#endif
    for (int i = 0; i < count; i++) ChunkMesh_Unload(meshes[i]);
}

void ChunkMesh_PrepareDrawing(Material material) {
    rlEnableShader(material.shader.id);
    rlEnableTexture(material.maps[0].texture.id);

    if (cachedShaderId != material.shader.id) {
        cachedShaderId = material.shader.id;
        matModelViewLocation = rlGetLocationUniform(material.shader.id, "matModelView");
    }

    float sunlightStrength = World_GetSunlightStrength();
    rlSetUniform(rlGetLocationUniform(material.shader.id, "sunlightStrength"), &sunlightStrength, RL_SHADER_UNIFORM_FLOAT, 1);

    float fogEnd = world.drawDistance * 16.0f + 8.0f;
    float fogStart = world.drawDistance * 16.0f * 0.7f + 8.0f;
    float fogColor[3] = {
        (140.0f / 255.0f) * sunlightStrength,
        (210.0f / 255.0f) * sunlightStrength,
        (240.0f / 255.0f) * sunlightStrength
    };
    Color liquidTint;
    if (Player_GetCameraLiquidTint(&liquidTint)) {
        fogStart = 10.0f;
        fogEnd = 32.0f;
        fogColor[0] = liquidTint.r / 255.0f;
        fogColor[1] = liquidTint.g / 255.0f;
        fogColor[2] = liquidTint.b / 255.0f;
    }
    rlSetUniform(rlGetLocationUniform(material.shader.id, "fogColor"), fogColor, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetUniform(rlGetLocationUniform(material.shader.id, "fogStart"), &fogStart, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(material.shader.id, "fogEnd"), &fogEnd, RL_SHADER_UNIFORM_FLOAT, 1);
}

void ChunkMesh_FinishDrawing(void) {
    rlDisableShader();
    rlDisableTexture();
}

void ChunkMesh_Draw(ChunkMesh *mesh, Material material, Matrix transform) {

    if (mesh->drawTriangleCount == 0 || mesh->vboId == NULL) return;

    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = matView;
    Matrix matProjection = rlGetMatrixProjection();

    matModelView = MatrixMultiply(transform, MatrixMultiply(rlGetMatrixTransform(), matView));

    rlEnableVertexArray(mesh->vaoId);

    Matrix matMVP = MatrixIdentity();
    matMVP = MatrixMultiply(matModelView, matProjection);

    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matMVP);
    rlSetUniformMatrix(matModelViewLocation, matModelView);

    for (int first = 0; first < mesh->drawVertexCount; first += 65536) {
        int count = mesh->drawVertexCount - first;
        if (count > 65536) count = 65536;
        rlEnableVertexBuffer(mesh->vboId[0]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_UNSIGNED_BYTE,
                             0, 0, (void *)(uintptr_t)(first * 3));
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);
        rlEnableVertexBuffer(mesh->vboId[1]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, 0x1403,
                             0, 0, (void *)(uintptr_t)(first * 2 * sizeof(unsigned short)));
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);
        rlEnableVertexBuffer(mesh->vboId[2]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 1, RL_UNSIGNED_BYTE,
                             0, 0, (void *)(uintptr_t)first);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
        rlEnableVertexBufferElement(mesh->vboId[3]);
        rlDrawVertexArrayElements((first / 4) * 6, (count / 4) * 6, 0);
    }

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}
