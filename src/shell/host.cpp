/*
 * host.cpp — the cartridge files and the requests to the host.
 *
 * The shell looks for cartridges on disk, loads them and remembers which
 * one is running.  A cart can also ask the host to do something (load
 * another cart, run, reboot), and those requests are collected here.
 *
 * 셸이 디스크에서 카트를 찾고, 그것을 불러오고, 지금 뭐가 돌고 있는지
 * 기억한다. 카트가 호스트에게 무언가를 시킬 수도 있고(다른 카트 불러오기,
 * 실행, 재부팅), 그런 요청은 여기에 모인다.
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
#include "util.h"
#include "shell_internal.h"

/* ------------------------------------------------------------------ */
/*  Host requests raised by commands (handled by main.cpp)            */
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
/*  Cartridge files                                                    */
/* ------------------------------------------------------------------ */

/*
 * shell_is_cart_name(n) — is that the name of a cartridge?
 *
 * True for a .p8 file and for a .p8.png file.
 *
 * .p8 파일과 .p8.png 파일이면 참.
 */
bool shell_is_cart_name(const std::string &n)
{
    return util_has_suffix(n, ".p8") || util_has_suffix(n, ".p8.png");
}

/*
 * shell_resolve_cart(name) — find the cartridge with that name.
 *
 * Looks for the file and, when the name has no extension, tries the three
 * names that PICO-8 would try: .p8, .p8.png and .png.  An empty result
 * means that there is no such file.
 *
 * 파일을 찾는다. 확장자가 없으면 PICO-8이 시도하는 세 이름(.p8, .p8.png, .png)을
 * 순서대로 시도한다. 결과가 비어 있으면 그런 파일이 없다는 뜻이다.
 */
std::string shell_resolve_cart(const std::string &name)
{
    if (name.empty()) return "";

    std::vector<std::string> cands;
    cands.push_back(name);
    if (!util_has_suffix(name, ".p8") && !util_has_suffix(name, ".png")) {
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
        std::string resolved = shell_resolve_cart(req);
        if (resolved.empty()) {
            std::string dir = util_path_dir(g_cart_path);
            if (!dir.empty()) resolved = shell_resolve_cart(dir + req);
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

    /* Keep the text of plain .p8 carts so "save" can round-trip it. */
    std::string text;
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
                text.resize((size_t)sz);
                text.resize(fread(&text[0], 1, (size_t)sz, f));
            }
        }
        fclose(f);
    }

    /* Set the whole identity of the cart (path, name, text) *before* it
       runs: its top-level code runs inside pico8_load_cart() and may call
       run(), load(), extcmd("reset") or a shell command such as backup(),
       which all need to know the current cart.  A failed load puts the
       previous cart back.
       카트의 정체(경로, 이름, 텍스트)를 실행 *전에* 모두 정한다. 최상위 코드는
       pico8_load_cart() 안에서 돌고, run(), load(), extcmd("reset"), backup()
       같은 셸 명령을 부를 수 있는데, 모두 현재 카트를 알아야 한다. 불러오기에
       실패하면 이전 카트로 되돌린다. */
    std::string prev_path = g_cart_path;
    std::string prev_name = g_cart_name;
    std::string prev_source = g_source;
    int prev_has = g_has_cart;
    g_cart_path = path;
    g_cart_name = util_path_base(path);
    g_source = text;
    g_has_cart = 1;

    if (pico8_load_cart(path) != 0) {
        g_cart_path = prev_path;
        g_cart_name = prev_name;
        g_source = prev_source;
        g_has_cart = prev_has;
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Captures (screenshots and audio recordings)
 * ------------------------------------------------------------------ */

static std::string g_capture_name;   /* set by extcmd("set_filename") */
static std::string g_capture_path;   /* the last path handed out      */

/*
 * pico8_console_set_capture_name(name) — the name of the next captures.
 *
 * An empty name goes back to the numbered names of the cart.
 *
 * 다음 캡처들의 이름. 빈 이름이면 카트 이름에 번호를 붙이는 방식으로 돌아간다.
 */
void pico8_console_set_capture_name(const char *name)
{
    g_capture_name = name ? name : "";
}

/*
 * pico8_console_capture_path(ext) — the file the next capture goes to.
 *
 * See pico8_console.h.  `ext` is ".png" or ".wav".
 *
 * pico8_console.h 참고. `ext`는 ".png" 또는 ".wav"다.
 */
const char *pico8_console_capture_path(const char *ext)
{
    const char *env  = getenv("PICO_C_DESKTOP");
    const char *home = getenv("HOME");
    std::string dir = (env && env[0]) ? std::string(env)
                    : std::string(home ? home : ".") + "/Desktop";

    if (!g_capture_name.empty()) {
        g_capture_path = dir + "/" + g_capture_name + ext;
        return g_capture_path.c_str();
    }

    std::string base = g_cart_path.empty() ? "pico8" : util_path_base(g_cart_path);
    if (util_has_suffix(base, ".png")) base.resize(base.size() - 4);
    if (util_has_suffix(base, ".p8"))  base.resize(base.size() - 3);
    for (int n = 0; ; n++) {
        g_capture_path = dir + "/" + base + "_" + std::to_string(n) + ext;
        FILE *f = fopen(g_capture_path.c_str(), "rb");
        if (!f) break;
        fclose(f);
    }
    return g_capture_path.c_str();
}
