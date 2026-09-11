#ifndef MIDLESS_LUA_QUERIES_H
#define MIDLESS_LUA_QUERIES_H
#include "../player.h"
int LuaQueries_Raycast(void);
int LuaQueries_Entities(void);
int LuaQueries_Players(void);
int LuaQueries_Light(void);
int LuaQueries_NearestPlayer(void);
int LuaQueries_FindPath(void);
int LuaQueries_CanWalk(void);
int LuaQueries_RegisterAttack(void);
void LuaQueries_Attack(Player *player);
void LuaQueries_Reset(void);
#endif
