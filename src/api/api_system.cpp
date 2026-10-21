/*
 * api_system.cpp — the system part of the PICO-8 API.
 *
 * stat() is the big one: it is the only way a cart can ask the machine
 * about itself, so it covers the memory and CPU usage, the clipboard, the
 * command line, the framerate, the devkit mouse and keyboard, the audio
 * state and the clock.  The rest of the file has the shell commands the
 * cart can trigger (extcmd, reboot, info, folder, ls) and the pause menu
 * items registered with menuitem().
 *
 * 이 파일에서 가장 큰 함수가 stat()이다. 카트가 자신에 대해 물을 수 있는 유일한
 * 방법이라 메모리/CPU, 클립보드, 커맨드라인, 프레임레이트, 데브킷 마우스/키보드,
 * 오디오 상태, 시계를 모두 담당한다. 나머지는 카트가 호출할 수 있는 셸 명령
 * (extcmd, reboot, info, folder, ls)과 menuitem()으로 등록한 일시정지 메뉴다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "api_internal.h"

static int pico8_time(lua_State *L)
{
    lua_pushnumber(L, fix32_from_int(pico8_frame_count));
    return 1;
}

static int pico8_extcmd(lua_State *L)
{
    const char *cmd = luaL_checkstring(L, 1);
    if (strcmp(cmd, "reset") == 0) {
        /* Restart the cart from the top, keeping the current param string. */
        pico8_console_queue_load(NULL, NULL, NULL);
        pico8_vm_halted = 1;
    } else if (strcmp(cmd, "shutdown") == 0) {
        pico8_console_request_quit = 1;
    } else if (strcmp(cmd, "pause") == 0) {
        /* No modal pause menu; fall back to the shell console. */
        pico8_console_request_pause = 1;
    } else if (strcmp(cmd, "folder") == 0) {
        pico8_console_open_folder();
    } else if (strcmp(cmd, "go_back") == 0) {
        const char *crumb = pico8_console_breadcrumb();
        if (crumb && crumb[0]) {
            pico8_console_queue_load(crumb, NULL, NULL);
            pico8_vm_halted = 1;
        }
    }
    /* "label", "screen", "video", "rec", "audio_rec", "audio_end" and
       "set_filename" are host/editor features that have no equivalent here. */
    return 0;
}

#include <dirent.h>
#include <sys/stat.h>

static int pico8_reboot(lua_State *L)
{
    (void)L;
    pico8_vm_halted = 1;
    return 0;
}

static int pico8_info(lua_State *L)
{
    (void)L;
    size_t len = 0;
    const char *src = pico8_cart_source(&len);
    int chars = (int)len;
    int lines = 1;
    for (size_t i = 0; i < len; i++)
        if (src[i] == '\n') lines++;

    char buf[192];
    snprintf(buf, sizeof buf, "%d chars, %d lines (pico-c 0.2.7-compat)", chars, lines);
    pico8_console_print(buf);
    return 0;
}

static int pico8_folder(lua_State *L)
{
    (void)L;
    pico8_console_open_folder();
    return 0;
}

static int pico8_ls(lua_State *L)
{
    const char *dir = ".";
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
        dir = lua_tostring(L, 1);

    lua_newtable(L);
    DIR *d = opendir(dir);
    if (!d) return 1;

    int idx = 1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        std::string name = ent->d_name;
        bool is_dir = false;
        struct stat st;
        std::string full = std::string(dir) + "/" + name;
        if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) is_dir = true;
        bool is_p8 = (name.rfind(".p8", name.size()-3) == name.size()-3) ||
                     (name.rfind(".p8.png", name.size()-7) == name.size()-7);
        if (!is_dir && !is_p8) continue;
        lua_pushstring(L, (name + (is_dir ? "/" : "")).c_str());
        lua_rawseti(L, -2, idx++);
    }
    closedir(d);
    return 1;
}

/* Pause-menu items registered by menuitem(). Index 0 is unused so the arrays
   can be indexed directly by PICO-8's 1..5 item numbers. */
static int         g_menu_ref[6]   = {LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF};
static int         g_menu_mask[6]  = {0,0,0,0,0,0};
static std::string g_menu_label[6];

static int pico8_menuitem(lua_State *L)
{
    int top = lua_gettop(L);
    /* A nil index refers to the currently executing item (used from within a
       callback). We do not track that, so ignore it. */
    if (top < 1 || lua_isnil(L, 1) || !lua_isnumber(L, 1)) return 0;

    int raw = fix32_to_int(luaL_checknumber(L, 1));
    int index = raw & 0xff;              /* 1..5 */
    int mask  = (raw >> 8) & 0xff;       /* button filter in bits 0xff00 */

    if (index < 1 || index > 5) return 0;

    const char *label = (top >= 2 && !lua_isnil(L, 2)) ? luaL_checkstring(L, 2) : NULL;
    int has_cb = (top >= 3 && lua_isfunction(L, 3));

    if (!label && !has_cb) {             /* remove the item */
        if (g_menu_ref[index] != LUA_NOREF && g_menu_ref[index] != LUA_REFNIL)
            luaL_unref(L, LUA_REGISTRYINDEX, g_menu_ref[index]);
        g_menu_ref[index] = LUA_NOREF;
        g_menu_label[index].clear();
        g_menu_mask[index] = 0;
        return 0;
    }

    if (label) g_menu_label[index] = label;
    g_menu_mask[index] = mask;

    if (has_cb) {
        if (g_menu_ref[index] != LUA_NOREF && g_menu_ref[index] != LUA_REFNIL)
            luaL_unref(L, LUA_REGISTRYINDEX, g_menu_ref[index]);
        lua_pushvalue(L, 3);
        g_menu_ref[index] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

void pico8_menuitem_clear(void)
{
    for (int i = 0; i < 6; i++) {
        g_menu_ref[i]  = LUA_NOREF;
        g_menu_mask[i] = 0;
        g_menu_label[i].clear();
    }
}

int pico8_menuitem_has(int index)
{
    if (index < 1 || index > 5) return 0;
    return !g_menu_label[index].empty() || g_menu_ref[index] != LUA_NOREF;
}

int pico8_menuitem_count(void)
{
    int n = 0;
    for (int i = 1; i <= 5; i++) if (pico8_menuitem_has(i)) n++;
    return n;
}

const char *pico8_menuitem_label(int index)
{
    if (index < 1 || index > 5 || g_menu_label[index].empty()) return NULL;
    return g_menu_label[index].c_str();
}

int pico8_menuitem_btnmask(int index)
{
    if (index < 1 || index > 5) return 0;
    return g_menu_mask[index];
}

int pico8_menuitem_invoke(lua_State *L, int index, int buttons)
{
    if (!L || index < 1 || index > 5) return 0;
    if (g_menu_ref[index] == LUA_NOREF || g_menu_ref[index] == LUA_REFNIL) return 0;

    lua_rawgeti(L, LUA_REGISTRYINDEX, g_menu_ref[index]);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }

    pico8_vm_push_traceback(L);
    lua_insert(L, -2);
    lua_pushnumber(L, fix32_from_int(buttons));
    if (lua_pcall(L, 1, 1, -2) != LUA_OK) {
        fprintf(stderr, "Error in menuitem: %s\n", lua_tostring(L, -1));
        lua_pop(L, 2);
        return 0;
    }
    int keep = lua_toboolean(L, -1);
    lua_pop(L, 2);
    return keep;
}

static void pico8_stat_clipboard(lua_State *L)
{
    char buf[1024];
    buf[0] = '\0';
#if defined(__APPLE__)
    const char *cmd = "pbpaste 2>/dev/null";
#elif defined(_WIN32)
    const char *cmd = "powershell -NoProfile -Command Get-Clipboard 2>NUL";
#else
    const char *cmd = "xclip -selection clipboard -o 2>/dev/null";
#endif
    FILE *f = popen(cmd, "r");
    if (f) {
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        /* pbpaste / Get-Clipboard append a trailing newline. */
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
    }
    lua_pushstring(L, buf);
}

/* stat(80..85) = UTC time, stat(90..95) = local time; both are
   year, month, day, hour, minute, second. */
static void pico8_stat_time(lua_State *L, int idx)
{
    time_t now = time(NULL);
    struct tm tmv;
    int ok;
    if (idx >= 90) ok = (localtime_r(&now, &tmv) != NULL);
    else           ok = (gmtime_r(&now, &tmv) != NULL);
    if (!ok) {
        lua_pushnumber(L, fix32_from_int(0));
        return;
    }
    int v = 0;
    switch (idx % 10) {
        case 0: v = tmv.tm_year + 1900; break;
        case 1: v = tmv.tm_mon + 1;     break;   /* 1..12 */
        case 2: v = tmv.tm_mday;        break;
        case 3: v = tmv.tm_hour;        break;
        case 4: v = tmv.tm_min;         break;
        case 5: v = tmv.tm_sec;         break;
    }
    lua_pushnumber(L, fix32_from_int(v));
}

static int pico8_stat(lua_State *L)
{
    int idx = fix32_to_int(luaL_optnumber(L, 1, 0));
    int devkit = pico8_ram[0x5f2d];

    switch (idx) {
        case 0: {
            int kb = (int)(lua_gc(L, LUA_GCCOUNT, 0));
            lua_pushnumber(L, fix32_from_int(kb * 1024));
            return 1;
        }
        case 1:   /* cpu load (1.0 == 100%) */
            lua_pushnumber(L, fix32_from_int(0));
            return 1;
        case 4:   /* clipboard contents */
            pico8_stat_clipboard(L);
            return 1;
        case 6:   /* parameter string */
            if (pico8_cmdline && pico8_cmdline[0]) {
                lua_pushstring(L, pico8_cmdline);
            } else {
                lua_pushstring(L, "");
            }
            return 1;
        case 7:   /* current framerate */
            lua_pushnumber(L, fix32_from_int(pico8_vm_has_function("_update60") ? 60 : 30));
            return 1;

        /* --- devkit mouse + keyboard (POKE(0x5f2d, flags)) --- */
        case 30:  /* keypress available */
            lua_pushboolean(L, (devkit & 1) && pico8_keyboard_available());
            return 1;
        case 31: { /* character returned by keyboard */
            char s[2] = { 0, 0 };
            if (devkit & 1) {
                int ch = pico8_keyboard_read();
                if (ch >= 0) s[0] = (char)ch;
            }
            lua_pushstring(L, s);
            return 1;
        }
        case 32:  /* mouse x */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_x : 0));
            return 1;
        case 33:  /* mouse y */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_y : 0));
            return 1;
        case 34:  /* mouse buttons (bitfield) */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_buttons : 0));
            return 1;
        case 36:  /* mouse wheel event */
            lua_pushnumber(L, fix32_from_int((devkit & 1) ? pico8_mouse_wheel : 0));
            return 1;
        case 38:  /* relative x movement (requires pointer lock, flag 0x4) */
            lua_pushnumber(L, fix32_from_int(((devkit & 5) == 5) ? pico8_mouse_dx : 0));
            return 1;
        case 39:  /* relative y movement */
            lua_pushnumber(L, fix32_from_int(((devkit & 5) == 5) ? pico8_mouse_dy : 0));
            return 1;

        /* --- audio --- */
        case 46: case 47: case 48: case 49:
            lua_pushnumber(L, fix32_from_int(pico8_audio_sfx_id(idx - 46)));
            return 1;
        case 50: case 51: case 52: case 53:
            lua_pushnumber(L, fix32_from_int(pico8_audio_note_num(idx - 50)));
            return 1;
        case 54: /* currently playing music pattern index */
            lua_pushnumber(L, fix32_from_int(pico8_audio_music_pattern()));
            return 1;
        case 55: /* total patterns played */
            lua_pushnumber(L, fix32_from_int(pico8_audio_music_count()));
            return 1;
        case 56: /* ticks played on the current pattern */
            lua_pushnumber(L, fix32_from_int(pico8_audio_music_ticks()));
            return 1;
        case 57:
            lua_pushboolean(L, pico8_audio_music_playing());
            return 1;

        /* --- clock --- */
        case 80: case 81: case 82: case 83: case 84: case 85:
        case 90: case 91: case 92: case 93: case 94: case 95:
            pico8_stat_time(L, idx);
            return 1;

        case 100: /* breadcrumb label */
            if (pico8_console_breadcrumb()) {
                lua_pushstring(L, pico8_console_breadcrumb());
            } else {
                lua_pushnil(L);
            }
            return 1;

        default:
            lua_pushnumber(L, 0);
            return 1;
    }
}


/* ------------------------------------------------------------------ *
 *  Registration
 * ------------------------------------------------------------------ */

static const luaL_Reg system_api[] = {
    { "time",    pico8_time    }, { "t",      pico8_time    },
    { "stat",    pico8_stat    },
    { "extcmd",  pico8_extcmd  }, { "reboot", pico8_reboot },
    { "info",    pico8_info    }, { "folder", pico8_folder },
    { "ls",      pico8_ls      }, { "menuitem", pico8_menuitem },
    { NULL, NULL }
};

void pico8_api_register_system(lua_State *L)
{
    lua_pushglobaltable(L);
    for (const luaL_Reg *lib = system_api; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
