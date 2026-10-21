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

/*
 * The modes the host loop can be in.
 *
 * 호스트 루프가 있을 수 있는 모드.
 */
enum {
    PICO8_APP_BOOT  = 0,   /* the boot splash is on the screen / 부팅 화면 */
    PICO8_APP_SHELL = 1,   /* the command shell is on the screen / 명령 셸 */
    PICO8_APP_RUN   = 2,   /* a cart is running / 카트가 동작 중 */
};

/* ------------------------------------------------------------------ *
 *  Keys
 * ------------------------------------------------------------------ */

/*
 * Special (non-printable) keys delivered to the shell.
 *
 * 셸로 전달되는 특별한(출력되지 않는) 키.
 */
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

/*
 * The shell raised one of these; the host loop acts on it and clears it.
 *
 * 셸이 올린 요청. 호스트 루프가 처리하고 지운다.
 */
extern int pico8_console_request_run;
extern int pico8_console_request_reboot;
extern int pico8_console_request_quit;
extern int pico8_console_request_pause;

/*
 * Queue a cartridge to be loaded and run at the end of the frame (Lua load()).
 * Returns 0 when the request was queued, -1 when the file cannot be found.
 *
 * 프레임이 끝나면 불러와 실행할 카트리지를 큐에 올린다(Lua의 load()).
 * 큐에 올렸으면 0, 파일을 찾지 못하면 -1.
 */
int  pico8_console_queue_load(const char *filename, const char *breadcrumb,
                              const char *param);

/* The host loop was asked to load the queued cartridge.  셸이 올린 로드 요청. */
extern int  pico8_console_request_load;

/* Where the queued cart comes from.  큐에 올린 카트가 오는 곳. */
const char *pico8_console_breadcrumb(void);

/* The path of the queued cart.  큐에 올린 카트의 경로. */
const char *pico8_console_pending_path(void);

/* The parameter the queued cart gets with stat(6).  큐에 올린 카트의 stat(6) 값. */
const char *pico8_console_pending_param(void);

/* ------------------------------------------------------------------ *
 *  Shell commands
 * ------------------------------------------------------------------ */

/*
 * Open the current working folder in the host file browser.
 *
 * 현재 작업 폴더를 호스트의 파일 브라우저로 연다.
 */
void pico8_console_open_folder(void);

/*
 * Run a shell command line (used by the shell-command Lua globals).
 *
 * 셸 명령 한 줄을 실행한다(셸 명령을 쓰는 Lua 전역 함수가 쓴다).
 */
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

/*
 * Forget every registered menu item.
 *
 * 등록된 메뉴 항목을 모두 잊는다.
 */
void        pico8_menuitem_clear(void);

/* Is there a menu item with that number?  그 번호의 항목이 있는가. */
int         pico8_menuitem_has(int index);

/* How many menu items are there?  메뉴 항목이 몇 개인가. */
int         pico8_menuitem_count(void);

/* The text of a menu item.  메뉴 항목의 글자. */
const char *pico8_menuitem_label(int index);

/* The buttons a menu item answers to.  메뉴 항목이 받는 버튼. */
int         pico8_menuitem_btnmask(int index);

/* Run the callback of a menu item; non-zero when the cart handled it.
 * 메뉴 항목의 콜백을 부른다. 카트가 처리했으면 0이 아니다. */
int         pico8_menuitem_invoke(lua_State *L, int index, int buttons);

/* ------------------------------------------------------------------ *
 *  Boot splash
 * ------------------------------------------------------------------ */

#define PICO8_BOOT_MS 2600   /* how long the splash is on the screen / 부팅 화면 시간 */

/* Start with an empty console.  빈 셸로 시작한다. */
void pico8_console_init(void);

/* Is the boot splash over?  부팅 화면이 끝났는가. */
int  pico8_console_boot_finished(uint32_t elapsed_ms);

/* Draw the boot splash.  부팅 화면을 그린다. */
void pico8_console_boot_draw(uint32_t elapsed_ms);

/* ------------------------------------------------------------------ *
 *  The shell itself
 * ------------------------------------------------------------------ */

/* Draw the shell.  셸을 그린다. */
void pico8_console_shell_draw(void);

/* A key was pressed in the shell.  셸에서 키가 눌렸다. */
void pico8_console_shell_key(int key);

/* Text was typed in the shell.  셸에 글자가 입력되었다. */
void pico8_console_shell_text(const char *utf8);

/* Clear the input line.  입력 줄을 비운다. */
void pico8_console_shell_reset(void);

/* Print a line in the shell.  셸에 한 줄을 출력한다. */
void pico8_console_print(const char *line);

/*
 * Let the shell know which cartridge is currently loaded (called by the host).
 *
 * 지금 어느 카트가 올라와 있는지 셸에 알려 준다(호스트가 부른다).
 */
void pico8_console_set_cart(const char *path, const char *source, size_t source_len);

/*
 * Load a cartridge and register it with the shell. Returns 0 on success.
 *
 * 카트리지 하나를 불러오고 셸에 등록한다. 성공하면 0.
 */
int  pico8_console_load(const char *path);

/*
 * While the shell evaluates a console line, print() output is routed to the
 * console instead of the screen. api_text.cpp checks pico8_console_capturing().
 *
 * 셸이 콘솔 줄을 계산하는 동안 print()의 출력은 화면이 아니라 셸로 간다.
 * api_text.cpp이 pico8_console_capturing()을 확인한다.
 */
int  pico8_console_capturing(void);

/* The text a cart printed, which goes into the scrollback of the console.
 * 카트가 출력한 텍스트. 셸의 스크롤백에 들어간다. */
void pico8_console_emit(const char *text, int color);

#endif /* PICO8_CONSOLE_H */
