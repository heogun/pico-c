#ifndef PICO8_SHELL_INTERNAL_H
#define PICO8_SHELL_INTERNAL_H

/*
 * shell_internal.h — the parts of the shell that its own files share.
 *
 * The shell is split over several files: the console itself, the cartridge
 * files it looks at, the commands it runs and the keyboard it listens to.
 * This header holds what they have in common: the geometry of the console
 * text, the colours it uses, the state of the session and the few helpers
 * that more than one file needs.
 *
 * 셸은 여러 파일로 나뉘어 있다: 콘솔 자체, 그것이 들여다보는 카트리지 파일,
 * 그것이 실행하는 명령, 그것이 듣는 키보드. 이 헤더는 그들이 공유하는 것을
 * 담는다: 콘솔 글자의 배치, 쓰는 색, 세션의 상태, 그리고 둘 이상의 파일이
 * 필요로 하는 몇 개의 헬퍼.
 *
 * None of this is part of the public interface of pico-c: the header is
 * private to the shell.
 *
 * 이 중 어느 것도 pico-c의 공개 인터페이스가 아니다. 이 헤더는 셸만 쓴다.
 */

#include <stdint.h>

#include <string>
#include <vector>

#include "pico8.h"

/* ------------------------------------------------------------------ *
 *  The geometry of the console text
 * ------------------------------------------------------------------ */

constexpr int CH_W = 4;                       /* character advance   */
constexpr int CH_H = 6;                       /* line height         */
constexpr int COLS = PICO8_SCREEN_W / CH_W;   /* 32 columns          */
constexpr int ROWS = PICO8_SCREEN_H / CH_H;   /* 21 rows             */

/* ------------------------------------------------------------------ *
 *  The colours the console uses
 * ------------------------------------------------------------------ */

enum {
    COL_BG    = 0,
    COL_DIM   = 5,
    COL_TEXT  = 6,
    COL_INPUT = 7,
    COL_ERROR = 8,
    COL_OK    = 11,
    COL_INFO  = 12,
};

/* ------------------------------------------------------------------ *
 *  The state of the session
 * ------------------------------------------------------------------ */

/* One line of the scrollback.  스크롤백의 한 줄. */
struct ConsoleLine {
    uint8_t     color;
    std::string text;
};

extern std::vector<ConsoleLine>  g_lines;     /* the scrollback        */
extern std::string               g_input;     /* what is being typed   */
extern std::vector<std::string>  g_history;   /* the commands before   */
extern int                       g_hist_pos;  /* where we are in it    */
extern std::string               g_draft;     /* the line that was typed */

extern std::string               g_cart_path; /* the cart that runs    */
static std::string               g_cart_name; /* its name              */
static std::string               g_source;    /* its source text       */
extern int                       g_has_cart;  /* is there a cart?      */

extern int                       g_capturing; /* collecting the output */

/* ------------------------------------------------------------------ *
 *  The helpers that more than one file needs
 * ------------------------------------------------------------------ */

/* Write a line of text on the screen.  화면에 한 줄을 쓴다. */
void shell_put_text(int x, int y, const std::string &s, uint8_t color);

/* Add a line to the scrollback.  스크롤백에 한 줄을 더한다. */
void shell_add_line(const std::string &text, uint8_t color);

/* Write something in the console.  콘솔에 내용을 쓴다. */
void shell_print(const std::string &text, uint8_t color);

/* An error message.  오류 메시지. */
void shell_print_error(const std::string &s);

/* An information line.  정보 줄. */
void shell_print_info(const std::string &s);

/* Is that the name of a cartridge?  저것이 카트 이름인가. */
bool        shell_is_cart_name(const std::string &n);

/* Find the cartridge with that name.  그 이름의 카트를 찾는다. */
std::string shell_resolve_cart(const std::string &name);

/* Run one command line.  셸 명령 한 줄을 실행한다. */
void shell_execute(const std::string &raw);

/* ------------------------------------------------------------------ *
 *  The commands of the shell
 * ------------------------------------------------------------------ */

/*
 * What a command does.  Every command gets the first and the second word of
 * the line; most of them ignore one of them.
 *
 * 명령이 하는 일. 모든 명령은 줄의 첫 번째 단어와 두 번째 단어를 받는다.
 * 대부분은 그중 하나를 무시한다.
 */
typedef void (*ShellCommandFn)(const std::string &arg, const std::string &arg2);

/*
 * One row of the command table: the word the player types, what it does and
 * whether the tab key offers it.
 *
 * 명령 표의 한 줄: 플레이어가 치는 단어, 그것이 하는 일, 탭 키가 제시하는지.
 */
struct ShellCommand {
    const char     *name;   /* the word the player types   */
    ShellCommandFn  run;    /* what it does                */
    bool            offer;  /* does the tab key offer it?  */
};

/* The commands the shell knows, in the order the tab key lists them.
 * 셸이 아는 명령들. 탭 키가 보여주는 순서대로. */
extern const ShellCommand kShellCommands[];

#endif /* PICO8_SHELL_INTERNAL_H */
