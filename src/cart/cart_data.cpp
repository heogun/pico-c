/*
 * cart_data.cpp — the data of the cartridge: reload, cstore and read.
 *
 * reload() puts the data area of the stored cartridge back into the RAM,
 * cstore() writes the RAM back into it, and read_data() takes the data
 * area of a file without touching the machine.
 *
 * reload()는 보관해 둔 카트의 데이터 영역을 RAM에 다시 넣고, cstore()는
 * RAM을 그 안에 써 넣고, read_data()는 기계를 건드리지 않고 파일의 데이터
 * 영역만 읽는다.
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
