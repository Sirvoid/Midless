/**
 * Copyright (c) 2021 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luabindings.h"
#include "luaengine.h"
#include "luaplayers.h"
#include "luablocks.h"
#include "luachat.h"
#include "lualifecycle.h"
#include "luatextures.h"
#include "luamodels.h"
#include "luaspawning.h"
#include "luaitems.h"
#include "luaworldgen.h"
#include "luatextcolors.h"
#include "luacrafting.h"
#include "luahudbars.h"
#include "luaqueries.h"
#include "luamobs.h"
#include "luadamage.h"
#include "luaitemactions.h"
#include "luadigging.h"
#include "luaentities.h"
#include "luametadata.h"
#include "luainventory.h"
#include "luavector.h"
#include "../crafting.h"
#include "../hudbars.h"
#include "../textcolors.h"

static const struct LuaMethod midlessLib[] = {
    {"raycast", LuaQueries_Raycast},
    {"get_entities_in_radius", LuaQueries_Entities},
    {"get_players_in_radius", LuaQueries_Players},
    {"get_player_in_radius", LuaQueries_NearestPlayer},
    {"get_light", LuaQueries_Light},
    {"register_on_player_damage", LuaDamage_RegisterPlayer},
    {"find_path", LuaQueries_FindPath},
    {"register_mob", LuaMobs_Register},
    {"can_walk_to", LuaQueries_CanWalk},
    {"register_on_player_attack", LuaQueries_RegisterAttack},
    {"define_player_inventory", LuaInventory_Define},
    {"define_player_inventory_screen", LuaInventory_DefineScreen},
    {"define_recipe", LuaCrafting_Register},
    {"define_texture", LuaTextures_Define},
    {"define_hud_bar", LuaHudBars_Define},
    {"define_text_color", LuaTextColors_Define},
    {"remove_text_color", LuaTextColors_Remove},
    {"escape_text", LuaTextColors_Escape},
    {"remove_hud_bar", LuaHudBars_Remove},
    {"define_player_metadata", LuaMetadata_DefinePlayer},
    {"register_on_player_metadata_change", LuaMetadata_RegisterPlayerChange},
    {"register_on_hp_change", LuaMetadata_RegisterHPChange},
    {"register_on_dig_time", LuaDigging_Register},
    {"set_breaking_texture", LuaTextures_SetBreaking},
    {"set_terrain_texture", LuaTextures_SetTerrain},
    {"define_entity", LuaEntities_Register},
    {"spawn_entity", LuaEntities_Spawn},
    {"register_spawn", LuaSpawning_Register},
    {"get_player_by_id", LuaPlayers_GetById},
    {"get_player_by_name", LuaPlayers_GetByName},
    {"get_players", LuaPlayers_List},
    {"get_block", LuaMetadata_GetBlock},
    {"set_block", LuaBlocks_SetBlock},
    {"set_blocks", LuaBlocks_SetBlocks},
    {"define_block", LuaBlocks_DefineBlock},
    {"define_item", LuaItems_Define},
    {"define_entity_model", LuaModels_Define},
    {"remove_entity_model", LuaModels_Remove},
    {"set_entity_model", LuaModels_SetEntity},
    {"register_on_ready", LuaLifecycle_RegisterReady},
    {"register_on_step", LuaLifecycle_RegisterStep},
    {"register_on_player_message", LuaChat_RegisterMessage},
    {"register_on_player_click", LuaPlayers_RegisterClick},
    {"register_on_player_join", LuaPlayers_RegisterJoin},
    {"register_on_player_land", LuaPlayers_RegisterLand},
    {"register_on_player_leave", LuaPlayers_RegisterLeave},
    {"register_on_block_update", LuaBlocks_RegisterBlockUpdate},
    {"broadcast", LuaChat_Broadcast},
    {"sleep", LuaLifecycle_Sleep},
    {NULL, NULL}};

void ScriptHooks_Init(void) {
    LuaDigging_Init();
    LuaItemActions_Init();
    Crafting_Reset();
    LuaInventory_Init();
    LuaVector_Init();
    LuaMetadata_Init();
    LuaEntities_Init();
    LuaLifecycle_Init();
    LuaBlocks_Init();
    LuaModels_Init();
    LuaPlayers_Init();
    Lua_DefineLib("midless", midlessLib);
    LuaWorldgen_Init();
}

void ScriptHooks_Shutdown(void) {
    LuaDamage_Reset();
    LuaMobs_Reset();
    LuaQueries_Reset();
    ServerTextColors_Reset();
    ServerHudBars_Reset();
    LuaDigging_Shutdown();
    LuaItemActions_Shutdown();
    LuaInventory_Shutdown();
    Crafting_Reset();
    LuaBlocks_Shutdown();
    ServerSpawning_Reset();
    LuaEntities_Shutdown();
    LuaMetadata_Shutdown();
    LuaPlayers_Shutdown();
    LuaChat_Shutdown();
    LuaLifecycle_Shutdown();
}
