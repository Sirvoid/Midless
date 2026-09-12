/**
 * Copyright (c) 2021 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define LUA_IMPL
#include "minilua.h"
#include "pthread.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
bool Worldgen_Freeze(void);

lua_State *L;
int luaRunning = 0;

void Lua_DefineLib(char *name, const void *functions) {
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

void Lua_SetField(int idx, const char *name) {
    lua_setfield(L, idx, name);
}

void Lua_SetGlobal(const char *name) {
    lua_setglobal(L, name);
}

void Lua_DefineGlobalFunc(char *name, void *function) {
    lua_pushcfunction(L, function);
    lua_setglobal(L, name);
}

int Lua_GetGlobal(char *name) {
    return lua_getglobal(L, name);
}

void Lua_GetField(char *name) {
    lua_getfield(L, -1, name);
}

void Lua_CallFunc(int arguments, int results) {
    if (lua_pcall(L, arguments, results, 0) != 0) {
        printf("error running function `f': %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void ScriptRuntime_Init(void) {
    L = luaL_newstate();
}

static int CompareModNames(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static bool Lua_LoadModFolder(const char *folder) {
    DIR *dir = opendir(folder);

    if (dir == NULL) {
        printf("Could not open mod folder: %s\n", folder);
        return true;
    }

    struct dirent *entry;
    char **names = NULL;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        size_t length = strlen(entry->d_name);

        if (entry->d_name[0] == '.')
            continue;
        char candidate[512];
        struct stat info;
        int written = snprintf(candidate, sizeof(candidate), "%s/%s", folder, entry->d_name);
        if (written < 0 || written >= sizeof(candidate) || stat(candidate, &info))
            continue;
        if (S_ISDIR(info.st_mode)) {
            written =
                snprintf(candidate, sizeof(candidate), "%s/%s/init.lua", folder, entry->d_name);
            if (written < 0 || written >= sizeof(candidate) || stat(candidate, &info) ||
                !S_ISREG(info.st_mode))
                continue;
        } else if (!S_ISREG(info.st_mode) || length < 4 ||
                   strcmp(entry->d_name + length - 4, ".lua"))
            continue;

        char **grown = realloc(names, (count + 1) * sizeof(*names));
        if (!grown) {
            for (int i = 0; i < count; i++)
                free(names[i]);
            free(names);
            closedir(dir);
            return false;
        }
        names = grown;
        names[count] = malloc(length + 1);
        if (!names[count]) {
            for (int i = 0; i < count; i++)
                free(names[i]);
            free(names);
            closedir(dir);
            return false;
        }
        memcpy(names[count++], entry->d_name, length + 1);
    }
    closedir(dir);
    if (count > 1)
        qsort(names, count, sizeof(*names), CompareModNames);
    bool success = true;
    for (int i = 0; i < count; i++) {
        char path[512];

        snprintf(path, sizeof(path), "%s/%s", folder, names[i]);

        struct stat info;
        bool directory = stat(path, &info) == 0 && S_ISDIR(info.st_mode);
        char script[512];
        int written = snprintf(script, sizeof(script), directory ? "%s/init.lua" : "%s", path);
        int top = lua_gettop(L);
        printf("Loading %s\n", script);
        if (written < 0 || written >= sizeof(script))
            success = false;
        else {
            int error = luaL_loadfile(L, script);
            if (!error) {
                if (directory)
                    lua_pushstring(L, path);
                error = lua_pcall(L, directory ? 1 : 0, 0, 0);
            }
            if (error) {
                success = false;
                printf("Lua error in %s: %s\n", script, lua_tostring(L, -1));
            }
        }
        lua_settop(L, top);
        free(names[i]);
    }
    free(names);
    return success;
}

bool ScriptRuntime_Run(void) {

    int error = 0;
    if (L != NULL) {
        luaL_openlibs(L);
        if (!Lua_LoadModFolder("mods") || !Worldgen_Freeze())
            return false;
        luaRunning = 1;
    }

    if (error) {
        fprintf(stderr, "%s \n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return L != NULL && !error;
}

void ScriptRuntime_Stop(void) {
    if (L == NULL)
        return;
    lua_close(L);
    L = NULL;
    luaRunning = 0;
}

int Lua_Ref(int table) {
    return luaL_ref(L, table);
}

bool Lua_CallFuncHandled(int arguments) {
    if (lua_pcall(L, arguments, 1, 0) != LUA_OK) {
        printf("error running callback: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    bool handled = lua_isboolean(L, -1) && lua_toboolean(L, -1);
    lua_pop(L, 1);
    return handled;
}

void Lua_Unref(int table, int reference) {
    luaL_unref(L, table, reference);
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

const char *Lua_GetString(int arg) {
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
        luaL_argerror(
            L, arg,
            "expected a nonempty string that fits the destination, without embedded NUL bytes");
    }
    memcpy(destination, value, length);
    destination[length] = 0;
}

int Lua_Error(const char *message) {
    return luaL_error(L, "%s", message);
}

void Lua_CheckTable(int arg) {
    luaL_checktype(L, arg, LUA_TTABLE);
}
int Lua_PushField(int table, const char *name) {
    lua_getfield(L, table, name);
    return !lua_isnil(L, -1);
}
void Lua_Pop(void) {
    lua_pop(L, 1);
}

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

int Lua_TableLength(int arg) {
    luaL_checktype(L, arg, LUA_TTABLE);
    return (int)lua_rawlen(L, arg);
}
int Lua_GetBoolean(int arg) {
    luaL_checktype(L, arg, LUA_TBOOLEAN);
    return lua_toboolean(L, arg);
}
