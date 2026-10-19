#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "pico8.h"

static lua_State *L = NULL;
const char *pico8_cmdline = NULL;
int pico8_vm_halted = 0;
uint32_t pico8_frame_count = 0;

void pico8_vm_set_cmdline(const char *args)
{
    pico8_cmdline = args;
}

static void *pico8_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nsize);
}

static int traceback(lua_State *L)
{
    const char *msg = lua_tostring(L, 1);
    if (msg) {
        luaL_traceback(L, L, msg, 1);
    } else {
        lua_pushliteral(L, "nil error");
    }
    return 1;
}

int pico8_vm_init(void)
{
    L = lua_newstate(pico8_alloc, NULL);
    if (!L) return -1;

    lua_setpico8memory(L, pico8_ram);

    luaL_openlibs(L);

    lua_pushcfunction(L, traceback);
    lua_setglobal(L, "__traceback");

    pico8_register_api(L);

    lua_pushnumber(L, (lua_Number)0);
    lua_setglobal(L, "__pico8_time");

    pico8_menuitem_clear();

    return 0;
}

void pico8_vm_destroy(void)
{
    if (L) {
        lua_close(L);
        L = NULL;
    }
}

int pico8_vm_load_buffer(const char *buf, size_t len, const char *name)
{
    if (!L) return -1;

    int err = luaL_loadbuffer(L, buf, len, name);
    if (err) {
        fprintf(stderr, "Lua load error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return err;
    }

    lua_pushcfunction(L, traceback);
    lua_insert(L, -2);
    err = lua_pcall(L, 0, 0, -2);
    if (err) {
        fprintf(stderr, "Lua run error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return err;
    }
    lua_pop(L, 1);

    return 0;
}

int pico8_vm_has_function(const char *name)
{
    if (!L) return 0;
    lua_getglobal(L, name);
    int ok = lua_isfunction(L, -1);
    lua_pop(L, 1);
    return ok;
}

static void call_function(const char *name)
{
    if (!L) return;
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_getglobal(L, "__traceback");
    lua_insert(L, -2);
    if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
        fprintf(stderr, "Error in %s: %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);
}

void pico8_vm_call_init(void)
{
    call_function("_init");
}

void pico8_vm_call_update(void)
{
    if (pico8_vm_has_function("_update60")) {
        call_function("_update60");
    } else if (pico8_vm_has_function("_update")) {
        call_function("_update");
    }
}

void pico8_vm_call_draw(void)
{
    call_function("_draw");
}

/* Invoke a pause-menu callback (used by the shell's "menu" command).
   Returns 1 when the callback asked the menu to stay open. */
int pico8_vm_call_menuitem(int index, int buttons)
{
    return pico8_menuitem_invoke(L, index, buttons);
}

/* Format a value the way PICO-8's console does: numbers use fixed-point
   formatting (integers plain, otherwise 4 decimals with trailing zeros
   trimmed), strings are shown bare and nil shows nothing. */
static std::string console_tostring(lua_State *L, int idx)
{
    if (lua_isnumber(L, idx)) {
        double v = (double)lua_tonumber(L, idx);
        char b[64];
        if (v == floor(v) && fabs(v) < 1.0e9) {
            snprintf(b, sizeof b, "%d", (int)v);
            return b;
        }
        snprintf(b, sizeof b, "%.4f", v);
        std::string s = b;
        size_t dot = s.find('.');
        if (dot != std::string::npos) {
            size_t last = s.find_last_not_of('0');
            if (s[last] == '.') last--;
            s = s.substr(0, last + 1);
        }
        return s;
    }
    if (lua_isnil(L, idx))    return "";
    if (lua_isboolean(L, idx)) return lua_toboolean(L, idx) ? "true" : "false";
    if (lua_isstring(L, idx))  return lua_tostring(L, idx);

    size_t len = 0;
    const char *s = luaL_tolstring(L, idx, &len);   /* pushes a string */
    std::string out = s ? s : "";
    lua_pop(L, 1);
    return out;
}

int pico8_vm_eval(const char *line)
{
    if (!L || !line) return -1;

    /* PICO-8's console first tries the line as a statement. Only if that does
       not compile does it re-try as an expression and echo the result -- which
       is why `print(1+2)` shows just `3` while a bare `1+2` shows `3` too. */
    int status = luaL_loadbuffer(L, line, strlen(line), "=console");
    if (status == LUA_OK) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            pico8_console_emit(lua_tostring(L, -1), 8);
            lua_pop(L, 1);
            return -1;
        }
        return 0;
    }
    lua_pop(L, 1);                                 /* drop the syntax error */

    std::string expr = std::string("return ") + line;
    if (luaL_loadbuffer(L, expr.c_str(), expr.size(), "=console") != LUA_OK) {
        pico8_console_emit(lua_tostring(L, -1), 8);
        lua_pop(L, 1);
        return -1;
    }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        pico8_console_emit(lua_tostring(L, -1), 8);
        lua_pop(L, 1);
        lua_settop(L, 0);
        return -1;
    }
    int nres = lua_gettop(L);
    if (nres > 0) {
        std::string out;
        for (int i = 1; i <= nres; i++) {
            if (i > 1) out += "\t";
            out += console_tostring(L, i);
        }
        if (!out.empty())
            pico8_console_emit(out.c_str(), 6);
    }
    lua_settop(L, 0);
    return 0;
}
