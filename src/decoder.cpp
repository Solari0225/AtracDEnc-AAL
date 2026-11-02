#include "decoder_api.hpp"
#include "stream_codec.hpp"
#include "stream_io.hpp"
#include "platform_io.hpp"
#include "sha256.hpp"
#include "../compression/enhancement.hpp"
#include "../core/decode/aal3/fixed_predictor.hpp"
#include "../core/decode/aalp/core_reader.h"
#include <memory>

namespace independent_aal {
namespace {
void notify(const Progress& progress,const std::atomic_bool* cancelled,int percent,const wchar_t* stage) {
    if(cancelled&&cancelled->load())throw std::runtime_error("Cancelled");
    if(progress)progress(percent,stage);
}
class StreamInput {
    const ByteRead& input;
    const std::atomic_bool* cancelled;
public:
    uint64_t remaining;
    StreamInput(const ByteRead& read,uint64_t bytes,const std::atomic_bool* cancel):input(read),cancelled(cancel),remaining(bytes) {}
    void exact(uint8_t* data,size_t size) {
        if(size>remaining)throw std::runtime_error("Truncated AAL stream.");
        while(size) {
            if(cancelled&&cancelled->load())throw std::runtime_error("Cancelled");
            const size_t n=input(data,size);
            if(!n||n>size)throw std::runtime_error("Invalid or incomplete AAL read callback.");
            data+=n;size-=n;remaining-=n;
        }
    }
    void skip(uint64_t size) {
        if(size>remaining)throw std::runtime_error("Truncated AAL metadata.");
        std::array<uint8_t,4096> scratch{};
        while(size){const auto n=size_t(std::min<uint64_t>(size,scratch.size()));exact(scratch.data(),n);size-=n;}
    }
};
}

struct PacketDecoder::Impl {
    const EncodingProfile* profile=nullptr;
    uint64_t frames=0,written=0;
    uint32_t count=0,next=0;
    uint16_t tail=0;
    Sha256 hash;
    independent_aal_decoder::OriginalFixedPredictor atrac3;
    std::unique_ptr<sony_aalp_predictor_state> plus;
    explicit Impl(const std::array<uint8_t,96>& header) {
        const auto* h=header.data();
        if(std::memcmp(h,"EA3",3)||h[5]!=96)throw std::runtime_error("Invalid OMA header.");
        const uint32_t parameters=(uint32_t(h[49])<<16)|be16(h+50);
        profile=&profile_by_parameters(h[32],parameters);
        if(h[48]!=(profile->codec==CoreCodec::Atrac3plus?1:0)||
           (be16(h+33)&0xfff0)!=(uint16_t(profile->oma_frame_parameters)<<8))
            throw std::runtime_error("OMA frame parameters do not match the core.");
        if(be16(h+6)!=0xff80&&be16(h+6)!=0xffff)throw std::runtime_error("Encrypted OMA is unsupported.");
        count=be32(h+40);tail=be16(h+34);
        const size_t samples=profile->frame_samples;
        if(count<2||!tail||tail>samples)throw std::runtime_error("Invalid AAL packet/sample count.");
        const uint64_t declared=uint64_t(count-2)*samples+tail;
        const size_t carry=tail+profile->pcm_trim>samples?samples:0;
        if(declared<=carry)throw std::runtime_error("Empty or invalid AAL PCM length.");
        frames=declared-carry;
        if(profile_block_count(frames,*profile)!=count)throw std::runtime_error("Inconsistent AAL sample counts.");
        if(profile->codec==CoreCodec::Atrac3plus) {
            plus=std::make_unique<sony_aalp_predictor_state>();
            sony_original_aalp_predictor_reset(plus.get());
        }
    }
    void packet(const uint8_t* data,size_t bytes,const PcmWrite& output) {
        const size_t samples=profile->frame_samples;
        if(!data||next>=count||bytes<24||std::memcmp(data,"BLK",3)||be16(data+4)!=bytes-24||
           be32(data+8)!=next||data[3]>7||bytes<24+profile->core_bytes)
            throw std::runtime_error("Invalid AAL packet framing or order.");
        const uint16_t flags=be16(data+12),packet_tail=flags&0xfff;
        const bool last=(flags&0x8000)!=0;
        const size_t enhancement_size=bytes-24-profile->core_bytes;
        if(last!=(next+1==count)||packet_tail!=(last?tail:samples)||
           (!next&&(enhancement_size||data[3]))||
           (next&&(!enhancement_size||enhancement_size>samples*4+8||enhancement_size%8||data[3]>=enhancement_size)))
            throw std::runtime_error("Invalid AAL enhancement framing.");
        const auto* core=data+24;
        std::array<int16_t,4096> predicted{};
        if(profile->codec==CoreCodec::Atrac3) {
            const auto pcm=atrac3.decode(core);std::copy(pcm.begin(),pcm.end(),predicted.begin());
        }else {
            sony_aalp_core_fields fields{};uint32_t positions[10];
            const int stage=sony_original_aalp_read_core(core,profile->core_bytes,&fields,positions);
            if(stage||fields.channels!=2)throw std::runtime_error("ATRAC3plus core parse failed at block "+std::to_string(next)+".");
            int16_t planar[2][2048];
            const uint32_t cap=profile->core_bytes<=744?8:profile->core_bytes<=1488?12:16;
            if(sony_original_aalp_predict(&fields,cap,plus.get(),planar))
                throw std::runtime_error("Invalid ATRAC3plus integer prediction.");
            for(size_t j=0;j<samples;++j)for(size_t ch=0;ch<2;++ch)predicted[j*2+ch]=planar[ch][j];
        }
        if(next) {
            compression::EnhancementPacket enhancement;
            enhancement.bytes.assign(core+profile->core_bytes,data+bytes);enhancement.padding_bytes=data[3];
            std::array<int16_t,4096> restored{};size_t produced=0;
            const auto restore=[&](const auto& residual) {
                for(size_t j=0;j<samples;++j) {
                    const int64_t position=(int64_t(next)-1)*int64_t(samples)+int64_t(j)-profile->pcm_trim;
                    if(position<0||uint64_t(position)>=frames)continue;
                    if(uint64_t(position)!=written+produced)throw std::runtime_error("Discontinuous AAL PCM.");
                    for(size_t ch=0;ch<2;++ch) {
                        const uint16_t word=uint16_t(uint16_t(predicted[j*2+ch])+uint16_t(residual[ch][j]));
                        std::memcpy(&restored[produced*2+ch],&word,2);
                    }
                    ++produced;
                }
            };
            if(profile->codec==CoreCodec::Atrac3plus)restore(compression::decode_plus_enhancement(enhancement));
            else restore(compression::decode_enhancement(enhancement));
            if(produced) {
                std::array<uint8_t,8192> little{};pcm_to_bytes(restored.data(),produced,little.data());
                hash.update(little.data(),produced*4);output(restored.data(),produced);written+=produced;
            }
        }
        ++next;
    }
};
PacketDecoder::PacketDecoder(const std::array<uint8_t,96>& header):impl(std::make_unique<Impl>(header)) {}
PacketDecoder::~PacketDecoder()=default;
uint64_t PacketDecoder::sample_frames() const{return impl->frames;}
uint32_t PacketDecoder::packet_count() const{return impl->count;}
const char* PacketDecoder::profile_id() const{return impl->profile->id;}
void PacketDecoder::packet(const uint8_t* bytes,size_t size,const PcmWrite& output){impl->packet(bytes,size,output);}
StreamSummary PacketDecoder::finish() const {
    if(impl->next!=impl->count||impl->written!=impl->frames)throw std::runtime_error("Incomplete AAL PCM reconstruction.");
    StreamSummary value;value.sample_frames=impl->frames;value.bytes=impl->frames*4;
    value.profile_id=impl->profile->id;value.pcm_sha256=impl->hash.hex_digest();return value;
}
StreamSummary decode_stream(const ByteRead& input,uint64_t bytes,const PcmWrite& output,
                            const Progress& progress,const std::atomic_bool* cancelled) {
    if(!input||!output||bytes<96)throw std::runtime_error("Invalid AAL streaming callbacks or input size.");
    notify(progress,cancelled,0,L"Read AAL container");
    StreamInput source(input,bytes,cancelled);std::array<uint8_t,96> header{};source.exact(header.data(),10);
    if(!std::memcmp(header.data(),"ea3",3)||!std::memcmp(header.data(),"ID3",3)) {
        uint64_t metadata=0;
        for(size_t i=6;i<10;++i){if(header[i]&128)throw std::runtime_error("Invalid metadata length.");metadata=(metadata<<7)|header[i];}
        metadata+=(header[5]&0x10)?10:0;source.skip(metadata);source.exact(header.data(),96);
    }else source.exact(header.data()+10,86);
    PacketDecoder decoder(header);
    const auto& profile=profile_by_id(decoder.profile_id());
    if(uint64_t(decoder.packet_count())*(24+profile.core_bytes)>source.remaining)
        throw std::runtime_error("AAL packet count exceeds available input.");
    std::array<uint8_t,24+2048+8192+8> packet{};
    for(uint32_t ordinal=0;ordinal<decoder.packet_count();++ordinal) {
        notify(progress,cancelled,1+int(uint64_t(ordinal)*94/decoder.packet_count()),L"Decode AAL core and lossless enhancement");
        source.exact(packet.data(),24);const size_t size=be16(packet.data()+4);
        if(size<profile.core_bytes||size>profile.core_bytes+size_t(profile.frame_samples)*4+8)
            throw std::runtime_error("Oversized or invalid AAL packet.");
        source.exact(packet.data()+24,size);decoder.packet(packet.data(),24+size,output);
    }
    if(source.remaining)throw std::runtime_error("Unexpected AAL trailing data.");
    auto result=decoder.finish();notify(progress,cancelled,97,L"Hash decoded PCM");return result;
}
DecodeBuffer decode_aal(const uint8_t* data,size_t bytes,const Progress& progress,const std::atomic_bool* cancelled) {
    if(!data||bytes<96)throw std::runtime_error("Invalid AAL input.");
    size_t offset=0;DecodeBuffer result;
    const auto summary=decode_stream([&](uint8_t* p,size_t n) {
        n=std::min(n,bytes-offset);if(n)std::memcpy(p,data+offset,n);offset+=n;return n;
    },bytes,[&](const int16_t* pcm,size_t n) {
        if(n>(result.pcm.max_size()-result.pcm.size())/2)throw std::runtime_error("Decoded PCM exceeds addressable buffer size.");
        result.pcm.insert(result.pcm.end(),pcm,pcm+n*2);
    },progress,cancelled);
    result.profile_id=summary.profile_id;result.pcm_sha256=summary.pcm_sha256;return result;
}
DecodeSummary decode_file(const std::filesystem::path& input,const std::filesystem::path& output,
                          const Progress& progress,const std::atomic_bool* cancelled) {
    const auto destination=std::filesystem::absolute(output).lexically_normal();
    if(std::filesystem::exists(destination))throw std::runtime_error("Output already exists; choose another name.");
    if(!std::filesystem::is_directory(destination.parent_path()))throw std::runtime_error("Output directory does not exist.");
    FileInput source(input);auto temporary=destination;temporary+=temporary_token();
    NewFile target(temporary);uint64_t output_bytes=0;
    // The PCM length is known from the small AAL header. A fixed-size prefix
    // reader records it without buffering the complete file or any metadata.
    std::array<uint8_t,10> first{};source.exact(first.data(),10);uint64_t offset=0;
    if(!std::memcmp(first.data(),"ea3",3)||!std::memcmp(first.data(),"ID3",3)) {
        for(size_t i=6;i<10;++i){if(first[i]&128)throw std::runtime_error("Invalid metadata length.");offset=(offset<<7)|first[i];}
        offset+=10+((first[5]&0x10)?10:0);
    }
    source.seek(offset);std::array<uint8_t,96> header{};source.exact(header.data(),96);
    Bytes wav;
    {PacketDecoder description(header);wav=wave_header(description.sample_frames());}
    target.write_bytes(wav.data(),wav.size());output_bytes=wav.size();source.seek(0);
    const auto decoded=decode_stream([&](uint8_t* p,size_t n){return source.read(p,n);},source.size(),
        [&](const int16_t* pcm,size_t n) {
            std::array<uint8_t,8192> block{};pcm_to_bytes(pcm,n,block.data());
            target.write_bytes(block.data(),n*4);output_bytes+=n*4;
        },progress,cancelled);
    notify(progress,cancelled,99,L"Save decoded PCM WAV");target.finish();
    notify({},cancelled,99,L"Save decoded PCM WAV");publish_new(temporary,destination);
    DecodeSummary result;result.output_path=destination;result.sample_frames=decoded.sample_frames;
    result.output_bytes=output_bytes;result.pcm_sha256=decoded.pcm_sha256;result.profile_id=decoded.profile_id;return result;
}
}
