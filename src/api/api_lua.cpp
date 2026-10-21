/*
 * api_lua.cpp — the Lua helpers of the PICO-8 API.
 *
 * These are the functions PICO-8 adds to Lua, or changes about it:
 * foreach() and unpack() replace the Lua 2.x versions, rnd()/srand() are
 * PICO-8's random number generator, and add()/del()/deli()/count()/all()
 * are the "cartridge tab" helpers the shell uses.  The shell commands that
 * PICO-8 also exposes as Lua functions (dir, cd, help, ...) are bound here
 * as well.
 *
 * PICO-8이 Lua에 추가하거나 바꾼 함수들이다. foreach()/unpack()은 Lua 2.x
 * 판을 대체하고, rnd()/srand()는 PICO-8 전용 난수 생성기다. add()/del()/deli()/
 * count()/all()은 셸이 쓰는 카트리지 탭 헬퍼이고, 셸 명령(dir, cd, help 등)도
 * 여기에 바인딩된다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_internal.h"

static int pico8_foreach(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    unsigned len = (unsigned)lua_rawlen(L, 1);
    for (unsigned i = 1; i <= len; i++) {
        lua_pushvalue(L, 2);
        lua_rawgeti(L, 1, i);
        lua_call(L, 1, 0);
    }
    return 0;
}

static int pico8_unpack(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int i = fix32_to_int(luaL_optnumber(L, 2, fix32_from_int(1)));
    unsigned len = (unsigned)lua_rawlen(L, 1);
    int j = fix32_to_int(luaL_optnumber(L, 3, fix32_from_int((int)len)));
    if (i < 1) i = 1;
    if ((unsigned)j > len) j = (int)len;
    int n = 0;
    for (int k = i; k <= j; k++) {
        lua_rawgeti(L, 1, k);
        n++;
    }
    return n;
}

static int32_t seed_hi = 0x60009755;
static int32_t seed_lo = 0xdeadbeef;

static int32_t pico8_random(int32_t limit)
{
    seed_hi = (seed_hi << 16) | ((uint32_t)seed_hi >> 16);
    seed_hi += seed_lo;
    seed_lo += seed_hi;
    if (limit < 0) {
        if (limit <= seed_hi && seed_hi < 0) return seed_hi - limit;
        return seed_hi;
    }
    int32_t result;
    if (seed_hi < 0)
        result = (seed_hi % limit + limit) % limit;
    else
        result = seed_hi % limit;
    return result;
}

static int pico8_srand(lua_State *L)
{
    int32_t s = ((z8::fix32)luaL_checknumber(L, 1)).bits();
    if (s == 0) { seed_hi = 0x60009755; seed_lo = 0xdeadbeef; }
    else { s &= 0x7fffffff; seed_hi = s ^ 0xbead29ba; seed_lo = s; }
    for (int i = 0; i < 32; i++) pico8_random(0);
    return 0;
}

static int pico8_rnd(lua_State *L)
{
    if (lua_istable(L, 1)) {
        unsigned len = (unsigned)lua_rawlen(L, 1);
        if (len == 0) { lua_pushnil(L); return 1; }
        int32_t v = pico8_random(len << 16);
        int idx = (v >> 16) + 1;
        if (idx > (int)len) idx = len;
        if (idx < 1) idx = 1;
        lua_pushnumber(L, fix32_from_int(idx));
        lua_gettable(L, 1);
    } else {
        z8::fix32 arg = (lua_gettop(L) > 0 && !lua_isnil(L, 1)) ? (z8::fix32)luaL_checknumber(L, 1) : z8::fix32(1);
        int32_t limit = arg.bits();
        if (limit == 0) limit = 1;
        int32_t r = pico8_random(limit);
        lua_pushnumber(L, z8::fix32::frombits(r));
    }
    return 1;
}

static int pico8_count(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_isnil(L, 1)) { lua_pushinteger(L, 0); return 1; }
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    if (lua_gettop(L) < 2) { lua_pushinteger(L, n); return 1; }
    int target = fix32_to_int(luaL_checknumber(L, 2));
    int cnt = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        int v = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
        lua_pop(L, 1);
        if (v == target) cnt++;
    }
    lua_pushinteger(L, cnt);
    return 1;
}

static int pico8_deli(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    int idx = (int)luaL_optinteger(L, 2, n);
    if (idx < 1 || idx > n) return 0;
    lua_rawgeti(L, 1, idx);
    for (int i = idx; i < n; i++) {
        lua_rawgeti(L, 1, i + 1);
        lua_rawseti(L, 1, i);
    }
    lua_pushnil(L);
    lua_rawseti(L, 1, n);
    return 1;
}

static int pico8_serial(lua_State *L)
{
    (void)L;
    return 0;
}

static int pico8_add(lua_State *L)
{
    if (lua_isnil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 2);
    lua_rawseti(L, 1, (int)lua_rawlen(L, 1) + 1);
    return 0;
}

static int pico8_del(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    int written = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        int equal = lua_rawequal(L, -1, 2);
        lua_pop(L, 1);
        if (!equal) {
            written++;
            lua_rawgeti(L, 1, i);
            lua_rawseti(L, 1, written);
        }
    }
    for (int i = written + 1; i <= n; i++)
        lua_pushnil(L), lua_rawseti(L, 1, i);
    return 0;
}

static int pico8_all(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
        const char *code = "return function() return nil end";
        luaL_loadstring(L, code);
        lua_call(L, 0, 1);
        return 1;
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getglobal(L, "_pico8_all_func");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        const char *code = "return function(t) local i=0; local n=#t; "
                           "return function() i=i+1; if i<=n then return t[i] end end end";
        luaL_loadstring(L, code);
        lua_call(L, 0, 1);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_pico8_all_func");
    }
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    return 1;
}

/* Forward a call such as mkdir("name") / cd("dir") / dir() to the shell. String
   arguments are joined with spaces to form the command line. */
static int pico8_shell_cmd(lua_State *L)
{
    const char *name = lua_tostring(L, lua_upvalueindex(1));
    std::string line = name ? name : "";
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        if (lua_isnil(L, i)) continue;
        size_t len = 0;
        const char *s = luaL_tolstring(L, i, &len);
        line += " ";
        line += (s ? s : "");
        lua_pop(L, 1);
    }
    pico8_console_exec(line.c_str());
    return 0;
}


/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg lua_api[] = {
    { "foreach", pico8_foreach }, { "unpack", pico8_unpack },
    { "srand",   pico8_srand   }, { "rnd",    pico8_rnd    },
    { "count",   pico8_count   }, { "deli",   pico8_deli   },
    { "serial",  pico8_serial  }, { "add",    pico8_add    },
    { "del",     pico8_del     }, { "all",    pico8_all    },
    { NULL, NULL }
};

void pico8_api_register_lua(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = lua_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }

    /* Shell commands that PICO-8 also exposes as global Lua functions, where
       each name is bound as an upvalue of a shared dispatcher.
       PICO-8이 전역 Lua 함수로도 제공하는 셸 명령들. 각 이름이 공용 디스패처의
       업밸류로 묶인다. */
    static const char *kShellFuncs[] = {
        "dir", "cd", "mkdir", "help", "backup", "exit", "export", "import",
        "install_demos", "install_games", "keyconfig", "login", "shutdown",
        "splore",
        NULL
    };
    for (int i = 0; kShellFuncs[i]; i++) {
        lua_pushstring(L, kShellFuncs[i]);
        lua_pushcclosure(L, pico8_shell_cmd, 1);
        lua_setfield(L, -2, kShellFuncs[i]);
    }

    lua_pop(L, 1);
}
