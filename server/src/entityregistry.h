/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_ENTITY_REGISTRY_H
#define MIDLESS_ENTITY_REGISTRY_H
#include "entity.h"

typedef struct EntityDefinition {
    EntityBody body;
    char name[65];
    char texture[65];
    int model, metadata;
    unsigned short hp;
    bool save;
    char group[65];
    float despawnDistance, despawnDelay;
} EntityDefinition;
const char *ServerEntities_Name(int definition);
int ServerEntities_Find(const char *name);
int ServerEntities_MetadataSchema(int definition);
bool ServerEntities_ShouldSave(int definition);
EntityBody ServerEntities_Body(int definition);
const char *ServerEntities_Group(int definition);
bool ServerEntities_Despawn(Entity *entity, float dt, float nearestDistanceSquared);
const EntityDefinition *ServerEntities_Get(int id);
int ServerEntities_Count(void);
bool ServerEntities_Register(const EntityDefinition *definition);
void ServerEntities_Reset(void);
int ServerEntities_Create(int definition, Vector3 position, bool validateBody);
typedef struct EntityDamageHooks {
    int (*adjust)(Entity *entity, int amount, void *context);
    void (*death)(Entity *entity, void *context);
    void *context;
} EntityDamageHooks;
int ServerEntities_Damage(Entity *entity, int amount, Vector3 impulse,
                          const EntityDamageHooks *hooks);
#endif
