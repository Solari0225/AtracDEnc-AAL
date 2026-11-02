#ifndef SONY_AAL_RECOVERED_INTEGER_GAIN_H
#define SONY_AAL_RECOVERED_INTEGER_GAIN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Complete original AAL integer gain application math. points contains8pairs
 * (sample-location,level), including a terminal(255,4); only entries through
 * that terminator are read. Real syntax locations have already been multiplied
 * by8. mode's low16bits select direct integer(1) or extra8-bit shift(0).
 * Writes256clamped samples. No shared state, original code or host floating
 * arithmetic. Input/output/points must not overlap.0success,negative invalid.
 */
int sony_aal_integer_gain(const int32_t input[256],const int16_t points[16],
    uint32_t mode,int16_t output[256]);
#ifdef __cplusplus
}
#endif
#endif
