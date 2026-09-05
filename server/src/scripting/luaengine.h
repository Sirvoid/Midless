/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_LUA_ENGINE_H
#define MIDLESS_SERVER_LUA_ENGINE_H

#include <stddef.h>

extern int luaRunning;
void Lua_DefineObjectType(const char *name, const void *methods);
void *Lua_NewObject(const char *name, size_t size);
void *Lua_CheckObject(int arg, const char *name);

void Lua_Init(void);
void Lua_MakeTable(int fields);
void Lua_SetField(int idx, const char* name);
void Lua_DefineLib(char* name, const void *functions);
void Lua_PushFunc(void *function);
void Lua_DefineGlobalFunc(char* name, void *function);
int Lua_GetGlobal(char* name);
void Lua_SetGlobal(const char* name);
void Lua_GetField(char* name);
void Lua_CallFunc(int arguments, int results);
void Lua_Run(void);
void Lua_Stop(void);

int Lua_Ref(int table);
int Lua_RefFunction(int arg);
int Lua_GetRegistryIndex(void);
int Lua_GetRawI(int table, int index);
void Lua_SetRawI(int table, int index);
int Lua_GetInt(int arg);
void Lua_CheckTable(int arg);
int Lua_TableLength(int arg);
int Lua_GetBoolean(int arg);
int Lua_PushField(int table, const char *name);
void Lua_Pop(void);
int Lua_GetIntRange(int arg, int min, int max);
void Lua_CopyString(int arg, char *destination, int capacity);
int Lua_Error(const char *message);
int Lua_GetTop();
float Lua_GetNumber(int arg);
const char* Lua_GetString(int arg);

void Lua_PushValue(int idx);
void Lua_PushInt(int integer);
void Lua_PushNumber(double number);
void Lua_PushString(const char *string);

#endif
