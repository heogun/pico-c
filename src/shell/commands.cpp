/*
 * commands.cpp — the commands of the shell and the table that runs them.
 *
 * Every command of PICO-8's shell that pico-c answers is here, together
 * with the table that maps the word the player types to the function that
 * does the work.  A line that matches no command is Lua, and is evaluated
 * in the console exactly like PICO-8 does it.
 *
 * pico-c가 대답하는 PICO-8 셸의 모든 명령이 여기에 있고, 플레이어가 치는
 * 단어를 실제 일을 하는 함수로 연결하는 표도 여기에 있다. 어느 명령에도
 * 해당하지 않는 줄은 Lua이며, PICO-8과 똑같이 셸에서 계산된다.
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
#include "stb_image.h"

/* ------------------------------------------------------------------ */
/*  Commands                                                          */
/* ------------------------------------------------------------------ */

/* The two help functions call each other, so one of them needs a forward
 * declaration.  두 도움말 함수가 서로를 부르므로 둘 중 하나는 미리 선언해야
 * 한다. */
static void cmd_help_topic(const std::string &topic);

/*
 * cmd_help(arg, arg2) — the help of the shell.
 *
 * Without a topic it lists the commands of the shell, with one it goes
 * to that topic.
 *
 * 주제가 없으면 셸의 명령 목록을 보여주고, 있으면 그 주제로 간다.
 */
static void cmd_help(const std::string &arg, const std::string &)
{
    if (!arg.empty()) { cmd_help_topic(arg); return; }

    shell_print_info("commands");
    shell_print("load <filename>  save <filename>", COL_TEXT);
    shell_print("run (or ctrl-r)  resume", COL_TEXT);
    shell_print("shutdown         reboot", COL_TEXT);
    shell_print("install_demos    ls", COL_TEXT);
    shell_print("cd <dirname>     mkdir <dirname>", COL_TEXT);
    shell_print("cd ..      to go up a directory", COL_TEXT);
    shell_print("keyconfig  to choose buttons", COL_TEXT);
    shell_print("splore     to explore cartridges", COL_TEXT);
    shell_print("help <topic>", COL_TEXT);
    shell_print("gfx data audio system math lua", COL_INFO);
    shell_print("press esc to toggle editor view", COL_DIM);
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
    std::string t = util_lower(topic);

    if (t == "gfx") {
        shell_print_info("gfx api functions");
        shell_print("camera clip cls color pal palt fillp flip line rect rectfill", COL_TEXT);
        shell_print("rrect rrectfill oval ovalfill circ circfill pget pset", COL_TEXT);
        shell_print("print printh cursor map mget mset fget fset tline", COL_TEXT);
        shell_print("spr sspr sget sset", COL_TEXT);
    } else if (t == "audio") {
        shell_print_info("audio api functions");
        shell_print("music sfx", COL_TEXT);
    } else if (t == "input") {
        shell_print_info("input api functions");
        shell_print("btn btnp", COL_TEXT);
    } else if (t == "data") {
        shell_print_info("data api functions");
        shell_print("add del deli count all foreach cartdata dget dset", COL_TEXT);
        shell_print("memcpy memset peek poke peek2 poke2 peek4 poke4", COL_TEXT);
    } else if (t == "math") {
        shell_print_info("math functions and operators");
        shell_print("abs atan2 band bnot bor bxor btest ceil cos flr", COL_TEXT);
        shell_print("lshr max mid min rnd rotl rotr sgn shl shr sin sqrt srand", COL_TEXT);
        shell_print("+ - * / \\ % ^ .. == ~= < > <= >= and or not", COL_TEXT);
    } else if (t == "system") {
        shell_print_info("system api functions");
        shell_print("run stop reset yield time menuitem stat serial extcmd", COL_TEXT);
        shell_print("cocreate coresume costatus", COL_TEXT);
        shell_print_info("system commands");
        shell_print("load save import export info resume folder shutdown reboot", COL_TEXT);
        shell_print("splore backup login logout", COL_TEXT);
        shell_print_info("callbacks");
        shell_print("_init _draw _update _update60", COL_TEXT);
    } else if (t == "lua") {
        shell_print_info("lua");
        shell_print("strings in lua are immutable; they can be created and joined", COL_TEXT);
        shell_print("together, but not modified.", COL_TEXT);
    } else if (t == "commands" || t == "system commands") {
        cmd_help("", "");
    } else {
        shell_print_error("no help topic for: " + topic);
    }
}

/*
 * cmd_dir(arg, arg2) — the list of the cartridges.
 *
 * Lists the directory: the subfolders and the cartridges in it.  The
 * hidden files are left out, and the list is sorted.
 *
 * 디렉터리의 내용을 보여준다. 서브폴더와 카트만, 숨은 파일은 빼고, 정렬해서.
 */
static void cmd_dir(const std::string &arg, const std::string &)
{
    std::string dir = arg.empty() ? "." : arg;
    DIR *d = opendir(dir.c_str());
    if (!d) {
        shell_print_error("dir: cannot open " + dir);
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
        else if (shell_is_cart_name(n)) names.push_back(n);
    }
    closedir(d);

    std::sort(names.begin(), names.end());
    if (names.empty()) {
        shell_print("no cartridges found in " + dir, COL_TEXT);
        return;
    }

    std::string joined;
    for (size_t i = 0; i < names.size(); i++) {
        if (!joined.empty()) joined += "  ";
        joined += names[i];
    }
    shell_print_info(dir + ":");
    shell_print(joined, COL_TEXT);
    shell_print(std::to_string(names.size()) + " item(s)", COL_DIM);
}

/*
 * cmd_cd(arg, arg2) — change the folder.
 *
 * Changes the working directory of the program.  Without an argument it
 * prints the current one.
 *
 * 프로그램의 작업 디렉터리를 바꾼다. 인자가 없으면 현재 위치를 보여준다.
 */
static void cmd_cd(const std::string &arg, const std::string &)
{
    if (arg.empty()) {
        char buf[1024];
        if (getcwd(buf, sizeof(buf))) shell_print(buf, COL_TEXT);
        return;
    }
    if (chdir(arg.c_str()) != 0) {
        shell_print_error("cd: no such folder: " + arg);
        return;
    }
}

/*
 * cmd_load(arg, arg2) — load a cartridge.
 *
 * Loads the named cartridge, or loads the running one again when there is
 * no name.
 *
 * 이름을 준 카트를 불러오고, 이름이 없으면 지금 올라와 있는 카트를 다시
 * 불러온다.
 */
static void cmd_load(const std::string &arg, const std::string &)
{
    if (arg.empty()) {
        if (!g_has_cart) { shell_print_error("no cartridge loaded"); return; }
        if (pico8_console_load(g_cart_path.c_str()) == 0)
            shell_print_info("reloaded " + g_cart_name);
        else
            shell_print_error("load: failed to reload " + g_cart_path);
        return;
    }

    std::string path = shell_resolve_cart(arg);
    if (path.empty()) { shell_print_error("load: file not found: " + arg); return; }

    if (pico8_console_load(path.c_str()) == 0) {
        std::string msg = "loaded " + g_cart_name;
        if (!g_source.empty()) msg += "  (" + std::to_string(g_source.size()) + " chars)";
        shell_print_info(msg);
    } else {
        shell_print_error("load: failed to load " + path);
    }
}

/*
 * cmd_save(arg, arg2) — save the cartridge.
 *
 * Writes the cart to a file.  A plain .p8 cart is written with the text
 * it was loaded from, so the header and the label survive the round trip.
 *
 * 카트를 파일로 쓴다. 평범한 .p8 카트는 불러왔을 때의 텍스트 그대로 쓰므로
 * 헤더와 라벨이 그대로 살아남는다.
 */
static void cmd_save(const std::string &arg, const std::string &)
{
    if (!g_has_cart) { shell_print_error("save: no cartridge loaded"); return; }

    std::string out = arg.empty() ? g_cart_name : arg;
    if (!util_has_suffix(out, ".p8") && !util_has_suffix(out, ".png"))
        out += ".p8";

    /* Plain .p8 carts keep their original text (header, label and all). */
    if (!g_source.empty() && g_source.find("pico-8 cartridge") != std::string::npos) {
        FILE *f = fopen(out.c_str(), "wb");
        if (!f) { shell_print_error("save: cannot write " + out); return; }
        std::string text = pico8_cart_splice_label(g_source);
        fwrite(text.data(), 1, text.size(), f);
        if (text.back() != '\n') fputc('\n', f);
        fclose(f);
        shell_print_info("saved " + out);
        return;
    }

    if (util_has_suffix(out, ".png")) {
        shell_print_error("save: .p8.png output is not supported yet");
        return;
    }
    if (pico8_cart_save_p8(out.c_str()) != 0) {
        shell_print_error("save: cannot write " + out);
        return;
    }
    shell_print_info("saved " + out);
}

/*
 * cmd_run(arg, arg2) — run a cartridge.
 *
 * Loads the named cartridge (or keeps the one that is loaded) and asks the
 * host to start it.
 *
 * 이름을 준 카트를 불러오거나(없으면 지금 올라온 카트를 그대로 두고) 호스트에
 * 실행을 요청한다.
 */
static void cmd_run(const std::string &arg, const std::string &)
{
    if (!arg.empty()) {
        std::string path = shell_resolve_cart(arg);
        if (path.empty()) { shell_print_error("run: file not found: " + arg); return; }
        if (pico8_console_load(path.c_str()) != 0) {
            shell_print_error("run: failed to load " + path);
            return;
        }
    }
    if (!g_has_cart) { shell_print_error("no cartridge loaded"); return; }
    pico8_console_request_run = 1;
}

/*
 * cmd_stop(arg, arg2) — stop the cartridge.
 *
 * There is no cartridge running in the shell, so this only says so.
 *
 * 셸에서는 카트가 돌고 있지 않으므로 그것만 알려 준다.
 */
static void cmd_stop(const std::string &, const std::string &)
{
    shell_print("stop: no cartridge is running", COL_TEXT);
}

/*
 * cmd_resume(arg, arg2) — continue the cartridge.
 *
 * Asks the host to start the cart that is loaded.
 *
 * 호스트에 지금 올라온 카트를 시작하라고 요청한다.
 */
static void cmd_resume(const std::string &, const std::string &)
{
    if (!g_has_cart) { shell_print_error("no cartridge loaded"); return; }
    pico8_console_request_run = 1;
}

/*
 * cmd_reset(arg, arg2) — the RESET of PICO-8.
 *
 * Puts the runtime state of the machine (the RAM from 0x5f00 to 0x5f7f)
 * back to its default.
 *
 * 기계의 런타임 상태(RAM의 0x5f00~0x5f7f)를 기본값으로 되돌린다.
 */
static void cmd_reset(const std::string &, const std::string &)
{
    /* PICO-8's RESET restores the runtime draw state (RAM 0x5f00..0x5f7f). */
    pico8_reset_draw_state();
}

/*
 * shell_open_in_file_manager(dir) — show a folder in the file manager.
 *
 * Opens the folder with the file manager of the system: the Explorer on
 * Windows, the Finder on macOS and the desktop handler on Linux.
 *
 * 시스템의 파일 관리자로 폴더를 연다. Windows는 Explorer, macOS는 Finder,
 * Linux는 데스크톱 핸들러다.
 */
void shell_open_in_file_manager(const std::string &dir)
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
    shell_open_in_file_manager(".");
}

/*
 * cmd_folder(arg, arg2) — the folders of PICO-8.
 *
 * Shows one of the folders that PICO-8 knows: the backups, the desktop,
 * the configuration or the BBS.  Without an argument it shows the current
 * directory.
 *
 * PICO-8이 아는 폴더를 보여준다: 백업, 데스크톱, 설정, BBS. 인자가 없으면 현재
 * 디렉터리를 보여준다.
 */
static void cmd_folder(const std::string &arg, const std::string &)
{
    std::string loc = util_lower(arg);
    char buf[1024];

    if (loc.empty()) {
        shell_print(getcwd(buf, sizeof(buf)) ? buf : ".", COL_TEXT);
        shell_open_in_file_manager(".");
    } else if (loc == "backups") {
        system("mkdir -p backup");
        shell_print("backups", COL_TEXT);
        shell_open_in_file_manager("backup");
    } else if (loc == "desktop") {
        const char *home = getenv("HOME");
        std::string desk = home ? std::string(home) + "/Desktop" : ".";
        shell_print(desk, COL_TEXT);
        shell_open_in_file_manager(desk);
    } else if (loc == "config" || loc == "bbs") {
        shell_print(getcwd(buf, sizeof(buf)) ? buf : ".", COL_TEXT);
        shell_open_in_file_manager(".");
    } else {
        shell_print_error("usage: folder [backups config bbs desktop]");
    }
}

/*
 * cmd_menu(arg, arg2) — the entries of the pause menu.
 *
 * Lists the entries that the cart registered with menuitem(), or runs the
 * one with the given number.  PICO-8 shows these in its pause menu; here
 * the shell takes that place.
 *
 * 카트가 menuitem()으로 등록한 항목을 보여주거나, 번호를 주면 그 항목을
 * 실행한다. PICO-8은 일시정지 메뉴에 이것들을 보여주지만, 여기서는 셸이 그
 * 역할을 한다.
 */
static void cmd_menu(const std::string &arg, const std::string &)
{
    if (pico8_menuitem_count() == 0) {
        shell_print("menu: no items registered", COL_TEXT);
        return;
    }
    if (arg.empty()) {
        for (int i = 1; i <= 5; i++) {
            const char *lbl = pico8_menuitem_label(i);
            if (lbl) shell_print(std::to_string(i) + ": " + lbl, COL_TEXT);
        }
        return;
    }
    int idx = atoi(arg.c_str());
    if (idx < 1 || idx > 5 || !pico8_menuitem_has(idx)) {
        shell_print_error("menu: no such item " + arg);
        return;
    }
    const char *lbl = pico8_menuitem_label(idx);
    shell_print(std::string("menu: ") + (lbl ? lbl : "?"), COL_TEXT);
    pico8_vm_call_menuitem(idx, 0);
}

/*
 * cmd_mkdir(arg, arg2) — make a folder.
 *
 * Creates a directory.
 *
 * 디렉터리를 만든다.
 */
static void cmd_mkdir(const std::string &arg, const std::string &)
{
    if (arg.empty()) { shell_print_error("usage: mkdir <name>"); return; }
    if (mkdir(arg.c_str(), 0777) != 0)
        shell_print_error("mkdir: cannot create " + arg);
    else
        shell_print_info("created " + arg + "/");
}

/*
 * cmd_backup(arg, arg2) — a copy of the cartridge.
 *
 * Writes the source of the cart into the folder "backup", so that the
 * original is not lost when the cart is saved again.
 *
 * 카트의 소스를 "backup" 폴더에 써서, 카트를 다시 저장해도 원본이 사라지지
 * 않게 한다.
 */
static void cmd_backup(const std::string &, const std::string &)
{
    if (!g_has_cart) { shell_print_error("backup: no cartridge loaded"); return; }
    if (g_source.empty()) { shell_print_error("backup: no editable source"); return; }

    system("mkdir -p backup");
    std::string out = "backup/" + g_cart_name;
    if (!util_has_suffix(out, ".p8")) out += ".p8";

    FILE *f = fopen(out.c_str(), "wb");
    if (!f) { shell_print_error("backup: cannot write " + out); return; }
    fwrite(g_source.data(), 1, g_source.size(), f);
    fclose(f);
    shell_print_info("backed up " + out);
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
        shell_print_info("config settings (session only)");
        shell_print("volume      256   audio volume 0..256", COL_TEXT);
        shell_print("gif_len     8     gif length in seconds", COL_TEXT);
        shell_print("filter      on    hide mature carts in splore", COL_TEXT);
        shell_print("show_tabs   off   draw tab characters in the editor", COL_TEXT);
        shell_print("pico-c does not persist settings to config.txt", COL_DIM);
        return;
    }
    if (val.empty())
        shell_print(key + " = (default)", COL_TEXT);
    else
        shell_print("set " + key + " to " + val + " (session only)", COL_TEXT);
}

/*
 * cmd_info(arg, arg2) — what is loaded.
 *
 * Shows the name of the cart, how many characters and lines its source
 * has, and which version of PICO-8 pico-c aims at.
 *
 * 불러와 있는 카트의 이름, 소스의 글자 수와 줄 수, 그리고 pico-c가 겨냥하는
 * PICO-8의 버전을 보여준다.
 */
static void cmd_info(const std::string &, const std::string &)
{
    if (!g_has_cart) {
        shell_print_error("no cartridge loaded");
        return;
    }

    int chars = (int)g_source.size();
    int lines = 1;
    for (size_t i = 0; i < g_source.size(); i++)
        if (g_source[i] == '\n') lines++;

    char buf[128];
    shell_print(g_cart_name + ":", COL_TEXT);
    snprintf(buf, sizeof buf, "chars:      %5d / 65536", chars);
    shell_print(buf, COL_TEXT);
    snprintf(buf, sizeof buf, "lines:      %5d", lines);
    shell_print(buf, COL_TEXT);
    shell_print("pico-c 0.1 -- PICO-8 compatible (target v0.2.7)", COL_DIM);
}

/*
 * cmd_reboot(arg, arg2) — restart the machine.
 *
 * Asks the host to start the machine again, with the boot splash.
 *
 * 호스트에 부팅 화면과 함께 기계를 다시 시작하라고 요청한다.
 */
static void cmd_reboot(const std::string &, const std::string &)
{
    pico8_console_request_reboot = 1;
}

/*
 * cmd_clear(arg, arg2) — empty the screen.
 *
 * Throws the scrollback away, so that the console starts from a clean
 * sheet.
 *
 * 스크롤백을 버린다. 콘솔이 깨끗한 종이에서 다시 시작한다.
 */
static void cmd_clear(const std::string &, const std::string &)
{
    g_lines.clear();
}

/*
 * cmd_quit(arg, arg2) — leave the program.
 *
 * Asks the host to close the window and end the program.
 *
 * 호스트에 창을 닫고 프로그램을 끝내라고 요청한다.
 */
static void cmd_quit(const std::string &, const std::string &)
{
    pico8_console_request_quit = 1;
}

/*
 * cmd_install_demos(arg, arg2) — the demo cartridges.
 *
 * PICO-8 can download its demo cartridges.  pico-c does not ship them, so
 * this only says so.
 *
 * PICO-8은 데모 카트를 내려받는다. pico-c에는 그것들이 없으므로 그것만
 * 말한다.
 */
static void cmd_install_demos(const std::string &, const std::string &)
{
    shell_print("install_demos: demo cartridges are not bundled with pico-c",
                COL_TEXT);
}

/*
 * cmd_install_games(arg, arg2) — the games of the BBS.
 *
 * PICO-8 can download games from the BBS.  pico-c has no network, so this
 * only says so.
 *
 * PICO-8은 BBS에서 게임을 내려받는다. pico-c에는 네트워크가 없으므로
 * 그것만 말한다.
 */
static void cmd_install_games(const std::string &, const std::string &)
{
    shell_print("install_games: demo cartridges are not bundled with pico-c",
                COL_TEXT);
}

/*
 * cmd_login(arg, arg2) — the BBS account.
 *
 * PICO-8 can log into the BBS.  pico-c has no network, so this only says
 * so.
 *
 * PICO-8은 BBS에 로그인한다. pico-c에는 네트워크가 없으므로 그것만
 * 말한다.
 */
static void cmd_login(const std::string &, const std::string &)
{
    shell_print("login: online features are not available in pico-c", COL_TEXT);
}

/*
 * cmd_logout(arg, arg2) — leave the BBS account.
 *
 * PICO-8 can log out of the BBS.  pico-c has no network, so this only
 * says so.
 *
 * PICO-8은 BBS에서 로그아웃한다. pico-c에는 네트워크가 없으므로 그것만
 * 말한다.
 */
static void cmd_logout(const std::string &, const std::string &)
{
    shell_print("logout: online features are not available in pico-c", COL_TEXT);
}

/*
 * cmd_upload(arg, arg2) — send a cart to the BBS.
 *
 * PICO-8 can upload a cart to the BBS.  pico-c has no network, so this
 * only says so.
 *
 * PICO-8은 카트를 BBS에 올린다. pico-c에는 네트워크가 없으므로 그것만
 * 말한다.
 */
static void cmd_upload(const std::string &, const std::string &)
{
    shell_print("upload: online features are not available in pico-c", COL_TEXT);
}

/*
 * cmd_edit(arg, arg2) — edit the cartridge.
 *
 * Writes the cart to a file, opens it in the editor of the $EDITOR
 * environment variable (vi when there is none) and loads it again
 * afterwards.
 *
 * 카트를 파일로 써서 $EDITOR 환경 변수가 가리키는 편집기(없으면 vi)에서
 * 연 다음, 편집이 끝나면 다시 불러온다.
 */
static void cmd_edit(const std::string &, const std::string &)
{
    if (!g_has_cart) {
        shell_print_error("edit: no cartridge loaded");
        return;
    }

    const char *editor = getenv("EDITOR");
    if (!editor || !editor[0]) editor = "vi";

    std::string path = g_cart_path;
    if (path.empty()) {
        path = g_cart_name.empty() ? "edit.p8" : g_cart_name;
        if (!util_has_suffix(path, ".p8") && !util_has_suffix(path, ".png"))
            path += ".p8";
    }

    if (!g_source.empty()) {
        FILE *f = fopen(path.c_str(), "wb");
        if (f) {
            fwrite(g_source.data(), 1, g_source.size(), f);
            fclose(f);
        }
    } else if (pico8_cart_save_p8(path.c_str()) != 0) {
        shell_print_error("edit: cannot write " + path);
        return;
    }

    std::string cmd = editor;
    cmd += " \"";
    cmd += path;
    cmd += "\"";
    int rc = system(cmd.c_str());
    if (rc != 0) {
        shell_print_error("edit: editor returned non-zero exit code");
        return;
    }

    if (pico8_console_load(path.c_str()) == 0) {
        shell_print_info("reloaded " + g_cart_name);
    } else {
        shell_print_error("edit: failed to reload " + path);
    }
}

/*
 * cmd_keyconfig(arg, arg2) — the buttons.
 *
 * PICO-8 lets the player choose the buttons.  pico-c does not keep that
 * choice, so this only says so.
 *
 * PICO-8은 플레이어가 버튼을 고르게 한다. pico-c는 그 선택을 저장하지
 * 않으므로 그것만 알려 준다.
 */
static void cmd_keyconfig(const std::string &, const std::string &)
{
    shell_print_info("keyconfig");
    shell_print("button mapping is not persisted in this build", COL_TEXT);
    shell_print("use POKE(0x5f2d,...) and host key bindings", COL_DIM);
}

/*
 * cmd_splore(arg, arg2) — the cart browser.
 *
 * PICO-8 can browse the carts of the internet.  pico-c has no network, so
 * this says so and points at the local carts.
 *
 * PICO-8은 인터넷의 카트를 볼 수 있다. pico-c에는 네트워크가 없으므로
 * 그것을 말하고 로컬 카트를 가리킨다.
 */
static void cmd_splore(const std::string &, const std::string &)
{
    shell_print("splore: online cart browser is not available in pico-c", COL_TEXT);
    shell_print("use 'load' with local .p8/.p8.png files", COL_DIM);
}

/*
 * sheet_to_pixels(pixels) — the spritesheet as a 128x128 RGB picture.
 *
 * The sprites live at 0x0000..0x1fff, two pixels a byte, the left one in
 * the low nibble.  The colours are the 16 of the plain palette.
 *
 * 스프라이트는 0x0000..0x1fff에 바이트당 두 픽셀(왼쪽이 하위 니블)로 있다.
 * 색은 기본 팔레트의 16색이다.
 */
static void sheet_to_pixels(std::vector<uint32_t> &pixels)
{
    pixels.assign(128 * 128, 0);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            uint8_t b = pico8_ram[y * 64 + x / 2];
            pixels[y * 128 + x] = pico8_palette[(x & 1) ? b >> 4 : b & 15];
        }
}

/*
 * nearest_colour(r, g, b) — the palette colour closest to an RGB colour.
 *
 * RGB 색에 가장 가까운 팔레트 색.
 */
static int nearest_colour(int r, int g, int b)
{
    int best = 0, best_d = 1 << 30;
    for (int c = 0; c < 16; c++) {
        int dr = r - (int)((pico8_palette[c] >> 16) & 0xff);
        int dg = g - (int)((pico8_palette[c] >> 8) & 0xff);
        int db = b - (int)(pico8_palette[c] & 0xff);
        int d = dr * dr + dg * dg + db * db;
        if (d < best_d) { best_d = d; best = c; }
    }
    return best;
}

/*
 * cmd_import_shell(arg, arg2) — import a file into the cart.
 *
 * "import foo.png" reads a picture into the spritesheet of the cart (the
 * RAM and the cart ROM, so reload() keeps it): the top left 128x128 pixels,
 * every pixel turned into the closest of the 16 colours (the same mapping
 * as PICO-8: a 128x128 gradient gives the same sheet).  Other files (a
 * cart from the BBS, ...) are not supported.
 *
 * "import foo.png"는 그림을 카트의 스프라이트 시트로 읽는다(RAM과 카트 ROM,
 * 그래서 reload() 뒤에도 남는다): 왼쪽 위 128x128 픽셀을, 픽셀마다 16색 중
 * 가장 가까운 색으로 바꿔서(PICO-8과 같은 대응: 128x128 그라데이션이 같은 시트가
 * 된다). 다른 파일(BBS의 카트 등)은 지원하지 않는다.
 */
static void cmd_import_shell(const std::string &arg, const std::string &)
{
    if (arg.empty()) {
        shell_print_error("usage: import <filename>");
        return;
    }
    if (util_has_suffix(arg, ".png")) {
        int w = 0, h = 0, comp = 0;
        unsigned char *img = stbi_load(arg.c_str(), &w, &h, &comp, 4);
        if (!img) { shell_print_error("import: cannot read " + arg); return; }
        for (int y = 0; y < 128 && y < h; y++)
            for (int x = 0; x < 128 && x < w; x++) {
                const unsigned char *p = img + ((size_t)y * w + x) * 4;
                int c = nearest_colour(p[0], p[1], p[2]);
                uint8_t &b = pico8_ram[y * 64 + x / 2];
                b = (x & 1) ? (uint8_t)((b & 0x0f) | (c << 4)) : (uint8_t)((b & 0xf0) | c);
            }
        stbi_image_free(img);
        /* PICO-8 changes the cart itself, so reload() keeps the picture
           (measured).  PICO-8은 카트 자체를 바꾸므로 reload() 뒤에도 그림이
           남는다(실측). */
        pico8_cart_rom_store(0, 0x2000);
        shell_print_info("imported " + arg);
        return;
    }
    shell_print("import: " + arg + " is not supported in pico-c", COL_TEXT);
    shell_print("use a .p8 or .p8.png cart instead", COL_DIM);
}

/*
 * cmd_export_shell(arg, arg2) — export a cart.
 *
 * "export foo%d.wav" writes every sound effect to its own file (foo0.wav ..
 * foo63.wav), like PICO-8 does, and "export foo.wav" writes sound effect 0
 * (the one PICO-8's editor has selected after a load).  "export foo.png"
 * writes the spritesheet as a 128x128 picture.  The other formats (html,
 * bin) are not available, and the command says so and points at "save".
 *
 * "export foo%d.wav"는 PICO-8처럼 효과음마다 파일 하나씩(foo0.wav ..
 * foo63.wav) 쓰고, "export foo.wav"는 효과음 0(로드 직후 PICO-8 편집기가
 * 고르고 있는 것)을 쓴다. "export foo.png"는 스프라이트 시트를 128x128 그림으로
 * 쓴다. 다른 형식(html, bin)은 지원하지 않으므로 그렇게 말하고 "save"를
 * 가리킨다.
 */
static void cmd_export_shell(const std::string &arg, const std::string &)
{
    if (arg.empty()) {
        shell_print_error("usage: export <filename>");
        return;
    }
    if (util_has_suffix(arg, ".wav")) {
        size_t pct = arg.find("%d");
        int first = 0, last = pct == std::string::npos ? 0 : 63;
        for (int n = first; n <= last; n++) {
            std::string out = arg;
            if (pct != std::string::npos) out.replace(pct, 2, std::to_string(n));
            if (pico8_audio_export_sfx_wav(n, out.c_str()) != 0) {
                shell_print_error("export: cannot write " + out);
                return;
            }
        }
        shell_print_info("exported " + arg);
        return;
    }
    if (util_has_suffix(arg, ".png") && !util_has_suffix(arg, ".p8.png")) {
        std::vector<uint32_t> pixels;
        sheet_to_pixels(pixels);
        if (pico8_png_write(arg.c_str(), pixels.data(), 128, 128) != 0) {
            shell_print_error("export: cannot write " + arg);
            return;
        }
        shell_print_info("exported " + arg);
        return;
    }
    shell_print("export: " + arg + " is not supported in pico-c", COL_TEXT);
    shell_print("use 'save <name>' to write a .p8 or .p8.png cart", COL_DIM);
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
 * The commands the shell knows.
 *
 * The order is the order in which the tab key lists them.  The flag says
 * whether the tab key offers the command at all: "menu" is not offered,
 * because PICO-8 does not offer it either.
 *
 * 셸이 아는 명령들. 순서가 탭 키가 보여주는 순서다. 플래그는 탭 키가 그
 * 명령을 아예 제시하는지를 말한다: "menu"는 제시하지 않는다. PICO-8도
 * 제시하지 않기 때문이다.
 */
const ShellCommand kShellCommands[] = {
    { "help",          cmd_help,          true  },
    { "dir",           cmd_dir,           true  },
    { "ls",            cmd_dir,           true  },
    { "cd",            cmd_cd,            true  },
    { "mkdir",         cmd_mkdir,         true  },
    { "load",          cmd_load,          true  },
    { "save",          cmd_save,          true  },
    { "import",        cmd_import_shell,  true  },
    { "export",        cmd_export_shell,  true  },
    { "info",          cmd_info,          true  },
    { "edit",          cmd_edit,          true  },
    { "run",           cmd_run,           true  },
    { "stop",          cmd_stop,          true  },
    { "resume",        cmd_resume,        true  },
    { "reset",         cmd_reset,         true  },
    { "reboot",        cmd_reboot,        true  },
    { "shutdown",      cmd_quit,          true  },
    { "folder",        cmd_folder,        true  },
    { "backup",        cmd_backup,        true  },
    { "splore",        cmd_splore,        true  },
    { "keyconfig",     cmd_keyconfig,     true  },
    { "config",        cmd_config,        true  },
    { "install_demos", cmd_install_demos, true  },
    { "install_games", cmd_install_games, true  },
    { "login",         cmd_login,         true  },
    { "logout",        cmd_logout,        true  },
    { "upload",        cmd_upload,        true  },
    { "man",           cmd_help,          true  },
    { "cls",           cmd_clear,         true  },
    { "clear",         cmd_clear,         true  },
    { "exit",          cmd_quit,          true  },
    { "quit",          cmd_quit,          true  },
    { "menu",          cmd_menu,          false },
    { NULL,            NULL,              false },
};

/*
 * shell_execute(raw) — run one command line.
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
void shell_execute(const std::string &raw)
{
    std::vector<std::string> tok = tokenize(raw);
    if (tok.empty()) return;

    std::string cmd = util_lower(tok[0]);
    std::string arg = (tok.size() > 1) ? tok[1] : "";
    std::string arg2 = (tok.size() > 2) ? tok[2] : "";

    for (int i = 0; kShellCommands[i].name; i++) {
        if (cmd == kShellCommands[i].name) {
            kShellCommands[i].run(arg, arg2);
            return;
        }
    }

    /* Anything else is Lua, evaluated in the console exactly like PICO-8.
       "?expr" is shorthand for print(expr). */
    std::string code = raw;
    if (!code.empty() && code[0] == '?')
        code = "print(" + code.substr(1) + ")";

    g_capturing = 1;
    pico8_vm_eval(code.c_str());
    g_capturing = 0;
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
    shell_execute(line ? line : "");
}
