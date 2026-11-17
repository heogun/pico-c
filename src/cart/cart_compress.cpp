/*
 * cart_compress.cpp — the two code compressors PICO-8 used.
 *
 * A .p8.png cartridge holds its code section compressed: with PXA in the
 * newer carts, with the old ":c:" format in the older ones.  Both are
 * unpacked here.
 *
 * .p8.png 카트의 코드 섹션은 압축되어 있다: 새 카트라면 PXA로, 오래된
 * 카트라면 예전의 ":c:" 형식으로. 둘 다 여기서 풀린다.
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

/* Lookup table used by the legacy ":c:" compressor. Note the first entry
   is a literal newline and the second a space. */
static const char legacy_lut[] =
    "\n 0123456789abcdefghijklmnopqrstuvwxyz!#%(){}[]<>+=/*:;.,~_";

/* Move-to-front table used by the PXA decompressor.
 *
 * The table holds the 256 possible bytes in the order they are used.  Every
 * time a byte is taken out it is moved to the front, so the bytes that keep
 * coming back end up near the front and are cheap to code.
 *
 * PXA 해제기가 쓰는 move-to-front 테이블.
 *
 * 테이블은 256개의 가능한 바이트를 사용된 순서로 들고 있다. 바이트를 꺼낼
 * 때마다 맨 앞으로 옮기므로, 자주 나오는 바이트가 앞쪽에 모이고 싸게
 * 코딩된다.
 */
struct move_to_front {
    uint8_t state[256];
    move_to_front() { for (int i = 0; i < 256; i++) state[i] = (uint8_t)i; }

    /*
     * get(n) — the byte at the n-th place, moved to the front.
     *
     * Takes the byte at position `n` out of the table, moves everything in
     * front of it one place back and puts the byte at the front.  The byte
     * that is returned is the one the caller wanted.
     *
     * 테이블의 n번째 자리에 있는 바이트를 꺼내, 그 앞의 것들을 한 칸씩
     * 뒤로 밀고, 그 바이트를 맨 앞에 놓는다. 돌려주는 바이트가 호출한
     * 쪽이 원하던 것이다.
     */
    uint8_t get(int n)
    {
        uint8_t ch = state[n];
        for (int i = n; i > 0; i--) state[i] = state[i - 1];
        state[0] = ch;
        return ch;
    }
};

/*
 * decompress_legacy(src, dest, maxlen) — the old ":c:" compressor.
 *
 * The format PICO-8 used before PXA: a header of eight bytes, then a
 * stream of codes.  A code below 0x3c is a character of the lookup table
 * (its first entry is a newline, the second a space), a code of 0x3c or
 * more is a back reference that copies a number of bytes from earlier in
 * the output, and a code of 0 is a literal byte that follows.
 *
 * PXA가 나오기 전에 PICO-8이 쓰던 형식이다. 8바이트 헤더 다음에 코드의
 * 흐름이 온다. 0x3c보다 작은 코드는 룩업 테이블의 문자(첫 항목은 줄바꿈,
 * 둘째는 공백)이고, 0x3c 이상인 코드는 앞에서 이미 나온 바이트를 복사하는
 * 역참조이며, 0은 바로 뒤에 오는 리터럴 바이트다.
 */
int decompress_legacy(const uint8_t *src, uint8_t *dest, uint32_t maxlen)
{
    uint32_t length = ((uint32_t)src[4] << 8) | src[5];
    uint32_t out = 0;

    for (uint32_t i = 8; i < 0x3d00 && out < length; i++) {
        if (out + 1 >= maxlen) break;

        if (src[i] >= 0x3c) {
            uint32_t a = (uint32_t)(src[i] - 0x3c) * 16 + (src[i + 1] & 0x0f);
            uint32_t b = (src[i + 1] >> 4) + 2;
            if (out >= a) {
                while (b-- && out + 1 < maxlen) {
                    dest[out] = dest[out - a];
                    out++;
                }
            }
            i++;
        } else if (src[i]) {
            dest[out++] = (uint8_t)legacy_lut[src[i] - 1];
        } else {
            dest[out++] = src[++i];
        }
    }

    dest[out] = 0;

    /* The stored length includes trailing zeroes; trim them. */
    uint32_t n = 0;
    while (n < out && dest[n]) n++;
    return (int)n;
}

/*
 * decompress_pxa(src, dest, maxlen) — the PXA compressor.
 *
 * The format that PICO-8 uses today.  The header holds the length of the
 * text and the length of the compressed block; the block itself is a bit
 * stream of three kinds of item:
 *
 *   - a literal character, coded by its position in a move-to-front table
 *     (a table that is reordered every time a character is used, so that
 *     the characters that keep coming back become cheap);
 *   - a run of the same byte, with a length that is coded in groups of
 *     seven;
 *   - a literal run of characters that ends with a zero byte.
 *
 * 오늘날 PICO-8이 쓰는 형식이다. 헤더에는 텍스트 길이와 압축 블록의 길이가
 * 들어 있고, 블록은 세 종류의 항목으로 이루어진 비트 스트림이다:
 *
 *   - 리터럴 문자. move-to-front 테이블에서의 위치로 코딩된다(사용될 때마다
 *     순서가 바뀌는 테이블이라 자주 나오는 문자가 싸진다).
 *   - 같은 바이트의 연속. 길이는 7개 단위로 코딩된다.
 *   - 0 바이트로 끝나는 리터럴 문자열.
 */
int decompress_pxa(const uint8_t *src, uint8_t *dest, uint32_t maxlen)
{
    uint32_t length     = ((uint32_t)src[4] << 8) | src[5];
    uint32_t compressed = ((uint32_t)src[6] << 8) | src[7];

    size_t pos = (size_t)8 * 8;   /* bit position */
    size_t end = (size_t)compressed * 8;

    /* Read `count` bits, least-significant bit first within each byte. */
    auto get_bits = [&](size_t count) -> uint32_t {
        uint32_t n = 0;
        for (size_t i = 0; i < count && pos < end; i++, pos++)
            n |= (uint32_t)((src[pos >> 3] >> (pos & 7)) & 1) << i;
        return n;
    };

    move_to_front mtf;
    uint32_t out = 0;

    while (out < length && pos < end) {
        if (out + 16 >= maxlen) break;

        if (get_bits(1)) {
            /* Emit a single character through the move-to-front table. */
            int nbits = 4;
            while (get_bits(1)) nbits++;
            int n = (int)get_bits((size_t)nbits) + (1 << nbits) - 16;
            if (n < 0 || n >= 256) break;
            uint8_t ch = mtf.get(n);
            if (!ch) break;
            dest[out++] = ch;
        } else {
            int nbits = get_bits(1) ? (get_bits(1) ? 5 : 10) : 15;
            int offset = (int)get_bits((size_t)nbits) + 1;

            if (nbits == 10 && offset == 1) {
                /* Literal run terminated by a zero byte. */
                uint8_t ch = (uint8_t)get_bits(8);
                while (ch) { dest[out++] = ch; ch = (uint8_t)get_bits(8); }
            } else {
                int n, len = 3;
                do { n = (int)get_bits(3); len += n; } while (n == 7);
                for (int i = 0; i < len && out + 1 < maxlen; i++) {
                    dest[out] = (out >= (uint32_t)offset) ? dest[out - offset] : 0;
                    out++;
                }
            }
        }
    }

    dest[out] = 0;
    return (int)out;
}
