#ifndef PICO8_VM_H
#define PICO8_VM_H

/*
 * pico8_vm.h — the bridge to the Lua virtual machine.
 *
 * The cart is a Lua program.  This header is how the rest of the emulator
 * talks to that program: loading a chunk, running the init/update/draw
 * functions of a cart, and asking whether a function exists at all.
 *
 * 카트는 Lua 프로그램이다. 이 헤더는 나머지 에뮬레이터가 그 프로그램과
 * 대화하는 방법이다: 청크를 불러오고, 카트의 init/update/draw 함수를
 * 실행하고, 함수가 아예 존재하는지 묻는다.
 *
 * The VM itself is z8lua, a Lua 5.2 fork that stores its numbers the way
 * PICO-8 does (32 bit fixed point) and that can read and write the
 * cart's RAM directly.  See platforms/common/deps/z8lua.
 *
 * VM 자체는 z8lua다. PICO-8과 같은 방식으로 숫자를 저장하고(32비트 고정소수)
 * 카트의 RAM을 직접 읽고 쓸 수 있는 Lua 5.2 포크다.
 * platforms/common/deps/z8lua 참고.
 */

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  Lifetime
 * ------------------------------------------------------------------ */

/*
 * Create the Lua state and register the PICO-8 API in it.
 *
 * Lua 상태를 만들고 그 안에 PICO-8 API를 등록한다.
 */
int  pico8_vm_init(void);

/*
 * Close the Lua state.
 *
 * Lua 상태를 닫는다.
 */
void pico8_vm_destroy(void);

/*
 * Load a chunk of Lua source.  Returns 0 on success; on a syntax error the
 * error is reported and the return value is non-zero.
 *
 * Lua 소스 한 덩어리를 불러온다. 성공하면 0. 문법 오류라면 오류를 보고하고
 * 0이 아닌 값을 돌려준다.
 */
int  pico8_vm_load_buffer(const char *buf, size_t len, const char *name);

/*
 * Non-zero when the last error was already reported to the user.
 *
 * 마지막 오류를 이미 사용자에게 보고했으면 0이 아니다.
 */
int  pico8_vm_error_reported(void);

/* ------------------------------------------------------------------ *
 *  Running a cart
 * ------------------------------------------------------------------ */

/*
 * Call the cart's init(), update() or draw() function.
 *
 * 카트의 init(), update(), draw() 함수를 부른다.
 */
void pico8_vm_call_init(void);
void pico8_vm_call_update(void);
void pico8_vm_call_draw(void);

/*
 * Call the cart's menuitem() callback for a pause menu entry and report
 * whether the cart handled it.
 *
 * 일시정지 메뉴 항목의 menuitem() 콜백을 부르고, 카트가 처리했는지
 * 알려 준다.
 */
int  pico8_vm_call_menuitem(int index, int buttons);

/*
 * Non-zero when the cart defines the named function.
 *
 * 카트가 그 이름의 함수를 정의하고 있으면 0이 아니다.
 */
int  pico8_vm_has_function(const char *name);

/*
 * Push the C traceback handler used as the error handler when the host
 * calls into cart code (PICO-8 exposes no __traceback global).
 *
 * 호스트가 카트 코드를 부를 때 오류 처리기로 쓰는 C 트레이스백 핸들러를
 * 스택에 올린다(PICO-8에는 __traceback 전역이 없다).
 */
void pico8_vm_push_traceback(lua_State *L);

/* ------------------------------------------------------------------ *
 *  State of the running cart
 * ------------------------------------------------------------------ */

/*
 * The command line the cart was started with.
 *
 * 카트를 시작할 때 주어진 커맨드 라인.
 */
void pico8_vm_set_cmdline(const char *args);
extern const char *pico8_cmdline;

/*
 * Non-zero once the cart has stopped (stop() was called, or it crashed).
 *
 * 카트가 멈췄으면 0이 아니다(stop()를 부르거나 오류로 죽은 경우).
 */
extern int pico8_vm_halted;

/*
 * How many frames have been drawn since the cart was started.
 *
 * 카트를 시작한 뒤 그려진 프레임 수.
 */
extern uint32_t pico8_frame_count;

/* ------------------------------------------------------------------ *
 *  The API and the shell
 * ------------------------------------------------------------------ */

/*
 * Register the whole PICO-8 API in the global table of a Lua state.
 *
 * PICO-8 API 전체를 Lua 상태의 전역 테이블에 등록한다.
 */
void pico8_register_api(lua_State *L);

/*
 * Evaluate a single console line through the Lua VM, printing any expression
 * results (and errors) to the shell. Returns 0 on success.
 *
 * 콘솔 줄 하나를 Lua VM으로 계산한다. 식의 결과와 오류는 셸에 출력한다.
 * 성공하면 0.
 */
int  pico8_vm_eval(const char *line);

#endif /* PICO8_VM_H */
