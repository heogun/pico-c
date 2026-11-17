#ifndef PICO8_CART_INTERNAL_H
#define PICO8_CART_INTERNAL_H

/*
 * cart_internal.h — the parts of the cartridge code that its own files share.
 *
 * The cartridge code is split over several files: the state of the loaded
 * cart, the readers, the writers and the two code compressors.  This header
 * holds what they have in common: the state itself and the few helpers
 * that more than one file needs.
 *
 * 카트리지 코드는 여러 파일로 나뉘어 있다: 올라와 있는 카트의 상태, 읽는 쪽,
 * 쓰는 쪽, 그리고 두 가지 코드 압축기. 이 헤더는 그들이 공유하는 것을 담는다:
 * 상태 자체, 그리고 둘 이상의 파일이 필요로 하는 몇 개의 헬퍼.
 *
 * None of this is part of the public interface of pico-c: the header is
 * private to the cartridge code.
 *
 * 이 중 어느 것도 pico-c의 공개 인터페이스가 아니다. 이 헤더는 카트리지
 * 코드만 쓴다.
 */

#include <stdint.h>

#include <string>

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  Sizes that describe the compiled sprite/audio banks
 * ------------------------------------------------------------------ */

/* The sizes mirror the defines used by audio.cpp.  audio.cpp의 정의와 같다. */
#define CART_SFX_SIZE   68
#define CART_SONG_SIZE  4

/* ------------------------------------------------------------------ *
 *  The state of the loaded cartridge
 * ------------------------------------------------------------------ */

/* The raw Lua source of the loaded cart.  불러온 카트의 Lua 소스. */
extern std::string g_cart_source;

/* The path of the loaded cart.  불러온 카트의 경로. */
extern std::string g_cart_path;

/* The version that is written into the "version N" line.  "version N" 줄. */
extern const int kCartVersion;

/* The data area of the cart as it was read from disk.  디스크에서 읽은 데이터 영역. */
extern uint8_t     g_cart_rom[PICO8_CART_DATA_SIZE];

/* The 32 metadata bytes that follow the code area in a .p8.png.  .p8.png의 메타데이터. */
extern uint8_t     g_cart_logo[32];

/* The label of the cart: the __label__ section, 128 lines of 128 characters
   0-9a-v (colours 0..15, then 128..143), or empty when it has none.
   카트의 라벨: __label__ 섹션(128자 128줄, 0-9a-v = 색 0..15, 128..143).
   없으면 비어 있다. */
extern std::string g_cart_label;

/* ------------------------------------------------------------------ *
 *  The helpers that more than one file needs
 * ------------------------------------------------------------------ */

/* Copy the data area of the RAM into the stored cart.  RAM의 데이터 영역을 저장된 카트에 복사한다. */
void cart_snapshot_rom(void);

/* Read a file into a string.  파일을 문자열로 읽는다. */
bool read_whole_file(const char *path, std::string &out);

/* Unpack the old ":c:" code section.  예전 ":c:" 코드 섹션을 푼다. */
int  decompress_legacy(const uint8_t *src, uint8_t *dest, uint32_t maxlen);

/* Unpack a PXA code section.  PXA 코드 섹션을 푼다. */
int  decompress_pxa(const uint8_t *src, uint8_t *dest, uint32_t maxlen);

/* Fill the data area from the text of a .p8 file.  .p8 파일의 텍스트로 데이터 영역을 채운다. */
int  parse_p8_text(const char *text, size_t textlen,
                   uint8_t *cart_data, std::string &code,
                   std::string *label = nullptr);

/* Write a cart as a .p8 file.  카트를 .p8 파일로 쓴다. */
int  cart_write_p8(const char *path, const uint8_t *rom,
                   const std::string &code, int version);

/* Write a cart as a .p8.png image.  카트를 .p8.png 이미지로 쓴다. */
int  cart_write_p8png(const char *path, const uint8_t *rom,
                      const std::string &code, const uint8_t *logo);

#endif /* PICO8_CART_INTERNAL_H */
