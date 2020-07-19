#if defined (__cplusplus)
extern "C" {
#endif

#include <stdint.h>

uint16_t sparserle_comp(const uint8_t *in, uint8_t *out, const uint16_t len);
void sparserle_decomp(const uint8_t *in, uint8_t *out, const uint16_t inlen);

uint16_t sparsebitrle_comp(const uint8_t *in, uint8_t *out, const uint16_t len);
void sparsebitrle_decomp(const uint8_t *in, uint8_t *out, const uint16_t outlen);

#if defined (__cplusplus)
} // extern C
#endif
