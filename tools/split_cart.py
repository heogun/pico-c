#!/usr/bin/env python3
"""Split src/cart.cpp into src/cart/*.cpp."""

import sys

sys.path.insert(0, "tools")

from split_module import split

INCLUDES_PLAIN = """#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <string>
#include <vector>

#include <stdint.h>

#include "pico8.h"
#include "util.h"
#include "cart_internal.h"
"""

INCLUDES_STB = """#include <stdio.h>
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
"""

INCLUDES_PNG = """#include <stdio.h>
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
"""

STATE_HEADER = """/*
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
"""

COMPRESS_HEADER = """/*
 * cart_compress.cpp — the two code compressors PICO-8 used.
 *
 * A .p8.png cartridge holds its code section compressed: with PXA in the
 * newer carts, with the old ":c:" format in the older ones.  Both are
 * unpacked here.
 *
 * .p8.png 카트의 코드 섹션은 압축되어 있다: 새 카트라면 PXA로, 오래된
 * 카트라면 예전의 ":c:" 형식으로. 둘 다 여기서 풀린다.
 */
"""

READ_HEADER = """/*
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
"""

WRITE_HEADER = """/*
 * cart_write.cpp — writing a cartridge as a .p8 file.
 *
 * The writer compares every part of the data area with its default and
 * leaves the parts that did not change out of the file, so a saved cart
 * stays as small as the real PICO-8 keeps it.
 *
 * 쓰는 쪽은 데이터 영역의 각 부분을 기본값과 비교해 바뀐 부분만 파일에
 * 남기므로, 저장한 카트가 실제 PICO-8이 만드는 만큼 작게 유지된다.
 */
"""

PNG_HEADER = """/*
 * cart_png.cpp — writing a cartridge as a .p8.png image.
 *
 * The image is built by hand: the code section is compressed with PXA and
 * the data area is scattered into the pixels of the picture, the same way
 * PICO-8 stores it.
 *
 * 이미지는 손으로 만든다: 코드 섹션을 PXA로 압축하고 데이터 영역을 그림의
 * 픽셀에 흩scatter, 실제 PICO-8과 같은 방식으로 저장한다.
 */
"""

DATA_HEADER = """/*
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
"""

SPEC = {
    "src": "src/cart.cpp",
    "groups": [
        {
            "out": "src/cart/cart_state.cpp",
            "header": STATE_HEADER + "\n" + INCLUDES_PLAIN,
            "raw": [("/* Raw Lua source of the currently loaded cart",
                     "static uint8_t     g_cart_logo[32];")],
            "functions": ["cart_snapshot_rom", "pico8_cart_set_source",
                          "pico8_cart_source", "pico8_cart_set_path",
                          "pico8_cart_get_path", "pico8_cart_rom"],
        },
        {
            "out": "src/cart/cart_compress.cpp",
            "header": COMPRESS_HEADER + "\n" + INCLUDES_PLAIN,
            "raw": [("/* Lookup table used by the legacy \":c:\" compressor.",
                     "    \"\\n 0123456789abcdefghijklmnopqrstuvwxyz!#%(){}[]<>+=/*:;.,~_\";"),
                    ("/* Move-to-front table used by the PXA decompressor.",
                     "};"),],
            "functions": ["decompress_legacy", "decompress_pxa"],
        },
        {
            "out": "src/cart/cart_read.cpp",
            "header": READ_HEADER + "\n" + INCLUDES_STB,
            "functions": ["read_whole_file", "hex_pair", "hex_single",
                          "preprocess_includes", "parse_sfx_line",
                          "parse_music_line", "check_source_limits",
                          "parse_p8_text", "load_p8png", "load_p8_text",
                          "pico8_load_cart", "pico8_run_cart",
                          "pico8_cart_read_data"],
        },
        {
            "out": "src/cart/cart_write.cpp",
            "header": WRITE_HEADER + "\n" + INCLUDES_PLAIN,
            "raw": [("static const char kHexDigits[]", "static const char kHexDigits[] = \"0123456789abcdef\";")],
            "functions": ["put_hex_byte", "gfx_line_is_default", "gff_is_empty",
                          "map_line_is_default", "sfx_line_is_default",
                          "music_line_is_default", "cart_write_p8",
                          "pico8_cart_save_p8", "pico8_cart_save_rom_p8"],
        },
        {
            "out": "src/cart/cart_png.cpp",
            "header": PNG_HEADER + "\n" + INCLUDES_PNG,
            "functions": ["png_crc32", "png_be32", "png_chunk", "zlib_stored",
                          "png_scatter", "cart_write_p8png",
                          "pico8_cart_save_png", "pico8_cart_save_rom_png"],
        },
        {
            "out": "src/cart/cart_data.cpp",
            "header": DATA_HEADER + "\n" + INCLUDES_PLAIN,
            "functions": ["pico8_cart_reload", "pico8_cart_cstore"],
        },
    ],
    # the helpers that now live in several files lose their "static"
    "fixups": [
        (r"^static (std::string|uint8_t|const int)( +)"
         r"(g_cart_source|g_cart_path|g_cart_rom|g_cart_logo|kCartVersion)",
         r"\1\2\3"),
        (r"^static void (cart_snapshot_rom)\(", r"void \1("),
        (r"^static int (decompress_legacy|decompress_pxa)\(",
         r"int \1("),
        (r"^static bool (read_whole_file)\(", r"bool \1("),
        (r"^static int (parse_p8_text|cart_write_p8|cart_write_p8png)\(",
         r"int \1("),
    ],
}

split(SPEC)
