/*
 * cart_png.cpp — writing a cartridge as a .p8.png image.
 *
 * The image is built by hand: the code section is compressed with PXA and
 * the data area is scattered into the pixels of the picture, the same way
 * PICO-8 stores it.
 *
 * 이미지는 손으로 만든다: 코드 섹션을 PXA로 압축하고 데이터 영역을 그림의
 * 픽셀에 흩scatter, 실제 PICO-8과 같은 방식으로 저장한다.
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
#include "pxa.h"
#include "cart_internal.h"

/*
 * png_crc32(p, n) — the checksum of a PNG chunk.
 *
 * The CRC that every PNG chunk carries: the same polynomial as zlib uses
 * (the reflected 0xedb88320) over the type of the chunk and its body.
 *
 * PNG의 모든 청크가 들고 다니는 체크섬. zlib과 같은 다항식(반사된
 * 0xedb88320)을 청크의 타입과 본문에 대해 계산한다.
 */
static uint32_t png_crc32(const uint8_t *p, size_t n)
{
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

/*
 * png_be32(s, v) — a 32 bit number, big endian.
 *
 * Adds the number to the string, most significant byte first.  PNG counts
 * in big endian.
 *
 * 수를 문자열에 붙인다. 큰 바이트부터. PNG는 빅 엔디안으로 센다.
 */
static void png_be32(std::string &s, uint32_t v)
{
    s += (char)((v >> 24) & 0xff);
    s += (char)((v >> 16) & 0xff);
    s += (char)((v >> 8) & 0xff);
    s += (char)(v & 0xff);
}

/*
 * png_chunk(out, type, body) — one PNG chunk.
 *
 * Writes the length, the four characters of the type, the body and the
 * CRC of both.
 *
 * 길이, 네 글자의 타입, 본문, 그리고 둘의 CRC를 쓴다.
 */
static void png_chunk(std::string &out, const char *type, const std::string &body)
{
    png_be32(out, (uint32_t)body.size());
    std::string tagged = std::string(type, 4) + body;
    out += tagged;
    png_be32(out, png_crc32((const uint8_t *)tagged.data(), tagged.size()));
}

/*
 * zlib_stored(out, raw) — the image as a zlib stream.
 *
 * The image is deflated with stored (uncompressed) blocks: the result is a
 * valid zlib stream, but a DEFLATE implementation is not needed.  The
 * adler32 checksum at the end is the one of the uncompressed data.
 *
 * 이미지를 stored(압축하지 않은) 블록으로 deflate한다. 결과는 올바른 zlib
 * 스트림이지만 DEFLATE 구현이 필요 없다. 끝에 붙는 adler32 체크섬은 압축하지
 * 않은 데이터의 것이다.
 */
static void zlib_stored(std::string &out, const std::string &raw)
{
    out += (char)0x78;                  /* CM=8, CINFO=7 */
    out += (char)0x01;                  /* FCHECK so 0x7801 is a multiple */

    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = raw.size() - pos;
        if (n > 65535) n = 65535;
        bool last = (pos + n >= raw.size());
        out += (char)(last ? 1 : 0);
        out += (char)(n & 0xff);
        out += (char)((n >> 8) & 0xff);
        out += (char)(~n & 0xff);
        out += (char)((~n >> 8) & 0xff);
        out.append(raw, pos, n);
        pos += n;
    }

    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw.size(); i++) {
        a = (a + (uint8_t)raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    png_be32(out, (b << 16) | a);
}

/*
 * png_scatter(rows, height, palette, stride, filtered) — hide bytes in the picture.
 *
 * The cart is stored in the two low bits of every colour channel of every
 * pixel.  The byte of a pixel is spread over the four channels: the alpha
 * channel holds the top bits, then red, green and blue.
 *
 * 카트는 모든 픽셀의 색 채널 하위 두 비트에 저장된다. 픽셀의 바이트 하나가
 * 네 채널에 흩어진다: 알파 채널이 상위 비트를, 그다음 빨강, 초록, 파랑이
 * 나눠 가진다.
 */
static void png_scatter(const uint8_t *const *rows, int height,
                        const uint8_t *palette, int stride,
                        std::string &filtered)
{
    filtered.clear();
    filtered.reserve((size_t)(stride * 4 + 1) * (size_t)height);
    for (int y = 0; y < height; y++) {
        const uint8_t *data = rows[y];
        filtered += (char)0;   /* filter type 0 (None) */
        for (int x = 0; x < stride; x++) {
            uint8_t v = data[x];
            const uint8_t *c = palette + (y * stride + x) * 4;
            filtered += (char)(c[0] | ((v >> 4) & 3));
            filtered += (char)(c[1] | ((v >> 2) & 3));
            filtered += (char)(c[2] | (v & 3));
            filtered += (char)(c[3] | ((v >> 6) & 3));
        }
    }
}

/*
 * cart_write_p8png(path, rom, code, logo) — write a .p8.png file.
 *
 * Builds the 160x205 picture that carries the cart: the data area in the
 * first 0x4300 pixels, the compressed source at 0x4300 and the metadata
 * at 0x8000.  The bytes are spread over the colour channels and the
 * picture is written as a PNG.
 *
 * PICO-8 writes its carts with LodePNG and tags them with a "LodePNG
 * 20110221" text chunk; the same chunk layout is used here so that the
 * tooling which looks for it keeps working.
 *
 * 카트를 담는 160x205 그림을 만든다: 첫 0x4300 픽셀이 데이터 영역, 0x4300부터
 * 압축된 소스, 0x8000부터 메타데이터다. 바이트를 색 채널에 흩어 놓고 PNG로
 * 쓴다.
 *
 * PICO-8은 카트를 LodePNG로 쓰고 "LodePNG 20110221" 텍스트 청크로 표시를
 * 남긴다. 여기서도 같은 청크 배치를 써서, 그것을 찾는 도구가 계속 동작하게
 * 한다.
 */
int cart_write_p8png(const char *path, const uint8_t *rom,
                            const std::string &code, const uint8_t *logo)
{
    const int kWidth = 160;
    const int kHeight = 205;

    /* PICO-8 palettes its label from the alphabet of cartridge colours; a
       cart with no label is drawn plain black (0x000000). */
    static const uint8_t kPalette[4] = { 0x00, 0x00, 0x00, 0xff };

    /* --- assemble the steganographic blob (0x8020 bytes). --- */
    std::vector<uint8_t> blob((size_t)kWidth * kHeight, 0);
    memcpy(blob.data(), rom, PICO8_CART_DATA_SIZE);

    std::vector<uint8_t> packed((size_t)code.size() + 1024);
    int packed_len = pxa_compress((const uint8_t *)code.data(), packed.data(),
                                  (int)code.size());

    /* PXA signals "not worth compressing" by copying the input back and
       returning its length; the code section then holds plain text, which
       PICO-8 also reads. Anything past what the code area holds is a
       failure. */
    if ((size_t)packed_len + 8 > 0x3d00) {
        fprintf(stderr, "cart code does not fit the .p8.png code area\n");
        return -1;
    }
    memcpy(blob.data() + 0x4300, packed.data(), (size_t)packed_len);

    if (logo) memcpy(blob.data() + 0x8000, logo, 32);

    /* --- spread the blob into the pixels and write the PNG. --- */
    std::vector<const uint8_t *> rows((size_t)kHeight);
    for (int y = 0; y < kHeight; y++) rows[y] = blob.data() + (size_t)y * kWidth;

    std::string filtered;
    png_scatter(rows.data(), kHeight, kPalette, kWidth, filtered);

    std::string out = "\x89PNG\r\n\x1a\n";
    {
        std::string ihdr;
        ihdr += (char)((kWidth >> 24) & 0xff);
        ihdr += (char)((kWidth >> 16) & 0xff);
        ihdr += (char)((kWidth >> 8) & 0xff);
        ihdr += (char)(kWidth & 0xff);
        ihdr += (char)((kHeight >> 24) & 0xff);
        ihdr += (char)((kHeight >> 16) & 0xff);
        ihdr += (char)((kHeight >> 8) & 0xff);
        ihdr += (char)(kHeight & 0xff);
        ihdr += (char)8;   /* bit depth    */
        ihdr += (char)6;   /* colour type  */
        ihdr += (char)0;   /* compression  */
        ihdr += (char)0;   /* filter       */
        ihdr += (char)0;   /* interlace    */
        png_chunk(out, "IHDR", ihdr);
    }
    {
        std::string z;
        zlib_stored(z, filtered);
        png_chunk(out, "IDAT", z);
    }
    png_chunk(out, "tEXt", std::string("LodePNG", 7) + std::string(1, '\0') +
                               std::string("20110221", 8));
    png_chunk(out, "IEND", std::string());

    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t written = fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return (written == out.size()) ? 0 : -1;
}

/*
 * pico8_png_write(path, pixels, width, height) — write an RGB picture.
 *
 * Colour type 2 (RGB, 8 bits a channel), every row with filter 0, the
 * data in stored deflate blocks, like the cart writer above.
 *
 * 색 형식 2(RGB, 채널당 8비트), 모든 줄은 필터 0, 데이터는 위의 카트 쓰기처럼
 * stored deflate 블록으로.
 */
int pico8_png_write(const char *path, const uint32_t *pixels, int width, int height)
{
    std::string raw;
    raw.reserve((size_t)height * (1 + (size_t)width * 3));
    for (int y = 0; y < height; y++) {
        raw += (char)0;                                  /* filter: none */
        for (int x = 0; x < width; x++) {
            uint32_t c = pixels[(size_t)y * width + x];
            raw += (char)((c >> 16) & 0xff);
            raw += (char)((c >> 8) & 0xff);
            raw += (char)(c & 0xff);
        }
    }

    std::string out = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    png_be32(ihdr, (uint32_t)width);
    png_be32(ihdr, (uint32_t)height);
    ihdr += (char)8;   /* bit depth   */
    ihdr += (char)2;   /* colour type */
    ihdr += (char)0;   /* compression */
    ihdr += (char)0;   /* filter      */
    ihdr += (char)0;   /* interlace   */
    png_chunk(out, "IHDR", ihdr);
    std::string z;
    zlib_stored(z, raw);
    png_chunk(out, "IDAT", z);
    png_chunk(out, "IEND", std::string());

    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t written = fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return (written == out.size()) ? 0 : -1;
}

/*
 * pico8_cart_save_png(path) — save the running cart as a .p8.png file.
 *
 * The data that is written is the RAM of the running cart.
 *
 * 쓰는 데이터는 실행 중인 카트의 RAM이다.
 */
int pico8_cart_save_png(const char *path)
{
    return cart_write_p8png(path, pico8_ram, g_cart_source, g_cart_logo);
}

/*
 * pico8_cart_save_rom_png(path) — save the cart as it was loaded.
 *
 * The data that is written is the data area of the cart on disk.
 *
 * 쓰는 데이터는 디스크에 있는 카트의 데이터 영역이다.
 */
int pico8_cart_save_rom_png(const char *path)
{
    return cart_write_p8png(path, g_cart_rom, g_cart_source, g_cart_logo);
}
