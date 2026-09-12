#include "entityregistry.h"
#include "entityphysics.h"
#include "world/world.h"
#include <string.h>

#define MAX_DEFINITIONS 256
static EntityDefinition definitions[MAX_DEFINITIONS];
static int definitionCount;

EntityBody ServerEntities_Body(int definition) {
    return definition >= 0 && definition < definitionCount ? definitions[definition].body
                                                           : EntityBody_Default();
}

const char *ServerEntities_Group(int definition) {
    return definition >= 0 && definition < definitionCount ? definitions[definition].group : "";
}

bool ServerEntities_Despawn(Entity *e, float dt, float nearestDistanceSquared) {
    if (e->definitionId < 0)
        return false;
    EntityDefinition *d = &definitions[e->definitionId];
    if (d->despawnDistance <= 0)
        return false;
    if (nearestDistanceSquared <= d->despawnDistance * d->despawnDistance)
        e->despawnElapsed = 0;
    else
        e->despawnElapsed += dt;
    return nearestDistanceSquared > d->despawnDistance * d->despawnDistance &&
           e->despawnElapsed >= d->despawnDelay;
}

const char *ServerEntities_Name(int definition) {
    return definition >= 0 && definition < definitionCount ? definitions[definition].name : NULL;
}

int ServerEntities_Find(const char *name) {
    for (int i = 0; i < definitionCount; i++)
        if (!strcmp(name, definitions[i].name))
            return i;
    return -1;
}

int ServerEntities_MetadataSchema(int definition) {
    return definition >= 0 && definition < definitionCount ? definitions[definition].metadata : -1;
}

bool ServerEntities_ShouldSave(int definition) {
    return definition >= 0 && definition < definitionCount && definitions[definition].save;
}

const EntityDefinition *ServerEntities_Get(int id) {
    return id >= 0 && id < definitionCount ? &definitions[id] : NULL;
}

int ServerEntities_Count(void) {
    return definitionCount;
}

bool ServerEntities_Register(const EntityDefinition *definition) {
    if (definitionCount == MAX_DEFINITIONS)
        return false;
    definitions[definitionCount++] = *definition;
    return true;
}

void ServerEntities_Reset(void) {
    definitionCount = 0;
}

int ServerEntities_Create(int definition, Vector3 position, bool validateBody) {
    const EntityDefinition *data = ServerEntities_Get(definition);
    if (!data || (data->model && !serverWorld.modelDefinitions[data->model]))
        return -1;
    int id = ServerWorld_AddEntity(2, data->model, position, -1);
    if (id < 0)
        return -1;
    Entity *entity = &serverWorld.entities[id];
    entity->definitionId = definition;
    strcpy(entity->texture, data->texture);
    entity->hp = entity->maxHp = data->hp;
    if (validateBody) {
        if (!ServerPhysics_SetBody(entity, data->body)) {
            entity->definitionId = -1;
            ServerWorld_RemoveEntity(id);
            return -1;
        }
    } else {
        entity->body = data->body;
    }
    return id;
}

int ServerEntities_Damage(Entity *entity, int amount, Vector3 impulse,
                          const EntityDamageHooks *hooks) {
    if (!entity->maxHp || entity->dead || entity->damageBusy || amount <= 0 || amount > 65535)
        return 0;
    entity->damageBusy = true;
    if (hooks && hooks->adjust)
        amount = hooks->adjust(entity, amount, hooks->context);
    if (entity->pendingRemoval || amount < 0 || amount > 65535)
        amount = 0;
    if (amount > entity->hp)
        amount = entity->hp;
    entity->hp -= amount;
    if (amount > 0)
        ServerPhysics_ApplyImpulse(entity, impulse);
    if (entity->hp == 0 && !entity->pendingRemoval) {
        entity->dead = true;
        if (hooks && hooks->death)
            hooks->death(entity, hooks->context);
        ServerWorld_RemoveEntity(entity->id);
    }
    entity->damageBusy = false;
    return amount;
}
