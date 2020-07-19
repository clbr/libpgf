#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sparserle.h"

//#include <stdio.h>

static void genesc(uint8_t escbits, uint8_t esc[2]) {
	const uint8_t firstbit = __builtin_ctz(escbits);
	esc[0] = 1 << firstbit;
	escbits &= ~esc[0];

	const uint8_t secondbit = __builtin_ctz(escbits);
	esc[1] = 1 << secondbit;

	//esc[2] = esc[0] | esc[1];
}

uint16_t sparserle_comp(const uint8_t *in, uint8_t *out, const uint16_t len) {

	uint16_t bytes[256] = { 0 }, i, used, run;
	uint8_t cur, esc[2], escbits = 0, wasbyte = 0;
	uint8_t * const start = out;

	if (len > 32768)
		return USHRT_MAX;

	for (i = 0; i < len; i++) {
		bytes[in[i]]++;
	}

	used = 0;
	for (i = 0; i < 256; i++) {
		if (bytes[i]) {
			used++;
			escbits |= i;
		}
	}

	escbits = ~escbits;
	if (!bytes[0])
		return USHRT_MAX; // We expect zero to be there
	if (__builtin_popcount(escbits) < 2)
		return USHRT_MAX; // Need two escape bits
	if (used > 32) // Just a heuristic
		return USHRT_MAX;
	used--;
	genesc(escbits, esc);

//	printf("%u used values, not incl zero; escbits %#02x (%u bits); esc %#02x %#02x\n",
//		used, escbits, __builtin_popcount(escbits), esc[0], esc[1]);

	cur = *in;
	run = 1;

	*out++ = escbits;

	for (i = 1; i < len; i++) {
		if (i == len - 1 || in[i] != cur) {
			if (i == len - 1 && in[i] == cur)
				run++;

			if (run == 1) {
				*out++ = cur;
				if (cur) // one-byte zero would not be distinguishable
					wasbyte = 1;
				else
					wasbyte = 0;
//				printf("byte %u\n", cur);
			} else {
				if (cur == 0) {
					if (wasbyte) {
						out--;
						*out++ |= esc[0];
					} else {
						*out++ = esc[0];
					}
					// Zero runs don't output the byte
				} else {
					if (wasbyte) {
						out--;
						*out++ |= esc[1];
					} else {
						*out++ = esc[1];
					}
					*out++ = cur;
				}
				if (run < 128) {
					*out++ = run;
//					printf("short run %u, %u\n", run, cur);
				} else {
					*out++ = (run & 0x7f) | 0x80;
					*out++ = run >> 7;
//					printf("long run %u, %u\n", run, cur);
				}
				wasbyte = 0;
			}

			if (i == len - 1 && in[i] != cur)
				*out++ = in[i];

			run = 1;
			cur = in[i];
		} else {
			run++;
		}
	}

	return out - start;
}

void sparserle_decomp(const uint8_t *in, uint8_t *out, const uint16_t flen) {
	const uint8_t * const end = in + flen;
//const uint8_t * const outstart = out;
	const uint8_t escbits = *in++;
	uint8_t esc[2];
//puts("\n\noutput\n");
	genesc(escbits, esc);

	while (in < end) {
		uint8_t byte = *in++;

		if (byte & escbits) {
			// Did it include a byte?
			const uint8_t incl = byte & ~escbits;
			if (incl) {
				*out++ = incl;
				byte &= escbits;
//				printf("byte incl %u\n", incl);
			}

			// What type of run? Zero or non-zero?
			uint8_t src = 0;
			if (byte != esc[0])
				src = *in++;

			uint16_t len = *in++;
			if (len & 0x80) {
				len &= ~0x80;
				len |= *in++ << 7;
			}
//			printf("run of %u, %u\n", len, src);
			memset(out, src, len);
			out += len;
		} else {
			*out++ = byte;
//			printf("byte %u\n", byte);
		}
		if (in > end) abort();
	}

//	printf("wrote %lu bytes\n", out - outstart);
}
