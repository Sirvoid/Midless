/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef S_LUAD_H
#define S_LUAD_H


void LuaDefinition_Init(void);
void LD_OnBlockUpdateCall(Vector3 position, unsigned short blockID, unsigned short prevBlockID);
void LD_OnChatMessageCall(const char *name, const char *message);

#endif