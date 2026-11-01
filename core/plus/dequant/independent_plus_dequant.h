#pragma once
#include <cstdint>

// One channel of decoded core metadata, independent of any Sony ABI or parser.
// quantized bins use32 quantization units spanning16 subbands of128 bins each.
extern "C" int aal_plus_fixed_dequant(const int32_t word_length[32],
    const int32_t scale_factor[32],const int16_t quantized[2048],
    unsigned subband_count,int16_t bands[16][128],int16_t band_scales[16]);
