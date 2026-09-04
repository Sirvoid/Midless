/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stddef.h>
#include "entitymodel.h"
#include "resource.h"
#include "rlgl.h"

EntityModelDefinition entityModels[256];

typedef enum ModelFaceDirection {
    MODEL_FACE_EAST,
    MODEL_FACE_WEST,
    MODEL_FACE_UP,
    MODEL_FACE_DOWN,
    MODEL_FACE_NORTH,
    MODEL_FACE_SOUTH
} ModelFaceDirection;

void EntityModel_DefineHumanoid(void) {
    EntityModelDefinition model;
    int boxCount = 6;
    model.boxCount = 6;
    model.boxes = MemAlloc(sizeof(BoundingBox[boxCount]));
    model.positions = MemAlloc(sizeof(Vector3[boxCount]));
    model.uvs = MemAlloc(sizeof(Rectangle[boxCount][6]));
    model.types = MemAlloc(sizeof(PartType[boxCount]));
    model.firstPersonVisible = MemAlloc(sizeof(bool[boxCount]));
    for (int i = 0; i < boxCount; i++) model.firstPersonVisible[i] = false;
    int partI = 0;

    //head
    model.types[partI] = PART_TYPE_HEAD;
    model.positions[partI] = (Vector3){0.0f,19.0f,0.0f};
    model.boxes[partI].min = (Vector3) {-4.0f,-0.5f,-4.0f};
    model.boxes[partI].max = (Vector3) {4.0f,7.5f,3.0f};
    model.uvs[partI][MODEL_FACE_NORTH] = (Rectangle){14,14,16,16};
    model.uvs[partI][MODEL_FACE_EAST] = (Rectangle){30,14,14,16};
    model.uvs[partI][MODEL_FACE_SOUTH] = (Rectangle){44,14,16,16};
    model.uvs[partI][MODEL_FACE_WEST] = (Rectangle){0,14,14,16};
    model.uvs[partI][MODEL_FACE_UP] = (Rectangle){30,14,-16,-14};
    model.uvs[partI][MODEL_FACE_DOWN] = (Rectangle){46,0,-16,14};
    partI++;

    //torso
    model.types[partI] = PART_TYPE_NONE;
    model.positions[partI] = (Vector3){0.4f,18.3f,-0.4f};
    model.boxes[partI].min = (Vector3) {-3.9f,-8.3f,-1.6f};
    model.boxes[partI].max = (Vector3) {3.1f,0.7f,1.4f};
    model.uvs[partI][MODEL_FACE_NORTH] = (Rectangle){6,36,14,18};
    model.uvs[partI][MODEL_FACE_EAST] = (Rectangle){20,36,6,18};
    model.uvs[partI][MODEL_FACE_SOUTH] = (Rectangle){26,36,14,18};
    model.uvs[partI][MODEL_FACE_WEST] = (Rectangle){0,36,6,18};
    model.uvs[partI][MODEL_FACE_UP] = (Rectangle){20,36,-14,-6};
    model.uvs[partI][MODEL_FACE_DOWN] = (Rectangle){34,30,-14,6};
    partI++;

    //rightarm
    model.types[partI] = PART_TYPE_RIGHT_ARM;
    model.firstPersonVisible[partI] = true;
    model.positions[partI] = (Vector3){-3.5f,17.5f,0.0f};
    model.boxes[partI].min = (Vector3) {-2.8f,-9.0f,-2.0f};
    model.boxes[partI].max = (Vector3) {0.3f,1.0f,1.0f};
    model.uvs[partI][MODEL_FACE_NORTH] = (Rectangle){52,36,-6,20};
    model.uvs[partI][MODEL_FACE_EAST] = (Rectangle){58,36,-6,20};
    model.uvs[partI][MODEL_FACE_SOUTH] = (Rectangle){64,36,-6,20};
    model.uvs[partI][MODEL_FACE_WEST] = (Rectangle){46,36,-6,20};
    model.uvs[partI][MODEL_FACE_UP] = (Rectangle){46,36,6,-6};
    model.uvs[partI][MODEL_FACE_DOWN] = (Rectangle){52,30,6,6};
    partI++;

    //leftarm
    model.types[partI] = PART_TYPE_LEFT_ARM;
    model.positions[partI] = (Vector3){3.5f,17.5f,0.0f};
    model.boxes[partI].min = (Vector3) {-0.3f,-9.0f,-2.0f};
    model.boxes[partI].max = (Vector3) {2.8f,1.0f,1.0f};
    model.uvs[partI][MODEL_FACE_NORTH] = (Rectangle){76,36,-6,20};
    model.uvs[partI][MODEL_FACE_EAST] = (Rectangle){82,36,-6,20};
    model.uvs[partI][MODEL_FACE_SOUTH] = (Rectangle){88,36,-6,20};
    model.uvs[partI][MODEL_FACE_WEST] = (Rectangle){70,36,-6,20};
    model.uvs[partI][MODEL_FACE_UP] = (Rectangle){70,36,6,-6};
    model.uvs[partI][MODEL_FACE_DOWN] = (Rectangle){76,30,6,6};
    partI++;

    //rightleg
    model.types[partI] = PART_TYPE_RIGHT_LEG;
    model.positions[partI] = (Vector3){-1.4f,8.6f,0.0f};
    model.boxes[partI].min = (Vector3) {-1.6f,-8.6f,-2.0f};
    model.boxes[partI].max = (Vector3) {1.4f,1.4f,1.0f};
    model.uvs[partI][MODEL_FACE_NORTH] = (Rectangle){90,6,6,20};
    model.uvs[partI][MODEL_FACE_EAST] = (Rectangle){96,6,6,20};
    model.uvs[partI][MODEL_FACE_SOUTH] = (Rectangle){102,6,6,20};
    model.uvs[partI][MODEL_FACE_WEST] = (Rectangle){84,6,6,20};
    model.uvs[partI][MODEL_FACE_UP] = (Rectangle){96,6,-6,-6};
    model.uvs[partI][MODEL_FACE_DOWN] = (Rectangle){102,0,-6,6};
    partI++;

    //leftleg
    model.types[partI] = PART_TYPE_LEFT_LEG;
    model.positions[partI] = (Vector3){1.6f,8.6f,0.0f};
    model.boxes[partI].min = (Vector3) {-1.6f,-8.6f,-2.0f};
    model.boxes[partI].max = (Vector3) {1.4f,1.4f,1.0f};
    model.uvs[partI][MODEL_FACE_NORTH] = (Rectangle){66,6,6,20};
    model.uvs[partI][MODEL_FACE_EAST] = (Rectangle){72,6,6,20};
    model.uvs[partI][MODEL_FACE_SOUTH] = (Rectangle){78,6,6,20};
    model.uvs[partI][MODEL_FACE_WEST] = (Rectangle){60,6,6,20};
    model.uvs[partI][MODEL_FACE_UP] = (Rectangle){72,6,-6,-6};
    model.uvs[partI][MODEL_FACE_DOWN] = (Rectangle){78,0,-6,6};
    partI++;

    model.defaultTexture = Resource_LoadTexture("humanoid.png");

    entityModels[0] = model;

}

void EntityModelDefinitions_Init(void) {
    EntityModel_DefineHumanoid();
}

void EntityModelDefinitions_Shutdown(void) {
    for (int i = 0; i < 256; i++) {
        EntityModelDefinition *model = &entityModels[i];
        if (model->boxCount == 0) continue;

        MemFree(model->boxes);
        MemFree(model->positions);
        MemFree(model->uvs);
        MemFree(model->types);
        MemFree(model->firstPersonVisible);
        UnloadTexture(model->defaultTexture);
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
