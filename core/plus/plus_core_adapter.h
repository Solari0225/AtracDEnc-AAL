#ifndef INDEPENDENT_ATRAC3PLUS_CORE_ADAPTER_H
#define INDEPENDENT_ATRAC3PLUS_CORE_ADAPTER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Official AALP compatibility-core rate identifiers, in bits/second:
 * 48000,64000,96000,128000,160000,192000,256000,320000,352000.
 * Returns 0 for unsupported rates; otherwise 280..2048 bytes/frame.
 */
size_t at3pcore_frame_bytes(uint32_t bitrate);
/* Construct using one of the nine confirmed frame byte sizes, 280..2048. */
void* at3pcore_create(uint32_t frame_bytes);

/* Exact spectral metadata corresponding to the latest encoded physical frame.
 * Unused quantization units / coefficients are zero. Array order is channel,
 * then natural spectral order, before the odd-subband IMDCT reversal.
 * Tonal/gain/noise side data are absent in this encoder; flags below are
 * presently all zero but are explicit for the deterministic predictor ABI.
 */
typedef struct at3pcore_spectrum {
    uint32_t num_quant_units;
    uint32_t num_subbands;
    uint8_t word_length[2][32];
    uint8_t scale_factor[2][32];
    uint8_t code_table[2][32];
    int16_t quantized[2][2048];
    uint8_t window_shape[2][16];
    uint8_t swap_channels[16];
    uint8_t negate_coefficients[16];
} at3pcore_spectrum;

/* Consume exactly 2048 stereo signed16 sample frames (4096 interleaved values).
 * A null pcm pointer feeds a zero block for draining. Every successful call
 * emits one full ATRAC3plus frame. There is no extra adapter look-ahead:
 * PQF/MDCT analysis history provides the codec's inherent delay.
 * Returns frame bytes, or -1. It never returns a fabricated silent core.
 */
int at3pcore_encode(void* context,const int16_t* pcm,uint8_t* output,size_t capacity);
const char* at3pcore_last_error(const void* context);
int at3pcore_last_spectrum(const void* context,at3pcore_spectrum* output);
void at3pcore_destroy(void* context);

#ifdef __cplusplus
}
#endif
#endif
