#ifndef G_CHUNKMESH_H
#define G_CHUNKMESH_H

#define MAX_CHUNKMESH_VERTEX_BUFFERS 4

typedef struct ChunkMesh {
    int vertexCount; 
    int triangleCount;

    float *vertices;
    float *texcoords;
    unsigned char *colors;

    unsigned int vaoId;  
    unsigned int *vboId;
} ChunkMesh;

void ChunkMesh_Upload(ChunkMesh *mesh);
void ChunkMesh_Unload(ChunkMesh mesh);
void ChunkMesh_Draw(ChunkMesh mesh, Material material, Matrix transform);

#endif