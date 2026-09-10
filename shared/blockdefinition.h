#ifndef MIDLESS_BLOCK_DEFINITION_H
#define MIDLESS_BLOCK_DEFINITION_H

#include "packetsizes.h"

#include <stdbool.h>
#include <stdint.h>

#define PACKET_DEFINE_BLOCK 12
#define PACKET_REMOVE_BLOCK_DEFINITION 13
#define BLOCK_MODEL_MAX_BOXES 8
#define BLOCK_MAX_STATES 32
#define BLOCK_RUNTIME_COUNT (256 * BLOCK_MAX_STATES)
#define BLOCK_GEOMETRY_BYTES (5 + BLOCK_MODEL_MAX_BOXES * 24)
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

typedef struct BlockBox { uint8_t min[3], max[3]; } BlockBox;
typedef struct BlockModelBox { BlockBox bounds; uint8_t textures[6]; } BlockModelBox;
typedef struct BlockGeometry {
    uint8_t enabled, rotation, boxCount, collisionCount, selectionCount;
    BlockModelBox boxes[BLOCK_MODEL_MAX_BOXES];
    BlockBox collision[BLOCK_MODEL_MAX_BOXES], selection[BLOCK_MODEL_MAX_BOXES];
} BlockGeometry;

typedef struct BlockDefinition {
    char name[65];
    uint8_t textures[6];
    uint8_t modelType, renderType, colliderType, lightType;
    uint8_t min[3], max[3];
    BlockGeometry geometry;
} BlockDefinition;

BlockBox BlockBox_Rotate(BlockBox box, int turns);
void BlockGeometry_Encode(uint8_t *bytes, const BlockGeometry *geometry);
bool BlockGeometry_Decode(BlockGeometry *geometry, const uint8_t *bytes);

bool BlockDefinition_Validate(int id, const BlockDefinition *definition);

#endif
