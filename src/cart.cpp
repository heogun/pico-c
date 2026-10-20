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

static inline int hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

/* Read the hex pair at `pos` (bounds-checked, missing digits read as zero). */
static int hex_pair(const char *s, size_t len, size_t pos)
{
    int a = (pos     < len) ? hex_digit((unsigned char)s[pos])     : 0;
    int b = (pos + 1 < len) ? hex_digit((unsigned char)s[pos + 1]) : 0;
    return (a << 4) | b;
}

static int hex_single(const char *s, size_t len, size_t pos)
{
    return (pos < len) ? hex_digit((unsigned char)s[pos]) : 0;
}

/* Directory part of a path: everything up to and including the last
   '/' or '\\', or "" when the path has no separator. */
static std::string get_dir(const std::string &path)
{
    size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return std::string();
    return path.substr(0, p + 1);
}

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

/* Decompress a legacy ":c:" block (src points at the ":c:\0" header).
   Returns the length of the produced text (trailing NULs removed). */
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

/* Move-to-front table used by the PXA decompressor. */
struct move_to_front {
    uint8_t state[256];
    move_to_front() { for (int i = 0; i < 256; i++) state[i] = (uint8_t)i; }
    uint8_t get(int n)
    {
        uint8_t ch = state[n];
        for (int i = n; i > 0; i--) state[i] = state[i - 1];
        state[0] = ch;
        return ch;
    }
};

/* Decompress a PXA block (src points at the "\0pxa" header). */
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

/* Decode one __sfx__ line into the 68-byte RAM representation. */
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

/* Decode one __music__ line ("FF c0c1c2c3") into 4 RAM bytes. The flag
   bits are folded into the top bit of each channel byte. */
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

/* PICO-8 checks the Lua source when a cart is loaded: more than 65536 bytes
   and the load fails outright, more than 8192 tokens and the cart is loaded
   but never run.  Returns -1 when the source is too large, 1 when it has too
   many tokens (the message is printed here, exactly like the oracle) and 0
   when the cart may run. */
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

/* Parse a complete .p8 text cart. Fills `cart_data` (a 0x4300 RAM image)
   and appends the Lua source to `code`. Returns 0 on success. */
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

void pico8_cart_set_source(const char *src, size_t len)
{
    if (src) g_cart_source.assign(src, len);
    else     g_cart_source.clear();
}

const char *pico8_cart_source(size_t *len)
{
    if (len) *len = g_cart_source.size();
    return g_cart_source.c_str();
}

void pico8_cart_set_path(const char *path) { g_cart_path = path ? path : ""; }
const char *pico8_cart_get_path(void)      { return g_cart_path.c_str(); }
const uint8_t *pico8_cart_rom(void)        { return g_cart_rom; }

static void put_hex_byte(std::string &out, uint8_t b)
{
    out += kHexDigits[b >> 4];
    out += kHexDigits[b & 0x0f];
}

/* The sprite sheet / map / sound banks compare against an all-zero default;
   sound banks additionally carry a note duration that PICO-8 seeds per bank. */
static bool gfx_line_is_default(const uint8_t *d, int line)
{
    const uint8_t *p = d + PICO8_GFX_ADDR + (size_t)line * 64;
    for (int i = 0; i < 64; i++) if (p[i]) return false;
    return true;
}

static bool gff_is_empty(const uint8_t *d)
{
    for (int i = 0; i < 0x100; i++) if (d[PICO8_GFX_FLAGS + i]) return false;
    return true;
}

static bool map_line_is_default(const uint8_t *d, int line)
{
    const uint8_t *p = d + PICO8_MAP_ADDR + (size_t)line * 128;
    for (int i = 0; i < 128; i++) if (p[i]) return false;
    return true;
}

static bool sfx_line_is_default(const uint8_t *d, int i)
{
    const uint8_t *p = d + PICO8_SFX_ADDR + (size_t)i * CART_SFX_SIZE;
    for (int n = 0; n < 64; n++) if (p[n]) return false;     /* the 32 notes */
    int editor = p[64], speed = p[65], loop0 = p[66], loop1 = p[67];
    int def_speed = (i == 0) ? 1 : 16;                       /* PICO-8 default */
    return editor == 0 && loop0 == 0 && loop1 == 0 &&
           (speed == 0 || speed == def_speed);
}

static bool music_line_is_default(const uint8_t *d, int i)
{
    const uint8_t *p = d + PICO8_SONG_ADDR + (size_t)i * CART_SONG_SIZE;
    return p[0] == 0x41 && p[1] == 0x42 && p[2] == 0x43 && p[3] == 0x44;
}

/* Serialize `d` (a 0x4300-byte data area) and `code` into PICO-8's .p8 text
   format. Sections that only contain default data -- and the trailing default
   lines of the others -- are omitted, exactly like PICO-8's own save. */
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

int pico8_cart_save_p8(const char *path)
{
    return cart_write_p8(path, pico8_ram, g_cart_source, kCartVersion);
}

int pico8_cart_save_rom_p8(const char *path)
{
    return cart_write_p8(path, g_cart_rom, g_cart_source, kCartVersion);
}

/* ------------------------------------------------------------------ *
 *  .p8.png serialization
 * ------------------------------------------------------------------ */

/* Small PNG encoder (8-bit RGBA, one IDAT). PICO-8 writes its carts with
   LodePNG and tags them with a "LodePNG 20110221" text chunk; the image
   payload is reproduced byte for byte by fixing the LSBs of each channel,
   so we emit the same chunk layout for tooling that looks for it. */
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

static void png_be32(std::string &s, uint32_t v)
{
    s += (char)((v >> 24) & 0xff);
    s += (char)((v >> 16) & 0xff);
    s += (char)((v >> 8) & 0xff);
    s += (char)(v & 0xff);
}

static void png_chunk(std::string &out, const char *type, const std::string &body)
{
    png_be32(out, (uint32_t)body.size());
    std::string tagged = std::string(type, 4) + body;
    out += tagged;
    png_be32(out, png_crc32((const uint8_t *)tagged.data(), tagged.size()));
}

/* Deflate the filtered image with stored (uncompressed) blocks: valid zlib,
   no dependency on a DEFLATE implementation. */
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

/* Byte `v` for pixel `p` is spread over the low two bits of every channel:
   A holds the top bits, then R, G and finally B. */
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

int pico8_cart_save_png(const char *path)
{
    return cart_write_p8png(path, pico8_ram, g_cart_source, g_cart_logo);
}

int pico8_cart_save_rom_png(const char *path)
{
    return cart_write_p8png(path, g_cart_rom, g_cart_source, g_cart_logo);
}

/* Read just the data area of a cartridge (either .p8 or .p8.png) without
   touching RAM or the VM; used by reload() with an explicit filename. */
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

/* RELOAD([DEST], [SOURCE], [LEN], [FILENAME])
   Copy LEN bytes from the cart's on-disk data area into RAM. */
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

/* CSTORE([DEST], [SOURCE], [LEN], [FILENAME])
   Copy LEN bytes from RAM into the cart's stored data area and, when the
   cart is backed by a file, persist it. */
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
