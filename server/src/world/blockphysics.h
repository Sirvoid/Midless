/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_BLOCK_PHYSICS_H
#define MIDLESS_BLOCK_PHYSICS_H
#include "chunk/chunk.h"

#define BLOCK_PHYSICS_BUDGET 4096
#define BLOCK_PHYSICS_SECONDS 0.002
typedef enum BlockPhysicsType {
    BLOCK_PHYSICS_NONE,
    BLOCK_PHYSICS_FALLING,
    BLOCK_PHYSICS_FLUID,
    BLOCK_PHYSICS_CUSTOM
} BlockPhysicsType;
typedef struct BlockPhysicsDefinition {
    BlockPhysicsType type;
    float interval;
    int maxLevel;
    bool renewableSources, callback;
    bool replaceable[256];
} BlockPhysicsDefinition;
typedef struct FluidState {
    bool source, falling;
    int level;
} FluidState;

void ServerBlockPhysics_Define(int id, const BlockPhysicsDefinition *definition);
void ServerBlockPhysics_Reset(void);
Chunk *ServerBlockPhysics_Find(Vector3 position, int *index);
bool ServerBlockPhysics_Schedule(Vector3 position, float delay);
void ServerBlockPhysics_Cancel(Chunk *chunk, int index);
void ServerBlockPhysics_Changed(Vector3 position);
void ServerBlockPhysics_Loaded(Chunk *chunk);
void ServerBlockPhysics_Forget(Chunk *chunk);
int ServerBlockPhysics_Update(float dt);
int ServerBlockPhysics_Pending(void);
bool ServerBlockPhysics_Move(Vector3 from, Vector3 to);
// Takes ownership of metadata on success; publishes only the complete block/state.
bool ServerBlockPhysics_Place(Vector3 position, int id, Metadata *metadata);
bool ServerBlockPhysics_GetFluid(Vector3 position, FluidState *state);
bool ServerBlockPhysics_SetFluid(Vector3 position, FluidState state);
bool ServerBlockPhysics_FluidModels(int id, BlockDefinition *base);
// During a physics step, send each changed cell once with its final state.
bool ServerBlockPhysics_Defer(Chunk *chunk, int index);
#endif
