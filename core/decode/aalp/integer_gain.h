#ifndef SONY_RECOVERED_AALP_GAIN_H
#define SONY_RECOVERED_AALP_GAIN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact64B original fixed decoder band gain record. Locations are4-sample
 * groups0..31, in strict order; levels0..15. Window is separate from curve. */
typedef struct sony_aalp_gain {uint32_t count,locations[7],levels[7],window;} sony_aalp_gain;
int sony_aalp_gain_curve(const sony_aalp_gain *gain,int16_t coefficients[128],int16_t *scale);
int sony_aalp_apply_gain(const sony_aalp_gain *gain,int16_t samples[128],int16_t *scale);
#ifdef __cplusplus
}
#endif
#endif
