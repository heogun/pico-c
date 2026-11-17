/*
 * api_memory.cpp — the memory part of the PICO-8 API.
 *
 * peek()/poke() and their 2 and 4 byte variants read and write the 64KB
 * of RAM the cart sees, plus memcpy()/memset() for blocks of it.  All of
 * them work on the same address space: the screen, the sprite sheet, the
 * cart data area and the draw state are just regions of it.
 *
 * peek()/poke()와 2/4바이트 변형이 카트가 보는 64KB RAM을 읽고 쓴다.
 * memcpy()/memset()는 그 블록 단위 버전이다. 화면, 스프라이트 시트, 카트
 * 데이터 영역, 드로우 상태는 모두 같은 주소 공간의 일부일 뿐이다.
 */

#include <string.h>

#include "api_internal.h"

/*
 * Every address wraps around at the end of the 64KB, as on PICO-8
 * (measured: poke(0xfffe,1,2,3,4) writes 3 and 4 at 0 and 1, poke2(0xffff)
 * puts its high byte at 0, memset and peek4 wrap the same way).
 *
 * 모든 주소는 PICO-8처럼 64KB 끝에서 되돌아간다(실측: poke(0xfffe,1,2,3,4)는
 * 3과 4를 0과 1에 쓰고, poke2(0xffff)는 상위 바이트를 0에 두며, memset과
 * peek4도 같은 식으로 되돌아간다).
 */
static inline uint8_t &mem(uint32_t address)
{
    return pico8_ram[address & 0xffff];
}

/* The most values one peek can give back.  peek 한 번이 돌려주는 최대 개수. */
static const unsigned kPeekMax = 8192;

/*
 * peek(address, [count]) — the bytes at an address.
 *
 * 그 주소의 바이트들.
 */
static int pico8_peek(lua_State *L)
{
    uint32_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned len = fix32_to_uint32(luaL_optnumber(L, 2, 1));
    if (len > kPeekMax) len = kPeekMax;
    luaL_checkstack(L, (int)len, "peek");
    for (unsigned i = 0; i < len; i++)
        lua_pushnumber(L, fix32_from_uint8(mem(addr + i)));
    return (int)len;
}

/*
 * poke(address, value, ...) — write bytes into the memory.
 *
 * Only the lowest 8 bits of every value are written.
 *
 * 값마다 하위 8비트만 쓴다.
 */
static int pico8_poke(lua_State *L)
{
    uint32_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    int n = lua_gettop(L);
    for (int i = 2; i <= n; i++)
        mem(addr++) = fix32_to_uint8(luaL_checkinteger(L, i));
    return 0;
}

/*
 * peek2(address, [count]) — the 16 bit words at an address.
 *
 * Little endian: the byte at the address is the low half.
 *
 * 리틀 엔디언: 그 주소의 바이트가 하위 절반이다.
 */
static int pico8_peek2(lua_State *L)
{
    uint32_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned len = fix32_to_uint32(luaL_optnumber(L, 2, 1));
    if (len > kPeekMax) len = kPeekMax;
    luaL_checkstack(L, (int)len, "peek2");
    for (unsigned i = 0; i < len; i++, addr += 2) {
        uint16_t v = (uint16_t)(mem(addr) | (mem(addr + 1) << 8));
        lua_pushnumber(L, fix32_from_uint16(v));
    }
    return (int)len;
}

/*
 * poke2(address, value, ...) — write 16 bit words into the memory.
 *
 * The low byte goes to the address, the high byte to the one after it.
 *
 * 낮은 바이트가 그 주소에, 높은 바이트가 그 다음 주소에 들어간다.
 */
static int pico8_poke2(lua_State *L)
{
    uint32_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    int n = lua_gettop(L);
    for (int i = 2; i <= n; i++) {
        uint16_t v = fix32_to_uint16(luaL_checkinteger(L, i));
        mem(addr++) = (uint8_t)(v & 0xff);
        mem(addr++) = (uint8_t)(v >> 8);
    }
    return 0;
}

/*
 * peek4(address, [count]) — the 32 bit words at an address.
 *
 * Little endian, like peek2().  The 32 bits are the number itself, 16.16
 * fixed point: bytes 1,2,3,4 read as 0x0403.0201.
 *
 * peek2()처럼 리틀 엔디언. 32비트가 곧 16.16 고정소수점 수다: 바이트 1,2,3,4는
 * 0x0403.0201로 읽힌다.
 */
static int pico8_peek4(lua_State *L)
{
    uint32_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned len = fix32_to_uint32(luaL_optnumber(L, 2, 1));
    if (len > kPeekMax) len = kPeekMax;
    luaL_checkstack(L, (int)len, "peek4");
    for (unsigned i = 0; i < len; i++, addr += 4) {
        uint32_t v = (uint32_t)mem(addr) | ((uint32_t)mem(addr + 1) << 8)
                   | ((uint32_t)mem(addr + 2) << 16) | ((uint32_t)mem(addr + 3) << 24);
        lua_pushnumber(L, z8::fix32::frombits((int32_t)v));   /* the raw 16.16 bits */
    }
    return (int)len;
}

/*
 * poke4(address, value, ...) — write 32 bit words into the memory.
 *
 * Four bytes, lowest first: the 16.16 bits of the number, fraction included.
 *
 * 네 바이트를 낮은 쪽부터 쓴다: 소수부까지 포함한 수의 16.16 비트.
 */
static int pico8_poke4(lua_State *L)
{
    uint32_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    int n = lua_gettop(L);
    for (int i = 2; i <= n; i++) {
        uint32_t v = (uint32_t)luaL_checknumber(L, i).bits();  /* the raw 16.16 bits */
        for (int k = 0; k < 4; k++) mem(addr++) = (uint8_t)(v >> (8 * k));
    }
    return 0;
}

/*
 * memcpy(destination, source, length) — copy a block of memory.
 *
 * Both blocks wrap around at the end of the RAM.  The source is read in
 * full before anything is written, so overlapping blocks copy as memmove.
 *
 * 두 블록 모두 RAM 끝에서 되돌아간다. 원본을 다 읽은 뒤에 쓰므로 겹치는
 * 블록도 memmove처럼 복사된다.
 */
static int pico8_memcpy(lua_State *L)
{
    uint32_t dst = fix32_to_uint16(luaL_checknumber(L, 1));
    uint32_t src = fix32_to_uint16(luaL_checknumber(L, 2));
    uint32_t len = fix32_to_uint32(luaL_checknumber(L, 3));
    if (len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE;
    if (len == 0) return 0;
    if (src + len <= PICO8_RAM_SIZE && dst + len <= PICO8_RAM_SIZE) {
        memmove(&pico8_ram[dst], &pico8_ram[src], len);
        return 0;
    }
    static uint8_t tmp[PICO8_RAM_SIZE];
    for (uint32_t i = 0; i < len; i++) tmp[i] = mem(src + i);
    for (uint32_t i = 0; i < len; i++) mem(dst + i) = tmp[i];
    return 0;
}

/*
 * memset(destination, value, length) — fill a block of memory.
 *
 * Writes the byte length times from the destination on, wrapping around
 * at the end of the RAM.
 *
 * destination부터 value 바이트를 length번 쓴다. RAM 끝에서 되돌아간다.
 */
static int pico8_memset(lua_State *L)
{
    uint32_t dst = fix32_to_uint16(luaL_checknumber(L, 1));
    uint8_t val = fix32_to_uint8(luaL_checkinteger(L, 2));
    uint32_t len = fix32_to_uint32(luaL_checknumber(L, 3));
    if (len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE;
    if (dst + len <= PICO8_RAM_SIZE) {
        if (len) memset(&pico8_ram[dst], val, len);
        return 0;
    }
    for (uint32_t i = 0; i < len; i++) mem(dst + i) = val;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg memory_api[] = {
    { "peek",   pico8_peek   }, { "poke",   pico8_poke   },
    { "peek2",  pico8_peek2  }, { "poke2",  pico8_poke2  },
    { "peek4",  pico8_peek4  }, { "poke4",  pico8_poke4  },
    { "memcpy", pico8_memcpy }, { "memset", pico8_memset },
    { NULL, NULL }
};

void pico8_api_register_memory(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = memory_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
