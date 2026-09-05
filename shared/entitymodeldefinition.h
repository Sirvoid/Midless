#ifndef ISLEFORGE_ENTITY_MODEL_DEFINITION_H
#define ISLEFORGE_ENTITY_MODEL_DEFINITION_H
#include <stdint.h>
#include <stdbool.h>
#define PACKET_DEFINE_ENTITY_MODEL 14
#define PACKET_REMOVE_ENTITY_MODEL 15
#define PACKET_SET_ENTITY_MODEL 16
#define ENTITY_MODEL_MAX_PARTS 64
#define ENTITY_MODEL_HEADER_SIZE 69
#define ENTITY_MODEL_PART_SIZE 68
// Coordinates use 1/64 of a model unit. UVs are signed pixel rectangles.
// Faces: east, west, up, down, north, south (matches existing models).
typedef struct ModelPartDefinition {
    uint8_t role, firstPersonVisible;
    int16_t position[3], min[3], max[3];
    int16_t uv[6][4];
} ModelPartDefinition;
typedef struct ModelDefinition {
    char name[65];
    uint16_t texture; // 0 = humanoid, 1 = terrain
    uint8_t partCount;
    ModelPartDefinition parts[ENTITY_MODEL_MAX_PARTS];
} ModelDefinition;
bool ModelDefinition_Validate(int id, const ModelDefinition *definition);
#endif
