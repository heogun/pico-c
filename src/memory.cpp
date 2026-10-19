#include <string.h>
#include "pico8.h"

uint8_t pico8_ram[PICO8_RAM_SIZE];

void pico8_init_song_default(void)
{
    /* The 64 songs (4 bytes each) default to 0x41,0x42,0x43,0x44 per song,
       PICO-8's "no pattern" marker. This is what a cart without a __music__
       section reports. */
    for (int s = 0; s < 64; s++)
        for (int c = 0; c < 4; c++)
            pico8_ram[PICO8_SONG_ADDR + s * 4 + c] = (uint8_t)(0x41 + c);
}

void pico8_init_sfx_default(void)
{
    /* A cart without a __sfx__ section still has a note duration for every
       sound: 1 for sfx 0 (the boot blip) and 16 for the rest. */
    for (int i = 0; i < 64; i++)
        pico8_ram[PICO8_SFX_ADDR + i * PICO8_SFX_SIZE + 65] =
            (uint8_t)(i == 0 ? 1 : 16);
}

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

/* PICO-8's RESET(): restore the runtime state at 0x5f00..0x5f7f (palettes,
   clip rect, camera, fill pattern, draw colour, ...) to its defaults without
   disturbing the cartridge's data below 0x5f00. */
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

void pico8_load_memory(const uint8_t *data, uint32_t len)
{
    if (len > PICO8_RAM_SIZE) len = PICO8_RAM_SIZE;
    memcpy(pico8_ram, data, len);
}
