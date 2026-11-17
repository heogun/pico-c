/*
 * memory.cpp — the memory of the machine.
 *
 * The RAM of the PICO-8 is one 64KB array (pico8_ram), and the whole
 * emulator works on it: the screen, the sprite sheet, the map and the
 * data of the cart are all regions of it.  This file holds the array
 * itself and the code that puts a fresh machine into it.
 *
 * PICO-8의 RAM은 64KB짜리 배열 하나(pico8_ram)이고, 에뮬레이터 전체가 이
 * 배열 위에서 돌아간다. 화면, 스프라이트 시트, 맵, 카트 데이터가 모두 이
 * 배열의 영역이다. 이 파일은 그 배열과, 새 기계의 상태를 채우는 코드를
 * 담는다.
 *
 * The draw state (0x5f00..0x5f3f) holds the things that belong to the
 * running program: the palettes, the cursor, the camera, the clip
 * rectangle, the fill pattern and the draw colour.  The hardware state
 * (0x5f40..0x5f7f) holds the buttons and the devkit devices.  Both are
 * reset by pico8_reset_draw_state(), which is what RESET() does.
 *
 * 드로우 상태(0x5f00~0x5f3f)는 실행 중인 프로그램에 속하는 것들을 담는다:
 * 팔레트, 커서, 카메라, 클립 사각형, 필 패턴, 드로우 색. 하드웨어 상태
 * (0x5f40~0x5f7f)는 버튼과 데브킷 장치를 담는다. 둘 다 RESET()이 하는
 * pico8_reset_draw_state()가 초기화한다.
 */

#include <string.h>
#include "pico8.h"

uint8_t pico8_ram[PICO8_RAM_SIZE];

/*
 * pico8_init_song_default() — the default state of the 64 songs.
 *
 * Every song is four bytes long, and each byte names one of the four
 * channels of that song.  The default value 0x41..0x44 is PICO-8's "no
 * pattern" marker, so a cart without a __music__ section reports the same
 * thing as a real PICO-8.
 *
 * song 하나는 4바이트이고, 각 바이트는 그 song의 채널 넷 중 하나를 가리킨다.
 * 기본값 0x41~0x44는 PICO-8의 "패턴 없음" 표지라, __music__ 섹션이 없는 카트는
 * 실제 PICO-8과 똑같은 값을 보인다.
 */
void pico8_init_song_default(void)
{
    for (int s = 0; s < 64; s++)
        for (int c = 0; c < 4; c++)
            pico8_ram[PICO8_SONG_ADDR + s * 4 + c] = (uint8_t)(0x41 + c);
}

/*
 * pico8_init_sfx_default() — the default state of the 64 sound effects.
 *
 * The interesting byte of a sound effect is its duration at the end of the
 * 68 byte record.  The default is 1 for the first effect (the short blip
 * the machine makes at boot) and 16 for all the others.
 *
 * 효과음에서 중요한 바이트는 68바이트 기록의 끝에 있는 길이다. 기본값은
 * 첫 번째 효과음(부팅할 때 나는 짧은 삐 소리)에 1, 나머지는 전부 16이다.
 */
void pico8_init_sfx_default(void)
{
    for (int i = 0; i < 64; i++)
        pico8_ram[PICO8_SFX_ADDR + i * PICO8_SFX_SIZE + 65] =
            (uint8_t)(i == 0 ? 1 : 16);
}

/*
 * pico8_reset_memory() — the state of a machine that has just been switched
 * on.
 *
 * Clears the whole RAM and writes the defaults of the draw state, the
 * palettes, the songs and the sound effects back into it.
 *
 * RAM 전체를 지우고 드로우 상태, 팔레트, song, 효과음의 기본값을 다시 써
 * 넣는다.
 */
void pico8_reset_memory(void)
{
    memset(pico8_ram, 0, PICO8_RAM_SIZE);

    for (int i = 0; i < 16; i++) {
        pico8_ram[PICO8_DRAW_STATE + i] = (uint8_t)i | (i == 0 ? 0x10 : 0x00);
        pico8_ram[PICO8_DRAW_STATE + 0x10 + i] = (uint8_t)i;
    }
    pico8_ram[PICO8_DRAW_STATE + 0x25] = 6;

    pico8_init_song_default();
    pico8_init_sfx_default();

    pico8_pal_reset_all();
}

/*
 * pico8_reset_draw_state() — the RESET() of PICO-8.
 *
 * Restores the runtime state at 0x5f00..0x5f7f (the palettes, the clip
 * rectangle, the camera, the fill pattern, the draw colour) to its default
 * values, without touching the data of the cart below 0x5f00.
 *
 * 0x5f00~0x5f7f의 런타임 상태(팔레트, 클립 사각형, 카메라, 필 패턴, 드로우 색)를
 * 기본값으로 되돌린다. 0x5f00 아래에 있는 카트 데이터는 건드리지 않는다.
 */
void pico8_reset_draw_state(void)
{
    for (int a = 0x5f00; a <= 0x5f7f; a++)
        pico8_ram[a] = 0;

    for (int i = 0; i < 16; i++) {
        pico8_ram[PICO8_DRAW_STATE + i] = (uint8_t)i | (i == 0 ? 0x10 : 0x00);
        pico8_ram[PICO8_DRAW_STATE + 0x10 + i] = (uint8_t)i;
    }
    pico8_ram[PICO8_DRAW_STATE + 0x25] = 6;             /* draw colour 0x5f25 */

    pico8_ram[0x5f20] = 0;                              /* clip rect         */
    pico8_ram[0x5f21] = 0;
    pico8_ram[0x5f22] = 128;
    pico8_ram[0x5f23] = 128;

    pico8_pal_reset_all();
}

/*
 * pico8_load_memory(data, len) — copy an image into the RAM.
 *
 * Everything past the 64KB of the RAM is dropped, so a bigger file is
 * loaded, but only as far as the memory reaches.
 *
 * 64KB를 넘는 부분은 버린다. 더 큰 파일을 불러와도 메모리가 닿는 곳까지만
 * 들어간다.
 */
void pico8_load_memory(const uint8_t *data, uint32_t len)
{
    if (len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE;
    memcpy(pico8_ram, data, len);
}
