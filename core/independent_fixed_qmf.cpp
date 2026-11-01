#include "independent_fixed_qmf.h"
#include <limits.h>

/* Public numerical synthesis-filter coefficients, in polyphase order.
 * Exact 24-word table is independently retained as a research observation:
 * runtime/static-math-tables.json, iqmf-coeff-int16-24, SHA-256
 * 73a2d241827530b7f1e966691b3152c4e9ecb27412c2f55b3ff96b22bd0ac382.
 * No executable instructions or runtime addresses are included here.
 */
static const int16_t kSynthesisCoefficients[24] = {
    -7, 19, -56, 133, -277, 513, -881, 1424,
    -2235, 3560, -6514, 30424, 8655, -2858, 1232, -512,
    161, -5, -50, 51, -35, 15, -4, -1
};

static int32_t signed_word(uint32_t value)
{
    /* Avoid implementation-defined uint32_t -> int32_t conversion. */
    return value <= INT32_MAX ? (int32_t)value
                             : -1 - (int32_t)(UINT32_MAX - value);
}

static int32_t arithmetic_shift(uint32_t value, unsigned shift)
{
    shift &= 31;
    if (shift == 0) return signed_word(value);
    uint32_t shifted = value >> shift;
    if (value & UINT32_C(0x80000000))
        shifted |= UINT32_MAX << (32 - shift);
    return signed_word(shifted);
}

static int16_t clip16(int32_t value)
{
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

static void matrix_pair(int16_t *first, int16_t *second, int reverse)
{
    const int32_t a = *first;
    const int32_t b = *second;
    *first = clip16(reverse ? b - a : a - b);
    *second = clip16(a + b);
}

extern "C" void aal_fixed_iqmf(int16_t state[AAL_FIXED_QMF_STATE_SAMPLES],
                               int16_t output[AAL_FIXED_QMF_OUTPUT_SAMPLES],
                               int low_bands_only)
{
    /* The observed native ABI tests the low 16 bits of the mode argument. */
    const int low = (uint16_t)low_bands_only != 0;
    for (unsigned sample = 0; sample < 1024; sample += 4) {
        const unsigned input = 138 + sample;
        matrix_pair(state + input, state + input + 1, 0);

        uint32_t first_even = 0, first_odd = 0;
        uint32_t high_even = 0, high_odd = 0;
        if (!low)
            matrix_pair(state + input + 2, state + input + 3, 1);

        for (unsigned tap = 0; tap < 24; ++tap) {
            const unsigned position = input - 4 * tap;
            const int32_t reverse = kSynthesisCoefficients[23 - tap];
            const int32_t forward = kSynthesisCoefficients[tap];
            /* Unsigned accumulation explicitly reproduces modulo-2^32
             * arithmetic. Each signed 16x16 product itself fits int32_t.
             */
            first_even += (uint32_t)((int32_t)state[position] * reverse);
            first_odd += (uint32_t)((int32_t)state[position + 1] * forward);
            if (!low) {
                high_even += (uint32_t)((int32_t)state[position + 2] * reverse);
                high_odd += (uint32_t)((int32_t)state[position + 3] * forward);
            }
        }

        const unsigned middle = 46 + sample;
        const int32_t even = arithmetic_shift(first_even, 16);
        const int32_t odd = arithmetic_shift(first_odd, 16);
        if (low) {
            state[middle] = state[middle + 1] = clip16(even);
            state[middle + 2] = state[middle + 3] = clip16(odd);
        } else {
            const int32_t high_e = arithmetic_shift(high_even, 16);
            const int32_t high_o = arithmetic_shift(high_odd, 16);
            state[middle] = clip16(even - high_e);
            state[middle + 1] = clip16(even + high_e);
            state[middle + 2] = clip16(odd - high_o);
            state[middle + 3] = clip16(odd + high_o);
        }

        uint32_t even0 = 0, odd0 = 0, even1 = 0, odd1 = 0;
        for (unsigned tap = 0; tap < 24; ++tap) {
            const unsigned position = middle - 2 * tap;
            const int32_t reverse = kSynthesisCoefficients[23 - tap];
            const int32_t forward = kSynthesisCoefficients[tap];
            even0 += (uint32_t)((int32_t)state[position] * reverse);
            odd0 += (uint32_t)((int32_t)state[position + 1] * forward);
            even1 += (uint32_t)((int32_t)state[position + 2] * reverse);
            odd1 += (uint32_t)((int32_t)state[position + 3] * forward);
        }
        output[sample] = clip16(arithmetic_shift(even0, 13));
        output[sample + 1] = clip16(arithmetic_shift(odd0, 13));
        output[sample + 2] = clip16(arithmetic_shift(even1, 13));
        output[sample + 3] = clip16(arithmetic_shift(odd1, 13));
    }
}
