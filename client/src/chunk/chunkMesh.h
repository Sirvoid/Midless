/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_CHUNKMESH_H
#define G_CHUNKMESH_H

#define MAX_CHUNKMESH_VERTEX_BUFFERS 4

typedef struct ChunkMesh {
    int vertexCount; 
    int drawVertexCount;
    int drawTriangleCount;
    int triangleCount;

    unsigned int vaoId;  
    unsigned int *vboId;
} ChunkMesh;

void ChunkMesh_Upload(ChunkMesh *mesh, unsigned char *vertices, unsigned short *indices, unsigned short *texcoords, unsigned char *colors);
void ChunkMesh_Unload(ChunkMesh *mesh);
void ChunkMesh_PrepareDrawing(Material mat);
void ChunkMesh_FinishDrawing(void);
void ChunkMesh_Draw(ChunkMesh *mesh, Material material, Matrix transform);

#endif