#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include "plus_core_adapter.h"

namespace independent_aal {
// Stateful deterministic reconstruction of the emitted gain/tonal/noise-free
// stereo ATRAC3plus core spectrum. Each call returns one complete 2048-sample
// core frame, interleaved L,R, including the stream's initial core priming frame.
// AALP enhanced packet ordinal1 corresponds to core decode call1 (call0 primes
// state). Source sample n maps to the concatenated result at 2048+368+n.
class FixedPlusPredictor {
    struct Impl;
    unsigned max_subbands_;
    std::unique_ptr<Impl> state_;
public:
    explicit FixedPlusPredictor(unsigned core_frame_bytes=2048);
    ~FixedPlusPredictor();
    FixedPlusPredictor(FixedPlusPredictor&&) noexcept;
    FixedPlusPredictor& operator=(FixedPlusPredictor&&) noexcept;
    FixedPlusPredictor(const FixedPlusPredictor&)=delete;
    FixedPlusPredictor& operator=(const FixedPlusPredictor&)=delete;
    void reset();
    std::array<int16_t,4096> decode(const at3pcore_spectrum& spectrum);
};
}
