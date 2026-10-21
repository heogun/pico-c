/*
 * api_input.cpp — the input part of the PICO-8 API.
 *
 * btn() reads the state of the six buttons, btnp() the "just pressed"
 * state.  The host fills the button state once per frame (see
 * pico8_update_input() in src/input.cpp); the functions here only read it.
 *
 * btn()은 여섯 버튼의 상태를, btnp()는 "방금 눌렸다" 상태를 읽는다.
 * 호스트가 프레임마다 버튼 상태를 채운다(src/input.cpp의 pico8_update_input).
 * 여기 있는 함수들은 그 상태를 읽기만 한다.
 */

#include "api_internal.h"

static int parse_index(lua_State *L, int idx)
{
    if (lua_type(L, idx) == LUA_TSTRING) {
        const char *s = lua_tostring(L, idx);
        uint8_t c = (uint8_t)s[0];
        switch (c) {
            case 139: return 0;
            case 145: return 1;
            case 148: return 2;
            case 131: return 3;
            case 142: return 4;
            case 151: return 5;
            default: return -1;
        }
    }
    if (lua_isnumber(L, idx))
        return fix32_to_int(lua_tointeger(L, idx));
    return -1;
}

static int pico8_btn(lua_State *L)
{
    int narg = lua_gettop(L);
    if (narg == 0) {
        lua_pushnumber(L, fix32_from_uint8(pico8_ram[PICO8_HW_STATE + 0x0c]));
        return 1;
    }
    int b = parse_index(L, 1);
    int p = (narg >= 2) ? fix32_to_int(luaL_checknumber(L, 2)) : 0;
    if (p < 0 || p > 1 || b < 0 || b > 5) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, (pico8_ram[PICO8_HW_STATE + 0x0c + p] >> b) & 1);
    return 1;
}

static int pico8_btnp(lua_State *L)
{
    int narg = lua_gettop(L);
    if (narg == 0) {
        uint8_t r = 0;
        for (int b = 0; b < 6; b++)
            if (pico8_btnp_state[0][b]) r |= (1 << b);
        lua_pushnumber(L, fix32_from_uint8(r));
        return 1;
    }
    int b = parse_index(L, 1);
    int p = (narg >= 2) ? fix32_to_int(luaL_checknumber(L, 2)) : 0;
    if (p < 0 || p > 1 || b < 0 || b > 5) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, pico8_btnp_state[p][b]);
    return 1;
}


/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg input_api[] = {
    { "btn",  pico8_btn  }, { "btnp", pico8_btnp },
    { NULL, NULL }
};

void pico8_api_register_input(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = input_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
