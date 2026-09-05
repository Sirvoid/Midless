#include "world.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../scripting/luaentities.h"

static uint64_t nextGeneration;
static bool shuttingDown;

void ServerWorld_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    if (!serverWorld.entities || id < 0 || id >= WORLD_MAX_ENTITIES) return;
    Entity *e = &serverWorld.entities[id];
    if (!e->active || e->pendingRemoval) return;
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
            .ownerPlayerId = ownerPlayerId, .definitionId = -1, .scriptRef = -2,
            .type = type, .model = model, .position = position};
        return id;
    }
    return -1;
}

void ServerWorld_RemoveEntity(int id) {
    if (serverWorld.entities && id >= 0 && id < WORLD_MAX_ENTITIES && serverWorld.entities[id].active)
        serverWorld.entities[id].pendingRemoval = true;
}

static void Destroy(Entity *e) {
    LuaEntities_Remove(e);
    if (e->announced) ServerWorld_BroadcastExcluding(ServerPacket_CreateDespawnEntity(e), e->ownerPlayerId);
    e->active = false;
    e->type = 0;
}

void ServerEntities_Update(float dt) {
    uint64_t cutoff = nextGeneration;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (e->active && !e->pendingRemoval && e->generation <= cutoff) LuaEntities_Step(e, dt);
    }
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (!e->active) continue;
        if (e->pendingRemoval) { Destroy(e); continue; }
        if (!e->announced) {
            ServerWorld_BroadcastExcluding(ServerPacket_CreateSpawnEntity(e), e->ownerPlayerId);
            e->announced = true;
            e->dirty = true;
        }
        if (e->dirty) {
            ServerWorld_BroadcastExcluding(ServerPacket_CreateTeleportEntity(e, e->position, e->rotation), e->ownerPlayerId);
            e->dirty = false;
        }
    }
}

void ServerEntities_Send(Player *player) {
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (!e->active || e->pendingRemoval || e->ownerPlayerId == player->id || !e->announced) continue;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(e));
        ServerNetwork_Send(player, ServerPacket_CreateTeleportEntity(e, e->position, e->rotation));
    }
}

void ServerEntities_Shutdown(void) {
    shuttingDown = true;
    for (int id = 0; id < WORLD_MAX_ENTITIES; id++) {
        Entity *e = &serverWorld.entities[id];
        if (e->active) { e->pendingRemoval = true; Destroy(e); }
    }
    shuttingDown = false;
}
