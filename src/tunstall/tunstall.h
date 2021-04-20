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

#ifndef TUNSTALL_H
#define TUNSTALL_H

#include <lrtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

u16 tunstall_comp(const u8 *in, u8 *out, const u16 len);
void tunstall_decomp(const u8 *in, u8 *out, const u16 outlen);

#ifdef __cplusplus
}
#endif

#endif
