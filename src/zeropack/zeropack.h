#if defined (__cplusplus)
extern "C" {
#endif

#include <stdint.h>

uint16_t zeropack_comp_rec(const uint8_t *in, uint8_t *out, const uint16_t len);
void zeropack_decomp_rec(const uint8_t *in, uint8_t *out, const uint16_t outlen);

#if defined (__cplusplus)
} // extern C
#endif

