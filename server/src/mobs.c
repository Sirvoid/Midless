#include "scripthooks.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "mobs.h"
#include "entityphysics.h"
#include "worldquery.h"
#include "world/world.h"

static int pathBudget;
static float Distance(Vector3 a, Vector3 b) {
    return hypotf(a.x - b.x, a.z - b.z);
}
static float RandomUnit(void) {
    return (float)rand() / RAND_MAX;
}
MobState *ServerMobs_State(Entity *e) {
    if (!e->mob) {
        e->mob = calloc(1, sizeof(*e->mob));
        if (e->mob) {
            e->mob->targetId = -1;
            e->mob->wanderWait = 2 + RandomUnit() * 3;
        }
    }
    return e->mob;
}
void ServerMobs_Detach(Entity *e) {
    free(e->mob);
    e->mob = NULL;
}
void ServerMobs_BeginTick(void) {
    pathBudget = 2;
}
void ServerMobs_Tick(Entity *e, float dt) {
    MobState *s = e->mob;
    if (!s)
        return;
    s->brainElapsed += dt;
    s->cooldown = fmaxf(0, s->cooldown - dt);
    s->retry = fmaxf(0, s->retry - dt);
    s->replan -= dt;
    s->lookAhead -= dt;
    s->wanderTime = fmaxf(0, s->wanderTime - dt);
    s->wanderWait = fmaxf(0, s->wanderWait - dt);
}
Entity *ServerMobs_Target(MobState *s) {
    if (!serverWorld.entities || s->targetId < 0 || s->targetId >= WORLD_MAX_ENTITIES)
        return NULL;
    Entity *e = &serverWorld.entities[s->targetId];
    if (!e->active || e->pendingRemoval || e->dead || e->generation != s->targetGeneration)
        return NULL;
    if (e->ownerPlayerId >= 0) {
        Player *p = serverWorld.players ? serverWorld.players[e->ownerPlayerId] : NULL;
        if (!p || p->disconnected || !p->movementReady || p->entityId != e->id)
            return NULL;
    }
    return e;
}
bool ServerMobs_Follow(Entity *e, const Vector3 *goal, float speed, float acceleration,
                       float jump) {
    MobState *s = ServerMobs_State(e);
    if (!s)
        return false;
    Vector3 pos = e->position;
    if (!goal) {
        s->pathCount = 0;
        s->hasWaypoint = false;
        s->replan = s->lookAhead = 0;
        ServerPhysics_Move(e, (Vector3){0}, 0, acceleration);
        return false;
    }
    if (Distance(s->pathGoal, *goal) > 1 || fabsf(s->pathGoal.y - goal->y) > 0.1f) {
        s->replan = s->lookAhead = 0;
    }
    if (s->lookAhead <= 0) {
        s->lookAhead = 0.2f;
        s->pathGoal = *goal;
        s->hasWaypoint = false;
        if (ServerQuery_CanWalk(e->body, pos, *goal)) {
            s->pathCount = 0;
            s->replan = 0;
            if (Distance(pos, *goal) > 0.3f) {
                s->waypoint = *goal;
                s->hasWaypoint = true;
            }
        } else {
            if (s->replan <= 0 && pathBudget > 0 && e->body.grounded) {
                pathBudget--;
                s->replan = 1;
                Vector3 destination = *goal;
                float distance = Distance(pos, *goal);
                if (distance > 24)
                    destination = (Vector3){pos.x + (goal->x - pos.x) * 16 / distance, pos.y,
                                            pos.z + (goal->z - pos.z) * 16 / distance};
                s->pathCount = ServerQuery_FindPath(e->body, pos, destination, s->path, 512);
                s->pathIndex = 1;
            }
            while (s->pathIndex < s->pathCount && Distance(pos, s->path[s->pathIndex]) <= 0.3f &&
                   fabsf(pos.y - s->path[s->pathIndex].y) <= 0.35f)
                s->pathIndex++;
            if (s->pathIndex < s->pathCount) {
                int last = s->pathIndex;
                while (last + 1 < s->pathCount && last < s->pathIndex + 8 &&
                       fabsf(s->path[last].y - pos.y) <= 0.001f &&
                       fabsf(s->path[last + 1].y - pos.y) <= 0.001f)
                    last++;
                for (int i = last; i > s->pathIndex; i--)
                    if (ServerQuery_CanWalk(e->body, pos, s->path[i])) {
                        s->pathIndex = i;
                        break;
                    }
                s->waypoint = s->path[s->pathIndex];
                s->hasWaypoint = true;
            }
        }
    }
    if (!s->hasWaypoint) {
        ServerPhysics_Move(e, (Vector3){0}, 0, acceleration);
        return false;
    }
    Vector3 direction = {s->waypoint.x - pos.x, 0, s->waypoint.z - pos.z};
    if (Distance(pos, s->waypoint) <= 0.3f && fabsf(pos.y - s->waypoint.y) <= 0.35f) {
        s->lookAhead = 0;
        ServerPhysics_Move(e, (Vector3){0}, 0, acceleration);
        return true;
    }
    e->rotation.y = atan2f(direction.x, direction.z);
    e->dirty = true;
    ServerPhysics_Move(e, direction, speed, acceleration);
    if (s->waypoint.y > pos.y + 0.15f && jump > 0)
        ServerPhysics_Jump(e, jump);
    return true;
}
bool ServerMobs_Wander(Entity *e, float radius, Vector3 *goal) {
    MobState *s = ServerMobs_State(e);
    if (!s)
        return false;
    if (s->wanderTime > 0 && Distance(e->position, s->wanderGoal) > 0.4f) {
        *goal = s->wanderGoal;
        return true;
    }
    if (s->wanderTime > 0)
        s->wanderTime = 0;
    if (s->wanderWait > 0 || !e->body.grounded)
        return false;
    float angle = RandomUnit() * 6.2831853f, distance = radius * (0.5f + RandomUnit() * 0.5f);
    s->wanderGoal = (Vector3){e->position.x + cosf(angle) * distance, e->position.y,
                              e->position.z + sinf(angle) * distance};
    s->wanderTime = 6;
    s->wanderWait = 8 + RandomUnit() * 3;
    *goal = s->wanderGoal;
    return true;
}

static MobDefinition definitions[256];

void ServerMobs_Define(int id, const MobDefinition *definition) {
    if (id >= 0 && id < 256)
        definitions[id] = *definition;
}

bool ServerMobs_IsDefined(int id) {
    return id >= 0 && id < 256 && definitions[id].registered;
}

void ServerMobs_ResetDefinitions(void) {
    memset(definitions, 0, sizeof(definitions));
}

static bool Alive(Entity *entity) {
    return entity->active && !entity->pendingRemoval && !entity->dead;
}

void ServerMobs_Update(Entity *entity, float dt) {
    ServerMobs_Tick(entity, dt);
    if (!ServerMobs_IsDefined(entity->definitionId) || entity->dead)
        return;
    const MobDefinition *definition = &definitions[entity->definitionId];
    MobState *state = ServerMobs_State(entity);
    if (!state) {
        ServerWorld_RemoveEntity(entity->id);
        return;
    }
    if (state->wasRecovering && !entity->recovering)
        state->brainElapsed = definition->interval;
    state->wasRecovering = entity->recovering;
    if (!state->initialized || state->brainElapsed >= definition->interval) {
        state->initialized = true;
        float elapsed = state->brainElapsed;
        state->brainElapsed = 0;
        if (!ScriptHooks_MobBrain(entity, elapsed)) {
            ServerWorld_RemoveEntity(entity->id);
            return;
        }
        if (!Alive(entity))
            return;
    }
    if (entity->recovering && !definition->moveDuringRecovery) {
        state->pathCount = 0;
        state->hasWaypoint = false;
        state->replan = state->lookAhead = 0;
        entity->moveEnabled = false;
    } else {
        if (definition->moveDuringRecovery)
            entity->recovering = false;
        if (!ScriptHooks_MobMovement(entity, dt)) {
            ServerWorld_RemoveEntity(entity->id);
            return;
        }
        if (!Alive(entity))
            return;
    }
    Entity *target = ServerMobs_Target(state);
    if (!state->attack || !target || target == entity || state->cooldown > 0 || state->retry > 0)
        return;
    float x = target->position.x - entity->position.x;
    float y = target->position.y - entity->position.y;
    float z = target->position.z - entity->position.z;
    if (x * x + y * y + z * z > definition->range * definition->range)
        return;
    if (definition->lineOfSight) {
        Vector3 from = entity->position;
        Vector3 to = target->position;
        from.y += (entity->body.localBounds.min.y + entity->body.localBounds.max.y) * 0.5f;
        to.y += (target->body.localBounds.min.y + target->body.localBounds.max.y) * 0.5f;
        if (ServerQuery_Raycast(from, to, false, -1).type != HIT_NOTHING)
            return;
    }
    int hp = ScriptHooks_EntityHealth(target);
    if (hp < 0) {
        ServerWorld_RemoveEntity(entity->id);
        return;
    }
    if (hp <= 0)
        return;
    bool performed;
    if (!ScriptHooks_MobAttack(entity, &performed)) {
        ServerWorld_RemoveEntity(entity->id);
        return;
    }
    if (!Alive(entity))
        return;
    if (performed)
        state->cooldown = definition->cooldown;
    else
        state->retry = definition->interval;
}
