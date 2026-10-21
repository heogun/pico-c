#ifndef PICO8_INPUT_H
#define PICO8_INPUT_H

/*
 * pico8_input.h — the buttons, the mouse and the devkit keyboard.
 *
 * The host fills in the state of the buttons once per frame; the Lua side
 * (btn(), btnp()) only reads it.  The devkit mouse and keyboard are
 * switched on by a poke into the hardware state and are then fed from the
 * SDL events.
 *
 * 호스트가 버튼 상태를 프레임마다 채우고, Lua 쪽(btn(), btnp())은 그 상태를
 * 읽기만 한다. 데브킷 마우스와 키보드는 하드웨어 상태에 poke를 하면 켜지고,
 * 그 뒤 SDL 이벤트가 이쪽으로 들어온다.
 */

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  Buttons
 * ------------------------------------------------------------------ */

/*
 * The six buttons of the PICO-8, in the order btn() uses.
 *
 * PICO-8의 여섯 버튼. btn()이 사용하는 순서와 같다.
 */
enum {
    PICO8_BTN_LEFT  = 0,
    PICO8_BTN_RIGHT = 1,
    PICO8_BTN_UP    = 2,
    PICO8_BTN_DOWN  = 3,
    PICO8_BTN_O     = 4,
    PICO8_BTN_X     = 5,
    PICO8_BTN_COUNT = 6,
};

/*
 * The raw button state as the host sees it: one byte per button, one bit
 * per player (bit 0 is player one).
 *
 * 호스트가 보는 원시 버튼 상태. 버튼당 한 바이트, 플레이어당 한 비트
 * (0번 비트가 1P).
 */
extern volatile uint8_t pico8_btn_state[2];
extern volatile uint8_t pico8_btn_pressed[2];

/* How long each button has been held, in frames, per player. */
extern uint8_t pico8_btn_held[2][PICO8_BTN_COUNT];

/* True for each button on the frames that btnp() reports a press (either the
   initial press or an auto-repeat). Recomputed once per logical (update) frame. */
extern uint8_t pico8_btnp_state[2][PICO8_BTN_COUNT];

/* Read the host's button state and update the held/pressed bookkeeping.
   update60 is non-zero on the frames that run a cart update. */
void pico8_update_input(int update60);

/* Forget every button press (used when a cart is loaded or reset). */
void pico8_reset_input(void);

/* ------------------------------------------------------------------ *
 *  Devkit mouse and keyboard
 * ------------------------------------------------------------------ *
 * The devkit devices are switched on by POKE(0x5f2d, flags):
 *
 *   0x1  the mouse reports its position
 *   0x2  the mouse reports its buttons
 *   0x4  the mouse reports relative movement
 *   0x8  the keyboard reports typed characters
 *
 * 데브킷 장치는 POKE(0x5f2d, flags)로 켠다:
 *
 *   0x1  마우스가 위치를 보고한다
 *   0x2  마우스가 버튼을 보고한다
 *   0x4  마우스가 상대 이동을 보고한다
 *   0x8  키보드가 입력 문자를 보고한다
 */

/* The absolute pointer position on the 128x128 screen, 0..127. */
extern int pico8_mouse_x;
extern int pico8_mouse_y;
extern int pico8_mouse_buttons;   /* bit0 = left, bit1 = right, bit2 = middle */
extern int pico8_mouse_wheel;     /* wheel movement accumulated this frame */
extern int pico8_mouse_dx;        /* relative movement in host pixels (flag 0x4) */
extern int pico8_mouse_dy;

void pico8_mouse_motion(int x, int y);         /* set the absolute screen position */
void pico8_mouse_relative(int dx, int dy);     /* accumulate relative movement */
void pico8_mouse_button(int sdl_button, int down);
void pico8_mouse_wheel_add(int dy);
void pico8_keyboard_char(int ch);              /* push a typed character */
int  pico8_keyboard_available(void);           /* non-zero when a keypress is pending */
int  pico8_keyboard_read(void);                /* pop the next character, or -1 */
void pico8_input_frame_end(void);              /* clear per-frame mouse events */

#endif /* PICO8_INPUT_H */
