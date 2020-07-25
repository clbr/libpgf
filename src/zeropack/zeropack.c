#include <limits.h>
#include <lrtypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "zeropack.h"

//#include <stdio.h>

// measured on the test data set, no wins vs fse over this used number
#define MAXUSED 100

void huffman(const u32 totalprob[256], const u32 len, u8 canonical[256]);
void canoncode(const u8 n, const u8 canon[256], u32 *val, u32 *bits);

static uint16_t zeropack_comp(const uint8_t *in, uint8_t *out, const uint16_t len) {

	uint16_t i, zeroes = 0;
	uint8_t bits[2048] = { 0 };
	uint8_t * const start = out;
	const uint8_t off = len % 8 ? 1 : 0;

	if (len > 16384 || len < 8)
		return USHRT_MAX;

	for (i = 0; i < len; i++) {
		if (in[i])
			bits[i / 8] |= 1 << i % 8;
		else
			zeroes++;
	}
	if (zeroes < len / 8)
		return USHRT_MAX;

	memcpy(out, bits, len / 8 + off);
	out += len / 8 + off;

	for (i = 0; i < len; i++) {
		if (in[i])
			*out++ = in[i];
	}

	return out - start;
}

static void zeropack_decomp(const uint8_t *in, uint8_t *out, const uint16_t outlen) {
	const uint8_t * const end = out + outlen;
	const uint8_t *bits = in;
	in += outlen / 8;
	uint16_t i = 0;

	while (out < end) {
		if (bits[i / 8] & (1 << i % 8))
			*out++ = *in++;
		else
			*out++ = 0;
		i++;
	}

	if (out != end) abort();
}

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

static const unsigned char BitReverseTable256[256] = {
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
    R6(0), R6(2), R6(1), R6(3)
};

// Reversing bits moves a bit of work from decompression to compression side
static u16 bitreverse(u16 in, const u8 bits) {
	in = (BitReverseTable256[in & 0xff] << 8) |
		(BitReverseTable256[(in >> 8) & 0xff]);
	in >>= 16 - bits;
	return in;
}

struct node_t {
	uint16_t count;
	uint8_t sym;
};

static int nodecmp(const void *ap, const void *bp) {
	const struct node_t * const a = ap;
	const struct node_t * const b = bp;

	if (a->count > b->count)
		return -1;
	if (a->count < b->count)
		return 1;
	return 0;
}

uint16_t zeropack_comp_rec(const uint8_t *in, uint8_t *out, const uint16_t len) {

	#define MAXLEVELS 4

	uint8_t level;
	uint8_t buf[len], buf2[MAXLEVELS][len / 8];
	uint8_t * const start = out;
	uint16_t sizes[MAXLEVELS], bitsizes[MAXLEVELS];

	const uint16_t size0 = sizes[0] = zeropack_comp(in, buf, len);
	if (size0 >= USHRT_MAX)
		return USHRT_MAX;

	uint16_t srcsize = len / 8;

	memcpy(buf2[0], buf, len / 8);

	for (level = 1; level < MAXLEVELS; level++) {
		sizes[level] = zeropack_comp(buf2[level - 1], buf2[level], srcsize);
		if (sizes[level] >= srcsize)
			break;
		srcsize /= 8;
		bitsizes[level] = srcsize;
	}
	level--;

	*out++ = level;

	if (!level) {
		memcpy(out, buf, len / 8);
		out += len / 8;
	} else {
		// For the highest level, output everything
		memcpy(out, buf2[level], sizes[level]);
		out += sizes[level];
		level--;

		// Remaining levels, just the bytes
		for (; level > 0; level--) {
			memcpy(out, buf2[level] + bitsizes[level],
				sizes[level] - bitsizes[level]);
			out += sizes[level] - bitsizes[level];
		}

		// Last level, same, but different source buffer
		//memcpy(out, buf + len / 8, size0 - len / 8);
		//out += size0 - len / 8;
	}

	// Huff the bytes of level 0
	const uint8_t * const bytesrc = buf + len / 8;
	const uint16_t num = size0 - len / 8;
	uint16_t prob[256] = { 0 };
	uint16_t i;

	for (i = 0; i < num; i++) {
		prob[bytesrc[i]]++;
	}

	struct node_t nodes[256];
	uint8_t largest = 0;
	uint16_t usedvals = 0;
	for (i = 0; i < 256; i++) {
		if (prob[i]) {
			nodes[usedvals] = (struct node_t) { .count = prob[i], .sym = i };
			usedvals++;
			largest = i;
		}
	}
	if (!usedvals || usedvals > MAXUSED)
		return USHRT_MAX;

	qsort(nodes, usedvals, sizeof(struct node_t), nodecmp);

	u32 tprob[MAXUSED];
	for (i = 0; i < usedvals; i++)
		tprob[i] = nodes[i].count;

	u8 canon[256] = { 0 };
	huffman(tprob, usedvals, canon);

	uint8_t longestbit = 0;
	for (i = 255; i >= 1; i--) {
		if (canon[i]) {
			longestbit = i;
			break;
		}
	}
	//printf("%u used vals, largest %u, longest bit %u\n", usedvals, largest, longestbit);

	if (longestbit < 1 || longestbit >= 16)
		return USHRT_MAX;

	*out++ = usedvals;
	*out++ = neededbits(largest);

	bit_init_write(out);

	for (i = 0; i < usedvals; i++)
		bit_write(nodes[i].sym, neededbits(largest));

	bit_write(longestbit, 4);

	for (i = 1; i <= longestbit; i++) {
		if (canon[i] >= 16)
			return USHRT_MAX;
		bit_write(canon[i], 4);
	}

	for (i = 0; i < num; i++) {
		u32 val, bits;
		u8 k;
		for (k = 0; k < usedvals; k++)
			if (nodes[k].sym == bytesrc[i])
				break;

		canoncode(k, canon, &val, &bits);
		val = bitreverse(val, bits);

		while (bits) {
			const u8 step = bits < 8 ? bits : 8;
			bit_write(val & 0xff, step);
			bits -= step;
			val >>= step;
		}
	}

	// Padding for the reader speed
	bit_write(0, 8);
	bit_write(0, 8);

	out += bit_flush();
	return out - start;
}

/*
static uint16_t ipow(uint16_t base, uint16_t exp) {
	uint16_t out = 1;

	while (exp) {
		if (exp & 1)
			out *= base;
		exp >>= 1;
		base *= base;
	}

	return out;
}*/

static u8 huffbytes[MAXUSED];
static u16 huffstate;
static u8 hsbits;
static u8 hsused;
static u16 hsand[MAXUSED];
static u8 hslen[MAXUSED];

// partial accel table
static u8 habyte[32];
static u8 habits[32];
static u8 hastart;

static const uint16_t pow8[MAXLEVELS] = { 1, 8, 64, 512 };
static uint16_t canoncodes[MAXUSED] = { 0 };
static const uint8_t *bytepos[MAXLEVELS];

#define REC(level, levelminus) \
static uint16_t inner_rec ## level(const uint8_t val, uint8_t *out) { \
\
	const uint16_t num = pow8[level]; \
	uint8_t i; \
\
	uint16_t wrote = 0; \
	for (i = 0; i < 8; i++) { \
		if (val & (1 << i)) { \
			const uint16_t got = inner_rec ## levelminus(*bytepos[level]++, \
							out); \
			if (got != num) { \
				/*printf("err, wrote %u expected %u\n", got, num);*/ \
				abort(); \
			} \
			out += num; \
			wrote += num; \
		} else { \
			memset(out, 0, num); \
			out += num; \
			wrote += num; \
		} \
	} \
\
	return wrote; \
}

static uint8_t gethuff() {
	uint8_t out = 0;

	// Read bits until we have a huff match
	if (!hsbits) {
		huffstate = bit_read(8);
		huffstate |= bit_read(8) << 8;
		hsbits = 16;
	} else if (hsbits < 8) {
		huffstate |= bit_read(8) << hsbits;
		hsbits += 8;
	}

	u8 h;

	h = huffstate & 0x1f;
	if (habits[h]) {
		hsbits -= habits[h];
		huffstate >>= habits[h];
		return habyte[h];
	}

	for (h = hastart; h < hsused; h++) {
		while (hslen[h] > hsbits) {
			if (hsbits <= 8) {
				huffstate |= bit_read(8) << hsbits;
				hsbits += 8;
			} else if (hsbits <= 12) {
				huffstate |= bit_read(4) << hsbits;
				hsbits += 4;
			} else {
				huffstate |= bit_read(1) << hsbits;
				hsbits++;
			}
		}
		if ((huffstate & hsand[h]) == canoncodes[h]) {
			out = huffbytes[h];
			hsbits -= hslen[h];
			huffstate >>= hslen[h];
			break;
		}
	}
	if (h == hsused) {
//		printf("err, not found\n");
		abort();
	}

	return out;
}

static uint16_t inner_rec0(const uint8_t val, uint8_t *out) {

	uint64_t v = 0;

	static const void * const jmp[16] = {
		&&low0,
		&&low1,
		&&low2,
		&&low3,
		&&low4,
		&&low5,
		&&low6,
		&&low7,
		&&low8,
		&&low9,
		&&low10,
		&&low11,
		&&low12,
		&&low13,
		&&low14,
		&&low15,
	};
	static const void * const jmp2[16] = {
		&&hi0,
		&&hi1,
		&&hi2,
		&&hi3,
		&&hi4,
		&&hi5,
		&&hi6,
		&&hi7,
		&&hi8,
		&&hi9,
		&&hi10,
		&&hi11,
		&&hi12,
		&&hi13,
		&&hi14,
		&&hi15,
	};

	goto *jmp[val & 0xf];

#define LOW(b) \
	if (b & (1 << 0)) v |= (uint64_t) gethuff() << 0 * 8; \
	if (b & (1 << 1)) v |= (uint64_t) gethuff() << 1 * 8; \
	if (b & (1 << 2)) v |= (uint64_t) gethuff() << 2 * 8; \
	if (b & (1 << 3)) v |= (uint64_t) gethuff() << 3 * 8;

#define ENTRY(e) low ## e: LOW(e) goto next;

	ENTRY(0)
	ENTRY(1)
	ENTRY(2)
	ENTRY(3)
	ENTRY(4)
	ENTRY(5)
	ENTRY(6)
	ENTRY(7)
	ENTRY(8)
	ENTRY(9)
	ENTRY(10)
	ENTRY(11)
	ENTRY(12)
	ENTRY(13)
	ENTRY(14)
	ENTRY(15)

#undef ENTRY
#undef LOW

next:
	goto *jmp2[val >> 4];

#define HI(b) \
	if (b & (1 << 0)) v |= (uint64_t) gethuff() << 4 * 8; \
	if (b & (1 << 1)) v |= (uint64_t) gethuff() << 5 * 8; \
	if (b & (1 << 2)) v |= (uint64_t) gethuff() << 6 * 8; \
	if (b & (1 << 3)) v |= (uint64_t) gethuff() << 7 * 8;

#define ENTRY(e) hi ## e: HI(e) goto out;

	ENTRY(0)
	ENTRY(1)
	ENTRY(2)
	ENTRY(3)
	ENTRY(4)
	ENTRY(5)
	ENTRY(6)
	ENTRY(7)
	ENTRY(8)
	ENTRY(9)
	ENTRY(10)
	ENTRY(11)
	ENTRY(12)
	ENTRY(13)
	ENTRY(14)
	ENTRY(15)

#undef ENTRY
#undef HI

out:
	;
	union pt {
		uint8_t *u8;
		uint64_t *u64;
	} u;

	u.u8 = out;
	*u.u64 = v;

	return 8;
}

REC(1, 0)
REC(2, 1)
REC(3, 2)

static uint16_t (* const inner_recs[MAXLEVELS])(const uint8_t val, uint8_t *out) = {
	inner_rec0,
	inner_rec1,
	inner_rec2,
	inner_rec3,
};

static uint8_t popcount8(uint8_t in) {
	static const uint8_t bits16[16] = {
		0, 1, 1, 2,
		1, 2, 2, 3,
		1, 2, 2, 3,
		2, 3, 3, 4
	};

	return bits16[in & 0xf] + bits16[in >> 4];
}

void zeropack_decomp_rec(const uint8_t *in, uint8_t *out, const uint16_t outlen) {
	const uint8_t level = *in++;
	const uint8_t * const outstart = out;
	int8_t k;

	uint16_t bytes[MAXLEVELS], bitsizes[MAXLEVELS], i;
	bitsizes[0] = outlen / 8;
	for (i = 1; i <= level; i++) {
		bitsizes[i] = bitsizes[i - 1] / 8;
	}

	// First go through it, count bytes of each level
	const uint8_t *ptr = in;
	bytes[level] = 0;
	for (i = 0; i < bitsizes[level]; i++) {
		bytes[level] += popcount8(ptr[i]);
	}
	ptr += bitsizes[level];
	bytepos[level] = ptr;

	for (k = level - 1; k > 0; k--) {
		bytes[k] = 0;
		for (i = 0; i < bytes[k + 1]; i++) {
			bytes[k] += popcount8(*ptr++);
		}
		bytepos[k] = ptr;
	}
	if (level)
		bytepos[0] = ptr + bytes[1];

	const u8 usedvals = hsused = *bytepos[0]++;
	const u8 valbits = *bytepos[0]++;
	bit_init_read(bytepos[0]);

	for (i = 0; i < usedvals; i++)
		huffbytes[i] = bit_read(valbits);

	const u8 longestbit = bit_read(4);
	u8 canonlen[16] = { 0 };
	for (i = 1; i <= longestbit; i++)
		canonlen[i] = bit_read(4);

	for (i = 0; i < usedvals; i++) {
		u32 val, bits;
		canoncode(i, canonlen, &val, &bits);
		canoncodes[i] = bitreverse(val, bits);
		hsand[i] = (1 << bits) - 1;
		hslen[i] = bits;

//		printf("Canoncode %u %u: %#04x, len %u, and %#04x\n", i, huffbytes[i],
//			canoncodes[i],
//			hslen[i], hsand[i]);
	}

	memset(habits, 0, 32);
	hastart = 0;
	for (i = 0; i < 32; i++) {
		uint8_t h;
		for (h = 0; h < hsused; h++) {
			if (hslen[h] > 5) {
				hastart = h;
				break;
			}
			if ((i & hsand[h]) == canoncodes[h]) {
				habyte[i] = huffbytes[h];
				habits[i] = hslen[h];
				break;
			}
		}
	}

	huffstate = hsbits = 0;

	// Recursively unpack, straight to dest
	const uint16_t num = pow8[level] * 8;
	for (i = 0; i < bitsizes[level]; i++) {
		if (!in[i]) {
			memset(out, 0, num);
			out += num;
		} else {
			out += inner_recs[level](in[i], out);
		}
	}

	if (out != outstart + outlen) {
//		printf("Wrote %lu\n", out - outstart);
		abort();
	}
}
