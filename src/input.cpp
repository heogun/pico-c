/*
 * input.cpp — the buttons, the mouse and the devkit keyboard.
 *
 * The host fills in the state of the buttons once per frame and this file
 * turns it into what the Lua side of the machine wants to know: whether a
 * button is held, whether it was just pressed, and how long it has been
 * held.
 *
 * 호스트가 버튼 상태를 프레임마다 채우고, 이 파일이 그것을 기계의 Lua 쪽이
 * 원하는 정보로 바꾼다. 버튼이 눌려 있는지, 방금 눌렸는지, 얼마나 오래
 * 눌려 있었는지.
 *
 * The auto repeat follows the documented behaviour of PICO-8: a button that
 * has just been pressed reports true, then after a delay of held frames it
 * reports true again, and every repeat frames after that.  The two numbers
 * live in the RAM at 0x5f5c and 0x5f5f.
 *
 * 자동 반복은 PICO-8에 문서화된 동작을 따른다. 방금 눌린 버튼은 true를 보고하고,
 * delay 프레임만큼 누르고 있으면 다시 true를 보고하며, 그 뒤에는 repeat
 * 프레임마다 true를 보고한다. 두 숫자는 RAM의 0x5f5c과 0x5f5f에 있다.
 *
 * The mouse and the keyboard belong to the devkit: a cart switches them on
 * with POKE(0x5f2d, flags), and the flags say which of them report what.
 *
 * 마우스와 키보드는 데브킷에 속한다. 카트가 POKE(0x5f2d, flags)로 켜고, 플래그가
 * 무엇을 보고할지 정한다.
 */

#include <string.h>
#include "pico8.h"

volatile uint8_t pico8_btn_state[2]  = {0, 0};
volatile uint8_t pico8_btn_pressed[2] = {0, 0};
uint8_t pico8_btn_held[2][PICO8_BTN_COUNT];
uint8_t pico8_btnp_state[2][PICO8_BTN_COUNT];

/* Devkit mouse + keyboard state (see POKE(0x5f2d, flags)). */
int pico8_mouse_x       = 0;
int pico8_mouse_y       = 0;
int pico8_mouse_buttons = 0;
int pico8_mouse_wheel   = 0;
int pico8_mouse_dx      = 0;
int pico8_mouse_dy      = 0;

#define PICO8_KBD_BUF 64
static int kbd_buf[PICO8_KBD_BUF];
static int kbd_head = 0;   /* next write */
static int kbd_tail = 0;   /* next read  */

/*
 * pico8_mouse_motion(x, y) — where the mouse pointer is.
 *
 * Sets the absolute position of the pointer on the 128x128 screen.  A
 * negative coordinate is clamped to 0, because the screen has no room for
 * it.
 *
 * 128x128 화면 위 마우스 포인터의 절대 위치를 정한다. 음수 좌표는 0으로
 * 맞춰지는데, 화면에 그만큼 공간이 없기 때문이다.
 */
void pico8_mouse_motion(int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    pico8_mouse_x = x;
    pico8_mouse_y = y;
}

/*
 * pico8_mouse_relative(dx, dy) — how far the mouse moved.
 *
 * Accumulates the relative movement of the pointer.  The sum is what
 * stat(38) and stat(39) report, and it is cleared at the end of every
 * frame.
 *
 * 포인터의 상대 이동량을 누적한다. 이 합이 stat(38), stat(39)이 돌려주는 값이고,
 * 프레임이 끝나면 0으로 돌아간다.
 */
void pico8_mouse_relative(int dx, int dy)
{
    pico8_mouse_dx += dx;
    pico8_mouse_dy += dy;
}

/*
 * pico8_mouse_button(sdl_button, down) — the state of a mouse button.
 *
 * The SDL button numbers are translated into the bit mask PICO-8 uses:
 * bit 0 is the left button, bit 1 the right one and bit 2 the middle one.
 * Any other button is ignored.
 *
 * SDL의 버튼 번호를 PICO-8이 쓰는 비트 마스크로 바꾼다. 0번 비트가 왼쪽,
 * 1번 비트가 오른쪽, 2번 비트가 가운데 버튼이다. 그 외의 버튼은 무시한다.
 */
void pico8_mouse_button(int sdl_button, int down)
{
    int bit = 0;
    switch (sdl_button) {
        case 1: bit = 1; break;   /* SDL_BUTTON_LEFT   -> bit0 */
        case 3: bit = 2; break;   /* SDL_BUTTON_RIGHT  -> bit1 */
        case 2: bit = 4; break;   /* SDL_BUTTON_MIDDLE -> bit2 */
        default: return;
    }
    if (down) pico8_mouse_buttons |= bit;
    else      pico8_mouse_buttons &= ~bit;
}

/*
 * pico8_mouse_wheel_add(dy) — the movement of the mouse wheel.
 *
 * The movements of the frame are summed up; stat(36) reports the sum and
 * it is cleared at the end of the frame.
 *
 * 한 프레임 동안의 휠 이동을 누적한다. stat(36)이 이 합을 보고하고, 프레임
 * 끝에 0이 된다.
 */
void pico8_mouse_wheel_add(int dy)
{
    pico8_mouse_wheel += dy;
}

/*
 * pico8_keyboard_char(ch) — a character was typed.
 *
 * Pushes a character into the keyboard buffer.  When the buffer is full the
 * character is dropped, so a fast typist cannot make the emulator run out
 * of memory.
 *
 * 키보드 버퍼에 문자 하나를 넣는다. 버퍼가 가득 차면 그 문자는 버려진다.
 * 너무 빠른 타이핑으로 메모리가 부족해지는 일을 막기 위해서다.
 */
void pico8_keyboard_char(int ch)
{
    int next = (kbd_head + 1) % PICO8_KBD_BUF;
    if (next == kbd_tail) return;      /* buffer full: drop */
    kbd_buf[kbd_head] = ch;
    kbd_head = next;
}

/*
 * pico8_keyboard_available() — is a character waiting?
 *
 * 비어 있지 않다면 1, 비어 있다면 0을 돌려준다.
 */
int pico8_keyboard_available(void)
{
    return kbd_head != kbd_tail;
}

/*
 * pico8_keyboard_read() — the next character that was typed.
 *
 * Pops the oldest character out of the buffer, or returns -1 when the
 * buffer is empty.
 *
 * 버퍼에서 가장 오래된 문자를 꺼내 돌려준다. 버퍼가 비어 있으면 -1을
 * 돌려준다.
 */
int pico8_keyboard_read(void)
{
    if (kbd_head == kbd_tail) return -1;
    int ch = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % PICO8_KBD_BUF;
    return ch;
}

/*
 * pico8_input_frame_end() — the end of a frame.
 *
 * The wheel and the relative mouse movement belong to a single frame, so
 * they are cleared here.
 *
 * 휠과 마우스 상대 이동은 한 프레임에만 속하는 값이므로 여기서 지운다.
 */
void pico8_input_frame_end(void)
{
    pico8_mouse_wheel = 0;
    pico8_mouse_dx = 0;
    pico8_mouse_dy = 0;
}

/*
 * pico8_reset_input() — forget every button press.
 *
 * Called when a cart is loaded, so that a button that was already held
 * does not look like a fresh press to the new cart.
 *
 * 카트를 불러올 때 호출한다. 이미 눌려 있던 버튼이 새 카트에게 새 누름으로
 * 보이지 않게 하기 위해서다.
 */
void pico8_reset_input(void)
{
    pico8_btn_state[0] = pico8_btn_state[1] = 0;
    pico8_btn_pressed[0] = pico8_btn_pressed[1] = 0;
    memset(pico8_btn_held, 0, sizeof(pico8_btn_held));
    memset(pico8_btnp_state, 0, sizeof(pico8_btnp_state));

    pico8_mouse_x = pico8_mouse_y = 0;
    pico8_mouse_buttons = 0;
    pico8_mouse_wheel = 0;
    pico8_mouse_dx = pico8_mouse_dy = 0;
    kbd_head = kbd_tail = 0;
}

/* Advance the input state by one logical frame. Called once per _update /
   _update60 invocation, mirroring the way PICO-8 resets btnp state at the
   start of each update.

   Auto-repeat follows PICO-8's documented behaviour: a freshly pressed button
   reports true, then after `delay` held frames it reports true again, and every
   `repeat` frames after that. The delays live at 0x5f5c / 0x5f5f; 0 selects the
   defaults (15 and 4), 255 (delay) disables repeating. Values are given in
   30fps frames and are doubled when running at 60fps. */

/*
 * pico8_update_input(update60) — move the input state one frame forward.
 *
 * Called once per _update / _update60, mirroring the way PICO-8 clears the
 * btnp state at the start of every update.
 *
 * The auto repeat follows the documented behaviour of PICO-8: a button that
 * has just been pressed reports true, then after "delay" frames of being
 * held it reports true again, and every "repeat" frames after that.  The
 * two numbers live at 0x5f5c and 0x5f5f; 0 selects the defaults (15 and 4)
 * and 255 in the delay switches the repeating off.  They are counted in
 * 30fps frames and doubled when the machine runs at 60fps.
 *
 * _update / _update60이 호출될 때마다 한 번씩 부른다. PICO-8이 매 update에서
 * btnp 상태를 지우는 것과 같은 방식이다.
 *
 * 자동 반복은 PICO-8에 문서화된 동작을 따른다. 방금 눌린 버튼은 true를 보고하고,
 * delay 프레임만큼 누르고 있으면 다시 true를 보고하며, 그 뒤에는 repeat 프레임
 * 마다 true를 보고한다. 두 숫자는 0x5f5c과 0x5f5f에 있고, 0이면 기본값(15와 4)을,
 * delay에 255가 있으면 반복을 끈다. 30fps 프레임 단위로 세며 60fps로 돌 때에는
 * 두 배가 된다.
 */
void pico8_update_input(int update60)
{
    int delay = pico8_ram[0x5f5c];
    int repeat = pico8_ram[0x5f5f];
    if (delay == 0) delay = 15;
    if (repeat == 0) repeat = 4;
    if (update60) {
        if (delay != 255) delay *= 2;
        repeat *= 2;
    }

    for (int p = 0; p < 2; p++) {
        uint8_t cur = pico8_btn_state[p];
        if (p == 0 && (pico8_ram[0x5f2d] & 0x2)) {
            /* Devkit flag 0x2: mouse buttons drive btn(4)/btn(5). */
            if (pico8_mouse_buttons & 1) cur |= (uint8_t)(1 << PICO8_BTN_O);
            if (pico8_mouse_buttons & 2) cur |= (uint8_t)(1 << PICO8_BTN_X);
        }
        uint8_t prev = pico8_ram[PICO8_HW_STATE + 0x0c + p];
        pico8_btn_pressed[p] = cur & ~prev;
        pico8_ram[PICO8_HW_STATE + 0x0c + p] = cur;

        for (int b = 0; b < PICO8_BTN_COUNT; b++) {
            int bit = 1 << b;
            if (cur & bit) {
                if (pico8_btn_held[p][b] < 255) pico8_btn_held[p][b]++;
            } else {
                pico8_btn_held[p][b] = 0;
            }

            int h = pico8_btn_held[p][b];
            int fire = 0;
            if (h == 1) {
                fire = 1;                       /* the initial press */
            } else if (delay != 255 && h >= delay + 1 &&
                       ((h - 1 - delay) % repeat) == 0) {
                fire = 1;                       /* an auto-repeat */
            }
            pico8_btnp_state[p][b] = (uint8_t)fire;
        }
    }
}
