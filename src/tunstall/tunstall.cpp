/*  A fast brute-force Tunstall implementation
    Copyright (C) 2021 Lauri Kasanen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <limits.h>
#include <lrtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tunstall.h"

#define XXH_INLINE_ALL
#include "xxhash.h"

#define MAXSIZE 32768

class piecer {
public:
	piecer(const u8 *mem_): mem(mem_) {
		clear();
	}

	void add(const u16 addr, const u8 len) {
		u16 i;
		const u16 curmax = num[len - 2];
		const u16 hashed = hash(addr, len);
		for (i = hashmap[hashed]; i < MAXSIZE; i = hashnext[i]) {
			if (!memcmp(&mem[addr], &mem[entries[len - 2][i].addr], len)) {
				// Found it, just add one
				entrynum[i]++;

				if (entrynum[i] > largestnum[len - 2]) {
					largestnum[len - 2] = entrynum[i];
					largestpos[len - 2] = i;
				}
				return;
			}
		}

		// Didn't find it, add a new one, clearing it first
		entries[len - 2][curmax].addr = addr;
		entrynum[curmax] = 1;

		hashnext[curmax] = hashmap[hashed];
		hashmap[hashed] = curmax;

		if (!largestnum[len - 2]) {
			largestnum[len - 2] = 1;
			largestpos[len - 2] = curmax;
		}

		num[len - 2]++;
	}

	u32 size(const u8 len) const {
		return num[len - 2];
	}

	u16 bestnum(const u8 len) const {
		return largestnum[len - 2];
	}

	const u8 *best(const u8 len) const {
/*		u16 i, opts = 0;
		const u16 curmax = num[len - 2];
		for (i = 0; i < curmax; i++) {
			if (entries[len - 2][i].num != largestnum[len - 2])
				continue;
			opts++;
		}
		if (opts > 1) printf("%u options\n", opts);*/

		return &mem[entries[len - 2][largestpos[len - 2]].addr];
	}

	void clearhash() {
/*		u16 i;
		u16 used = 0, smallnum = 65535, bignum = 0;
		for (i = 0; i < 64; i++) {
			if (!hashnum[i])
				continue;
			used++;
			if (hashnum[i] < smallnum)
				smallnum = hashnum[i];
			if (hashnum[i] > bignum)
				bignum = hashnum[i];
		}
		printf("%u/64 buckets used, min/max %u %u\n", used, smallnum, bignum);*/

		memset(hashmap, 0xff, sizeof(hashmap));
		memset(hashnext, 0xff, sizeof(hashnext));
		memset(entrynum, 0, sizeof(entrynum));
	}

	void clear() {
		memset(num, 0, sizeof(num));
		memset(largestpos, 0, sizeof(largestpos));
		memset(largestnum, 0, sizeof(largestnum));

		clearhash();
	}

private:
	const u8 *mem;

	struct entry {
		u16 addr;
	};

	entry entries[128 + 1 - 2][MAXSIZE];
	u16 num[128 + 1 - 2];

	u16 largestpos[128 + 1 - 2], largestnum[128 + 1 - 2];

	// Hashes and running numbers are only used while adding, during one len
	u16 entrynum[MAXSIZE];
	u16 hashnext[MAXSIZE];
	u16 hashmap[4096];

	u16 hash(const u16 addr, const u8 len) const {
		return XXH3_64bits(&mem[addr], len) & 4095;
	}
};

struct entry {
	u8 data[128];
	u8 len;
};

static int lencmp(const void *ap, const void *bp) {
	const struct entry * const a = (struct entry *) ap;
	const struct entry * const b = (struct entry *) bp;

	if (a->len > b->len)
		return -1;
	if (a->len < b->len)
		return 1;

	// Secondary sort on the first byte value
	if (a->data[0] < b->data[0])
		return -1;
	if (a->data[0] > b->data[0])
		return 1;

	return 0;
}

#define verbose 0

u16 tunstall_comp(const u8 *in, u8 *out, const u16 len) {

	if (len > MAXSIZE)
		return USHRT_MAX;

	// Analyze
	piecer *pc = new piecer(in);
	u32 maxes[129];
	u8 lastlevel = 128;

	u8 bytes[256] = { 0 };

	u32 i;
	for (i = 0; i < len; i++) {
		bytes[in[i]] = 1;
	}

	struct entry entries[256];

	u32 used = 0;
	for (i = 0; i < 256; i++) {
		if (bytes[i]) {
			entries[used].data[0] = i;
			entries[used].len = 1;
			used++;
		}
	}
	if (verbose)
		printf("%u values used\n", used);
	if (used == 256)
		return USHRT_MAX;

	for (i = 2; i <= 128; i++) {
		u32 p;
		for (p = 0; p < len; p++) {
			if (p + i >= len)
				break;
			pc->add(p, i);
		}
		pc->clearhash();

		if (verbose)
			printf("Length %u had %u unique matches\n", i, pc->size(i));

		const u16 max = pc->bestnum(i);
		if (verbose)
			printf("Largest amount was %u\n", max);
		maxes[i] = max;

		if (max == 1) {
			lastlevel = i;
			break;
		}
	}

	if (verbose)
		puts("");

	u32 bestamount = 0;
	u8 best = 0;

	for (i = 2; i <= lastlevel; i++) {
		const u32 mul = i * maxes[i];
		if (mul > bestamount) {
			best = i;
			bestamount = mul;
		}
	}

	if (verbose)
		printf("Best amount was %u, at %u\n", bestamount, best);

	u32 numentries = used + 1;

	const u8 *ptr = pc->best(best);
	memcpy(entries[used].data, ptr, best);
	entries[used].len = best;

	if (verbose) {
		printf("Contents: ");
		for (i = 0; i < best; i++) {
			printf("%u,", ptr[i]);
		}
		puts("");
	}

	// Erase them from the memmap
	u8 memmap[MAXSIZE] = { 0 };
	u32 erased = 0;

	for (i = 0; i < (u16) (len - best); i++) {
		if (!memcmp(&entries[used].data[0], &in[i], best)) {
			memset(&memmap[i], 1, best);
			i += best - 1;
			erased += best;
		}
	}

	// We have our first entry. Time to iterate
	while (numentries < 256) {
		if (verbose)
			printf("Iterating. %u entries found, %u/%u bytes\n",
				numentries, erased, len);

		memset(maxes, 0, 129 * sizeof(u32));
		pc->clear();

		for (i = 2; i <= lastlevel; i++) {
			u32 p;
			for (p = 0; p < len; p++) {
				if (p + i >= len)
					break;

				if (u8 *ptr = (u8 *) memrchr(&memmap[p], 1, i)) {
					const u32 dist = ptr - &memmap[p];
					if (dist)
						p += dist - 1;
					continue;
				}

				pc->add(p, i);
			}
			pc->clearhash();

			const u16 max = pc->bestnum(i);
			if (verbose)
				printf("Length %u had %u unique matches, max %u\n", i,
					pc->size(i), max);

			maxes[i] = max;

			if (max == 1) {
				lastlevel = i;
				break;
			} else if (!max) {
				lastlevel = i - 1;
				break;
			}
		}

		bestamount = 0;
		best = 0;

		for (i = 2; i <= lastlevel; i++) {
			const u32 mul = i * maxes[i];
			if (mul > bestamount) {
				best = i;
				bestamount = mul;
			}
		}
		if (verbose)
			printf("Best amount was %u, at %u\n", bestamount, best);

		if (!best) // Found nothing
			break;

		// If dict length exceeds 512
		u32 dlen = 0;
		for (i = 0; i < numentries; i++)
			dlen += entries[i].len;
		if (dlen + best > 512) {
			if (verbose)
				puts("Dictionary length capped");
			break;
		}

		// Found a new best
		ptr = pc->best(best);
		memcpy(entries[numentries].data, ptr, best);
		entries[numentries].len = best;
		numentries++;

		if (verbose) {
			printf("Contents: ");
			for (i = 0; i < best; i++) {
				printf("%u,", ptr[i]);
			}
			puts("");
		}

		// Erase from map
		for (i = 0; i < (u16) (len - best); i++) {

			if (u8 *ptr = (u8 *) memrchr(&memmap[i], 1, best)) {
				const u32 dist = ptr - &memmap[i];
				if (dist)
					i += dist - 1;
				continue;
			}

			if (!memcmp(&entries[numentries - 1].data[0], &in[i], best)) {
				memset(&memmap[i], 1, best);
				i += best - 1;
				erased += best;
			}
		}

		// End conditions
		// If longest open streak is 1
		if (lastlevel == 1) {
			if (verbose)
				puts("No more runs to find");
			break;
		}
	}

	if (verbose)
		puts("");

	u32 dlen = 0;
	for (i = 0; i < numentries; i++)
		dlen += entries[i].len;
	if (verbose)
		printf("Dictionary has %u entries, %u total length.\n", numentries, dlen);

	qsort(entries, numentries, sizeof(struct entry), lencmp);

	u32 k;
	if (verbose) {
		for (i = 0; i < numentries; i++) {
			printf("Entry %u: len %u: ", i, entries[i].len);
			for (k = 0; k < entries[i].len; k++)
				printf("%u,", entries[i].data[k]);
			puts("");
		}
	}

	// Output
	const u8 * const origout = out;

	// N entries
	*out++ = numentries;

	// Their sizes, as pairs
	k = 1;
	u8 prevlen = 0;
	for (i = 0; i < numentries; i++) {
		if (i == numentries - 1 || entries[i + 1].len != entries[i].len) {
			*out++ = k;
			// Save a byte if 1 is obvious
			if (prevlen != 2 || entries[i].len != 1)
				*out++ = entries[i].len;
			prevlen = entries[i].len;
			k = 1;
		} else {
			k++;
		}
	}

	// Dict
	for (i = 0; i < numentries; i++) {
		if (entries[i].len == 1)
			break;
		memcpy(out, entries[i].data, entries[i].len);
		out += entries[i].len;
	}

	// Ones as a bitmap
	memset(bytes, 0, 32);
	for (; i < numentries; i++) {
		const u8 x = entries[i].data[0];
		bytes[x / 8] |= 1 << (x % 8);
	}

	memcpy(out, bytes, 32);
	out += 32;

	// Stream
	u32 greedysize = 0;
	for (i = 0; i < len;) {
		for (k = 0; k < numentries; k++) {
			if (i + entries[k].len > len)
				continue;
			if (!memcmp(&in[i], &entries[k].data[0], entries[k].len)) {
				greedysize++;
				//*out++ = k;
				i += entries[k].len;
				goto next;
			}
		}

		if (verbose)
			printf("Not found at %u, val %u\n", i, in[i]);
		abort();

		next:;
	}
	if (i != len)
		abort();

	if (verbose)
		printf("Greedy stream size %u\n", greedysize);

	// Perfect stream
	u8 perflen[MAXSIZE] = { 0 };
	u8 perfmatch[MAXSIZE] = { 0 };
	u16 hops[MAXSIZE] = { 0 };

	for (i = len - 1; i < len; i--) {
		u8 numhits = 0;
		u8 hitlen[256] = { 0 };
		u8 hitmatch[256] = { 0 };

		for (k = 0; k < numentries; k++) {
			if (i + entries[k].len > len)
				continue;
			if (!memcmp(&in[i], &entries[k].data[0], entries[k].len)) {
				hitmatch[numhits] = k;
				hitlen[numhits] = entries[k].len;
				numhits++;
			}
		}

		// Pick the best one
		if (verbose)
			printf("At %u, %u hits\n", i, numhits);

		if (!numhits)
			abort();
		if (numhits == 1) {
			perflen[i] = hitlen[0];
			perfmatch[i] = hitmatch[0];
			if (i + hitlen[0] > len)
				hops[i] = 1;
			else
				hops[i] = 1 + hops[i + hitlen[0]];
		} else {
			u8 best = 0;
			u16 besthops = USHRT_MAX;

			for (k = 0; k < numhits; k++) {
				u16 curhops = 1;
				if (i + hitlen[k] <= len)
					curhops += hops[i + hitlen[k]];

				if (verbose)
					printf("\tChoice %u had %u hops, len %u\n",
						k, curhops, hitlen[k]);

				if (curhops < besthops) {
					besthops = curhops;
					best = k;
				}
			}

			if (verbose)
				printf("\tBest choice was %u hops\n", besthops);

			perflen[i] = hitlen[best];
			perfmatch[i] = hitmatch[best];
			hops[i] = besthops;
		}
	}

	u32 perfsize = 0;
	for (i = 0; i < len;) {
		k = perfmatch[i];
		perfsize++;
		*out++ = k;
		i += entries[k].len;
	}
	if (i != len)
		abort();

	if (verbose)
		printf("Perfect stream size %u, saved %u bytes/%.2f%%\n",
			perfsize, greedysize - perfsize,
			100 - perfsize * 100.0f / greedysize);

	delete pc;

	return out - origout;
}
