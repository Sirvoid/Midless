#ifndef MIDLESS_SPAWN_MANAGER_H
#define MIDLESS_SPAWN_MANAGER_H
#include "entity.h"
#define SPAWN_RULE_LIMIT 128
typedef struct SpawnRule {
    char name[65], group[65];
    int definition, attempts, localLimit, globalLimit, verticalRange;
    float interval, chance, minDistance, maxDistance, localRadius;
    bool groundBlocks[256], filterGround, avoidLiquids;
    double elapsed;
    int remaining[256], nextPlayer;
} SpawnRule;
const char *ServerSpawning_Register(const SpawnRule *rule, int *id);
void ServerSpawning_Update(float dt);
void ServerSpawning_Reset(void);
bool ServerSpawnPlacement_Clear(EntityBody body, Vector3 position, bool avoidLiquids);
bool ServerSpawnPlacement_Valid(const SpawnRule *rule, EntityBody body, Vector3 position);
bool ServerSpawnPlacement_Find(const SpawnRule *rule, EntityBody body, Vector3 column,
                               Vector3 *position);
#endif
