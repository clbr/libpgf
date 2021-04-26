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

uint16_t sb2_comp(const uint8_t *in, uint8_t *out, const uint16_t len) {

	uint16_t bytes[256] = { 0 }, i, used;
	uint8_t chr2pos[256] = { 0 };
	uint8_t * const start = out;

	if (len > 16384)
		return USHRT_MAX;

	for (i = 0; i < len; i++) {
		bytes[in[i]]++;
	}

	used = 0;
	for (i = 0; i < 256; i++) {
		if (bytes[i]) {
			chr2pos[i] = used;
			used++;
		}
	}

	if (used > 32) // Just a heuristic
		return USHRT_MAX;

//	printf("%u used values; neededbits %u\n",
//		used, neededbits(used - 1));

	*out++ = used;
	for (i = 0; i < 256; i++) {
		if (bytes[i]) {
			*out++ = i;
		}
	}
	bit_init_write(out);

	const uint8_t bitlen = neededbits(used - 1);

	for (i = 0; i < len; ) {
		const uint8_t cur = in[i];
		if (cur) {
			bit_write(chr2pos[cur], bitlen);
			i++;
		} else {
			// How many zeros?
			uint16_t end;
			for (end = i + 1; end < len; end++) {
				if (in[end])
					break;
			}

			end -= i;
			i += end;

			do {
				bit_write(0, bitlen);
				end--;
				if (end < 32) {
					bit_write(end, 6);
					end = 0;
				} else if (end < 2047) {
					const uint8_t val = (end & 31) | 32;
					bit_write(val, 6);
					bit_write(end >> 5, 6);
					end = 0;
				} else if (end == 2047) {
					// Special-case the edge case, split to 2046 + 1
					const uint8_t val = (2046 & 31) | 32;
					bit_write(val, 6);
					bit_write(2046 >> 5, 6);

					bit_write(0, bitlen);
					bit_write(0, 6);

					end = 0;
				} else {
					// For very long zero runs, output a long run and repeat
					const uint8_t val = (2047 & 31) | 32;
					bit_write(val, 6);
					bit_write(2047 >> 5, 6);
					end -= 2047;
				}
			} while (end);
		}
	}
	if (i != len) abort();

	out += bit_flush();

	return out - start;
}

void sb2_decomp(const uint8_t *in, uint8_t *out, const uint16_t outlen) {
//uint8_t * const outstart = out;
	const uint8_t * const end = out + outlen;
	const uint8_t numvals = *in++;
	const uint8_t * const valtab = in;
	in += numvals;

	const uint8_t bitlen = neededbits(numvals - 1);

	bit_init_read(in);

	while (out < end) {
		uint8_t val = bit_read(bitlen);

		*out++ = valtab[val];
		if (!val) { // zero rle?
			uint16_t len;
			len = bit_read(6);
			//printf("read %u (6 bits)\n", len);
			if (len & (1 << 5)) {
				len &= ~(1 << 5);
				len |= bit_read(6) << 5;
			}

			if (len) {
				memset(out, 0, len);
				out += len;
				//printf("adding %u zeroes, pos now %lu\n", len, out - outstart);
			}
		}
	}

	//printf("wrote %ld bytes, %u expected\n", out - outstart, outlen);
	if (out != end) abort();
}
/*
#include <stdio.h>

// sparseblock fse 14, lz4 84
// sparseblock2 fse 8, lz4 79

int main(int argc, char **argv) {

	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return 0;
	}
#if 0
// Test the bit routines
uint16_t i;
uint8_t invals[16384], tmpbuf[16384];
bit_init_write(tmpbuf);
for (i = 0; i < 16384; i++) {
	invals[i] = rand();
//printf("writing %u (%#02x), %u bits\n", invals[i], invals[i], neededbits(invals[i]));
	bit_write(invals[i], neededbits(invals[i]));
}
printf("wrote %u bytes\n", bit_flush());

bit_init_read(tmpbuf);
for (i = 0; i < 16384; i++) {
	uint8_t val = bit_read(neededbits(invals[i]));
	if (val != invals[i]) {
		printf("ERROR %u, got %u expected %u (%u bits)\n", i, val, invals[i],
			neededbits(invals[i]));
		return 0;
	}
}
#endif

	uint8_t buf[16384], outbuf[16384], testbuf[16384];
	memset(testbuf, 0x12, 16384);
	FILE *f = fopen(argv[1], "r");
	if (!f)
		return 1;
	const uint16_t inlen = fread(buf, 1, 16384, f);
	fclose(f);

	printf("Read %u bytes\n", inlen);

	const uint16_t num = sb2_comp(buf, outbuf, inlen);
	if (num == USHRT_MAX) {
		puts("not compressible");
		return 0;
	}
	printf("Compressed to %u bytes\n", num);
	sb2_decomp(outbuf, testbuf, inlen);

	if (memcmp(testbuf, buf, inlen)) {
		puts("FAIL");
		FILE *f = fopen("/tmp/fail", "w");
		fwrite(testbuf, inlen, 1, f);
		fclose(f);
		return 1;
	} else {
		puts("Success");
	}

	return 0;
}
*/
