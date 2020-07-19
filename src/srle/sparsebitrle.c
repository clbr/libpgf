#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sparserle.h"

//#include <stdio.h>

static uint8_t neededbits(uint8_t val) {
	uint8_t bits = 0;
	while (val) {
		bits++;
		val >>= 1;
	}
	return bits;
}

static uint8_t store;
static uint8_t storedbits;
static uint8_t *bitout, *bitstart;
static const uint8_t *bitin;

static void bit_init_write(uint8_t *ptr) {
	store = 0;
	storedbits = 0;
	bitstart = bitout = ptr;
}

static void bit_init_read(const uint8_t *ptr) {
	store = 0;
	storedbits = 0;
	bitin = ptr;
}

static uint16_t bit_flush() {
	if (storedbits) {
		*bitout++ = store >> (8 - storedbits);
		storedbits = 0;
		store = 0;
	}

	return bitout - bitstart;
}

static void bit_write(uint8_t val, uint8_t num) {
	if (!storedbits && num == 8) {
		*bitout++ = val;
		return;
	}

	while (num) {
		store >>= 1;
		store |= (val & 1) << 7;
		val >>= 1;
		num--;
		storedbits++;

		if (storedbits == 8) {
			bit_flush();
		}
	}
}

static uint8_t bit_read(uint8_t num) {
	uint8_t val = 0;

	if (!storedbits) {
		store = *bitin++;
		storedbits = 8;
	}

	if (num == storedbits) {
		storedbits = 0;
		return store;
	} else if (num < storedbits) {
		val = store & ((1 << num) - 1);
		storedbits -= num;
		store >>= num;
		return val;
	} else {
		const uint8_t num_mask = ((1 << num) - 1);
		val = store;
		num -= storedbits;
		const uint8_t got = storedbits;

		store = *bitin++;
		storedbits = 8;

		val |= store << got;
		val &= num_mask;

		store >>= num;
		storedbits -= num;
		return val;
	}
}

uint16_t sparsebitrle_comp(const uint8_t *in, uint8_t *out, const uint16_t len) {

	uint16_t bytes[256] = { 0 }, i, used, run;
	uint8_t cur, hi;
	uint8_t chr2pos[256] = { 0 };
	uint8_t * const start = out;

	if (len > 16384)
		return USHRT_MAX;

	for (i = 0; i < len; i++) {
		cur = in[i];
		// Zero only counts if it's there as a single byte, not a run
		if (cur) {
			bytes[cur]++;
		} else {
			if ((!i || in[i - 1]) &&
				(i == len - 1 || in[i + 1]))
				bytes[cur]++;
		}
	}

	used = 0;
	hi = 0;
	for (i = 0; i < 256; i++) {
		if (bytes[i]) {
			chr2pos[i] = used;
			used++;
			if (i > hi)
				hi = i;
		}
	}

	if (used > 28) // Just a heuristic, plus four values needed for escapes
		return USHRT_MAX;

//	printf("%u used values; largest %u, neededbits %u\n",
//		used, hi, neededbits(used + 3));

	*out++ = used;
	for (i = 0; i < 256; i++) {
		if (bytes[i]) {
			*out++ = i;
		}
	}
	bit_init_write(out);

	cur = *in;
	run = 1;

	const uint8_t zero_short = used;
	const uint8_t zero_long = used + 1;
	const uint8_t run_short = used + 2;
	const uint8_t run_long = used + 3;
	const uint8_t bitlen = neededbits(run_long);

	for (i = 1; i < len; i++) {
		if (i == len - 1 || in[i] != cur) {
			if (i == len - 1 && in[i] == cur)
				run++;

			if (run == 1) {
				bit_write(chr2pos[cur], bitlen);
//				printf("byte %u\n", cur);
			} else {
				if (cur == 0) {
					bit_write(run < 256 ? zero_short : zero_long, bitlen);
					// Zero runs don't output the byte
				} else {
					bit_write(run < 256 ? run_short : run_long, bitlen);
					bit_write(chr2pos[cur], bitlen);
				}
				if (run < 256) {
					bit_write(run, 8);
//					printf("short run %u, %u\n", run, cur);
				} else {
					bit_write(run & 0xff, 8);
					bit_write(run >> 8, 6);
//					printf("long run %u, %u\n", run, cur);
				}
			}

			if (i == len - 1 && in[i] != cur)
				bit_write(chr2pos[in[i]], bitlen);

			run = 1;
			cur = in[i];
		} else {
			run++;
		}
	}

	out += bit_flush();

	return out - start;
}

void sparsebitrle_decomp(const uint8_t *in, uint8_t *out, const uint16_t outlen) {
//uint8_t * const outstart = out;
	const uint8_t * const end = out + outlen;
	const uint8_t numvals = *in++;
	const uint8_t * const valtab = in;
	in += numvals;

	const uint8_t zero_short = numvals;
	const uint8_t zero_long = numvals + 1;
	const uint8_t run_short = numvals + 2;
	const uint8_t run_long = numvals + 3;
	const uint8_t bitlen = neededbits(run_long);

	bit_init_read(in);

	while (out < end) {
		uint8_t val = bit_read(bitlen);
		uint8_t src = 0;

		if (val < zero_short) {
			*out++ = valtab[val];
		} else if (val > zero_long) {
			src = valtab[bit_read(bitlen)];
		}

		uint16_t len;
		if (val == zero_short || val == run_short) {
			len = bit_read(8);
			memset(out, src, len);
			out += len;
		} else if (val == zero_long || val == run_long) {
			len = bit_read(8);
			len |= bit_read(6) << 8;
			if (!len)
				len = 1 << 14;

			memset(out, src, len);
			out += len;
		}
	}

	if (out != end) abort();
//	printf("wrote %ld bytes, %u expected\n", out - outstart, outlen);
}
