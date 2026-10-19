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
 */

#include "pico8.h"

#include <cstring>

static int is_name_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_name_char(unsigned char c)
{
    return is_name_start(c) || (c >= '0' && c <= '9');
}

static int is_digit(unsigned char c) { return c >= '0' && c <= '9'; }

static int is_hex_digit(unsigned char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* A long bracket opener `[[`, `[=[`, `[==[` ... Returns its length, or 0. */
static size_t long_bracket_open(const char *s, size_t len, size_t i)
{
    if (i >= len || s[i] != '[') return 0;
    size_t j = i + 1;
    while (j < len && s[j] == '=') j++;
    if (j < len && s[j] == '[') return j - i + 1;
    return 0;
}

/* Skip to the closing bracket of a long bracket opened at `i + open_len`
   (`open_len` counts `[`, the `=`s and the second `[`). Returns the index
   just past the closing bracket, or `len` when unterminated. */
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

/* Number literal starting at `i`: `0x` keeps hex digits and dots, `0b` keeps
   binary digits and dots, everything else keeps decimal digits and dots. */
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

static int word_is(const char *s, size_t n, const char *word)
{
    size_t m = std::strlen(word);
    return n == m && !memcmp(s, word, m);
}

/* `end` and `local` are the only free words; everything else costs 1. */
static int word_costs_one(const char *s, size_t n)
{
    return !word_is(s, n, "end") && !word_is(s, n, "local");
}

/* A `-` immediately after one of these glues to a following number literal
   (`not-1`, `and-1`, `if -1`, `return -1`).  These are exactly the keywords
   that can be followed straight away by an expression: `then`, `do`, `else`,
   `end`, `local`, `repeat`, `for`, `in` and `goto` all leave the following
   `-1` as two tokens instead. */
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
