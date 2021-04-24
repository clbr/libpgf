#include <limits.h>
#include <lrtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bitpack.h"

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#else
#define BP_BE
#endif

static uint8_t neededbits(uint8_t val) {
	uint8_t bits = 0;
	while (val) {
		bits++;
		val >>= 1;
	}
	return bits;
}

static u8 patchvals[256];
static u8 vals[16];
static u8 numpatches, numvals;
static u16 cumpatches;
static u8 valmap[256];

struct patch_t {
	u16 pos;
	u8 val;
};

static struct patch_t patches[32768];

static int patchcmp(const void *ap, const void *bp) {
	const struct patch_t *a = (struct patch_t *) ap;
	const struct patch_t *b = (struct patch_t *) bp;

	if (a->val < b->val)
		return -1;
	if (a->val > b->val)
		return 1;

	if (a->pos < b->pos)
		return -1;
	if (a->pos > b->pos)
		return 1;

	return 0;
}

static void writepatches(u16 curpatch, const u16 counts[], u8 *patchstart) {
	qsort(patches, curpatch, sizeof(struct patch_t),
		patchcmp);

	if (curpatch != cumpatches) abort();

	u16 p, i;
	curpatch = 0;
	for (p = 0; p < numpatches; p++) {
		const u16 curval = patches[curpatch].val;
		const u16 curmax = counts[curval];

		*patchstart++ = curval;
		*patchstart++ = curmax;

		for (i = 0; i < curmax; i++, curpatch++) {
			// BE pos of this patch
			*patchstart++ = patches[curpatch].pos >> 8;
			*patchstart++ = patches[curpatch].pos & 0xff;
		}
	}
}

static u16 bitpack_comp4(const u8 *in, u8 *out, const u16 len,
				const u8 patching, const u16 counts[]) {

	//printf("4-bit compression\n");

	if (len % 2)
		return USHRT_MAX;

	const u8 * const origout = out;
	u16 i;
	u16 curpatch = 0;

	*out++ = numvals;
	for (i = 0; i < numvals; i++)
		*out++ = vals[i];

	*out++ = numpatches;

	u8 *patchstart = out;
	if (patching) {
		out += numpatches * 2 + cumpatches * 2;
		//printf("Patches took %lu bytes\n", out - patchstart);
	}

	for (i = 0; i < len; i += 2) {
		const u8 a = *in++;
		const u8 b = *in++;

		u8 amap = valmap[a];
		u8 bmap = valmap[b];

		if (patching) {
			if (amap >= numvals) {
				amap = 0;

				patches[curpatch].pos = i;
				patches[curpatch].val = a;
				curpatch++;
			}
			if (bmap >= numvals) {
				bmap = 0;

				patches[curpatch].pos = i + 1;
				patches[curpatch].val = b;
				curpatch++;
			}
		}

		*out++ = amap | bmap << 4;
	}

	if (patching) {
		writepatches(curpatch, counts, patchstart);
	}

	//printf("Compressed %u to %lu\n", len, out - origout);

	return out - origout;
}

static u16 bitpack_comp3(const u8 *in, u8 *out, const u16 len,
				const u8 patching, const u16 counts[]) {

	//printf("3-bit compression\n");

	if (len % 8)
		return USHRT_MAX;

	const u8 * const origout = out;
	u16 i;
	u16 curpatch = 0;

	*out++ = numvals;
	for (i = 0; i < numvals; i++)
		*out++ = vals[i];

	*out++ = numpatches;

	u8 *patchstart = out;
	if (patching) {
		out += numpatches * 2 + cumpatches * 2;
		//printf("Patches took %lu bytes\n", out - patchstart);
	}

	for (i = 0; i < len; i += 8) {
		u8 reads[8], rmap[8], r;
		for (r = 0; r < 8; r++) {
			reads[r] = *in++;
			rmap[r] = valmap[reads[r]];
		}

		if (patching) {
			for (r = 0; r < 8; r++) {
				if (rmap[r] >= numvals) {
					rmap[r] = 0;

					patches[curpatch].pos = i + r;
					patches[curpatch].val = reads[r];
					curpatch++;
				}
			}
		}

		const u32 val = rmap[0] |
				rmap[1] << 3 |
				rmap[2] << 6 |
				rmap[3] << 9 |
				rmap[4] << 12 |
				rmap[5] << 15 |
				rmap[6] << 18 |
				rmap[7] << 21;

		// BE 24-bit value
		*out++ = val >> 16;
		*out++ = val >> 8 & 0xff;
		*out++ = val & 0xff;
	}

	if (patching) {
		writepatches(curpatch, counts, patchstart);
	}

	//printf("Compressed %u to %lu\n", len, out - origout);

	return out - origout;
}

u16 bitpack_comp(const u8 *in, u8 *out, const u16 len) {

	u16 counts[256] = { 0 };
	u16 i;

	for (i = 0; i < len; i++) {
		counts[in[i]]++;
	}

	const u16 limit = len / 256;
	u16 used = 0, patchused = 0;

	for (i = 0; i < 256; i++) {
		if (counts[i])
			used++;
		if (counts[i] >= limit)
			patchused++;
	}

	if (patchused > 16) // we only handle 3 and 4 bits
		return USHRT_MAX;

	// Use patching only if it brings us down a bit
	const u8 patchedbits = neededbits(patchused - 1);
	const u8 usedbits = neededbits(used - 1);
	const u8 patching = patchedbits != usedbits && usedbits > 3;

	if (!patching && used > 16)
		return USHRT_MAX;

	//printf("used %u, patchused %u, bits %u %u, patching %u\n",
	//	used, patchused, usedbits, patchedbits, patching);

	numpatches = numvals = cumpatches = 0;

	memset(valmap, 0xff, 256);

	if (patching) {
		for (i = 0; i < 256; i++) {
			if (!counts[i])
				continue;
			if (counts[i] < limit) {
				patchvals[numpatches] = i;
				numpatches++;
				cumpatches += counts[i];
			} else {
				vals[numvals] = i;
				valmap[i] = numvals;
				numvals++;
			}
		}
	} else {
		for (i = 0; i < 256; i++) {
			if (!counts[i])
				continue;
			vals[numvals] = i;
			valmap[i] = numvals;
			numvals++;
		}
	}

	if (numvals > 8)
		return bitpack_comp4(in, out, len, patching, counts);
	else
		return bitpack_comp3(in, out, len, patching, counts);
}

void bitpack_decomp(const u8 *in, u8 *out, const u16 outlen) {

	const u8 numvals = *in++;
	const u8 * const vals = in;
	in += numvals;
	const u8 numpatches = *in++;

	u8 i, p;

	//printf("Decomp. Numvals %u, numpatches %u\n", numvals, numpatches);

	const u8 * patchstart = in;
	if (numpatches) {
		// skip the patches for now
		for (i = 0; i < numpatches; i++) {
			in++; // val
			const u8 num = *in++;
			in += num * 2;
		}
	}

	const u8 * const end = out + outlen;
	u8 * const origout = out;

	if (numvals > 8) {
		// 4-bit
		u16 prep[256];

		for (i = 0; i < numvals; i++) {
			for (p = 0; p < numvals; p++) {
				const u8 pos = i << 4 | p;
#ifdef BP_BE
				prep[pos] = vals[p] << 8 | vals[i];
#else
				prep[pos] = vals[i] << 8 | vals[p];
#endif
			}
		}

		do {
			const u8 val = *in++;
			*(u16 *) out = prep[val];
			out += 2;
			//*out++ = vals[val & 0xf];
			//*out++ = vals[val >> 4];
		} while (out < end);
	} else {
		// 3-bit
		u16 prep[64];

		for (i = 0; i < numvals; i++) {
			for (p = 0; p < numvals; p++) {
				const u8 pos = i << 3 | p;
#ifdef BP_BE
				prep[pos] = vals[p] << 8 | vals[i];
#else
				prep[pos] = vals[i] << 8 | vals[p];
#endif
			}
		}

		do {
			u32 val = 0;
			val |= *in++ << 16;
			val |= *in++ << 8;
			val |= *in++;

			for (i = 0; i < 8; i += 2) {
				//*out++ = vals[val & 7];
				//val >>= 3;
				*(u16 *) out = prep[val & 63];
				out += 2;
				val >>= 6;
			}
		} while (out < end);
	}

	if (numpatches) {
		for (i = 0; i < numpatches; i++) {
			const u8 val = *patchstart++; // val
			const u8 num = *patchstart++;
			for (p = 0; p < num; p++) {
				u16 pos = 0;
				pos |= *patchstart++ << 8;
				pos |= *patchstart++;

				origout[pos] = val;
			}
		}
	}
}
/*
int main(int argc, char **argv) {

	uint8_t buf[16384], outbuf[16384], testbuf[16384] __attribute__((aligned(4)));

	if (argc < 2) {
		// Do tests
		u16 i, num;
		srand(time(NULL));

		printf("Testing normal 4\n");
		for (i = 0; i < 16384; i++)
			buf[i] = i % 16;

		num = bitpack_comp(buf, outbuf, 16384);
		#ifdef BENCH
		for (i = 0; i < 1000 * 10; i++)
		#endif
		bitpack_decomp(outbuf, testbuf, 16384);
		if (memcmp(testbuf, buf, 16384)) abort();

		printf("Testing normal 3\n");
		for (i = 0; i < 16384; i++)
			buf[i] = i % 8;

		num = bitpack_comp(buf, outbuf, 16384);
		#ifdef BENCH
		for (i = 0; i < 1000 * 10; i++)
		#endif
		bitpack_decomp(outbuf, testbuf, 16384);
		if (memcmp(testbuf, buf, 16384)) abort();

		printf("Testing patched 4\n");
		for (i = 0; i < 16384; i++)
			buf[i] = i % 16;
		buf[rand() % 16384] = 17;
		buf[rand() % 16384] = 17;
		buf[rand() % 16384] = 18;

		num = bitpack_comp(buf, outbuf, 16384);
		#ifdef BENCH
		for (i = 0; i < 1000 * 10; i++)
		#endif
		bitpack_decomp(outbuf, testbuf, 16384);
		if (memcmp(testbuf, buf, 16384)) abort();

		printf("Testing patched 3\n");
		for (i = 0; i < 16384; i++)
			buf[i] = i % 8;
		buf[rand() % 16384] = 17;
		buf[rand() % 16384] = 17;
		buf[rand() % 16384] = 18;

		num = bitpack_comp(buf, outbuf, 16384);
		#ifdef BENCH
		for (i = 0; i < 1000 * 10; i++)
		#endif
		bitpack_decomp(outbuf, testbuf, 16384);
		if (memcmp(testbuf, buf, 16384)) abort();

		printf("Testing random 16\n");
		for (i = 0; i < 16384; i++)
			buf[i] = rand() % 16;

		num = bitpack_comp(buf, outbuf, 16384);
		#ifdef BENCH
		for (i = 0; i < 1000 * 10; i++)
		#endif
		bitpack_decomp(outbuf, testbuf, 16384);
		if (memcmp(testbuf, buf, 16384)) abort();

		printf("Testing random mix\n");
		for (i = 0; i < 16384; i++)
			buf[i] = rand() % 8;
		for (i = 0; i < 256; i++)
			buf[rand() % 16384] = rand();

		num = bitpack_comp(buf, outbuf, 16384);
		#ifdef BENCH
		for (i = 0; i < 1000 * 10; i++)
		#endif
		bitpack_decomp(outbuf, testbuf, 16384);
		if (memcmp(testbuf, buf, 16384)) abort();

		return 0;
	}

	memset(testbuf, 0x12, 16384);
	FILE *f = fopen(argv[1], "r");
	if (!f)
		return 1;
	const uint16_t inlen = fread(buf, 1, 16384, f);
	fclose(f);

	printf("Read %u bytes\n", inlen);

	const uint16_t num = bitpack_comp(buf, outbuf, inlen);
	if (num == USHRT_MAX) {
		puts("not compressible");
		return 0;
	}
	printf("Compressed to %u bytes\n", num);
	bitpack_decomp(outbuf, testbuf, 16384);

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
