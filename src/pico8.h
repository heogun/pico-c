#ifndef PICO8_H
#define PICO8_H

#include <stdint.h>
#include <stdbool.h>

#include <string>

#include "fix32.h"
#include "lua.h"

using z8::fix32;

#define PICO8_RAM_SIZE     0x10000
#define PICO8_SCREEN_W     128
#define PICO8_SCREEN_H     128

#define PICO8_PAL_COUNT    16
#define PICO8_SPRITE_COUNT 256
#define PICO8_MAP_W        128
#define PICO8_MAP_H        64
#define PICO8_FPS          60

#define PICO8_GFX_ADDR     0x0000
#define PICO8_MAP_ADDR     0x2000
#define PICO8_MAP2_ADDR    0x1000
#define PICO8_GFX_FLAGS    0x3000
#define PICO8_SONG_ADDR    0x3100
#define PICO8_SFX_ADDR     0x3200
#define PICO8_SFX_SIZE     68
#define PICO8_USER_DATA    0x4300
#define PICO8_CUSTOM_FONT  0x5600
#define PICO8_PERSISTENT   0x5e00
#define PICO8_DRAW_STATE   0x5f00
#define PICO8_HW_STATE     0x5f40
#define PICO8_GPIO_PINS    0x5f80
#define PICO8_SCREEN       0x6000
#define PICO8_EXT_MAP      0x8000

#define PICO8_CART_DATA_SIZE  0x4300

extern uint8_t pico8_ram[PICO8_RAM_SIZE];

static inline int32_t fix32_to_int(fix32 x)    { return (int32_t)x; }
static inline int32_t fix32_to_int32(fix32 x)  { return (int32_t)x; }
static inline uint8_t  fix32_to_uint8(fix32 x)  { return (uint8_t)(int32_t)x; }
static inline uint16_t fix32_to_uint16(fix32 x) { return (uint16_t)(int32_t)x; }
static inline uint32_t fix32_to_uint32(fix32 x) { return (uint32_t)(int32_t)x; }
static inline fix32 fix32_from_int(int32_t x)    { return fix32(x); }
static inline fix32 fix32_from_uint8(uint8_t x)  { return fix32((int32_t)x); }
static inline fix32 fix32_from_uint16(uint16_t x) { return fix32((int32_t)x); }
static inline fix32 fix32_from_uint32(uint32_t x) { return fix32((int32_t)x); }
static inline fix32 fix32_value(uint16_t hi, uint16_t lo) {
    return fix32::frombits(((int32_t)hi << 16) | (uint32_t)lo);
}

static const uint32_t pico8_palette[PICO8_PAL_COUNT] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751,
    0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436,
    0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
};

enum {
    PICO8_BTN_LEFT  = 0,
    PICO8_BTN_RIGHT = 1,
    PICO8_BTN_UP    = 2,
    PICO8_BTN_DOWN  = 3,
    PICO8_BTN_O     = 4,
    PICO8_BTN_X     = 5,
    PICO8_BTN_COUNT = 6,
};

extern volatile uint8_t pico8_btn_state[2];
extern volatile uint8_t pico8_btn_pressed[2];
extern uint8_t pico8_btn_held[2][PICO8_BTN_COUNT];
/* True for each button on the frames that btnp() reports a press (either the
   initial press or an auto-repeat). Recomputed once per logical (update) frame. */
extern uint8_t pico8_btnp_state[2][PICO8_BTN_COUNT];

void pico8_reset_memory(void);
void pico8_init_song_default(void);
void pico8_init_sfx_default(void);
void pico8_load_memory(const uint8_t *data, uint32_t len);

void pico8_render_screen(uint32_t *pixels, int pitch, int scale);

int  pico8_vm_init(void);
void pico8_vm_destroy(void);
int  pico8_vm_load_buffer(const char *buf, size_t len, const char *name);
void pico8_vm_call_init(void);
void pico8_vm_call_update(void);
void pico8_vm_call_draw(void);
int  pico8_vm_call_menuitem(int index, int buttons);
int  pico8_vm_has_function(const char *name);
/* Push the C traceback handler used as the error handler when the host
   calls into cart code (PICO-8 exposes no __traceback global). */
void pico8_vm_push_traceback(lua_State *L);

void pico8_register_api(lua_State *L);
void pico8_pal_reset_all(void);
void pico8_draw_char(int x, int y, uint8_t ch, uint8_t color);
void pico8_draw_char_scaled(int x, int y, uint8_t ch, uint8_t color, int scale);
void pico8_clear_screen(uint8_t color);
void pico8_put_pixel(int x, int y, uint8_t color);

void pico8_update_input(int update60);
void pico8_reset_input(void);

/* ------------------------------------------------------------------ *
 *  Devkit mouse + keyboard input (enabled via POKE(0x5f2d, flags))
 * ------------------------------------------------------------------ */

extern int pico8_mouse_x;         /* absolute pointer position, 0..127 */
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

int  pico8_load_cart(const char *path);
int  pico8_run_cart(const char *path);

/* ------------------------------------------------------------------ *
 *  Cartridge storage (save / load / reload / cstore)
 * ------------------------------------------------------------------ */

/* Raw Lua source of the currently loaded cart (preserved by save()). */
void        pico8_cart_set_source(const char *src, size_t len);
const char *pico8_cart_source(size_t *len);
void        pico8_cart_set_path(const char *path);
const char *pico8_cart_get_path(void);

/* The cart's on-disk data area (0x0000..0x42ff), retained for reload(). */
const uint8_t *pico8_cart_rom(void);

/* Serialize live RAM (or the stored cart ROM) plus the source to a .p8 file. */
int pico8_cart_save_p8(const char *path);
int pico8_cart_save_rom_p8(const char *path);

/* Read only a cartridge's data area (0x4300 bytes) without touching RAM. */
int pico8_cart_read_data(const char *path, uint8_t *out, size_t out_len);

/* reload()/cstore() against the stored cart ROM (and an optional other file). */
int pico8_cart_reload(int dest, int source, int len, const char *filename);
int pico8_cart_cstore(int dest, int source, int len, const char *filename);

/* Queue a cartridge to be loaded and run at the end of the frame (Lua load()).
   Returns 0 when the request was queued, -1 when the file cannot be found. */
int pico8_console_queue_load(const char *filename, const char *breadcrumb,
                             const char *param);
extern int  pico8_console_request_load;
const char *pico8_console_breadcrumb(void);
const char *pico8_console_pending_path(void);
const char *pico8_console_pending_param(void);

void pico8_vm_set_cmdline(const char *args);
extern const char *pico8_cmdline;
extern int pico8_vm_halted;
extern uint32_t pico8_frame_count;

int  pico8_audio_init(void);
void pico8_audio_destroy(void);
void pico8_audio_sfx(int sfx_id, int channel, int offset, int length);
void pico8_audio_music(int song_id, int fade_len, int channel_mask);
void pico8_audio_stop(void);
int  pico8_audio_sfx_id(int channel);        /* playing sfx id, or -1 */
int  pico8_audio_note_num(int channel);      /* current note index, or -1 */
int  pico8_audio_music_pattern(void);        /* stat(54), or -1 */
int  pico8_audio_music_count(void);          /* stat(55) */
int  pico8_audio_music_ticks(void);          /* stat(56), or -1 */
int  pico8_audio_music_playing(void);        /* stat(57) */

/* ------------------------------------------------------------------ *
 *  Text charset
 * ------------------------------------------------------------------ *
 * PICO-8 keeps text as charset bytes (its own font's codes) and only
 * turns it into UTF-8 at the edges: cart files and console output are
 * UTF-8 text, everything inside the VM is bytes. See src/charset.cpp. */
std::string pico8_charset_from_utf8(const char *src, size_t len);
std::string pico8_charset_to_utf8(const char *src, size_t len);

/* ------------------------------------------------------------------ *
 *  Console: boot splash + command shell
 * ------------------------------------------------------------------ */

/* Application modes owned by the host loop. */
enum {
    PICO8_APP_BOOT  = 0,
    PICO8_APP_SHELL = 1,
    PICO8_APP_RUN   = 2,
};

/* Special (non-printable) keys delivered to the shell. */
enum {
    PICO8_KEY_UP       = 0x101,
    PICO8_KEY_DOWN     = 0x102,
    PICO8_KEY_LEFT     = 0x103,
    PICO8_KEY_RIGHT    = 0x104,
    PICO8_KEY_HOME     = 0x105,
    PICO8_KEY_END      = 0x106,
    PICO8_KEY_TAB      = 0x107,
};

/* Requests raised by shell commands, handled by the host loop. */
extern int pico8_console_request_run;
extern int pico8_console_request_reboot;
extern int pico8_console_request_quit;
extern int pico8_console_request_pause;

/* Open the current working folder in the host file browser. */
void pico8_console_open_folder(void);

/* Run a shell command line (used by the shell-command Lua globals). */
void pico8_console_exec(const char *line);

/* Pause-menu items registered from Lua with menuitem(). */
void        pico8_menuitem_clear(void);
int         pico8_menuitem_has(int index);
int         pico8_menuitem_count(void);
const char *pico8_menuitem_label(int index);
int         pico8_menuitem_btnmask(int index);
int         pico8_menuitem_invoke(lua_State *L, int index, int buttons);

/* Boot splash. */
#define PICO8_BOOT_MS 2600
void pico8_console_init(void);
int  pico8_console_boot_finished(uint32_t elapsed_ms);
void pico8_console_boot_draw(uint32_t elapsed_ms);

/* Shell. */
void pico8_console_shell_draw(void);
void pico8_console_shell_key(int key);
void pico8_console_shell_text(const char *utf8);
void pico8_console_shell_reset(void);
void pico8_console_print(const char *line);
/* Let the shell know which cartridge is currently loaded (called by the host). */
void pico8_console_set_cart(const char *path, const char *source, size_t source_len);

/* Load a cartridge and register it with the shell. Returns 0 on success. */
int  pico8_console_load(const char *path);

/* While the shell evaluates a console line, print() output is routed to the
   console instead of the screen. api.cpp checks pico8_console_capturing(). */
int  pico8_console_capturing(void);
void pico8_console_emit(const char *text, int color);

/* Evaluate a single console line through the Lua VM, printing any expression
   results (and errors) to the shell. Returns 0 on success. */
int  pico8_vm_eval(const char *line);

/* Reset the runtime draw state (RAM 0x5f00..0x5f7f) to its default values,
   matching PICO-8's RESET(). */
void pico8_reset_draw_state(void);

#endif
