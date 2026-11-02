#pragma once
#include "encoder_api.hpp"
#include <memory>
#include <array>

namespace independent_aal {
// Reads may return fewer items than requested. Zero is EOF; returning more
// than requested is an error. PCM counts interleaved stereo sample frames.
using ByteRead=std::function<size_t(uint8_t*,size_t)>;
using ByteWrite=std::function<void(const uint8_t*,size_t)>;
using PcmRead=std::function<size_t(int16_t*,size_t)>;
using PcmWrite=std::function<void(const int16_t*,size_t)>;
struct StreamSummary {
    uint64_t sample_frames=0,bytes=0,raw_blocks=0,compressed_blocks=0;
    std::string profile_id,pcm_sha256;
};
class PacketDecoder {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit PacketDecoder(const std::array<uint8_t,96>& header);
    ~PacketDecoder();
    PacketDecoder(const PacketDecoder&)=delete;
    PacketDecoder& operator=(const PacketDecoder&)=delete;
    uint64_t sample_frames() const;
    uint32_t packet_count() const;
    const char* profile_id() const;
    void packet(const uint8_t* bytes,size_t size,const PcmWrite& output);
    StreamSummary finish() const;
};
StreamSummary encode_stream(const PcmRead& input,uint64_t sample_frames,const ByteWrite& output,
                            const Progress& progress={},const std::atomic_bool* cancelled=nullptr,
                            const EncodeOptions& options={});
StreamSummary decode_stream(const ByteRead& input,uint64_t bytes,const PcmWrite& output,
                            const Progress& progress={},const std::atomic_bool* cancelled=nullptr);
}
