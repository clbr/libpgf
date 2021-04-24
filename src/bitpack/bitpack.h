#ifndef BITPACK_H
#define BITPACK_H

#include <lrtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

u16 bitpack_comp(const u8 *in, u8 *out, const u16 len);
void bitpack_decomp(const u8 *in, u8 *out, const u16 outlen);

#ifdef __cplusplus
}
#endif

#endif
