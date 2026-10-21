/*
 * cart.cpp — the cartridge files on disk.
 *
 * A PICO-8 cartridge is a .p8 file: a text section with the Lua source
 * and a binary section with 0x4300 bytes of data.  The same content can
 * also live in a .p8.png image, where the code section is compressed with
 * PXA.  This file reads both and writes both.
 *
 * PICO-8 카트리지는 .p8 파일이다: 텍스트 섹션에 Lua 소스가, 바이너리 섹션에
 * 0x4300 바이트의 데이터가 있다. 같은 내용을 .p8.png 이미지로도 담을 수 있고,
 * 그때는 코드 섹션이 PXA로 압축된다. 이 파일은 둘 다 읽고 둘 다 쓴다.
 *
 * The data section is a mixture of several things: the sprite sheet, the
 * map, the graphics flags, the songs and the sound effects, and the data
 * the cart keeps for itself.  The writer compares every part with its
 * default and leaves the parts that did not change out of the file, so a
 * saved cart stays as small as the real PICO-8 keeps it.
 *
 * 데이터 섹션은 여러 가지가 섞여 있다: 스프라이트 시트, 맵, 그래픽 플래그,
 * song과 효과음, 그리고 카트가 자기 몫으로 두는 데이터. 쓰는 쪽은 각 부분을
 * 기본값과 비교해 바뀐 부분만 파일에 남기므로, 저장한 카트가 실제 PICO-8이
 * 만드는 만큼 작게 유지된다.
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
#include "pxa.h"

/* Sizes that describe the compiled sprite/audio banks inside the cartridge
   image. They mirror the defines used by audio.cpp. */
#define CART_SFX_SIZE   68
#define CART_SONG_SIZE  4

/* ------------------------------------------------------------------ *
 *  Small helpers
 * ------------------------------------------------------------------ */

/*
 * hex_digit(c) — the value of one hex digit.
 *
 * Turns '0'..'9', 'a'..'f' and 'A'..'F' into 0..15.  Anything else is
 * read as 0, so a broken file does not crash the loader.
 *
 * '0'~'9', 'a'~'f', 'A'~'F'를 0~15로 바꾼다. 그 외의 문자는 0으로 읽는다.
 * 망가진 파일이 로더를 죽이지 않게 하기 위해서다.
 */
static inline int hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
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
    int a = (pos     < len) ? hex_digit((unsigned char)s[pos])     : 0;
    int b = (pos + 1 < len) ? hex_digit((unsigned char)s[pos + 1]) : 0;
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
    return (pos < len) ? hex_digit((unsigned char)s[pos]) : 0;
}

/*
 * get_dir(path) — the directory a file lives in.
 *
 * Everything up to and including the last '/' or '\\'.  A path without a
 * separator has no directory, so the result is empty.
 *
 * 마지막 '/' 또는 '\\'까지의 부분. 구분자가 없는 경로에는 디렉터리가 없으므로
 * 결과가 비어 있다.
 */
static std::string get_dir(const std::string &path)
{
    size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return std::string();
    return path.substr(0, p + 1);
}

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
static bool read_whole_file(const char *path, std::string &out)
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

/* ------------------------------------------------------------------ *
 *  Cartridge storage state (used by save / load / reload / cstore)
 * ------------------------------------------------------------------ */

/* Raw Lua source of the currently loaded cart (comments and formatting are
   preserved so that saving round-trips the editor contents). */
static std::string g_cart_source;
/* Path of the currently loaded cartridge. */
static std::string g_cart_path;
/* Cart format version written to the "version N" header line. Real PICO-8
   always writes its own current version rather than the one it loaded: a
   cstore() issued from a v41 cart produced a v43 file on the oracle (0.2.7),
   and our writer output is byte-identical to the oracle's for that cart. */
static const int   kCartVersion = 43;
/* The cart's data area (0x0000..0x42ff) exactly as it was loaded from disk.
   reload() restores from this and cstore() updates it before saving. */
static uint8_t     g_cart_rom[PICO8_CART_DATA_SIZE];
/* The 32 metadata bytes that live past the code area in a .p8.png cart
   (0x8000..0x801f). BBS carts carry all zeroes; editor saves carry a version
   stamp and a SHA1 of the preceding 0x8000 bytes. Preserved so a loaded
   cart's stamp is written back unchanged. */
static uint8_t     g_cart_logo[32];

/*
 * cart_snapshot_rom() — remember the data area of the cart.
 *
 * Takes a copy of the 0x4300 bytes of the data area as they are right
 * after the cart was loaded.  reload() copies them back into the RAM, so
 * this is the picture of the cart on disk and not of the running cart.
 *
 * 카트를 방금 불러온 직후의 데이터 영역 0x4300바이트를 복사해 둔다.
 * reload()이 이걸 RAM에 다시 복사하므로, 이건 디스크에 있는 카트의 모습이지
 * 실행 중인 카트의 모습이 아니다.
 */
static void cart_snapshot_rom(void)
{
    memcpy(g_cart_rom, pico8_ram, PICO8_CART_DATA_SIZE);
}

/* ------------------------------------------------------------------ *
 *  Code compression
 * ------------------------------------------------------------------ */

/* Lookup table used by the legacy ":c:" compressor. Note the first entry
   is a literal newline and the second a space. */
static const char legacy_lut[] =
    "\n 0123456789abcdefghijklmnopqrstuvwxyz!#%(){}[]<>+=/*:;.,~_";

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
static int decompress_legacy(const uint8_t *src, uint8_t *dest, uint32_t maxlen)
{
    uint32_t length = ((uint32_t)src[4] << 8) | src[5];
    uint32_t out = 0;

    for (uint32_t i = 8; i < 0x3d00 && out < length; i++) {
        if (out + 1 >= maxlen) break;

        if (src[i] >= 0x3c) {
            uint32_t a = (uint32_t)(src[i] - 0x3c) * 16 + (src[i + 1] & 0x0f);
            uint32_t b = (src[i + 1] >> 4) + 2;
            if (out >= a) {
                while (b-- && out + 1 < maxlen)
                    dest[out++] = dest[out - a];
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
static int decompress_pxa(const uint8_t *src, uint8_t *dest, uint32_t maxlen)
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

/* ------------------------------------------------------------------ *
 *  #include preprocessing
 * ------------------------------------------------------------------ */

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
                    out += preprocess_includes(get_dir(full), inc, depth + 1);
                continue;
            }
        }

        out += line;
        out += '\n';
    }

    return out;
}

/* ------------------------------------------------------------------ *
 *  .p8 (text) parsing
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ *
 *  Cartridge size limits (matched against the oracle)
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ *
 *  .p8 text parsing
 * ------------------------------------------------------------------ */

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
static int parse_p8_text(const char *text, size_t textlen,
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
                    (uint8_t)(hex_digit((unsigned char)l[k + 1]) * 16 +
                              hex_digit((unsigned char)l[k]));
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

/* ------------------------------------------------------------------ *
 *  .p8.png parsing
 * ------------------------------------------------------------------ */

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
    std::string full = preprocess_includes(get_dir(path), raw, 0);

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

/* ------------------------------------------------------------------ *
 *  Public entry points
 * ------------------------------------------------------------------ */

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

    std::string full = preprocess_includes(get_dir(path), raw, 0);
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

/* ------------------------------------------------------------------ *
 *  Cartridge writing (.p8 serialization)
 * ------------------------------------------------------------------ */

static const char kHexDigits[] = "0123456789abcdef";

/*
 * pico8_cart_set_source(src, len) — the source of the cart.
 *
 * Keeps the raw Lua source of the loaded cart.  A NULL pointer clears it.
 *
 * 불러온 카트의 원본 Lua 소스를 보관한다. NULL을 주면 지운다.
 */
void pico8_cart_set_source(const char *src, size_t len)
{
    if (src) g_cart_source.assign(src, len);
    else     g_cart_source.clear();
}

/*
 * pico8_cart_source(len) — the source of the cart.
 *
 * Returns the source that was kept, and writes its length into `len` when
 * that pointer is given.
 *
 * 보관해 둔 소스를 돌려준다. `len`이 주어지면 그 길이를 써 넣는다.
 */
const char *pico8_cart_source(size_t *len)
{
    if (len) *len = g_cart_source.size();
    return g_cart_source.c_str();
}

/*
 * pico8_cart_set_path(path) — where the cart came from.
 *
 * The path of the loaded cart, used when a cart is saved without a name
 * of its own.  A NULL pointer empties it.
 *
 * 불러온 카트의 경로. 이름 없이 카트를 저장할 때 쓰인다. NULL을 주면 비운다.
 */
void pico8_cart_set_path(const char *path) { g_cart_path = path ? path : ""; }

/*
 * pico8_cart_get_path() — where the cart came from.
 *
 * 불러온 카트의 경로를 돌려준다.
 */
const char *pico8_cart_get_path(void)      { return g_cart_path.c_str(); }

/*
 * pico8_cart_rom() — the data area of the cart.
 *
 * The 0x4300 bytes of the cart as they were loaded from disk.  reload()
 * copies from here and cstore() writes back into it.
 *
 * 디스크에서 불러온 그대로의 카트 데이터 영역 0x4300바이트. reload()은
 * 여기서 복사하고, cstore()는 여기로 쓴다.
 */
const uint8_t *pico8_cart_rom(void)        { return g_cart_rom; }

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
static int cart_write_p8(const char *path, const uint8_t *d,
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

/* ------------------------------------------------------------------ *
 *  .p8.png serialization
 * ------------------------------------------------------------------ */

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
static int cart_write_p8png(const char *path, const uint8_t *rom,
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

/*
 * pico8_cart_reload(dest, source, len, filename) — copy data back into the RAM.
 *
 * Copies `len` bytes from the data area of the cart into the RAM at
 * `dest`.  Without a file name the data comes from the cart as it was
 * loaded from disk, with one it comes from that file.  The copy is cut
 * short at the end of both areas.
 *
 * `len`바이트를 카트의 데이터 영역에서 RAM의 `dest`로 복사한다. 파일 이름이
 * 없으면 디스크에서 불러온 카트에서, 있으면 그 파일에서 가져온다. 복사는 두
 * 영역의 끝에서 잘린다.
 */
int pico8_cart_reload(int dest, int source, int len, const char *filename)
{
    if (len <= 0) len = PICO8_CART_DATA_SIZE;
    if (dest < 0) dest = 0;
    if (source < 0) source = 0;

    const uint8_t *src = g_cart_rom;
    uint8_t tmp[PICO8_CART_DATA_SIZE];
    if (filename && filename[0]) {
        if (pico8_cart_read_data(filename, tmp, sizeof(tmp)) != 0)
            return -1;
        src = tmp;
    }

    if (source >= PICO8_CART_DATA_SIZE) return 0;
    if (dest >= PICO8_RAM_SIZE) return 0;
    if (source + len > PICO8_CART_DATA_SIZE) len = PICO8_CART_DATA_SIZE - source;
    if (dest + len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE - dest;
    if (len <= 0) return 0;

    memmove(pico8_ram + dest, src + source, (size_t)len);
    return len;
}

/*
 * pico8_cart_cstore(dest, source, len, filename) — copy data into the cart.
 *
 * Copies `len` bytes from the RAM into the data area of the cart at
 * `dest`.  When the cart is backed by a file the file is written again,
 * in the format its name asks for; otherwise the change only stays in
 * memory until the next save.
 *
 * `len`바이트를 RAM에서 카트의 데이터 영역 `dest`로 복사한다. 카트가 파일로
 * 뒷받침되어 있으면 그 파일을 다시 쓴다. 파일 이름이 요구하는 형식으로.
 * 그렇지 않으면 다음 저장 때까지 메모리에만 남는다.
 */
int pico8_cart_cstore(int dest, int source, int len, const char *filename)
{
    if (len <= 0) len = PICO8_CART_DATA_SIZE;
    if (dest < 0) dest = 0;
    if (source < 0) source = 0;

    if (source >= PICO8_RAM_SIZE) return 0;
    if (dest >= PICO8_CART_DATA_SIZE) return 0;
    if (source + len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE - source;
    if (dest + len > PICO8_CART_DATA_SIZE) len = PICO8_CART_DATA_SIZE - dest;
    if (len <= 0) return 0;

    memmove(g_cart_rom + dest, pico8_ram + source, (size_t)len);

    std::string out = (filename && filename[0]) ? std::string(filename) : g_cart_path;
    if (out.empty()) return len;

    if (out.size() < 3 ||
        (out.compare(out.size() - 3, 3, ".p8") != 0 &&
         out.compare(out.size() - 4, 4, ".png") != 0))
        out += ".p8";

    if (out.compare(out.size() - 4, 4, ".png") == 0)
        cart_write_p8png(out.c_str(), g_cart_rom, g_cart_source, g_cart_logo);
    else
        cart_write_p8(out.c_str(), g_cart_rom, g_cart_source, kCartVersion);
    return len;
}
