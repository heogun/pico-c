/*
 * cart_write.cpp — writing a cartridge as a .p8 file.
 *
 * The writer compares every part of the data area with its default and
 * leaves the parts that did not change out of the file, so a saved cart
 * stays as small as the real PICO-8 keeps it.
 *
 * 쓰는 쪽은 데이터 영역의 각 부분을 기본값과 비교해 바뀐 부분만 파일에
 * 남기므로, 저장한 카트가 실제 PICO-8이 만드는 만큼 작게 유지된다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <string>
#include <vector>

#include <stdint.h>

#include "pico8.h"
#include "util.h"
#include "cart_internal.h"

static const char kHexDigits[] = "0123456789abcdef";

/*
 * put_hex_byte(out, b) — one byte as two hex digits.
 *
 * Adds the byte to the string in lower case hex.
 *
 * 바이트를 소문자 16진수 두 자리로 문자열에 붙인다.
 */
static void put_hex_byte(std::string &out, uint8_t b)
{
    out += kHexDigits[b >> 4];
    out += kHexDigits[b & 0x0f];
}

/*
 * gfx_line_is_default(d, line) — is this row of the sprite sheet empty?
 *
 * One row of the sprite sheet is 64 bytes.  A row that is all zeroes is
 * the default and does not have to be written into the file.
 *
 * 스프라이트 시트의 한 줄은 64바이트다. 전부 0인 줄은 기본값이므로 파일에
 * 쓸 필요가 없다.
 */
static bool gfx_line_is_default(const uint8_t *d, int line)
{
    const uint8_t *p = d + PICO8_GFX_ADDR + (size_t)line * 64;
    for (int i = 0; i < 64; i++) if (p[i]) return false;
    return true;
}

/*
 * gff_is_empty(d) — are the graphics flags empty?
 *
 * The flags are 256 bytes, one per sprite.  When they are all zero the
 * __gff__ section is left out of the file.
 *
 * 플래그는 스프라이트마다 한 바이트씩 256바이트다. 전부 0이면 __gff__
 * 섹션을 파일에 넣지 않는다.
 */
static bool gff_is_empty(const uint8_t *d)
{
    for (int i = 0; i < 0x100; i++) if (d[PICO8_GFX_FLAGS + i]) return false;
    return true;
}

/*
 * map_line_is_default(d, line) — is this row of the map empty?
 *
 * One row of the map is 128 bytes.  A row that is all zeroes is the
 * default and does not have to be written into the file.
 *
 * 맵의 한 줄은 128바이트다. 전부 0인 줄은 기본값이므로 파일에 쓸 필요가
 * 없다.
 */
static bool map_line_is_default(const uint8_t *d, int line)
{
    const uint8_t *p = d + PICO8_MAP_ADDR + (size_t)line * 128;
    for (int i = 0; i < 128; i++) if (p[i]) return false;
    return true;
}

/*
 * sfx_line_is_default(d, i) — is this sound effect the default one?
 *
 * The 32 notes have to be zero.  The four header bytes may not be, apart
 * from the speed, which PICO-8 seeds per bank: 1 for the first effect and
 * 16 for the others.
 *
 * 음표 32개는 전부 0이어야 한다. 헤더 4바이트는 속도를 제외하면 0이어야
 * 하는데, 속도는 PICO-8이 뱅크마다 정해 둔다. 첫 번째 효과음은 1, 나머지는
 * 16이다.
 */
static bool sfx_line_is_default(const uint8_t *d, int i)
{
    const uint8_t *p = d + PICO8_SFX_ADDR + (size_t)i * CART_SFX_SIZE;
    for (int n = 0; n < 64; n++) if (p[n]) return false;     /* the 32 notes */
    int editor = p[64], speed = p[65], loop0 = p[66], loop1 = p[67];
    int def_speed = (i == 0) ? 1 : 16;                       /* PICO-8 default */
    return editor == 0 && loop0 == 0 && loop1 == 0 &&
           (speed == 0 || speed == def_speed);
}

/*
 * music_line_is_default(d, i) — is this song the default one?
 *
 * A song that was never touched holds the "no pattern" marker
 * 0x41,0x42,0x43,0x44 in its four channel bytes.
 *
 * 건드리지 않은 song은 채널 바이트 네 개에 "패턴 없음" 표지
 * 0x41,0x42,0x43,0x44를 들고 있다.
 */
static bool music_line_is_default(const uint8_t *d, int i)
{
    const uint8_t *p = d + PICO8_SONG_ADDR + (size_t)i * CART_SONG_SIZE;
    return p[0] == 0x41 && p[1] == 0x42 && p[2] == 0x43 && p[3] == 0x44;
}

/*
 * cart_write_p8(path, d, code, version) — write a .p8 file.
 *
 * Puts the source and the data area into PICO-8's own text format.  The
 * sections that only hold default data are left out, and the sections that
 * hold real data are cut off after their last line that is not default, so
 * a saved cart stays as small as the one the real PICO-8 writes.
 *
 * The source is stored as charset bytes and the file is UTF-8 text, so
 * the glyphs are re-encoded on the way out.
 *
 * 소스와 데이터 영역을 PICO-8의 텍스트 형식으로 정리한다. 기본값만
 * 들어 있는 섹션은 아예 빼고, 실제 데이터가 있는 섹션은 마지막으로 기본값이
 * 아닌 줄에서 잘라내므로, 저장된 카트는 실제 PICO-8이 쓰는 만큼 작게
 * 남는다.
 *
 * 소스는 charset 바이트로 저장되고 파일은 UTF-8 텍스트라서, 나갈 때 글자를
 * 다시 인코딩한다.
 */
int cart_write_p8(const char *path, const uint8_t *d,
                         const std::string &code, int version)
{
    std::string out;
    out.reserve(0x40000);

    out += "pico-8 cartridge // http://www.pico-8.com\n";
    out += "version ";
    out += std::to_string(version);
    out += "\n__lua__\n";
    /* The stored source is charset bytes; cart files are UTF-8 text, so
       glyphs are re-encoded on the way out (as the oracle does -- unknown
       characters survive a load/save roundtrip because their raw bytes
       re-encode into the glyphs they spell). */
    out += pico8_charset_to_utf8(code.data(), code.size());
    if (out.empty() || out.back() != '\n') out += '\n';

    /* __gfx__ : 128 rows of 128 hex digits, nibble pairs swapped. */
    int last = -1;
    for (int row = 0; row < 128; row++)
        if (!gfx_line_is_default(d, row)) last = row;
    if (last >= 0) {
        out += "__gfx__\n";
        for (int row = 0; row <= last; row++) {
            for (int col = 0; col < 64; col++) {
                uint8_t b = d[PICO8_GFX_ADDR + (size_t)row * 64 + col];
                out += kHexDigits[b & 0x0f];
                out += kHexDigits[b >> 4];
            }
            out += '\n';
        }
    }

    /* __gff__ : two lines of 256 hex digits, most-significant nibble first. */
    if (!gff_is_empty(d)) {
        out += "__gff__\n";
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 128; col++)
                put_hex_byte(out, d[PICO8_GFX_FLAGS + (size_t)row * 128 + col]);
            out += '\n';
        }
    }

    /* __label__ : 128 rows of 128 colour digits, where PICO-8 puts it. */
    if (!g_cart_label.empty()) {
        out += "__label__\n";
        out += g_cart_label;
    }

    /* __map__ : 32 rows of 256 hex digits. */
    last = -1;
    for (int row = 0; row < 32; row++)
        if (!map_line_is_default(d, row)) last = row;
    if (last >= 0) {
        out += "__map__\n";
        for (int row = 0; row <= last; row++) {
            for (int col = 0; col < 128; col++)
                put_hex_byte(out, d[PICO8_MAP_ADDR + (size_t)row * 128 + col]);
            out += '\n';
        }
    }

    /* __sfx__ : 64 lines of 4 header bytes + 32 five-nibble notes. */
    last = -1;
    for (int i = 0; i < 64; i++)
        if (!sfx_line_is_default(d, i)) last = i;
    if (last >= 0) {
        out += "__sfx__\n";
        for (int i = 0; i <= last; i++) {
            const uint8_t *p = d + PICO8_SFX_ADDR + (size_t)i * CART_SFX_SIZE;
            for (int k = 0; k < 4; k++) put_hex_byte(out, p[64 + k]);
            for (int n = 0; n < 32; n++) {
                uint8_t lsb = p[n * 2], msb = p[n * 2 + 1];
                int pitch    = lsb & 0x3f;
                int waveform = ((msb & 0x80) >> 4) | ((msb & 0x01) << 2) |
                               ((lsb & 0xc0) >> 6);
                int volume   = (msb & 0x0e) >> 1;
                int effect   = (msb & 0x70) >> 4;
                put_hex_byte(out, (uint8_t)pitch);
                out += kHexDigits[waveform & 0x0f];
                out += kHexDigits[volume & 0x07];
                out += kHexDigits[effect & 0x07];
            }
            out += '\n';
        }
    }

    /* __music__ : 64 lines of "FF c0c1c2c3". The flags live in the high bit
       of the four channel bytes in RAM. */
    last = -1;
    for (int i = 0; i < 64; i++)
        if (!music_line_is_default(d, i)) last = i;
    if (last >= 0) {
        out += "__music__\n";
        for (int i = 0; i <= last; i++) {
            const uint8_t *p = d + PICO8_SONG_ADDR + (size_t)i * CART_SONG_SIZE;
            int fnext   = (p[0] >> 7) & 1;
            int frepeat = (p[1] >> 7) & 1;
            int fstop   = (p[2] >> 7) & 1;
            int flags   = fnext | (frepeat << 1) | (fstop << 2);
            put_hex_byte(out, (uint8_t)flags);
            out += ' ';
            put_hex_byte(out, p[0] & 0x7f);
            put_hex_byte(out, p[1] & 0x7f);
            put_hex_byte(out, p[2] & 0x7f);
            put_hex_byte(out, p[3] & 0x7f);
            out += '\n';
        }
        out += '\n';   /* PICO-8 leaves a blank line after __music__ */
    }

    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t written = fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return (written == out.size()) ? 0 : -1;
}

/*
 * pico8_cart_save_p8(path) — save the running cart as a .p8 file.
 *
 * The data that is written is the RAM of the running cart, so a cart that
 * changed its sprites saves the new sprites.
 *
 * 쓰는 데이터는 실행 중인 카트의 RAM이므로, 스프라이트를 바꾼 카트는
 * 새 스프라이트를 저장한다.
 */
int pico8_cart_save_p8(const char *path)
{
    return cart_write_p8(path, pico8_ram, g_cart_source, kCartVersion);
}

/*
 * pico8_cart_save_rom_p8(path) — save the cart as it was loaded.
 *
 * The data that is written is the data area of the cart on disk, so a
 * cart that changed its sprites at runtime saves the old sprites.
 *
 * 쓰는 데이터는 디스크에 있는 카트의 데이터 영역이므로, 실행 중에 스프라이트를
 * 바꾼 카트는 원래 스프라이트를 저장한다.
 */
int pico8_cart_save_rom_p8(const char *path)
{
    return cart_write_p8(path, g_cart_rom, g_cart_source, kCartVersion);
}
