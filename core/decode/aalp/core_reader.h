#ifndef SONY_ORIGINAL_AALP_READER_H
#define SONY_ORIGINAL_AALP_READER_H
#include "fixed_predictor.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Parse one physical mono/stereo ATRAC3plus core frame into the complete input
 * of Sony's fixed AALP predictor. Tonal/PWC/noise syntax is consumed; it does
 * not enter that original fixed predictor graph. No foreign decoder library.
 * positions[0..9] are boundaries after header, WL, SF, codebooks, spectrum,
 * windows, gain, tones, noise and the terminating channel-unit marker.
 * Returns0 on success; negative stage number on malformed/truncated input. */
int sony_original_aalp_read_core(const uint8_t *data,uint32_t bytes,
    sony_aalp_core_fields *out,uint32_t positions[10]);
#ifdef __cplusplus
}
#endif
#endif
