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

void pico8_mouse_motion(int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    pico8_mouse_x = x;
    pico8_mouse_y = y;
}

void pico8_mouse_relative(int dx, int dy)
{
    pico8_mouse_dx += dx;
    pico8_mouse_dy += dy;
}

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

void pico8_mouse_wheel_add(int dy)
{
    pico8_mouse_wheel += dy;
}

void pico8_keyboard_char(int ch)
{
    int next = (kbd_head + 1) % PICO8_KBD_BUF;
    if (next == kbd_tail) return;      /* buffer full: drop */
    kbd_buf[kbd_head] = ch;
    kbd_head = next;
}

int pico8_keyboard_available(void)
{
    return kbd_head != kbd_tail;
}

int pico8_keyboard_read(void)
{
    if (kbd_head == kbd_tail) return -1;
    int ch = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % PICO8_KBD_BUF;
    return ch;
}

void pico8_input_frame_end(void)
{
    pico8_mouse_wheel = 0;
    pico8_mouse_dx = 0;
    pico8_mouse_dy = 0;
}

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
