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
 * peek(address) — the byte at an address.
 *
 * Reads the cart's memory.  Addresses above the 64KB of RAM are wrapped
 * around, which is what PICO-8 does.
 *
 * 카트의 메모리를 읽는다. 64KB를 넘는 주소는 되돌아간다. 실제 PICO-8도
 * 그래서한다.
 */
static int pico8_peek(lua_State *L)
{
    uint16_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned len = fix32_to_uint32(luaL_optnumber(L, 2, 1));
    if (addr >= PICO8_RAM_SIZE) return 0;
    unsigned max = PICO8_RAM_SIZE - addr;
    if (len > max) len = max;
    for (unsigned i = 0; i < len; i++)
        lua_pushnumber(L, fix32_from_uint8(pico8_ram[addr + i]));
    return len;
}

/*
 * poke(address, value) — write a byte into the memory.
 *
 * Only the lowest 8 bits of the value are written.
 *
 * 값의 하위 8비트만 쓴다.
 */
static int pico8_poke(lua_State *L)
{
    uint16_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned n = lua_gettop(L);
    for (unsigned i = 2; i <= n; i++) {
        if (addr >= PICO8_RAM_SIZE) break;
        pico8_ram[addr++] = fix32_to_uint8(luaL_checkinteger(L, i));
    }
    return 0;
}

/*
 * peek2(address) — the 16 bit word at an address.
 *
 * The word is read little endian, so the byte at the address is the low
 * half of the result.
 *
 * 워드는 리틀 엔디언으로 읽는다. 즉 그 주소의 바이트가 결과의 하위 절반이다.
 */
static int pico8_peek2(lua_State *L)
{
    uint16_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned len = fix32_to_uint32(luaL_optnumber(L, 2, 1));
    if (addr >= PICO8_RAM_SIZE) return 0;
    unsigned max = (PICO8_RAM_SIZE - addr) / 2;
    if (len > max) len = max;
    for (unsigned i = 0; i < len; i++) {
        uint16_t v = (uint16_t)pico8_ram[addr + i * 2] | ((uint16_t)pico8_ram[addr + i * 2 + 1] << 8);
        lua_pushnumber(L, fix32_from_uint16(v));
    }
    return len;
}

/*
 * poke2(address, value) — write a 16 bit word into the memory.
 *
 * The low byte goes to the address, the high byte to the address after it.
 *
 * 낮은 바이트가 그 주소에, 높은 바이트가 그 다음 주소에 들어간다.
 */
static int pico8_poke2(lua_State *L)
{
    uint16_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned n = lua_gettop(L);
    for (unsigned i = 2; i <= n; i++) {
        if (addr >= PICO8_RAM_SIZE - 1) break;
        uint16_t v = fix32_to_uint16(luaL_checkinteger(L, i));
        pico8_ram[addr++] = (uint8_t)(v & 0xff);
        pico8_ram[addr++] = (uint8_t)((v >> 8) & 0xff);
    }
    return 0;
}

/*
 * peek4(address) — the 32 bit word at an address.
 *
 * Read little endian, like peek2().
 *
 * 리틀 엔디언으로 읽는다. peek2()와 같다.
 */
static int pico8_peek4(lua_State *L)
{
    uint16_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned len = fix32_to_uint32(luaL_optnumber(L, 2, 1));
    if (addr >= PICO8_RAM_SIZE) return 0;
    unsigned max = (PICO8_RAM_SIZE - addr) / 4;
    if (len > max) len = max;
    for (unsigned i = 0; i < len; i++) {
        uint32_t v = (uint32_t)pico8_ram[addr + i * 4] | ((uint32_t)pico8_ram[addr + i * 4 + 1] << 8)
                    | ((uint32_t)pico8_ram[addr + i * 4 + 2] << 16) | ((uint32_t)pico8_ram[addr + i * 4 + 3] << 24);
        lua_pushnumber(L, fix32_from_uint32(v));
    }
    return len;
}

/*
 * poke4(address, value) — write a 32 bit word into the memory.
 *
 * Four bytes, lowest first.
 *
 * 네 바이트를 낮은 쪽부터 쓴다.
 */
static int pico8_poke4(lua_State *L)
{
    uint16_t addr = fix32_to_uint16(luaL_checknumber(L, 1));
    unsigned n = lua_gettop(L);
    for (unsigned i = 2; i <= n; i++) {
        if (addr >= PICO8_RAM_SIZE - 3) break;
        uint32_t v = fix32_to_uint32(luaL_checkinteger(L, i));
        pico8_ram[addr++] = (uint8_t)(v & 0xff);
        pico8_ram[addr++] = (uint8_t)((v >> 8) & 0xff);
        pico8_ram[addr++] = (uint8_t)((v >> 16) & 0xff);
        pico8_ram[addr++] = (uint8_t)((v >> 24) & 0xff);
    }
    return 0;
}

/*
 * memcpy(destination, source, length) — copy a block of memory.
 *
 * Copies length bytes from the source address to the destination address.
 * Both wrap around at the end of the RAM.
 *
 * source 주소에서 destination 주소로 length 바이트를 복사한다. 두 주소 모두
 * RAM 끝에서 되돌아간다.
 */
static int pico8_memcpy(lua_State *L)
{
    uint16_t dst = fix32_to_uint16(luaL_checknumber(L, 1));
    uint16_t src = fix32_to_uint16(luaL_checknumber(L, 2));
    uint32_t len = fix32_to_uint32(luaL_checknumber(L, 3));
    if (src >= PICO8_RAM_SIZE || dst >= PICO8_RAM_SIZE) return 0;
    if (src + len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE - src;
    if (dst + len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE - dst;
    if (len) memmove(&pico8_ram[dst], &pico8_ram[src], len);
    return 0;
}

/*
 * memset(destination, value, length) — fill a block of memory.
 *
 * Writes the byte value length times, starting at the destination address.
 *
 * destination 주소부터 value 바이트를 length번 쓴다.
 */
static int pico8_memset(lua_State *L)
{
    uint16_t dst = fix32_to_uint16(luaL_checknumber(L, 1));
    uint8_t val = fix32_to_uint8(luaL_checkinteger(L, 2));
    uint32_t len = fix32_to_uint32(luaL_checknumber(L, 3));
    if (dst >= PICO8_RAM_SIZE) return 0;
    if (dst + len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE - dst;
    if (len) memset(&pico8_ram[dst], val, len);
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
