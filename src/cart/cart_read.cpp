/*
 * cart_read.cpp — reading a cartridge from disk.
 *
 * A .p8 file is a text section with the Lua source and a binary section
 * with the data area; a .p8.png holds the same thing as an image, with the
 * code section compressed.  This file reads both.
 *
 * .p8 파일은 Lua 소스가 있는 텍스트 섹션과 데이터 영역이 있는 바이너리
 * 섹션이다. .p8.png은 같은 내용을 이미지로 담고 코드 섹션이 압축되어 있다.
 * 이 파일은 둘 다 읽는다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <string>
#include <vector>

#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "pico8.h"
#include "util.h"
#include "cart_internal.h"

/*
 * read_whole_file(path, out) — read a file into a string.
 *
 * Reads the whole file in binary mode and puts it into `out`.  The file is
 * opened, its size asked for and then read in one go; a file that cannot
 * be opened gives false.
 *
 * 파일 전체를 바이너리 모드로 읽어 `out`에 넣는다. 파일을 열고 크기를 물은
 * 다음 한 번에 읽는다. 열 수 없는 파일은 false를 준다.
 */
bool read_whole_file(const char *path, std::string &out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return false; }
    out.resize((size_t)sz);
    size_t got = (sz > 0) ? fread(&out[0], 1, (size_t)sz, f) : 0;
    out.resize(got);
    fclose(f);
    return true;
}

/*
 * hex_pair(s, len, pos) — the value of two hex digits.
 *
 * Reads the pair at `pos` and returns the byte they make.  The read is
 * bounds checked: a digit that is past the end of the line counts as 0.
 *
 * `pos`에 있는 두 자리 16진수를 읽어 그 값(바이트)을 돌려준다. 범위를
 * 벗어난 자리는 0으로 센다.
 */
static int hex_pair(const char *s, size_t len, size_t pos)
{
    int a = (pos     < len) ? util_hex_digit((unsigned char)s[pos])     : 0;
    int b = (pos + 1 < len) ? util_hex_digit((unsigned char)s[pos + 1]) : 0;
    return (a << 4) | b;
}

/*
 * hex_single(s, len, pos) — the value of one hex digit.
 *
 * The low nibble of a byte, read out of the file as one character.
 *
 * 바이트의 하위 니블. 파일에서 한 문자로 읽는다.
 */
static int hex_single(const char *s, size_t len, size_t pos)
{
    return (pos < len) ? util_hex_digit((unsigned char)s[pos]) : 0;
}

/*
 * preprocess_includes(dir, code, depth) — expand the #include lines.
 *
 * PICO-8's carts can pull in other files with a line that starts with
 * #include and a file name in quotes.  The file is read and its contents
 * take the place of the line; the directory of the included file becomes
 * the base for the includes inside it.  The nesting goes up to eleven
 * levels deep.
 *
 * PICO-8 카트는 #include로 시작하는 줄과 따옴표로 감싼 파일 이름으로 다른
 * 파일을 끌어올 수 있다. 그 파일을 읽어 그 내용이 줄을 대신하고, 포함된
 * 파일의 디렉터리가 그 안의 include의 기준이 된다. 중첩은 11단계까지
 * 간다.
 */
static std::string preprocess_includes(const std::string &dir,
                                       const std::string &code, int depth)
{
    if (depth >= 11) return code;

    std::string out;
    size_t i = 0;

    while (i < code.size()) {
        size_t nl = code.find('\n', i);
        std::string line = (nl == std::string::npos)
                               ? code.substr(i)
                               : code.substr(i, nl - i);
        i = (nl == std::string::npos) ? code.size() : nl + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        size_t s = 0;
        while (s < line.size() && (line[s] == ' ' || line[s] == '\t')) s++;
        const std::string trimmed = line.substr(s);

        if (trimmed.compare(0, 8, "#include") == 0) {
            size_t k = 8;
            while (k < trimmed.size() && (trimmed[k] == ' ' || trimmed[k] == '\t')) k++;
            std::string name = trimmed.substr(k);
            while (!name.empty() &&
                   (name.back() == ' ' || name.back() == '\t' || name.back() == '\r'))
                name.pop_back();
            if (name.size() >= 2 &&
                ((name.front() == '"' && name.back() == '"') ||
                 (name.front() == '\'' && name.back() == '\'')))
                name = name.substr(1, name.size() - 2);

            if (!name.empty()) {
                std::string full = dir + name;
                std::string inc;
                if (read_whole_file(full.c_str(), inc))
                    out += preprocess_includes(util_path_dir(full), inc, depth + 1);
                continue;
            }
        }

        out += line;
        out += '\n';
    }

    return out;
}

/*
 * parse_sfx_line(line, len, dst) — one __sfx__ line into the RAM.
 *
 * A line of the __sfx__ section holds four header bytes (the editor mode,
 * the speed and the two loop points) and then the 32 notes, five hex
 * digits each: the pitch, the instrument, the volume and the effect.  The
 * notes are stored in the RAM in the packed form that the mixer wants.
 *
 * __sfx__ 섹션의 한 줄에는 헤더 4바이트(편집기 모드, 속도, 두 루프 지점)와
 * 그 뒤 32개 음표가 16진수 다섯 자리씩으로 들어 있다. 각각 피치, 악기, 음량,
 * 효과다. 음표는 믹서가 원하는 압축 형태로 RAM에 저장된다.
 */
static void parse_sfx_line(const char *line, size_t len, uint8_t *dst)
{
    /* Header: 4 bytes from the first 8 hex characters. */
    for (int k = 0; k < 4; k++)
        dst[64 + k] = (uint8_t)hex_pair(line, len, (size_t)k * 2);

    /* 32 notes, five hex characters each. */
    for (int n = 0; n < 32; n++) {
        size_t p = 8 + (size_t)n * 5;
        int pitch      = hex_pair(line, len, p) & 0x3f;
        int instr_hex  = hex_single(line, len, p + 2);
        int custom     = (instr_hex > 7) ? 1 : 0;
        int instrument = instr_hex & 7;
        int volume     = hex_single(line, len, p + 3) & 7;
        int effect     = hex_single(line, len, p + 4) & 0x0f;

        dst[n * 2]     = (uint8_t)(pitch | ((instrument & 3) << 6));
        dst[n * 2 + 1] = (uint8_t)(((effect | (custom << 3)) << 4) |
                                   (volume << 1) |
                                   ((instrument >> 2) & 1));
    }
}

/*
 * parse_music_line(line, len, dst) — one __music__ line into the RAM.
 *
 * A line of the __music__ section reads "FF c0c1c2c3": a byte of flags
 * and then the four patterns of the channels.  The RAM keeps the three
 * flags (stop, repeat, next) in the top bit of the first three channel
 * bytes, and the fourth bit in the top bit of the fourth channel byte.
 *
 * __music__ 섹션의 한 줄은 "FF c0c1c2c3" 꼴이다: 플래그 바이트 하나와 채널
 * 넷의 패턴. RAM에는 세 플래그(stop, repeat, next)가 첫 세 채널 바이트의
 * 최상위 비트에, 네 번째 플래그가 네 번째 채널 바이트의 최상위 비트에 저장된다.
 */
static void parse_music_line(const char *line, size_t len, uint8_t *dst)
{
    int flag = hex_pair(line, len, 0);
    int fnext   = flag & 1;
    int frepeat = (flag >> 1) & 1;
    int fstop   = (flag >> 2) & 1;
    int mode    = (flag >> 3) & 1;

    int ch[4];
    for (int k = 0; k < 4; k++)
        ch[k] = hex_pair(line, len, 3 + (size_t)k * 2);

    dst[0] = (uint8_t)(ch[0] | (fnext   << 7));
    dst[1] = (uint8_t)(ch[1] | (frepeat << 7));
    dst[2] = (uint8_t)(ch[2] | (fstop   << 7));
    dst[3] = (uint8_t)(ch[3] | (mode    << 7));
}

/*
 * check_source_limits(source) — is the source too big?
 *
 * PICO-8 checks the Lua source when a cart is loaded: more than 65536
 * bytes and the load fails outright, more than 8192 tokens and the cart
 * is loaded but never run.  The message for the second case is printed
 * here, exactly like the oracle prints it.
 *
 *   -1  소스가 너무 크다(불러오기 실패)
 *    0  실행해도 된다
 *    1  토큰이 너무 많다(불러왔지만 실행하지 않는다)
 *
 * 카트를 불러올 때 PICO-8은 Lua 소스를 검사한다. 65536바이트를 넘으면
 * 불러오기가 아예 실패하고, 8192 토큰을 넘으면 카트는 불러오되 실행하지
 * 않는다. 두 번째 경우의 메시지는 오라클과 똑같이 여기서 출력한다.
 */
static int check_source_limits(const std::string &source)
{
    if (source.size() > PICO8_SOURCE_LIMIT)
        return -1;

    int tokens = pico8_count_tokens(source.data(), source.size());
    if (tokens > PICO8_TOKEN_LIMIT) {
        printf("program too large\n%d / %d tokens\n\n",
               tokens, PICO8_TOKEN_LIMIT);
        fflush(stdout);
        return 1;
    }
    return 0;
}

/*
 * parse_p8_text(text, textlen, cart_data, code) — read a .p8 file.
 *
 * Goes over the file line by line and follows the sections: __lua__ is the
 * source, __gfx__ the sprite sheet, __gff__ the graphics flags, __map__ the
 * map, __sfx__ the sound effects, __music__ the songs and __label__ the
 * name of the cart.  The data ends up in `cart_data` (a 0x4300 byte image
 * of the RAM) and the source in `code`.
 *
 * 파일을 한 줄씩 훑으며 섹션을 따라간다. __lua__는 소스, __gfx__는 스프라이트
 * 시트, __gff__는 그래픽 플래그, __map__는 맵, __sfx__는 효과음, __music__는
 * song, __label__는 카트 이름이다. 데이터는 `cart_data`(RAM의 0x4300바이트
 * 이미지)에, 소스는 `code`에 들어간다.
 *
 * The songs start out at the "no pattern" marker 0x41,0x42,0x43,0x44, so
 * that a cart without a __music__ section reports the same values as it
 * does on a real PICO-8.
 *
 * song은 "패턴 없음" 표지 0x41,0x42,0x43,0x44로 시작한다. 그래서
 * __music__ 섹션이 없는 카트는 실제 PICO-8과 같은 값을 보인다.
 */
int parse_p8_text(const char *text, size_t textlen,
                         uint8_t *cart_data, std::string &code)
{
    enum { SEC_NONE, SEC_LUA, SEC_GFX, SEC_GFF, SEC_MAP, SEC_SFX, SEC_MUSIC, SEC_LABEL };

    /* Songs default to the "no pattern" marker 0x41,0x42,0x43,0x44 so a
       cart without a __music__ section reports the same values as PICO-8. */
    for (int s = 0; s < 64; s++)
        for (int c = 0; c < 4; c++)
            cart_data[PICO8_SONG_ADDR + s * CART_SONG_SIZE + c] = (uint8_t)(0x41 + c);

    int section = SEC_NONE;
    size_t gfx_off = 0, map_off = 0, gff_off = 0;
    int sfx_i = 0, mus_i = 0;

    size_t i = 0;

    while (i < textlen) {
        size_t nl = i;
        while (nl < textlen && text[nl] != '\n') nl++;

        /* Lines are kept whole: PICO-8 accepts lines longer than any fixed
           buffer (a 65520-character comment line still loads). */
        std::string line(text + i, nl - i);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        const char *l = line.c_str();
        size_t len = line.size();
        i = (nl < textlen) ? nl + 1 : textlen;

        if (len >= 2 && l[0] == '_' && l[1] == '_') {
            if      (!strncmp(l, "__lua__",   7)) section = SEC_LUA;
            else if (!strncmp(l, "__gfx__",   7)) section = SEC_GFX;
            else if (!strncmp(l, "__gff__",   7)) section = SEC_GFF;
            else if (!strncmp(l, "__map__",   7)) section = SEC_MAP;
            else if (!strncmp(l, "__sfx__",   7)) section = SEC_SFX;
            else if (!strncmp(l, "__music__", 9)) section = SEC_MUSIC;
            else if (!strncmp(l, "__label__", 9)) section = SEC_LABEL;
            else                                     section = SEC_NONE;
            continue;
        }

        if (!strncmp(l, "pico-8 cartridge", 16)) continue;
        if (!strncmp(l, "version ", 8)) continue;

        switch (section) {
        case SEC_LUA:
            code += line;
            code += '\n';
            break;

        case SEC_GFX:
            /* Pairs of hex digits; the *second* digit is the high nibble
               (left pixel is stored in the low nibble). */
            for (size_t k = 0; k + 1 < len; k += 2) {
                if (l[k] <= ' ' || l[k + 1] <= ' ') break;
                if (gfx_off >= 0x2000) break;
                cart_data[PICO8_GFX_ADDR + gfx_off++] =
                    (uint8_t)(util_hex_digit((unsigned char)l[k + 1]) * 16 +
                              util_hex_digit((unsigned char)l[k]));
            }
            break;

        case SEC_GFF:
            for (size_t k = 0; k + 1 < len; k += 2) {
                if (l[k] <= ' ' || l[k + 1] <= ' ') break;
                if (gff_off >= 0x100) break;
                cart_data[PICO8_GFX_FLAGS + gff_off++] =
                    (uint8_t)hex_pair(l, len, k);
            }
            break;

        case SEC_MAP:
            for (size_t k = 0; k + 1 < len; k += 2) {
                if (l[k] <= ' ' || l[k + 1] <= ' ') break;
                if (map_off >= 0x1000) break;
                cart_data[PICO8_MAP_ADDR + map_off++] =
                    (uint8_t)hex_pair(l, len, k);
            }
            break;

        case SEC_SFX:
            if (sfx_i < 64)
                parse_sfx_line(l, len,
                               cart_data + PICO8_SFX_ADDR + sfx_i * CART_SFX_SIZE);
            sfx_i++;
            break;

        case SEC_MUSIC:
            if (mus_i < 64)
                parse_music_line(l, len,
                                 cart_data + PICO8_SONG_ADDR + mus_i * CART_SONG_SIZE);
            mus_i++;
            break;

        case SEC_LABEL:
        default:
            break;
        }
    }

    return 0;
}

/*
 * load_p8png(path) — read a .p8.png file.
 *
 * The picture carries the cart: the first 0x4300 pixels are the data area,
 * the code follows at 0x4300 (compressed with PXA or with the old ":c:"
 * compressor, or as plain text) and 32 bytes of metadata sit at 0x8000.
 *
 * The image is decoded, the two low bits of every colour channel are put
 * back together into the byte of the cart, the RAM is filled with the data
 * area and the source is handed to the VM.
 *
 * 그림 자체가 카트다. 첫 0x4300 픽셀이 데이터 영역이고, 0x4300부터 코드가
 * 온다(PXA 또는 옛 ":c:" 압축, 또는 평문). 0x8000에는 32바이트의 메타데이터가
 * 있다.
 *
 * 이미지를 해독하고 색 채널의 하위 두 비트를 카트의 바이트로 다시 합친 뒤,
 * RAM을 데이터 영역으로 채우고 소스를 VM에 넘긴다.
 */
static int load_p8png(const char *path)
{
    int w = 0, h = 0, comp = 0;
    unsigned char *img = stbi_load(path, &w, &h, &comp, 4);
    if (!img) return -1;

    size_t num_pixels = (size_t)w * (size_t)h;
    if (num_pixels < 0x8001) {
        stbi_image_free(img);
        return -1;
    }

    std::vector<uint8_t> data(num_pixels);
    for (size_t p = 0; p < num_pixels; p++) {
        uint8_t r = img[p * 4 + 0] & 0x03;
        uint8_t g = img[p * 4 + 1] & 0x03;
        uint8_t b = img[p * 4 + 2] & 0x03;
        uint8_t a = img[p * 4 + 3] & 0x03;
        data[p] = (uint8_t)((b) | (g << 2) | (r << 4) | (a << 6));
    }
    stbi_image_free(img);

    pico8_reset_memory();
    size_t ram = num_pixels < PICO8_CART_DATA_SIZE ? num_pixels : PICO8_CART_DATA_SIZE;
    memcpy(pico8_ram, data.data(), ram);

    if (num_pixels <= 0x4300) return -1;

    const uint8_t *codearea = data.data() + 0x4300;
    size_t codeavail = num_pixels - 0x4300;
    std::vector<uint8_t> out(0x10000);
    int len = 0;

    if (codearea[0] == ':' && codearea[1] == 'c' && codearea[2] == ':') {
        len = decompress_legacy(codearea, out.data(), 0x10000);
    } else if (codearea[0] == 0 && codearea[1] == 'p' &&
               codearea[2] == 'x' && codearea[3] == 'a') {
        len = decompress_pxa(codearea, out.data(), 0x10000);
    } else {
        while ((size_t)len < codeavail && codearea[len]) {
            out[(size_t)len] = codearea[len];
            len++;
        }
    }

    if (len <= 0) return -1;
    out[(size_t)len] = 0;

    /* Preserve the 32 metadata bytes past the code area (version stamp and
       optional SHA1) so a save() keeps them. */
    memset(g_cart_logo, 0, sizeof(g_cart_logo));
    if (num_pixels > 0x8000) {
        size_t n = num_pixels - 0x8000;
        if (n > sizeof(g_cart_logo)) n = sizeof(g_cart_logo);
        memcpy(g_cart_logo, data.data() + 0x8000, n);
    }

    std::string raw((char *)out.data(), (size_t)len);
    std::string full = preprocess_includes(util_path_dir(path), raw, 0);

    /* UTF-8 glyph text -> charset bytes, the way the oracle stores text
       internally. Includes go through the same pass over the final text. */
    full = pico8_charset_from_utf8(full.data(), full.size());

    g_cart_source = pico8_charset_from_utf8(raw.data(), raw.size());
    g_cart_path = path ? path : "";
    cart_snapshot_rom();

    int limit = check_source_limits(full);
    if (limit < 0) return -1;               /* caller reports the failure */
    if (limit > 0) return 0;                /* loaded, but never run */

    if (pico8_vm_load_buffer(full.data(), full.size(), path) != 0)
        return -1;

    return 0;
}

/*
 * load_p8_text(path, text) — load a .p8 file.
 *
 * Parses the text, resets the machine, copies the data area into the RAM
 * and hands the source to the VM.  When the source is too big the cart is
 * loaded but not run.
 *
 * 텍스트를 파싱하고, 기계를 초기화하고, 데이터 영역을 RAM에 복사하고, 소스를
 * VM에 넘긴다. 소스가 너무 크면 카트는 불러오되 실행하지 않는다.
 */
static int load_p8_text(const char *path, const std::string &text)
{
    uint8_t cart_data[PICO8_CART_DATA_SIZE];
    memset(cart_data, 0, sizeof(cart_data));

    std::string raw;
    parse_p8_text(text.data(), text.size(), cart_data, raw);

    pico8_reset_memory();
    memcpy(pico8_ram, cart_data, PICO8_CART_DATA_SIZE);

    g_cart_source = pico8_charset_from_utf8(raw.data(), raw.size());
    g_cart_path = path ? path : "";
    cart_snapshot_rom();

    std::string full = preprocess_includes(util_path_dir(path), raw, 0);
    full = pico8_charset_from_utf8(full.data(), full.size());

    int limit = check_source_limits(full);
    if (limit < 0) return -1;               /* caller reports the failure */
    if (limit > 0) return 0;                /* loaded, but never run */

    if (pico8_vm_load_buffer(full.data(), full.size(), path) != 0)
        return -1;

    return 0;
}

/*
 * pico8_load_cart(path) — load a cart from a file.
 *
 * Looks at the first bytes of the file: a PNG signature means a .p8.png
 * cart, anything else is read as the text of a .p8 cart.
 *
 * 파일이 처음 몇 바이트인지 본다. PNG 시그니처면 .p8.png 카트이고, 그 외에는
 * .p8 카트의 텍스트로 읽는다.
 */
int pico8_load_cart(const char *path)
{
    if (!path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    unsigned char magic[8] = {0};
    size_t n = fread(magic, 1, sizeof(magic), f);
    fclose(f);

    if (n >= 8 && magic[0] == 0x89 && magic[1] == 'P' &&
        magic[2] == 'N' && magic[3] == 'G')
        return load_p8png(path);

    std::string text;
    if (!read_whole_file(path, text))
        return -1;

    /* A text cart always begins with the "pico-8 cartridge" header. If it
       does not, fall back to treating the whole buffer as source anyway. */
    return load_p8_text(path, text);
}

/*
 * pico8_run_cart(path) — load a cart and start it.
 *
 * Loads the cart and calls its _init, so that a cart that is run from the
 * command line begins the same way it would in the PICO-8.
 *
 * 카트를 불러오고 _init을 부른다. 그래서 커맨드 라인에서 실행한 카트가
 * PICO-8에서 실행될 때와 똑같이 시작한다.
 */
int pico8_run_cart(const char *path)
{
    if (pico8_load_cart(path) != 0)
        return -1;
    pico8_vm_call_init();
    return 0;
}

/*
 * pico8_cart_read_data(path, out, out_len) — the data area of a cart file.
 *
 * Reads only the data area of a cart, either .p8 or .p8.png, without
 * touching the RAM or the VM.  This is what reload() uses when it is given
 * a file name of its own.
 *
 * 카트 파일(.p8 또는 .p8.png)의 데이터 영역만 읽는다. RAM이나 VM은 건드리지
 * 않는다. reload()이 파일 이름을 따로 받았을 때 쓰는 것이 이것이다.
 */
int pico8_cart_read_data(const char *path, uint8_t *out, size_t out_len)
{
    if (!path || !out || out_len < PICO8_CART_DATA_SIZE) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char magic[8] = {0};
    size_t n = fread(magic, 1, sizeof(magic), f);
    fclose(f);

    if (n >= 8 && magic[0] == 0x89 && magic[1] == 'P' &&
        magic[2] == 'N' && magic[3] == 'G') {
        int w = 0, h = 0, comp = 0;
        unsigned char *img = stbi_load(path, &w, &h, &comp, 4);
        if (!img) return -1;
        size_t num_pixels = (size_t)w * (size_t)h;
        if (num_pixels < PICO8_CART_DATA_SIZE) { stbi_image_free(img); return -1; }
        memset(out, 0, PICO8_CART_DATA_SIZE);
        for (size_t p = 0; p < PICO8_CART_DATA_SIZE; p++) {
            uint8_t r = img[p * 4 + 0] & 0x03;
            uint8_t g = img[p * 4 + 1] & 0x03;
            uint8_t b = img[p * 4 + 2] & 0x03;
            uint8_t a = img[p * 4 + 3] & 0x03;
            out[p] = (uint8_t)(b | (g << 2) | (r << 4) | (a << 6));
        }
        stbi_image_free(img);
        return 0;
    }

    std::string text;
    if (!read_whole_file(path, text)) return -1;
    std::string code;
    memset(out, 0, PICO8_CART_DATA_SIZE);
    parse_p8_text(text.data(), text.size(), out, code);
    return 0;
}
