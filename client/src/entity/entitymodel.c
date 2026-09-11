/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stddef.h>
#include <limits.h>
#include "world.h"
#include "player.h"
#include "entitymodel.h"
#include "resource.h"
#include "rlgl.h"
#include "../textures.h"

EntityModelDefinition entityModels[256];
static ModelDefinition *receivedModels[256];

typedef enum ModelFaceDirection {
    MODEL_FACE_EAST,
    MODEL_FACE_WEST,
    MODEL_FACE_UP,
    MODEL_FACE_DOWN,
    MODEL_FACE_NORTH,
    MODEL_FACE_SOUTH
} ModelFaceDirection;

void EntityModel_DefineHumanoid(void) {
    ModelDefinition d = ModelDefinition_Humanoid();
    EntityModelDefinition model = {0};
    model.boxCount = d.partCount;
    model.boxes = MemAlloc(d.partCount * sizeof(*model.boxes));
    model.positions = MemAlloc(d.partCount * sizeof(*model.positions));
    model.uvs = MemAlloc(d.partCount * sizeof(*model.uvs));
    model.types = MemAlloc(d.partCount * sizeof(*model.types));
    model.firstPersonVisible = MemAlloc(d.partCount * sizeof(*model.firstPersonVisible));
    for (int i=0; i<d.partCount; i++) {
        ModelPartDefinition *p = &d.parts[i];
        model.types[i] = p->role;
        model.firstPersonVisible[i] = p->firstPersonVisible;
        model.positions[i] = (Vector3){p->position[0]/64.0f,p->position[1]/64.0f,p->position[2]/64.0f};
        model.boxes[i].min = (Vector3){p->min[0]/64.0f,p->min[1]/64.0f,p->min[2]/64.0f};
        model.boxes[i].max = (Vector3){p->max[0]/64.0f,p->max[1]/64.0f,p->max[2]/64.0f};
        for (int f=0; f<6; f++) model.uvs[i][f] = (Rectangle){p->uv[f][0],p->uv[f][1],p->uv[f][2],p->uv[f][3]};
    }
    model.defaultTexture = Resource_LoadTexture("humanoid.png");
    entityModels[0] = model;
}

void EntityModelDefinitions_Init(void) {
    EntityModel_DefineHumanoid();

}

void EntityModelDefinitions_Shutdown(void) {
    for(int i=1;i<256;i++) { MemFree(receivedModels[i]); receivedModels[i]=NULL; }
    for (int i = 0; i < 256; i++) {
        EntityModelDefinition *model = &entityModels[i];
        if (model->boxCount == 0) continue;

        MemFree(model->boxes);
        MemFree(model->positions);
        MemFree(model->uvs);
        MemFree(model->types);
        MemFree(model->firstPersonVisible);
        if (i == 0) UnloadTexture(model->defaultTexture);
        *model = (EntityModelDefinition){0};
    }
}

void EntityModel_Create(EntityModel *model, EntityModelDefinition modelDef) {
    model->partCount = modelDef.boxCount;
    model->parts = MemAlloc(modelDef.boxCount * sizeof(EntityModelPart));

    model->material = LoadMaterialDefault();
    SetMaterialTexture(&model->material, MATERIAL_MAP_DIFFUSE, modelDef.defaultTexture);
    
    for (int i = 0; i < modelDef.boxCount; i++) {
        model->parts[i].type = modelDef.types[i];
        model->parts[i].visibleInFirstPerson = modelDef.firstPersonVisible[i];
        EntityModelPart_Build(&model->parts[i], modelDef.boxes[i], modelDef.uvs[i], (Vector2) {modelDef.defaultTexture.width, modelDef.defaultTexture.height}, modelDef.positions[i]);
        if (modelDef.hasGrip[i]) model->parts[i].grip = modelDef.grips[i];
    }
}

void EntityModel_Unload(EntityModel *model) {
    for (int i = 0; i < model->partCount; i++) { 
        UnloadMesh(model->parts[i].mesh);
    }

    // Prevent UnloadMaterial() from deleting that shared GPU texture.
    if (model->material.maps != NULL) {
        model->material.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
    }
    
    UnloadMaterial(model->material);
}

void EntityModel_Destroy(EntityModel *model) {
    MemFree(model->parts);
    *model = (EntityModel){0};
}

const EntityModelDefinition *EntityModel_GetDefinition(int id) {
    if (id < 0 || id > 255 || !entityModels[id].boxCount) return &entityModels[0];
    return &entityModels[id];
}
static void FreeDefinition(EntityModelDefinition *d) {
    MemFree(d->boxes); MemFree(d->uvs); MemFree(d->positions);
    MemFree(d->types); MemFree(d->firstPersonVisible);
    *d = (EntityModelDefinition){0};
}
void EntityModel_SetEntityModel(int entityId, int modelId) {
    if (modelId < 0 || modelId > 255) return;
    if (entityId == USHRT_MAX) {
        if (player.hasEntityModel) Player_SetEntityModel(player.entityType, modelId);
        return;
    }
    if (!world.entities || entityId < 0 || entityId >= WORLD_MAX_ENTITIES) return;
    Entity *e = &world.entities[entityId];
    if (e->type == ENTITY_TYPE_DROPPED_ITEM) return;
    if (!e->type) return;
    EntityModel_Unload(&e->model);
    EntityModel_Destroy(&e->model);
    e->modelId = modelId;
    EntityModel_CreateTextured(&e->model,modelId,e->textureOverride);
}
void EntityModel_CreateTextured(EntityModel *model, int modelId, int override) {
    EntityModelDefinition definition = *EntityModel_GetDefinition(modelId);
    Texture2D texture = override ? ClientTextures_Get(override-1) : (Texture2D){0};
    bool fits = texture.id != 0;
    for (int p=0; fits && p<definition.boxCount; p++) for (int f=0; f<6; f++) {
        Rectangle uv = definition.uvs[p][f];
        if (uv.x>texture.width || uv.x+uv.width>texture.width || uv.y>texture.height || uv.y+uv.height>texture.height) fits = false;
    }
    if (fits) definition.defaultTexture = texture;
    EntityModel_Create(model,definition);
}
void EntityModel_SetEntityTexture(int entityId, int texture) {
    if (texture<0 || texture>66) return;
    if (entityId==USHRT_MAX) {
        player.textureOverride = texture;
        if (player.hasEntityModel) Player_SetEntityModel(player.entityType,player.modelId);
    } else if (world.entities && entityId>=0 && entityId<WORLD_MAX_ENTITIES && world.entities[entityId].type) {
        Entity *e = &world.entities[entityId];
        e->textureOverride = texture;
        EntityModel_SetEntityModel(entityId,e->modelId);
    }
}
static void RefreshModelUsers(int id) {
    if (world.entities) for (int i = 0; i < WORLD_MAX_ENTITIES; i++)
        if (world.entities[i].type && world.entities[i].modelId == id) EntityModel_SetEntityModel(i, id);
    if (player.hasEntityModel && player.modelId == id) EntityModel_SetEntityModel(USHRT_MAX, id);
}
bool EntityModel_ApplyDefinition(int id, const ModelDefinition *d) {
    if (!ModelDefinition_Validate(id, d)) return false;
    Texture2D texture = ClientTextures_Get(d->texture);
    if (!receivedModels[id]) {
        receivedModels[id] = MemAlloc(sizeof(*d));
        if (receivedModels[id]) *receivedModels[id] = (ModelDefinition){0};
    }
    if (!receivedModels[id]) return false;
    if (!texture.id) {
        *receivedModels[id] = *d;
        FreeDefinition(&entityModels[id]);
        RefreshModelUsers(id);
        return true;
    }
    for (int i = 0; i < d->partCount; i++) for (int f = 0; f < 6; f++) {
        const int16_t *uv = d->parts[i].uv[f];
        if (uv[0] > texture.width || uv[0]+uv[2] > texture.width ||
            uv[1] > texture.height || uv[1]+uv[3] > texture.height) return false;
    }
    *receivedModels[id] = *d;
    EntityModelDefinition result = {0};
    int count = d->partCount;
    result.boxCount = count;
    result.defaultTexture = texture;
    result.boxes = MemAlloc(count * sizeof(*result.boxes));
    result.positions = MemAlloc(count * sizeof(*result.positions));
    result.uvs = MemAlloc(count * sizeof(*result.uvs));
    result.types = MemAlloc(count * sizeof(*result.types));
    result.firstPersonVisible = MemAlloc(count * sizeof(*result.firstPersonVisible));
    if (!result.boxes || !result.positions || !result.uvs || !result.types || !result.firstPersonVisible) {
        FreeDefinition(&result); return false;
    }
    for (int i = 0; i < count; i++) {
        const ModelPartDefinition *p = &d->parts[i];
        result.positions[i] = (Vector3){p->position[0]/64.0f,p->position[1]/64.0f,p->position[2]/64.0f};
        result.boxes[i].min = (Vector3){p->min[0]/64.0f,p->min[1]/64.0f,p->min[2]/64.0f};
        result.boxes[i].max = (Vector3){p->max[0]/64.0f,p->max[1]/64.0f,p->max[2]/64.0f};
        result.types[i] = (PartType)p->role;
        result.firstPersonVisible[i] = p->firstPersonVisible;
        result.hasGrip[i] = p->hasGrip;
        result.grips[i] = (Vector3){p->grip[0]/1024.0f, p->grip[1]/1024.0f, p->grip[2]/1024.0f};
        for (int f = 0; f < 6; f++) result.uvs[i][f] = (Rectangle){p->uv[f][0],p->uv[f][1],p->uv[f][2],p->uv[f][3]};
    }
    FreeDefinition(&entityModels[id]);
    entityModels[id] = result;
    RefreshModelUsers(id);
    return true;
}
void EntityModel_RemoveDefinition(int id) {
    if (id < 1 || id > 255) return;
    MemFree(receivedModels[id]); receivedModels[id]=NULL;
    if (!entityModels[id].boxCount) return;
    FreeDefinition(&entityModels[id]);
    RefreshModelUsers(id);
}
void EntityModel_ResetDefinitions(void) {
    for (int id = 1; id < 256; id++) EntityModel_RemoveDefinition(id);
}

bool EntityModel_TextureFits(int textureId, int width, int height) {
    for(int id=1;id<256;id++) {
        ModelDefinition *d=receivedModels[id];
        if(!d || d->texture!=textureId) continue;
        for(int p=0;p<d->partCount;p++) for(int f=0;f<6;f++) {
            int16_t *uv=d->parts[p].uv[f];
            if(uv[0]>width || uv[0]+uv[2]>width || uv[1]>height || uv[1]+uv[3]>height) return false;
        }
    }
    return true;
}
void EntityModel_RefreshTextures(int textureId) {
    for(int id=1;id<256;id++)
        if(receivedModels[id] && receivedModels[id]->texture==textureId)
            EntityModel_ApplyDefinition(id,receivedModels[id]);
    if (world.entities) for (int i=0; i<WORLD_MAX_ENTITIES; i++) {
        Entity *e = &world.entities[i];
        if (e->type && e->textureOverride==textureId+1) EntityModel_SetEntityModel(i,e->modelId);
    }
    if (player.hasEntityModel && player.textureOverride==textureId+1) Player_SetEntityModel(player.entityType,player.modelId);
}
