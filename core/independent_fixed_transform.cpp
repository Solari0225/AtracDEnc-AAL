/* Independent loop-based reconstruction of the observed integer transform.
 * Evidence: narrow math-function capture, region offset 0x37be40, 950 bytes;
 * multiply semantics from region offset 0x37c200, 50 bytes.
 * Numeric permutation/window/twiddle constants are kept separately.
 */
#include "independent_fixed_transform.h"

#include <cstring>

#include "independent_fixed_transform_tables.inc"

namespace {

// Explicit modulo-2^32 arithmetic: signed overflow never occurs in C++.
int32_t signed_word(uint32_t word) {
    int32_t result;
    std::memcpy(&result, &word, sizeof result);
    return result;
}

int32_t add_wrap(int32_t a, int32_t b) {
    return signed_word(uint32_t(a) + uint32_t(b));
}

int32_t sub_wrap(int32_t a, int32_t b) {
    return signed_word(uint32_t(a) - uint32_t(b));
}

int32_t neg_wrap(int32_t a) {
    return signed_word(uint32_t(0) - uint32_t(a));
}

int32_t half_floor(int32_t a) {
    const uint32_t bits = uint32_t(a);
    return signed_word((bits >> 1) | (bits & 0x80000000u));
}

} // namespace

extern "C" int32_t aal_fixed_mul32x16(int32_t value, int coefficient,
                                      int shift) {
    // The observed helper first sign-extends its low coefficient half-word.
    const uint16_t coefficient_bits = uint16_t(coefficient);
    const int signed_coefficient = coefficient_bits < 0x8000u
        ? int(coefficient_bits) : int(coefficient_bits) - 65536;
    const int64_t product = int64_t(value) * signed_coefficient;
    const unsigned count = unsigned(shift) & 31u;
    return signed_word(uint32_t(uint64_t(product) >> count));
}

extern "C" void aal_fixed_imdct(int32_t overlap[128], unsigned band,
                                 int scale, int32_t inout[256]) {
    int32_t fft[256];
    int32_t rotated[256];

    for (unsigned i = 0; i < 256; ++i) {
        const unsigned source = (band & 1u) ? 255u - i : i;
        fft[kAalPermutation[i]] = inout[source];
    }

    // Seven normalized stages, operating on 128 interleaved complex values.
    // The twiddle table is stored stage-by-stage: 1, 2, 4, ..., 64 values.
    unsigned twiddle_base = 0;
    for (unsigned span = 1; span <= 64; span *= 2) {
        const unsigned half_words = span * 2;
        const unsigned group_words = half_words * 2;
        for (unsigned group = 0; group < 256; group += group_words) {
            for (unsigned j = 0; j < span; ++j) {
                const unsigned lo = group + j * 2;
                const unsigned hi = lo + half_words;
                const unsigned twiddle = twiddle_base + j;
                const int32_t real = fft[hi];
                const int32_t imag = fft[hi + 1];
                const int32_t product_real = add_wrap(
                    aal_fixed_mul32x16(real, kAalCosine[twiddle], 15),
                    aal_fixed_mul32x16(imag, kAalSine[twiddle], 15));
                const int32_t product_imag = sub_wrap(
                    aal_fixed_mul32x16(real, kAalSine[twiddle], 15),
                    aal_fixed_mul32x16(imag, kAalCosine[twiddle], 15));
                const int32_t old_real = fft[lo];
                const int32_t old_imag = fft[lo + 1];
                fft[lo] = half_floor(add_wrap(old_real, product_real));
                fft[lo + 1] = half_floor(add_wrap(old_imag, product_imag));
                fft[hi] = half_floor(sub_wrap(old_real, product_real));
                fft[hi + 1] = half_floor(sub_wrap(old_imag, product_imag));
            }
        }
        twiddle_base += span;
    }

    // The second halves of the same Q15 tables contain the post-rotation.
    for (unsigned i = 0; i < 128; ++i) {
        const int32_t real = fft[i * 2];
        const int32_t imag = fft[i * 2 + 1];
        const int cosine = kAalCosine[128 + i];
        const int sine = kAalSine[128 + i];
        rotated[i * 2] = add_wrap(
            aal_fixed_mul32x16(real, cosine, 15),
            aal_fixed_mul32x16(imag, sine, 15));
        rotated[255 - i * 2] = sub_wrap(
            aal_fixed_mul32x16(real, sine, 15),
            aal_fixed_mul32x16(imag, cosine, 15));
    }

    // Previous overlap has a fixed shift of 13. The other branch uses the
    // caller-supplied exponent + 9, masked like an x86 shift by the helper.
    const int current_shift = signed_word(uint32_t(scale) + 9u);
    for (unsigned i = 0; i < 128; ++i) {
        const int32_t old_overlap = overlap[i];
        const int32_t current = rotated[255 - i];
        const int rising = kAalWindow[128 + i];
        const int falling = kAalWindow[127 - i];
        inout[127 - i] = add_wrap(
            aal_fixed_mul32x16(old_overlap, rising, 13),
            aal_fixed_mul32x16(current, falling, current_shift));
        inout[128 + i] = sub_wrap(
            aal_fixed_mul32x16(old_overlap, falling, 13),
            aal_fixed_mul32x16(current, rising, current_shift));
        overlap[i] = neg_wrap(rotated[i]);
    }
}
