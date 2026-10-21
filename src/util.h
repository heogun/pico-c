#ifndef PICO8_UTIL_H
#define PICO8_UTIL_H

/*
 * util.h — the little helpers that more than one part of pico-c needs.
 *
 * A few things come up again and again: taking the spaces off a string,
 * putting a string in small letters, telling whether a name ends with a
 * given suffix, splitting a path into its directory and its file name, and
 * reading the hexadecimal digits of a cartridge file.  They used to be
 * copied from one file to the next, which is how the cartridge loader and
 * the shell ended up with two slightly different ways of doing the same
 * thing.  They live here now, so that there is only one of each.
 *
 * 몇 가지 일은 늘 반복해서 나온다: 문자열의 공백을 지우기, 문자열을 소문자로
 * 바꾸기, 이름이 주어진 접미사로 끝나는지 확인하기, 경로를 디렉터리와 파일
 * 이름으로 나누기, 카트리지 파일의 16진수 자리 읽기. 이들은 예전에는 파일마다
 * 복사되어 있었고, 그 때문에 카트리지 로더와 셸이 같은 일을 조금씩 다르게
 * 했다. 이제는 각 하나가 여기 한 벌만 있다.
 *
 * Nothing in here knows anything about PICO-8: these are plain text and
 * path helpers that would work in any program.
 *
 * 여기 있는 것들은 PICO-8에 대해 아무것도 모른다. 어느 프로그램에서나 쓸 수 있는
 * 평범한 문자열과 경로 헬퍼들이다.
 */

#include <stddef.h>
#include <stdint.h>

#include <string>

/* ------------------------------------------------------------------ *
 *  Text
 * ------------------------------------------------------------------ */

/*
 * util_trim(s) — the string without its spaces.
 *
 * Removes the white space at the beginning and at the end.
 *
 * 앞뒤의 공백을 지운 문자열.
 */
std::string util_trim(const std::string &s);

/*
 * util_lower(s) — the string in small letters.
 *
 * Turns every letter into its small form.
 *
 * 모든 알파벳을 소문자로 바꾼 문자열.
 */
std::string util_lower(const std::string &s);

/*
 * util_has_suffix(s, suffix) — does the string end with that?
 *
 * True when the string ends with the given text.
 *
 * 그 문자열이 주어진 글자로 끝나면 참.
 */
bool util_has_suffix(const std::string &s, const char *suffix);

/* ------------------------------------------------------------------ *
 *  Paths
 * ------------------------------------------------------------------ */

/*
 * util_path_dir(p) — the directory of a path.
 *
 * Everything up to and including the last '/' or '\\'.  A path without a
 * separator has no directory, so the result is empty.
 *
 * 마지막 '/' 또는 '\\'까지의 부분. 구분자가 없는 경로에는 디렉터리가 없으므로
 * 결과가 비어 있다.
 */
std::string util_path_dir(const std::string &p);

/*
 * util_path_base(p) — the name of a file without its directory.
 *
 * Everything behind the last '/' or '\\'.
 *
 * 마지막 '/' 또는 '\\' 뒤의 부분.
 */
std::string util_path_base(const std::string &p);

/* ------------------------------------------------------------------ *
 *  Hexadecimal
 * ------------------------------------------------------------------ */

/*
 * util_hex_digit(c) — the value of one hex digit.
 *
 * Turns '0'..'9', 'a'..'f' and 'A'..'F' into 0..15.  Anything else is
 * read as 0, so a broken file does not crash the loader.
 *
 * '0'~'9', 'a'~'f', 'A'~'F'를 0~15로 바꾼다. 그 외의 문자는 0으로 읽는다.
 * 망가진 파일이 로더를 죽이지 않게 하기 위해서다.
 */
int util_hex_digit(int c);

/*
 * util_is_hex_digit(c) — is this a digit of a hexadecimal number?
 *
 * True for '0'..'9', 'a'..'f' and 'A'..'F'.
 *
 * '0'~'9', 'a'~'f', 'A'~'F'이면 참.
 */
bool util_is_hex_digit(int c);

#endif /* PICO8_UTIL_H */
