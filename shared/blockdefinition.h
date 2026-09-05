#ifndef ISLEFORGE_BLOCK_DEFINITION_H
#define ISLEFORGE_BLOCK_DEFINITION_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_PROTOCOL_VERSION 2
#define PACKET_DEFINE_BLOCK 12
#define PACKET_REMOVE_BLOCK_DEFINITION 13
#define DEFINE_BLOCK_PACKET_SIZE 82
#define BLOCK_DEFAULT_LAST_ID 18

typedef enum BlockModelType {
    BLOCK_MODEL_GAS, BLOCK_MODEL_SOLID, BLOCK_MODEL_SPRITE
} BlockModelType;
typedef enum BlockLightType {
    BLOCK_LIGHT_NONE, BLOCK_LIGHT_EMIT
} BlockLightType;
typedef enum BlockRenderType {
    BLOCK_RENDER_OPAQUE, BLOCK_RENDER_TRANSPARENT, BLOCK_RENDER_TRANSLUCENT
} BlockRenderType;
typedef enum BlockColliderType {
    BLOCK_COLLIDER_NONE, BLOCK_COLLIDER_SOLID, BLOCK_COLLIDER_LIQUID
} BlockColliderType;

typedef struct BlockDefinition {
    char name[65];
    uint8_t textures[6];
    uint8_t modelType, renderType, colliderType, lightType;
    uint8_t min[3], max[3];
} BlockDefinition;

bool BlockDefinition_Validate(int id, const BlockDefinition *definition);

#endif
