/*
	pxa_compress.c -- PXA code compression for the .p8.png cartridge format.

	Derived from the official Lexaloffle Games LLP implementation:

		pxa compression snippets for PICO-8 cartridge format (as of 0.2.4c)
		author: joseph@lexaloffle.com
		Copyright (c) 2020-22  Lexaloffle Games LLP
		https://github.com/dansanderson/lexaloffle (pxa_compress_snippets.c)

	released by Lexaloffle under the MIT license:

		Permission is hereby granted, free of charge, to any person obtaining a
		copy of this software and associated documentation files (the
		"Software"), to deal in the Software without restriction, including
		without limitation the rights to use, copy, modify, merge, publish,
		distribute, sublicense, and/or sell copies of the Software, and to
		permit persons to whom the Software is furnished to do so, subject to
		the following conditions:

		The above copyright notice and this permission notice shall be included
		in all copies or substantial portions of the Software.

		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
		OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
		MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
		IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
		CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
		TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
		SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

	This version is a C++ adaptation that keeps the compressed stream
	byte-identical to the oracle's (so a cart saved by pico-c loads and
	behaves the same in real PICO-8, and the two writers agree byte for
	byte). The only edits are:

	  - `codo_malloc`/`codo_free` -> std::vector (no leaked static heap)
	  - the static hash tables became function-local state
	  - `char *modified_code` renamed (it is just a mutable copy of the input)

	The float-based `score = i * 256 / bit_cost` integer division of the
	original is kept untouched: it decides *which* blocks win, so changing it
	would change the output.
*/

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "pxa.h"

typedef unsigned short int uint16;
typedef unsigned char      uint8;

/* 3 3 5 4  (gives balanced trees for typical data) */
#define PXA_MIN_BLOCK_LEN     3
#define BLOCK_LEN_CHAIN_BITS  3
#define BLOCK_DIST_BITS       5
#define TINY_LITERAL_BITS     4

/* has to be 3 (optimized in find_repeatable_block) */
#define MIN_BLOCK_LEN 3
#define HASH_MAX      4096
#define MINI_HASH(pp, i) \
    (((pp)[(i) + 0] * 7 + (pp)[(i) + 1] * 1503 + (pp)[(i) + 2] * 51717) & \
     (HASH_MAX - 1))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/* ------------------------------------------------------------------ *
 *  Bit stream (LSB first)
 * ------------------------------------------------------------------ */

static uint8 *dest_buf = NULL;
static int    bit      = 1;
static int    dest_pos = 0;

static void putbit(int bval)
{
    dest_buf[dest_pos] &= (uint8)~bit;      /* per-bit, don't clobber      */
    if (bval) dest_buf[dest_pos] |= (uint8)bit;

    bit <<= 1;
    if (bit == 256) {
        bit = 1;
        dest_pos++;
    }
}

static int putval(int val, int bits)
{
    if (bits <= 0) return 0;
    for (int i = 0; i < bits; i++) putbit(val & (1 << i));
    return bits;
}

/* Write `val` as a 3-bit chain: 7,7,7,...,remainder. */
static int putchain(int val, int link_bits, int max_bits)
{
    const int max_link_val = (1 << link_bits) - 1;
    int bits_written = 0;
    int vv = max_link_val;

    while (vv == max_link_val) {
        vv = MIN(val, max_link_val);
        bits_written += putval(vv, link_bits);
        val -= vv;
        if (bits_written >= max_bits) return bits_written;
    }
    return bits_written;
}

/* Block distance: 1 + 15 / 01 + 10 / 00 + 5 bits, cheapest first. */
static int putnum(int val)
{
    const int jump = BLOCK_DIST_BITS;
    int bits = jump;

    while ((1 << bits) <= val) bits += jump;

    putchain(3 - (bits / jump), 1, 2);
    putval(val, bits);
    return (bits / jump) + bits;
}

#define PXA_WRITE_VAL(x) { literal_bits_written += putval((x), 8); }
static int literal_bits_written = 0;

/* ------------------------------------------------------------------ *
 *  Match finder
 * ------------------------------------------------------------------ */

struct pxa_state {
    uint16 *hash_list[HASH_MAX];
    std::vector<uint16> heap;
    int     found[HASH_MAX];
};

/* Build per-hash lists of the positions where each 3-byte hash occurs. */
static void pxa_build_hash_lookup(pxa_state &st, const uint8 *in, int len)
{
    int hash;
    uint16 *list;
    uint16 *new_list;

    memset(st.hash_list, 0, sizeof(st.hash_list));

    /* Worst case ~262144 uint16s for a 64k input; grow by 2x as in the
       original (the heap is stable because `heap` is never reallocated
       once reserved). */
    st.heap.assign(262144 + 1024, 0);
    int heap_pos = 0;

    for (int i = 0; i < len - 2; i++) {
        hash = MINI_HASH(in, i);
        list = st.hash_list[hash];

        if (!list) {
            st.hash_list[hash] = list = &st.heap[heap_pos];
            list[0] = 4;   /* allocated */
            list[1] = 0;   /* items     */
            heap_pos += 2 + list[0];
        }

        if (list[0] == list[1]) {
            st.hash_list[hash] = new_list = &st.heap[heap_pos];
            new_list[0] = (uint16)(list[0] * 2);
            new_list[1] = list[1];
            memcpy(&new_list[2], &list[2], list[1] * sizeof(uint16));
            list = new_list;
            heap_pos += 2 + list[0];
        }

        list[2 + list[1]] = (uint16)i;
        list[1]++;
    }
}

static int pxa_find_repeatable_block(pxa_state &st, uint8 *dat, int pos,
                                     int data_len, int *block_offset,
                                     int *score_out)
{
    const int max_hist_len = 32767;   /* 15 bits */
    int best_len   = 0;
    int best_pos0  = -100000;
    int max_len    = data_len - pos;
    int hash;
    int score, dist, bit_cost, best_score = -1;

    if (max_len < PXA_MIN_BLOCK_LEN) return 0;
    if (max_hist_len < PXA_MIN_BLOCK_LEN) return 0;

    hash = MINI_HASH(dat, pos);

    uint16 *list = st.hash_list[hash];
    if (!list) return 0;

    for (int list_pos = 0; list_pos < list[1] && list[2 + list_pos] < pos;
         list_pos++) {
        if (list[2 + list_pos] < pos - max_hist_len) continue;   /* in range? */

        int pos0 = list[2 + list_pos];
        int i = 0;

        /* matches in history */
        while (i < max_len && (pos0 + i) < pos && dat[pos0 + i] == dat[pos + i])
            i++;

        /* matches in output of this repeated block */
        while (i < max_len && (pos0 + i) >= pos &&
               dat[pos0 + (i % (pos - pos0))] == dat[pos + i])
            i++;

        dist = pos - pos0;
        bit_cost = 0;
        while (dist > 0) {
            bit_cost++;
            dist >>= BLOCK_DIST_BITS;
        }
        bit_cost = MIN(bit_cost, 2) + bit_cost * BLOCK_DIST_BITS;  /* 6,12,17 */
        bit_cost += 3;   /* chain link(s) */
        bit_cost += 1;   /* is_block marker */

        score = i * 256 / bit_cost;

        if (score > best_score) {
            best_score = score;
            best_pos0  = pos0;
            best_len   = i;
        }
    }

    *block_offset = (best_pos0 >= 0) ? (pos - best_pos0) : 0;
    *score_out    = best_score;

    return best_len;
}

/* ------------------------------------------------------------------ *
 *  Compressor
 * ------------------------------------------------------------------ */

static void init_literals_state(int *literal, int *literal_pos)
{
    for (int i = 0; i < 256; i++) literal[i] = i;
    for (int i = 0; i < 256; i++) literal_pos[literal[i]] = i;
}

#define BACKUP_VLIST_STATE()   memcpy(literal_backup, literal, sizeof(literal)); \
                               memcpy(literal_pos_backup, literal_pos, sizeof(literal_pos));
#define RESTORE_VLIST_STATE()  memcpy(literal, literal_backup, sizeof(literal)); \
                               memcpy(literal_pos, literal_pos_backup, sizeof(literal_pos));

int pxa_compress(const uint8 *in_p, uint8 *out, int len)
{
    int pos = 0;
    int block_offset = 0;
    int block_len;
    int i, hash;
    int block_score, literal_score;
    int literal[256], literal_pos[256];
    int literal_backup[256], literal_pos_backup[256];

    /* raw block bookkeeping (0.2.0j) */
    int raw_pos_src0 = 0;
    int raw_header_write_pos = 0;
    int raw_block_write_pos = 0;
    int raw_pos_src = 0;
    int raw_pos_dest = 0;
    int stored_last_segment_as_raw = 0;

    init_literals_state(literal, literal_pos);

    pxa_state st;
    pxa_build_hash_lookup(st, in_p, len);

    bit       = 1;
    dest_buf  = out;
    dest_pos  = 0;
    literal_bits_written = 0;

    if (len == 0) return 0;

    for (i = 0; i < HASH_MAX; i++) st.found[i] = -1;

    /* The oracle compresses a scratch copy; `in` is mutated nowhere but is
       kept separate so the caller's buffer stays untouched. */
    std::vector<uint8> modified(in_p, in_p + len);
    uint8 *in = modified.data();

    /* The header is written as raw bytes 'p','x','a' but the first byte is
       deliberately left zero (reads as "\0pxa" in the code section). */
    PXA_WRITE_VAL(0);
    PXA_WRITE_VAL('p');
    PXA_WRITE_VAL('x');
    PXA_WRITE_VAL('a');

    PXA_WRITE_VAL(len / 256);
    PXA_WRITE_VAL(len % 256);

    PXA_WRITE_VAL(0);            /* compressed size, patched at the end */
    PXA_WRITE_VAL(0);

    raw_pos_dest  = dest_pos;
    raw_pos_src = raw_pos_src0 = pos;
    raw_header_write_pos = raw_block_write_pos = 0;
    /* (bit/byte write position saved by the macros below) */
    int saved_bit = bit;
    int saved_pos = dest_pos;
    raw_header_write_pos = (saved_pos << 16) | (0 << 8) | saved_bit;
    raw_block_write_pos  = raw_header_write_pos;
    BACKUP_VLIST_STATE();

    while (pos < len) {
        block_len = pxa_find_repeatable_block(st, in, pos, len, &block_offset,
                                              &block_score);

        int c    = in[pos];
        int lpos = literal_pos[c];

        /* literal cost: 1 (marker) + 1 (category chain) + category + index */
        int cat_bits    = TINY_LITERAL_BITS;
        int cat_max_val = 1 << cat_bits;
        while (lpos >= cat_max_val) {
            cat_bits++;
            cat_max_val += (1 << cat_bits);
        }
        literal_score = 1 * 256 / (2 + ((cat_bits - TINY_LITERAL_BITS) + cat_bits));

        /* If block score is good (>= 128), just take it. Otherwise look for a
           better block in the next 2 characters before committing. */
        if (block_len >= PXA_MIN_BLOCK_LEN && block_score > literal_score &&
            block_score < 128) {
            for (int ii = 1; ii < 3; ii++) {
                int block_offset2 = 0, block_score2 = 0;
                pxa_find_repeatable_block(st, in, pos + ii, len, &block_offset2,
                                          &block_score2);
                if (block_score2 > block_score * 6 / 5) {
                    block_score = 0;
                    break;
                }
            }
        }

        if (block_len >= PXA_MIN_BLOCK_LEN && block_score > literal_score) {
            /* block */
            putbit(0);
            putnum(block_offset - 1);
            putchain(block_len - PXA_MIN_BLOCK_LEN, BLOCK_LEN_CHAIN_BITS, 100000);
            pos += block_len;
        } else {
            /* literal */
            putbit(1);

            int cat_bits2    = TINY_LITERAL_BITS;
            int cat_max_val2 = 1 << cat_bits2;
            int val = lpos;
            while (lpos >= cat_max_val2) {
                val -= (1 << cat_bits2);
                cat_bits2++;
                cat_max_val2 += (1 << cat_bits2);
            }
            putchain(cat_bits2 - TINY_LITERAL_BITS, 1, 16);
            putval(val, cat_bits2);

            /* move-to-front update (only tracked outside blocks, as in the
               original -- the ratio is fine and it is faster) */
            for (i = lpos; i > 0; i--) {
                literal[i] = literal[i - 1];
                literal_pos[literal[i]]++;
            }
            literal[0]      = c;
            literal_pos[c]  = 0;

            pos++;
            block_len = 1;   /* for the hash update below */
        }

        /* add hash positions for the bytes just emitted */
        for (i = MAX(0, pos - block_len - 2); i < pos - 2; i++) {
            hash = MINI_HASH(in, i);
            st.found[hash] = i;
        }

        /* if the last 32 bytes written have a ratio worse than ~1.0, rewrite
           them as a raw (uncompressed) block instead */
        if (dest_pos - raw_pos_dest >= 32 || pos == len) {
            int compressed_size = dest_pos - raw_pos_dest;
            int raw_size        = pos - raw_pos_src;
            int margin = (raw_pos_src0 == raw_pos_src) ? 3 : 0;

            if (compressed_size > raw_size + margin) {
                if (stored_last_segment_as_raw == 0) {
                    raw_header_write_pos = raw_block_write_pos;
                    /* jump back to the block start and emit the raw marker
                       (0,1,0 then ten zero bits) */
                    int save_bit = bit, save_pos = dest_pos;
                    bit = raw_header_write_pos & 0xff;
                    dest_pos = (raw_header_write_pos >> 16) & 0x7fff;
                    putbit(0); putbit(1); putbit(0); putval(0, 10);
                    bit = save_bit; dest_pos = save_pos;
                } else {
                    /* append: overwrite the previous null terminator */
                    int save_bit = bit, save_pos = dest_pos;
                    bit = raw_block_write_pos & 0xff;
                    dest_pos = (raw_block_write_pos >> 16) & 0x7fff;
                    dest_pos--;
                    bit = save_bit; dest_pos = save_pos;
                }

                for (int k = 0; k < raw_size; k++)
                    putval(in[raw_pos_src + k], 8);
                putval(0, 8);   /* null terminator */

                stored_last_segment_as_raw = 1;
                RESTORE_VLIST_STATE();
            } else {
                stored_last_segment_as_raw = 0;
                raw_pos_src0 = pos;
                BACKUP_VLIST_STATE();
            }

            raw_pos_dest     = dest_pos;
            raw_pos_src      = pos;
            raw_block_write_pos = (dest_pos << 16) | (0 << 8) | bit;
        }
    }

    /* flush to a byte boundary */
    while (bit != 1) putbit(0);

    int bytes_written = dest_pos;
    dest_buf[6] = (uint8)(bytes_written / 256);
    dest_buf[7] = (uint8)(bytes_written % 256);

    /* If compression made things worse, return the input verbatim (the code
       section then holds plain text, which PICO-8 also understands). */
    if (bytes_written > len) {
        memcpy(out, in_p, len);
        return len;
    }

    return bytes_written;
}
