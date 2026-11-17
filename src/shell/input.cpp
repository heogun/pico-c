/*
 * input.cpp — the keyboard of the shell: typing, history and completion.
 *
 * The tab key finishes what has been typed, the arrow keys walk through
 * the history and the enter key runs the line.
 *
 * 셸의 키보드: 입력, 히스토리, 완성. 탭 키는 입력한 것을 완성하고, 방향키는
 * 히스토리를 오가며, 엔터 키는 줄을 실행한다.
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
/*  Tab completion                                                    */
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
        if (!shell_is_cart_name(n)) continue;
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
        for (int i = 0; kShellCommands[i].name; i++) {
            if (!kShellCommands[i].offer) continue;
            if (strncmp(kShellCommands[i].name, in.c_str(), in.size()) == 0)
                matches.push_back(kShellCommands[i].name);
        }

        if (matches.size() == 1) {
            g_input = matches[0] + " ";
        } else if (matches.size() > 1) {
            std::string all;
            for (size_t i = 0; i < matches.size(); i++) {
                if (i) all += "  ";
                all += matches[i];
            }
            shell_add_line("> " + in, COL_INPUT);
            shell_print(all, COL_TEXT);
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
        shell_add_line("> " + in, COL_INPUT);
        shell_print(all, COL_TEXT);
    }
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
    std::string raw = util_trim(g_input);
    g_input.clear();
    g_hist_pos = -1;
    if (raw.empty()) return;

    shell_add_line("> " + raw, COL_INPUT);
    if (g_history.empty() || g_history.back() != raw)
        g_history.push_back(raw);
    if (g_history.size() > 64)
        g_history.erase(g_history.begin());

    shell_execute(raw);
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
