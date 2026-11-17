/* video.cpp — gif recording: extcmd("rec") and extcmd("video").
 *
 * Every frame the cart draws is kept, as the 128x128 colour numbers it
 * shows through the screen palette (0..15, and 16..31 for the secret
 * colours 128..143), in a ring of the last 16 seconds (PICO-8's gif_len).
 * extcmd("rec") marks where a recording starts; extcmd("video") writes the
 * frames from that mark, or the last 8 seconds when there is none, as an
 * animated gif at 3x (PICO-8's default gif_scale).
 *
 * The gif has one global colour table of the 32 colours and LZW-compressed
 * frames with a minimum code size of 5.
 *
 * 카트가 그리는 프레임마다 화면 팔레트를 거쳐 보이는 128x128 색 번호(0..15, 숨은
 * 색 128..143은 16..31)로, 최근 16초(PICO-8의 gif_len)를 고리 버퍼에 담는다.
 * extcmd("rec")은 녹화 시작점을 표시하고, extcmd("video")는 그 지점부터(없으면
 * 최근 8초) 프레임을 3배 크기(PICO-8 기본 gif_scale)의 움직이는 gif로 쓴다.
 * gif는 32색 전역 색표 하나와 최소 코드 크기 5의 LZW 압축 프레임을 쓴다.
 */
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "pico8.h"

static const int kFrameBytes = PICO8_SCREEN_W * PICO8_SCREEN_H;
static const int kMaxFrames  = 16 * 60;      /* 16 seconds at 60fps */
static const int kScale      = 3;

static std::vector<uint8_t> g_ring;          /* kMaxFrames frames       */
static std::vector<int>     g_fps;           /* the rate of every frame */
static long g_written = 0;                   /* frames recorded so far  */
static long g_mark    = -1;                  /* extcmd("rec"), or -1    */

/*
 * pico8_video_frame(fps) — keep the frame that is on the screen.
 *
 * Called by the host after every draw of the cart; `fps` is 30 or 60.
 *
 * 화면에 있는 프레임을 보관한다. 호스트가 카트의 draw 뒤마다 부르며 `fps`는
 * 30 또는 60이다.
 */
void pico8_video_frame(int fps)
{
    if (g_ring.empty()) {
        g_ring.assign((size_t)kMaxFrames * kFrameBytes, 0);
        g_fps.assign(kMaxFrames, 30);
    }
    size_t slot = (size_t)(g_written % kMaxFrames);
    uint8_t *out = &g_ring[slot * kFrameBytes];
    for (int i = 0; i < kFrameBytes; i++) {
        uint8_t b = pico8_ram[PICO8_SCREEN + (i >> 1)];
        uint8_t c = (i & 1) ? (b >> 4) : (b & 0x0f);
        uint8_t e = pico8_ram[PICO8_DRAW_STATE + 0x10 + c];
        out[i] = (uint8_t)((e & 0x0f) + ((e & 0x80) ? 16 : 0));
    }
    g_fps[slot] = fps;
    g_written++;
}

/*
 * pico8_video_start() — extcmd("rec"): the recording starts here.
 *
 * extcmd("rec"): 녹화가 여기서 시작한다.
 */
void pico8_video_start(void)
{
    g_mark = g_written;
}

/* ------------------------------------------------------------------ *
 *  The gif writer
 * ------------------------------------------------------------------ */

/*
 * Bits — the bit stream of the LZW codes, cut into sub-blocks of 255 bytes.
 *
 * LZW 코드의 비트 스트림. 255바이트 서브블록으로 자른다.
 */
struct Bits {
    std::string &out;
    std::string block;
    uint32_t acc = 0;
    int nbits = 0;

    explicit Bits(std::string &o) : out(o) {}

    void put(int code, int size)
    {
        acc |= (uint32_t)code << nbits;
        nbits += size;
        while (nbits >= 8) {
            byte((uint8_t)(acc & 0xff));
            acc >>= 8;
            nbits -= 8;
        }
    }
    void byte(uint8_t b)
    {
        block += (char)b;
        if (block.size() == 255) flush();
    }
    void flush()
    {
        if (block.empty()) return;
        out += (char)block.size();
        out += block;
        block.clear();
    }
    void finish()
    {
        if (nbits > 0) byte((uint8_t)(acc & 0xff));
        acc = 0;
        nbits = 0;
        flush();
        out += (char)0;                       /* block terminator */
    }
};

/*
 * lzw(out, pixels, count) — compress one frame (minimum code size 5).
 *
 * The dictionary is a hash of (prefix code, next colour); it starts over
 * with a clear code when it reaches 4096 entries.
 *
 * 한 프레임을 압축한다(최소 코드 크기 5). 사전은 (접두 코드, 다음 색)의
 * 해시이고 4096개에 이르면 clear 코드로 새로 시작한다.
 */
static void lzw(std::string &out, const uint8_t *pixels, size_t count)
{
    const int min_size = 5;
    const int clear = 1 << min_size, eoi = clear + 1;
    std::vector<int> table(1 << 16, -1);        /* key -> code */
    std::vector<int> keys(1 << 16, -1);
    auto slot = [&](int prefix, int c) {
        int key = (prefix << 5) | c;
        size_t h = ((size_t)key * 2654435761u) & 0xffff;
        while (keys[h] != -1 && keys[h] != key) h = (h + 1) & 0xffff;
        return h;
    };

    out += (char)min_size;
    Bits bits(out);
    int size = min_size + 1, next = eoi + 1;
    bits.put(clear, size);
    int prefix = pixels[0];
    for (size_t i = 1; i < count; i++) {
        int c = pixels[i];
        size_t h = slot(prefix, c);
        if (keys[h] != -1) { prefix = table[h]; continue; }
        bits.put(prefix, size);
        if (next < 4096) {
            keys[h] = (prefix << 5) | c;
            table[h] = next++;
            if (next > (1 << size) && size < 12) size++;
        } else {
            bits.put(clear, size);
            std::fill(keys.begin(), keys.end(), -1);
            size = min_size + 1;
            next = eoi + 1;
        }
        prefix = c;
    }
    bits.put(prefix, size);
    bits.put(eoi, size);
    bits.finish();
}

static void le16(std::string &s, int v)
{
    s += (char)(v & 0xff);
    s += (char)((v >> 8) & 0xff);
}

/*
 * pico8_video_save(path) — extcmd("video"): write the recording as a gif.
 *
 * The frames from the extcmd("rec") mark, or the last 8 seconds, at 3x.
 * Returns 0 on success, -1 when there is nothing to write or the file
 * cannot be written.
 *
 * extcmd("video"): 녹화를 gif로 쓴다. extcmd("rec") 표시부터, 없으면 최근
 * 8초의 프레임을 3배로. 성공하면 0, 쓸 것이 없거나 파일을 쓸 수 없으면 -1.
 */
int pico8_video_save(const char *path)
{
    if (g_written == 0) return -1;
    long first = g_mark >= 0 ? g_mark : g_written - 8 * g_fps[(g_written - 1) % kMaxFrames];
    if (first < g_written - kMaxFrames) first = g_written - kMaxFrames;
    if (first < 0) first = 0;
    if (first >= g_written) return -1;
    g_mark = -1;

    const int w = PICO8_SCREEN_W * kScale, h = PICO8_SCREEN_H * kScale;
    std::string out = "GIF89a";
    le16(out, w);
    le16(out, h);
    out += (char)0xf4;                        /* global table of 2^(4+1) colours */
    out += (char)0;                           /* background colour */
    out += (char)0;                           /* aspect */
    for (int i = 0; i < 32; i++) {
        uint32_t rgb = i < 16 ? pico8_palette[i] : pico8_palette_ext[i - 16];
        out += (char)((rgb >> 16) & 0xff);
        out += (char)((rgb >> 8) & 0xff);
        out += (char)(rgb & 0xff);
    }
    /* loop forever (NETSCAPE2.0)  무한 반복 */
    out += "\x21\xff\x0bNETSCAPE2.0\x03\x01";
    le16(out, 0);
    out += (char)0;

    std::vector<uint8_t> big((size_t)w * h);
    double owed = 0.0;                        /* delay rounding, in 1/100 s */
    for (long n = first; n < g_written; n++) {
        size_t slot = (size_t)(n % kMaxFrames);
        const uint8_t *src = &g_ring[slot * kFrameBytes];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                big[(size_t)y * w + x] = src[(y / kScale) * PICO8_SCREEN_W + x / kScale];

        owed += 100.0 / g_fps[slot];
        int delay = (int)owed;
        owed -= delay;

        out += "\x21\xf9\x04";                /* graphic control extension */
        out += (char)0;
        le16(out, delay);
        out += (char)0;
        out += (char)0;
        out += (char)0x2c;                    /* image descriptor */
        le16(out, 0);
        le16(out, 0);
        le16(out, w);
        le16(out, h);
        out += (char)0;
        lzw(out, big.data(), big.size());
    }
    out += (char)0x3b;                        /* trailer */

    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t written = fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return written == out.size() ? 0 : -1;
}
