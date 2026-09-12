/**
 * Copyright (c) 2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_ENTITY_MODEL_H
#define MIDLESS_CLIENT_ENTITY_MODEL_H

#include "raylib.h"
#include "entitymodeldefinition.h"
#include "entitymodelpart.h"

typedef enum PartType{
    PART_TYPE_NONE,
	PART_TYPE_HEAD,
    PART_TYPE_RIGHT_ARM,
    PART_TYPE_LEFT_ARM,
    PART_TYPE_RIGHT_LEG,
    PART_TYPE_LEFT_LEG
} PartType;

typedef struct EntityModelDefinition {
    int boxCount;
    Vector3 grips[ENTITY_MODEL_MAX_PARTS];
    bool hasGrip[ENTITY_MODEL_MAX_PARTS];
    BoundingBox *boxes;
    Rectangle (*uvs)[6];
    Vector3 *positions;
    PartType *types;
    bool *firstPersonVisible;
    Texture2D defaultTexture;
} EntityModelDefinition;

extern EntityModelDefinition entityModels[256];

typedef struct EntityModel{
    int partCount;
    EntityModelPart *parts;
    Material material;
} EntityModel;

void EntityModelDefinitions_Init(void);
const EntityModelDefinition *EntityModel_GetDefinition(int id);
bool EntityModel_ApplyDefinition(int id, const ModelDefinition *definition);
void EntityModel_RemoveDefinition(int id);
void EntityModel_ResetDefinitions(void);
void EntityModel_SetEntityModel(int entityId, int modelId);
void EntityModel_SetEntityTexture(int entityId, int texture);
void EntityModel_CreateTextured(EntityModel *model, int modelId, int texture);
void EntityModelDefinitions_Shutdown(void);
void EntityModel_Create(EntityModel *model, EntityModelDefinition modelDef);
void EntityModel_Unload(EntityModel *model);
void EntityModel_Destroy(EntityModel *model);

bool EntityModel_TextureFits(int textureId, int width, int height);
void EntityModel_RefreshTextures(int textureId);

#endif
