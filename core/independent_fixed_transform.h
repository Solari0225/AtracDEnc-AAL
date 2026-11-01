#ifndef AAL_INDEPENDENT_FIXED_TRANSFORM_H
#define AAL_INDEPENDENT_FIXED_TRANSFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Integer transform used by the AAL prediction path.
 *
 * inout initially holds 256 dequantized spectral coefficients. On return it
 * holds 256 windowed, overlapped subband samples. scale is a caller-supplied
 * exponent; its exact signal domain still needs caller verification. The
 * transform preserves the original integer domain.
 *
 * overlap is 128 signed 32-bit values, initially zero, persistent per channel
 * and per subband. band only controls odd-band spectral reversal. No global
 * mutable state, DLL, Sony code, or executable-code copying is used.
 */
void aal_fixed_imdct(int32_t overlap[128], unsigned band, int scale,
                     int32_t inout[256]);

/* Low 32 bits of the signed 32 x signed 16 product shifted by (shift & 31).
 * There is no rounding or saturation. shift == 0 or 32 returns the product's
 * low word. Exposed so dequantization/filtering can share the same semantics.
 */
int32_t aal_fixed_mul32x16(int32_t value, int coefficient, int shift);

#ifdef __cplusplus
}
#endif

#endif
