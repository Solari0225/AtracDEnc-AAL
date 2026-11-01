#ifndef AAL_INDEPENDENT_LPC_H
#define AAL_INDEPENDENT_LPC_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* history[0..7] is oldest to newest; coefficient[0] multiplies the most recent
 * sample. Order is 0..8. Coefficients use 10 fractional bits. count <= 2048.
 * The native forward path reads signed low half-words of samples and
 * coefficients; the inverse path multiplies full int32 values. width is 1..32.
 * No caller history is changed. Input and output must not overlap.
 */
int aal_lpc_forward(const int32_t* input, int32_t* residual,
                    const int32_t coefficients[8], const int32_t history[8],
                    unsigned order, unsigned count, unsigned width);
int aal_lpc_inverse(const int32_t* residual, int32_t* output,
                    const int32_t coefficients[8], const int32_t history[8],
                    unsigned order, unsigned count, unsigned width);

/* Fixed predictors use two previous samples and Q4 coefficients:
 * mode 1..7: first coefficient {32,28,16,0,-16,-28,-32}, second -16.
 * history order, width, input/output restrictions are the same as above.
 */
int aal_fixed_prediction(const int32_t* input, int32_t* output,
                         const int32_t history[8], unsigned mode,
                         unsigned count, unsigned width, int inverse);

/* Decode two 11-bit and six 6-bit reflection-coefficient codes to Q10 values.
 * Then perform the native in-place integer step-up recursion. The latter
 * returns 1 if an intermediate coefficient falls outside [-32768,+32768].
 */
int aal_lpc_reflection_from_codes(const int32_t* codes, int32_t* reflection,
                                 unsigned order);
int aal_lpc_direct_from_reflection(const int32_t* reflection,
                                   int32_t* coefficients, unsigned order);

#ifdef __cplusplus
}
#endif
#endif
