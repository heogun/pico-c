#ifndef PICO8_CONSOLE_H
#define PICO8_CONSOLE_H

/*
 * pico8_console.h — the boot splash, the shell and the pause menu.
 *
 * PICO-8 is not only a game console: it also has a command shell, a boot
 * splash, a pause menu and a file browser.  This header is the interface
 * between that side of the program and the rest of the emulator.
 *
 * PICO-8은 게임 콘솔만은 아니다: 명령 셸, 부팅 화면, 일시정지 메뉴, 파일
 * 브라우저도 있다. 이 헤더는 그쪽과 나머지 에뮬레이터 사이의 접점이다.
 *
 * The shell evaluates its lines through the Lua VM (pico8_vm_eval), so the
 * shell knows the whole Lua API.  A shell command that wants to change
 * what the machine does (run a cart, reboot, quit) raises a request that
 * the host loop picks up.
 *
 * 셸은 Lua VM을 통해 명령을 평가하므로(pico8_vm_eval) 셸은 Lua API 전체를
 * 알고 있다. 기계의 상태를 바꾸려는 셸 명령(카트 실행, 재부팅, 종료)은
 * 요청을 올리고 호스트 루프가 그것을 처리한다.
 */

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  Application modes
 * ------------------------------------------------------------------ */

/* The modes the host loop can be in. */
enum {
    PICO8_APP_BOOT  = 0,   /* the boot splash is on the screen  */
    PICO8_APP_SHELL = 1,   /* the command shell is on the screen */
    PICO8_APP_RUN   = 2,   /* a cart is running                 */
};

/* ------------------------------------------------------------------ *
 *  Keys
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ *
 *  Requests to the host loop
 * ------------------------------------------------------------------ */

/* The shell raised one of these; the host loop acts on it and clears it. */
extern int pico8_console_request_run;
extern int pico8_console_request_reboot;
extern int pico8_console_request_quit;
extern int pico8_console_request_pause;

/*
 * Queue a cartridge to be loaded and run at the end of the frame (Lua load()).
 * Returns 0 when the request was queued, -1 when the file cannot be found.
 */
int  pico8_console_queue_load(const char *filename, const char *breadcrumb,
                              const char *param);
extern int  pico8_console_request_load;
const char *pico8_console_breadcrumb(void);
const char *pico8_console_pending_path(void);
const char *pico8_console_pending_param(void);

/* ------------------------------------------------------------------ *
 *  Shell commands
 * ------------------------------------------------------------------ */

/* Open the current working folder in the host file browser. */
void pico8_console_open_folder(void);

/* Run a shell command line (used by the shell-command Lua globals). */
void pico8_console_exec(const char *line);

/* ------------------------------------------------------------------ *
 *  Pause menu
 * ------------------------------------------------------------------ *
 * The cart registers the entries of the pause menu with menuitem(); the
 * host shows them and calls back when one is picked.
 *
 * 카트가 menuitem()으로 일시정지 메뉴의 항목을 등록하고, 호스트가 그것을
 * 보여주며 하나가 선택되면 다시 불러준다.
 */

/* Forget every registered menu item. */
void        pico8_menuitem_clear(void);
int         pico8_menuitem_has(int index);
int         pico8_menuitem_count(void);
const char *pico8_menuitem_label(int index);
int         pico8_menuitem_btnmask(int index);
int         pico8_menuitem_invoke(lua_State *L, int index, int buttons);

/* ------------------------------------------------------------------ *
 *  Boot splash
 * ------------------------------------------------------------------ */

#define PICO8_BOOT_MS 2600   /* how long the splash is on the screen */

void pico8_console_init(void);
int  pico8_console_boot_finished(uint32_t elapsed_ms);
void pico8_console_boot_draw(uint32_t elapsed_ms);

/* ------------------------------------------------------------------ *
 *  The shell itself
 * ------------------------------------------------------------------ */

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
   console instead of the screen. api_text.cpp checks pico8_console_capturing(). */
int  pico8_console_capturing(void);
void pico8_console_emit(const char *text, int color);

#endif /* PICO8_CONSOLE_H */
