/*
 * util.cpp — the little helpers that more than one part of pico-c needs.
 *
 * The implementations of the functions declared in util.h.  They are short
 * and they do not depend on anything but the standard library.
 *
 * util.h에 선언된 함수들의 구현. 모두 짧고 표준 라이브러리 외에는 아무것도
 * 쓰지 않는다.
 */

#include <ctype.h>
#include <string.h>

#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "util.h"

/*
 * util_trim(s) — the string without its spaces.
 *
 * Removes the white space at the beginning and at the end.
 *
 * 앞뒤의 공백을 지운 문자열.
 */
std::string util_trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

/*
 * util_lower(s) — the string in small letters.
 *
 * Turns every letter into its small form.
 *
 * 모든 알파벳을 소문자로 바꾼 문자열.
 */
std::string util_lower(const std::string &s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

/*
 * util_has_suffix(s, suffix) — does the string end with that?
 *
 * True when the string ends with the given text.
 *
 * 그 문자열이 주어진 글자로 끝나면 참.
 */
bool util_has_suffix(const std::string &s, const char *suffix)
{
    size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

/*
 * util_path_dir(p) — the directory of a path.
 *
 * Everything up to and including the last '/' or '\\'.  A path without a
 * separator has no directory, so the result is empty.
 *
 * 마지막 '/' 또는 '\\'까지의 부분. 구분자가 없는 경로에는 디렉터리가 없으므로
 * 결과가 비어 있다.
 */
std::string util_path_dir(const std::string &p)
{
    size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : p.substr(0, pos + 1);
}

/*
 * util_path_base(p) — the name of a file without its directory.
 *
 * Everything behind the last '/' or '\\'.
 *
 * 마지막 '/' 또는 '\\' 뒤의 부분.
 */
std::string util_path_base(const std::string &p)
{
    size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

/*
 * util_hex_digit(c) — the value of one hex digit.
 *
 * Turns '0'..'9', 'a'..'f' and 'A'..'F' into 0..15.  Anything else is
 * read as 0, so a broken file does not crash the loader.
 *
 * '0'~'9', 'a'~'f', 'A'~'F'를 0~15로 바꾼다. 그 외의 문자는 0으로 읽는다.
 * 망가진 파일이 로더를 죽이지 않게 하기 위해서다.
 */
int util_hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

/*
 * util_is_hex_digit(c) — is this a digit of a hexadecimal number?
 *
 * True for '0'..'9', 'a'..'f' and 'A'..'F'.
 *
 * '0'~'9', 'a'~'f', 'A'~'F'이면 참.
 */
bool util_is_hex_digit(int c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* ------------------------------------------------------------------ *
 *  The host system
 * ------------------------------------------------------------------ */

namespace fs = std::filesystem;

bool util_list_dir(const std::string &dir, std::vector<UtilDirEntry> &out)
{
    std::error_code ec;
    fs::directory_iterator it(fs::u8path(dir), ec), end;
    if (ec) return false;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        std::string name = it->path().filename().u8string();
        if (name.empty() || name[0] == '.') continue;
        std::error_code ec2;
        out.push_back({ name, it->is_directory(ec2) });
    }
    return true;
}

bool util_is_dir(const std::string &path)
{
    std::error_code ec;
    return fs::is_directory(fs::u8path(path), ec);
}

bool util_is_file(const std::string &path)
{
    std::error_code ec;
    return fs::is_regular_file(fs::u8path(path), ec);
}

std::string util_cwd(void)
{
    std::error_code ec;
    fs::path p = fs::current_path(ec);
    return ec ? std::string(".") : p.u8string();
}

bool util_chdir(const std::string &path)
{
    std::error_code ec;
    fs::current_path(fs::u8path(path), ec);
    return !ec;
}

bool util_make_dir(const std::string &path, bool parents)
{
    std::error_code ec;
    if (parents) {
        fs::create_directories(fs::u8path(path), ec);
        return !ec && util_is_dir(path);
    }
    return fs::create_directory(fs::u8path(path), ec) && !ec;
}

FILE *util_popen(const char *command, const char *mode)
{
#if defined(_WIN32)
    return _popen(command, mode);
#else
    return popen(command, mode);
#endif
}

int util_pclose(FILE *f)
{
#if defined(_WIN32)
    return _pclose(f);
#else
    return pclose(f);
#endif
}

bool util_stdin_is_terminal(void)
{
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

bool util_local_time(time_t t, struct tm *out)
{
#if defined(_WIN32)
    return localtime_s(out, &t) == 0;
#else
    return localtime_r(&t, out) != NULL;
#endif
}

bool util_utc_time(time_t t, struct tm *out)
{
#if defined(_WIN32)
    return gmtime_s(out, &t) == 0;
#else
    return gmtime_r(&t, out) != NULL;
#endif
}
