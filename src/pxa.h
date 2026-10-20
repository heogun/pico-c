/* PXA code compression for the .p8.png cartridge format (see pxa.cpp). */

#ifndef PICO8_PXA_H
#define PICO8_PXA_H

#include <stdint.h>

/* Compress `len` bytes from `in` into `out`, which must have room for
   `len` bytes plus the 8-byte header.

   Returns the number of bytes written to `out`. When the data does not
   compress, `in` is copied verbatim and `len` is returned (the code section
   then holds plain text, which PICO-8 also accepts). The result includes
   the "\0pxa" header for the 0.2.0+ code section format. */
int pxa_compress(const uint8_t *in, uint8_t *out, int len);

#endif /* PICO8_PXA_H */
