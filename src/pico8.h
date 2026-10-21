#ifndef PICO8_H
#define PICO8_H

/*
 * pico8.h — everything the emulator exposes, in one include.
 *
 * The real declarations live in one header per subsystem:
 *
 *   pico8_core.h     the constants of the machine, the RAM, the fixed point
 *   pico8_gfx.h      the picture: rendering and drawing
 *   pico8_input.h    the buttons, the mouse, the devkit keyboard
 *   pico8_audio.h    the sound
 *   pico8_cart.h     the cartridge files
 *   pico8_console.h  the boot splash, the shell, the pause menu
 *   pico8_vm.h       the bridge to the Lua virtual machine
 *   pico8_charset.h  between PICO-8's font codes and UTF-8
 *
 * 이 헤더는 에뮬레이터가 노출하는 모든 것을 한 번에 포함한다.
 * 실제 선언은 서브시스템별 헤더에 들어 있다:
 *
 *   pico8_core.h     기계의 상수, RAM, 고정소수
 *   pico8_gfx.h      화면: 렌더링과 드로잉
 *   pico8_input.h    버튼, 마우스, 데브킷 키보드
 *   pico8_audio.h    소리
 *   pico8_cart.h     카트리지 파일
 *   pico8_console.h  부팅 화면, 셸, 일시정지 메뉴
 *   pico8_vm.h       Lua 가상기계와 잇는 다리
 *   pico8_charset.h  PICO-8 폰트 코드와 UTF-8 사이
 *
 * A file that only needs one part of the emulator can include just that
 * header; including this one is the easy way and pulls in everything.
 *
 * 에뮬레이터의 일부만 필요한 파일은 해당 헤더만 포함하면 된다. 이 헤더를
 * 포함하면 편하지만 전부 들어온다.
 */

#include "pico8_core.h"
#include "pico8_gfx.h"
#include "pico8_input.h"
#include "pico8_audio.h"
#include "pico8_cart.h"
#include "pico8_console.h"
#include "pico8_vm.h"
#include "pico8_charset.h"

#endif /* PICO8_H */
