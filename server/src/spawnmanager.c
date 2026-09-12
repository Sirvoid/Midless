/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "spawnmanager.h"
#include "world/world.h"
#include "scripthooks.h"
#include "entityregistry.h"
#include "items.h"

static SpawnRule rules[SPAWN_RULE_LIMIT];
static int ruleCount, nextRule;

static Entity *PlayerEntity(int id) {
    Player *p = serverWorld.players[id];
    if (!p || p->disconnected || !p->movementReady || p->entityId < 0 ||
        p->entityId >= WORLD_MAX_ENTITIES)
        return NULL;
    Entity *e = &serverWorld.entities[p->entityId];
    return e->active && !e->pendingRemoval ? e : NULL;
}
static float DistanceSquared(Vector3 a, Vector3 b) {
    float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return x * x + y * y + z * z;
}
static float Nearest(Vector3 position) {
    float nearest = INFINITY;
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Entity *e = PlayerEntity(i);
        if (e)
            nearest = fminf(nearest, DistanceSquared(position, e->position));
    }
    return nearest;
}
static bool Allowed(const SpawnRule *r, Vector3 p) {
    float nearest = Nearest(p);
    if (nearest < r->minDistance * r->minDistance || nearest > r->maxDistance * r->maxDistance)
        return false;
    int local = 0, global = 0, free = 0;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (!e->active) {
            free++;
            continue;
        }
        if (e->pendingRemoval || e->definitionId < 0)
            continue;
        bool matches = r->group[0] ? !strcmp(r->group, ServerEntities_Group(e->definitionId))
                                   : e->definitionId == r->definition;
        if (!matches)
            continue;
        global++;
        if (DistanceSquared(e->position, p) <= r->localRadius * r->localRadius)
            local++;
    }
    return free > 0 && local < r->localLimit && global < r->globalLimit;
}
static double Random(void) {
    return (double)rand() / ((double)RAND_MAX + 1);
}
void ServerSpawning_Update(float dt) {
    if (!serverWorld.entities || !serverWorld.players || !isfinite(dt) || dt <= 0)
        return;
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *e = &serverWorld.entities[i];
        if (e->active && !e->pendingRemoval && e->definitionId >= 0 &&
            ServerEntities_Despawn(e, dt, Nearest(e->position)))
            ServerWorld_RemoveEntity(i);
    }
    int count = ruleCount; // Rules added by callbacks start on the next update.
    for (int i = 0; i < count; i++) {
        SpawnRule *r = &rules[i];
        r->elapsed += dt;
        if (r->elapsed < r->interval)
            continue;
        r->elapsed = fmod(r->elapsed, r->interval);
        for (int p = 0; p < WORLD_MAX_PLAYERS; p++)
            if (!r->remaining[p] && PlayerEntity(p))
                r->remaining[p] = r->attempts;
    }
    // At most four bounded column searches per tick, rotating rules and players.
    for (int budget = 0; budget < 4 && count; budget++) {
        SpawnRule *r = NULL;
        int playerId = -1;
        for (int n = 0; n < count && playerId < 0; n++) {
            r = &rules[nextRule++ % count];
            nextRule %= count;
            for (int p = 0; p < WORLD_MAX_PLAYERS; p++) {
                int id = r->nextPlayer++ % WORLD_MAX_PLAYERS;
                r->nextPlayer %= WORLD_MAX_PLAYERS;
                if (!r->remaining[id])
                    continue;
                if (!PlayerEntity(id)) {
                    r->remaining[id] = 0;
                    continue;
                }
                r->remaining[id]--;
                playerId = id;
                break;
            }
        }
        if (playerId < 0)
            break;
        Entity *player = PlayerEntity(playerId);
        double angle = Random() * 6.283185307179586;
        double radius =
            sqrt(r->minDistance * r->minDistance +
                 Random() * (r->maxDistance * r->maxDistance - r->minDistance * r->minDistance));
        Vector3 column = {player->position.x + cos(angle) * radius, player->position.y,
                          player->position.z + sin(angle) * radius};
        Vector3 position;
        EntityBody body = ServerEntities_Body(r->definition);
        if (!ServerSpawnPlacement_Find(r, body, column, &position) || !Allowed(r, position) ||
            Random() >= r->chance)
            continue;
        if (!ScriptHooks_SpawnFilter((int)(r - rules), r, position, playerId))
            continue;
        if (Allowed(r, position) && ServerSpawnPlacement_Valid(r, body, position))
            ScriptHooks_EntitiesTrySpawn(r->definition, position);
    }
}
void ServerSpawning_Reset(void) {
    ScriptHooks_ResetSpawnFilters();
    memset(rules, 0, sizeof rules);
    ruleCount = nextRule = 0;
}

const char *ServerSpawning_Register(const SpawnRule *rule, int *id) {
    if (ruleCount == SPAWN_RULE_LIMIT)
        return "spawn registry is full";
    for (int i = 0; i < ruleCount; i++) {
        const SpawnRule *other = &rules[i];
        if (!strcmp(rule->name, other->name))
            return "spawn rule already registered";
        bool samePopulation = rule->group[0]
                                  ? !strcmp(rule->group, other->group)
                                  : !other->group[0] && rule->definition == other->definition;
        if (samePopulation &&
            (rule->localLimit != other->localLimit || rule->globalLimit != other->globalLimit ||
             rule->localRadius != other->localRadius)) {
            return "spawn rules sharing a population must use identical caps and radius";
        }
    }
    *id = ruleCount;
    rules[ruleCount++] = *rule;
    return NULL;
}
