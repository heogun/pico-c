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

/*
 * foreach(table, function) — run a function for every element of a table.
 *
 * The function gets the key and the value of one element.  The loop stops
 * early when the function returns true.
 *
 * 함수가 테이블의 원소 하나마다 키와 값을 받는다. 함수가 true를 돌려주면
 * 일찍 끝난다.
 */
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

/*
 * unpack(table, [first, [last]]) — the elements of a table as values.
 *
 * Returns the elements from first to last, so that they can be given to a
 * function.  The default is the whole table.
 *
 * first부터 last까지의 원소를 값으로 돌려준다. 그래야 함수에 넘길 수 있다.
 * 기본값은 테이블 전체다.
 */
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

/*
 * pico8_random(limit) — a random number below limit.
 *
 * The generator is a 64 bit linear congruential one; the seed lives in the
 * two variables seed_hi and seed_lo.
 *
 * 생성기는 64비트 선형 합동식이다. 시드는 seed_hi와 seed_lo 두 변수에 있다.
 */
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

/*
 * srand(seed) — set the seed of the random number generator.
 *
 * Sets the seed of the random number generator.
 *
 * 난수 생성기의 시드를 정한다.
 */
static int pico8_srand(lua_State *L)
{
    int32_t s = ((z8::fix32)luaL_checknumber(L, 1)).bits();
    if (s == 0) { seed_hi = 0x60009755; seed_lo = 0xdeadbeef; }
    else { s &= 0x7fffffff; seed_hi = s ^ 0xbead29ba; seed_lo = s; }
    for (int i = 0; i < 32; i++) pico8_random(0);
    return 0;
}

/*
 * rnd(x) — a random number.
 *
 * rnd()          a number from 0 to 1
 * rnd(n)         a whole number from 0 to n-1
 * rnd(a, b)      a whole number from a to b-1
 *
 * rnd()은 0부터 1 사이의 수, rnd(n)은 0부터 n-1까지의 정수,
 * rnd(a, b)는 a부터 b-1까지의 정수를 돌려준다.
 */
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

/*
 * count(table) — the number of elements in a table.
 *
 * Counts the elements the way PICO-8 does, which is not always the same as
 * the length of the array part.
 *
 * PICO-8이 세는 방식대로 원소를 센다. 배열 부분의 길이와 항상 같지는 않다.
 */
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

/*
 * deli(table, [position]) — remove an element from a table.
 *
 * Without a position the last element is removed, otherwise the one at
 * that position.  The elements after it move down.
 *
 * 위치를 주지 않으면 마지막 원소를, 주면 그 위치의 원소를 지운다. 그 뒤의
 * 원소들이 앞으로 당겨진다.
 */
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

/*
 * serial() — a new serial number for the cart.
 *
 * Every call gives a number that has not been used before, so that a cart
 * can name its save files.
 *
 * 호출할 때마다 전에 쓰지 않은 번호를 주므로, 카트가 저장 파일 이름을
 * 지을 때 쓸 수 있다.
 */
static int pico8_serial(lua_State *L)
{
    (void)L;
    return 0;
}

/*
 * add(table) — add a table to the list of carts of the shell.
 *
 * The shell keeps the carts of the "cartridge tab" in one table.
 *
 * 셸은 "카트리지 탭"의 카트들을 하나의 테이블에 모아 둔다.
 */
static int pico8_add(lua_State *L)
{
    if (lua_isnil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 2);
    lua_rawseti(L, 1, (int)lua_rawlen(L, 1) + 1);
    return 0;
}

/*
 * del(table) — remove a cart from the list of carts of the shell.
 *
 * 셸의 카트 목록에서 카트를 지운다.
 */
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

/*
 * all() — every cart of the shell.
 *
 * Returns the table with all the carts of the cartridge tab.
 *
 * 카트리지 탭의 모든 카트를 담은 테이블을 돌려준다.
 */
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

/*
 * pico8_shell_cmd(L) — run a shell command from Lua.
 *
 * The name of the command is the upvalue of the closure, so all the shell
 * commands share this one function.
 *
 * 명령 이름은 클로저의 업밸류이므로, 모든 셸 명령이 이 함수 하나를 공유한다.
 */
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
