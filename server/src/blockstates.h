#ifndef MIDLESS_BLOCKSTATES_H
#define MIDLESS_BLOCKSTATES_H
#include "blockdefinition.h"
#include "blockshape.h"
#include "minilua.h"
struct Chunk;
void ServerBlockStates_Define(lua_State *L, int id, int table, BlockDefinition *base);
void ServerBlockStates_Reset(void);
void ServerBlockStates_Remove(int id);
const BlockDefinition *ServerBlockStates_Definition(int id, int state);
int ServerBlockStates_Count(int id);
int ServerBlockStates_Resolve(struct Chunk *chunk, int index);
int ServerBlockStates_WireId(int id, Vector3 position);
void ServerBlockStates_Changed(struct Chunk *chunk, int index);
BlockShape ServerBlockStates_Shape(int id, Vector3 position);
#endif
