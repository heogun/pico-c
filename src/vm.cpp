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

/* Push the shared handler used whenever the host calls into cart code.
   PICO-8 has no global __traceback (see pico8_apply_parity), so the handler
   is reachable from C only. */
void pico8_vm_push_traceback(lua_State *L)
{
    lua_pushcfunction(L, traceback);
}

/* ------------------------------------------------------------------ *
 *  Global namespace parity with real PICO-8
 *
 *  luaL_openlibs() hands us a stock Lua environment, but a real PICO-8
 *  exposes a far smaller one: no pcall/error, no string/table/coroutine/
 *  debug libraries, plus glyph constants and a few host internals. All of
 *  this was measured against the oracle so that
 *  `for k,v in pairs(_ENV)` enumerates exactly the same names.
 * ------------------------------------------------------------------ */

static void parity_set_global(lua_State *L, const char *name, lua_CFunction fn)
{
    lua_pushcfunction(L, fn);
    lua_setglobal(L, name);
}

static void parity_clear_global(lua_State *L, const char *name)
{
    lua_pushnil(L);
    lua_setglobal(L, name);
}

/* Host internals that PICO-8 declares so carts may call them. They take no
   interesting arguments and return *zero* values rather than nil -- which is
   why `tostr(_set_fps(30))` prints an empty string on real hardware. */
static int parity_noop(lua_State *L)
{
    (void)L;
    return 0;
}

/* PICO-8's inext(t, i): starts at index 0 and returns `i+1, t[i+1]`, or
   nothing at all when that slot is empty (it never walks holes). */
static int parity_inext(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_Integer next = (lua_Integer)luaL_optnumber(L, 2, (lua_Number)0) + 1;
    lua_pushinteger(L, next);
    lua_rawget(L, 1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    lua_pushinteger(L, next);
    lua_insert(L, -2);
    return 2;
}

static void pico8_apply_parity(lua_State *L)
{
    /* Host internals: no-op functions (see parity_noop). */
    static const char *const kNoopGlobals[] = {
        "__flip", "__trace",
        "_get_menu_item_selected", "_map_display", "_mark_cpu", "_menuitem",
        "_set_fps", "_set_mainloop_exists", "_startframe", "_update_buttons",
        "_update_framerate", "holdframe", "mapdraw", "scoresub",
        "set_draw_slice", "trace",
        NULL
    };

    /* Names a stock Lua state defines that real PICO-8 does not have.
       (seconds/resume are our own extensions and are no longer registered.) */
    static const char *const kRemovedGlobals[] = {
        "_G", "_VERSION", "__pico8_time", "__traceback",
        "collectgarbage", "coroutine", "debug", "dofile", "error",
        "loadfile", "pcall", "string", "table", "tonumber", "xpcall",
        NULL
    };

    /* Glyph/button constants; values are fixed-point readings taken from the
       oracle with tostr(). Keys are exact UTF-8 byte sequences. */
    static const struct { const char *name; double value; } kGlyphGlobals[] = {
        { "\342\226\210", 0.5 },   /* █ U+2588 */
        { "\342\226\222", 23130.5 },   /* ▒ U+2592 */
        { "\360\237\220\261", 20767.5 },   /* 🐱 U+1F431 */
        { "\342\254\207\357\270\217", 3 },   /* ⬇️ U+2B07 U+FE0F */
        { "\342\226\221", 32125.5 },   /* ░ U+2591 */
        { "\342\234\275", -18402.5 },   /* ✽ U+273D */
        { "\342\227\217", -1632.5 },   /* ● U+25CF */
        { "\342\231\245", 20927.5 },   /* ♥ U+2665 */
        { "\342\230\211", -19008.5 },   /* ☉ U+2609 */
        { "\354\233\203", -26208.5 },   /* 웃 U+C6C3 */
        { "\342\214\202", -20192.5 },   /* ⌂ U+2302 */
        { "\342\254\205\357\270\217", 0 },   /* ⬅️ U+2B05 U+FE0F */
        { "\360\237\230\220", -24351.5 },   /* 😐 U+1F610 */
        { "\342\231\252", -25792.5 },   /* ♪ U+266A */
        { "\360\237\205\276\357\270\217", 4 },   /* 🅾️ U+1F17E U+FE0F */
        { "\342\227\206", -20032.5 },   /* ◆ U+25C6 */
        { "\342\200\246", -2560.5 },   /* … U+2026 */
        { "\342\236\241\357\270\217", 1 },   /* ➡️ U+27A1 U+FE0F */
        { "\342\230\205", -20128.5 },   /* ★ U+2605 */
        { "\342\247\227", 6943.5 },   /* ⧗ U+29D7 */
        { "\342\254\206\357\270\217", 2 },   /* ⬆️ U+2B06 U+FE0F */
        { "\313\207", -2624.5 },   /* ˇ U+02C7 */
        { "\342\210\247", 31455.5 },   /* ∧ U+2227 */
        { "\342\235\216", 5 },   /* ❎ U+274E */
        { "\342\226\244", 3855.5 },   /* ▤ U+25A4 */
        { "\342\226\245", 21845.5 },   /* ▥ U+25A5 */
        { NULL, 0 }
    };

    /* Coroutine API is flattened into globals before coroutine disappears
       (PICO-8 has no coroutine table and no wrap). */
    static const struct { const char *global_name; const char *member; } kFlatCoro[] = {
        { "cocreate", "create" },
        { "coresume", "resume" },
        { "costatus", "status" },
        { "yield",    "yield"  },
    };
    lua_getglobal(L, "coroutine");
    if (lua_istable(L, -1)) {
        for (size_t i = 0; i < sizeof kFlatCoro / sizeof kFlatCoro[0]; i++) {
            lua_getfield(L, -1, kFlatCoro[i].member);
            lua_setglobal(L, kFlatCoro[i].global_name);
        }
    }
    lua_pop(L, 1);

    /* __type(x) is exactly type(x) on the oracle. */
    lua_getglobal(L, "type");
    lua_setglobal(L, "__type");

    parity_set_global(L, "inext", parity_inext);
    for (size_t i = 0; kNoopGlobals[i]; i++)
        parity_set_global(L, kNoopGlobals[i], parity_noop);

    /* _pausemenu is an empty table on the oracle. */
    lua_newtable(L);
    lua_setglobal(L, "_pausemenu");

    for (size_t i = 0; kGlyphGlobals[i].name; i++) {
        lua_pushnumber(L, (lua_Number)kGlyphGlobals[i].value);
        lua_setglobal(L, kGlyphGlobals[i].name);
    }

    for (size_t i = 0; kRemovedGlobals[i]; i++)
        parity_clear_global(L, kRemovedGlobals[i]);
}

int pico8_vm_init(void)
{
    L = lua_newstate(pico8_alloc, NULL);
    if (!L) return -1;

    lua_setpico8memory(L, pico8_ram);

    luaL_openlibs(L);

    pico8_register_api(L);

    /* Strip stock Lua globals and add the PICO-8-only ones; needs the API
       table above to be registered first. */
    pico8_apply_parity(L);

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
    pico8_vm_push_traceback(L);
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
