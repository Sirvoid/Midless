#include "luablockstates.h"
#include "luametadata.h"
#include "../blockstates.h"
#include <string.h>

static int Integer(lua_State *L, int t, const char *key, int fallback, int min, int max) {
    lua_getfield(L, t, key);
    lua_Integer n = fallback;
    if (!lua_isnil(L, -1)) {
        n = luaL_checkinteger(L, -1);
        if (n < min || n > max)
            luaL_error(L, "%s is out of range", key);
    }
    lua_pop(L, 1);
    return n;
}

static void Box(lua_State *L, int t, BlockBox *b) {
    const char *keys[] = {"min", "max"};
    for (int k = 0; k < 2; k++) {
        lua_getfield(L, t, keys[k]);
        luaL_checktype(L, -1, LUA_TTABLE);
        if (lua_rawlen(L, -1) != 3)
            luaL_error(L, "box vectors require three coordinates");
        for (int a = 0; a < 3; a++) {
            lua_rawgeti(L, -1, a + 1);
            lua_Integer n = luaL_checkinteger(L, -1);
            lua_pop(L, 1);
            if (n < 0 || n > 16)
                luaL_error(L, "box coordinates must be 0..16");
            (k ? b->max : b->min)[a] = n;
        }
        lua_pop(L, 1);
    }
    for (int a = 0; a < 3; a++)
        if (b->min[a] >= b->max[a])
            luaL_error(L, "box min must be below max");
}

static void Textures(lua_State *L, int t, uint8_t out[6]) {
    lua_getfield(L, t, "textures");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        int texture = lua_gettop(L);
        int all = Integer(L, texture, "all", -1, 0, 255),
            sides = Integer(L, texture, "sides", all, 0, 255);
        const char *faces[] = {"left", "right", "top", "bottom", "front", "back"};
        for (int f = 0; f < 6; f++) {
            int fallback = f == 2 || f == 3 ? all : sides;
            out[f] = Integer(L, texture, faces[f], fallback < 0 ? out[f] : fallback, 0, 255);
        }
    }
    lua_pop(L, 1);
}

static void Geometry(lua_State *L, int t, BlockDefinition *d) {
    BlockGeometry *g = &d->geometry;
    Textures(L, t, d->textures);
    for (int i = 0; i < g->boxCount; i++)
        Textures(L, t, g->boxes[i].textures);
    lua_getfield(L, t, "boxes");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        int n = lua_rawlen(L, -1);
        if (n < 1 || n > BLOCK_MODEL_MAX_BOXES)
            luaL_error(L, "models require 1..8 boxes");
        *g = (BlockGeometry){.enabled = 1, .boxCount = n, .collisionCount = n, .selectionCount = n};
        for (int i = 0; i < n; i++) {
            lua_rawgeti(L, -1, i + 1);
            luaL_checktype(L, -1, LUA_TTABLE);
            int box = lua_gettop(L);
            Box(L, box, &g->boxes[i].bounds);
            memcpy(g->boxes[i].textures, d->textures, 6);
            Textures(L, box, g->boxes[i].textures);
            g->collision[i] = g->selection[i] = g->boxes[i].bounds;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    if (!g->enabled) {
        g->enabled = 1;
        g->boxCount = g->collisionCount = g->selectionCount = 1;
        memcpy(g->boxes[0].bounds.min, d->min, 3);
        memcpy(g->boxes[0].bounds.max, d->max, 3);
        memcpy(g->boxes[0].textures, d->textures, 6);
        g->collision[0] = g->selection[0] = g->boxes[0].bounds;
    }
    const char *keys[] = {"collision_boxes", "selection_boxes"};
    for (int k = 0; k < 2; k++) {
        lua_getfield(L, t, keys[k]);
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TTABLE);
            int n = lua_rawlen(L, -1);
            if (n > BLOCK_MODEL_MAX_BOXES)
                luaL_error(L, "at most 8 shape boxes are supported");
            if (k)
                g->selectionCount = n;
            else
                g->collisionCount = n;
            for (int i = 0; i < n; i++) {
                lua_rawgeti(L, -1, i + 1);
                luaL_checktype(L, -1, LUA_TTABLE);
                Box(L, lua_gettop(L), k ? &g->selection[i] : &g->collision[i]);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    d->renderType = Integer(L, t, "render", d->renderType, 0, BLOCK_RENDER_TRANSLUCENT);
    d->lightType = Integer(L, t, "light", d->lightType, 0, BLOCK_LIGHT_EMIT);
    d->colliderType = Integer(L, t, "collider", d->colliderType, 0, BLOCK_COLLIDER_LIQUID);
}

static int FieldIndex(lua_State *L, StateSet *set, const char *name) {
    for (int i = 0; i < set->fieldCount; i++)
        if (!strcmp(name, set->fields[i].name))
            return i;
    return luaL_error(L, "variant field '%s' must be listed in state_fields", name);
}

void LuaBlockStates_Define(lua_State *L, int id, int table, BlockDefinition *base) {
    table = lua_absindex(L, table);
    bool custom = false;
    const char *keys[] = {"boxes", "models", "variants", "collision_boxes", "selection_boxes"};
    for (int i = 0; i < 5; i++) {
        lua_getfield(L, table, keys[i]);
        custom |= !lua_isnil(L, -1);
        lua_pop(L, 1);
    }
    if (!custom)
        return;
    if (base->modelType != BLOCK_MODEL_SOLID)
        luaL_error(L, "box models require model = block.model.SOLID");
    // Lua owns temporary storage so validation errors cannot leak it.
    StateSet *set = lua_newuserdata(L, sizeof(*set));
    memset(set, 0, sizeof(*set));
    set->count = 1;
    Geometry(L, table, base);
    ServerBlockStates_UpdateBounds(base);
    set->states[0] = *base;
    lua_getfield(L, table, "state_fields");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        set->fieldCount = lua_rawlen(L, -1);
        if (set->fieldCount > BLOCK_STATE_MAX_FIELDS)
            luaL_error(L, "at most 8 state_fields are supported");
        for (int i = 0; i < set->fieldCount; i++) {
            lua_rawgeti(L, -1, i + 1);
            const char *name = luaL_checkstring(L, -1);
            if (strlen(name) > 64)
                luaL_error(L, "state field name too long");
            StateField *f = &set->fields[i];
            strcpy(f->name, name);
            if (!ServerMetadata_StateField(id, name, &f->boolean, &f->bits))
                luaL_error(L, "state fields must reference integer or boolean metadata");
            for (int j = 0; j < i; j++)
                if (!strcmp(set->fields[j].name, name))
                    luaL_error(L, "duplicate state field");
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua_getfield(L, table, "models");
    int models = lua_gettop(L);
    if (!lua_isnil(L, models)) {
        luaL_checktype(L, models, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, models)) {
            if (lua_type(L, -2) != LUA_TSTRING)
                luaL_error(L, "model names must be strings");
            luaL_checktype(L, -1, LUA_TTABLE);
            BlockDefinition check = *base;
            Geometry(L, lua_gettop(L), &check);
            lua_pop(L, 1);
        }
    }
    lua_getfield(L, table, "variants");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        set->ruleCount = lua_rawlen(L, -1);
        if (set->ruleCount > BLOCK_STATE_MAX_RULES)
            luaL_error(L, "too many variants");
        for (int i = 0; i < set->ruleCount; i++) {
            lua_rawgeti(L, -1, i + 1);
            luaL_checktype(L, -1, LUA_TTABLE);
            int variant = lua_gettop(L);
            StateRule *r = &set->rules[i];
            r->rotationField = -1;
            r->state = set->count;
            lua_getfield(L, variant, "when");
            luaL_checktype(L, -1, LUA_TTABLE);
            int when = lua_gettop(L);
            lua_pushnil(L);
            while (lua_next(L, when)) {
                if (lua_type(L, -2) != LUA_TSTRING)
                    luaL_error(L, "when keys must be field names");
                int f = FieldIndex(L, set, lua_tostring(L, -2));
                r->mask |= 1 << f;
                if (set->fields[f].boolean) {
                    luaL_checktype(L, -1, LUA_TBOOLEAN);
                    r->values[f] = lua_toboolean(L, -1);
                } else
                    r->values[f] = luaL_checkinteger(L, -1);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            BlockDefinition d = *base;
            lua_getfield(L, variant, "model");
            if (!lua_isnil(L, -1)) {
                const char *name = luaL_checkstring(L, -1);
                if (lua_isnil(L, models))
                    luaL_error(L, "variant references a missing models table");
                lua_getfield(L, models, name);
                luaL_checktype(L, -1, LUA_TTABLE);
                Geometry(L, lua_gettop(L), &d);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            Geometry(L, variant, &d);
            int degrees = Integer(L, variant, "rotate_y", 0, 0, 270);
            if (degrees % 90)
                luaL_error(L, "rotate_y must be 0, 90, 180 or 270");
            lua_getfield(L, variant, "rotate_y_from");
            if (!lua_isnil(L, -1)) {
                r->rotationField = FieldIndex(L, set, luaL_checkstring(L, -1));
                if (set->fields[r->rotationField].boolean)
                    luaL_error(L, "rotate_y_from requires integer metadata (0..3)");
            }
            lua_pop(L, 1);
            int rotations = r->rotationField < 0 ? 1 : 4;
            if (set->count + rotations > BLOCK_MAX_STATES)
                luaL_error(L, "at most 31 compiled variants plus the default are supported");
            for (int turn = 0; turn < rotations; turn++) {
                d.geometry.rotation = (degrees / 90 + turn) % 4;
                ServerBlockStates_UpdateBounds(&d);
                if (!BlockDefinition_Validate(id, &d))
                    luaL_error(L, "invalid variant");
                set->states[set->count++] = d;
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 2);
    if (!ServerBlockStates_Define(id, set, base))
        luaL_error(L, "out of memory");
    lua_pop(L, 1);
}
