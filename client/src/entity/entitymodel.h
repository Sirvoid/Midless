/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_ENTITYM_H
#define G_ENTITYM_H

#include "raylib.h"
#include "entitymodelpart.h"

typedef enum PartType{
    PartType_None,
	PartType_Head
} PartType;

typedef struct EntityModelDef {
    int amountBoxes;
    BoundingBox *boxes;
    Rectangle (*uvs)[6];
    Vector3 *positions;
    PartType *types;
    Texture2D defaultTexture;
} EntityModelDef;

extern EntityModelDef entityModels[256];

typedef struct EntityModel{
    int amountParts;
    EntityModelPart *parts;
    Material mat;
} EntityModel;

void EntityModel_DefineAll(void);
void EntityModel_Build(EntityModel *model, EntityModelDef modelDef);
void EntityModel_Free(EntityModel *model);

#endif