#ifndef MIDLESS_SERVER_METADATA_H
#define MIDLESS_SERVER_METADATA_H
#include "world/chunk/chunkmetadata.h"
#include "inventory.h"
#include "raylib.h"

bool ServerMetadata_StateField(int id, const char *name, bool *boolean, int *bits);
bool ServerMetadata_StateValue(int id, const Metadata *value, const char *name, int64_t *out);
bool ServerMetadata_Validate(int schema, const Metadata *value);
bool ServerMetadata_CanRead(int schema, const Metadata *value);
bool ServerMetadata_ValidateChunk(struct Chunk *chunk);
// Collect occupied stacks from every inventory field. -1 leaves the block intact.
int ServerMetadata_CollectBlockItems(Vector3 position, ItemStack *stacks, int capacity);
bool ServerMetadata_BlockInventory(Vector3 position, const char *field, ItemStack *slots, int count,
                                   bool write);
bool ServerMetadata_Progress(Vector3 position, const char *field, float *value);
#endif
