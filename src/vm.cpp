/*
 * vm.cpp — the bridge to the Lua virtual machine.
 *
 * The cart is a Lua program, and this file is how the rest of the
 * emulator talks to it: the state is created here, the source is compiled
 * here, and the _init, _update and _draw of the cart are called from here.
 *
 * 카트는 Lua 프로그램이고, 이 파일은 나머지 에뮬레이터가 그와 대화하는
 * 방법이다. 상태를 여기서 만들고, 소스를 여기서 컴파일하고, 카트의 _init,
 * _update, _draw를 여기서 부른다.
 *
 * The VM is z8lua, a Lua 5.2 fork that keeps its numbers the way PICO-8
 * does (32 bit fixed point) and that reads and writes the RAM of the cart
 * directly.  See platforms/common/deps/z8lua.
 *
 * VM은 z8lua다. PICO-8과 같은 방식으로 숫자를 저장하고(32비트 고정소수) 카트의
 * RAM을 직접 읽고 쓸 수 있는 Lua 5.2 포크다. platforms/common/deps/z8lua 참고.
 *
 * Two things here are not obvious:
 *
 *   - the error report.  The oracle prints a runtime error as a small
 *     block with the failing line, the source line, the message and one
 *     line per Lua frame.  runtime_error_block() builds exactly that.
 *   - the global namespace.  A stock Lua state has far more globals than a
 *     real PICO-8, so pico8_apply_parity() takes the extra ones away and
 *     adds the ones that only PICO-8 has (the glyph constants, the host
 *     internals, inext).
 *
 * 여기서 자명하지 않은 두 가지:
 *
 *   - 오류 보고. 오라클은 런타임 오류를 실패한 줄, 그 소스 줄, 메시지, 그리고
 *     Lua 프레임마다 한 줄씩으로 이루어진 작은 블록으로 출력한다.
 *     runtime_error_block()이 정확히 그것을 만든다.
 *   - 전역 네임스페이스. 표준 Lua 상태에는 실제 PICO-8보다 전역 이름이 훨씬
 *     많으므로, pico8_apply_parity()가 남는 것을 걷어내고 PICO-8에만 있는
 *     것(글자 상수, 호스트 내부 함수, inext)을 더한다.
 */

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

/*
 * pico8_vm_set_cmdline(args) — the parameter string of the cart.
 *
 * The cart reads it with stat(6).  It is the text that was given after -p
 * on the command line.
 *
 * 카트는 stat(6)으로 이 값을 읽는다. 커맨드 라인에서 -p 뒤에 준 문자열이다.
 */
void pico8_vm_set_cmdline(const char *args)
{
    pico8_cmdline = args;
}

/*
 * pico8_alloc(ud, ptr, osize, nsize) — the memory of the Lua state.
 *
 * The VM hands every allocation to this function.  There is no limit and
 * no accounting: the cart has the whole 64KB of the machine for its data
 * area, and a cart that allocates too much simply runs out of memory.
 *
 * Lua 상태가 하는 모든 할당이 이 함수로 온다. 한계도, 집계도 없다. 카트
 * 데이터 영역으로는 기계의 64KB 전체를 쓸 수 있고, 너무 많이 할당한 카트는
 * 그냥 메모리가 바닥난다.
 */
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

/*
 * pico8_vm_error_reported() — was the last error already shown?
 *
 * The host asks this before it prints its own message, so that an error
 * that the VM already reported is not reported twice.
 *
 * 호스트가 자기 메시지를 찍기 전에 이걸 물어본다. VM이 이미 보고한 오류를
 * 두 번 보고하지 않기 위해서다.
 */
int pico8_vm_error_reported(void) { return g_error_reported; }

/*
 * error_position(msg, text) — the line number of an error message.
 *
 * The message of Lua has the shape "<chunk>:<line>: <text>".  The line
 * number and the text are picked out of it; a message without a line
 * number gives 0.
 *
 * Lua의 오류 메시지는 "<chunk>:<line>: <text>" 꼴이다. 여기서 줄 번호와
 * 텍스트를 뽑아낸다. 줄 번호가 없으면 0을 돌려준다.
 */
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

/*
 * chunk_line(line) — the source line of the chunk.
 *
 * The given 1-based line of the chunk that is being run, copied out
 * exactly as it was written.  A line that does not exist gives an empty
 * string.
 *
 * 실행 중인 청크의 1부터 세는 해당 줄을, 쓰인 그대로 복사해 온다. 없는
 * 줄이면 빈 문자열이다.
 */
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

/*
 * runtime_error_block(msg) — the error block of a runtime error.
 *
 * Puts together the whole block the oracle prints: the line of the error,
 * the source line, the message and one line per Lua frame, from the
 * innermost frame out.
 *
 * 오라클이 출력하는 오류 블록 전체를 짜낸다. 오류가 난 줄, 그 소스 줄,
 * 메시지, 그리고 Lua 프레임마다 한 줄씩(안쪽 프레임부터 바깥쪽 순서).
 */
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

/*
 * traceback(L) — the error handler of the host.
 *
 * Puts the error block on the stack instead of the plain Lua message, so
 * that the host sees the same text the oracle prints.
 *
 * 일반적인 Lua 메시지 대신 오류 블록을 스택에 올린다. 그래야 호스트가
 * 오라클과 똑같은 텍스트를 보게 된다.
 */
static int traceback(lua_State *L)
{
    std::string block = runtime_error_block(lua_tostring(L, 1));
    lua_pushlstring(L, block.data(), block.size());
    return 1;
}

/* Push the shared handler used whenever the host calls into cart code.
   PICO-8 has no global __traceback (see pico8_apply_parity), so the handler
   is reachable from C only. */

/*
 * pico8_vm_push_traceback(L) — the error handler on the stack.
 *
 * Pushes the handler that is used whenever the host calls into the code of
 * a cart.  PICO-8 has no global __traceback, so the handler can only be
 * reached from C.
 *
 * 호스트가 카트 코드를 부를 때마다 쓰는 에러 핸들러를 스택에 올린다.
 * PICO-8에는 전역 __traceback가 없으므로 이 핸들러는 C에서만 닿을 수 있다.
 */
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

/*
 * parity_set_global(L, name, fn) — put a function into the global table.
 *
 * 숫자 네임스페이스 정합성을 맞추기 위한 도우미. 전역 테이블에 함수를
 * 넣는다.
 */
static void parity_set_global(lua_State *L, const char *name, lua_CFunction fn)
{
    lua_pushcfunction(L, fn);
    lua_setglobal(L, name);
}

/*
 * parity_clear_global(L, name) — take a name out of the global table.
 *
 * 숫자 네임스페이스 정합성을 맞추기 위한 도우미. 전역 테이블에서 이름을
 * 지운다.
 */
static void parity_clear_global(lua_State *L, const char *name)
{
    lua_pushnil(L);
    lua_setglobal(L, name);
}

/* Host internals that PICO-8 declares so carts may call them. They take no
   interesting arguments and return *zero* values rather than nil -- which is
   why `tostr(_set_fps(30))` prints an empty string on real hardware. */

/*
 * parity_noop(L) — a function that does nothing.
 *
 * PICO-8 declares a number of host internals so that a cart may call
 * them.  They take no interesting arguments and return *zero* values
 * rather than nil, which is why tostr(_set_fps(30)) prints an empty
 * string on real hardware.
 *
 * PICO-8은 카트가 부를 수 있도록 여러 호스트 내부를 선언한다. 이들은
 * 흥미로운 인자를 받지 않고 nil이 아니라 *값을 0개* 돌려준다. 그래서 실제
 * 하드웨어에서 tostr(_set_fps(30))은 빈 문자열을 출력한다.
 */
static int parity_noop(lua_State *L)
{
    (void)L;
    return 0;
}

/* PICO-8's inext(t, i): starts at index 0 and returns `i+1, t[i+1]`, or
   nothing at all when that slot is empty (it never walks holes). */

/*
 * parity_inext(L, t, i) — the next entry of a table.
 *
 * PICO-8's inext(t, i) starts at index 0 and returns "i+1, t[i+1]", or
 * nothing at all when that slot is empty.  It never walks the holes of a
 * table.
 *
 * PICO-8의 inext(t, i)는 인덱스 0에서 시작해서 "i+1, t[i+1]"을 돌려주지만,
 * 그 자리가 비어 있으면 아무것도 돌려주지 않는다. 테이블의 구멍은 절대
 * 건너뛰지 않는다.
 */
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

/*
 * pico8_apply_parity(L) — make the global table look like a PICO-8.
 *
 * luaL_openlibs() hands us a stock Lua environment, but a real PICO-8
 * exposes a far smaller one: no pcall/error, no string/table/coroutine/
 * debug libraries, plus glyph constants and a few host internals.  All of
 * this was measured against the oracle, so that `for k,v in pairs(_ENV)`
 * enumerates exactly the same names.
 *
 * luaL_openlibs()는 표준 Lua 환경을 주지만, 실제 PICO-8이 노출하는 환경은 훨씬
 * 작다: pcall/error가 없고, string/table/coroutine/debug 라이브러리도 없다.
 * 그 대신 글자 상수와 몇 가지 호스트 내부 함수가 있다. 전부 오라클에서 측정한
 * 것이라 `for k,v in pairs(_ENV)`가 정확히 같은 이름들을 열거한다.
 */
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

/*
 * pico8_vm_init() — create the Lua state.
 *
 * Creates the state, hands it the RAM of the machine, opens the standard
 * libraries, registers the PICO-8 API and then trims the global table
 * down to what a real PICO-8 has.
 *
 * 상태를 만들고, 기계의 RAM을 넘기고, 표준 라이브러리를 열고, PICO-8 API를
 * 등록한 다음, 전역 테이블을 실제 PICO-8과 같은 모양으로 줄인다.
 */
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

/*
 * pico8_vm_destroy() — close the Lua state.
 *
 * The state is closed and the pointer cleared, so that a later call does
 * not use a state that is gone.
 *
 * 상태를 닫고 포인터를 비운다. 그래야 나중에 부른 코드가 없어진 상태를
 * 쓰지 않는다.
 */
void pico8_vm_destroy(void)
{
    if (L) {
        lua_close(L);
        L = NULL;
    }
}

/*
 * pico8_vm_load_buffer(buf, len, name) — load the source of a cart.
 *
 * Compiles the source and runs the main chunk.  A syntax error is printed
 * in the same block the oracle uses ("syntax error line 4 (tab 0)"), and
 * a runtime error in the cart comes out as the error block.
 *
 * 소스를 컴파일하고 메인 청크를 실행한다. 문법 오류는 오라클과 같은 블록
 * ("syntax error line 4 (tab 0)")으로 출력하고, 카트 안에서 난 런타임 오류는
 * 오류 블록으로 나온다.
 */
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

/*
 * pico8_vm_has_function(name) — does the cart define that function?
 *
 * Looks the name up in the global table and reports whether it is a
 * function.  This is how the host finds out about _init, _draw, _update
 * and _update60.
 *
 * 전역 테이블에서 이름을 찾아 함수가 있는지 알려준다. 호스트가 _init,
 * _draw, _update, _update60을 알아내는 방법이다.
 */
int pico8_vm_has_function(const char *name)
{
    if (!L) return 0;
    lua_getglobal(L, name);
    int ok = lua_isfunction(L, -1);
    lua_pop(L, 1);
    return ok;
}

/*
 * call_function(name) — call one function of the cart.
 *
 * Calls the named function of the cart with the error handler of the
 * host.  When the call fails the error block is printed once and the
 * cart is stopped, because PICO-8 stops the cart when a frame callback
 * fails: it prints the error once, not on every frame.
 *
 * 카트의 이름 있는 함수를 호스트의 에러 핸들러와 함께 부른다. 호출이
 * 실패하면 오류 블록을 한 번 출력하고 카트를 멈춘다. PICO-8이 프레임
 * 콜백이 실패하면 카트를 멈추기 때문이다. 오류는 매 프레임이 아니라 한 번만
 * 출력한다.
 */
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

/*
 * pico8_vm_call_init() — the _init of the cart.
 *
 * 카트의 _init을 부른다.
 */
void pico8_vm_call_init(void)
{
    call_function("_init");
}

/*
 * pico8_vm_call_update() — the update of the cart.
 *
 * A cart with _update60 is updated through that function, a cart with
 * only _update through that one.  A cart with neither is not updated at
 * all.
 *
 * _update60이 있는 카트는 그 함수로, _update만 있는 카트는 그 함수로
 * 업데이트된다. 둘 다 없는 카트는 아예 업데이트되지 않는다.
 */
void pico8_vm_call_update(void)
{
    if (pico8_vm_has_function("_update60")) {
        call_function("_update60");
    } else if (pico8_vm_has_function("_update")) {
        call_function("_update");
    }
}

/*
 * pico8_vm_call_draw() — the _draw of the cart.
 *
 * 카트의 _draw를 부른다.
 */
void pico8_vm_call_draw(void)
{
    call_function("_draw");
}

/* Invoke a pause-menu callback (used by the shell's "menu" command).
   Returns 1 when the callback asked the menu to stay open. */

/*
 * pico8_vm_call_menuitem(index, buttons) — a callback of the pause menu.
 *
 * Calls the callback of a menu entry and reports whether the entry asked
 * the menu to stay open.
 *
 * 메뉴 항목의 callback을 부르고, 그 항목이 메뉴를 계속 띄워 달라고 했는지
 * 알려준다.
 */
int pico8_vm_call_menuitem(int index, int buttons)
{
    return pico8_menuitem_invoke(L, index, buttons);
}

/* Format a value the way PICO-8's console does: numbers use fixed-point
   formatting (integers plain, otherwise 4 decimals with trailing zeros
   trimmed), strings are shown bare and nil shows nothing. */

/*
 * console_tostring(L, idx) — a value the way the console of PICO-8 shows it.
 *
 * Numbers use the fixed point formatting (integers plain, otherwise four
 * decimals with the trailing zeros trimmed), strings are shown bare, nil
 * shows nothing and a boolean shows "true" or "false".
 *
 * 숫자는 고정소수 형식을 쓴다(정수는 그대로, 그 외에는 소수점 넷째 자리까지
 * 쓰고 뒤의 0을 지운다). 문자열은 그대로, nil은 아무것도 아니고, 논리값은
 * "true"나 "false"로 보인다.
 */
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

/*
 * pico8_vm_eval(line) — run one line of the console.
 *
 * The line is first tried as a statement.  Only when that does not
 * compile is it tried again as an expression, and then the result is
 * echoed, which is why `print(1+2)` shows just `3` while a bare `1+2`
 * shows `3` too.  The typed characters become charset bytes first, the
 * way the source of a cart does.
 *
 * 셸의 한 줄을 실행한다. 먼저 문장(statement)으로 시도하고, 컴파일되지 않을
 * 때만 식(expression)으로 다시 시도한 다음 결과를 보여준다. 그래서
 * `print(1+2)`도 `3`만 보여주고, 그냥 `1+2`도 `3`을 보여준다. 입력한 문자는
 * 카트 소스와 마찬가지로 먼저 charset 바이트가 된다.
 */
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
