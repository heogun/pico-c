/*
 * cart_state.cpp — the cartridge that is loaded, and where it came from.
 *
 * The loader keeps the source of the cart, its path, the data area it was
 * read from and the 32 metadata bytes that follow the code area in a
 * .p8.png.  The helpers that hand that state to the rest of the program
 * live here.
 *
 * 불러온 카트를 기억한다: 소스, 경로, 디스크에서 읽은 데이터 영역, 그리고
 * .p8.png에서 코드 영역 뒤에 오는 32바이트의 메타데이터. 그 상태를 나머지
 * 프로그램에 건네는 헬퍼도 여기에 있다.
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

/* Raw Lua source of the currently loaded cart (comments and formatting are
   preserved so that saving round-trips the editor contents). */
std::string g_cart_source;
/* Path of the currently loaded cartridge. */
std::string g_cart_path;
/* Cart format version written to the "version N" header line. Real PICO-8
   always writes its own current version rather than the one it loaded: a
   cstore() issued from a v41 cart produced a v43 file on the oracle (0.2.7),
   and our writer output is byte-identical to the oracle's for that cart. */
const int   kCartVersion = 43;
/* The cart's data area (0x0000..0x42ff) exactly as it was loaded from disk.
   reload() restores from this and cstore() updates it before saving. */
uint8_t     g_cart_rom[PICO8_CART_DATA_SIZE];
/* The 32 metadata bytes that live past the code area in a .p8.png cart
   (0x8000..0x801f). BBS carts carry all zeroes; editor saves carry a version
   stamp and a SHA1 of the preceding 0x8000 bytes. Preserved so a loaded
   cart's stamp is written back unchanged. */
uint8_t     g_cart_logo[32];
std::string g_cart_label;

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
void cart_snapshot_rom(void)
{
    memcpy(g_cart_rom, pico8_ram, PICO8_CART_DATA_SIZE);
}

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
 * pico8_cart_rom_store(addr, len) — copy part of the RAM into the cart ROM.
 *
 * Only the data area (below 0x4300) has a ROM behind it.
 *
 * RAM 일부를 카트 ROM에 복사한다. ROM이 있는 곳은 데이터 영역(0x4300 아래)뿐이다.
 */
void pico8_cart_rom_store(int addr, int len)
{
    if (addr < 0) { len += addr; addr = 0; }
    if (addr + len > PICO8_CART_DATA_SIZE) len = PICO8_CART_DATA_SIZE - addr;
    if (len > 0) memcpy(g_cart_rom + addr, pico8_ram + addr, (size_t)len);
}

/*
 * pico8_cart_capture_label() — make the screen the label of the cart.
 *
 * extcmd("label").  Every pixel goes through the screen palette, so a
 * secret colour (128..143) is kept as one (the digits g..v).
 *
 * extcmd("label"). 픽셀마다 화면 팔레트를 거치므로 숨은 색(128..143)도 그대로
 * (g..v) 남는다.
 */
void pico8_cart_capture_label(void)
{
    static const char kDigits[] = "0123456789abcdefghijklmnopqrstuv";
    g_cart_label.clear();
    g_cart_label.reserve(128 * 129);
    for (int y = 0; y < PICO8_SCREEN_H; y++) {
        for (int x = 0; x < PICO8_SCREEN_W; x++) {
            uint8_t b = pico8_ram[PICO8_SCREEN + y * 64 + (x >> 1)];
            uint8_t c = (x & 1) ? (b >> 4) : (b & 0x0f);
            uint8_t e = pico8_ram[PICO8_DRAW_STATE + 0x10 + c];
            g_cart_label += kDigits[(e & 0x0f) + ((e & 0x80) ? 16 : 0)];
        }
        g_cart_label += '\n';
    }
}

/*
 * pico8_cart_splice_label(text) — put the label into the text of a .p8 file.
 *
 * Replaces the __label__ section of the text, or adds one before the
 * __map__, __sfx__ or __music__ section (or at the end), which is where
 * PICO-8 writes it.  Without a label the text is returned as it is.
 *
 * .p8 파일 텍스트의 __label__ 섹션을 바꾸거나, PICO-8이 쓰는 자리인 __map__,
 * __sfx__, __music__ 섹션 앞(없으면 끝)에 넣는다. 라벨이 없으면 텍스트를 그대로
 * 돌려준다.
 */
std::string pico8_cart_splice_label(const std::string &text)
{
    if (g_cart_label.empty()) return text;
    std::string section = "__label__\n" + g_cart_label;

    size_t at = text.find("\n__label__");
    if (at != std::string::npos) {
        size_t start = at + 1;
        size_t end = text.find("\n__", start + 9);
        end = (end == std::string::npos) ? text.size() : end + 1;
        return text.substr(0, start) + section + text.substr(end);
    }
    static const char *const kAfter[] = { "\n__map__", "\n__sfx__", "\n__music__" };
    for (const char *mark : kAfter) {
        size_t pos = text.find(mark);
        if (pos != std::string::npos)
            return text.substr(0, pos + 1) + section + text.substr(pos + 1);
    }
    std::string out = text;
    if (!out.empty() && out.back() != '\n') out += '\n';
    return out + section;
}
