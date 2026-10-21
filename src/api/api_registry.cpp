/*
 * api_registry.cpp — the entry point of the PICO-8 API.
 *
 * The API is split over the modules of this folder, one per subsystem.  Each
 * module registers its own functions; this file only calls them, in a fixed
 * order, and keeps the two functions the rest of the program calls.
 *
 * API는 이 폴더의 모듈(서브시스템별로 하나씩)로 나뉘어 있다. 각 모듈이 자기
 * 함수를 직접 등록하고, 이 파일은 정해진 순서로 그것들을 호출하며 나머지
 * 프로그램이 쓰는 두 함수만 남긴다.
 */

#include "api_internal.h"

/*
 * Register the whole PICO-8 API in the global table of a Lua state.
 *
 * PICO-8 exposes its API as plain globals, so a cart calls cls() or spr()
 * without any prefix.  The names are registered in the global table of the
 * state; the stock Lua globals are removed afterwards by pico8_apply_parity()
 * (see src/vm.cpp), which needs this to have run first.
 *
 * PICO-8의 API는 접두사 없는 전역 함수다. 따라서 카트는 cls()나 spr()를 그냥
 * 호출한다. 여기서 Lua 상태의 전역 테이블에 이름을 등록하고, stock Lua 전역은
 * pico8_apply_parity()(src/vm.cpp)가 나중에 제거한다. 그 전에 이것이 끝나야 한다.
 */
void pico8_register_api(lua_State *L)
{
    pico8_api_register_gfx(L);
    pico8_api_register_text(L);
    pico8_api_register_memory(L);
    pico8_api_register_input(L);
    pico8_api_register_audio(L);
    pico8_api_register_system(L);
    pico8_api_register_cart(L);
    pico8_api_register_lua(L);
}

/*
 * Kept for compatibility with the old single file API; there is nothing to
 * prepare any more.
 *
 * 예전 단일 파일 API와의 호환을 위해 남겨둔 함수. 더 준비할 일은 없다.
 */
void pico8_init_api(void)
{
}
