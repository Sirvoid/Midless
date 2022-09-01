/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "entitymodel.h"

EntityModelDef entityModels[256];

enum directions {
    east,
    west,
    up,
    down,
    north,
    south
};

void EntityModel_DefineHumanoid(void) {
    EntityModelDef model;
    int amountBoxes = 6;
    model.amountBoxes = 6;
    model.boxes = MemAlloc(sizeof(BoundingBox[amountBoxes]));
    model.positions = MemAlloc(sizeof(Vector3[amountBoxes]));
    model.uvs = MemAlloc(sizeof(Rectangle[amountBoxes][6]));
    model.types = MemAlloc(sizeof(PartType[amountBoxes]));
    int partI = 0;

    //head
    model.types[partI] = PartType_Head;
    model.positions[partI] = (Vector3){0.0f,19.0f,0.0f};
    model.boxes[partI].min = (Vector3) {-4.0f,-0.5f,-4.0f};
    model.boxes[partI].max = (Vector3) {4.0f,7.5f,3.0f};
    model.uvs[partI][north] = (Rectangle){14,14,16,16};
    model.uvs[partI][east] = (Rectangle){30,14,14,16};
    model.uvs[partI][south] = (Rectangle){44,14,16,16};
    model.uvs[partI][west] = (Rectangle){0,14,14,16};
    model.uvs[partI][up] = (Rectangle){30,14,-16,-14};
    model.uvs[partI][down] = (Rectangle){46,0,-16,14};
    partI++;

    //rightleg
    model.types[partI] = PartType_None;
    model.positions[partI] = (Vector3){1.6f,8.6f,0.0f};
    model.boxes[partI].min = (Vector3) {-1.6f,-8.6f,-2.0f};
    model.boxes[partI].max = (Vector3) {1.4f,1.4f,1.0f};
    model.uvs[partI][north] = (Rectangle){66,6,6,20};
    model.uvs[partI][east] = (Rectangle){72,6,6,20};
    model.uvs[partI][south] = (Rectangle){78,6,6,20};
    model.uvs[partI][west] = (Rectangle){60,6,6,20};
    model.uvs[partI][up] = (Rectangle){72,6,-6,-6};
    model.uvs[partI][down] = (Rectangle){78,0,-6,6};
    partI++;

    //torso
    model.types[partI] = PartType_None;
    model.positions[partI] = (Vector3){0.4f,18.3f,-0.4f};
    model.boxes[partI].min = (Vector3) {-3.9f,-8.3f,-1.6f};
    model.boxes[partI].max = (Vector3) {3.1f,0.7f,1.4f};
    model.uvs[partI][north] = (Rectangle){6,36,14,18};
    model.uvs[partI][east] = (Rectangle){20,36,6,18};
    model.uvs[partI][south] = (Rectangle){26,36,14,18};
    model.uvs[partI][west] = (Rectangle){0,36,6,18};
    model.uvs[partI][up] = (Rectangle){20,36,-14,-6};
    model.uvs[partI][down] = (Rectangle){34,30,-14,6};
    partI++;

    //leftarm
    model.types[partI] = PartType_None;
    model.positions[partI] = (Vector3){-3.5f,17.5f,0.0f};
    model.boxes[partI].min = (Vector3) {-2.8f,-9.0f,-2.0f};
    model.boxes[partI].max = (Vector3) {0.3f,1.0f,1.0f};
    model.uvs[partI][north] = (Rectangle){52,36,-6,20};
    model.uvs[partI][east] = (Rectangle){58,36,-6,20};
    model.uvs[partI][south] = (Rectangle){64,36,-6,20};
    model.uvs[partI][west] = (Rectangle){46,36,-6,20};
    model.uvs[partI][up] = (Rectangle){46,36,6,-6};
    model.uvs[partI][down] = (Rectangle){52,30,6,6};
    partI++;

    //rightarm
    model.types[partI] = PartType_None;
    model.positions[partI] = (Vector3){3.5f,17.5f,0.0f};
    model.boxes[partI].min = (Vector3) {-0.3f,-9.0f,-2.0f};
    model.boxes[partI].max = (Vector3) {2.8f,1.0f,1.0f};
    model.uvs[partI][north] = (Rectangle){76,36,-6,20};
    model.uvs[partI][east] = (Rectangle){82,36,-6,20};
    model.uvs[partI][south] = (Rectangle){88,36,-6,20};
    model.uvs[partI][west] = (Rectangle){70,36,-6,20};
    model.uvs[partI][up] = (Rectangle){70,36,6,-6};
    model.uvs[partI][down] = (Rectangle){76,30,6,6};
    partI++;

    //leftleg
    model.types[partI] = PartType_None;
    model.positions[partI] = (Vector3){-1.4f,8.6f,0.0f};
    model.boxes[partI].min = (Vector3) {-1.6f,-8.6f,-2.0f};
    model.boxes[partI].max = (Vector3) {1.4f,1.4f,1.0f};
    model.uvs[partI][north] = (Rectangle){90,6,6,20};
    model.uvs[partI][east] = (Rectangle){96,6,6,20};
    model.uvs[partI][south] = (Rectangle){102,6,6,20};
    model.uvs[partI][west] = (Rectangle){84,6,6,20};
    model.uvs[partI][up] = (Rectangle){96,6,-6,-6};
    model.uvs[partI][down] = (Rectangle){102,0,-6,6};
    partI++;

    Image tex = LoadImage("textures/humanoid.png"); 
    model.defaultTexture = LoadTextureFromImage(tex);

    entityModels[0] = model;

}

void EntityModel_DefineAll(void) {
    EntityModel_DefineHumanoid();
}

void EntityModel_Build(EntityModel *model, EntityModelDef modelDef) {
    model->amountParts = modelDef.amountBoxes;
    model->parts = MemAlloc(modelDef.amountBoxes * sizeof(EntityModelPart));

    model->mat = LoadMaterialDefault();
    SetMaterialTexture(&model->mat, MATERIAL_MAP_DIFFUSE, modelDef.defaultTexture);
    
    for(int i = 0; i < modelDef.amountBoxes; i++) {
        model->parts[i].type = modelDef.types[i];
        EntityModelPart_Build(&model->parts[i], modelDef.boxes[i], modelDef.uvs[i], (Vector2) {modelDef.defaultTexture.width, modelDef.defaultTexture.height}, modelDef.positions[i]);
    }
}

void EntityModel_Free(EntityModel *model) {
    for(int i = 0; i < model->amountParts; i++) { 
        UnloadMesh(model->parts[i].mesh);
    }
    MemFree(model->parts);
    //UnloadMaterial(model->mat);
}