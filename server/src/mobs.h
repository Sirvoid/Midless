#ifndef MIDLESS_MOBS_H
#define MIDLESS_MOBS_H
#include "entity.h"

typedef struct MobState {
    float brainElapsed, cooldown, retry;
    Vector3 goal;
    bool hasGoal, attack, wasRecovering, initialized;
    int targetId;
    uint64_t targetGeneration;
    Vector3 path[512], pathGoal, waypoint;
    int pathCount, pathIndex;
    float replan, lookAhead;
    bool hasWaypoint;
    Vector3 wanderGoal;
    float wanderTime, wanderWait;
} MobState;

MobState *ServerMobs_State(Entity *entity);
void ServerMobs_Detach(Entity *entity);
void ServerMobs_BeginTick(void);
void ServerMobs_Tick(Entity *entity, float dt);
Entity *ServerMobs_Target(MobState *state);
bool ServerMobs_Follow(Entity *entity, const Vector3 *goal, float speed, float acceleration,
                       float jump);
bool ServerMobs_Wander(Entity *entity, float radius, Vector3 *goal);
typedef struct MobDefinition {
    bool registered, lineOfSight, moveDuringRecovery;
    float interval, range, cooldown;
} MobDefinition;
void ServerMobs_Define(int id, const MobDefinition *definition);
bool ServerMobs_IsDefined(int id);
void ServerMobs_ResetDefinitions(void);
void ServerMobs_Update(Entity *entity, float dt);
#endif
