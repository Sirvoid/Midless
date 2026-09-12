/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SCRIPT_HOOKS_H
#define MIDLESS_SCRIPT_HOOKS_H
#include "player.h"
#include "entity.h"
#include "inventoryprotocol.h"
#include "spawnmanager.h"

// The server calls this interface with C values. Lua objects and references stay in scripting.
void ScriptHooks_Init(void);
void ScriptHooks_Shutdown(void);
void ScriptHooks_Ready(void);
void ScriptHooks_Step(float delta);
void ScriptHooks_PlayerJoin(int playerId);
void ScriptHooks_PlayerLeave(int playerId);
void ScriptHooks_PlayerLand(int playerId, float distance);
void ScriptHooks_PlayerClick(int playerId, int button);
void ScriptHooks_BlockUpdate(Vector3 position, unsigned short blockId,
                             unsigned short previousBlockId);
bool ScriptHooks_ChatMessage(int playerId, const char *message);
bool ScriptHooks_InteractBlock(struct Player *player, Vector3 position, int blockId);
void ScriptHooks_EntitiesStep(Entity *entity, float dt);
void ScriptHooks_EntitiesRemove(Entity *entity);
void ScriptHooks_EntitiesLoaded(Entity *entity);
void ScriptHooks_EntitiesDetach(Entity *entity);
void ScriptHooks_EntitiesUnload(Entity *entity);
int ScriptHooks_EntitiesRestore(int definition, Vector3 position);
bool ScriptHooks_EntitiesTrySpawn(int definition, Vector3 position);
bool ScriptHooks_MetadataCanInsert(Vector3 position, const char *field, int slot, int item);
bool ScriptHooks_MetadataTimer(Vector3 position, float dt);
void ScriptHooks_MetadataInventoryChanged(Vector3 position, const char *field);
void ScriptHooks_MetadataFlushChanges(void);
bool ScriptHooks_MobBrain(Entity *entity, float elapsed);
bool ScriptHooks_MobMovement(Entity *entity, float dt);
bool ScriptHooks_MobAttack(Entity *entity, bool *performed);
int ScriptHooks_EntityHealth(Entity *entity);
double ScriptHooks_DiggingTime(Player *player, Vector3 position, ItemStack stack, double seconds);
double ScriptHooks_DiggingSpeed(ItemStack stack, const char *group);
void ScriptHooks_DiggingFinished(Player *player, Vector3 position, ItemStack stack);
bool ScriptHooks_ItemActionsUse(Player *player, const InventoryAction *block, Entity *entity);
void ScriptHooks_ItemActionsPlaced(Player *player, Vector3 position, int blockId);
int ScriptHooks_ItemActionsDrops(Player *player, Vector3 position, int block, ItemStack tool,
                                 ItemStack *stacks, int capacity);
bool ScriptHooks_InventoryOpenPlayer(Player *player);
void ScriptHooks_QueriesAttack(Player *player);
void ScriptRuntime_Init(void);
bool ScriptRuntime_Run(void);
void ScriptRuntime_Stop(void);
bool ScriptHooks_SpawnFilter(int id, const SpawnRule *rule, Vector3 position, int playerId);
void ScriptHooks_ResetSpawnFilters(void);

#endif
