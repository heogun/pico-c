/*
 * api_cart.cpp — the cartridge part of the PICO-8 API.
 *
 * save() and load() write and read .p8 files, cstore() and reload() copy
 * raw data areas between the cart's memory and a file, cartdata()/dget()/
 * dset() keep the persistent data of the cart next to it, and run()/
 * reset()/stop() drive the runtime.
 *
 * save()/load()는 .p8 파일을 쓰고 읽고, cstore()/reload()는 카트 메모리와
 * 파일 사이에서 데이터 영역을 복사하며, cartdata()/dget()/dset()는 카트 옆에
 * 영속 데이터를 보관한다. run()/reset()/stop()는 런타임을 구동한다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_internal.h"

static double pico8_dmem[64] = {0};
static char pico8_data_id[256] = {0};

static void pico8_data_save(void)
{
    if (!pico8_data_id[0]) return;
    char filename[512];
    snprintf(filename, sizeof(filename), "%s.p8d", pico8_data_id);
    FILE *f = fopen(filename, "w");
    if (!f) return;
    for (int i = 0; i < 64; i++) fprintf(f, "%.17g\n", pico8_dmem[i]);
    fclose(f);
}

static int pico8_cartdata(lua_State *L)
{
    const char *id = (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) ? lua_tostring(L, 1) : "";
    memset(pico8_dmem, 0, sizeof(pico8_dmem));
    if (id && id[0]) {
        strncpy(pico8_data_id, id, sizeof(pico8_data_id) - 1);
        pico8_data_id[sizeof(pico8_data_id)-1] = 0;
        char filename[512];
        snprintf(filename, sizeof(filename), "%s.p8d", pico8_data_id);
        FILE *f = fopen(filename, "r");
        if (f) {
            for (int i = 0; i < 64; i++) {
                if (fscanf(f, "%lf", &pico8_dmem[i]) != 1) break;
            }
            fclose(f);
        }
    } else {
        pico8_data_id[0] = 0;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int pico8_dget(lua_State *L)
{
    int idx = fix32_to_int(luaL_checknumber(L, 1));
    if (idx < 0 || idx >= 64) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, (lua_Number)pico8_dmem[idx]);
    return 1;
}

static int pico8_dset(lua_State *L)
{
    int idx = fix32_to_int(luaL_checknumber(L, 1));
    double val = luaL_checknumber(L, 2);
    if (idx >= 0 && idx < 64) {
        pico8_dmem[idx] = val;
        pico8_data_save();
    }
    return 0;
}

static bool path_has_ext(const std::string &p, const char *ext)
{
    size_t n = strlen(ext);
    return p.size() >= n && p.compare(p.size() - n, n, ext) == 0;
}

static int pico8_save(lua_State *L)
{
    std::string path;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
        path = lua_tostring(L, 1);
    else
        path = pico8_cart_get_path();

    if (path.empty()) { lua_pushboolean(L, 0); return 1; }

    if (!path_has_ext(path, ".p8") && !path_has_ext(path, ".png"))
        path += ".p8";

    int rc;
    if (path_has_ext(path, ".png"))
        rc = pico8_cart_save_png(path.c_str());
    else
        rc = pico8_cart_save_p8(path.c_str());

    lua_pushboolean(L, rc == 0);
    return 1;
}

static int pico8_load(lua_State *L)
{
    const char *filename   = NULL;
    const char *breadcrumb = NULL;
    const char *param      = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) filename   = lua_tostring(L, 1);
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) breadcrumb = lua_tostring(L, 2);
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) param      = lua_tostring(L, 3);

    int ok = (pico8_console_queue_load(filename, breadcrumb, param) == 0);
    lua_pushboolean(L, ok);
    return 1;
}

static int pico8_reload(lua_State *L)
{
    int dest = fix32_to_int(luaL_optnumber(L, 1, 0));
    int source = fix32_to_int(luaL_optnumber(L, 2, 0));
    int len = fix32_to_int(luaL_optnumber(L, 3, 0x4300));
    const char *filename = NULL;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) filename = lua_tostring(L, 4);
    pico8_cart_reload(dest, source, len, filename);
    return 0;
}

static int pico8_cstore(lua_State *L)
{
    int dest = fix32_to_int(luaL_optnumber(L, 1, 0));
    int source = fix32_to_int(luaL_optnumber(L, 2, 0));
    int len = fix32_to_int(luaL_optnumber(L, 3, 0x4300));
    const char *filename = NULL;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) filename = lua_tostring(L, 4);
    pico8_cart_cstore(dest, source, len, filename);
    return 0;
}

static int pico8_run(lua_State *L)
{
    const char *param = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) param = lua_tostring(L, 1);
    /* Restart from the beginning of the program: ask the host to reload the
       current cart (if any) with the new parameter string. */
    pico8_console_queue_load(NULL, NULL, param);
    pico8_vm_halted = 1;
    return 0;
}

static int pico8_reset(lua_State *L)
{
    (void)L;
    pico8_reset_draw_state();
    return 0;
}

static int pico8_stop(lua_State *L)
{
    (void)L;
    pico8_vm_halted = 1;
    return 0;
}


/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg cart_api[] = {
    { "cartdata", pico8_cartdata }, { "dget", pico8_dget },
    { "dset",     pico8_dset     },
    { "save",     pico8_save     }, { "load", pico8_load },
    { "reload",   pico8_reload   }, { "cstore", pico8_cstore },
    { "run",      pico8_run      }, { "reset", pico8_reset },
    { "stop",     pico8_stop     },
    { NULL, NULL }
};

void pico8_api_register_cart(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = cart_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
