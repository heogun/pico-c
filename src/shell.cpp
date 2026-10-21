/* shell.cpp — PICO-8 style boot splash and command shell.
 *
 * The splash is drawn with the built-in 4x6 font (scaled up for the logo),
 * exactly like the rest of the fantasy console.  The shell provides a
 * blinking prompt, command history, tab completion and the usual set of
 * PICO-8 host commands (load/save/run/dir/cd/...).
 *
 * 부팅 화면은 나머지 판타지 콘솔과 똑같이 내장 4x6 폰트로 그린다(로고는
 * 확대한다). 셸은 깜빡이는 프롬프트, 명령 히스토리, 탭 완성, 그리고 PICO-8의
 * (호스트 명령 load/save/run/dir/cd/...)을 제공한다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <string>
#include <vector>
#include <algorithm>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pico8.h"

/* ------------------------------------------------------------------ */
/*  Host requests raised by commands (handled by main.cpp)             */
/* ------------------------------------------------------------------ */

int pico8_console_request_run    = 0;
int pico8_console_request_reboot = 0;
int pico8_console_request_quit   = 0;
int pico8_console_request_load   = 0;
int pico8_console_request_pause  = 0;

/* A load()/run() request queued from Lua, consumed by the host loop. */
static std::string g_pending_path;
static std::string g_pending_breadcrumb;
static std::string g_pending_param;

/* ------------------------------------------------------------------ */
/*  Console state                                                      */
/* ------------------------------------------------------------------ */

struct ConsoleLine {
    uint8_t     color;
    std::string text;
};

static std::vector<ConsoleLine>  g_lines;
static std::string               g_input;
static std::vector<std::string>  g_history;
static int                       g_hist_pos = -1;
static std::string               g_draft;

static std::string               g_cart_path;
static std::string               g_cart_name;
static std::string               g_source;
static int                       g_has_cart = 0;

static int                       g_frames = 0;

static const int CH_W = 4;                       /* character advance   */
static const int CH_H = 6;                       /* line height         */
static const int COLS = PICO8_SCREEN_W / CH_W;   /* 32 columns          */
static const int ROWS = PICO8_SCREEN_H / CH_H;   /* 21 rows             */

enum {
    COL_BG    = 0,
    COL_DIM   = 5,
    COL_TEXT  = 6,
    COL_INPUT = 7,
    COL_ERROR = 8,
    COL_OK    = 11,
    COL_INFO  = 12,
};

/* ------------------------------------------------------------------ */
/*  Small helpers                                                      */
/* ------------------------------------------------------------------ */

/*
 * trim(s) — the string without its spaces.
 *
 * Removes the white space at the beginning and at the end.
 *
 * 앞뒤의 공백을 지운 문자열.
 */
static std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

/*
 * lower(s) — the string in small letters.
 *
 * Turns every letter into its small form.  The commands of the shell are
 * not case sensitive.
 *
 * 모든 알파벳을 소문자로 바꾼 문자열. 셸의 명령은 대소문자를 가리지 않는다.
 */
static std::string lower(const std::string &s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

/*
 * has_suffix(s, suf) — does the string end with that?
 *
 * True when the string ends with the given text.
 *
 * 그 문자열이 주어진 글자로 끝나면 참.
 */
static bool has_suffix(const std::string &s, const char *suf)
{
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

/*
 * basename_of(p) — the name of a file without its directory.
 *
 * Everything behind the last '/' or '\\'.
 *
 * 마지막 '/' 또는 '\\' 뒤의 부분.
 */
static std::string basename_of(const std::string &p)
{
    size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

/*
 * put_text(x, y, s, color) — write a line of text on the screen.
 *
 * Draws the string with the built-in font, one character every four
 * pixels.
 *
 * 내장 폰트로 문자열을 그린다. 문자 하나마다 네 픽셀씩.
 */
static void put_text(int x, int y, const std::string &s, uint8_t color)
{
    int cx = x;
    for (size_t i = 0; i < s.size(); i++) {
        pico8_draw_char(cx, y, (uint8_t)s[i], color);
        cx += CH_W;
    }
}

/*
 * add_line(text, color) — one more line in the scrollback.
 *
 * Adds the text to the scrollback of the console.  A text that is longer
 * than the screen is cut into several lines.
 *
 * 텍스트를 셸의 스크롤백에 더한다. 화면보다 긴 텍스트는 여러 줄로 잘린다.
 */
static void add_line(const std::string &text, uint8_t color)
{
    if (text.empty()) {
        g_lines.push_back(ConsoleLine{color, ""});
        return;
    }
    for (size_t i = 0; i < text.size(); i += COLS)
        g_lines.push_back(ConsoleLine{color, text.substr(i, COLS)});
}

/*
 * print(text, color) — write something in the console.
 *
 * Splits the text at the line breaks and adds every piece to the
 * scrollback.  The scrollback holds the last 500 lines; the older ones are
 * thrown away.
 *
 * 텍스트를 줄바꿈으로 쪼개어 각 조각을 스크롤백에 더한다. 스크롤백은 최근
 * 500줄을 남기고 더 오래된 것은 버린다.
 */
static void print(const std::string &text, uint8_t color)
{
    size_t start = 0;
    for (;;) {
        size_t nl = text.find('\n', start);
        std::string seg = (nl == std::string::npos) ? text.substr(start)
                                                    : text.substr(start, nl - start);
        add_line(seg, color);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    const size_t kMaxScrollback = 500;
    if (g_lines.size() > kMaxScrollback)
        g_lines.erase(g_lines.begin(), g_lines.begin() + (g_lines.size() - kMaxScrollback));
}

/*
 * print_error(s) — an error message.
 *
 * An error message in the colour of the errors.
 *
 * 오류 색으로 쓰인 오류 메시지.
 */
static void print_error(const std::string &s) { print(s, COL_ERROR); }

/*
 * print_info(s) — an information line.
 *
 * A line in the colour of the information.
 *
 * 정보 색으로 쓰인 한 줄.
 */
static void print_info(const std::string &s)  { print(s, COL_INFO); }

/* ------------------------------------------------------------------ */
/*  Console (Lua) evaluation output                                    */
/* ------------------------------------------------------------------ */

static int g_capturing = 0;

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
    if (text) print(text, (uint8_t)color);
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
        put_text((PICO8_SCREEN_W - sw) / 2, 82, sub, color);
    }

    /* Version tag. */
    if (t > 1500) {
        const char *ver = "pico-c 0.1";
        int vw = (int)strlen(ver) * CH_W - 1;
        put_text(PICO8_SCREEN_W - vw - 2, 119, ver, COL_DIM);
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
        put_text(0, i * CH_H, g_lines[start + i].text, g_lines[start + i].color);

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
    if (line) print(line, COL_TEXT);
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
        g_cart_name = basename_of(path);
    }
    if (source && len)
        g_source.assign(source, len);
}

/* ------------------------------------------------------------------ */
/*  Cartridge files                                                    */
/* ------------------------------------------------------------------ */

/*
 * is_cart_name(n) — is that the name of a cartridge?
 *
 * True for a .p8 file and for a .p8.png file.
 *
 * .p8 파일과 .p8.png 파일이면 참.
 */
static bool is_cart_name(const std::string &n)
{
    return has_suffix(n, ".p8") || has_suffix(n, ".p8.png");
}

/*
 * resolve_cart(name) — find the cartridge with that name.
 *
 * Looks for the file and, when the name has no extension, tries the three
 * names that PICO-8 would try: .p8, .p8.png and .png.  An empty result
 * means that there is no such file.
 *
 * 파일을 찾는다. 확장자가 없으면 PICO-8이 시도하는 세 이름(.p8, .p8.png, .png)을
 * 순서대로 시도한다. 결과가 비어 있으면 그런 파일이 없다는 뜻이다.
 */
static std::string resolve_cart(const std::string &name)
{
    if (name.empty()) return "";

    std::vector<std::string> cands;
    cands.push_back(name);
    if (!has_suffix(name, ".p8") && !has_suffix(name, ".png")) {
        cands.push_back(name + ".p8");
        cands.push_back(name + ".p8.png");
        cands.push_back(name + ".png");
    }

    struct stat st;
    for (size_t i = 0; i < cands.size(); i++)
        if (stat(cands[i].c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return cands[i];
    return "";
}

/*
 * dir_of(p) — the directory of a path.
 *
 * Everything up to and including the last '/' or '\\'.
 *
 * 마지막 '/' 또는 '\\'까지의 부분.
 */
static std::string dir_of(const std::string &p)
{
    size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : p.substr(0, pos + 1);
}

/*
 * pico8_console_queue_load(filename, breadcrumb, param) — load a cart after this frame.
 *
 * Puts a cartridge on the list of things the host has to do.  An empty
 * file name means "load the cart that is running again".  The breadcrumb
 * is the name of the place the cart comes from and the parameter is the
 * string the cart gets with stat(6).
 *
 * Returns 0 when the cart was found and queued.
 *
 * 호스트가 다음에 해야 할 일 목록에 카트를 올린다. 파일 이름이 비어 있으면
 * "지금 실행 중인 카트를 다시 불러라"는 뜻이다. breadcrumb은 카트가 오는
 * 곳의 이름이고, parameter는 카트가 stat(6)으로 받는 문자열이다.
 *
 * 카트를 찾아 큐에 올렸으면 0을 돌려준다.
 */
int pico8_console_queue_load(const char *filename, const char *breadcrumb,
                             const char *param)
{
    std::string req = filename ? filename : "";
    if (req.empty()) {
        if (g_cart_path.empty()) return -1;
        req = g_cart_path;
    } else {
        std::string resolved = resolve_cart(req);
        if (resolved.empty()) {
            std::string dir = dir_of(g_cart_path);
            if (!dir.empty()) resolved = resolve_cart(dir + req);
        }
        if (resolved.empty() && req.size() && req[0] == '/') {
            struct stat st;
            if (stat(req.c_str(), &st) == 0 && S_ISREG(st.st_mode))
                resolved = req;
        }
        if (resolved.empty()) return -1;
        req = resolved;
    }

    g_pending_path       = req;
    g_pending_breadcrumb = breadcrumb ? breadcrumb : "";
    g_pending_param      = param ? param : "";
    pico8_console_request_load = 1;
    return 0;
}

/*
 * pico8_console_breadcrumb() — where the cart comes from.
 *
 * The place the queued cart comes from, or NULL when there is none.
 *
 * 큐에 올린 카트가 오는 곳. 없으면 NULL.
 */
const char *pico8_console_breadcrumb(void)
{
    return g_pending_breadcrumb.empty() ? NULL : g_pending_breadcrumb.c_str();
}

/*
 * pico8_console_pending_path() — the cart that is waiting.
 *
 * The path of the cartridge that was queued.
 *
 * 큐에 올려 둔 카트의 경로.
 */
const char *pico8_console_pending_path(void)  { return g_pending_path.c_str(); }

/*
 * pico8_console_pending_param() — the parameter of the queued cart.
 *
 * The string the queued cart gets with stat(6).
 *
 * 큐에 올려 둔 카트가 stat(6)으로 받는 문자열.
 */
const char *pico8_console_pending_param(void) { return g_pending_param.c_str(); }

/*
 * pico8_console_load(path) — load a cartridge.
 *
 * Loads the cart and remembers its path and its name.  The path is set
 * before the cart runs, because the code of the cart may call run() or
 * load() itself, and those need to know which cart is the current one.
 *
 * A plain .p8 cart keeps its text, so that the command "save" can write
 * it back unchanged.
 *
 * 카트를 불러오고 경로와 이름을 기억한다. 경로는 카트가 돌기 전에 정해야
 * 한다. 카트의 코드가 스스로 run()이나 load()를 부를 수 있고, 그것들은
 * 현재 카트가 무엇인지 알아야 하기 때문이다.
 *
 * 평범한 .p8 카트는 텍스트를 보관한다. 그래야 "save" 명령이 그것을 그대로
 * 다시 쓸 수 있다.
 */
int pico8_console_load(const char *path)
{
    if (!path)
        return -1;

    /* Set the current cart identity *before* running the cart, because the
       cart's top-level code runs during pico8_load_cart() and may call
       run()/load()/extcmd("reset"), which need the current cart path. */
    std::string prev_path = g_cart_path;
    std::string prev_name = g_cart_name;
    g_cart_path = path;
    g_cart_name = basename_of(path);

    if (pico8_load_cart(path) != 0) {
        g_cart_path = prev_path;
        g_cart_name = prev_name;
        return -1;
    }

    g_has_cart = 1;
    g_source.clear();

    /* Keep the text of plain .p8 carts so "save" can round-trip it. */
    FILE *f = fopen(path, "rb");
    if (f) {
        unsigned char magic[4] = {0};
        size_t n = fread(magic, 1, 4, f);
        bool is_png = (n == 4 && magic[0] == 0x89 && magic[1] == 'P' &&
                       magic[2] == 'N' && magic[3] == 'G');
        if (!is_png) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 1024 * 1024) {
                g_source.resize((size_t)sz);
                if (fread(&g_source[0], 1, (size_t)sz, f) != (size_t)sz)
                    g_source.resize((size_t)sz);
            }
        }
        fclose(f);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Commands                                                           */
/* ------------------------------------------------------------------ */

static const char *kCommands[] = {
    "help", "dir", "ls", "cd", "mkdir", "load", "save", "import", "export",
    "info", "edit", "run", "stop", "resume", "reset", "reboot", "shutdown",
    "folder", "backup", "splore", "keyconfig", "config", "install_demos",
    "install_games", "login", "logout", "upload", "man", "cls", "clear",
    "exit", "quit",
    NULL
};

/* The two help functions call each other, so one of them needs a forward
 * declaration.  두 도움말 함수가 서로를 부르므로 둘 중 하나는 미리 선언해야
 * 한다. */
static void cmd_help_topic(const std::string &topic);

/*
 * cmd_help(topic) — the help of the shell.
 *
 * Without a topic it lists the commands of the shell, with one it goes
 * to that topic.
 *
 * 주제가 없으면 셸의 명령 목록을 보여주고, 있으면 그 주제로 간다.
 */
static void cmd_help(const std::string &topic)
{
    if (!topic.empty()) { cmd_help_topic(topic); return; }

    print_info("commands");
    print("load <filename>  save <filename>", COL_TEXT);
    print("run (or ctrl-r)  resume", COL_TEXT);
    print("shutdown         reboot", COL_TEXT);
    print("install_demos    ls", COL_TEXT);
    print("cd <dirname>     mkdir <dirname>", COL_TEXT);
    print("cd ..      to go up a directory", COL_TEXT);
    print("keyconfig  to choose buttons", COL_TEXT);
    print("splore     to explore cartridges", COL_TEXT);
    print("help <topic>", COL_TEXT);
    print("gfx data audio system math lua", COL_INFO);
    print("press esc to toggle editor view", COL_DIM);
}

/*
 * cmd_help_topic(topic) — the help of one subject.
 *
 * Lists the functions of one part of the API: gfx, audio, input, data,
 * math, system or lua.  The topic is not case sensitive, and an unknown
 * topic gives an error.
 *
 * API의 한 부분에 대한 함수들을 보여준다: gfx, audio, input, data, math,
 * system, lua. 주제는 대소문자를 가리지 않고, 모르는 주제는 오류다.
 */
static void cmd_help_topic(const std::string &topic)
{
    std::string t = lower(topic);

    if (t == "gfx") {
        print_info("gfx api functions");
        print("camera clip cls color pal palt fillp flip line rect rectfill", COL_TEXT);
        print("rrect rrectfill oval ovalfill circ circfill pget pset", COL_TEXT);
        print("print printh cursor map mget mset fget fset tline", COL_TEXT);
        print("spr sspr sget sset", COL_TEXT);
    } else if (t == "audio") {
        print_info("audio api functions");
        print("music sfx", COL_TEXT);
    } else if (t == "input") {
        print_info("input api functions");
        print("btn btnp", COL_TEXT);
    } else if (t == "data") {
        print_info("data api functions");
        print("add del deli count all foreach cartdata dget dset", COL_TEXT);
        print("memcpy memset peek poke peek2 poke2 peek4 poke4", COL_TEXT);
    } else if (t == "math") {
        print_info("math functions and operators");
        print("abs atan2 band bnot bor bxor btest ceil cos flr", COL_TEXT);
        print("lshr max mid min rnd rotl rotr sgn shl shr sin sqrt srand", COL_TEXT);
        print("+ - * / \\ % ^ .. == ~= < > <= >= and or not", COL_TEXT);
    } else if (t == "system") {
        print_info("system api functions");
        print("run stop reset yield time menuitem stat serial extcmd", COL_TEXT);
        print("cocreate coresume costatus", COL_TEXT);
        print_info("system commands");
        print("load save import export info resume folder shutdown reboot", COL_TEXT);
        print("splore backup login logout", COL_TEXT);
        print_info("callbacks");
        print("_init _draw _update _update60", COL_TEXT);
    } else if (t == "lua") {
        print_info("lua");
        print("strings in lua are immutable; they can be created and joined", COL_TEXT);
        print("together, but not modified.", COL_TEXT);
    } else if (t == "commands" || t == "system commands") {
        cmd_help("");
    } else {
        print_error("no help topic for: " + topic);
    }
}

/*
 * cmd_dir(arg) — the list of the cartridges.
 *
 * Lists the directory: the subfolders and the cartridges in it.  The
 * hidden files are left out, and the list is sorted.
 *
 * 디렉터리의 내용을 보여준다. 서브폴더와 카트만, 숨은 파일은 빼고, 정렬해서.
 */
static void cmd_dir(const std::string &arg)
{
    std::string dir = arg.empty() ? "." : arg;
    DIR *d = opendir(dir.c_str());
    if (!d) {
        print_error("dir: cannot open " + dir);
        return;
    }

    std::vector<std::string> names;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        std::string n = ent->d_name;
        struct stat st;
        std::string full = dir + "/" + n;
        bool is_dir = (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        if (is_dir) names.push_back(n + "/");
        else if (is_cart_name(n)) names.push_back(n);
    }
    closedir(d);

    std::sort(names.begin(), names.end());
    if (names.empty()) {
        print("no cartridges found in " + dir, COL_TEXT);
        return;
    }

    std::string joined;
    for (size_t i = 0; i < names.size(); i++) {
        if (!joined.empty()) joined += "  ";
        joined += names[i];
    }
    print_info(dir + ":");
    print(joined, COL_TEXT);
    print(std::to_string(names.size()) + " item(s)", COL_DIM);
}

/*
 * cmd_cd(arg) — change the folder.
 *
 * Changes the working directory of the program.  Without an argument it
 * prints the current one.
 *
 * 프로그램의 작업 디렉터리를 바꾼다. 인자가 없으면 현재 위치를 보여준다.
 */
static void cmd_cd(const std::string &arg)
{
    if (arg.empty()) {
        char buf[1024];
        if (getcwd(buf, sizeof(buf))) print(buf, COL_TEXT);
        return;
    }
    if (chdir(arg.c_str()) != 0) {
        print_error("cd: no such folder: " + arg);
        return;
    }
}

/*
 * cmd_load(arg) — load a cartridge.
 *
 * Loads the named cartridge, or loads the running one again when there is
 * no name.
 *
 * 이름을 준 카트를 불러오고, 이름이 없으면 지금 올라와 있는 카트를 다시
 * 불러온다.
 */
static void cmd_load(const std::string &arg)
{
    if (arg.empty()) {
        if (!g_has_cart) { print_error("no cartridge loaded"); return; }
        if (pico8_console_load(g_cart_path.c_str()) == 0)
            print_info("reloaded " + g_cart_name);
        else
            print_error("load: failed to reload " + g_cart_path);
        return;
    }

    std::string path = resolve_cart(arg);
    if (path.empty()) { print_error("load: file not found: " + arg); return; }

    if (pico8_console_load(path.c_str()) == 0) {
        std::string msg = "loaded " + g_cart_name;
        if (!g_source.empty()) msg += "  (" + std::to_string(g_source.size()) + " chars)";
        print_info(msg);
    } else {
        print_error("load: failed to load " + path);
    }
}

/*
 * cmd_save(arg) — save the cartridge.
 *
 * Writes the cart to a file.  A plain .p8 cart is written with the text
 * it was loaded from, so the header and the label survive the round trip.
 *
 * 카트를 파일로 쓴다. 평범한 .p8 카트는 불러왔을 때의 텍스트 그대로 쓰므로
 * 헤더와 라벨이 그대로 살아남는다.
 */
static void cmd_save(const std::string &arg)
{
    if (!g_has_cart) { print_error("save: no cartridge loaded"); return; }

    std::string out = arg.empty() ? g_cart_name : arg;
    if (!has_suffix(out, ".p8") && !has_suffix(out, ".png"))
        out += ".p8";

    /* Plain .p8 carts keep their original text (header, label and all). */
    if (!g_source.empty() && g_source.find("pico-8 cartridge") != std::string::npos) {
        FILE *f = fopen(out.c_str(), "wb");
        if (!f) { print_error("save: cannot write " + out); return; }
        fwrite(g_source.data(), 1, g_source.size(), f);
        if (g_source.back() != '\n') fputc('\n', f);
        fclose(f);
        print_info("saved " + out);
        return;
    }

    if (has_suffix(out, ".png")) {
        print_error("save: .p8.png output is not supported yet");
        return;
    }
    if (pico8_cart_save_p8(out.c_str()) != 0) {
        print_error("save: cannot write " + out);
        return;
    }
    print_info("saved " + out);
}

/*
 * cmd_run(arg) — run a cartridge.
 *
 * Loads the named cartridge (or keeps the one that is loaded) and asks the
 * host to start it.
 *
 * 이름을 준 카트를 불러오거나(없으면 지금 올라온 카트를 그대로 두고) 호스트에
 * 실행을 요청한다.
 */
static void cmd_run(const std::string &arg)
{
    if (!arg.empty()) {
        std::string path = resolve_cart(arg);
        if (path.empty()) { print_error("run: file not found: " + arg); return; }
        if (pico8_console_load(path.c_str()) != 0) {
            print_error("run: failed to load " + path);
            return;
        }
    }
    if (!g_has_cart) { print_error("no cartridge loaded"); return; }
    pico8_console_request_run = 1;
}

/*
 * cmd_stop() — stop the cartridge.
 *
 * There is no cartridge running in the shell, so this only says so.
 *
 * 셸에서는 카트가 돌고 있지 않으므로 그것만 알려 준다.
 */
static void cmd_stop(void)
{
    print("stop: no cartridge is running", COL_TEXT);
}

/*
 * cmd_resume() — continue the cartridge.
 *
 * Asks the host to start the cart that is loaded.
 *
 * 호스트에 지금 올라온 카트를 시작하라고 요청한다.
 */
static void cmd_resume(void)
{
    if (!g_has_cart) { print_error("no cartridge loaded"); return; }
    pico8_console_request_run = 1;
}

/*
 * cmd_reset() — the RESET of PICO-8.
 *
 * Puts the runtime state of the machine (the RAM from 0x5f00 to 0x5f7f)
 * back to its default.
 *
 * 기계의 런타임 상태(RAM의 0x5f00~0x5f7f)를 기본값으로 되돌린다.
 */
static void cmd_reset(void)
{
    /* PICO-8's RESET restores the runtime draw state (RAM 0x5f00..0x5f7f). */
    pico8_reset_draw_state();
}

/*
 * open_in_file_manager(dir) — show a folder in the file manager.
 *
 * Opens the folder with the file manager of the system: the Explorer on
 * Windows, the Finder on macOS and the desktop handler on Linux.
 *
 * 시스템의 파일 관리자로 폴더를 연다. Windows는 Explorer, macOS는 Finder,
 * Linux는 데스크톱 핸들러다.
 */
static void open_in_file_manager(const std::string &dir)
{
#if defined(_WIN32)
    std::string cmd = "explorer \"" + dir + "\"";
#elif defined(__APPLE__)
    std::string cmd = "open \"" + dir + "\"";
#else
    std::string cmd = "xdg-open \"" + dir + "\" >/dev/null 2>&1 &";
#endif
    system(cmd.c_str());
}

/*
 * pico8_console_open_folder() — show the current folder.
 *
 * The entry point of the command extcmd("folder").
 *
 * extcmd("folder") 명령의 진입점.
 */
void pico8_console_open_folder(void)
{
    open_in_file_manager(".");
}

/*
 * cmd_folder(arg) — the folders of PICO-8.
 *
 * Shows one of the folders that PICO-8 knows: the backups, the desktop,
 * the configuration or the BBS.  Without an argument it shows the current
 * directory.
 *
 * PICO-8이 아는 폴더를 보여준다: 백업, 데스크톱, 설정, BBS. 인자가 없으면 현재
 * 디렉터리를 보여준다.
 */
static void cmd_folder(const std::string &arg)
{
    std::string loc = lower(arg);
    char buf[1024];

    if (loc.empty()) {
        print(getcwd(buf, sizeof(buf)) ? buf : ".", COL_TEXT);
        open_in_file_manager(".");
    } else if (loc == "backups") {
        system("mkdir -p backup");
        print("backups", COL_TEXT);
        open_in_file_manager("backup");
    } else if (loc == "desktop") {
        const char *home = getenv("HOME");
        std::string desk = home ? std::string(home) + "/Desktop" : ".";
        print(desk, COL_TEXT);
        open_in_file_manager(desk);
    } else if (loc == "config" || loc == "bbs") {
        print(getcwd(buf, sizeof(buf)) ? buf : ".", COL_TEXT);
        open_in_file_manager(".");
    } else {
        print_error("usage: folder [backups config bbs desktop]");
    }
}

/*
 * cmd_menu(arg) — the entries of the pause menu.
 *
 * Lists the entries that the cart registered with menuitem(), or runs the
 * one with the given number.  PICO-8 shows these in its pause menu; here
 * the shell takes that place.
 *
 * 카트가 menuitem()으로 등록한 항목을 보여주거나, 번호를 주면 그 항목을
 * 실행한다. PICO-8은 일시정지 메뉴에 이것들을 보여주지만, 여기서는 셸이 그
 * 역할을 한다.
 */
static void cmd_menu(const std::string &arg)
{
    if (pico8_menuitem_count() == 0) {
        print("menu: no items registered", COL_TEXT);
        return;
    }
    if (arg.empty()) {
        for (int i = 1; i <= 5; i++) {
            const char *lbl = pico8_menuitem_label(i);
            if (lbl) print(std::to_string(i) + ": " + lbl, COL_TEXT);
        }
        return;
    }
    int idx = atoi(arg.c_str());
    if (idx < 1 || idx > 5 || !pico8_menuitem_has(idx)) {
        print_error("menu: no such item " + arg);
        return;
    }
    const char *lbl = pico8_menuitem_label(idx);
    print(std::string("menu: ") + (lbl ? lbl : "?"), COL_TEXT);
    pico8_vm_call_menuitem(idx, 0);
}

/*
 * cmd_mkdir(arg) — make a folder.
 *
 * Creates a directory.
 *
 * 디렉터리를 만든다.
 */
static void cmd_mkdir(const std::string &arg)
{
    if (arg.empty()) { print_error("usage: mkdir <name>"); return; }
    if (mkdir(arg.c_str(), 0777) != 0)
        print_error("mkdir: cannot create " + arg);
    else
        print_info("created " + arg + "/");
}

/*
 * cmd_backup() — a copy of the cartridge.
 *
 * Writes the source of the cart into the folder "backup", so that the
 * original is not lost when the cart is saved again.
 *
 * 카트의 소스를 "backup" 폴더에 써서, 카트를 다시 저장해도 원본이 사라지지
 * 않게 한다.
 */
static void cmd_backup(void)
{
    if (!g_has_cart) { print_error("backup: no cartridge loaded"); return; }
    if (g_source.empty()) { print_error("backup: no editable source"); return; }

    system("mkdir -p backup");
    std::string out = "backup/" + g_cart_name;
    if (!has_suffix(out, ".p8")) out += ".p8";

    FILE *f = fopen(out.c_str(), "wb");
    if (!f) { print_error("backup: cannot write " + out); return; }
    fwrite(g_source.data(), 1, g_source.size(), f);
    fclose(f);
    print_info("backed up " + out);
}

/*
 * cmd_config(key, val) — the settings.
 *
 * Without a key it lists the settings that PICO-8 has.  pico-c keeps them
 * for the session only and does not write a config.txt.
 *
 * 키가 없으면 PICO-8이 가진 설정들을 보여준다. pico-c는 이 설정을 그
 * 세션 동안에만 기억하고 config.txt를 쓰지 않는다.
 */
static void cmd_config(const std::string &key, const std::string &val)
{
    if (key.empty()) {
        print_info("config settings (session only)");
        print("volume      256   audio volume 0..256", COL_TEXT);
        print("gif_len     8     gif length in seconds", COL_TEXT);
        print("filter      on    hide mature carts in splore", COL_TEXT);
        print("show_tabs   off   draw tab characters in the editor", COL_TEXT);
        print("pico-c does not persist settings to config.txt", COL_DIM);
        return;
    }
    if (val.empty())
        print(key + " = (default)", COL_TEXT);
    else
        print("set " + key + " to " + val + " (session only)", COL_TEXT);
}

/*
 * cmd_info() — what is loaded.
 *
 * Shows the name of the cart, how many characters and lines its source
 * has, and which version of PICO-8 pico-c aims at.
 *
 * 불러와 있는 카트의 이름, 소스의 글자 수와 줄 수, 그리고 pico-c가 겨냥하는
 * PICO-8의 버전을 보여준다.
 */
static void cmd_info(void)
{
    if (!g_has_cart) {
        print_error("no cartridge loaded");
        return;
    }

    int chars = (int)g_source.size();
    int lines = 1;
    for (size_t i = 0; i < g_source.size(); i++)
        if (g_source[i] == '\n') lines++;

    char buf[128];
    print(g_cart_name + ":", COL_TEXT);
    snprintf(buf, sizeof buf, "chars:      %5d / 65536", chars);
    print(buf, COL_TEXT);
    snprintf(buf, sizeof buf, "lines:      %5d", lines);
    print(buf, COL_TEXT);
    print("pico-c 0.1 -- PICO-8 compatible (target v0.2.7)", COL_DIM);
}

/* ------------------------------------------------------------------ */
/*  Tab completion                                                     */
/* ------------------------------------------------------------------ */

/*
 * complete_files(prefix) — the cartridges that start with that.
 *
 * The names of the cartridges in the current directory that begin with the
 * given text, sorted.
 *
 * 현재 디렉터리에서 주어진 글자로 시작하는 카트의 이름. 정렬되어 있다.
 */
static std::vector<std::string> complete_files(const std::string &prefix)
{
    std::vector<std::string> out;
    DIR *d = opendir(".");
    if (!d) return out;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        std::string n = ent->d_name;
        if (!is_cart_name(n)) continue;
        if (n.compare(0, prefix.size(), prefix) == 0) out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

/*
 * complete_input() — the tab key.
 *
 * Finishes what has been typed.  Without a space in the line it completes
 * the command, with one it completes the file name.  A single match is
 * put into the line, several matches are listed.
 *
 * 입력한 것을 완성한다. 줄에 공백이 없으면 명령을, 있으면 파일 이름을
 * 완성한다. 후보가 하나면 줄에 채우고, 여러 개면 목록을 보여준다.
 */
static void complete_input(void)
{
    std::string in = g_input;
    size_t sp = in.find(' ');

    if (sp == std::string::npos) {
        std::vector<std::string> matches;
        for (int i = 0; kCommands[i]; i++)
            if (strncmp(kCommands[i], in.c_str(), in.size()) == 0)
                matches.push_back(kCommands[i]);

        if (matches.size() == 1) {
            g_input = matches[0] + " ";
        } else if (matches.size() > 1) {
            std::string all;
            for (size_t i = 0; i < matches.size(); i++) {
                if (i) all += "  ";
                all += matches[i];
            }
            add_line("> " + in, COL_INPUT);
            print(all, COL_TEXT);
        }
        return;
    }

    std::string cmd = in.substr(0, sp);
    std::string arg = in.substr(sp + 1);
    std::vector<std::string> matches = complete_files(arg);
    if (matches.size() == 1) {
        g_input = cmd + " " + matches[0];
    } else if (matches.size() > 1) {
        std::string all;
        for (size_t i = 0; i < matches.size(); i++) {
            if (i) all += "  ";
            all += matches[i];
        }
        add_line("> " + in, COL_INPUT);
        print(all, COL_TEXT);
    }
}

/* ------------------------------------------------------------------ */
/*  Input                                                              */
/* ------------------------------------------------------------------ */

/*
 * cmd_edit(arg) — edit the cartridge.
 *
 * Writes the cart to a file, opens it in the editor of the $EDITOR
 * environment variable (vi when there is none) and loads it again
 * afterwards.
 *
 * 카트를 파일로 써서 $EDITOR 환경 변수가 가리키는 편집기(없으면 vi)에서
 * 연 다음, 편집이 끝나면 다시 불러온다.
 */
static void cmd_edit(const std::string &arg)
{
    (void)arg;
    if (!g_has_cart) {
        print_error("edit: no cartridge loaded");
        return;
    }

    const char *editor = getenv("EDITOR");
    if (!editor || !editor[0]) editor = "vi";

    std::string path = g_cart_path;
    if (path.empty()) {
        path = g_cart_name.empty() ? "edit.p8" : g_cart_name;
        if (!has_suffix(path, ".p8") && !has_suffix(path, ".png"))
            path += ".p8";
    }

    if (!g_source.empty()) {
        FILE *f = fopen(path.c_str(), "wb");
        if (f) {
            fwrite(g_source.data(), 1, g_source.size(), f);
            fclose(f);
        }
    } else if (pico8_cart_save_p8(path.c_str()) != 0) {
        print_error("edit: cannot write " + path);
        return;
    }

    std::string cmd = editor;
    cmd += " \"";
    cmd += path;
    cmd += "\"";
    int rc = system(cmd.c_str());
    if (rc != 0) {
        print_error("edit: editor returned non-zero exit code");
        return;
    }

    if (pico8_console_load(path.c_str()) == 0) {
        print_info("reloaded " + g_cart_name);
    } else {
        print_error("edit: failed to reload " + path);
    }
}

/*
 * cmd_keyconfig() — the buttons.
 *
 * PICO-8 lets the player choose the buttons.  pico-c does not keep that
 * choice, so this only says so.
 *
 * PICO-8은 플레이어가 버튼을 고르게 한다. pico-c는 그 선택을 저장하지
 * 않으므로 그것만 알려 준다.
 */
static void cmd_keyconfig(void)
{
    print_info("keyconfig");
    print("button mapping is not persisted in this build", COL_TEXT);
    print("use POKE(0x5f2d,...) and host key bindings", COL_DIM);
}

/*
 * cmd_splore() — the cart browser.
 *
 * PICO-8 can browse the carts of the internet.  pico-c has no network, so
 * this says so and points at the local carts.
 *
 * PICO-8은 인터넷의 카트를 볼 수 있다. pico-c에는 네트워크가 없으므로
 * 그것을 말하고 로컬 카트를 가리킨다.
 */
static void cmd_splore(void)
{
    print("splore: online cart browser is not available in pico-c", COL_TEXT);
    print("use 'load' with local .p8/.p8.png files", COL_DIM);
}

/*
 * cmd_import_shell(arg) — import a cart.
 *
 * PICO-8 can import a cart from the BBS.  pico-c cannot, so this only
 * says so.
 *
 * PICO-8은 BBS에서 카트를 가져올 수 있다. pico-c는 할 수 없으므로
 * 그것을 말할 뿐이다.
 */
static void cmd_import_shell(const std::string &arg)
{
    if (arg.empty()) {
        print_error("usage: import <filename>");
        return;
    }
    print("import: " + arg + " is not supported in pico-c", COL_TEXT);
    print("use a .p8 or .p8.png cart instead", COL_DIM);
}

/*
 * cmd_export_shell(arg) — export a cart.
 *
 * PICO-8 can send a cart to the BBS.  pico-c cannot, so this only says
 * so and points at the command "save".
 *
 * PICO-8은 카트를 BBS로 보낼 수 있다. pico-c는 할 수 없으므로 그것을
 * 말하고 "save" 명령을 가리킨다.
 */
static void cmd_export_shell(const std::string &arg)
{
    if (arg.empty()) {
        print_error("usage: export <filename>");
        return;
    }
    print("export: " + arg + " is not supported in pico-c", COL_TEXT);
    print("use 'save <name>' to write a .p8 or .p8.png cart", COL_DIM);
}

/*
 * tokenize(s) — the words of a command line.
 *
 * Splits the line at the spaces.  A word in quotes is one word, whatever
 * it holds.
 *
 * 공백을 기준으로 줄을 나눈다. 따옴표로 감싼 것은 안에 뭐가 있든 한 단어로
 * 본다.
 */
static std::vector<std::string> tokenize(const std::string &s)
{
    std::vector<std::string> t;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace((unsigned char)s[i])) i++;
        if (i >= s.size()) break;
        std::string tok;
        if (s[i] == '"') {
            i++;
            while (i < s.size() && s[i] != '"') tok.push_back(s[i++]);
            if (i < s.size()) i++;
        } else {
            while (i < s.size() && !isspace((unsigned char)s[i])) tok.push_back(s[i++]);
        }
        t.push_back(tok);
    }
    return t;
}

/*
 * execute(raw) — run one command line.
 *
 * Runs a line of the shell.  The known commands do their work, and
 * anything else is Lua that is evaluated in the console exactly like
 * PICO-8 does it.  A line that starts with a question mark is a
 * shorthand for print(...).
 *
 * 셸의 한 줄을 실행한다. 알려진 명령은 그 일을 하고, 그 외의 것은 PICO-8과
 * 똑같이 셸에서 Lua로 계산된다. 물음표로 시작하는 줄은 print(...)의 줄여
 * 쓰기다.
 */
static void execute(const std::string &raw)
{
    std::vector<std::string> tok = tokenize(raw);
    if (tok.empty()) return;

    std::string cmd = lower(tok[0]);
    std::string arg = (tok.size() > 1) ? tok[1] : "";
    std::string arg2 = (tok.size() > 2) ? tok[2] : "";

    if (cmd == "help" || cmd == "man")          cmd_help(arg);
    else if (cmd == "dir" || cmd == "ls")       cmd_dir(arg);
    else if (cmd == "cd")                       cmd_cd(arg);
    else if (cmd == "mkdir")                    cmd_mkdir(arg);
    else if (cmd == "load")                     cmd_load(arg);
    else if (cmd == "save")                     cmd_save(arg);
    else if (cmd == "run")                      cmd_run(arg);
    else if (cmd == "stop")                     cmd_stop();
    else if (cmd == "resume")                   cmd_resume();
    else if (cmd == "reset")                    cmd_reset();
    else if (cmd == "reboot")                   pico8_console_request_reboot = 1;
    else if (cmd == "folder")                   cmd_folder(arg);
    else if (cmd == "menu")                     cmd_menu(arg);
    else if (cmd == "backup")                   cmd_backup();
    else if (cmd == "info")                     cmd_info();
    else if (cmd == "config")                   cmd_config(arg, arg2);
    else if (cmd == "cls" || cmd == "clear")    g_lines.clear();
    else if (cmd == "shutdown" || cmd == "exit" || cmd == "quit")
                                                pico8_console_request_quit = 1;
    else if (cmd == "edit")                     cmd_edit(arg);
    else if (cmd == "keyconfig")                cmd_keyconfig();
    else if (cmd == "splore")                    cmd_splore();
    else if (cmd == "import")                    cmd_import_shell(arg);
    else if (cmd == "export")                    cmd_export_shell(arg);
    else if (cmd == "install_demos" || cmd == "install_games")
        print(cmd + ": demo cartridges are not bundled with pico-c", COL_TEXT);
    else if (cmd == "login" || cmd == "logout" || cmd == "upload")
        print(cmd + ": online features are not available in pico-c", COL_TEXT);
    else {
        /* Anything else is Lua, evaluated in the console exactly like PICO-8.
           "?expr" is shorthand for print(expr). */
        std::string code = raw;
        if (!code.empty() && code[0] == '?')
            code = "print(" + code.substr(1) + ")";

        g_capturing = 1;
        pico8_vm_eval(code.c_str());
        g_capturing = 0;
    }
}

/*
 * pico8_console_exec(line) — a command from a cart.
 *
 * Runs a shell command on behalf of a cart, which is how PICO-8 gives its
 * shell commands to the cartridges (mkdir("x"), cd("dir"), dir(), help()).
 *
 * 카트를 대신해서 셸 명령을 실행한다. PICO-8이 셸 명령을 카트에 주는 방법이다
 * (mkdir("x"), cd("dir"), dir(), help()).
 */
void pico8_console_exec(const char *line)
{
    execute(line ? line : "");
}

/*
 * submit() — the enter key.
 *
 * Takes what was typed, puts it into the scrollback and into the history,
 * and runs it.  The same line is not put into the history twice.
 *
 * 입력한 것을 스크롤백과 히스토리에 넣고 실행한다. 같은 줄은 히스토리에
 * 두 번 들어가지 않는다.
 */
static void submit(void)
{
    std::string raw = trim(g_input);
    g_input.clear();
    g_hist_pos = -1;
    if (raw.empty()) return;

    add_line("> " + raw, COL_INPUT);
    if (g_history.empty() || g_history.back() != raw)
        g_history.push_back(raw);
    if (g_history.size() > 64)
        g_history.erase(g_history.begin());

    execute(raw);
}

/*
 * hist_prev() — the up arrow.
 *
 * Goes back through the history.  The first time the line that was being
 * typed is kept aside, so that it comes back when the arrow is pressed
 * down again.
 *
 * 히스토리에서 뒤로 간다. 처음에는 입력하던 줄을 따로 빼 두므로, 아래
 * 화살표를 누르면 그 줄이 돌아온다.
 */
static void hist_prev(void)
{
    if (g_history.empty()) return;
    if (g_hist_pos == -1) {
        g_draft = g_input;
        g_hist_pos = (int)g_history.size() - 1;
    } else if (g_hist_pos > 0) {
        g_hist_pos--;
    }
    g_input = g_history[g_hist_pos];
}

/*
 * hist_next() — the down arrow.
 *
 * Goes forward through the history.  Past the newest entry the line that
 * was being typed comes back.
 *
 * 히스토리에서 앞으로 간다. 가장 최근 항목을 넘으면 빼 두었던 줄이 돌아온다.
 */
static void hist_next(void)
{
    if (g_hist_pos == -1) return;
    if (g_hist_pos < (int)g_history.size() - 1) {
        g_hist_pos++;
        g_input = g_history[g_hist_pos];
    } else {
        g_hist_pos = -1;
        g_input = g_draft;
    }
}

/*
 * pico8_console_shell_text(utf8) — the text that was typed.
 *
 * Puts the typed characters into the input line.  Only the plain ASCII
 * letters and digits are taken; the other characters belong to the
 * cartridge's own font and are not part of the shell.
 *
 * 입력한 문자를 입력 줄에 넣는다. 평범한 ASCII 문자만 받는다. 나머지 문자는
 * 카트 고유의 글씨라 셸의 것이 아니다.
 */
void pico8_console_shell_text(const char *utf8)
{
    if (!utf8) return;
    for (const unsigned char *p = (const unsigned char *)utf8; *p; ++p) {
        unsigned char c = *p;
        if (c >= 32 && c < 127)
            g_input.push_back((char)c);
    }
}

/*
 * pico8_console_shell_key(key) — a key that was pressed.
 *
 * Acts on the keys the shell uses: enter runs the line, backspace deletes
 * a character, escape clears the line (or leaves the shell when the line
 * is already empty), the arrow keys walk through the history and the tab
 * key completes.
 *
 * 셸이 쓰는 키를 처리한다. 엔터는 줄을 실행하고, 백스페이스는 한 글자를
 * 지우고, esc는 줄을 비우며(비어 있으면 셸을 나간다), 방향키는 히스토리를
 * 오가고, 탭은 완성한다.
 */
void pico8_console_shell_key(int key)
{
    switch (key) {
        case '\r':
        case '\n':
            submit();
            break;

        case 8:      /* backspace */
        case 127:    /* delete    */
            if (!g_input.empty()) g_input.pop_back();
            break;

        case 27:     /* escape: clear line, or quit when already empty */
            if (!g_input.empty()) g_input.clear();
            else pico8_console_request_quit = 1;
            break;

        case PICO8_KEY_UP:   hist_prev(); break;
        case PICO8_KEY_DOWN: hist_next(); break;
        case PICO8_KEY_TAB:  complete_input(); break;

        default:
            break;
    }
}
