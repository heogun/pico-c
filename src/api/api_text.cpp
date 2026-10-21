/*
 * api_text.cpp — the text part of the PICO-8 API.
 *
 * print() and printh() write text, sub()/ord()/pack() read it back.  The
 * interesting part is print(): PICO-8 prints p8scii, its own encoding in
 * which the first 16 bytes are control codes, so the cursor can be moved,
 * the colours changed or a glyph repeated.  print_walk() below is the
 * interpreter of those codes.
 *
 * print()와 printh()가 텍스트를 출력하고, sub()/ord()/pack()가 그것을 읽는다.
 * 핵심은 print()다. PICO-8은 p8scii(자체 인코딩, 앞 16바이트가 제어 코드)를
 * 출력하기 때문에 커서 이동, 색 변경, 글자 반복이 가능하다. 아래 print_walk()가
 * 그 제어 코드를 해석한다.
 *
 * The text itself is charset bytes, not UTF-8: PICO-8 keeps its own font
 * codes inside the VM and only converts at the edges (cart files, console
 * output).  See src/charset.cpp.
 *
 * 텍스트 자체는 UTF-8이 아니라 charset 바이트다. PICO-8은 VM 안에서 자기 폰트
 * 코드를 쓰고 경계(카트 파일, 콘솔 출력)에서만 변환한다. src/charset.cpp 참고.
 */

#include <string.h>

#include "api_internal.h"

/*
** Advance in pixels of one charset byte when printing, measured on the
** oracle with print(chr(c)) for every c: ASCII and the small funny-font
** glyphs (16..127) take 4px, everything from 128 up -- the wide graphics
** glyphs, emoji, hiragana and katakana -- takes 8px, and the control codes
** 0..15 don't move the cursor at all. Tab and newline are cursor moves and
** are handled by the caller.
*/
static int print_advance(uint8_t c)
{
    if (c >= 128) return 8;
    if (c >= 16)  return 4;
    return 0;
}

/*
** p8scii text state for one print() call. The control codes, measured on the
** oracle:
**   0       end of string
**   1  \*   hex digit count, then the next byte repeated count times
**   2  \#   hex digit background colour (invalid digit = black)
**   3  \-   cursor x = x - 16 + hex digit (undocumented cursor op)
**   4  \|   cursor y = y - 16 + hex digit (undocumented cursor op)
**   5  \+   undocumented cursor op: takes one byte and ends the string
**   6  \^   i/w/t/p modes ("-" prefix clears them), unknown letters ignored
**   7  \a   beep: takes one byte and ends the string
**   8  \b   backspace: cursor x -= 4
**   9  \t   tab: cursor x up to the next multiple of 16
**   10 \n   newline: x = print origin, y += 6
**   11      undocumented: takes one byte and ends the string
**   12 \f   hex digit foreground colour (invalid digit = black)
**   13 \r   carriage return: x = print origin
**   14      undocumented: takes one byte and ends the string
**   15      draws the code's own glyph (4px) and eats one byte, if any
*/
typedef struct {
    int cx, cy, startx;
    uint8_t fg;
    int has_bg;
    uint8_t bg;
    int invert, wide, tall, pinball;
} print_state;

static int hex_digit(uint8_t c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Draw one glyph at the cursor and move past it. */
static void print_emit(print_state *st, uint8_t c, int draw)
{
    if (draw) {
        int dx = st->cx, dy = st->cy;
        api_apply_camera(&dx, &dy);
        pico8_draw_char_text(dx, dy, c, st->fg, st->has_bg, st->bg,
                             st->invert, st->wide, st->tall, st->pinball);
    }
    int adv = print_advance(c);
    if (st->wide || st->pinball) adv *= 2;
    st->cx += adv;
}

/* Walk a p8scii string, updating the cursor and (when draw is set) painting
   the screen. */
static void print_walk(const char *text, print_state *st, int draw)
{
    for (int i = 0; text[i]; ) {
        uint8_t c = (uint8_t)text[i];
        switch (c) {
        case '\b':
            st->cx -= 4;
            i++;
            continue;
        case '\t':
            st->cx = (st->cx / 16 + 1) * 16;
            i++;
            continue;
        case '\n':
            st->cx = st->startx;
            st->cy += 6;
            i++;
            continue;
        case '\r':
            st->cx = st->startx;
            i++;
            continue;
        case 1: {                       /* \* repeat next byte */
            uint8_t cnt = (uint8_t)text[i + 1];
            if (!cnt) { i++; continue; }
            uint8_t rep = (uint8_t)text[i + 2];
            if (!rep) { i += 2; continue; }
            if (rep < 16) {             /* repeated control: apply once */
                i += 2;
                continue;
            }
            int n = hex_digit(cnt);
            if (n < 0) n = 0;
            for (int k = 0; k < n; k++) print_emit(st, rep, draw);
            i += 3;
            continue;
        }
        case 2:                         /* \# background colour */
        case 12: {                      /* \f foreground colour */
            uint8_t arg = (uint8_t)text[i + 1];
            if (!arg) { i++; continue; }
            int v = hex_digit(arg);
            if (v < 0) v = 0;
            if (c == 2) {
                st->bg = (uint8_t)v;
                st->has_bg = 1;
            } else {
                st->fg = (uint8_t)v;
            }
            i += 2;
            continue;
        }
        case 3: {                       /* \- cursor x op */
            uint8_t arg = (uint8_t)text[i + 1];
            if (!arg) { i++; continue; }
            int v = hex_digit(arg);
            st->cx = st->cx - 16 + (v < 0 ? 0 : v);
            i += 2;
            continue;
        }
        case 4: {                       /* \| cursor y op */
            uint8_t arg = (uint8_t)text[i + 1];
            if (!arg) { i++; continue; }
            int v = hex_digit(arg);
            st->cy = st->cy - 16 + (v < 0 ? 0 : v);
            i += 2;
            continue;
        }
        case 5:                         /* \+ undocumented */
            i += 2;
            return;
        case 6: {                       /* \^ text modes */
            uint8_t arg = (uint8_t)text[i + 1];
            if (!arg) { i++; continue; }
            int j = i + 1, off = 0;
            if (text[j] == '-') {
                off = 1;
                j++;
                if (!text[j]) { i = j; continue; }
            }
            switch (text[j]) {
            case 'i': st->invert = !off; break;
            case 'w': st->wide = !off; break;
            case 't': st->tall = !off; break;
            case 'p': st->pinball = !off; break;
            default: break;
            }
            i = j + 1;
            continue;
        }
        case 7:                         /* \a beep */
        case 11:
        case 14:
            i += 2;
            return;
        case 15: {                      /* own control glyph, eats one byte */
            if (!text[i + 1]) { i++; continue; }
            if (draw) {
                int dx = st->cx, dy = st->cy;
                api_apply_camera(&dx, &dy);
                pico8_draw_char_text(dx, dy, 15, st->fg, st->has_bg, st->bg,
                                     st->invert, st->wide, st->tall,
                                     st->pinball);
            }
            st->cx += (st->wide || st->pinball) ? 8 : 4;
            i += 2;
            continue;
        }
        default:
            print_emit(st, c, draw);
            i++;
            continue;
        }
    }
}

static int pico8_print(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);
    int narg = lua_gettop(L);

    print_state st;
    st.fg = pico8_ram[0x5f25] & 0x0f;
    st.has_bg = 0;
    st.bg = 0;
    st.invert = st.wide = st.tall = st.pinball = 0;

    int x = pico8_ram[0x5f26], y = pico8_ram[0x5f27];

    if (narg >= 4) {
        x = fix32_to_int(luaL_checknumber(L, 2));
        y = fix32_to_int(luaL_checknumber(L, 3));
        st.fg = fix32_to_uint8(luaL_checknumber(L, 4)) & 0x0f;
    } else if (narg >= 3) {
        x = fix32_to_int(luaL_checknumber(L, 2));
        y = fix32_to_int(luaL_checknumber(L, 3));
    } else if (narg == 2) {
        /* print(str, col): keep the cursor position, override the colour. */
        st.fg = fix32_to_uint8(luaL_checknumber(L, 2)) & 0x0f;
    }

    st.cx = st.startx = x;
    st.cy = y;

    /* While the shell is evaluating a console line, print() output goes to the
       console scrollback rather than the (hidden) screen. */
    if (pico8_console_capturing()) {
        pico8_console_emit(text, 6);
        print_state cst = st;
        cst.cx = cst.startx = 0;
        cst.cy = 0;
        print_walk(text, &cst, 0);
        lua_pushnumber(L, fix32_from_int(cst.cx));
        return 1;
    }

    print_walk(text, &st, 1);

    pico8_ram[0x5f25] = st.fg;
    pico8_ram[0x5f26] = (uint8_t)st.startx;
    pico8_ram[0x5f27] = (uint8_t)(st.cy + 6);
    lua_pushnumber(L, fix32_from_int(st.cx));
    return 1;
}

static int pico8_printh(lua_State *L)
{
    size_t msglen = 0;
    const char *msg_raw = luaL_checklstring(L, 1, &msglen);
    /* Everything leaving the VM as text is transcoded to UTF-8 here, the way
       the oracle does it: printh("한") prints "ムˇぬ" because those are the
       glyphs for bytes 237,149,156. */
    std::string msg_utf8 = pico8_charset_to_utf8(msg_raw, msglen);
    const char *msg = msg_utf8.c_str();
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        const char *filename = lua_tostring(L, 2);
        /* OVERWRITE defaults to false (append). SAVE_TO_DESKTOP redirects the
           file into the host's Desktop folder. */
        int overwrite    = (lua_gettop(L) >= 3) ? lua_toboolean(L, 3) : 0;
        int to_desktop   = (lua_gettop(L) >= 4) ? lua_toboolean(L, 4) : 0;

        if (strcmp(filename, "@clip") == 0) {
            /* Copy to the host clipboard (no trailing newline). */
#if defined(__APPLE__)
            FILE *p = popen("pbcopy 2>/dev/null", "w");
#elif defined(_WIN32)
            FILE *p = popen("clip", "w");
#else
            FILE *p = popen("xclip -selection clipboard 2>/dev/null", "w");
#endif
            if (p) { fputs(msg, p); pclose(p); }
            return 0;
        }

        char path[1024];
        if (to_desktop) {
            const char *home = getenv("HOME");
            snprintf(path, sizeof(path), "%s/Desktop/%s", home ? home : ".", filename);
        } else {
            snprintf(path, sizeof(path), "%s", filename);
        }

        FILE *f = fopen(path, overwrite ? "w" : "a");
        if (f) {
            fprintf(f, "%s\n", msg);
            fclose(f);
        }
    } else {
        fprintf(stdout, "%s\n", msg);
    }
    return 0;
}

static int pico8_assert(lua_State *L)
{
    if (!lua_toboolean(L, 1))
        return luaL_error(L, "%s", luaL_optstring(L, 2, "assertion failed!"));
    lua_pushboolean(L, 1);
    return 1;
}

static int pico8_sub(lua_State *L)
{
    size_t len;
    const char *s = luaL_checklstring(L, 1, &len);
    int start = fix32_to_int(luaL_checkinteger(L, 2));
    int end = (int)len;
    if (!lua_isnoneornil(L, 3)) end = fix32_to_int(luaL_checkinteger(L, 3));
    if (start < 1) start = 1;
    if (start > (int)len || end < 1) { lua_pushliteral(L, ""); return 1; }
    if (end > (int)len) end = (int)len;
    if (start > end) { lua_pushliteral(L, ""); return 1; }
    lua_pushlstring(L, s + start - 1, (size_t)(end - start + 1));
    return 1;
}

/*
** ord(s, [i, [count]]) -- shadows z8lua's version, which returns no values
** at all where PICO-8 returns nil. Measured on the oracle: a position past
** the end (or before 1) yields nil, a non-string non-number argument yields
** nil, a count below 1 yields nothing, and numbers are coerced to strings
** (ord(5) is 53, the code of "5").
*/
static int pico8_ord(lua_State *L)
{
    size_t len;
    int n = 0;
    int count = 1;

    int t = lua_type(L, 1);
    if (t != LUA_TSTRING && t != LUA_TNUMBER) { lua_pushnil(L); return 1; }
    const char *s = luaL_checklstring(L, 1, &len);

    if (!lua_isnone(L, 3)) {
        if (!lua_isnumber(L, 3)) return 0;
        count = fix32_to_int(luaL_checkinteger(L, 3));
    }
    if (!lua_isnone(L, 2)) {
        if (!lua_isnumber(L, 2)) { lua_pushnil(L); return 1; }
        n = fix32_to_int(luaL_checkinteger(L, 2)) - 1;
    }
    if (count < 1) return 0;
    if (n < 0 || size_t(n) >= len) { lua_pushnil(L); return 1; }
    if (size_t(n) + size_t(count) > len) count = (int)len - n;
    lua_checkstack(L, count);
    for (int i = 0; i < count; i++)
        lua_pushnumber(L, (lua_Number)(uint8_t)s[n + i]);
    return count;
}

static int pico8_pack(lua_State *L)
{
    int n = lua_gettop(L);
    lua_createtable(L, n, 1);
    for (int i = 0; i < n; i++) {
        lua_pushvalue(L, i + 1);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pushinteger(L, n);
    lua_setfield(L, -2, "n");
    return 1;
}


/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg text_api[] = {
    { "print",  pico8_print  }, { "printh", pico8_printh },
    { "sub",    pico8_sub    }, { "ord",   pico8_ord   },
    { "pack",   pico8_pack   }, { "assert", pico8_assert },
    { NULL, NULL }
};

void pico8_api_register_text(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = text_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
