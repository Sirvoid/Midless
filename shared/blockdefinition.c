#include <string.h>
#include "blockdefinition.h"

bool BlockDefinition_Validate(int id, const BlockDefinition *d) {
    if (id < 1 || id >= BLOCK_RUNTIME_COUNT || !(id & 255) || !d || !d->name[0] ||
        !memchr(d->name, 0, sizeof(d->name))) return false;
    if (d->modelType > BLOCK_MODEL_SPRITE || d->renderType > BLOCK_RENDER_TRANSLUCENT ||
        d->colliderType > BLOCK_COLLIDER_LIQUID || d->lightType > BLOCK_LIGHT_EMIT) return false;
    for (int i = 0; i < 3; i++) {
        if (d->min[i] >= d->max[i] || d->max[i] > 16) return false;
    }
    const BlockGeometry *g = &d->geometry;
    if (g->enabled > 1 || g->rotation > 3 || g->boxCount > BLOCK_MODEL_MAX_BOXES ||
        g->collisionCount > BLOCK_MODEL_MAX_BOXES || g->selectionCount > BLOCK_MODEL_MAX_BOXES) return false;
    if (g->enabled && (!g->boxCount || d->modelType != BLOCK_MODEL_SOLID)) return false;
    for (int group = 0; group < 3; group++) {
        int count = group == 0 ? g->boxCount : group == 1 ? g->collisionCount : g->selectionCount;
        for (int j = 0; j < count; j++) {
            BlockBox b = group == 0 ? g->boxes[j].bounds : group == 1 ? g->collision[j] : g->selection[j];
            for (int a = 0; a < 3; a++) if (b.min[a] >= b.max[a] || b.max[a] > 16) return false;
        }
    }
    return true;
}

BlockBox BlockBox_Rotate(BlockBox b, int turns) {
    while (turns-- > 0) {
        BlockBox old = b;
        b.min[0] = 16 - old.max[2]; b.max[0] = 16 - old.min[2];
        b.min[2] = old.min[0]; b.max[2] = old.max[0];
    }
    return b;
}
void BlockGeometry_Encode(uint8_t *out, const BlockGeometry *g) {
    *out++=g->enabled; *out++=g->rotation; *out++=g->boxCount;
    *out++=g->collisionCount; *out++=g->selectionCount;
    for (int i=0;i<BLOCK_MODEL_MAX_BOXES;i++) {
        memcpy(out,g->boxes[i].bounds.min,3); out+=3;
        memcpy(out,g->boxes[i].bounds.max,3); out+=3;
        memcpy(out,g->boxes[i].textures,6); out+=6;
        memcpy(out,g->collision[i].min,3); out+=3; memcpy(out,g->collision[i].max,3); out+=3;
        memcpy(out,g->selection[i].min,3); out+=3; memcpy(out,g->selection[i].max,3); out+=3;
    }
}
bool BlockGeometry_Decode(BlockGeometry *g, const uint8_t *in) {
    *g=(BlockGeometry){0};
    g->enabled=*in++; g->rotation=*in++; g->boxCount=*in++;
    g->collisionCount=*in++; g->selectionCount=*in++;
    for (int i=0;i<BLOCK_MODEL_MAX_BOXES;i++) {
        memcpy(g->boxes[i].bounds.min,in,3); in+=3;
        memcpy(g->boxes[i].bounds.max,in,3); in+=3;
        memcpy(g->boxes[i].textures,in,6); in+=6;
        memcpy(g->collision[i].min,in,3); in+=3; memcpy(g->collision[i].max,in,3); in+=3;
        memcpy(g->selection[i].min,in,3); in+=3; memcpy(g->selection[i].max,in,3); in+=3;
    }
    return g->enabled<=1 && g->rotation<4 && g->boxCount<=BLOCK_MODEL_MAX_BOXES &&
        g->collisionCount<=BLOCK_MODEL_MAX_BOXES && g->selectionCount<=BLOCK_MODEL_MAX_BOXES;
}
