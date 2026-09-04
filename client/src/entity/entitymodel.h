/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_ENTITY_MODEL_H
#define MIDLESS_CLIENT_ENTITY_MODEL_H

#include "raylib.h"
#include "entitymodelpart.h"

typedef enum PartType{
    PART_TYPE_NONE,
	PART_TYPE_HEAD
} PartType;

typedef struct EntityModelDefinition {
    int boxCount;
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
void EntityModelDefinitions_Shutdown(void);
void EntityModel_Create(EntityModel *model, EntityModelDefinition modelDef);
void EntityModel_Unload(EntityModel *model);
void EntityModel_Destroy(EntityModel *model);

#endif
