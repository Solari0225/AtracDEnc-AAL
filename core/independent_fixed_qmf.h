#ifndef AAL_INDEPENDENT_FIXED_QMF_H
#define AAL_INDEPENDENT_FIXED_QMF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AAL_FIXED_QMF_BAND_SAMPLES = 256,
    AAL_FIXED_QMF_OUTPUT_SAMPLES = 1024,
    AAL_FIXED_QMF_HISTORY_SAMPLES = 138,
    AAL_FIXED_QMF_STATE_SAMPLES = 1162
};

/*
 * Independently written integer synthesis mathematics.
 *
 * state[0..137] is the synthesis history. Current subband samples are
 * state[138 + 4*i + band], i=0..255, band=0..3. The function modifies this
 * working state, and writes 1024 signed PCM samples to a separate buffer.
 * low_bands_only != 0 selects the native AAL3 two-band prediction path.
 *
 * This low-level operation does not advance frame history. History loading
 * and advancement belong to the surrounding codec framing implementation.
 * State and output buffers must not overlap.
 *
 * No Sony code, runtime loader, process invocation or external component is
 * used by this implementation. Research verification lives separately.
 */
void aal_fixed_iqmf(int16_t state[AAL_FIXED_QMF_STATE_SAMPLES],
                    int16_t output[AAL_FIXED_QMF_OUTPUT_SAMPLES],
                    int low_bands_only);

#ifdef __cplusplus
}
#endif
#endif
