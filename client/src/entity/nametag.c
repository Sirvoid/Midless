/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "nametag.h"
#include "entity.h"
#include "world.h"
#include "formattedtext.h"
#include "raymath.h"
#include "rlgl.h"

static Vector3 GetAnchor(const Entity *entity) {
    float top = 0;
    const EntityModelDefinition *model = EntityModel_GetDefinition(entity->modelId);
    if (entity->type == ENTITY_TYPE_DROPPED_ITEM) top = 0.3f;
    else if (model) {
        Matrix rotation = MatrixRotateXYZ(entity->rotation);
        for (int i = 0; i < model->boxCount; i++) {
            BoundingBox box = model->boxes[i];
            for (int j = 0; j < 8; j++) {
                Vector3 corner = {j & 1 ? box.max.x : box.min.x,
                    j & 2 ? box.max.y : box.min.y, j & 4 ? box.max.z : box.min.z};
                corner = Vector3Scale(Vector3Add(corner, model->positions[i]), 1.0f / 16);
                corner = Vector3Transform(corner, rotation);
                top = fmaxf(top, corner.y);
            }
        }
    }
    return Vector3Add(entity->position, (Vector3){0, top + 0.25f + entity->nametag.offset, 0});
}

static void DrawTag(const Entity *entity, Camera camera, float distance) {
    TextGlyph glyphs[NAMETAG_TEXT_SIZE];
    int lines;
    float width;
    int count = FormattedText_Layout(entity->nametag.text, entity->nametag.color, 10, 0,
        glyphs, NAMETAG_TEXT_SIZE, &lines, &width);
    if (!count) return;
    Vector3 anchor = GetAnchor(entity);
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    if (Vector3DotProduct(Vector3Subtract(anchor, camera.position), forward) <= 0) return;
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3CrossProduct(right, forward);
    float transform[16] = {right.x, right.y, right.z, 0, up.x, up.y, up.z, 0,
        -forward.x, -forward.y, -forward.z, 0, anchor.x, anchor.y, anchor.z, 1};
    float opacity = Clamp((32 - distance) / 8, 0, 1);
    rlPushMatrix();
    rlMultMatrixf(transform);
    rlScalef(0.025f, -0.025f, 0.025f);
    DrawRectangleRec((Rectangle){-width / 2 - 2, -12, width + 4, 14}, (Color){0, 0, 0, (unsigned char)(80 * opacity)});
    for (int i = 0; i < count; i++)
        FormattedText_DrawGlyph(glyphs[i], (Vector2){glyphs[i].x - width / 2, -10}, 10, opacity);
    rlPopMatrix();
}

void Nametags_Draw(Camera camera) {
    struct { int id; float distance; } sorted[WORLD_MAX_ENTITIES];
    int count = 0;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        const Entity *e = &world.entities[i];
        if (!e->type || !e->nametag.visible || !e->nametag.text[0]) continue;
        float distance = Vector3Distance(camera.position, e->position);
        if (distance >= 32) continue;
        Vector3 chunkPosition = {floorf(e->position.x / CHUNK_SIZE_X),
            floorf(e->position.y / CHUNK_SIZE_Y), floorf(e->position.z / CHUNK_SIZE_Z)};
        Chunk *chunk = World_GetChunkAt(chunkPosition);
        if (!chunk || !chunk->isBlockDataReady) continue;
        int j = count++;
        while (j > 0 && sorted[j - 1].distance < distance) { sorted[j] = sorted[j - 1]; j--; }
        sorted[j].id = i; sorted[j].distance = distance;
    }
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    for (int i = 0; i < count; i++) DrawTag(&world.entities[sorted[i].id], camera, sorted[i].distance);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}
