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

static std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static std::string lower(const std::string &s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

static bool has_suffix(const std::string &s, const char *suf)
{
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

static std::string basename_of(const std::string &p)
{
    size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

static void put_text(int x, int y, const std::string &s, uint8_t color)
{
    int cx = x;
    for (size_t i = 0; i < s.size(); i++) {
        pico8_draw_char(cx, y, (uint8_t)s[i], color);
        cx += CH_W;
    }
}

/* Split a (possibly multi-line) string into wrapped console lines. */
static void add_line(const std::string &text, uint8_t color)
{
    if (text.empty()) {
        g_lines.push_back(ConsoleLine{color, ""});
        return;
    }
    for (size_t i = 0; i < text.size(); i += COLS)
        g_lines.push_back(ConsoleLine{color, text.substr(i, COLS)});
}

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

static void print_error(const std::string &s) { print(s, COL_ERROR); }
static void print_info(const std::string &s)  { print(s, COL_INFO); }

/* ------------------------------------------------------------------ */
/*  Console (Lua) evaluation output                                    */
/* ------------------------------------------------------------------ */

static int g_capturing = 0;

int pico8_console_capturing(void) { return g_capturing; }

void pico8_console_emit(const char *text, int color)
{
    if (text) print(text, (uint8_t)color);
}

/* ------------------------------------------------------------------ */
/*  Boot splash                                                        */
/* ------------------------------------------------------------------ */

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

void pico8_console_shell_reset(void)
{
    g_input.clear();
    g_hist_pos = -1;
}

int pico8_console_boot_finished(uint32_t elapsed_ms)
{
    return elapsed_ms >= PICO8_BOOT_MS;
}

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

void pico8_console_print(const char *line)
{
    if (line) print(line, COL_TEXT);
}

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

static bool is_cart_name(const std::string &n)
{
    return has_suffix(n, ".p8") || has_suffix(n, ".p8.png");
}

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

static std::string dir_of(const std::string &p)
{
    size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : p.substr(0, pos + 1);
}

/* Queue a cartridge to load (and run) after the current frame. A null/empty
   filename means "reload the current cartridge". Returns 0 when queued. */
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

const char *pico8_console_breadcrumb(void)
{
    return g_pending_breadcrumb.empty() ? NULL : g_pending_breadcrumb.c_str();
}

const char *pico8_console_pending_path(void)  { return g_pending_path.c_str(); }
const char *pico8_console_pending_param(void) { return g_pending_param.c_str(); }

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

static void cmd_help_topic(const std::string &topic);

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

static void cmd_stop(void)
{
    print("stop: no cartridge is running", COL_TEXT);
}

static void cmd_resume(void)
{
    if (!g_has_cart) { print_error("no cartridge loaded"); return; }
    pico8_console_request_run = 1;
}

static void cmd_reset(void)
{
    /* PICO-8's RESET restores the runtime draw state (RAM 0x5f00..0x5f7f). */
    pico8_reset_draw_state();
}

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

/* Public entry used by extcmd("folder"): open the current working directory. */
void pico8_console_open_folder(void)
{
    open_in_file_manager(".");
}

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

/* List/invoke items registered with menuitem(). PICO-8 shows these in its
   pause menu; here the shell acts as the pause menu. */
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

static void cmd_mkdir(const std::string &arg)
{
    if (arg.empty()) { print_error("usage: mkdir <name>"); return; }
    if (mkdir(arg.c_str(), 0777) != 0)
        print_error("mkdir: cannot create " + arg);    else
        print_info("created " + arg + "/");
}

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

static void cmd_keyconfig(void)
{
    print_info("keyconfig");
    print("button mapping is not persisted in this build", COL_TEXT);
    print("use POKE(0x5f2d,...) and host key bindings", COL_DIM);
}

static void cmd_splore(void)
{
    print("splore: online cart browser is not available in pico-c", COL_TEXT);
    print("use 'load' with local .p8/.p8.png files", COL_DIM);
}

static void cmd_import_shell(const std::string &arg)
{
    if (arg.empty()) {
        print_error("usage: import <filename>");
        return;
    }
    print("import: " + arg + " is not supported in pico-c", COL_TEXT);
    print("use a .p8 or .p8.png cart instead", COL_DIM);
}

static void cmd_export_shell(const std::string &arg)
{
    if (arg.empty()) {
        print_error("usage: export <filename>");
        return;
    }
    print("export: " + arg + " is not supported in pico-c", COL_TEXT);
    print("use 'save <name>' to write a .p8 or .p8.png cart", COL_DIM);
}

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

/* Run a shell command line on behalf of a Lua function (e.g. mkdir("x"),
   cd("dir"), dir(), help()). This is how PICO-8 exposes its shell commands to
   cartridges. */
void pico8_console_exec(const char *line)
{
    execute(line ? line : "");
}

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

void pico8_console_shell_text(const char *utf8)
{
    if (!utf8) return;
    for (const unsigned char *p = (const unsigned char *)utf8; *p; ++p) {
        unsigned char c = *p;
        if (c >= 32 && c < 127)
            g_input.push_back((char)c);
    }
}

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
