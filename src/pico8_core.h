#ifndef PICO8_CORE_H
#define PICO8_CORE_H

/*
 * pico8_core.h — the facts about the machine that every part needs.
 *
 * This header holds the constants that describe the PICO-8: the size of the
 * RAM, the layout of that RAM and the limits the cartridge loader enforces.
 * It also holds the fixed point helpers that convert between Lua numbers and
 * PICO-8's 16.16 number format.
 *
 * 이 헤더는 PICO-8을 설명하는 상수들을 담는다: RAM 크기, RAM의 배치, 카트리지
 * 로더가 지키는 한계. 그리고 Lua 숫자와 PICO-8의 16.16 고정소수 형식을 오가는
 * 고정소수 헬퍼도 여기에 있다.
 *
 * The RAM map is the most important part.  PICO-8 gives the cart 64KB and
 * every region of it has a fixed meaning:
 *
 * RAM 맵이 여기서 가장 중요하다. PICO-8은 카트에 64KB를 주고, 그 안의 각 영역은
 * 고정된 의미를 가진다:
 *
 *   0x0000 - 0x0fff   sprite sheet, 128x128 pixels, 4 bits each
 *   0x1000 - 0x1fff   the lower half of the map (rows 32..63)
 *   0x2000 - 0x2fff   the upper half of the map (rows 0..31)
 *   0x3000 - 0x30ff   graphics flags, one byte per sprite (the fill pattern
 *                     and the "which layer is visible" bits)
 *   0x3100 - 0x31ff   the song data
 *   0x3200 - 0x3243   the sound effect data
 *   0x4300 - 0x55ff   the cartridge's own data area
 *   0x5600 - 0x5dff   a custom font
 *   0x5e00 - 0x5eff   persistent data
 *   0x5f00 - 0x5f3f   the draw state (cursor, camera, palette, ...)
 *   0x5f40 - 0x5f7f   the hardware state (buttons, mouse, ...)
 *   0x5f80 - 0x5fff   the GPIO pins
 *   0x6000 - 0x7fff   the screen, 128x128 pixels, 4 bits each
 *   0x8000 - 0xffff   a second map the cart may draw
 *
 * A 4 bit pixel means two pixels share a byte: the even one is in the low
 * nibble, the odd one in the high nibble.  The screen, the sprite sheet and
 * the map all use that layout.
 *
 * 4비트 픽셀이라 한 바이트에 픽셀 두 개가 들어간다: 짝수 픽셀이 하위 니블,
 * 홀수 픽셀이 상위 니블. 화면, 스프라이트 시트, 맵이 모두 그 배치를 쓴다.
 */

#include <stdint.h>
#include <stdbool.h>

#include <string>

#include "fix32.h"
#include "lua.h"

using z8::fix32;

/* ------------------------------------------------------------------ *
 *  Machine dimensions
 * ------------------------------------------------------------------ */

#define PICO8_RAM_SIZE     0x10000   /* the cart sees 64KB / 카트가 보는 64KB */
#define PICO8_SCREEN_W     128       /* the screen is 128 wide / 화면 너비 */
#define PICO8_SCREEN_H     128       /* the screen is 128 high / 화면 높이 */

#define PICO8_PAL_COUNT    16        /* the colours of the palette / 팔레트 색 수 */
#define PICO8_SPRITE_COUNT 256       /* the sprites of the sheet / 스프라이트 수 */
#define PICO8_MAP_W        128       /* the map is 128 wide / 맵 너비 */
#define PICO8_MAP_H        64        /* the map is 64 high / 맵 높이 */
#define PICO8_FPS          60        /* the frames per second / 초당 프레임 수 */

/* ------------------------------------------------------------------ *
 *  RAM map
 * ------------------------------------------------------------------ */

#define PICO8_GFX_ADDR     0x0000   /* sprite sheet / 스프라이트 시트 */
#define PICO8_MAP2_ADDR    0x1000   /* map, rows 32..63 / 맵 32~63행 */
#define PICO8_MAP_ADDR     0x2000   /* map, rows 0..31 / 맵 0~31행 */
#define PICO8_GFX_FLAGS    0x3000   /* per sprite graphics flags / 그래픽 플래그 */
#define PICO8_SONG_ADDR    0x3100   /* the songs / song 영역 */
#define PICO8_SFX_ADDR     0x3200   /* the sound effects / 효과음 영역 */
#define PICO8_SFX_SIZE     68       /* the size of one effect / 효과음 하나 크기 */
#define PICO8_USER_DATA    0x4300   /* the cartridge's data area / 카트 데이터 영역 */
#define PICO8_CUSTOM_FONT  0x5600   /* a custom font / 사용자 폰트 */
#define PICO8_PERSISTENT   0x5e00   /* the persistent data / 유지되는 데이터 */
#define PICO8_DRAW_STATE   0x5f00   /* cursor, camera, palette / 커서, 카메라, 팔레트 */
#define PICO8_HW_STATE     0x5f40   /* buttons, mouse, keyboard / 버튼, 마우스, 키보드 */
#define PICO8_GPIO_PINS    0x5f80   /* the GPIO pins / GPIO 핀 */
#define PICO8_SCREEN       0x6000   /* the screen / 화면 */
#define PICO8_EXT_MAP      0x8000   /* the second map / 두 번째 맵 */

#define PICO8_CART_DATA_SIZE  0x4300   /* size of the data area / 데이터 영역 크기 */

/* ------------------------------------------------------------------ *
 *  Cartridge limits
 * ------------------------------------------------------------------ *
 * Cart limits the oracle enforces when a cart is loaded.  A Lua section
 * longer than PICO8_SOURCE_LIMIT bytes is rejected outright ("could not
 * load"), and a cart with more than PICO8_TOKEN_LIMIT tokens is loaded but
 * never run ("program too large").
 *
 * 오라클이 카트를 불러올 때 지키는 한계. Lua 소스가 PICO8_SOURCE_LIMIT 바이트를
 * 넘으면 아예 거부("could not load")되고, 토큰 수가 PICO8_TOKEN_LIMIT을 넘으면
 * 불러는 되지만 실행되지 않는다("program too large").
 */
#define PICO8_SOURCE_LIMIT    0x10000   /* 65536 bytes */
#define PICO8_TOKEN_LIMIT     8192

/* ------------------------------------------------------------------ *
 *  The RAM itself
 * ------------------------------------------------------------------ */

/*
 * The whole memory of the machine, 64KB.  Everything (the screen, the
 * sprites, the cart data, the draw state) lives in here.
 *
 * 기계의 전체 메모리인 64KB. 화면, 스프라이트, 카트 데이터, 드로우 상태가 모두
 * 여기에 들어 있다.
 */
extern uint8_t pico8_ram[PICO8_RAM_SIZE];

/* ------------------------------------------------------------------ *
 *  Fixed point helpers
 * ------------------------------------------------------------------ *
 * PICO-8 stores numbers as 32 bit fixed point values with 16 fractional
 * bits.  The helpers below convert them to and from the integers Lua
 * works with.  They truncate towards zero, which is what PICO-8 does.
 *
 * PICO-8은 숫자를 소수점 아래 16비트인 32비트 고정소수로 저장한다. 아래 헬퍼는
 * 그것을 Lua가 쓰는 정수와 오간다. 0 방향으로 버리며, PICO-8도 그렇게 한다.
 */

static inline int32_t fix32_to_int(fix32 x)    { return (int32_t)x; }
static inline int32_t fix32_to_int32(fix32 x)  { return (int32_t)x; }
static inline uint8_t  fix32_to_uint8(fix32 x)  { return (uint8_t)(int32_t)x; }
static inline uint16_t fix32_to_uint16(fix32 x) { return (uint16_t)(int32_t)x; }
static inline uint32_t fix32_to_uint32(fix32 x) { return (uint32_t)(int32_t)x; }
static inline fix32 fix32_from_int(int32_t x)    { return fix32(x); }
static inline fix32 fix32_from_uint8(uint8_t x)  { return fix32((int32_t)x); }
static inline fix32 fix32_from_uint16(uint16_t x) { return fix32((int32_t)x); }
static inline fix32 fix32_from_uint32(uint32_t x) { return fix32((int32_t)x); }

/*
 * Build a fixed point value out of two 16 bit halves, high part first.
 *
 * 상위 16비트와 하위 16비트 두 조각에서 고정소수 값을 만든다.
 */
static inline fix32 fix32_value(uint16_t hi, uint16_t lo) {
    return fix32::frombits(((int32_t)hi << 16) | (uint32_t)lo);
}

/* ------------------------------------------------------------------ *
 *  The palette
 * ------------------------------------------------------------------ */

/*
 * The 16 colours of the PICO-8 as 24 bit RGB.  The cart refers to them by
 * index (0..15); the host turns the index into this colour.
 *
 * PICO-8의 16색을 24비트 RGB로 적은 것. 카트는 인덱스(0..15)로 색을 가리키고,
 * 호스트가 그 인덱스를 이 색으로 바꾼다.
 */
static const uint32_t pico8_palette[PICO8_PAL_COUNT] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751,
    0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436,
    0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
};

/* ------------------------------------------------------------------ *
 *  Memory
 * ------------------------------------------------------------------ */

/*
 * Fill the RAM with the state a fresh PICO-8 starts in.
 *
 * 새로 켠 PICO-8이 시작하는 상태로 RAM을 채운다.
 */
void pico8_reset_memory(void);

/*
 * Copy a cartridge image (or a raw dump) into the RAM.
 *
 * 카트리지 이미지(또는 raw 덤프)를 RAM에 복사한다.
 */
void pico8_load_memory(const uint8_t *data, uint32_t len);

/*
 * Reset the draw state (RAM 0x5f00..0x5f7f) to its default values,
 * matching PICO-8's RESET().
 *
 * 드로우 상태(RAM 0x5f00~0x5f7f)를 기본값으로 되돌린다. PICO-8의 RESET()과
 * 같다.
 */
void pico8_reset_draw_state(void);

/*
 * The default song and the default sound effect, written into RAM.
 *
 * 기본 song과 기본 효과음을 RAM에 쓴다.
 */
void pico8_init_song_default(void);
void pico8_init_sfx_default(void);

#endif /* PICO8_CORE_H */
