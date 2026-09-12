/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "luatextcolors.h"
#include "../textcolors.h"
#include "textcolor.h"
#include <string.h>
#include <math.h>

extern lua_State *L;

static Color ReadColor(lua_State *state, int index) {
    size_t length;
    const char *text = luaL_checklstring(state, index, &length);
    if ((length != 7 && length != 9) || text[0] != '#')
        luaL_error(state, "color must be #RRGGBB or #RRGGBBAA");
    unsigned char values[4] = {0, 0, 0, 255};
    for (size_t i = 1; i < length; i += 2) {
        int value = 0;
        for (int j = 0; j < 2; j++) {
            char c = text[i + j];
            int digit = c >= '0' && c <= '9'   ? c - '0'
                        : c >= 'a' && c <= 'f' ? c - 'a' + 10
                        : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                               : -1;
            if (digit < 0)
                luaL_error(state, "invalid hex color");
            value = value * 16 + digit;
        }
        values[(i - 1) / 2] = value;
    }
    return (Color){values[0], values[1], values[2], values[3]};
}

static unsigned char ReadCode(void) {
    size_t length;
    const char *code = luaL_checklstring(L, 1, &length);
    if (length != 1 || !TextColor_ValidCode(*code))
        luaL_error(L, "invalid text color code");
    return *code;
}

int LuaTextColors_Define(void) {
    unsigned char code = ReadCode();
    ServerTextColors_Define(code, ReadColor(L, 2));
    return 0;
}

int LuaTextColors_Remove(void) {
    unsigned char code = ReadCode();
    ServerTextColors_Remove(code);
    return 0;
}

int LuaTextColors_Escape(void) {
    size_t length;
    const char *text = luaL_checklstring(L, 1, &length);
    if (memchr(text, 0, length))
        return luaL_error(L, "text cannot contain NUL");
    char *output = lua_newuserdata(L, TextColor_Escape(NULL, text) + 1);
    TextColor_Escape(output, text);
    lua_pushstring(L, output);
    return 1;
}

int LuaNametag_Set(lua_State *state, Entity *entity) {
    luaL_checktype(state, 2, LUA_TTABLE);
    Nametag tag = entity->nametag;
    lua_getfield(state, 2, "text");
    if (!lua_isnil(state, -1)) {
        size_t length;
        const char *text = luaL_checklstring(state, -1, &length);
        if (length >= NAMETAG_TEXT_SIZE || memchr(text, 0, length) || memchr(text, '\n', length) ||
            memchr(text, '\r', length))
            return luaL_error(state, "nametag must be a single line of at most 128 bytes");
        memset(tag.text, 0, sizeof(tag.text));
        memcpy(tag.text, text, length);
    }
    lua_pop(state, 1);
    lua_getfield(state, 2, "color");
    if (!lua_isnil(state, -1))
        tag.color = ReadColor(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, 2, "visible");
    if (!lua_isnil(state, -1)) {
        luaL_checktype(state, -1, LUA_TBOOLEAN);
        tag.visible = lua_toboolean(state, -1);
    }
    lua_pop(state, 1);
    lua_getfield(state, 2, "offset");
    if (!lua_isnil(state, -1)) {
        double offset = luaL_checknumber(state, -1);
        if (!isfinite(offset) || fabs(offset) > 16)
            return luaL_error(state, "nametag offset must be between -16 and 16 blocks");
        tag.offset = offset;
    }
    lua_pop(state, 1);
    ServerNametag_Set(entity, tag);
    return 0;
}
