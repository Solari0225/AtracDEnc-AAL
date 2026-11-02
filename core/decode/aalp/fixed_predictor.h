#ifndef SONY_ORIGINAL_AALP_PREDICTOR_H
#define SONY_ORIGINAL_AALP_PREDICTOR_H
#include <stdint.h>
#include "integer_gain.h"
#include "../../plus/imdct/independent_plus_transform.h"
#include "../../plus/ipqf/independent_plus_ipqf.h"
typedef struct sony_aalp_core_fields {
    uint32_t channels,units,frame_subbands,coefficient_subbands,mute;
    int32_t word_length[2][32],scale_factor[2][32],codebook[2][32];
    int16_t quantized[2][2048];uint32_t negative[16],swap[16],window[2][16];
    sony_aalp_gain gain[2][16];
} sony_aalp_core_fields;
typedef struct sony_aalp_predictor_state {
    AalPlusTransformState transform[2][16];AalPlusIpqfState synthesis[2];
    sony_aalp_gain previous_gain[2][16];uint32_t older_window[2][16];
} sony_aalp_predictor_state;
#ifdef __cplusplus
extern "C" {
#endif
void sony_original_aalp_predictor_reset(sony_aalp_predictor_state *state);
/* Original fixed AALP prediction, not a perceptual floating music decoder.
 * decoded core fields must come from the actual physical core parser.
 * cap is8/12/16 from profile. Every call returns2048 planar signed16 samples.
 * Tone/noise syntax does not enter this original integer mathematical graph;
 * parser still consumes and validates it. No resampler/component dependency. */
int sony_original_aalp_predict(const sony_aalp_core_fields *core,uint32_t cap,
    sony_aalp_predictor_state *state,int16_t output[2][2048]);
#ifdef __cplusplus
}
#endif
#endif
