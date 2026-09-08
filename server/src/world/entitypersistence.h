#ifndef MIDLESS_ENTITY_PERSISTENCE_H
#define MIDLESS_ENTITY_PERSISTENCE_H
#include "chunk/chunk.h"
void EntityPersistence_EnsureChunks(void);
bool EntityPersistence_Activate(Chunk *chunk);
bool EntityPersistence_Save(Chunk *chunk);
void EntityPersistence_Unload(Chunk *chunk);
#endif
