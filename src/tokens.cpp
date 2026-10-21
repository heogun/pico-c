/*
 *  PICO-8 token counter.
 *
 *  The oracle does not expose a running cart's token count through info()
 *  in -x mode, but it *does* refuse to run a cart with more than 8192 tokens
 *  and prints the exact count it computed:
 *
 *      program too large
 *      9002 / 8192 tokens
 *
 *  so every rule below was measured by padding a cart past the limit and
 *  reading back the reported total (see progress.md, "3-4. 토큰 카운트").
 *  The counting is lexical, with a few parser-ish quirks:
 *
 *    - a name, keyword, number, string, operator, `(`, `[` or `{` costs 1;
 *      `)`, `]`, `}`, `,` and `;` cost 0, comments cost 0, and so do the
 *      keywords `end` and `local`.
 *    - `.` and `:` are free, so `a.b`, `a . b`, `a : b()` and `::label::`
 *      all count exactly the names they use.  `..`, `...` and `..=` are
 *      operators (one token each).
 *    - a number literal is greedy over its own alphabet, so `1..2`, `1.5..2`
 *      and `0x1.5` are one token each, while `1e5` is two (`1`, `e5`).
 *    - a `-` immediately followed by a number literal joins it when the
 *      previous token cannot end an expression, so `x=-1` is 3 tokens but
 *      `a-1` is not merged.  `#` and `..` are exceptions (they cannot end an
 *      expression either, yet a following `-1` stays separate), and so are
 *      the keywords that introduce a statement (`then`, `do`, `else`, `end`,
 *      `local`, `repeat`, `for`, `in`, `goto`): `if -1 then -1=1 end` is 7.
 *    - `//` starts a line comment, the same as `--`.
 *
 *  PICO-8 토큰 카운터.
 *
 *  오라클은 -x 모드에서 info()로 실행 중인 카트의 토큰 수를 알려주지 않지만,
 *  토큰이 8192를 넘는 카트는 *실행하지 않고* 계산한 정확한 개수를 출력한다:
 *
 *      program too large
 *      9002 / 8192 tokens
 *
 *  그래서 아래의 모든 규칙은 한계를 넘는 카트를 인위적으로 늘린 뒤 보고된
 *  합계를 읽어서 측정했다(progress.md의 "3-4. 토큰 카운트" 참고). 세기는
 * 어휘적이며, 파서 같은 특이점이 몇 가지 있다:
 *
 *    - 이름, 키워드, 숫자, 문자열, 연산자, `(`, `[`, `{`는 각각 1이고,
 *      `)`, `]`, `}`, `,`, `;`는 0이다. 주석도 0이고, `end`와 `local`
 *      키워드도 0이다.
 *    - `.`과 `:`는 무료라서 `a.b`, `a . b`, `a : b()`, `::label::`는 모두
 *      쓰인 이름만큼만 센다. `..`, `...`, `..=`는 연산자라서 각각 1이다.
 *    - 숫자 리터럴은 자기 알파벳 위에서 탐욕적으로 읽는다. 그래서 `1..2`,
 *      `1.5..2`, `0x1.5`는 각각 1개이지만 `1e5`는 2개(`1`, `e5`)다.
 *    - 숫자 리터럴 바로 앞에 오는 `-`는 앞 토큰이 식을 끝낼 수 없을 때 그
 *      숫자에 붙는다. 그래서 `x=-1`은 3토큰이지만 `a-1`은 합쳐지지 않는다.
 *      `#`와 `..`는 예외다(이들도 식을 끝낼 수 없지만 뒤의 `-1`은 따로
 *      남는다). 문장을 시작하는 키워드(`then`, `do`, `else`, `end`,
 *      `local`, `repeat`, `for`, `in`, `goto`)도 예외다:
 *      `if -1 then -1=1 end`는 7이다.
 *    - `//`도 `--`처럼 줄 주석을 시작한다.
 */

#include "pico8.h"

#include <cstring>

/*
 * is_name_start(c) — can a name start with this?
 *
 * True for the letters and for the underscore.
 *
 * 알파벳과 밑줄이면 참.
 */
static int is_name_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/*
 * is_name_char(c) — can a name continue with this?
 *
 * A letter, an underscore or a digit.
 *
 * 알파벳, 밑줄, 숫자면 참.
 */
static int is_name_char(unsigned char c)
{
    return is_name_start(c) || (c >= '0' && c <= '9');
}

static int is_digit(unsigned char c) { return c >= '0' && c <= '9'; }

/*
 * is_hex_digit(c) — is this a digit of a hexadecimal number?
 *
 * '0'..'9', 'a'..'f' and 'A'..'F'.
 *
 * '0'~'9', 'a'~'f', 'A'~'F'.
 */
static int is_hex_digit(unsigned char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/*
 * long_bracket_open(s, len, i) — the long bracket that starts here.
 *
 * Looks for a long bracket opener (`[[`, `[=[`, `[==[` and so on) at the
 * given place and returns how long it is, or 0 when there is none.
 *
 * 주어진 자리에 있는 긴 괄호 여는 기호(`[[`, `[=[`, `[==[` 등)를 찾아 그
 * 길이를 돌려준다. 없으면 0이다.
 */
static size_t long_bracket_open(const char *s, size_t len, size_t i)
{
    if (i >= len || s[i] != '[') return 0;
    size_t j = i + 1;
    while (j < len && s[j] == '=') j++;
    if (j < len && s[j] == '[') return j - i + 1;
    return 0;
}

/*
 * long_bracket_close(s, len, from, open_len) — the end of a long bracket.
 *
 * Skips to the bracket that closes the long bracket that was opened at
 * `from`.  The number of equals signs has to match the one of the opener.
 * A bracket that is never closed runs to the end of the source.
 *
 * `from`에서 열린 긴 괄호를 닫는 괄호까지 건너뛴다. '='의 개수가 여는 기호와
 * 같아야 한다. 닫히지 않는 괄호는 소스 끝까지 간다.
 */
static size_t long_bracket_close(const char *s, size_t len,
                                 size_t from, size_t open_len)
{
    size_t level = open_len - 2;
    size_t j = from;
    while (j < len) {
        if (s[j] == ']') {
            size_t k = j + 1, eq = 0;
            while (k < len && s[k] == '=') { eq++; k++; }
            if (eq == level && k < len && s[k] == ']') return k + 1;
        }
        j++;
    }
    return len;
}

/*
 * scan_number(s, len, i) — the number literal that starts here.
 *
 * A number that starts with 0x keeps the hexadecimal digits and the
 * dots, one that starts with 0b keeps the binary digits and the dots, and
 * everything else keeps the decimal digits and the dots.  The scan is
 * greedy over that alphabet, so `1..2` is one number and not two.
 *
 * 여기서 시작하는 숫자 리터럴을 읽는다. 0x로 시작하면 16진수 자리와 점을,
 * 0b로 시작하면 2진수 자리와 점을, 그 외에는 10진수 자리와 점을 계속 읽는다.
 * 탐욕적으로 읽으므로 `1..2`는 숫자 두 개가 아니라 하나다.
 */
static size_t scan_number(const char *s, size_t len, size_t i)
{
    if (s[i] == '0' && i + 1 < len && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        i += 2;
        while (i < len && (is_hex_digit((unsigned char)s[i]) || s[i] == '.'))
            i++;
        return i;
    }
    if (s[i] == '0' && i + 1 < len && (s[i + 1] == 'b' || s[i + 1] == 'B')) {
        i += 2;
        while (i < len && (s[i] == '0' || s[i] == '1' || s[i] == '.'))
            i++;
        return i;
    }
    while (i < len && (is_digit((unsigned char)s[i]) || s[i] == '.'))
        i++;
    return i;
}

/*
 * word_is(s, n, word) — is this word that one?
 *
 * Compares the first `n` characters with the given word.
 *
 * 처음 `n`글자를 주어진 단어와 비교한다.
 */
static int word_is(const char *s, size_t n, const char *word)
{
    size_t m = std::strlen(word);
    return n == m && !memcmp(s, word, m);
}

/*
 * word_costs_one(s, n) — does this word cost a token?
 *
 * Every word costs one token, apart from `end` and `local`, which are
 * free.
 *
 * `end`와 `local`만 무료이고, 나머지 단어는 모두 토큰 하나다.
 */
static int word_costs_one(const char *s, size_t n)
{
    return !word_is(s, n, "end") && !word_is(s, n, "local");
}

/*
 * word_allows_unary_merge(s, n) — can a minus follow this word directly?
 *
 * The words that can be followed straight away by an expression let the
 * minus of a negative number join the number: `not-1`, `and-1`, `if -1`
 * and `return -1` are read as a single token.  The keywords that open a
 * statement (`then`, `do`, `else`, `end`, `local`, `repeat`, `for`, `in`
 * and `goto`) leave the `-1` as two tokens.
 *
 * 식을 바로 뒤에 둘 수 있는 단어는 음수 표지의 마이너스를 숫자에 붙인다.
 * `not-1`, `and-1`, `if -1`, `return -1`이 한 토큰으로 읽힌다. 문장을 여는
 * 키워드(`then`, `do`, `else`, `end`, `local`, `repeat`, `for`, `in`, `goto`)는
 * `-1`을 두 토큰으로 남긴다.
 */
static int word_allows_unary_merge(const char *s, size_t n)
{
    static const char *const kKeywords[] = {
        "and", "elseif", "if", "not", "or", "return", "until", "while", NULL
    };
    for (int k = 0; kKeywords[k]; k++)
        if (word_is(s, n, kKeywords[k])) return 1;
    return 0;
}

/* Longest-match operator table.  `//` is a comment and `..`/`...` are handled
   by the caller, so neither appears here. */
static const char *const kOperators[] = {
    ">>>=", ">>>",
    "<<=", ">>=", "^^=", "..=",
    "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
    "==", "~=", "!=", "<=", ">=", "<<", ">>", "^^",
    "+", "-", "*", "/", "%", "^", "=", "<", ">",
    "&", "|", "~", "!", "#", "\\", "(", "{",
    NULL
};

/*
 * pico8_count_tokens(src, len) — how many tokens the source has.
 *
 * Counts the tokens the way PICO-8 does.  A name, a keyword, a number, a
 * string, an operator and `(`, `[` or `{` cost one each; `)`, `]`, `}`, `,`
 * and `;` are free, and so are the comments, the keywords `end` and
 * `local`, the `.` and the `:`.
 *
 * The rule of the unary minus is the tricky one and was measured on the
 * oracle: a `-` in front of a number joins it when the token before it
 * cannot end an expression, and the words that open a statement are the
 * exception to that.
 *
 * PICO-8과 같은 방식으로 토큰을 센다. 이름, 키워드, 숫자, 문자열, 연산자와
 * `(`, `[`, `{`는 각각 1이고, `)`, `]`, `}`, `,`, `;`는 0이다. 주석도 0이고,
 * `end`와 `local` 키워드도 0이며, `.`과 `:`도 0이다.
 *
 * 단항 마이너스의 규칙이 까다롭고 오라클에서 측정했다. 숫자 앞의 `-`는 앞
 * 토큰이 식을 끝낼 수 없을 때 그 숫자에 붙고, 문장을 여는 키워드가 그
 * 예외다.
 */
int pico8_count_tokens(const char *src, size_t len)
{
    int tokens = 0;
    /* True when the previous token cannot be followed by a *unary* minus,
       i.e. when a `-` glued to a number must stay a separate token. */
    int no_unary_merge = 0;
    size_t i = 0;

    while (i < len) {
        unsigned char c = (unsigned char)src[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '\v' || c == '\f') {
            i++;
            continue;
        }

        /* Comments: `--`, `//`, `--[[ ]]`, `--[==[ ]==]`. */
        if ((c == '-' && i + 1 < len && src[i + 1] == '-') ||
            (c == '/' && i + 1 < len && src[i + 1] == '/')) {
            size_t j = i + 2;
            size_t open = (c == '-') ? long_bracket_open(src, len, j) : 0;
            if (open)
                j = long_bracket_close(src, len, j + open, open);
            else
                while (j < len && src[j] != '\n') j++;
            i = j;
            continue;
        }

        /* Numbers (also `.5`). */
        if (is_digit(c) ||
            (c == '.' && i + 1 < len && is_digit((unsigned char)src[i + 1]))) {
            i = scan_number(src, len, i);
            tokens++;
            no_unary_merge = 1;
            continue;
        }

        /* Unary minus glued to a number literal (`x=-1`, `f(-1)`, `{-1}`).
           Only a real number joins it: `-.5` does, but `-...` does not. */
        if (c == '-' && !no_unary_merge && i + 1 < len &&
            (is_digit((unsigned char)src[i + 1]) ||
             (src[i + 1] == '.' && i + 2 < len &&
              is_digit((unsigned char)src[i + 2])))) {
            i = scan_number(src, len, i + 1);
            tokens++;
            no_unary_merge = 1;
            continue;
        }

        /* Names and keywords.  `end` and `local` are free. */
        if (is_name_start(c)) {
            size_t j = i;
            while (j < len && is_name_char((unsigned char)src[j])) j++;
            size_t n = j - i;
            tokens += word_costs_one(src + i, n);
            /* A value (an identifier or `nil`/`true`/`false`) blocks a
               following unary minus; a keyword lets it merge. */
            no_unary_merge = !word_allows_unary_merge(src + i, n);
            i = j;
            continue;
        }

        /* Quoted strings, with backslash escapes. */
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < len) {
                if (src[j] == '\\') { j += 2; continue; }
                if (src[j] == c) { j++; break; }
                if (src[j] == '\n') break;
                j++;
            }
            i = j > len ? len : j;
            tokens++;
            no_unary_merge = 1;
            continue;
        }

        /* Long strings `[[ ]]`, `[=[ ]=]` cost 1; `[` alone costs 1. */
        if (c == '[') {
            size_t open = long_bracket_open(src, len, i);
            if (open) {
                i = long_bracket_close(src, len, i + open, open);
                tokens++;
                no_unary_merge = 1;
            } else {
                i++;
                tokens++;
                no_unary_merge = 0;
            }
            continue;
        }

        if (c == '.') {
            /* `...`, `..=` and `..` are operators built from adjacent dots.
               A lone `.` is free: `a.b`, `a . b` and `.5` all just count the
               name or number that follows, so `.name` needs no special case. */
            if (i + 2 < len && src[i + 1] == '.' && src[i + 2] == '.') {
                i += 3;                     /* `...`, a value */
                tokens++;
                no_unary_merge = 1;
                continue;
            }
            if (i + 2 < len && src[i + 1] == '.' && src[i + 2] == '=') {
                i += 3;                     /* `..=`, concatenate and assign */
                tokens++;
                no_unary_merge = 0;
                continue;
            }
            if (i + 1 < len && src[i + 1] == '.') {
                i += 2;                     /* `..`, but no unary merge */
                tokens++;
                no_unary_merge = 1;
                continue;
            }
            i++;                            /* a free `.` */
            no_unary_merge = 1;
            continue;
        }

        if (c == ':') {
            /* `:` is free too (`a:b()`, `a : b()` and `::label::`). */
            i++;
            no_unary_merge = 1;
            continue;
        }

        /* Closing brackets and `;` end an expression, so a `-1` after one
           stays two tokens; a value is expected after `,`. */
        if (c == ')' || c == ']' || c == '}' || c == ';') {
            i++;
            no_unary_merge = 1;
            continue;
        }
        if (c == ',') {
            i++;
            no_unary_merge = 0;
            continue;
        }

        /* Operators, longest match first. */
        {
            int matched = 0;
            for (int k = 0; kOperators[k]; k++) {
                size_t n = std::strlen(kOperators[k]);
                if (i + n <= len && !memcmp(src + i, kOperators[k], n)) {
                    i += n;
                    tokens++;
                    /* `#` cannot end an expression, yet a following `-1`
                       does not merge into it. */
                    no_unary_merge = (n == 1 && kOperators[k][0] == '#');
                    matched = 1;
                    break;
                }
            }
            if (matched) continue;
        }

        /* Anything else (a stray byte) still costs one token. */
        i++;
        tokens++;
        no_unary_merge = 0;
    }

    return tokens;
}
