/*
 * console.cpp — the console of the shell: its state and its drawing.
 *
 * The scrollback, the input line, the history and the colours they use
 * live here, together with the two things the shell draws: the boot splash
 * and the prompt with the blinking cursor.
 *
 * 셸의 콘솔: 그 상태와 그 그림. 스크롤백, 입력 줄, 히스토리와 그 색들이
 * 여기에 있고, 셸이 그리는 두 가지(부팅 화면과 깜빡이는 커서가 있는 프롬프트)
 * 도 여기 있다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <string>
#include <vector>
#include <algorithm>

#include "pico8.h"
#include "util.h"
#include "shell_internal.h"

/* ------------------------------------------------------------------ */
/*  Console state                                                     */
/* ------------------------------------------------------------------ */

/* The scrollback of the console.  셸의 스크롤백. */
std::vector<ConsoleLine>  g_lines;
/* What is being typed.  입력 중인 글자. */
std::string               g_input;
/* The commands that were run before.  예전에 실행한 명령들. */
std::vector<std::string>  g_history;
/* Where we are in the history; -1 means "not in it".  히스토리 위치. */
int                       g_hist_pos = -1;
/* The line that was typed when the history was entered.  원래 입력한 줄. */
std::string               g_draft;

/* The cart that is loaded.  올라와 있는 카트. */
std::string               g_cart_path;
std::string               g_cart_name;
std::string               g_source;
int                       g_has_cart = 0;

/* The frame counter, used for the blinking cursor.  깜빡이는 커서용. */
static int                g_frames = 0;

/* ------------------------------------------------------------------ */
/*  Small helpers                                                      */
/* ------------------------------------------------------------------ */

/*
 * shell_put_text(x, y, s, color) — write a line of text on the screen.
 *
 * Draws the string with the built-in font, one character every four
 * pixels.
 *
 * 내장 폰트로 문자열을 그린다. 문자 하나마다 네 픽셀씩.
 */
void shell_put_text(int x, int y, const std::string &s, uint8_t color)
{
    int cx = x;
    for (size_t i = 0; i < s.size(); i++) {
        pico8_draw_char(cx, y, (uint8_t)s[i], color);
        cx += CH_W;
    }
}

/*
 * shell_add_line(text, color) — one more line in the scrollback.
 *
 * Adds the text to the scrollback of the console.  A text that is longer
 * than the screen is cut into several lines.
 *
 * 텍스트를 셸의 스크롤백에 더한다. 화면보다 긴 텍스트는 여러 줄로 잘린다.
 */
void shell_add_line(const std::string &text, uint8_t color)
{
    if (text.empty()) {
        g_lines.push_back(ConsoleLine{color, ""});
        return;
    }
    for (size_t i = 0; i < text.size(); i += COLS)
        g_lines.push_back(ConsoleLine{color, text.substr(i, COLS)});
}

/*
 * shell_print(text, color) — write something in the console.
 *
 * Splits the text at the line breaks and adds every piece to the
 * scrollback.  The scrollback holds the last 500 lines; the older ones are
 * thrown away.
 *
 * 텍스트를 줄바꿈으로 쪼개어 각 조각을 스크롤백에 더한다. 스크롤백은 최근
 * 500줄을 남기고 더 오래된 것은 버린다.
 */
void shell_print(const std::string &text, uint8_t color)
{
    size_t start = 0;
    for (;;) {
        size_t nl = text.find('\n', start);
        std::string seg = (nl == std::string::npos) ? text.substr(start)
                                                    : text.substr(start, nl - start);
        shell_add_line(seg, color);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    const size_t kMaxScrollback = 500;
    if (g_lines.size() > kMaxScrollback)
        g_lines.erase(g_lines.begin(), g_lines.begin() + (g_lines.size() - kMaxScrollback));
}

/*
 * shell_print_error(s) — an error message.
 *
 * An error message in the colour of the errors.
 *
 * 오류 색으로 쓰인 오류 메시지.
 */
void shell_print_error(const std::string &s) { shell_print(s, COL_ERROR); }

/*
 * shell_print_info(s) — an information line.
 *
 * A line in the colour of the information.
 *
 * 정보 색으로 쓰인 한 줄.
 */
void shell_print_info(const std::string &s)  { shell_print(s, COL_INFO); }

/* ------------------------------------------------------------------ */
/*  Console (Lua) evaluation output                                    */
/* ------------------------------------------------------------------ */

int g_capturing = 0;

/*
 * pico8_console_capturing() — is the console collecting the output?
 *
 * While the output of a cart is being collected, the host does not put it
 * on the screen itself.  This tells the two apart.
 *
 * 카트의 출력을 모으는 동안 호스트는 그 내용을 화면에 직접 띄우지 않는다.
 * 이 함수가 둘을 구분한다.
 */
int pico8_console_capturing(void) { return g_capturing; }

/*
 * pico8_console_emit(text, color) — the output of a cart.
 *
 * The text a cart printed, which goes into the scrollback of the console.
 *
 * 카트가 출력한 텍스트. 셸의 스크롤백에 들어간다.
 */
void pico8_console_emit(const char *text, int color)
{
    if (text) shell_print(text, (uint8_t)color);
}

/* ------------------------------------------------------------------ */
/*  Boot splash                                                        */
/* ------------------------------------------------------------------ */

/*
 * pico8_console_init() — a fresh console.
 *
 * Empties the scrollback, the input line and the history, and forgets the
 * cart that was loaded.  This is what the machine does when it is switched
 * on.
 *
 * 스크롤백, 입력 줄, 히스토리를 비우고, 불러온 카트를 잊는다. 기계가 켜질
 * 때 하는 일이다.
 */
void pico8_console_init(void)
{
    g_lines.clear();
    g_input.clear();
    g_history.clear();
    g_hist_pos = -1;
    g_draft.clear();
    g_cart_path.clear();
    g_cart_name.clear();
    g_source.clear();
    g_has_cart = 0;
    g_frames = 0;

    pico8_console_request_run = 0;
    pico8_console_request_reboot = 0;
    pico8_console_request_quit = 0;
}

/*
 * pico8_console_shell_reset() — an empty input line.
 *
 * Clears what was typed and forgets the place in the history.
 *
 * 입력한 내용을 지우고 히스토리 위치를 잊는다.
 */
void pico8_console_shell_reset(void)
{
    g_input.clear();
    g_hist_pos = -1;
}

/*
 * pico8_console_boot_finished(elapsed_ms) — is the boot splash over?
 *
 * True once the splash has been on the screen long enough.
 *
 * 부팅 화면이 충분히 오래 있었으면 참.
 */
int pico8_console_boot_finished(uint32_t elapsed_ms)
{
    return elapsed_ms >= PICO8_BOOT_MS;
}

/*
 * pico8_console_boot_draw(t) — draw the boot splash.
 *
 * Draws the splash at the time `t` (in milliseconds since the machine was
 * switched on).  The letters of the logo come one by one and flash a
 * colour before they turn white; the subtitle and the version tag follow,
 * and a thin line sweeps along the bottom edge.
 *
 * 기계를 켠 뒤 `t`밀리초 시점의 부팅 화면을 그린다. 로고의 글자가 하나씩
 * 나오며 흰색으로 바뀌기 전에 색을 한 번 깜빡인다. 그다음 부제와 버전 표시가
 * 나오고, 아래 가장자리를 가는 선이 한 번 지나간다.
 */
void pico8_console_boot_draw(uint32_t t)
{
    pico8_clear_screen(COL_BG);
    for (int i = 0; i < 16; i++)
        pico8_ram[PICO8_DRAW_STATE + 0x10 + i] = (uint8_t)i;

    static const char *LOGO = "PICO-8";
    static const uint8_t FLASH[6] = { 8, 9, 10, 11, 12, 13 };

    const int scale = 2;
    const int n = 6;
    const int advance = CH_W * scale;              /* 10 */
    const int width = (n - 1) * advance + 4 * scale;
    const int x0 = (PICO8_SCREEN_W - width) / 2;   /* 35 */
    const int y0 = 44;

    /* Letters pop in one at a time, flashing a colour before turning white. */
    for (int i = 0; i < n; i++) {
        uint32_t appear = 220 + (uint32_t)i * 130;
        if (t < appear) continue;
        uint8_t color = ((t - appear) < 130) ? FLASH[i] : 7;
        pico8_draw_char_scaled(x0 + i * advance, y0, (uint8_t)LOGO[i], color, scale);
    }

    /* Subtitle fades in. */
    if (t > 1150) {
        const char *sub = "fantasy console";
        int sw = (int)strlen(sub) * CH_W - 1;
        uint8_t color = (t < 1300) ? 7 : COL_DIM;
        shell_put_text((PICO8_SCREEN_W - sw) / 2, 82, sub, color);
    }

    /* Version tag. */
    if (t > 1500) {
        const char *ver = "pico-c 0.1";
        int vw = (int)strlen(ver) * CH_W - 1;
        shell_put_text(PICO8_SCREEN_W - vw - 2, 119, ver, COL_DIM);
    }

    /* Little progress sweep along the bottom edge. */
    int bw = (int)((uint64_t)t * PICO8_SCREEN_W / PICO8_BOOT_MS);
    if (bw > PICO8_SCREEN_W) bw = PICO8_SCREEN_W;
    for (int x = 0; x < bw; x++)
        pico8_put_pixel(x, PICO8_SCREEN_H - 1, COL_INFO);
}

/* ------------------------------------------------------------------ */
/*  Shell rendering                                                    */
/* ------------------------------------------------------------------ */

/*
 * pico8_console_shell_draw() — draw the shell.
 *
 * Draws the scrollback, the prompt with what has been typed and a block
 * cursor that blinks.
 *
 * 스크롤백, 프롬프트와 입력 중인 글자, 깜빡이는 블록 커서를 그린다.
 */
void pico8_console_shell_draw(void)
{
    pico8_clear_screen(COL_BG);
    for (int i = 0; i < 16; i++)
        pico8_ram[PICO8_DRAW_STATE + 0x10 + i] = (uint8_t)i;

    std::string full = "> " + g_input;

    int input_rows = (int)((full.size() + COLS - 1) / COLS);
    if (input_rows < 1) input_rows = 1;
    int max_out = ROWS - input_rows;
    if (max_out < 0) max_out = 0;

    int total = (int)g_lines.size();
    int visible = total > max_out ? max_out : total;
    int start = total - visible;

    for (int i = 0; i < visible; i++)
        shell_put_text(0, i * CH_H, g_lines[start + i].text, g_lines[start + i].color);

    /* Prompt + current input, wrapping at the screen edge. */
    for (size_t i = 0; i < full.size(); i++) {
        int idx = (int)i;
        int rx = (idx % COLS) * CH_W;
        int ry = (visible + idx / COLS) * CH_H;
        uint8_t color = (idx < 2) ? COL_TEXT : COL_INPUT;
        pico8_draw_char(rx, ry, (uint8_t)full[i], color);
    }

    /* Blinking block cursor. */
    g_frames++;
    if ((g_frames / 30) & 1) {
        int idx = (int)full.size();
        int rx = (idx % COLS) * CH_W;
        int ry = (visible + idx / COLS) * CH_H;
        for (int yy = 0; yy < CH_H - 1; yy++)
            for (int xx = 0; xx < 4; xx++)
                pico8_put_pixel(rx + xx, ry + yy, COL_TEXT);
    }
}

/*
 * pico8_console_print(line) — a line from the host.
 *
 * Prints a line that the host itself wants to show in the console.
 *
 * 호스트가 셸에 보여주고 싶은 한 줄을 출력한다.
 */
void pico8_console_print(const char *line)
{
    if (line) shell_print(line, COL_TEXT);
}

/*
 * pico8_console_set_cart(path, source, len) — remember the cart.
 *
 * Tells the shell which cart is loaded and keeps its source, so that the
 * commands can show and save it.
 *
 * 셸에 어느 카트가 올라와 있는지 알려 주고 소스를 보관한다. 그래야 명령들이
 * 그 카트를 보여주고 저장할 수 있다.
 */
void pico8_console_set_cart(const char *path, const char *source, size_t len)
{
    if (path) {
        g_has_cart = 1;
        g_cart_path = path;
        g_cart_name = util_path_base(path);
    }
    if (source && len)
        g_source.assign(source, len);
}
