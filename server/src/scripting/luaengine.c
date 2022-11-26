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
int Lua_running = 0;

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
        Lua_running = 1;
    }

    if (error) {
        fprintf(stderr, "%s \n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void Lua_Stop(void) {
    if(L == NULL) return;
    lua_close(L);
    Lua_running = 0;
}

int Lua_Ref(int table) {
    return luaL_ref(L, table);
}

int Lua_GetRegistryIndex() {
    return LUA_REGISTRYINDEX;
}

int Lua_GetRawI(int table, int index) {
    return lua_rawgeti(L, table, index);
}

int Lua_GetInt(int arg) {
    return luaL_checkinteger(L, arg);
    
}

float Lua_GetNumber(int arg) {
    return luaL_checknumber(L, arg);
}

int Lua_GetTop() {
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

void Lua_PushNumber(int number) {
    lua_pushnumber(L, number);
}

void Lua_PushString(const char *string) {
    lua_pushstring(L, string);
}