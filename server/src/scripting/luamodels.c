/**
 * Copyright (c) 2021 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luaengine.h"
#include "luavalues.h"
#include "../world/textures.h"
#include <math.h>
#include "minilua.h"
#include "luamodels.h"
#include "../world/world.h"
#include <string.h>
extern lua_State *L;

static const char *CheckName(int argument) {
    size_t length;
    const char *name = luaL_checklstring(L, argument, &length);
    if (!length || length > 64 || memchr(name, 0, length))
        luaL_argerror(L, argument, "model name must contain 1 to 64 bytes without NULs");
    return name;
}

int LuaModels_Resolve(int argument, bool defining) {
    if (lua_type(L, argument) == LUA_TNUMBER) {
        lua_Integer id = luaL_checkinteger(L, argument);
        if (id < (defining ? 1 : 0) || id > 255)
            luaL_argerror(L, argument, "model ID out of range");
        return (int)id;
    }
    const char *name = CheckName(argument);
    if (!strcmp(name, "humanoid")) {
        if (defining)
            luaL_argerror(L, argument, "humanoid is a reserved model name");
        return 0;
    }
    for (int id = 1; id < 256; id++)
        if (!strcmp(serverWorld.modelNames[id], name))
            return id;
    if (defining) {
        for (int id = 1; id < 256; id++)
            if (!serverWorld.modelNames[id][0] && !serverWorld.modelDefinitions[id])
                return id;
        return luaL_error(L, "entity model registry is full");
    }
    return luaL_error(L, "model name is not registered");
}

void LuaModels_BindName(int argument, int id) {
    if (lua_type(L, argument) == LUA_TSTRING)
        strcpy(serverWorld.modelNames[id], CheckName(argument));
}

static void ModelVector(int table, const char *name, int16_t values[3], bool optional) {
    if (!Lua_PushField(table, name) && optional) {
        Lua_Pop();
        return;
    }
    int vector = Lua_GetTop();
    if (Lua_TableLength(vector) != 3)
        Lua_Error("model vectors require exactly three coordinates");
    for (int i = 0; i < 3; i++) {
        Lua_GetRawI(vector, i + 1);
        float value = Lua_GetNumber(-1);
        if (!isfinite(value) || value < -512.0f || value > 511.984375f)
            Lua_Error("model coordinates must be finite and between -512 and 511.984375");
        values[i] = (int16_t)roundf(value * 64.0f);
        Lua_Pop();
    }
    Lua_Pop();
}

int LuaModels_Define(void) {
    int id = LuaModels_Resolve(1, true);
    Lua_CheckTable(2);
    ModelDefinition d = {0};
    lua_getfield(L, 2, "base");
    if (!lua_isnil(L, -1)) {
        int base = LuaModels_Resolve(-1, false);
        if (base && !serverWorld.modelDefinitions[base])
            return Lua_Error("base model is not defined");
        d = base ? *serverWorld.modelDefinitions[base] : ModelDefinition_Humanoid();
        lua_pop(L, 1);
        lua_getfield(L, 2, "parts");
        if (!lua_isnil(L, -1))
            return Lua_Error("model variants cannot also specify parts");
        lua_pop(L, 1);
        lua_getfield(L, 2, "texture");
        if (!lua_isnil(L, -1)) {
            int texture = ServerTextures_Find(luaL_checkstring(L, -1));
            if (texture < 0)
                return Lua_Error("texture is not defined");
            d.texture = texture;
        }
        lua_pop(L, 1);
        lua_getfield(L, 2, "name");
        if (!lua_isnil(L, -1))
            Lua_CopyString(-1, d.name, sizeof(d.name));
        lua_pop(L, 1);
        if (!ServerWorld_DefineEntityModel(id, &d))
            return Lua_Error("invalid model variant");
        LuaModels_BindName(1, id);
        return 0;
    }
    lua_pop(L, 1);
    Lua_PushField(2, "name");
    Lua_CopyString(-1, d.name, sizeof(d.name));
    Lua_Pop();
    Lua_PushField(2, "texture");
    const char *texture = Lua_GetString(-1);
    int textureId = ServerTextures_Find(texture);
    if (textureId < 0)
        return Lua_Error("texture is not defined");
    d.texture = textureId;
    Lua_Pop();
    Lua_PushField(2, "parts");
    int parts = Lua_GetTop();
    int count = Lua_TableLength(parts);
    if (count < 1 || count > ENTITY_MODEL_MAX_PARTS)
        return Lua_Error("models require 1 to 64 parts");
    d.partCount = count;
    static const char *faces[] = {"east", "west", "up", "down", "north", "south"};
    for (int i = 0; i < count; i++) {
        Lua_GetRawI(parts, i + 1);
        int part = Lua_GetTop();
        Lua_CheckTable(part);
        ModelPartDefinition *p = &d.parts[i];
        p->role = LuaValues_IntField(part, "role", 0, 0, 5);
        if (Lua_PushField(part, "first_person_visible"))
            p->firstPersonVisible = Lua_GetBoolean(-1);
        Lua_Pop();
        if (Lua_PushField(part, "grip"))
            p->hasGrip = true;
        Lua_Pop();
        if (p->hasGrip)
            ModelVector(part, "grip", p->grip, false);
        ModelVector(part, "position", p->position, true);
        ModelVector(part, "min", p->min, false);
        ModelVector(part, "max", p->max, false);
        Lua_PushField(part, "uv");
        int uv = Lua_GetTop();
        Lua_CheckTable(uv);
        for (int f = 0; f < 6; f++) {
            Lua_PushField(uv, faces[f]);
            int rectangle = Lua_GetTop();
            if (Lua_TableLength(rectangle) != 4)
                return Lua_Error("UV rectangles require x, y, width, height");
            for (int a = 0; a < 4; a++) {
                Lua_GetRawI(rectangle, a + 1);
                p->uv[f][a] = Lua_GetIntRange(-1, -32768, 32767);
                Lua_Pop();
            }
            Lua_Pop();
        }
        Lua_Pop();
        Lua_Pop();
    }
    Lua_Pop();
    if (!ServerWorld_DefineEntityModel(id, &d))
        return Lua_Error("invalid entity model or allocation failed");
    LuaModels_BindName(1, id);
    return 0;
}

int LuaModels_Remove(void) {
    int id = LuaModels_Resolve(1, false);
    if (id == 0)
        return Lua_Error("cannot remove the built-in humanoid model");
    ServerWorld_RemoveEntityModel(id);
    return 0;
}

int LuaModels_SetEntity(void) {
    int entityId = Lua_GetIntRange(1, 0, WORLD_MAX_ENTITIES - 1);
    int modelId = LuaModels_Resolve(2, false);
    if (!ServerWorld_SetEntityModel(entityId, modelId))
        return Lua_Error("entity or model is not defined");
    return 0;
}

void LuaModels_Init(void) {
    static const LuaConstant roles[] = {{"NONE", 0},     {"HEAD", 1},      {"RIGHT_ARM", 2},
                                        {"LEFT_ARM", 3}, {"RIGHT_LEG", 4}, {"LEFT_LEG", 5},
                                        {NULL, 0}};
    Lua_MakeTable(1);
    LuaValues_ConstantTable(roles);
    Lua_SetField(-2, "part");
    Lua_SetGlobal("model");
}
