#ifndef PICO8_CART_H
#define PICO8_CART_H

/*
 * pico8_cart.h — the cartridge files on disk.
 *
 * A PICO-8 cartridge is a .p8 file: a Lua source in a text section and a
 * 0x4300 byte data area in a binary one.  The same content can also be
 * stored as a .p8.png image, where the code section is compressed.
 *
 * PICO-8 카트리지는 .p8 파일이다: 텍스트 섹션에 Lua 소스를, 바이너리 섹션에
 * 0x4300 바이트짜리 데이터 영역을 담는다. 같은 내용을 .p8.png 이미지로도
 * 저장할 수 있는데, 그때는 코드 섹션이 압축된다.
 *
 * The Lua side of the cartridge (save, load, reload, cstore, cartdata)
 * lives in src/api/api_cart.cpp and calls the helpers below.
 *
 * 카트리지의 Lua 쪽(save, load, reload, cstore, cartdata)은
 * src/api/api_cart.cpp에 있고 아래 헬퍼들을 사용한다.
 */

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  The loaded cart
 * ------------------------------------------------------------------ */

/*
 * Raw Lua source of the currently loaded cart (preserved by save()).
 *
 * 지금 올라와 있는 카트의 원본 Lua 소스. save()가 그대로 되돌려 쓰도록
 * 보관한다.
 */
void        pico8_cart_set_source(const char *src, size_t len);
const char *pico8_cart_source(size_t *len);

/*
 * The path of the loaded cart, as the shell knows it.
 *
 * 셸이 알고 있는, 불러온 카트의 경로.
 */
void        pico8_cart_set_path(const char *path);
const char *pico8_cart_get_path(void);

/*
 * The cart's on-disk data area (0x0000..0x42ff), retained for reload().
 *
 * 디스크에 있는 카트의 데이터 영역(0x0000~0x42ff). reload()가 이걸 쓴다.
 */
const uint8_t *pico8_cart_rom(void);

/*
 * Number of PICO-8 tokens in a Lua source buffer, matching the count the
 * oracle reports when a cart exceeds the 8192-token limit.
 *
 * Lua 소스에 있는 PICO-8 토큰 수. 카트가 8192 토큰 한계를 넘었을 때 오라클이
 * 알려 주는 수와 같아야 한다.
 */
int         pico8_count_tokens(const char *src, size_t len);

/* ------------------------------------------------------------------ *
 *  Writing and reading cart files
 * ------------------------------------------------------------------ */

/*
 * Serialize live RAM (or the stored cart ROM) plus the source to a .p8 file.
 *
 * 살아 있는 RAM(또는 보관해 둔 카트 ROM)과 소스를 .p8 파일로 직렬화한다.
 */
int pico8_cart_save_p8(const char *path);
int pico8_cart_save_rom_p8(const char *path);

/*
 * Serialize to a .p8.png cartridge image (PXA-compressed code section).
 *
 * .p8.png 카트리지 이미지로 직렬화한다. 코드 섹션은 PXA로 압축된다.
 */
int pico8_cart_save_png(const char *path);
int pico8_cart_save_rom_png(const char *path);

/*
 * Read only a cartridge's data area (0x4300 bytes) without touching RAM.
 *
 * RAM을 건드리지 않고 카트의 데이터 영역(0x4300바이트)만 읽는다.
 */
int pico8_cart_read_data(const char *path, uint8_t *out, size_t out_len);

/*
 * reload()/cstore() against the stored cart ROM (and an optional other file).
 *
 * 보관해 둔 카트 ROM(또는 따로 준 파일)을 대상으로 하는 reload()/cstore().
 */
int pico8_cart_reload(int dest, int source, int len, const char *filename);
int pico8_cart_cstore(int dest, int source, int len, const char *filename);

/* ------------------------------------------------------------------ *
 *  Running a cart
 * ------------------------------------------------------------------ */

/*
 * Load a cartridge into the machine.  Returns 0 on success.
 *
 * 카트리지 하나를 기계에 불러온다. 성공하면 0.
 */
int pico8_load_cart(const char *path);

/*
 * Load a cartridge and start it running.  Returns 0 on success.
 *
 * 카트리지 하나를 불러오고 실행을 시작한다. 성공하면 0.
 */
int pico8_run_cart(const char *path);

#endif /* PICO8_CART_H */
