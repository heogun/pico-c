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

/* ------------------------------------------------------------------ *
 *  PICO-8 error reporting
 *
 *  The oracle prints cart errors to stdout as a small block (measured
 *  2026-10-20 against PICO-8 0.2.7):
 *
 *    runtime error line 24 tab 0
 *    printh("g1="..ord(g,1).." g2="..ord(g,2))   <- the source line
 *    attempt to concatenate a nil value          <- the message
 *    in inner line 2 (tab 0)                     <- one per Lua frame,
 *    in outer line 5 (tab 0)                        innermost first
 *    at line 7 (tab 0)                           <- outermost frame
 *
 *  A load failure prints the same block with "syntax error line 4 (tab 0)"
 *  and no frames. When the engine itself calls the failing function there is
 *  no main chunk frame and the last line is "at line 1 (tab 1)" under _init
 *  and "at line 0 (tab 0)" under _draw/_update.
 *
 *  The source echo is not transcoded: the oracle emits the raw charset bytes
 *  of the line (an identifier like ˇ comes out as byte 149).
 * ------------------------------------------------------------------ */

static std::string g_chunk_text;    /* text of the chunk being run */
static int g_error_reported = 0;
/* Name the host used to call into the cart (_init, _draw, ...). Lua cannot
   name that outermost frame because the call comes from C. */
static const char *g_entry_name = "";

int pico8_vm_error_reported(void) { return g_error_reported; }

/* Split "<chunk>:<line>: <message>" into its line number and message text. */
static int error_position(const char *msg, std::string &text)
{
    text = msg ? msg : "";
    if (!msg) return 0;
    for (const char *p = msg; *p && *p != '\n'; p++) {
        if (*p != ':' || p[1] < '0' || p[1] > '9') continue;
        const char *q = p + 1;
        while (*q >= '0' && *q <= '9') q++;
        if (q[0] != ':' || q[1] != ' ') continue;
        text.assign(q + 2);
        return atoi(p + 1);
    }
    return 0;
}

/* The given 1-based line of the loaded chunk, verbatim. */
static std::string chunk_line(int line)
{
    if (line < 1) return "";
    size_t pos = 0;
    for (int n = 1; n < line; n++) {
        pos = g_chunk_text.find('\n', pos);
        if (pos == std::string::npos) return "";
        pos++;
    }
    size_t end = g_chunk_text.find('\n', pos);
    if (end == std::string::npos) end = g_chunk_text.size();
    return g_chunk_text.substr(pos, end - pos);
}

struct ErrorFrame {
    char name[64];
    int  line;
    bool is_main;
    bool named;
};

/* Build the whole runtime error block from the live call stack. */
static std::string runtime_error_block(const char *msg)
{
    std::string text;
    int line = error_position(msg, text);

    ErrorFrame frames[32];
    int nframes = 0;
    for (int level = 1; nframes < 32; level++) {
        lua_Debug ar;
        if (!lua_getstack(L, level, &ar)) break;
        if (!lua_getinfo(L, "lnS", &ar)) break;
        if (ar.currentline < 0) continue;          /* C frame: not shown */
        ErrorFrame &f = frames[nframes++];
        snprintf(f.name, sizeof f.name, "%s",
                 (ar.name && *ar.name) ? ar.name : "?");
        f.named = (ar.name && *ar.name) != NULL;
        f.line = ar.currentline;
        f.is_main = ar.what && strcmp(ar.what, "main") == 0;
    }
    if (nframes > 0 && !frames[nframes - 1].named && *g_entry_name)
        snprintf(frames[nframes - 1].name, sizeof frames[nframes - 1].name,
                 "%s", g_entry_name);
    if (line <= 0) line = (nframes > 0) ? frames[0].line : 0;

    std::string out;
    out += "runtime error line " + std::to_string(line) + " tab 0\n";
    out += chunk_line(line) + "\n";
    out += text + "\n";
    for (int i = 0; i < nframes; i++) {
        if (frames[i].is_main) {
            out += "at line " + std::to_string(frames[i].line) + " (tab 0)\n";
        } else {
            out += "in ";
            out += frames[i].name;
            out += " line " + std::to_string(frames[i].line) + " (tab 0)\n";
        }
    }
    if (nframes == 0 || !frames[nframes - 1].is_main) {
        if (nframes > 0 && strcmp(frames[nframes - 1].name, "_init") == 0)
            out += "at line 1 (tab 1)\n";
        else
            out += "at line 0 (tab 0)\n";
    }
    return out;
}

static int traceback(lua_State *L)
{
    std::string block = runtime_error_block(lua_tostring(L, 1));
    lua_pushlstring(L, block.data(), block.size());
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
       oracle with tostr(). Names are written as UTF-8 and converted to
       charset bytes at registration (see src/charset.cpp). */
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
        /* Cart text is charset bytes (see src/charset.cpp), so the names
           register under the byte PICO-8 itself uses for each glyph. */
        std::string name = pico8_charset_from_utf8(kGlyphGlobals[i].name,
                                                   strlen(kGlyphGlobals[i].name));
        lua_setglobal(L, name.c_str());
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

    g_chunk_text.assign(buf, len);
    g_error_reported = 0;
    g_entry_name = "";

    int err = luaL_loadbuffer(L, buf, len, name);
    if (err) {
        std::string text;
        int line = error_position(lua_tostring(L, -1), text);
        std::string block;
        block += "syntax error line " + std::to_string(line) + " (tab 0)\n";
        block += chunk_line(line) + "\n";
        block += text + "\n";
        fputs(block.c_str(), stdout);
        fflush(stdout);
        g_error_reported = 1;
        lua_pop(L, 1);
        return err;
    }

    lua_pushcfunction(L, traceback);
    lua_insert(L, -2);
    err = lua_pcall(L, 0, 0, -2);
    if (err) {
        const char *block = lua_tostring(L, -1);
        if (block) fputs(block, stdout);
        fflush(stdout);
        g_error_reported = 1;
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
    g_entry_name = name;
    pico8_vm_push_traceback(L);
    lua_insert(L, -2);
    if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
        const char *block = lua_tostring(L, -1);
        if (block) fputs(block, stdout);
        fflush(stdout);
        g_error_reported = 1;
        /* PICO-8 stops the cart when a frame callback fails: it prints the
           error once, not on every frame. */
        pico8_vm_halted = 1;
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

    /* Console input is source text too: characters typed or pasted at the
       shell become charset bytes here, the way cart source does. */
    std::string line8 = pico8_charset_from_utf8(line, strlen(line));
    line = line8.c_str();

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
