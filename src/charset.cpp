/*
** charset.cpp -- PICO-8's text charset
**
** PICO-8 stores text internally as a stream of single bytes that index its
** own font: 0..127 are ASCII, code 127 is a circle, 128..153 hold the
** graphics glyphs and emoji ("█", "🐱", "⬅️", ...), 154..253 hold hiragana
** and katakana and a handful of codes below 32 hold the "funny fonts".
** Cart files and console output are UTF-8 text instead, so every text
** boundary crosses this encoding:
**
**   load (cart source, console input)   UTF-8 glyphs -> charset bytes
**   output (printh, errors, saved cart) charset bytes -> UTF-8 glyphs
**
** Measured against the oracle (PICO-8 0.2.7): a cart whose string literal
** lists all 157 glyphs reports #s == 157 with ord() yielding the charset
** codes, unknown characters such as "한" or "😀" are kept as raw UTF-8
** bytes, cstore() writes the source back with glyphs re-encoded (and the
** raw bytes of unknown characters re-encoded as the glyphs their bytes
** would spell), and printh() transcodes unconditionally -- printh("한")
** prints "ムˇぬ" because those are the glyphs for bytes 237,149,156.
**
** The tables come from tools/gen_charset.py.
*/

#include <cstring>
#include <string>

#include "pico8.h"

struct GlyphSeq {
    const char *utf8;
    uint8_t     code;
    uint8_t     len;
};

#include "charset_table.inc"

/*
** Source text -> internal bytes.  A UTF-8 sequence that names a charset
** glyph collapses into that single byte; everything else (ASCII and
** characters PICO-8 does not know) is copied byte for byte, so an unknown
** character stays a run of raw bytes just like it does on the oracle.
*/
std::string pico8_charset_from_utf8(const char *src, size_t len)
{
    std::string out;
    out.reserve(len);
    size_t i = 0;
    while (i < len) {
        if ((uint8_t)src[i] < 0x80) {
            out += src[i++];
            continue;
        }
        bool matched = false;
        for (size_t k = 0; k < sizeof kGlyphSeqs / sizeof kGlyphSeqs[0]; k++) {
            const GlyphSeq &g = kGlyphSeqs[k];
            if (i + g.len <= len && memcmp(src + i, g.utf8, g.len) == 0) {
                out += (char)g.code;
                i += g.len;
                matched = true;
                break;
            }
        }
        if (!matched) out += src[i++];
    }
    return out;
}

/*
** Internal bytes -> UTF-8 text, for anything that leaves the VM as text:
** the console, error messages and the __lua__ section of a saved cart.
** A NUL stops the conversion: printh() on the oracle has C-string
** semantics, so printh("a"..chr(0).."b") prints just "a".
*/
std::string pico8_charset_to_utf8(const char *src, size_t len)
{
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)src[i];
        if (!c) break;
        if (kGlyphUTF8[c]) out += kGlyphUTF8[c];
        else               out += (char)c;
    }
    return out;
}
