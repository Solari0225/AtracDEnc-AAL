#pragma once
#include "encoder_api.hpp"

namespace independent_aal {
struct DecodeBuffer {
    std::vector<int16_t> pcm;
    std::string pcm_sha256;
    std::string profile_id;
};
struct DecodeSummary {
    std::filesystem::path output_path;
    uint64_t sample_frames = 0;
    uint64_t output_bytes = 0;
    std::string pcm_sha256;
    std::string profile_id;
};
// Unencrypted, 44.1 kHz / signed16 / stereo AAL3 or AALP. Input and decoded
// PCM use caller-sized whole buffers; there is no fixed 1 GiB cap.
// File and streaming entry points use bounded working buffers instead.
DecodeBuffer decode_aal(const uint8_t* data, size_t bytes,
                        const Progress& progress = Progress(),
                        const std::atomic_bool* cancelled = nullptr);
DecodeSummary decode_file(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          const Progress& progress = Progress(),
                          const std::atomic_bool* cancelled = nullptr);
}
