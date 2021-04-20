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

void tunstall_decomp(const u8 *in, u8 *out, const u16 outlen) {

	const u8 * const origout = out;
	u16 numentries = *in++;
	if (!numentries)
		numentries = 256;

	u8 dict[512];
	u8 lens[256];
	u16 starts[256];
	u8 cur = 0;
	u16 i;
	u8 nonones;
	u8 ones;

	while (1) {
		const u8 num = *in++;
		const u8 len = *in++;

		memset(&lens[cur], len, num);
		cur += num;

		if (len == 1) {
			ones = num;
			break;
		}
		// 1 is omitted if obvious
		if (len == 2) {
			ones = *in++;
			memset(&lens[cur], 1, ones);
			break;
		}
	}
	nonones = numentries - ones;

	// Fill in starts array
	u16 cum = 0, nononecum = 0;
	for (i = 0; i < numentries; i++) {
		starts[i] = cum;
		cum += lens[i];
		if (i < nonones)
			nononecum += lens[i];
	}

	// Read dict
	memcpy(dict, in, nononecum);
	in += nononecum;

	// Ones as a bitmap
	u16 dictpos = nononecum;
	for (i = 0; i < 32; i++) {
		const u8 byte = *in++;
		if (!byte)
			continue;
		u8 bit;
		for (bit = 0; bit < 8; bit++) {
			if (byte & (1 << bit)) {
				dict[dictpos++] = i * 8 + bit;
			}
		}
	}
/*
	for (i = 0; i < numentries; i++)
		printf("%u: len %u: first %u\n", i, lens[i], dict[starts[i]]);
*/
	// Unpack stream
	do {
		cur = *in++;
		const u8 len = lens[cur];
		memcpy(out, &dict[starts[cur]], len);
		out += len;
	} while (out - origout < outlen);

	if (out - origout != outlen) abort();
}
