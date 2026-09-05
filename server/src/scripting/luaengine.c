/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define LUA_IMPL
#include "minilua.h"
#include "pthread.h"

lua_State *L;
int luaRunning = 0;

void Lua_DefineLib(char* name, const void *functions) {
    lua_newtable(L);
    luaL_setfuncs(L, functions, 0);
    lua_setglobal(L, name);
}

void Lua_PushFunc(void *function) {
    lua_pushcfunction(L, function);
}

void Lua_MakeTable(int fields) {
    lua_createtable(L, 0, fields);
}

void Lua_SetField(int idx, const char* name) {
    lua_setfield(L, idx, name);
}

void Lua_SetGlobal(const char* name) {
    lua_setglobal(L, name);
}

void Lua_DefineGlobalFunc(char* name, void *function) {
    lua_pushcfunction(L, function);
    lua_setglobal(L, name);
}

int Lua_GetGlobal(char* name) {
    return lua_getglobal(L, name);
}  

void Lua_GetField(char* name) {
    lua_getfield(L, -1, name);
}

void Lua_CallFunc(int arguments, int results) {
    if(lua_pcall(L, arguments, results, 0) != 0) {
        printf("error running function `f': %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void Lua_Init(void) {
    L = luaL_newstate();
}

void Lua_Run(void) {

    int error = 0;
    if(L != NULL) {
        luaL_openlibs(L);
        error = luaL_dofile(L, "mod.lua");
        luaRunning = 1;
    }

    if (error) {
        fprintf(stderr, "%s \n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void Lua_Stop(void) {
    if(L == NULL) return;
    lua_close(L);
    L = NULL;
    luaRunning = 0;
}

int Lua_Ref(int table) {
    return luaL_ref(L, table);
}

int Lua_GetRegistryIndex(void) {
    return LUA_REGISTRYINDEX;
}

int Lua_GetRawI(int table, int index) {
    return lua_rawgeti(L, table, index);
}

void Lua_SetRawI(int table, int index) {
    lua_rawseti(L, table, index);
}

int Lua_GetInt(int arg) {
    return luaL_checkinteger(L, arg);
    
}

float Lua_GetNumber(int arg) {
    return luaL_checknumber(L, arg);
}

int Lua_GetTop(void) {
    return lua_gettop(L);
}

const char* Lua_GetString(int arg) {
    return luaL_checkstring(L, arg);
}

void Lua_PushValue(int idx) {
    lua_pushvalue(L, idx);
}

void Lua_PushInt(int integer) {
    lua_pushinteger(L, integer);
}

void Lua_PushNumber(double number) {
    lua_pushnumber(L, number);
}

void Lua_PushString(const char *string) {
    lua_pushstring(L, string);
}

int Lua_RefFunction(int arg) {
    luaL_checktype(L, arg, LUA_TFUNCTION);
    lua_pushvalue(L, arg);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

int Lua_GetIntRange(int arg, int min, int max) {
    lua_Integer value = luaL_checkinteger(L, arg);
    if (value < min || value > max) {
        luaL_argerror(L, arg, "integer out of range");
    }
    return (int)value;
}

void Lua_CopyString(int arg, char *destination, int capacity) {
    size_t length;
    const char *value = luaL_checklstring(L, arg, &length);
    if (length == 0 || length >= (size_t)capacity || memchr(value, 0, length)) {
        luaL_argerror(L, arg, "expected a nonempty string that fits the destination, without embedded NUL bytes");
    }
    memcpy(destination, value, length);
    destination[length] = 0;
}

int Lua_Error(const char *message) {
    return luaL_error(L, "%s", message);
}

void Lua_CheckTable(int arg) { luaL_checktype(L, arg, LUA_TTABLE); }
int Lua_PushField(int table, const char *name) {
    lua_getfield(L, table, name);
    return !lua_isnil(L, -1);
}
void Lua_Pop(void) { lua_pop(L, 1); }

void Lua_DefineObjectType(const char *name, const void *methods) {
    luaL_newmetatable(L, name);
    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, name);
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
}

void *Lua_NewObject(const char *name, size_t size) {
    void *object = lua_newuserdata(L, size);
    luaL_setmetatable(L, name);
    return object;
}

void *Lua_CheckObject(int arg, const char *name) {
    return luaL_checkudata(L, arg, name);
}
