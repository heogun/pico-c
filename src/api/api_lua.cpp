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
#include <unistd.h>

#include "api_internal.h"

/*
 * The way PICO-8 walks a table in all() and foreach() (measured on 0.2.7):
 * the walk moves on only when the slot it is at still holds the value it
 * gave last, and it skips holes.  So the element that was just visited may
 * be deleted (del() moves the rest down) without the next one being
 * skipped, an element deleted further on is not visited, and an element
 * added at the end is.
 *
 * PICO-8이 all()과 foreach()에서 테이블을 도는 방식(0.2.7 실측): 지금 칸에
 * 마지막으로 준 값이 그대로 있을 때만 다음으로 가고, 구멍은 건너뛴다. 그래서
 * 방금 방문한 원소를 지워도(del()이 나머지를 당김) 다음 원소를 건너뛰지 않고,
 * 뒤쪽에서 지운 원소는 방문하지 않으며, 끝에 더한 원소는 방문한다.
 */
static int walk_next(lua_State *L, int table, int *pos, int prev)
{
    lua_rawgeti(L, table, *pos);
    int same = lua_rawequal(L, -1, prev);
    lua_pop(L, 1);
    if (same) (*pos)++;
    int len = (int)lua_rawlen(L, table);
    for (;;) {
        lua_rawgeti(L, table, *pos);
        if (!lua_isnil(L, -1) || *pos > len) return !lua_isnil(L, -1);
        lua_pop(L, 1);
        (*pos)++;
    }
}

/*
 * foreach(table, function) — call a function with every element of a table.
 *
 * Walks the table like all() (see walk_next()).  A nil table does nothing.
 *
 * all()처럼 테이블을 돈다(walk_next() 참고). nil 테이블이면 아무것도 하지 않는다.
 */
static int pico8_foreach(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 2);
    int pos = 1;
    lua_pushnil(L);                             /* prev, at index 3 */
    while (walk_next(L, 1, &pos, 3)) {
        lua_replace(L, 3);                      /* prev = element */
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
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
 * count(table, [value]) — the length of a table, or how often a value is in it.
 *
 * With a value, counts the elements equal to it (numbers, strings, tables:
 * by identity).  A nil table gives no value at all, like PICO-8.
 *
 * 값을 주면 그 값과 같은 원소를 센다(숫자, 문자열, 테이블은 동일성). nil
 * 테이블이면 PICO-8처럼 아무 값도 돌려주지 않는다.
 */
static int pico8_count(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    if (lua_gettop(L) < 2) { lua_pushinteger(L, n); return 1; }
    int cnt = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_rawequal(L, -1, 2)) cnt++;
        lua_pop(L, 1);
    }
    lua_pushinteger(L, cnt);
    return 1;
}

/*
 * deli(table, [position]) — remove an element from a table.
 *
 * Without a position the last element is removed, otherwise the one at
 * that position.  The elements after it move down.  Returns the removed
 * element; a nil table or a position outside of the table gives no value.
 *
 * 위치가 없으면 마지막 원소를, 있으면 그 위치의 원소를 지우고 뒤의 원소들을
 * 당긴다. 지운 원소를 돌려준다. nil 테이블이나 범위 밖 위치면 아무 값도
 * 돌려주지 않는다.
 */
static int pico8_deli(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) return 0;
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
 * serial(channel, address, length) — move bytes between the RAM and a port.
 *
 * Channel 0x805 writes `length` bytes of the RAM from `address` to the
 * standard output, as they are; channel 0x804 reads up to `length` bytes of
 * the standard input into the RAM.  Both return how many bytes they moved
 * (measured on PICO-8 0.2.7: 11 for an 11 byte write, 0 for a read at the
 * end of the input).  The other channels (GPIO, dropped files, ...) have
 * nothing behind them here and move 0 bytes.
 *
 * 0x805 채널은 RAM의 `address`부터 `length` 바이트를 그대로 표준 출력에 쓰고,
 * 0x804 채널은 표준 입력에서 최대 `length` 바이트를 RAM으로 읽는다. 둘 다 옮긴
 * 바이트 수를 돌려준다(PICO-8 0.2.7 실측: 11바이트 쓰기는 11, 입력 끝에서 읽기는
 * 0). 다른 채널(GPIO, 끌어다 놓은 파일 등)은 여기에 대응하는 것이 없어 0바이트를
 * 옮긴다.
 */
static int pico8_serial(lua_State *L)
{
    int channel = (int)luaL_optinteger(L, 1, 0);
    int address = (int)luaL_optinteger(L, 2, 0);
    int length  = (int)luaL_optinteger(L, 3, 0);
    int moved = 0;
    if (address < 0) address = 0;
    if (length < 0) length = 0;
    if (address + length > PICO8_RAM_SIZE) length = PICO8_RAM_SIZE - address;

    if (channel == 0x805 && length > 0) {
        fflush(stdout);
        moved = (int)fwrite(pico8_ram + address, 1, (size_t)length, stdout);
        fflush(stdout);
    } else if (channel == 0x804 && length > 0 && !isatty(fileno(stdin))) {
        /* A terminal is left alone: reading it would stop the emulator
           until someone types.  터미널은 건드리지 않는다: 읽으면 누군가
           입력할 때까지 에뮬레이터가 멈춘다. */
        moved = (int)fread(pico8_ram + address, 1, (size_t)length, stdin);
    }
    lua_pushinteger(L, moved);
    return 1;
}

/*
 * add(table, value, [position]) — put a value into a table.
 *
 * Appends the value, or inserts it at the position (1 .. #table + 1, the
 * elements from there move up; any other position is the error "position
 * out of bounds").  Returns the value.  A nil table gives no value.
 *
 * 값을 끝에 붙이거나, 위치(1 .. #table + 1)에 끼워 넣는다(그 뒤 원소는 한 칸씩
 * 밀리고, 다른 위치는 "position out of bounds" 오류). 값을 돌려준다. nil
 * 테이블이면 아무 값도 돌려주지 않는다.
 */
static int pico8_add(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 3);
    int n = (int)lua_rawlen(L, 1);
    int pos = n + 1;
    if (!lua_isnil(L, 3)) {
        pos = (int)luaL_checkinteger(L, 3);
        if (pos < 1 || pos > n + 1) return luaL_error(L, "position out of bounds");
        for (int i = n; i >= pos; i--) {
            lua_rawgeti(L, 1, i);
            lua_rawseti(L, 1, i + 1);
        }
    }
    lua_pushvalue(L, 2);
    lua_rawseti(L, 1, pos);
    lua_pushvalue(L, 2);
    return 1;
}

/*
 * del(table, value) — remove a value from a table.
 *
 * Removes the first element equal to the value (the elements after it move
 * down) and returns it.  When the value is not there, or the table is nil,
 * nothing is removed and no value is returned.
 *
 * 값과 같은 첫 원소를 지우고(뒤 원소들을 당김) 그것을 돌려준다. 값이 없거나
 * 테이블이 nil이면 아무것도 지우지 않고 아무 값도 돌려주지 않는다.
 */
static int pico8_del(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    int n = (int)lua_rawlen(L, 1);
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_rawequal(L, -1, 2)) {
            for (int j = i; j < n; j++) {
                lua_rawgeti(L, 1, j + 1);
                lua_rawseti(L, 1, j);
            }
            lua_pushnil(L);
            lua_rawseti(L, 1, n);
            return 1;                           /* the removed element */
        }
        lua_pop(L, 1);
    }
    return 0;
}

/*
 * all_step(L) — the iterator that all() returns.
 *
 * Upvalue 1 is the table, 2 the position, 3 the element given last.
 *
 * all()이 돌려주는 반복자. 업밸류 1은 테이블, 2는 위치, 3은 마지막으로 준 원소.
 */
static int all_step(lua_State *L)
{
    int pos = (int)lua_tointeger(L, lua_upvalueindex(2));
    if (!walk_next(L, lua_upvalueindex(1), &pos, lua_upvalueindex(3))) return 0;
    lua_pushinteger(L, pos);
    lua_replace(L, lua_upvalueindex(2));
    lua_pushvalue(L, -1);
    lua_replace(L, lua_upvalueindex(3));
    return 1;
}

/*
 * all(table) — an iterator over the elements of a table, for "for in".
 *
 * Walks the table the PICO-8 way (see walk_next()).  A nil table gives an
 * iterator that ends at once.
 *
 * "for in"에 쓰는 테이블 원소 반복자. PICO-8 방식으로 돈다(walk_next() 참고).
 * nil 테이블이면 바로 끝나는 반복자를 준다.
 */
static int pico8_all(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        lua_newtable(L);                        /* an empty table */
    } else {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua_pushvalue(L, 1);
    }
    lua_pushinteger(L, 1);
    lua_pushnil(L);
    lua_pushcclosure(L, all_step, 3);
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
