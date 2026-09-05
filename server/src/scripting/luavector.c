#include "minilua.h"
#include <math.h>
#include "luavector.h"

extern lua_State *L;

typedef struct LuaVector { double x, y, z; } LuaVector;

static double Number(lua_State *state, int index) {
    double value = luaL_checknumber(state, index);
    if (!isfinite(value)) luaL_argerror(state, index, "expected a finite number");
    return value;
}

static LuaVector Read(lua_State *state, int index) {
    luaL_checktype(state, index, LUA_TTABLE);
    lua_getfield(state, index, "x");
    double x = Number(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, index, "y");
    double y = Number(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, index, "z");
    double z = Number(state, -1);
    lua_pop(state, 1);
    return (LuaVector){x, y, z};
}

static int PushNumber(lua_State *state, double value) {
    if (!isfinite(value)) return luaL_error(state, "vector calculation overflowed");
    lua_pushnumber(state, value);
    return 1;
}

static int Push(lua_State *state, LuaVector v) {
    lua_createtable(state, 0, 3);
    PushNumber(state, v.x); lua_setfield(state, -2, "x");
    PushNumber(state, v.y); lua_setfield(state, -2, "y");
    PushNumber(state, v.z); lua_setfield(state, -2, "z");
    return 1;
}

static LuaVector Difference(LuaVector a, LuaVector b) {
    return (LuaVector){a.x - b.x, a.y - b.y, a.z - b.z};
}

static double Magnitude(LuaVector v) { return hypot(hypot(v.x, v.y), v.z); }

static LuaVector Normalized(lua_State *state, LuaVector v) {
    double scale = fmax(fabs(v.x), fmax(fabs(v.y), fabs(v.z)));
    if (!isfinite(scale)) luaL_error(state, "vector calculation overflowed");
    if (scale == 0) return (LuaVector){0};
    v = (LuaVector){v.x / scale, v.y / scale, v.z / scale};
    double length = Magnitude(v);
    return (LuaVector){v.x / length, v.y / length, v.z / length};
}

static int New(lua_State *state) {
    if (lua_gettop(state) == 0) return Push(state, (LuaVector){0});
    if (lua_istable(state, 1)) return Push(state, Read(state, 1));
    double x = Number(state, 1), y = Number(state, 2), z = Number(state, 3);
    return Push(state, (LuaVector){x, y, z});
}

static int Add(lua_State *state) {
    LuaVector a = Read(state, 1), b = Read(state, 2);
    return Push(state, (LuaVector){a.x + b.x, a.y + b.y, a.z + b.z});
}

static int Subtract(lua_State *state) {
    LuaVector a = Read(state, 1), b = Read(state, 2);
    return Push(state, Difference(a, b));
}

static int Multiply(lua_State *state) {
    LuaVector v = Read(state, 1);
    double scale = Number(state, 2);
    return Push(state, (LuaVector){v.x * scale, v.y * scale, v.z * scale});
}

static int Length(lua_State *state) { return PushNumber(state, Magnitude(Read(state, 1))); }

static int Distance(lua_State *state) {
    LuaVector a = Read(state, 1), b = Read(state, 2);
    return PushNumber(state, Magnitude(Difference(a, b)));
}

static int Normalize(lua_State *state) { return Push(state, Normalized(state, Read(state, 1))); }

static int Direction(lua_State *state) {
    LuaVector from = Read(state, 1), to = Read(state, 2);
    return Push(state, Normalized(state, Difference(to, from)));
}

static int Dot(lua_State *state) {
    LuaVector a = Read(state, 1), b = Read(state, 2);
    return PushNumber(state, a.x * b.x + a.y * b.y + a.z * b.z);
}

static int Cross(lua_State *state) {
    LuaVector a = Read(state, 1), b = Read(state, 2);
    return Push(state, (LuaVector){a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x});
}

void LuaVector_Init(void) {
    static const luaL_Reg methods[] = {
        {"new", New}, {"add", Add}, {"subtract", Subtract}, {"multiply", Multiply},
        {"length", Length}, {"distance", Distance}, {"normalize", Normalize},
        {"direction", Direction}, {"dot", Dot}, {"cross", Cross}, {NULL, NULL}
    };
    luaL_newlib(L, methods);
    lua_setglobal(L, "vector");
}
