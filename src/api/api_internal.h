#ifndef PICO8_API_INTERNAL_H
#define PICO8_API_INTERNAL_H

/*
 * Internal helpers shared by the modules of src/api/.
 *
 * The PICO-8 API is split over several translation units, one per subsystem
 * (drawing, text, memory, input, audio, system, cartridge, Lua helpers).
 * Every module owns the Lua functions of its subsystem and registers them
 * itself, so this header only carries what really crosses a module border.
 *
 * src/api/ 안의 모듈(그래픽, 텍스트, 메모리, 입력, 오디오, 시스템, 카트리지,
 * Lua 헬퍼) 사이에서만 공유되는 헬퍼를 모아둔 헤더. 각 모듈은 자기 영역의
 * Lua 함수를 직접 등록하므로, 여기에는 모듈 경계를 넘어가는 것만 둔다.
 */

#include <stdint.h>

#include "lua.h"
#include "lauxlib.h"
#include "pico8.h"

/* ------------------------------------------------------------------ *
 *  Camera
 * ------------------------------------------------------------------ */

/*
 * Move a screen coordinate into camera space.
 *
 * PICO-8 stores the camera offset as two 16 bit signed values in the draw
 * state (0x5f29/0x5f2a for x, 0x5f2b/0x5f2c for y).  Every drawing call
 * subtracts that offset from the coordinates the cart passed in, so a cart
 * can scroll the world by moving the camera.
 *
 * 화면 좌표를 카메라 공간으로 옮긴다. 카메라는 드로우 상태(0x5f29~0x5f2c)에
 * 16비트 부호 있는 값으로 저장되어 있고, 모든 드로잉 호출이 카트가 준 좌표에서
 * 그 오프셋을 뺀다. 카메라는 카트가 세상의 좌표를 스크롤시키는 수단이다.
 */
void api_apply_camera(int *x, int *y);

/* ------------------------------------------------------------------ *
 *  Module registration
 * ------------------------------------------------------------------ */

/*
 * Each module registers its own Lua functions.  The dispatcher below calls
 * them in a fixed order; the order does not matter for the behaviour (the
 * names are unique), it only keeps the order stable between runs.
 *
 * 각 모듈이 자기 Lua 함수를 직접 등록한다. 아래 디스패처가 정해진 순서로
 * 호출하지만, 이름이 겹치지 않으므로 동작에는 영향이 없고 실행 순서만 고정된다.
 */
void pico8_api_register_gfx(lua_State *L);
void pico8_api_register_text(lua_State *L);
void pico8_api_register_memory(lua_State *L);
void pico8_api_register_input(lua_State *L);
void pico8_api_register_audio(lua_State *L);
void pico8_api_register_system(lua_State *L);
void pico8_api_register_cart(lua_State *L);
void pico8_api_register_lua(lua_State *L);

#endif /* PICO8_API_INTERNAL_H */
