#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "pico8.h"
#include <string>
static int tline_px_mode = 0;

static int pico8_btn(lua_State *L);
static int pico8_btnp(lua_State *L);
static int pico8_cls(lua_State *L);
static int pico8_spr(lua_State *L);
static int pico8_sspr(lua_State *L);
static int pico8_print(lua_State *L);
static int pico8_cursor(lua_State *L);
static int pico8_camera(lua_State *L);
static int pico8_clip(lua_State *L);
static int pico8_pget(lua_State *L);
static int pico8_pset(lua_State *L);
static int pico8_sget(lua_State *L);
static int pico8_sset(lua_State *L);
static int pico8_line(lua_State *L);
static int pico8_rect(lua_State *L);
static int pico8_rectfill(lua_State *L);
static int pico8_rrect(lua_State *L);
static int pico8_rrectfill(lua_State *L);
static int pico8_reboot(lua_State *L);
static int pico8_resume(lua_State *L);
static int pico8_info(lua_State *L);
static int pico8_folder(lua_State *L);
static int pico8_ls(lua_State *L);
static int pico8_circ(lua_State *L);
static int pico8_circfill(lua_State *L);
static int pico8_oval(lua_State *L);
static int pico8_ovalfill(lua_State *L);
static int pico8_pal(lua_State *L);
static int pico8_palt(lua_State *L);
static int pico8_color(lua_State *L);
static int pico8_fillp(lua_State *L);
static int pico8_fget(lua_State *L);
static int pico8_fset(lua_State *L);
static int pico8_map(lua_State *L);
static int pico8_mget(lua_State *L);
static int pico8_tline(lua_State *L);
static int pico8_mset(lua_State *L);
static int pico8_peek(lua_State *L);
static int pico8_poke(lua_State *L);
static int pico8_peek2(lua_State *L);
static int pico8_poke2(lua_State *L);
static int pico8_peek4(lua_State *L);
static int pico8_poke4(lua_State *L);
static int pico8_memcpy(lua_State *L);
static int pico8_memset(lua_State *L);
static int pico8_time(lua_State *L);
static int pico8_stat(lua_State *L);
static int pico8_printh(lua_State *L);
static int pico8_srand(lua_State *L);
static int pico8_rnd(lua_State *L);
static int pico8_sfx(lua_State *L);
static int pico8_music(lua_State *L);
static int pico8_cartdata(lua_State *L);
static int pico8_dget(lua_State *L);
static int pico8_dset(lua_State *L);
static int pico8_assert(lua_State *L);
static int pico8_sub(lua_State *L);
static int pico8_reload(lua_State *L);
static int pico8_cstore(lua_State *L);
static int pico8_run(lua_State *L);
static int pico8_save(lua_State *L);
static int pico8_load(lua_State *L);
static int pico8_reset(lua_State *L);
static int pico8_stop(lua_State *L);
static int pico8_add(lua_State *L);
static int pico8_del(lua_State *L);
static int pico8_count(lua_State *L);
static int pico8_deli(lua_State *L);
static int pico8_serial(lua_State *L);
static void apply_camera(int *x, int *y)
{
    uint16_t cx = pico8_ram[0x5f29] | ((uint16_t)pico8_ram[0x5f2a] << 8);
    uint16_t cy = pico8_ram[0x5f2b] | ((uint16_t)pico8_ram[0x5f2c] << 8);
    *x -= (int16_t)cx;
    *y -= (int16_t)cy;
}

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

/* Current 4x4 fill pattern: 0x5f31 holds the low byte, 0x5f32 the high byte. */
static inline uint16_t fillp_pattern(void)
{
    return (uint16_t)(((uint16_t)pico8_ram[0x5f32] << 8) | pico8_ram[0x5f31]);
}

/* Pick the on/off colour for pixel (x,y) according to the fill pattern.
 * Returns 0 if the pixel is transparent (pattern on-bit with the transparency
 * flag set), else stores the chosen colour index in *out. */
static int fillp_pick(int x, int y, uint8_t on, uint8_t off, uint8_t *out)
{
    uint16_t pat = fillp_pattern();
    if (pat == 0) { *out = off; return 1; }
    int bit = (pat >> (15 - (((y & 3) << 2) + (x & 3)))) & 1;
    if (bit) {
        if (pico8_ram[0x5f33] & 1) return 0;      /* transparent on-bits */
        *out = on;
    } else {
        *out = off;
    }
    return 1;
}

/* Resolve the colour to store for pixel (x,y) given a raw colour argument.
 * The fill pattern observes the high/low nibbles as the on/off colour, then
 * the result is mapped through the draw palette. */
static int fillp_color(int x, int y, uint32_t colraw, uint8_t *out)
{
    uint8_t c;
    if (!fillp_pick(x, y, (uint8_t)((colraw >> 4) & 0x0f), (uint8_t)(colraw & 0x0f), &c))
        return 0;
    *out = pico8_ram[PICO8_DRAW_STATE + c] & 0x0f;
    return 1;
}

/* The secondary palette (pal(_,_,2)) is not memory-mapped in PICO-8, so it is
   kept in a private buffer rather than pico8_ram. */
static uint8_t g_secondary_pal[16] = {0};

/* Resolve a sprite pixel: the sprite colour is first mapped through the draw
   palette (done by the caller), then, when the fill pattern's "apply to
   sprites" flag is set, the mapped index selects an on/off pair from the
   secondary palette according to the 4x4 fill pattern. */
static int sprite_fillp(int px, int py, uint8_t mc, uint8_t *out)
{
    if (!(pico8_ram[0x5f33] & 2)) { *out = mc; return 1; }
    uint8_t sec = g_secondary_pal[mc & 0x0f];
    uint8_t c;
    if (!fillp_pick(px, py, (uint8_t)((sec >> 4) & 0x0f), (uint8_t)(sec & 0x0f), &c))
        return 0;
    *out = c;
    return 1;
}

static void pset(int x, int y, int *color)
{
    if ((unsigned)x >= 128 || (unsigned)y >= 128) return;
    uint32_t colraw = color ? (uint32_t)(*color & 0xff) : (uint32_t)pico8_ram[0x5f25];
    uint8_t pal;
    if (!fillp_color(x, y, colraw, &pal)) return;
    uint16_t addr = PICO8_SCREEN + (uint16_t)y * 64 + (uint16_t)(x >> 1);
    if (x & 1)
        pico8_ram[addr] = (pico8_ram[addr] & 0x0f) | (pal << 4);
    else
        pico8_ram[addr] = (pico8_ram[addr] & 0xf0) | pal;
}

static uint8_t pget(int x, int y)
{
    if ((unsigned)x >= 128 || (unsigned)y >= 128) return 0;
    uint16_t addr = PICO8_SCREEN + (uint16_t)y * 64 + (uint16_t)(x >> 1);
    return (x & 1) ? (pico8_ram[addr] >> 4) & 0x0f : pico8_ram[addr] & 0x0f;
}

static void hline(int x0, int x1, int y, int *color)
{
    if ((unsigned)y >= 128) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 > 127) x1 = 127;
    if (x0 > x1) return;

    uint32_t colraw = color ? (uint32_t)(*color & 0xff) : (uint32_t)pico8_ram[0x5f25];
    uint16_t pattern = fillp_pattern();

    if (pattern == 0) {
        uint8_t pal = pico8_ram[PICO8_DRAW_STATE + (colraw & 0x0f)] & 0x0f;
        uint8_t *row = &pico8_ram[PICO8_SCREEN + (uint16_t)y * 64];
        if (x0 & 1) {
            row[x0 >> 1] = (row[x0 >> 1] & 0x0f) | (pal << 4);
            x0++;
        }
        if (!(x1 & 1)) {
            row[x1 >> 1] = (row[x1 >> 1] & 0xf0) | pal;
            x1--;
        }
        if (x0 <= x1) {
            uint8_t pair = pal | (pal << 4);
            memset(&row[x0 >> 1], pair, (x1 - x0) / 2 + 1);
        }
    } else {
        for (int x = x0; x <= x1; x++) pset(x, y, color);
    }
}

static void draw_line(int x0, int y0, int x1, int y1, int *color)
{
    if (x0 == x1) {
        if ((unsigned)x0 >= 128) return;
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        if (y0 < 0) y0 = 0;
        if (y1 > 127) y1 = 127;
        if (fillp_pattern() != 0) {
            for (int y = y0; y <= y1; y++) pset(x0, y, color);
            return;
        }
        uint8_t c = color ? (*color & 0x0f) : (pico8_ram[0x5f25] & 0x0f);
        uint8_t pal = pico8_ram[PICO8_DRAW_STATE + c] & 0x0f;
        uint8_t mask = (x0 & 1) ? 0x0f : 0xf0;
        uint8_t shift = (x0 & 1) ? 4 : 0;
        uint8_t bits = pal << shift;
        for (int y = y0; y <= y1; y++) {
            uint8_t *p = &pico8_ram[PICO8_SCREEN + (uint16_t)y * 64 + (x0 >> 1)];
            *p = (*p & mask) | bits;
        }
        return;
    }
    if (y0 == y1) { hline(x0, x1, y0, color); return; }
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        pset(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static void draw_circle(int cx, int cy, int r, int *color, int fill)
{
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        if (fill) {
            hline(cx - x, cx + x, cy + y, color);
            hline(cx - x, cx + x, cy - y, color);
            hline(cx - y, cx + y, cy + x, color);
            hline(cx - y, cx + y, cy - x, color);
        } else {
            pset(cx + x, cy + y, color); pset(cx - x, cy + y, color);
            pset(cx + x, cy - y, color); pset(cx - x, cy - y, color);
            pset(cx + y, cy + x, color); pset(cx - y, cy + x, color);
            pset(cx + y, cy - x, color); pset(cx - y, cy - x, color);
        }
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

static void draw_sprite(uint8_t n, int x, int y, uint8_t w, uint8_t h, int flip_x, int flip_y)
{
    int bw = w * 8, bh = h * 8;
    int sx0 = (n & 0x0f) << 3, sy0 = (n >> 4) << 3;

    for (int dy = 0; dy < bh; dy++) {
        int sy = flip_y ? bh - 1 - dy : dy;
        int py = y + dy;
        if (py < 0 || py >= 128) continue;
        for (int dx = 0; dx < bw; dx++) {
            int sx = flip_x ? bw - 1 - dx : dx;
            int px = x + dx;
            if (px < 0 || px >= 128) continue;

            uint16_t saddr = PICO8_GFX_ADDR + (uint16_t)(sy0 + sy) * 64 + (uint16_t)((sx0 + sx) >> 1);
            uint8_t sbyte = pico8_ram[saddr];
            uint8_t col = ((sx0 + sx) & 1) ? (sbyte >> 4) : (sbyte & 0x0f);

            uint8_t pal = pico8_ram[PICO8_DRAW_STATE + col];
            if (pal & 0x10) continue;
            uint8_t mc;
            if (!sprite_fillp(px, py, pal & 0x0f, &mc)) continue;

            uint16_t daddr = PICO8_SCREEN + (uint16_t)py * 64 + (uint16_t)(px >> 1);
            if (px & 1)
                pico8_ram[daddr] = (pico8_ram[daddr] & 0x0f) | (mc << 4);
            else
                pico8_ram[daddr] = (pico8_ram[daddr] & 0xf0) | mc;
        }
    }
}

static int pico8_cls(lua_State *L)
{
    int c = fix32_to_int(luaL_optinteger(L, 1, 0));
    uint8_t pair = (c & 0x0f) << 4 | (c & 0x0f);
    memset(&pico8_ram[PICO8_SCREEN], pair, 0x2000);
    pico8_ram[0x5f26] = 0;
    pico8_ram[0x5f27] = 0;
    return 0;
}

static int pico8_spr(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || !lua_isnumber(L, 1)) return 0;
    uint8_t n = fix32_to_uint8(luaL_checknumber(L, 1));
    int x = fix32_to_int(luaL_optnumber(L, 2, 0));
    int y = fix32_to_int(luaL_optnumber(L, 3, 0));
    uint8_t w = fix32_to_uint8(luaL_optnumber(L, 4, 1));
    uint8_t h = fix32_to_uint8(luaL_optnumber(L, 5, 1));
    int fx = lua_toboolean(L, 6);
    int fy = lua_toboolean(L, 7);
    apply_camera(&x, &y);
    draw_sprite(n, x, y, w, h, fx, fy);
    return 0;
}

static int pico8_sspr(lua_State *L)
{
    int sx = fix32_to_int(luaL_checknumber(L, 1));
    int sy = fix32_to_int(luaL_checknumber(L, 2));
    int sw = fix32_to_int(luaL_checknumber(L, 3));
    int sh = fix32_to_int(luaL_checknumber(L, 4));
    int dx = fix32_to_int(luaL_checknumber(L, 5));
    int dy = fix32_to_int(luaL_checknumber(L, 6));
    int dw = fix32_to_int(luaL_optnumber(L, 7, fix32_from_int(sw)));
    int dh = fix32_to_int(luaL_optnumber(L, 8, fix32_from_int(sh)));
    int fx = lua_toboolean(L, 9), fy = lua_toboolean(L, 10);

    apply_camera(&dx, &dy);
    if (sw <= 0 || sh <= 0 || dw == 0 || dh == 0) return 0;
    if (dw < 0) { fx = !fx; dw = -dw; dx -= dw; }
    if (dh < 0) { fy = !fy; dh = -dh; dy -= dh; }

    for (int py = 0; py < dh; py++) {
        int src_y = fy ? (sh - 1 - py * sh / dh) : (py * sh / dh);
        src_y += sy;
        for (int px = 0; px < dw; px++) {
            int src_x = fx ? (sw - 1 - px * sw / dw) : (px * sw / dw);
            src_x += sx;
            uint8_t col = pico8_ram[PICO8_GFX_ADDR + (uint16_t)(src_y & 0x7f) * 64 + (uint16_t)((src_x & 0x7f) >> 1)];
            col = (src_x & 1) ? (col >> 4) : (col & 0x0f);
            uint8_t pal = pico8_ram[PICO8_DRAW_STATE + col];
            if (pal & 0x10) continue;
            uint8_t mc;
            if (!sprite_fillp(dx + px, dy + py, pal & 0x0f, &mc)) continue;

            int px2 = dx + px, py2 = dy + py;
            if (px2 < 0 || px2 >= 128 || py2 < 0 || py2 >= 128) continue;
            uint16_t da = PICO8_SCREEN + (uint16_t)py2 * 64 + (uint16_t)(px2 >> 1);
            if (px2 & 1) pico8_ram[da] = (pico8_ram[da] & 0x0f) | (mc << 4);
            else pico8_ram[da] = (pico8_ram[da] & 0xf0) | mc;
        }
    }
    return 0;
}

static int pico8_print(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);
    int narg = lua_gettop(L);

    /* While the shell is evaluating a console line, print() output goes to the
       console scrollback rather than the (hidden) screen. */
    if (pico8_console_capturing()) {
        pico8_console_emit(text, 6);
        lua_pushnumber(L, fix32_from_int((int)strlen(text) * 4));
        return 1;
    }

    int x = pico8_ram[0x5f26], y = pico8_ram[0x5f27];
    uint8_t col = pico8_ram[0x5f25] & 0x0f;

    if (narg >= 4) {
        x = fix32_to_int(luaL_checknumber(L, 2));
        y = fix32_to_int(luaL_checknumber(L, 3));
        col = fix32_to_uint8(luaL_checknumber(L, 4)) & 0x0f;
    } else if (narg >= 3) {
        x = fix32_to_int(luaL_checknumber(L, 2));
        y = fix32_to_int(luaL_checknumber(L, 3));
    } else if (narg >= 2) {
        x = fix32_to_int(luaL_checknumber(L, 2));
    }

    /* Logical (un-camera'd) cursor; camera is applied when drawing. */
    int cx = x, cy = y;
    const int startx = x;

    for (int i = 0; text[i]; i++) {
        switch (text[i]) {
            case '\t': cx = (cx / 16 + 1) * 16; continue;
            case '\n': cx = startx; cy += 6; continue;
            case '\b': cx -= 4; continue;
            case '\r': cx = startx; continue;
            default: break;
        }
        int dx = cx, dy = cy;
        apply_camera(&dx, &dy);
        pico8_draw_char(dx, dy, (uint8_t)text[i], col);
        cx += 4;
    }
    if (narg < 2) {
        pico8_ram[0x5f26] = 0;
        pico8_ram[0x5f27] = (uint8_t)(y + 6);
    }
    lua_pushnumber(L, fix32_from_int(cx));
    return 1;
}

static int pico8_cursor(lua_State *L)
{
    pico8_ram[0x5f26] = fix32_to_uint8(luaL_checknumber(L, 1));
    pico8_ram[0x5f27] = fix32_to_uint8(luaL_checknumber(L, 2));
    if (lua_gettop(L) >= 3)
        pico8_ram[0x5f25] = fix32_to_uint8(luaL_optinteger(L, 3, 6));
    return 0;
}

static int pico8_camera(lua_State *L)
{
    int nx = 0, ny = 0;
    if (lua_gettop(L) >= 1) nx = fix32_to_int(luaL_optnumber(L, 1, 0));
    if (lua_gettop(L) >= 2) ny = fix32_to_int(luaL_optnumber(L, 2, 0));
    int16_t vx = (int16_t)nx, vy = (int16_t)ny;
    pico8_ram[0x5f29] = (uint8_t)(vx & 0xff);
    pico8_ram[0x5f2a] = (uint8_t)((vx >> 8) & 0xff);
    pico8_ram[0x5f2b] = (uint8_t)(vy & 0xff);
    pico8_ram[0x5f2c] = (uint8_t)((vy >> 8) & 0xff);
    return 0;
}

static int pico8_clip(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        pico8_ram[0x5f20] = 0;
        pico8_ram[0x5f21] = 0;
        pico8_ram[0x5f22] = 128;
        pico8_ram[0x5f23] = 128;
        return 0;
    }
    pico8_ram[0x5f20] = fix32_to_uint8(luaL_checknumber(L, 1));
    pico8_ram[0x5f21] = fix32_to_uint8(luaL_checknumber(L, 2));
    pico8_ram[0x5f22] = fix32_to_uint8(luaL_checknumber(L, 3));
    pico8_ram[0x5f23] = fix32_to_uint8(luaL_checknumber(L, 4));
    return 0;
}

static int pico8_pset(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    apply_camera(&x, &y);
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        int c = fix32_to_int(luaL_checkinteger(L, 3));
        pset(x, y, &c);
    } else pset(x, y, NULL);
    return 0;
}

static int pico8_pget(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    apply_camera(&x, &y);
    lua_pushnumber(L, fix32_from_uint8(pget(x, y)));
    return 1;
}

static int pico8_sget(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1)) & 0x7f;
    int y = fix32_to_int(luaL_checknumber(L, 2)) & 0x7f;
    uint16_t addr = PICO8_GFX_ADDR + (uint16_t)(y << 6) + (uint16_t)(x >> 1);
    uint8_t col = pico8_ram[addr];
    col = (x & 1) ? (col >> 4) : (col & 0x0f);
    lua_pushnumber(L, fix32_from_uint8(col));
    return 1;
}

static int pico8_sset(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1)) & 0x7f;
    int y = fix32_to_int(luaL_checknumber(L, 2)) & 0x7f;
    uint8_t col = fix32_to_uint8(luaL_checknumber(L, 3)) & 0x0f;
    uint16_t addr = PICO8_GFX_ADDR + (uint16_t)(y << 6) + (uint16_t)(x >> 1);
    if (x & 1) pico8_ram[addr] = (pico8_ram[addr] & 0x0f) | (col << 4);
    else pico8_ram[addr] = (pico8_ram[addr] & 0xf0) | col;
    return 0;
}

static int pico8_line(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    apply_camera(&x0, &y0); apply_camera(&x1, &y1);
    if (lua_gettop(L) >= 5) {
        int c = fix32_to_int(luaL_checkinteger(L, 5));
        draw_line(x0, y0, x1, y1, &c);
    } else draw_line(x0, y0, x1, y1, NULL);
    return 0;
}

static int pico8_rect(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    apply_camera(&x0, &y0); apply_camera(&x1, &y1);
    int *c = NULL;
    int cv;
    if (lua_gettop(L) >= 5) { cv = fix32_to_int(luaL_checkinteger(L, 5)); c = &cv; }
    for (int x = x0; x <= x1; x++) { pset(x, y0, c); pset(x, y1, c); }
    for (int y = y0; y <= y1; y++) { pset(x0, y, c); pset(x1, y, c); }
    return 0;
}

static int pico8_rectfill(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    apply_camera(&x0, &y0); apply_camera(&x1, &y1);
    int *c = NULL;
    int cv;
    if (lua_gettop(L) >= 5) { cv = fix32_to_int(luaL_checkinteger(L, 5)); c = &cv; }
    for (int y = y0; y <= y1; y++) hline(x0, x1, y, c);
    return 0;
}

static int pico8_circ(lua_State *L)
{
    int cx = fix32_to_int(luaL_checknumber(L, 1));
    int cy = fix32_to_int(luaL_checknumber(L, 2));
    apply_camera(&cx, &cy);
    int r = fix32_to_int(luaL_optnumber(L, 3, fix32_from_int(4)));
    int *c = NULL, cv;
    if (lua_gettop(L) >= 4) { cv = fix32_to_int(luaL_checkinteger(L, 4)); c = &cv; }
    draw_circle(cx, cy, r, c, 0);
    return 0;
}

static int pico8_circfill(lua_State *L)
{
    int cx = fix32_to_int(luaL_checknumber(L, 1));
    int cy = fix32_to_int(luaL_checknumber(L, 2));
    apply_camera(&cx, &cy);
    int r = fix32_to_int(luaL_optnumber(L, 3, fix32_from_int(4)));
    int *c = NULL, cv;
    if (lua_gettop(L) >= 4) { cv = fix32_to_int(luaL_checkinteger(L, 4)); c = &cv; }
    draw_circle(cx, cy, r, c, 1);
    return 0;
}

static int pico8_oval(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    apply_camera(&x0, &y0); apply_camera(&x1, &y1);
    int a = abs(x1 - x0) / 2, b = abs(y1 - y0) / 2;
    int xc = (x0 + x1) / 2, yc = (y0 + y1) / 2;
    int *c = NULL, cv;
    if (lua_gettop(L) >= 5) { cv = fix32_to_int(luaL_checkinteger(L, 5)); c = &cv; }
    int x = 0, y = b, a2 = a * a, b2 = b * b;
    int sigma = 2 * b2 + a2 * (1 - 2 * b);
    while (b2 * x <= a2 * y) {
        pset(xc + x, yc + y, c); pset(xc - x, yc + y, c);
        pset(xc + x, yc - y, c); pset(xc - x, yc - y, c);
        if (sigma >= 0) { sigma += 4 * a2 * (1 - y); y--; }
        sigma += b2 * (4 * x + 6); x++;
    }
    x = a; y = 0; sigma = 2 * a2 + b2 * (1 - 2 * a);
    while (a2 * y <= b2 * x) {
        pset(xc + x, yc + y, c); pset(xc - x, yc + y, c);
        pset(xc + x, yc - y, c); pset(xc - x, yc - y, c);
        if (sigma >= 0) { sigma += 4 * b2 * (1 - x); x--; }
        sigma += a2 * (4 * y + 6); y++;
    }
    return 0;
}

static int pico8_ovalfill(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    apply_camera(&x0, &y0); apply_camera(&x1, &y1);
    int a = abs(x1 - x0) / 2, b = abs(y1 - y0) / 2;
    int xc = (x0 + x1) / 2, yc = (y0 + y1) / 2;
    int *c = NULL, cv;
    if (lua_gettop(L) >= 5) { cv = fix32_to_int(luaL_checkinteger(L, 5)); c = &cv; }
    int x = 0, y = b, a2 = a * a, b2 = b * b;
    int sigma = 2 * b2 + a2 * (1 - 2 * b);
    while (b2 * x <= a2 * y) {
        hline(xc - x, xc + x, yc + y, c);
        hline(xc - x, xc + x, yc - y, c);
        if (sigma >= 0) { sigma += 4 * a2 * (1 - y); y--; }
        sigma += b2 * (4 * x + 6); x++;
    }
    x = a; y = 0; sigma = 2 * a2 + b2 * (1 - 2 * a);
    while (a2 * y <= b2 * x) {
        hline(xc - x, xc + x, yc + y, c);
        hline(xc - x, xc + x, yc - y, c);
        if (sigma >= 0) { sigma += 4 * b2 * (1 - x); x--; }
        sigma += a2 * (4 * y + 6); y++;
    }
    return 0;
}

static uint8_t *pal_base(int p)
{
    if (p == 1) return &pico8_ram[0x5f10];
    if (p == 2) return g_secondary_pal;
    return &pico8_ram[0x5f00];
}

/* Default secondary palette: each entry packs a highlight (high nibble) and the
   base colour (low nibble), matching PICO-8's built-in dither pairs. */
static const uint8_t PICO8_SECONDARY_DEFAULT[16] = {
    0x00, 0x01, 0x12, 0x13, 0x24, 0x15, 0xD6, 0x67,
    0x48, 0x49, 0x9A, 0x3B, 0xDC, 0x5D, 0x8E, 0xEF,
};

static void pal_reset_one(int p)
{
    uint8_t *b = pal_base(p);
    for (int i = 0; i < 16; i++) {
        if (p == 0)      b[i] = (uint8_t)(i | (i == 0 ? 0x10 : 0));
        else if (p == 2) b[i] = PICO8_SECONDARY_DEFAULT[i];
        else             b[i] = (uint8_t)i;
    }
}

void pico8_pal_reset_all(void)
{
    pal_reset_one(0);
    pal_reset_one(1);
    pal_reset_one(2);
}

static int pico8_pal(lua_State *L)
{
    int narg = lua_gettop(L);
    if (narg == 0) {
        pico8_pal_reset_all();
    } else if (narg == 1 && lua_isnumber(L, 1)) {
        pal_reset_one(fix32_to_int(luaL_checknumber(L, 1)) & 3);
    } else if (lua_istable(L, 1)) {
        int p = fix32_to_int(luaL_optnumber(L, 2, 0));
        uint8_t *b = pal_base(p);
        lua_pushnil(L);
        while (lua_next(L, 1)) {
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                int c0 = fix32_to_int(lua_tonumber(L, -2)) % 16;
                int c1 = fix32_to_int(lua_tonumber(L, -1));
                if (p == 2) b[c0] = (uint8_t)(c1 & 0xff);
                else        b[c0] = (uint8_t)((b[c0] & 0xf0) | (c1 & 0x0f));
            }
            lua_pop(L, 1);
        }
    } else if (narg >= 2) {
        int c0 = fix32_to_int(luaL_checknumber(L, 1)) & 0x0f;
        int c1 = lua_isnoneornil(L, 2) ? c0 : fix32_to_int(luaL_checknumber(L, 2));
        int p = fix32_to_int(luaL_optnumber(L, 3, 0));
        uint8_t *b = pal_base(p);
        if (p == 2) b[c0] = (uint8_t)(c1 & 0xff);
        else        b[c0] = (uint8_t)((b[c0] & 0xf0) | (c1 & 0x0f));
    }
    return 0;
}

static int pico8_palt(lua_State *L)
{
    int narg = lua_gettop(L);
    if (narg == 0) {
        for (int i = 0; i < 16; i++)
            pico8_ram[0x5f00 + i] = (pico8_ram[0x5f00 + i] & 0xef) | (i == 0 ? 0x10 : 0);
    } else if (narg == 1) {
        uint16_t mask = (uint16_t)fix32_to_uint32(luaL_checknumber(L, 1));
        for (int i = 0; i < 16; i++) {
            int t = (mask >> (15 - i)) & 1;
            pico8_ram[0x5f00 + i] = (pico8_ram[0x5f00 + i] & 0xef) | (t ? 0x10 : 0);
        }
    } else {
        int col = fix32_to_int(luaL_checkinteger(L, 1)) & 0x0f;
        int t = lua_toboolean(L, 2);
        pico8_ram[0x5f00 + col] = (pico8_ram[0x5f00 + col] & 0xef) | (t ? 0x10 : 0);
    }
    return 0;
}

static int pico8_color(lua_State *L)
{
    pico8_ram[0x5f25] = fix32_to_uint8(luaL_optinteger(L, 1, 6));
    return 0;
}

static int pico8_fillp(lua_State *L)
{
    uint32_t raw = 0;
    int have = 0;
    if (lua_type(L, 1) == LUA_TNUMBER) {
        raw = (uint32_t)luaL_checknumber(L, 1).bits();
        have = 1;
    } else if (lua_type(L, 1) == LUA_TSTRING) {
        uint8_t c = (uint8_t)lua_tostring(L, 1)[0];
        static const uint32_t patterns[] = {
            0b0000000000000000, 0b0101101001011010,
            0b0101000100011111, 0b0000000000000011,
            0b0111110101111101, 0b1011100000011101,
            0b1111100110011111, 0b0101000110111111,
        };
        if (c >= 128 && c <= 135) raw = patterns[c - 128] << 16;
        have = 1;
    }
    (void)have;

    /* The 16-bit pattern lives in the top half of the number; the fractional
       bits (0b0.111) select the transparency / sprite / secondary flags. */
    uint16_t pattern = (uint16_t)((raw >> 16) & 0xffff);
    uint8_t  flags   = (uint8_t)(((raw >> 15) & 1) | ((raw >> 13) & 2) | ((raw >> 11) & 4));

    pico8_ram[0x5f31] = (uint8_t)(pattern & 0xff);
    pico8_ram[0x5f32] = (uint8_t)((pattern >> 8) & 0xff);
    pico8_ram[0x5f33] = flags;
    return 0;
}

static int pico8_fget(lua_State *L)
{
    uint8_t n = fix32_to_uint8(luaL_checknumber(L, 1));
    uint8_t flags = pico8_ram[PICO8_GFX_FLAGS + n];
    if (lua_gettop(L) >= 2) {
        int f = fix32_to_int(luaL_checkinteger(L, 2));
        lua_pushboolean(L, (unsigned)f < 8 ? (flags >> f) & 1 : 0);
    } else {
        lua_pushnumber(L, fix32_from_uint8(flags));
    }
    return 1;
}

static int pico8_fset(lua_State *L)
{
    uint8_t n = fix32_to_uint8(luaL_checknumber(L, 1));
    if (lua_gettop(L) >= 3) {
        int f = fix32_to_int(luaL_checkinteger(L, 2));
        int v = lua_toboolean(L, 3);
        if ((unsigned)f < 8) {
            if (v) pico8_ram[PICO8_GFX_FLAGS + n] |= (1 << f);
            else pico8_ram[PICO8_GFX_FLAGS + n] &= ~(1 << f);
        }
    } else {
        pico8_ram[PICO8_GFX_FLAGS + n] = fix32_to_uint8(luaL_checknumber(L, 2));
    }
    return 0;
}

static uint8_t map_get(int col, int row)
{
    if ((unsigned)col >= PICO8_MAP_W || (unsigned)row >= PICO8_MAP_H) return 0;
    if (row < 32) return pico8_ram[PICO8_MAP_ADDR + row * 128 + col];
    return pico8_ram[PICO8_MAP2_ADDR + (row - 32) * 128 + col];
}

static int pico8_map(lua_State *L)
{
    int celx = fix32_to_int(luaL_optnumber(L, 1, 0));
    int cely = fix32_to_int(luaL_optnumber(L, 2, 0));
    int sx = fix32_to_int(luaL_optnumber(L, 3, 0));
    int sy = fix32_to_int(luaL_optnumber(L, 4, 0));
    int celw = fix32_to_int(luaL_optnumber(L, 5, fix32_from_int(PICO8_MAP_W)));
    int celh = fix32_to_int(luaL_optnumber(L, 6, fix32_from_int(PICO8_MAP_H)));
    uint8_t layer = 0;
    apply_camera(&sx, &sy);
    if (lua_gettop(L) >= 7) layer = (uint8_t)fix32_to_uint32(luaL_checknumber(L, 7));

    for (int ty = 0; ty < celh; ty++)
        for (int tx = 0; tx < celw; tx++) {
            uint8_t spr = map_get(celx + tx, cely + ty);
            if (spr == 0 && !(pico8_ram[0x5F36] & 0x08)) continue;
            if (layer && (pico8_ram[PICO8_GFX_FLAGS + spr] & layer) == 0) continue;
            draw_sprite(spr, sx + tx * 8, sy + ty * 8, 1, 1, 0, 0);
        }
    return 0;
}

static int pico8_mget(lua_State *L)
{
    int col = fix32_to_int(luaL_checknumber(L, 1));
    int row = fix32_to_int(luaL_checknumber(L, 2));
    lua_pushnumber(L, fix32_from_uint8(map_get(col, row)));
    return 1;
}

static int pico8_mset(lua_State *L)
{
    int col = fix32_to_int(luaL_checknumber(L, 1));
    int row = fix32_to_int(luaL_checknumber(L, 2));
    uint8_t v = fix32_to_uint8(luaL_checknumber(L, 3));
    if ((unsigned)col < PICO8_MAP_W && (unsigned)row < PICO8_MAP_H) {
        if (row < 32) pico8_ram[PICO8_MAP_ADDR + row * 128 + col] = v;
        else pico8_ram[PICO8_MAP2_ADDR + (row - 32) * 128 + col] = v;
    }
    return 0;
}

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

static int pico8_time(lua_State *L)
{
    lua_pushnumber(L, fix32_from_int(pico8_frame_count));
    return 1;
}

static int pico8_seconds(lua_State *L)
{
    lua_pushnumber(L, fix32(double(pico8_frame_count) / 60.0));
    return 1;
}

static int pico8_foreach(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    unsigned len = (unsigned)lua_rawlen(L, 1);
    for (unsigned i = 1; i <= len; i++) {
        lua_pushvalue(L, 2);
        lua_rawgeti(L, 1, i);
        lua_call(L, 1, 0);
    }
    return 0;
}

static int pico8_unpack(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int i = fix32_to_int(luaL_optnumber(L, 2, fix32_from_int(1)));
    unsigned len = (unsigned)lua_rawlen(L, 1);
    int j = fix32_to_int(luaL_optnumber(L, 3, fix32_from_int((int)len)));
    if (i < 1) i = 1;
    if ((unsigned)j > len) j = (int)len;
    int n = 0;
    for (int k = i; k <= j; k++) {
        lua_rawgeti(L, 1, k);
        n++;
    }
    return n;
}

static int pico8_extcmd(lua_State *L)
{
    const char *cmd = luaL_checkstring(L, 1);
    if (strcmp(cmd, "reset") == 0) {
        /* Restart the cart from the top, keeping the current param string. */
        pico8_console_queue_load(NULL, NULL, NULL);
        pico8_vm_halted = 1;
    } else if (strcmp(cmd, "shutdown") == 0) {
        pico8_console_request_quit = 1;
    } else if (strcmp(cmd, "pause") == 0) {
        /* No modal pause menu; fall back to the shell console. */
        pico8_console_request_pause = 1;
    } else if (strcmp(cmd, "folder") == 0) {
        pico8_console_open_folder();
    } else if (strcmp(cmd, "go_back") == 0) {
        const char *crumb = pico8_console_breadcrumb();
        if (crumb && crumb[0]) {
            pico8_console_queue_load(crumb, NULL, NULL);
            pico8_vm_halted = 1;
        }
    }
    /* "label", "screen", "video", "rec", "audio_rec", "audio_end" and
       "set_filename" are host/editor features that have no equivalent here. */
    return 0;
}

#include <dirent.h>
#include <sys/stat.h>

static int pico8_reboot(lua_State *L)
{
    (void)L;
    pico8_vm_halted = 1;
    return 0;
}

static int pico8_resume(lua_State *L)
{
    (void)L;
    return 0;
}

static int pico8_info(lua_State *L)
{
    (void)L;
    size_t len = 0;
    const char *src = pico8_cart_source(&len);
    int chars = (int)len;
    int lines = 1;
    for (size_t i = 0; i < len; i++)
        if (src[i] == '\n') lines++;

    char buf[192];
    snprintf(buf, sizeof buf, "%d chars, %d lines (pico-c 0.2.7-compat)", chars, lines);
    pico8_console_print(buf);
    return 0;
}

static int pico8_folder(lua_State *L)
{
    (void)L;
    pico8_console_open_folder();
    return 0;
}

static int pico8_ls(lua_State *L)
{
    const char *dir = ".";
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
        dir = lua_tostring(L, 1);

    lua_newtable(L);
    DIR *d = opendir(dir);
    if (!d) return 1;

    int idx = 1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        std::string name = ent->d_name;
        bool is_dir = false;
        struct stat st;
        std::string full = std::string(dir) + "/" + name;
        if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) is_dir = true;
        bool is_p8 = (name.rfind(".p8", name.size()-3) == name.size()-3) ||
                     (name.rfind(".p8.png", name.size()-7) == name.size()-7);
        if (!is_dir && !is_p8) continue;
        lua_pushstring(L, (name + (is_dir ? "/" : "")).c_str());
        lua_rawseti(L, -2, idx++);
    }
    closedir(d);
    return 1;
}

static uint8_t sprite_pixel(uint8_t spr, int px, int py)
{
    if (px < 0 || px > 7 || py < 0 || py > 7) return 0;
    int sx0 = (spr & 0x0f) << 3;
    int sy0 = (spr >> 4) << 3;
    uint16_t addr = PICO8_GFX_ADDR + (uint16_t)(sy0 + py) * 64 + (uint16_t)((sx0 + px) >> 1);
    uint8_t byte = pico8_ram[addr];
    if ((sx0 + px) & 1) return (byte >> 4) & 0x0f;
    return byte & 0x0f;
}

static int pico8_tline(lua_State *L)
{
    if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
        int bits = fix32_to_int(luaL_checknumber(L, 1));
        /* A value of 16 means coordinates are given in pixels. */
        tline_px_mode = (bits >= 8) ? 1 : 0;
        return 0;
    }

    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    float mx = (float)luaL_checknumber(L, 5);
    float my = (float)luaL_checknumber(L, 6);
    float mdx = tline_px_mode ? 1.0f : 0.125f;
    float mdy = 0.0f;
    int layer = 0;
    int has_layer = 0;
    if (lua_gettop(L) >= 7) mdx = (float)luaL_checknumber(L, 7);
    if (lua_gettop(L) >= 8) mdy = (float)luaL_checknumber(L, 8);
    if (tline_px_mode) {
        mx /= 8.0f; my /= 8.0f; mdx /= 8.0f; mdy /= 8.0f;
    }
    if (lua_gettop(L) >= 9) { layer = fix32_to_int(luaL_checknumber(L, 9)); has_layer = 1; }

    apply_camera(&x0, &y0);
    apply_camera(&x1, &y1);

    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    if (steps <= 0) steps = 1;

    int offsetx = (signed char)pico8_ram[0x5F3A];
    int offsety = (signed char)pico8_ram[0x5F3B];
    int mxmask_raw = pico8_ram[0x5F38];
    int mymask_raw = pico8_ram[0x5F39];
    int mxmask = (mxmask_raw <= 0) ? 255 : (mxmask_raw - 1);
    int mymask = (mymask_raw <= 0) ? 255 : (mymask_raw - 1);

    float cur_mx = mx;
    float cur_my = my;

    for (int i = 0; i <= steps; i++) {
        int px = x0 + (dx * i) / steps;
        int py = y0 + (dy * i) / steps;

        int cell_x = (int)floorf(cur_mx);
        int cell_y = (int)floorf(cur_my);
        /* Fractional part of the coordinate (independent of masking) selects the
           pixel within the tile. */
        int fx = (int)floorf((cur_mx - floorf(cur_mx)) * 8.0f);
        int fy = (int)floorf((cur_my - floorf(cur_my)) * 8.0f);
        /* Mask the map coordinate first, then apply the sample offset. */
        cell_x = (cell_x >= 0) ? (cell_x & mxmask) : cell_x;
        cell_y = (cell_y >= 0) ? (cell_y & mymask) : cell_y;
        cell_x += offsetx;
        cell_y += offsety;

        if (cell_x >= 0 && cell_x < PICO8_MAP_W && cell_y >= 0 && cell_y < PICO8_MAP_H) {
            uint8_t tile = map_get(cell_x, cell_y);
            int draw_it = (tile != 0) || (pico8_ram[0x5F36] & 0x08);
            if (draw_it) {
                if (has_layer && layer != 0 &&
                    (pico8_ram[PICO8_GFX_FLAGS + tile] & (uint8_t)layer) == 0) {
                    /* skip */
                } else {
                    int local_x = fx;
                    int local_y = fy;
                    if (local_x < 0) local_x = 0;
                    if (local_x > 7) local_x = 7;
                    if (local_y < 0) local_y = 0;
                    if (local_y > 7) local_y = 7;
                    uint8_t col = sprite_pixel(tile, local_x, local_y);
                    uint8_t pal = pico8_ram[PICO8_DRAW_STATE + col];
                    uint8_t mc;
                    if (!(pal & 0x10) && sprite_fillp(px, py, pal & 0x0f, &mc)) {
                        uint16_t da = PICO8_SCREEN + (uint16_t)py * 64 + (uint16_t)(px >> 1);
                        if (px & 1)
                            pico8_ram[da] = (pico8_ram[da] & 0x0f) | (mc << 4);
                        else
                            pico8_ram[da] = (pico8_ram[da] & 0xf0) | mc;
                    }
                }
            }
        }

        cur_mx += mdx;
        cur_my += mdy;
    }

    return 0;
}

static int getRRectCutAmount(int radius, int row)
{
    if (radius == 6) { static int cuts[] = {5,3,2,1,1,0}; return (row < 6) ? cuts[row] : 0; }
    else if (radius == 7) { static int cuts[] = {6,4,3,2,1,1,0}; return (row < 7) ? cuts[row] : 0; }
    else if (radius == 8) { static int cuts[] = {7,5,3,2,2,1,1,0}; return (row < 8) ? cuts[row] : 0; }
    else if (radius == 9) { static int cuts[] = {7,5,4,3,2,1,1,0}; return (row < 8) ? cuts[row] : 0; }
    else if (radius == 10) { static int cuts[] = {7,5,4,3,2,1,1,0}; return (row < 8) ? cuts[row] : 0; }
    else if (radius == 11 || radius == 12) { static int cuts[] = {9,7,5,4,3,2,2,1,1,0,0}; return (row < 11) ? cuts[row] : 0; }
    else if (radius == 13 || radius == 14) { static int cuts[] = {11,8,7,5,4,3,3,2,1,1,1,0,0}; return (row < 13) ? cuts[row] : 0; }
    else if (radius == 15 || radius == 16) { static int cuts[] = {13,10,8,7,5,4,4,3,2,2,1,1,1,0,0}; return (row < 15) ? cuts[row] : 0; }
    else {
        if (row >= radius) return 0;
        double y = (double)(radius - row - 1);
        double disc = (double)(radius * radius) - (y * y);
        if (disc <= 0) return 0;
        double x = (double)radius - sqrt(disc);
        int cut = (int)(x + 0.5) - 1;
        if (cut == 0 && row < radius - 1) {
            double tail_y = (double)(radius - row);
            if (tail_y <= 2.5) cut = 1;
        }
        return (cut < 0) ? 0 : cut;
    }
}

static int pico8_rrect(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    int w = fix32_to_int(luaL_checknumber(L, 3));
    int h = fix32_to_int(luaL_checknumber(L, 4));
    int r = fix32_to_int(luaL_checknumber(L, 5));
    apply_camera(&x, &y);
    int *c = NULL; int cv;
    if (lua_gettop(L) >= 6) { cv = fix32_to_int(luaL_checkinteger(L, 6)); c = &cv; }
    int maxR = (w < h ? w : h) / 2;
    if (r > maxR) r = maxR;
    if (r <= 0) {
        /* fallback rect outline */
        int x0 = x, y0 = y, x1 = x + w - 1, y1 = y + h - 1;
        for (int xx = x0; xx <= x1; xx++) { pset(xx, y0, c); pset(xx, y1, c); }
        for (int yy = y0; yy <= y1; yy++) { pset(x0, yy, c); pset(x1, yy, c); }
        return 0;
    }
    for (int row = 0; row < h; row++) {
        int cut = 0;
        if (row < r) {
            if (r <= 5) cut = r - 1 - row;
            else cut = getRRectCutAmount(r, row);
        } else if (row >= h - r) {
            int bottomRow = h - 1 - row;
            if (r <= 5) cut = r - 1 - bottomRow;
            else cut = getRRectCutAmount(r, bottomRow);
        }
        if (cut < 0) cut = 0;
        if (cut * 2 >= w) cut = (w - 1) / 2;
        int ls = x + cut;
        int le = x + w - 1 - cut;
        if (ls <= le) {
            if (row == 0 || row == h - 1) {
                hline(ls, le, y + row, c);
            } else {
                pset(ls, y + row, c);
                if (ls != le) pset(le, y + row, c);
            }
        }
    }
    return 0;
}

static int pico8_rrectfill(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    int w = fix32_to_int(luaL_checknumber(L, 3));
    int h = fix32_to_int(luaL_checknumber(L, 4));
    int r = fix32_to_int(luaL_checknumber(L, 5));
    apply_camera(&x, &y);
    int *c = NULL; int cv;
    if (lua_gettop(L) >= 6) { cv = fix32_to_int(luaL_checkinteger(L, 6)); c = &cv; }
    int maxR = (w < h ? w : h) / 2;
    if (r > maxR) r = maxR;
    if (r <= 0) {
        int x0=x, y0=y, x1=x+w-1, y1=y+h-1;
        for (int yy=y0; yy<=y1; yy++) hline(x0, x1, yy, c);
        return 0;
    }
    for (int row = 0; row < h; row++) {
        int cut = 0;
        if (row < r) {
            if (r <= 5) cut = r - 1 - row;
            else cut = getRRectCutAmount(r, row);
        } else if (row >= h - r) {
            int bottomRow = h - 1 - row;
            if (r <= 5) cut = r - 1 - bottomRow;
            else cut = getRRectCutAmount(r, bottomRow);
        }
        if (cut < 0) cut = 0;
        if (cut * 2 >= w) cut = (w - 1) / 2;
        int ls = x + cut;
        int le = x + w - 1 - cut;
        if (ls <= le) hline(ls, le, y + row, c);
    }
    return 0;
}

static int pico8_flip(lua_State *L)
{
    /* In PICO-8, flip() flips the draw buffer, syncs to frame, and processes input.
       In headless mode or normal mode with our frame-driven loop, this is a no-op. */
    return 0;
}

/* Pause-menu items registered by menuitem(). Index 0 is unused so the arrays
   can be indexed directly by PICO-8's 1..5 item numbers. */
static int         g_menu_ref[6]   = {LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF};
static int         g_menu_mask[6]  = {0,0,0,0,0,0};
static std::string g_menu_label[6];

static int pico8_menuitem(lua_State *L)
{
    int top = lua_gettop(L);
    /* A nil index refers to the currently executing item (used from within a
       callback). We do not track that, so ignore it. */
    if (top < 1 || lua_isnil(L, 1) || !lua_isnumber(L, 1)) return 0;

    int raw = fix32_to_int(luaL_checknumber(L, 1));
    int index = raw & 0xff;              /* 1..5 */
    int mask  = (raw >> 8) & 0xff;       /* button filter in bits 0xff00 */

    if (index < 1 || index > 5) return 0;

    const char *label = (top >= 2 && !lua_isnil(L, 2)) ? luaL_checkstring(L, 2) : NULL;
    int has_cb = (top >= 3 && lua_isfunction(L, 3));

    if (!label && !has_cb) {             /* remove the item */
        if (g_menu_ref[index] != LUA_NOREF && g_menu_ref[index] != LUA_REFNIL)
            luaL_unref(L, LUA_REGISTRYINDEX, g_menu_ref[index]);
        g_menu_ref[index] = LUA_NOREF;
        g_menu_label[index].clear();
        g_menu_mask[index] = 0;
        return 0;
    }

    if (label) g_menu_label[index] = label;
    g_menu_mask[index] = mask;

    if (has_cb) {
        if (g_menu_ref[index] != LUA_NOREF && g_menu_ref[index] != LUA_REFNIL)
            luaL_unref(L, LUA_REGISTRYINDEX, g_menu_ref[index]);
        lua_pushvalue(L, 3);
        g_menu_ref[index] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

void pico8_menuitem_clear(void)
{
    for (int i = 0; i < 6; i++) {
        g_menu_ref[i]  = LUA_NOREF;
        g_menu_mask[i] = 0;
        g_menu_label[i].clear();
    }
}

int pico8_menuitem_has(int index)
{
    if (index < 1 || index > 5) return 0;
    return !g_menu_label[index].empty() || g_menu_ref[index] != LUA_NOREF;
}

int pico8_menuitem_count(void)
{
    int n = 0;
    for (int i = 1; i <= 5; i++) if (pico8_menuitem_has(i)) n++;
    return n;
}

const char *pico8_menuitem_label(int index)
{
    if (index < 1 || index > 5 || g_menu_label[index].empty()) return NULL;
    return g_menu_label[index].c_str();
}

int pico8_menuitem_btnmask(int index)
{
    if (index < 1 || index > 5) return 0;
    return g_menu_mask[index];
}

int pico8_menuitem_invoke(lua_State *L, int index, int buttons)
{
    if (!L || index < 1 || index > 5) return 0;
    if (g_menu_ref[index] == LUA_NOREF || g_menu_ref[index] == LUA_REFNIL) return 0;

    lua_rawgeti(L, LUA_REGISTRYINDEX, g_menu_ref[index]);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }

    lua_getglobal(L, "__traceback");
    lua_insert(L, -2);
    lua_pushnumber(L, fix32_from_int(buttons));
    if (lua_pcall(L, 1, 1, -2) != LUA_OK) {
        fprintf(stderr, "Error in menuitem: %s\n", lua_tostring(L, -1));
        lua_pop(L, 2);
        return 0;
    }
    int keep = lua_toboolean(L, -1);
    lua_pop(L, 2);
    return keep;
}

static void pico8_stat_clipboard(lua_State *L)
{
    char buf[1024];
    buf[0] = '\0';
#if defined(__APPLE__)
    const char *cmd = "pbpaste 2>/dev/null";
#elif defined(_WIN32)
    const char *cmd = "powershell -NoProfile -Command Get-Clipboard 2>NUL";
#else
    const char *cmd = "xclip -selection clipboard -o 2>/dev/null";
#endif
    FILE *f = popen(cmd, "r");
    if (f) {
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        /* pbpaste / Get-Clipboard append a trailing newline. */
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
    }
    lua_pushstring(L, buf);
}

/* stat(80..85) = UTC time, stat(90..95) = local time; both are
   year, month, day, hour, minute, second. */
static void pico8_stat_time(lua_State *L, int idx)
{
    time_t now = time(NULL);
    struct tm tmv;
    int ok;
    if (idx >= 90) ok = (localtime_r(&now, &tmv) != NULL);
    else           ok = (gmtime_r(&now, &tmv) != NULL);
    if (!ok) {
        lua_pushnumber(L, fix32_from_int(0));
        return;
    }
    int v = 0;
    switch (idx % 10) {
        case 0: v = tmv.tm_year + 1900; break;
        case 1: v = tmv.tm_mon + 1;     break;   /* 1..12 */
        case 2: v = tmv.tm_mday;        break;
        case 3: v = tmv.tm_hour;        break;
        case 4: v = tmv.tm_min;         break;
        case 5: v = tmv.tm_sec;         break;
    }
    lua_pushnumber(L, fix32_from_int(v));
}

static int pico8_stat(lua_State *L)
{
    int idx = fix32_to_int(luaL_optnumber(L, 1, 0));
    int devkit = pico8_ram[0x5f2d];

    switch (idx) {
        case 0: {
            int kb = (int)(lua_gc(L, LUA_GCCOUNT, 0));
            lua_pushnumber(L, fix32_from_int(kb * 1024));
            return 1;
        }
        case 1:   /* cpu load (1.0 == 100%) */
            lua_pushnumber(L, fix32_from_int(0));
            return 1;
        case 4:   /* clipboard contents */
            pico8_stat_clipboard(L);
            return 1;
        case 6:   /* parameter string */
            if (pico8_cmdline && pico8_cmdline[0]) {
                lua_pushstring(L, pico8_cmdline);
            } else {
                lua_pushstring(L, "");
            }
            return 1;
        case 7:   /* current framerate */
            lua_pushnumber(L, fix32_from_int(pico8_vm_has_function("_update60") ? 60 : 30));
            return 1;

        /* --- devkit mouse + keyboard (POKE(0x5f2d, flags)) --- */
        case 30:  /* keypress available */
            lua_pushboolean(L, (devkit & 1) && pico8_keyboard_available());
            return 1;
        case 31: { /* character returned by keyboard */
            char s[2] = { 0, 0 };
            if (devkit & 1) {
                int ch = pico8_keyboard_read();
                if (ch >= 0) s[0] = (char)ch;
            }
            lua_pushstring(L, s);
            return 1;
        }
        case 32:  /* mouse x */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_x : 0));
            return 1;
        case 33:  /* mouse y */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_y : 0));
            return 1;
        case 34:  /* mouse buttons (bitfield) */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_buttons : 0));
            return 1;
        case 36:  /* mouse wheel event */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_wheel : 0));
            return 1;
        case 38:  /* relative x movement (requires pointer lock, flag 0x4) */
            lua_pushnumber(L, fix32_from_int(((devkit & 5) == 5) ? pico8_mouse_dx : 0));
            return 1;
        case 39:  /* relative y movement */
            lua_pushnumber(L, fix32_from_int(((devkit & 5) == 5) ? pico8_mouse_dy : 0));
            return 1;

        /* --- audio --- */
        case 46: case 47: case 48: case 49:
            lua_pushnumber(L, fix32_from_int(pico8_audio_sfx_id(idx - 46)));
            return 1;
        case 50: case 51: case 52: case 53:
            lua_pushnumber(L, fix32_from_int(pico8_audio_note_num(idx - 50)));
            return 1;
        case 54: /* currently playing music pattern index */
            lua_pushnumber(L, fix32_from_int(pico8_audio_music_pattern()));
            return 1;
        case 55: /* total patterns played */
            lua_pushnumber(L, fix32_from_int(pico8_audio_music_count()));
            return 1;
        case 56: /* ticks played on the current pattern */
            lua_pushnumber(L, fix32_from_int(pico8_audio_music_ticks()));
            return 1;
        case 57:
            lua_pushboolean(L, pico8_audio_music_playing());
            return 1;

        /* --- clock --- */
        case 80: case 81: case 82: case 83: case 84: case 85:
        case 90: case 91: case 92: case 93: case 94: case 95:
            pico8_stat_time(L, idx);
            return 1;

        case 100: /* breadcrumb label */
            if (pico8_console_breadcrumb()) {
                lua_pushstring(L, pico8_console_breadcrumb());
            } else {
                lua_pushnil(L);
            }
            return 1;

        default:
            lua_pushnumber(L, 0);
            return 1;
    }
}

static int pico8_printh(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
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

static int32_t seed_hi = 0x60009755;
static int32_t seed_lo = 0xdeadbeef;

static int32_t pico8_random(int32_t limit)
{
    seed_hi = (seed_hi << 16) | ((uint32_t)seed_hi >> 16);
    seed_hi += seed_lo;
    seed_lo += seed_hi;
    if (limit < 0) {
        if (limit <= seed_hi && seed_hi < 0) return seed_hi - limit;
        return seed_hi;
    }
    int32_t result;
    if (seed_hi < 0)
        result = (seed_hi % limit + limit) % limit;
    else
        result = seed_hi % limit;
    return result;
}

static int pico8_srand(lua_State *L)
{
    int32_t s = ((z8::fix32)luaL_checknumber(L, 1)).bits();
    if (s == 0) { seed_hi = 0x60009755; seed_lo = 0xdeadbeef; }
    else { s &= 0x7fffffff; seed_hi = s ^ 0xbead29ba; seed_lo = s; }
    for (int i = 0; i < 32; i++) pico8_random(0);
    return 0;
}

static int pico8_rnd(lua_State *L)
{
    if (lua_istable(L, 1)) {
        unsigned len = (unsigned)lua_rawlen(L, 1);
        if (len == 0) { lua_pushnil(L); return 1; }
        int32_t v = pico8_random(len << 16);
        int idx = (v >> 16) + 1;
        if (idx > (int)len) idx = len;
        if (idx < 1) idx = 1;
        lua_pushnumber(L, fix32_from_int(idx));
        lua_gettable(L, 1);
    } else {
        z8::fix32 arg = (lua_gettop(L) > 0 && !lua_isnil(L, 1)) ? (z8::fix32)luaL_checknumber(L, 1) : z8::fix32(1);
        int32_t limit = arg.bits();
        if (limit == 0) limit = 1;
        int32_t r = pico8_random(limit);
        lua_pushnumber(L, z8::fix32::frombits(r));
    }
    return 1;
}

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
        rc = -1;   /* writing .p8.png carts is not supported yet */
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

static int pico8_count(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_isnil(L, 1)) { lua_pushinteger(L, 0); return 1; }
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    if (lua_gettop(L) < 2) { lua_pushinteger(L, n); return 1; }
    int target = fix32_to_int(luaL_checknumber(L, 2));
    int cnt = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        int v = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
        lua_pop(L, 1);
        if (v == target) cnt++;
    }
    lua_pushinteger(L, cnt);
    return 1;
}

static int pico8_deli(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    int idx = (int)luaL_optinteger(L, 2, n);
    if (idx < 1 || idx > n) return 0;
    lua_rawgeti(L, 1, idx);
    for (int i = idx; i < n; i++) {
        lua_rawgeti(L, 1, i + 1);
        lua_rawseti(L, 1, i);
    }
    lua_pushnil(L);
    lua_rawseti(L, 1, n);
    return 1;
}

static int pico8_serial(lua_State *L)
{
    (void)L;
    return 0;
}

static int pico8_add(lua_State *L)
{
    if (lua_isnil(L, 1)) return 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 2);
    lua_rawseti(L, 1, (int)lua_rawlen(L, 1) + 1);
    return 0;
}

static int pico8_del(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    int written = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        int equal = lua_rawequal(L, -1, 2);
        lua_pop(L, 1);
        if (!equal) {
            written++;
            lua_rawgeti(L, 1, i);
            lua_rawseti(L, 1, written);
        }
    }
    for (int i = written + 1; i <= n; i++)
        lua_pushnil(L), lua_rawseti(L, 1, i);
    return 0;
}

static int pico8_all(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
        const char *code = "return function() return nil end";
        luaL_loadstring(L, code);
        lua_call(L, 0, 1);
        return 1;
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getglobal(L, "_pico8_all_func");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        const char *code = "return function(t) local i=0; local n=#t; "
                           "return function() i=i+1; if i<=n then return t[i] end end end";
        luaL_loadstring(L, code);
        lua_call(L, 0, 1);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_pico8_all_func");
    }
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    return 1;
}

/* Forward a call such as mkdir("name") / cd("dir") / dir() to the shell. String
   arguments are joined with spaces to form the command line. */
static int pico8_shell_cmd(lua_State *L)
{
    const char *name = lua_tostring(L, lua_upvalueindex(1));
    std::string line = name ? name : "";
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        if (lua_isnil(L, i)) continue;
        size_t len = 0;
        const char *s = luaL_tolstring(L, i, &len);
        line += " ";
        line += (s ? s : "");
        lua_pop(L, 1);
    }
    pico8_console_exec(line.c_str());
    return 0;
}

static const luaL_Reg pico8_api[] = {
    {"btn", pico8_btn}, {"btnp", pico8_btnp},
    {"cls", pico8_cls}, {"spr", pico8_spr}, {"sspr", pico8_sspr},
    {"print", pico8_print}, {"cursor", pico8_cursor}, {"camera", pico8_camera}, {"clip", pico8_clip},
    {"pset", pico8_pset}, {"pget", pico8_pget},
    {"sget", pico8_sget}, {"sset", pico8_sset},
    {"line", pico8_line}, {"rect", pico8_rect}, {"rectfill", pico8_rectfill},
    {"rrect", pico8_rrect}, {"rrectfill", pico8_rrectfill},    {"circ", pico8_circ}, {"circfill", pico8_circfill},
    {"oval", pico8_oval}, {"ovalfill", pico8_ovalfill},
    {"pal", pico8_pal}, {"palt", pico8_palt},
    {"color", pico8_color}, {"fillp", pico8_fillp},
    {"fget", pico8_fget}, {"fset", pico8_fset},
    {"map", pico8_map}, {"mget", pico8_mget}, {"mset", pico8_mset},
    {"tline", pico8_tline},
    {"peek", pico8_peek}, {"poke", pico8_poke},
    {"peek2", pico8_peek2}, {"poke2", pico8_poke2},
    {"peek4", pico8_peek4}, {"poke4", pico8_poke4},
    {"memcpy", pico8_memcpy}, {"memset", pico8_memset},
    {"srand", pico8_srand}, {"rnd", pico8_rnd},
    {"time", pico8_time}, {"stat", pico8_stat}, {"printh", pico8_printh},
    {"sfx", pico8_sfx}, {"music", pico8_music},
    {"cartdata", pico8_cartdata}, {"dget", pico8_dget}, {"dset", pico8_dset},
    {"assert", pico8_assert},
    {"sub", pico8_sub},
    {"stop", pico8_stop},
    {"reload", pico8_reload}, {"cstore", pico8_cstore},
    {"run", pico8_run}, {"reset", pico8_reset},    {"add", pico8_add},
    {"save", pico8_save}, {"load", pico8_load},
    {"del", pico8_del}, {"deli", pico8_deli}, {"count", pico8_count},
    {"all", pico8_all}, {"serial", pico8_serial},
    {"pack", pico8_pack},
    {"t", pico8_time},
    {"seconds", pico8_seconds},
    {"foreach", pico8_foreach},
    {"unpack", pico8_unpack},
    {"extcmd", pico8_extcmd},
    {"flip", pico8_flip},
    {"reboot", pico8_reboot}, {"resume", pico8_resume},
    {"info", pico8_info}, {"folder", pico8_folder},
    {"ls", pico8_ls},    {"menuitem", pico8_menuitem},
    {NULL, NULL}
};

void pico8_init_api(void)
{
}

void pico8_register_api(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = pico8_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }

    /* Shell commands that PICO-8 also exposes as global Lua functions, where
       each name is bound as an upvalue of a shared dispatcher. */
    static const char *kShellFuncs[] = {
        "dir", "cd", "mkdir", "help", "backup", "exit", "export", "import",
        "install_demos", "install_games", "keyconfig", "login", "shutdown",
        "splore",
        NULL
    };
    for (int i = 0; kShellFuncs[i]; i++) {
        lua_pushstring(L, kShellFuncs[i]);
        lua_pushcclosure(L, pico8_shell_cmd, 1);
        lua_setfield(L, -2, kShellFuncs[i]);
    }

    lua_pop(L, 1);
}
