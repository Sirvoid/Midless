/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef ISLEFORGE_LUA_ATTACHMENTS_H
#define ISLEFORGE_LUA_ATTACHMENTS_H
#include "minilua.h"
int LuaAttachment_Attach(lua_State *state);
int LuaAttachment_Detach(lua_State *state);
int LuaAttachment_Get(lua_State *state);
int LuaAttachment_Children(lua_State *state);
int LuaAttachment_Controller(lua_State *state);
int LuaAttachment_SetController(lua_State *state);
int LuaAttachment_Controlled(lua_State *state);
#endif
