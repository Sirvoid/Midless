/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 */

#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "block.h"
#include "blockmeshgeneration.h"
#include "blockitemrenderer.h"

#define BLOCK_ITEM_COUNT 256
#define BLOCK_ITEM_TEXTURE_SIZE 256
#define BLOCK_ATLAS_SIZE 256.0f

typedef struct BlockItemIcon {
    RenderTexture2D target;
    bool loaded;
} BlockItemIcon;

static BlockItemIcon icons[BLOCK_ITEM_COUNT];
static Texture2D iconTerrain;

static Color FaceColor(int face, bool sprite) {
    if (sprite) return WHITE;
    if (face == BLOCK_FACE_TOP) return WHITE;
    if (face == BLOCK_FACE_BOTTOM) return (Color){125, 125, 125, 255};
    if (face == BLOCK_FACE_LEFT || face == BLOCK_FACE_RIGHT) return (Color){180, 180, 180, 255};
    return (Color){215, 215, 215, 255};
}

static Mesh BuildMesh(int blockId, const Block *block) {
    const bool sprite = block->modelType == BLOCK_MODEL_SPRITE;
    const int faceCount = sprite ? 4 : 6;
    static const int triangleCorners[6] = {0, 1, 2, 1, 0, 3};
    const BlockMeshTemplate *meshTemplate = BlockMesh_GetTemplate(blockId);
    Mesh mesh = {0};
    mesh.vertexCount = faceCount * 6;
    mesh.triangleCount = faceCount * 2;
    mesh.vertices = MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    mesh.colors = MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));

    Vector3 center = sprite
        ? (Vector3){8.0f, 8.0f, 8.0f}
        : Vector3Scale(Vector3Add(block->minBB, block->maxBB), 0.5f);

    for (int face = 0; face < faceCount; face++) {
        const unsigned char *source = meshTemplate->vertices[face];
        const unsigned short *uvs = meshTemplate->texcoords[face];
        Color color = FaceColor(face, sprite);

        for (int vertex = 0; vertex < 6; vertex++) {
            int corner = triangleCorners[vertex];
            int output = face * 6 + vertex;
            mesh.vertices[output * 3] = (source[corner * 3] - center.x) / 16.0f;
            mesh.vertices[output * 3 + 1] = (source[corner * 3 + 1] - center.y) / 16.0f;
            mesh.vertices[output * 3 + 2] = (source[corner * 3 + 2] - center.z) / 16.0f;
            mesh.texcoords[output * 2] = uvs[corner * 2] / BLOCK_ATLAS_SIZE;
            mesh.texcoords[output * 2 + 1] = uvs[corner * 2 + 1] / BLOCK_ATLAS_SIZE;
            mesh.colors[output * 4] = color.r;
            mesh.colors[output * 4 + 1] = color.g;
            mesh.colors[output * 4 + 2] = color.b;
            mesh.colors[output * 4 + 3] = color.a;
        }
    }

    UploadMesh(&mesh, false);
    return mesh;
}

static void BuildIcon(int blockId, Material material) {
    const Block *block = Block_GetDefinition(blockId);
    Mesh mesh = BuildMesh(blockId, block);
    RenderTexture2D target = LoadRenderTexture(BLOCK_ITEM_TEXTURE_SIZE, BLOCK_ITEM_TEXTURE_SIZE);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    Camera3D camera = {
        .position = {1.7f, 1.35f, 1.7f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 1.75f,
        .projection = CAMERA_ORTHOGRAPHIC
    };

    BeginTextureMode(target);
        ClearBackground(BLANK);
        BeginMode3D(camera);
            rlDisableBackfaceCulling();
            DrawMesh(mesh, material, MatrixIdentity());
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();

    UnloadMesh(mesh);
    icons[blockId].target = target;
    icons[blockId].loaded = true;
}

void BlockItemRenderer_Init(Texture2D terrain) {
    iconTerrain = terrain;
    Material material = LoadMaterialDefault();
    SetMaterialTexture(&material, MATERIAL_MAP_DIFFUSE, terrain);

    for (int blockId = 1; blockId < BLOCK_ITEM_COUNT; blockId++) {
        if (Block_IsSelectable(blockId)) {
            BuildIcon(blockId, material);
        }
    }

    if (material.maps != NULL) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
    }
    UnloadMaterial(material);
}

void BlockItemRenderer_Shutdown(void) {
    iconTerrain = (Texture2D){0};
    for (int blockId = 0; blockId < BLOCK_ITEM_COUNT; blockId++) {
        if (icons[blockId].loaded) UnloadRenderTexture(icons[blockId].target);
        icons[blockId] = (BlockItemIcon){0};
    }
}

void BlockItemRenderer_Refresh(int blockId) {
    if (blockId < 1 || blockId >= BLOCK_ITEM_COUNT) return;
    if (icons[blockId].loaded) UnloadRenderTexture(icons[blockId].target);
    icons[blockId] = (BlockItemIcon){0};
    if (!iconTerrain.id || !Block_IsSelectable(blockId)) return;
    Material material = LoadMaterialDefault();
    SetMaterialTexture(&material, MATERIAL_MAP_DIFFUSE, iconTerrain);
    BuildIcon(blockId, material);
    material.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
    UnloadMaterial(material);
}

void BlockItemRenderer_Draw(int blockId, Rectangle bounds) {
    if (blockId < 0 || blockId >= BLOCK_ITEM_COUNT || !icons[blockId].loaded) return;

    Texture2D texture = icons[blockId].target.texture;
    Rectangle source = {0.0f, 0.0f, (float)texture.width, -(float)texture.height};
    DrawTexturePro(texture, source, bounds, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
}
