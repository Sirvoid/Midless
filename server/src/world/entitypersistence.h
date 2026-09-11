#ifndef MIDLESS_ENTITY_PERSISTENCE_H
#define MIDLESS_ENTITY_PERSISTENCE_H
#include "chunk/chunk.h"
#include "binarydata.h"
#include "../entity.h"
bool EntityPersistence_Encode(Chunk *chunk, BinaryWriter *out);
bool EntityPersistence_IsSaving(Entity *entity);
void EntityPersistence_EnsureChunks(void);
bool EntityPersistence_Activate(Chunk *chunk);
bool EntityPersistence_Save(Chunk *chunk);
void EntityPersistence_Unload(Chunk *chunk);
#endif
