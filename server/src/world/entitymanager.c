#include "entitypersistence.h"
#include "world.h"
#include "../spawnmanager.h"
#include "../entityphysics.h"
#include "../mobs.h"
#include "../entitytexture.h"
#include "../droppeditems.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../textcolors.h"
#include "scripthooks.h"
#include "entityregistry.h"

static uint64_t nextGeneration;
static bool shuttingDown;

void ServerWorld_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    if (!serverWorld.entities || id < 0 || id >= WORLD_MAX_ENTITIES) return;
    Entity *e = &serverWorld.entities[id];
    if (!e->active || e->pendingRemoval) return;
    if (e->mob) {
        e->mob->pathCount = 0; e->mob->hasWaypoint = false;
        e->mob->lookAhead = e->mob->replan = 0;
    }
    if (e->ownerPlayerId < 0 && e->body.enabled) {
        e->body.velocity = (Vector3){0};
        e->body.sleeping = e->body.grounded = e->body.blockedByUnloaded = false;
        e->recovering = e->moveEnabled = false;
    }
    ServerPhysics_InvalidateIndex();
    e->position = position;
    e->rotation = rotation;
    e->dirty = true;
}

int ServerWorld_AddEntity(int type, int model, Vector3 position, int ownerPlayerId) {
    if (shuttingDown || !serverWorld.entities) return -1;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (e->active) continue;
        *e = (Entity){.id = id, .generation = ++nextGeneration, .active = true,
            .ownerPlayerId = ownerPlayerId, .definitionId = -1,
            .type = type, .model = model, .position = position};
        e->body = EntityBody_Default();
        e->nametag = (Nametag){.color = WHITE, .visible = true};
        ServerPhysics_InvalidateIndex();
        return id;
    }
    return -1;
}

void ServerWorld_RemoveEntity(int id) {
    if (serverWorld.entities && id >= 0 && id < WORLD_MAX_ENTITIES && serverWorld.entities[id].active)
        serverWorld.entities[id].pendingRemoval = true;
}

static void Destroy(Entity *e) {
    ScriptHooks_EntitiesRemove(e);
    if (e->type == ENTITY_TYPE_DROPPED_ITEM) ServerDrops_Remove(e);
    else if (e->announced) ServerWorld_BroadcastExcluding(ServerPacket_CreateDespawnEntity(e), e->ownerPlayerId);
    ServerPhysics_InvalidateIndex();
    Metadata_Free(&e->metadata);
    e->active = false;
    e->type = 0;
}

void ServerEntities_Update(float dt) {
    ServerSpawning_Update(dt);
    uint64_t cutoff = nextGeneration;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (e->active && !e->pendingRemoval && e->generation <= cutoff && !EntityPersistence_IsSaving(e)) ScriptHooks_EntitiesStep(e, dt);
    }
    ServerPhysics_Update(dt);
    ServerDrops_Update(dt);
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (!e->active) continue;
        if (e->pendingRemoval) { Destroy(e); continue; }
        if (e->type == ENTITY_TYPE_DROPPED_ITEM) {
            ServerDrops_Replicate(e);
            continue;
        }
        if (!e->announced) {
            ServerWorld_BroadcastExcluding(ServerPacket_CreateSpawnEntity(e), e->ownerPlayerId);
            e->announced = true;
            e->nametagDirty = true;
            e->dirty = true;
        }
        if (e->dirty) {
            ServerWorld_BroadcastExcluding(ServerPacket_CreateTeleportEntity(e, e->position, e->rotation), e->ownerPlayerId);
            e->dirty = false;
        }
        if (e->nametagDirty) {
            ServerWorld_BroadcastExcluding(ServerNametag_CreatePacket(e), e->ownerPlayerId);
            e->nametagDirty = false;
        }
        if (e->textureDirty) {
            for (int p=0; p<WORLD_MAX_PLAYERS; p++) {
                Player *recipient = serverWorld.players[p];
                if (recipient && !recipient->disconnected)
                    ServerNetwork_Send(recipient,ServerEntityTexture_Packet(e,recipient->entityId));
            }
            e->textureDirty = false;
        }
    }
}

void ServerEntities_Send(Player *player) {
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (!e->active || e->pendingRemoval || e->ownerPlayerId == player->id || !e->announced ||
            e->type == ENTITY_TYPE_DROPPED_ITEM) continue;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(e));
        ServerNetwork_Send(player, ServerNametag_CreatePacket(e));
        ServerNetwork_Send(player, ServerPacket_CreateTeleportEntity(e, e->position, e->rotation));
    }
}

void ServerEntities_Shutdown(void) {
    shuttingDown = true;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (e->active) { e->pendingRemoval = true; Destroy(e); }
    }
    ServerPhysics_Reset();
    ServerDrops_Reset();
    shuttingDown = false;
}
