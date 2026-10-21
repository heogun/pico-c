#ifndef PICO8_GFX_H
#define PICO8_GFX_H

/*
 * pico8_gfx.h — the drawing side of the emulator.
 *
 * This is the part of the host that owns the picture: it renders the RAM
 * of the screen into a pixel buffer, and it knows how to put a glyph on
 * that screen.  The Lua side of the drawing (spr, pset, rect, ...) lives in
 * src/api/api_gfx.cpp and calls the helpers below.
 *
 * 화면을 다루는 호스트 쪽 코드. RAM에 있는 화면 데이터를 픽셀 버퍼로 렌더링하고,
 * 화면에 글자를 그리는 방법을 안다. Lua 쪽 드로잉 API(spr, pset, rect 등)는
 * src/api/api_gfx.cpp에 있고, 아래 헬퍼들을 사용한다.
 */

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  Rendering
 * ------------------------------------------------------------------ */

/*
 * Draw the screen RAM (0x6000..0x7fff) into a 32 bit pixel buffer.
 *
 * pixels  the destination, 32 bits per pixel, 0x00RRGGBB
 * pitch   the distance in bytes between two rows of the destination
 * scale   how many host pixels one PICO-8 pixel covers (1, 2, 3, ...)
 *
 * pixels  목적지 버퍼. 픽셀당 32비트, 0x00RRGGBB 형식
 * pitch   목적지에서 한 줄 내려갈 때의 바이트 간격
 * scale   PICO-8 픽셀 하나가 차지할 호스트 픽셀 수 (1, 2, 3, ...)
 */
void pico8_render_screen(uint32_t *pixels, int pitch, int scale);

/* ------------------------------------------------------------------ *
 *  Drawing helpers
 * ------------------------------------------------------------------ */

/* Put a single pixel of the given colour at (x, y) on the screen. */
void pico8_put_pixel(int x, int y, uint8_t color);

/* Fill the whole screen with one colour. */
void pico8_clear_screen(uint8_t color);

/* Put one charset glyph on the screen with the given colour. */
void pico8_draw_char(int x, int y, uint8_t ch, uint8_t color);

/*
 * Put one charset glyph on the screen with the options print() supports.
 *
 * fg/bg      the foreground and background colours
 * has_bg     zero draws the glyph without a background
 * invert     swap the two colours
 * wide       twice as wide
 * tall       twice as high
 * pinball    PICO-8's pinball style font (wider, with a drop shadow)
 */
void pico8_draw_char_text(int x, int y, uint8_t ch,
                          uint8_t fg, int has_bg, uint8_t bg,
                          int invert, int wide, int tall, int pinball);

/* Put one charset glyph on the screen, scaled up by an integer factor. */
void pico8_draw_char_scaled(int x, int y, uint8_t ch, uint8_t color, int scale);

/* Restore the palette to its default colours (the call of pal() with no
   arguments, and what RESET() does). */
void pico8_pal_reset_all(void);

#endif /* PICO8_GFX_H */
