#ifndef AAL_INDEPENDENT_PLUS_TRANSFORM_H
#define AAL_INDEPENDENT_PLUS_TRANSFORM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AalPlusTransformState {
    int16_t overlap[64];
    int16_t scale;
} AalPlusTransformState;
// Reset one subband state. Sixteen independent subband states per channel.
void aal_plus_transform_reset(AalPlusTransformState* state);
// Mathematical 128-bin integer inverse transform, window and overlap. The
// active encoder uses gain_scale=0; window_shape and reverse are Boolean.
int aal_plus_fixed_imdct(const int16_t spectrum[128],int16_t spectrum_scale,
                         int16_t samples[128],int16_t* sample_scale,
                         AalPlusTransformState* state,int gain_scale,
                         unsigned window_shape,unsigned reverse);
#ifdef __cplusplus
}
#endif
#endif
