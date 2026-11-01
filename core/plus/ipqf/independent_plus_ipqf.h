#pragma once
#include <cstdint>

// Persistent inverse-polyphase history. Slots24..31 are reserved and preserved.
// This compact independent state deliberately omits the reference ABI's gap.
struct AalPlusIpqfState {
    int16_t history[2][32][8];
    uint32_t position;
};

// Each active band supplies128 signed16 time samples and a signed scale.
// Produces2048 signed16 PCM samples. Returns0, or-1 for invalid arguments.
extern "C" int aal_plus_fixed_ipqf(const int16_t* const bands[16],
    const int16_t scales[16], int16_t pcm[2048], AalPlusIpqfState* state,
    unsigned band_count);
