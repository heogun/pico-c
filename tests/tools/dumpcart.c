#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../src/stb_image.h"

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <cart.png>\n", argv[0]); return 1; }

    int w, h, ch;
    unsigned char *img = stbi_load(argv[1], &w, &h, &ch, 4);
    if (!img) { fprintf(stderr, "Failed to load image\n"); return 1; }

    int num_pixels = w * h;
    printf("Image dimensions: %dx%d, total pixels: %d\n", w, h, num_pixels);

    if (num_pixels < 0x8001) {
        printf("Image too small to be a PICO-8 cart (need > 0x8000 pixels)\n");
        stbi_image_free(img);
        return 1;
    }

    unsigned char *picodata = (unsigned char *)calloc(num_pixels, 1);
    for (int i = 0; i < num_pixels; i++) {
        int px = i * 4;
        picodata[i] = (img[px + 2] & 3) | ((img[px + 1] & 3) << 2) | ((img[px + 0] & 3) << 4) | ((img[px + 3] & 3) << 6);
    }
    stbi_image_free(img);

    printf("\n--- PICO-8 Cart Info ---\n");

    /* 1. Version byte */
    int version = picodata[0x8000];
    printf("Version byte (0x8000): %d (0x%02x)\n", version, version);

    /* 2. Header bytes at 0x4300 (first 32 bytes) */
    int code_start = 0x4300;
    int code_data_len = (num_pixels - code_start < 0x3D00) ? num_pixels - code_start : 0x3D00;

    printf("\nHeader bytes at 0x4300 (32 bytes hex):\n");
    for (int i = 0; i < 32 && i < code_data_len; i++) {
        printf(" %02x", picodata[code_start + i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    /* 3. Magic detection */
    const unsigned char *code = picodata + code_start;
    int is_legacy = (code[0] == ':' && code[1] == 'c' && code[2] == ':' && code[3] == '\0');
    int is_pxa    = (code[0] == '\0' && code[1] == 'p' && code[2] == 'x' && code[3] == 'a');
    int is_plain  = 0;
    if (!is_legacy && !is_pxa) {
        int maybe_plain = 1;
        for (int i = 0; i < code_data_len && i < 128 && maybe_plain; i++) {
            unsigned char c = code[i];
            if (c == 0) break;
            if (c < 32 && c != '\n' && c != '\r' && c != '\t') maybe_plain = 0;
        }
        if (maybe_plain) is_plain = 1;
    }

    printf("Code format: ");
    if (is_legacy)    printf("LEGACY (:%c:\\0 magic)\n", code[1]);
    else if (is_pxa)  printf("PXA (\\0pxa magic)\n");
    else if (is_plain) printf("PLAIN TEXT (null-terminated ASCII)\n");
    else               printf("UNKNOWN\n");

    /* 4. Code length (bytes 4-5 big-endian) */
    if (code_data_len >= 6) {
        int code_len = (code[4] << 8) | code[5];
        printf("Code length from header (bytes 4-5): %d (0x%04x)\n", code_len, code_len);

        /* 5. Compressed length (bytes 6-7 big-endian, PXA format) */
        if (is_pxa && code_data_len >= 8) {
            int comp_len = (code[6] << 8) | code[7];
            printf("Compressed length from header (bytes 6-7): %d (0x%04x)\n", comp_len, comp_len);
        }
    }

    /* Also show first 4 bytes as string */
    printf("\nFirst 4 bytes as chars: ");
    for (int i = 0; i < 4; i++) {
        unsigned char c = code[i];
        printf("%c", (c >= 32 && c <= 126) ? c : '.');
    }
    printf("\n");

    /* Show full raw first 64 bytes */
    printf("\nFirst 64 bytes of code section (raw hex):\n");
    int show = (code_data_len < 64) ? code_data_len : 64;
    for (int i = 0; i < show; i++) {
        printf(" %02x", code[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (show % 16 != 0) printf("\n");

    free(picodata);
    return 0;
}
