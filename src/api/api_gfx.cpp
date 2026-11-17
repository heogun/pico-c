/*
 * api_gfx.cpp — the drawing part of the PICO-8 API.
 *
 * Everything that puts pixels on the screen lives here: the primitives
 * (pset, line, rect, circ, oval, rrect), the sprites (spr, sspr), the
 * palettes (pal, palt, color, fillp), the map (map, mget, mset) and the
 * text line tool (tline).
 *
 * 화면에 픽셀을 그리는 모든 것이 여기에 있다: 기본 도형(pset, line, rect,
 * circ, oval, rrect), 스프라이트(spr, sspr), 팔레트(pal, palt, color, fillp),
 * 맵(map, mget, mset), 그리고 텍스트 줄 도구(tline).
 *
 * The screen is a 128x128 4 bit plane in RAM (0x6000..0x7fff), two pixels
 * per byte: the even pixel is in the low nibble, the odd one in the high
 * nibble.  The same layout is used for the sprite sheet (0x0000..0x1fff) and
 * the map (0x2000..0x2fff), which is why the same helpers read all three.
 *
 * 화면은 RAM의 0x6000~0x7fff에 있는 128x128 4비트 평면이다. 한 바이트에 픽셀
 * 두 개가 들어가며 짝수 픽셀이 하위 니블, 홀수 픽셀이 상위 니블에 있다.
 * 스프라이트 시트(0x0000~0x1fff)와 맵(0x2000~0x2fff)도 같은 배치라서 같은
 * 헬퍼들이 세 개를 모두 읽는다.
 */

#include <stdlib.h>
#include <string.h>

#include "api_internal.h"

static int tline_px_mode = 0;

/*
 * api_apply_camera(x, y) — move a screen coordinate into camera space.
 *
 * The camera offset lives in the draw state (0x5f29..0x5f2c) as two signed
 * 16 bit values.  Every drawing call subtracts that offset from the
 * coordinates the cart passed in, so a cart scrolls the world by moving the
 * camera.
 *
 * 카메라 오프셋은 드로우 상태(0x5f29~0x5f2c)에 부호 있는 16비트 값 두 개로
 * 저장된다. 모든 드로잉 호출이 카트가 준 좌표에서 그 오프셋을 빼므로, 카트는
 * 카메라를 움직여 세상의 좌표를 스크롤시킬 수 있다.
 */
void api_apply_camera(int *x, int *y)
{
    uint16_t cx = pico8_ram[0x5f29] | ((uint16_t)pico8_ram[0x5f2a] << 8);
    uint16_t cy = pico8_ram[0x5f2b] | ((uint16_t)pico8_ram[0x5f2c] << 8);
    *x -= (int16_t)cx;
    *y -= (int16_t)cy;
}

/*
 * fillp_pattern() — the current 4x4 fill pattern.
 *
 * The pattern is a 16 bit mask with one bit per pixel of a 4x4 block.  It
 * is stored in 0x5f31 (low byte) and 0x5f32 (high byte).
 *
 * 패턴은 4x4 블록의 픽셀당 한 비트씩 가진 16비트 마스크다. 0x5f31(하위
 * 바이트)과 0x5f32(상위 바이트)에 저장된다.
 */
static inline uint16_t fillp_pattern(void)
{
    return (uint16_t)(((uint16_t)pico8_ram[0x5f32] << 8) | pico8_ram[0x5f31]);
}

/*
 * fillp_pick(x, y, on, off, out) — the on/off colour of one pixel.
 *
 * A set bit of the pattern means the "on" colour, a clear bit the "off"
 * colour.  When the transparency flag (0x5f33 bit 0) is set, the on bits
 * are transparent: the function returns 0 and nothing is drawn.
 *
 * 패턴의 켜진 비트는 "on" 색, 꺼진 비트는 "off" 색을 뜻한다. 투명도 플래그
 * (0x5f33의 0번 비트)가 켜져 있으면 on 비트는 투명하다. 함수는 0을 돌려주고
 * 아무것도 그리지 않는다.
 */
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

/*
 * fillp_color(x, y, colraw, out) — the colour to store for one pixel.
 *
 * The raw colour argument packs the on colour in the high nibble and the
 * off colour in the low one.  The fill pattern decides which of the two is
 * used, and the result is mapped through the draw palette afterwards.
 *
 * 원시 색 인자는 상위 니블에 on 색, 하위 니블에 off 색을 담는다. 필 패턴이 둘
 * 중 어느 쪽을 쓸지 정하고, 그 결과는 나중에 드로우 팔레트를 거쳐 매핑된다.
 */
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

/*
 * sprite_fillp(px, py, mc, out) — the colour of one sprite pixel.
 *
 * The colour of the sprite pixel has already been mapped through the draw
 * palette by the caller.  When the "apply the fill pattern to sprites" flag
 * (0x5f33 bit 1) is set, the mapped index picks an on/off pair out of the
 * secondary palette instead of being the final colour.
 *
 * 스프라이트 픽셀의 색은 호출자가 이미 드로우 팔레트를 거쳐 매핑했다.
 * "스프라이트에도 필 패턴 적용" 플래그(0x5f33의 1번 비트)가 켜져 있으면, 그
 * 매핑된 인덱스가 최종 색 대신 세컨더리 팔레트에서 on/off 짝을 고른다.
 */
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

/*
 * pset(x, y, color) — put one pixel on the screen.
 *
 * A NULL colour means the current draw colour (0x5f25).  The colour goes
 * through the fill pattern and the draw palette first, so what lands in the
 * screen RAM is a palette index and not a colour.
 *
 * color가 NULL이면 현재 드로우 색(0x5f25)을 쓴다. 색은 먼저 필 패턴과 드로우
 * 팔레트를 거치므로, 화면 RAM에 들어가는 값은 색이 아니라 팔레트 인덱스다.
 */
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

/*
 * pget(x, y) — the palette index of one screen pixel.
 *
 * Reads the screen RAM directly; the fill pattern is not applied.
 *
 * 화면 RAM을 직접 읽는다. 필 패턴은 적용하지 않는다.
 */
static uint8_t pget(int x, int y)
{
    if ((unsigned)x >= 128 || (unsigned)y >= 128) return 0;
    uint16_t addr = PICO8_SCREEN + (uint16_t)y * 64 + (uint16_t)(x >> 1);
    return (x & 1) ? (pico8_ram[addr] >> 4) & 0x0f : pico8_ram[addr] & 0x0f;
}

/*
 * hline(x0, x1, y, color) — draw a horizontal line.
 *
 * Without a fill pattern the whole run is written with memset(), two pixels
 * per byte, which is much faster than calling pset() for every pixel.  With
 * a pattern every pixel is drawn on its own.
 *
 * 필 패턴이 없으면 memset()으로 두 픽셀씩 한꺼번에 쓴다. 픽셀마다 pset()을
 * 부르는 것보다 훨씬 빠르다. 패턴이 있으면 픽셀마다 따로 그린다.
 */
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

/*
 * draw_line(x0, y0, x1, y1, color) — draw a line.
 *
 * Horizontal and vertical lines take a fast path; everything else uses
 * Bresenham's algorithm, so the line has neither gaps nor doubled pixels.
 *
 * 수평선과 수직선은 빠른 경로를 탄다. 그 외에는 Bresenham 알고리즘을 써서
 * 선에 끊김도, 두 번 그려지는 픽셀도 없게 한다.
 */
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

/*
 * draw_circle(cx, cy, r, color, fill) — draw a circle.
 *
 * The midpoint algorithm walks the circle in eighths, which keeps the
 * outline even.  A filled circle draws four horizontal runs per step.
 *
 * 중점 알고리즘이 원을 8분할로 걸어 가므로 외곽선이 매끄럽다. 채우는 쪽은 한
 * 걸음마다 수평선 네 개를 그린다.
 */
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

/*
 * draw_sprite(n, x, y, w, h, flip_x, flip_y) — draw a sprite.
 *
 * The sprite sheet holds 16x16 sprites of 8x8 pixels; w and h give the size
 * in sprites.  A sprite colour whose draw palette entry has bit 4 set is
 * transparent and is skipped.
 *
 * 스프라이트 시트에는 8x8 픽셀짜리 스프라이트가 16x16개 들어 있고, w와 h는
 * 스프라이트 단위의 크기다. 드로우 팔레트 항목의 4번 비트가 켜진 색은 투명해서
 * 건너뛴다.
 */
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

/*
 * cls(color) — clear the screen.
 *
 * Fills the whole screen with one colour and puts the text cursor back to
 * the top left corner.  Without an argument the screen is cleared with the
 * current draw colour.
 *
 * 화면 전체를 한 색으로 채우고 텍스트 커서를 왼쪽 위로 되돌린다. 인자가
 * 없으면 현재 드로우 색으로 지운다.
 */
static int pico8_cls(lua_State *L)
{
    int c = fix32_to_int(luaL_optinteger(L, 1, 0));
    uint8_t pair = (c & 0x0f) << 4 | (c & 0x0f);
    memset(&pico8_ram[PICO8_SCREEN], pair, 0x2000);
    pico8_ram[0x5f26] = 0;
    pico8_ram[0x5f27] = 0;
    return 0;
}

/*
 * spr(n, x, y, w, h, flip_x, flip_y) — draw a sprite.
 *
 * Draws the sprite number n at (x, y).  The size w and h are given in
 * sprites, so spr(n, x, y, 2, 2) draws a 16x16 pixel sprite.  The two flip
 * flags mirror the sprite horizontally or vertically.
 *
 * 스프라이트 번호 n을 (x, y)에 그린다. 크기 w와 h는 스프라이트 단위이므로
 * spr(n, x, y, 2, 2)는 16x16 픽셀 스프라이트를 그린다. 두 flip 플래그는
 * 스프라이트를 가로 또는 세로로 뒤집는다.
 */
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
    api_apply_camera(&x, &y);
    draw_sprite(n, x, y, w, h, fx, fy);
    return 0;
}

/*
 * sspr(sx, sy, sw, sh, dx, dy, dw, dh, flip_x, flip_y) — stretch a part of
 * the sprite sheet onto the screen.
 *
 * Copies the rectangle (sx, sy, sw, sh) of the sprite sheet to (dx, dy),
 * scaled to dw x dh pixels.  A negative dw or dh flips the copy and moves
 * the destination accordingly.
 *
 * 스프라이트 시트의 사각형 (sx, sy, sw, sh)를 (dx, dy)에 dw x dh 픽셀로
 * 늘려서 그린다. dw나 dh가 음수면 복사 방향을 뒤집고 목적지도 그에 맞춰
 * 옮긴다.
 */
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

    api_apply_camera(&dx, &dy);
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

/*
 * cursor(x, y, color) — move the text cursor.
 *
 * Sets the position print() writes at next and, when a third argument is
 * given, the colour it writes in.
 *
 * print()가 다음에 쓸 위치와, 세 번째 인자가 주어지면 그 색을 정한다.
 */
static int pico8_cursor(lua_State *L)
{
    pico8_ram[0x5f26] = fix32_to_uint8(luaL_checknumber(L, 1));
    pico8_ram[0x5f27] = fix32_to_uint8(luaL_checknumber(L, 2));
    if (lua_gettop(L) >= 3)
        pico8_ram[0x5f25] = fix32_to_uint8(luaL_optinteger(L, 3, 6));
    return 0;
}

/*
 * camera(x, y) — move the camera.
 *
 * Everything that is drawn afterwards is shifted by (x, y), which is how a
 * cart scrolls a world that is larger than the screen.  camera() without
 * arguments moves the camera back to the origin.
 *
 * 그 뒤에 그려지는 모든 것이 (x, y)만큼 밀린다. 카트가 화면보다 큰 세계를
 * 스크롤하는 방법이다. 인자가 없으면 카메라를 원점으로 되돌린다.
 */
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

/*
 * clip(x, y, w, h) — limit where drawing may go.
 *
 * Nothing is drawn outside the given rectangle until the clip is changed
 * again.  clip() without arguments sets it back to the whole screen.
 *
 * 주어진 사각형 밖으로는 아무것도 그려지지 않는다. clip()을 인자 없이
 * 부르면 다시 화면 전체로 되돌아간다.
 */
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

/*
 * pset(x, y, color) — put one pixel on the screen.
 *
 * Without a colour the current draw colour is used.  The camera offset is
 * applied, so the pixel lands where the cart thinks it does.
 *
 * 색을 주지 않으면 현재 드로우 색을 쓴다. 카메라 오프셋이 적용되므로,
 * 카트가 생각하는 위치에 픽셀이 놓인다.
 */
static int pico8_pset(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    api_apply_camera(&x, &y);
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        int c = fix32_to_int(luaL_checkinteger(L, 3));
        pset(x, y, &c);
    } else pset(x, y, NULL);
    return 0;
}

/*
 * pget(x, y) — the palette index of the pixel at (x, y).
 *
 * The camera offset is applied, so the pixel that is read is the one that
 * pset() would have written.
 *
 * 카메라 오프셋이 적용되므로, 읽히는 픽셀은 pset()이 썼을 픽셀이랑 같다.
 */
static int pico8_pget(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    api_apply_camera(&x, &y);
    lua_pushnumber(L, fix32_from_uint8(pget(x, y)));
    return 1;
}

/*
 * sget(x, y) — the colour of a pixel of the sprite sheet.
 *
 * Reads the sprite sheet (0x0000..0x0fff) and returns the colour index
 * stored there, 0..15.  The camera does not apply here.
 *
 * 스프라이트 시트(0x0000~0x0fff)를 읽어 그 자리에 저장된 색 인덱스(0~15)를
 * 돌려준다. 여기는 카메라가 적용되지 않는다.
 */
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

/*
 * sset(x, y, color) — write a pixel of the sprite sheet.
 *
 * Writes the colour index into the sprite sheet (0x0000..0x0fff).  The
 * camera does not apply here.
 *
 * 스프라이트 시트(0x0000~0x0fff)에 색 인덱스를 쓴다. 여기는 카메라가
 * 적용되지 않는다.
 */
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

/*
 * line(x0, y0, x1, y1, color) — draw a line.
 *
 * Draws a straight line between the two points.  The camera offset applies
 * to both ends.
 *
 * 두 점을 잇는 직선을 그린다. 카메라 오프셋이 양 끝점 모두에 적용된다.
 */
static int pico8_line(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    api_apply_camera(&x0, &y0); api_apply_camera(&x1, &y1);
    if (lua_gettop(L) >= 5) {
        int c = fix32_to_int(luaL_checkinteger(L, 5));
        draw_line(x0, y0, x1, y1, &c);
    } else draw_line(x0, y0, x1, y1, NULL);
    return 0;
}

/*
 * rect(x0, y0, x1, y1, color) — draw the outline of a rectangle.
 *
 * The corners are included.  The camera offset applies to both corners.
 *
 * 모서리도 포함한다. 카메라 오프셋이 양쪽 모서리 모두에 적용된다.
 */
static int pico8_rect(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    api_apply_camera(&x0, &y0); api_apply_camera(&x1, &y1);
    int *c = NULL;
    int cv;
    if (lua_gettop(L) >= 5) { cv = fix32_to_int(luaL_checkinteger(L, 5)); c = &cv; }
    for (int x = x0; x <= x1; x++) { pset(x, y0, c); pset(x, y1, c); }
    for (int y = y0; y <= y1; y++) { pset(x0, y, c); pset(x1, y, c); }
    return 0;
}

/*
 * rectfill(x0, y0, x1, y1, color) — fill a rectangle.
 *
 * The corners are included.  The camera offset applies to both corners.
 *
 * 모서리도 포함한다. 카메라 오프셋이 양쪽 모서리 모두에 적용된다.
 */
static int pico8_rectfill(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    api_apply_camera(&x0, &y0); api_apply_camera(&x1, &y1);
    int *c = NULL;
    int cv;
    if (lua_gettop(L) >= 5) { cv = fix32_to_int(luaL_checkinteger(L, 5)); c = &cv; }
    for (int y = y0; y <= y1; y++) hline(x0, x1, y, c);
    return 0;
}

/*
 * circ(x, y, radius, color) — draw the outline of a circle.
 *
 * The radius defaults to 4.  The camera offset applies to the centre.
 *
 * 반지름의 기본값은 4다. 카메라 오프셋이 중심에 적용된다.
 */
static int pico8_circ(lua_State *L)
{
    int cx = fix32_to_int(luaL_checknumber(L, 1));
    int cy = fix32_to_int(luaL_checknumber(L, 2));
    api_apply_camera(&cx, &cy);
    int r = fix32_to_int(luaL_optnumber(L, 3, fix32_from_int(4)));
    int *c = NULL, cv;
    if (lua_gettop(L) >= 4) { cv = fix32_to_int(luaL_checkinteger(L, 4)); c = &cv; }
    draw_circle(cx, cy, r, c, 0);
    return 0;
}

/*
 * circfill(x, y, radius, color) — fill a circle.
 *
 * The radius defaults to 4.  The camera offset applies to the centre.
 *
 * 반지름의 기본값은 4다. 카메라 오프셋이 중심에 적용된다.
 */
static int pico8_circfill(lua_State *L)
{
    int cx = fix32_to_int(luaL_checknumber(L, 1));
    int cy = fix32_to_int(luaL_checknumber(L, 2));
    api_apply_camera(&cx, &cy);
    int r = fix32_to_int(luaL_optnumber(L, 3, fix32_from_int(4)));
    int *c = NULL, cv;
    if (lua_gettop(L) >= 4) { cv = fix32_to_int(luaL_checkinteger(L, 4)); c = &cv; }
    draw_circle(cx, cy, r, c, 1);
    return 0;
}

/*
 * oval(x0, y0, x1, y1, color) — draw the outline of an ellipse.
 *
 * The ellipse is inscribed in the rectangle formed by the two points.  The
 * camera offset applies to both corners.
 *
 * 두 점이 만드는 사각형에 내접하는 타원을 그린다. 카메라 오프셋이 양쪽
 * 모서리 모두에 적용된다.
 */
static int pico8_oval(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    api_apply_camera(&x0, &y0); api_apply_camera(&x1, &y1);
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

/*
 * ovalfill(x0, y0, x1, y1, color) — fill an ellipse.
 *
 * The ellipse is inscribed in the rectangle formed by the two points.  The
 * camera offset applies to both corners.
 *
 * 두 점이 만드는 사각형에 내접하는 타원을 채운다. 카메라 오프셋이 양쪽
 * 모서리 모두에 적용된다.
 */
static int pico8_ovalfill(lua_State *L)
{
    int x0 = fix32_to_int(luaL_checknumber(L, 1));
    int y0 = fix32_to_int(luaL_checknumber(L, 2));
    int x1 = fix32_to_int(luaL_checknumber(L, 3));
    int y1 = fix32_to_int(luaL_checknumber(L, 4));
    api_apply_camera(&x0, &y0); api_apply_camera(&x1, &y1);
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

/*
 * pal_base(p) — the memory of one of the three palettes.
 *
 *   0  the draw palette, in the RAM at 0x5f00
 *   1  the draw palette of the screen below, in the RAM at 0x5f10
 *   2  the secondary palette, which PICO-8 does not map into the RAM and
 *      which is therefore kept in a private buffer
 *
 *   0  드로우 팔레트. RAM의 0x5f00
 *   1  아래쪽 화면의 드로우 팔레트. RAM의 0x5f10
 *   2  세컨더리 팔레트. PICO-8은 이것을 RAM에 대응시키지 않으므로 별도의
 *      버퍼에 보관한다
 */
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

/*
 * pal(...) — change the palette.
 *
 * pal()                 resets every palette to its default
 * pal(c0, c1)           changes one colour of the draw palette
 * pal(c0, c1, p)        changes one colour of palette p
 * pal(table, p)         changes many colours at once
 *
 * The second colour of a pair replaces the whole nibble of the palette
 * entry, so the high nibble of the draw colour is lost.  That is what
 * PICO-8 does as well.
 *
 * 한 쌍의 두 번째 색이 팔레트 항목의 니블 전체를 대체하므로 드로우 색의
 * 상위 니블은 사라진다. 실제 PICO-8도 그렇게 한다.
 */
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

/*
 * palt(...) — change which colours of the draw palette are transparent.
 *
 * palt()             makes every colour but the first one transparent
 * palt(mask)         sets the transparency of the 16 colours at once
 * palt(color, value) sets the transparency of a single colour
 *
 * The transparency bit is bit 4 of the palette entry, so a transparent
 * colour is the one that is not drawn at all.
 *
 * 투명도 비트는 팔레트 항목의 4번 비트다. 투명한 색은 아예 그려지지 않는다.
 */
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

/*
 * color(color) — set the colour that is drawn.
 *
 * The colour is stored in the draw state (0x5f25) and used by every drawing
 * call that has no colour of its own.  The default is 6.
 *
 * 색을 드로우 상태(0x5f25)에 저장한다. 스스로 색을 지정하지 않는 모든
 * 드로잉 호출이 이 색을 쓴다. 기본값은 6이다.
 */
static int pico8_color(lua_State *L)
{
    pico8_ram[0x5f25] = fix32_to_uint8(luaL_optinteger(L, 1, 6));
    return 0;
}

/*
 * fillp(pattern) — set the 4x4 fill pattern.
 *
 * The pattern is a 16 bit mask, one bit per pixel of a 4x4 block, given as
 * a number or as one of the eight built in patterns (the characters with
 * the code 128..135).  The fractional bits of the number carry three flags:
 * transparency, "apply to sprites" and the secondary palette.
 *
 * 패턴은 4x4 블록의 픽셀당 한 비트씩 가진 16비트 마스크다. 숫자로 주거나
 * 내장 패턴 여덟 개(코드 128~135인 문자) 중 하나로 준다. 숫자의 소수 부분이
 * 투명도, "스프라이트에도 적용", 세컨더리 팔레트 세 개의 플래그를 담는다.
 */
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

/*
 * fget(n, flag) — read the graphics flags of a sprite.
 *
 * With one argument the whole byte of flags is returned, with two a single
 * bit of it.  The flags live in the RAM at 0x3000, one byte per sprite.
 *
 * 인자가 하나면 플래그 바이트 전체를, 두 개면 그중 한 비트를 돌려준다.
 * 플래그는 RAM의 0x3000에 스프라이트마다 한 바이트로 저장된다.
 */
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

/*
 * fset(n, flag, value) — set the graphics flags of a sprite.
 *
 * With two arguments the whole byte is replaced, with three a single bit
 * is set or cleared.
 *
 * 인자가 두 개면 바이트 전체를 바꾸고, 세 개면 한 비트만 켜거나 끈다.
 */
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

/*
 * map_get(col, row) — the sprite number of a map cell.
 *
 * The map is 128x64 cells; the upper half (rows 32..63) lives in a second
 * block of the RAM, so both halves are looked up here.
 *
 * 맵은 128x64 칸이다. 위쪽 절반(32~63행)은 RAM의 다른 블록에 있으므로,
 * 여기서 두 절반을 모두 찾아준다.
 */
static uint8_t map_get(int col, int row)
{
    if ((unsigned)col >= PICO8_MAP_W || (unsigned)row >= PICO8_MAP_H) return 0;
    if (row < 32) return pico8_ram[PICO8_MAP_ADDR + row * 128 + col];
    return pico8_ram[PICO8_MAP2_ADDR + (row - 32) * 128 + col];
}

/*
 * map(celx, cely, sx, sy, celw, celh, layer) — draw the map.
 *
 * Draws the map cells starting at (celx, cely) onto the screen at (sx, sy).
 * celw and celh default to the size of the whole map.  When a layer is
 * given, only the sprites whose graphics flags contain that layer are
 * drawn.
 *
 * (celx, cely)부터 시작하는 맵 칸들을 화면의 (sx, sy)에 그린다. celw와 celh는
 * 맵 전체 크기가 기본값이다. 레이어를 주면 그래픽 플래그에 그 레이어가 들어
 * 있는 스프라이트만 그린다.
 */
static int pico8_map(lua_State *L)
{
    int celx = fix32_to_int(luaL_optnumber(L, 1, 0));
    int cely = fix32_to_int(luaL_optnumber(L, 2, 0));
    int sx = fix32_to_int(luaL_optnumber(L, 3, 0));
    int sy = fix32_to_int(luaL_optnumber(L, 4, 0));
    int celw = fix32_to_int(luaL_optnumber(L, 5, fix32_from_int(PICO8_MAP_W)));
    int celh = fix32_to_int(luaL_optnumber(L, 6, fix32_from_int(PICO8_MAP_H)));
    uint8_t layer = 0;
    api_apply_camera(&sx, &sy);
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

/*
 * mget(x, y) — the sprite number in a map cell.
 *
 * Reads the map cell and returns the sprite number stored there.
 *
 * 맵 칸을 읽어 그 자리에 저장된 스프라이트 번호를 돌려준다.
 */
static int pico8_mget(lua_State *L)
{
    int col = fix32_to_int(luaL_checknumber(L, 1));
    int row = fix32_to_int(luaL_checknumber(L, 2));
    lua_pushnumber(L, fix32_from_uint8(map_get(col, row)));
    return 1;
}

/*
 * mset(x, y, v) — write a sprite number into a map cell.
 *
 * Cells outside the map are ignored.
 *
 * 맵 밖의 칸은 무시된다.
 */
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

/*
 * sprite_pixel(spr, px, py) — the colour of one pixel of a sprite.
 *
 * Reads the 8x8 sprite out of the sprite sheet.
 *
 * 스프라이트 시트에서 8x8 스프라이트의 픽셀 하나를 읽는다.
 */
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

/*
 * tline(...) — walk a line across the map and draw the tiles it passes.
 *
 * tline(x0, y0, x1, y1, mx, my [, mdx, mdy [, layer]]) draws the map tiles
 * along a line, where (mx, my) is the map coordinate the line starts in
 * and (mdx, mdy) how far it moves per step.  tline(bits) switches between
 * map cells (8 pixels) and pixels.
 *
 * tline(x0, y0, x1, y1, mx, my [, mdx, mdy [, layer]])는 선을 따라 만나는
 * 맵 타일을 그린다. (mx, my)는 선이 시작하는 맵 좌표이고 (mdx, mdy)는 한 걸음
 * 마다 이동하는 거리다. tline(bits)는 맵 칸(8픽셀)과 픽셀 단위를 바꾼다.
 */
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

    api_apply_camera(&x0, &y0);
    api_apply_camera(&x1, &y1);

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

/*
 * getRRectCutAmount(radius, row) — how many pixels a rounded corner cuts off.
 *
 * The first five radii use a simple straight cut; the larger ones use the
 * values PICO-8 uses, so that the corners look the same as on the real
 * machine.  Everything above that is a circle.
 *
 * 앞의 다섯 반지름은 단순한 직선 절단으로 처리하고, 그보다 큰 것은 실제
 * PICO-8과 같은 값을 써서 모서리 모양이 원본과 같게 한다. 그보다 큰 것은
 * 원으로 계산한다.
 */
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

/*
 * rrect(x, y, w, h, radius, color) — draw a rectangle with rounded corners.
 *
 * The rectangle is w x h pixels with its top left corner at (x, y).  The
 * radius is capped at half of the smaller side, so a radius of 0 gives a
 * plain rectangle.
 *
 * 사각형은 (x, y)를 왼쪽 위로 하는 w x h 픽셀이다. 반지름은 짧은 변의 절반까지만
 * 가능하므로, 반지름이 0이면 그냥 사각형이 된다.
 */
static int pico8_rrect(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    int w = fix32_to_int(luaL_checknumber(L, 3));
    int h = fix32_to_int(luaL_checknumber(L, 4));
    int r = fix32_to_int(luaL_checknumber(L, 5));
    api_apply_camera(&x, &y);
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

/*
 * rrectfill(x, y, w, h, radius, color) — fill a rectangle with rounded
 * corners.
 *
 * The rectangle is w x h pixels with its top left corner at (x, y).  The
 * radius is capped at half of the smaller side.
 *
 * 사각형은 (x, y)를 왼쪽 위로 하는 w x h 픽셀이다. 반지름은 짧은 변의 절반까지만
 * 가능하다.
 */
static int pico8_rrectfill(lua_State *L)
{
    int x = fix32_to_int(luaL_checknumber(L, 1));
    int y = fix32_to_int(luaL_checknumber(L, 2));
    int w = fix32_to_int(luaL_checknumber(L, 3));
    int h = fix32_to_int(luaL_checknumber(L, 4));
    int r = fix32_to_int(luaL_checknumber(L, 5));
    api_apply_camera(&x, &y);
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

/*
 * flip() — flip the screen.
 *
 * PICO-8 flips the draw buffer, syncs it to the screen and processes the
 * input.  This emulator draws directly into the screen buffer and reads the
 * input once per frame, so there is nothing left to do here.
 *
 * PICO-8은 드로우 버퍼를 뒤집고 화면에 반영한 뒤 입력을 처리한다. 이
 * 에뮬레이터는 화면 버퍼에 직접 그리고 입력을 프레임마다 한 번 읽으므로,
 * 여기서 할 일이 없다.
 */
static int pico8_flip(lua_State *L)
{
    /* In PICO-8, flip() flips the draw buffer, syncs to frame, and processes input.
       In headless mode or normal mode with our frame-driven loop, this is a no-op. */
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg gfx_api[] = {
    { "cls",      pico8_cls      }, { "spr",      pico8_spr      },
    { "sspr",     pico8_sspr     }, { "cursor",   pico8_cursor   },
    { "camera",   pico8_camera   }, { "clip",     pico8_clip     },
    { "pset",     pico8_pset     }, { "pget",     pico8_pget     },
    { "sget",     pico8_sget     }, { "sset",     pico8_sset     },
    { "line",     pico8_line     }, { "rect",     pico8_rect     },
    { "rectfill", pico8_rectfill }, { "rrect",    pico8_rrect    },
    { "rrectfill", pico8_rrectfill },
    { "circ",     pico8_circ     }, { "circfill", pico8_circfill },
    { "oval",     pico8_oval     }, { "ovalfill", pico8_ovalfill },
    { "pal",      pico8_pal      }, { "palt",     pico8_palt     },
    { "color",    pico8_color    }, { "fillp",    pico8_fillp    },
    { "fget",     pico8_fget     }, { "fset",     pico8_fset     },
    { "map",      pico8_map      }, { "mget",     pico8_mget     },
    { "mset",     pico8_mset     }, { "tline",    pico8_tline    },
    { "flip",     pico8_flip     },
    { NULL, NULL }
};

void pico8_api_register_gfx(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = gfx_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
