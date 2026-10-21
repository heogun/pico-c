/*
 * api_audio.cpp — the sound part of the PICO-8 API.
 *
 * sfx() starts a sound effect on a channel, music() starts a song.  The
 * synthesis itself lives in src/audio.cpp; these two functions only
 * translate the cart's arguments and hand them over.
 *
 * sfx()는 채널에서 효과음을 재생하고, music()는 songs를 재생한다. 실제 합성은
 * src/audio.cpp에 있고, 여기의 두 함수는 카트의 인자를 옮겨 전달할 뿐이다.
 */

#include "api_internal.h"

static int pico8_sfx(lua_State *L)
{
    int n = (int)fix32_to_int(luaL_checknumber(L, 1));
    int channel = (int)fix32_to_int(luaL_optnumber(L, 2, -1));
    int offset = (int)fix32_to_int(luaL_optnumber(L, 3, 0));
    int length = (int)fix32_to_int(luaL_optnumber(L, 4, 0));
    if (n == -1 || n == -2) {
        pico8_audio_sfx(n, channel, offset, length);
    } else if (n >= 0 && n < 64) {
        pico8_audio_sfx(n, channel, offset, length);
    }
    return 0;
}

static int pico8_music(lua_State *L)
{
    int n = (int)fix32_to_int(luaL_optnumber(L, 1, -1));
    int fade_len = (int)fix32_to_int(luaL_optnumber(L, 2, 0));
    int channel_mask = (int)fix32_to_int(luaL_optnumber(L, 3, 0));
    if (n == -1 || (n >= 0 && n < 64)) {
        pico8_audio_music(n, fade_len, channel_mask);
    }
    return 0;
}


/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg audio_api[] = {
    { "sfx",   pico8_sfx   }, { "music", pico8_music },
    { NULL, NULL }
};

void pico8_api_register_audio(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = audio_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
