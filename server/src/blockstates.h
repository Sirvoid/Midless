/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_BLOCKSTATES_H
#define MIDLESS_BLOCKSTATES_H
#include "blockdefinition.h"
#include "blockshape.h"
struct Chunk;
#define BLOCK_STATE_MAX_FIELDS 8
#define BLOCK_STATE_MAX_RULES 31
typedef struct StateField {
    char name[65];
    bool boolean;
    int bits;
} StateField;
typedef struct StateRule {
    int mask, state, rotationField;
    int64_t values[BLOCK_STATE_MAX_FIELDS];
} StateRule;
typedef struct StateSet {
    int fieldCount, ruleCount, count;
    StateField fields[BLOCK_STATE_MAX_FIELDS];
    StateRule rules[BLOCK_STATE_MAX_RULES];
    BlockDefinition states[BLOCK_MAX_STATES];
} StateSet;

void ServerBlockStates_UpdateBounds(BlockDefinition *definition);
bool ServerBlockStates_Define(int id, const StateSet *definition, BlockDefinition *base);
void ServerBlockStates_Reset(void);
void ServerBlockStates_Remove(int id);
const BlockDefinition *ServerBlockStates_Definition(int id, int state);
int ServerBlockStates_Count(int id);
int ServerBlockStates_Resolve(struct Chunk *chunk, int index);
int ServerBlockStates_WireId(int id, Vector3 position);
void ServerBlockStates_Changed(struct Chunk *chunk, int index);
BlockShape ServerBlockStates_Shape(int id, Vector3 position);
#endif
