#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace independent_aal {
struct EncodeOptions {
    // Explicitly selects both codec family and nominal core rate. No fallback.
    std::string profile_id="aalp-352";
};
struct EncodeSummary {
    uint64_t sample_frames=0;
    uint64_t output_bytes=0;
    std::filesystem::path output_path;
    std::filesystem::path report_path;
    std::string pcm_sha256;
    bool verified=false;
    std::string profile_id;
};
using Progress=std::function<void(int,const std::wstring&)>;
struct EncodeBuffer {std::vector<uint8_t> bytes;std::string pcm_sha256;};
EncodeBuffer encode_pcm16(const int16_t* interleaved_stereo,size_t sample_frames,
                         const Progress& progress=Progress(),
                         const std::atomic_bool* cancelled=nullptr,
                         const EncodeOptions& options=EncodeOptions());
// CD PCM WAV -> an explicitly selected unencrypted AAL3/AALP profile.
// Refuses existing output/report files and performs an independent internal
// PCM round-trip before publishing. No Sony library or process is invoked.
EncodeSummary encode_file(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          const Progress& progress=Progress(),
                          const std::atomic_bool* cancelled=nullptr,
                          const EncodeOptions& options=EncodeOptions());
// Synthetic in-memory encode and independent decode; throws on failure.
std::string self_test(const Progress& progress=Progress(),
                      const std::atomic_bool* cancelled=nullptr,
                      const EncodeOptions& options=EncodeOptions());
}
