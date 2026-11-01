#pragma once
// Independent AAL enhancement serializer. Analysis and entropy stages are
// separately verified numerical implementations, without Sony runtime calls.
#include "analysis.hpp"
#include "decorrelation.hpp"
#include "rice.hpp"

namespace independent_aal { namespace compression {
using StereoFrame = std::array<std::array<int32_t,1024>,2>;
using PlusStereoFrame = std::array<std::array<int32_t,2048>,2>;

inline EnhancementPacket encode_analyzed_enhancement(
    const int32_t* original_left, const int32_t* original_right,
    const std::array<ChannelAnalysis,2>& channels,
    unsigned decorrelation = 0, unsigned width = 16, bool allow_raw = true) {
    if (width != 16 || decorrelation > 2)
        throw std::invalid_argument("AAL requires 16-bit stereo and a valid decorrelation mode");
    const size_t samples=channels[0].residual.size();
    if(samples!=1024&&samples!=2048)throw std::invalid_argument("Invalid AAL residual block size");
    const size_t half=samples/2;
    BitWriter writer;
    writer.put(0,1);
    writer.put(2,2);
    writer.put(0,2);
    writer.put(decorrelation,2);
    for (const auto& channel : channels) {
        if (channel.mode > 8 || channel.residual.size() != samples)
            throw std::invalid_argument("Invalid AAL temporal prediction plan");
        writer.put(channel.mode,4);
        if (channel.mode == 8) for (unsigned i=0;i<8;++i) {
            const unsigned bits = i < 2 ? 11 : 6;
            if (channel.parameters[i] < 0 || uint32_t(channel.parameters[i]) >= (1u << bits))
                throw std::invalid_argument("Invalid AAL LPC coefficient code");
            writer.put(uint32_t(channel.parameters[i]),bits);
        }
        writer.put(1,2); // Original AAL3 formatter uses exactly two halves.
        write_rice_block(writer,channel.residual.data(),half,width);
        write_rice_block(writer,channel.residual.data()+half,half,width);
    }
    auto result = finish_enhancement(writer);
    if (allow_raw && double(result.meaningful_bytes) > double(samples * 2 * width / 8) * 0.95)
        return encode_raw_enhancement(original_left,original_right,width,samples);
    return result;
}

// Analysis uses a fresh zero eight-sample working history for each channel and
// packet, matching the recovered AAL3 analysis routine. Stream core prediction
// state and the 138-sample PCM priming alignment belong to the caller.
inline EnhancementPacket encode_enhancement(const int32_t* left, const int32_t* right,
                                            unsigned width = 16) {
    const unsigned decorrelation = choose_decorrelation(left,right,1024);
    StereoFrame transformed;
    decorrelate(decorrelation,left,right,transformed[0].data(),transformed[1].data(),1024,width);
    const std::array<ChannelAnalysis,2> channels = {
        analyze_channel(transformed[0].data(),1024,width),
        analyze_channel(transformed[1].data(),1024,width)};
    return encode_analyzed_enhancement(left,right,channels,decorrelation,width);
}
inline EnhancementPacket encode_enhancement(const StereoFrame& input, unsigned width = 16) {
    return encode_enhancement(input[0].data(),input[1].data(),width);
}
inline EnhancementPacket encode_plus_enhancement(const PlusStereoFrame& input,const double* exact_window=nullptr,
                                                 unsigned width = 16) {
    const unsigned decorrelation=choose_decorrelation(input[0].data(),input[1].data(),2048);
    PlusStereoFrame transformed;
    decorrelate(decorrelation,input[0].data(),input[1].data(),transformed[0].data(),transformed[1].data(),2048,width);
    const std::array<ChannelAnalysis,2> channels={analyze_channel(transformed[0].data(),2048,width,exact_window),
        analyze_channel(transformed[1].data(),2048,width,exact_window)};
    return encode_analyzed_enhancement(input[0].data(),input[1].data(),channels,decorrelation,width);
}

template<size_t Samples>
inline std::array<std::array<int32_t,Samples>,2> decode_enhancement_samples(const EnhancementPacket& packet, unsigned width = 16) {
    static_assert(Samples==1024||Samples==2048,"Invalid AAL residual block size");
    if (width != 16 || packet.padding_bytes > packet.bytes.size())
        throw std::invalid_argument("Invalid AAL3 enhancement packet");
    BitReader reader(packet.bytes.data(),packet.bytes.size()-packet.padding_bytes);
    if (reader.get(1) || reader.get(2) != 2)
        throw std::runtime_error("Unsupported AAL enhancement header");
    const unsigned raw = reader.get(2);
    if (raw == 1) return decode_no_lpc_samples<Samples>(packet,width);
    if (raw != 0) throw std::runtime_error("Unsupported AAL enhancement coding mode");
    const unsigned decorrelation = reader.get(2);
    if (decorrelation > 2) throw std::runtime_error("Invalid AAL decorrelation mode");
    std::array<std::array<int32_t,Samples>,2> result{};
    for (auto& channel : result) {
        const unsigned mode = reader.get(4);
        if (mode > 8) throw std::runtime_error("Invalid AAL temporal prediction mode");
        std::array<int32_t,8> parameters{};
        if (mode == 8) for (unsigned i=0;i<8;++i) parameters[i]=int32_t(reader.get(i<2?11:6));
        const unsigned partitions = reader.get(2)+1;
        if (partitions != 2) throw std::runtime_error("Unsupported AAL3 partition layout");
        std::array<int32_t,Samples> residual;
        read_rice_block(reader,residual.data(),Samples/2,width);
        read_rice_block(reader,residual.data()+Samples/2,Samples/2,width);
        const auto decoded = inverse_channel(residual.data(),Samples,width,mode,parameters);
        std::copy(decoded.begin(),decoded.end(),channel.begin());
    }
    if (reader.get(2)) throw std::runtime_error("Nonzero AAL end marker");
    reader.align_byte();
    if (reader.bit_count()/8 != packet.bytes.size()-packet.padding_bytes)
        throw std::runtime_error("Extra meaningful bytes after AAL end marker");
    correlate(decorrelation,result[0].data(),result[1].data(),result[0].data(),result[1].data(),Samples,width);
    return result;
}
inline StereoFrame decode_enhancement(const EnhancementPacket& packet,unsigned width=16) {
    return decode_enhancement_samples<1024>(packet,width);
}
inline PlusStereoFrame decode_plus_enhancement(const EnhancementPacket& packet,unsigned width=16) {
    return decode_enhancement_samples<2048>(packet,width);
}
}} // namespace independent_aal::compression
