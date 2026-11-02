#include "encoder_api.hpp"
#include "stream_codec.hpp"
#include "stream_io.hpp"
#include "predictor.hpp"
#include "platform_io.hpp"
#include "sha256.hpp"
#include "../compression/enhancement.hpp"
#include "../core/plus/plus_core_adapter.h"
#include "../core/plus/parser/plus_core_parser.h"
#include "../core/plus/fixed_plus_predictor.hpp"
#include <deque>
#include <iomanip>
#include <memory>
#include <sstream>
#include <cmath>

extern "C" void* at3core_create();
extern "C" int at3core_encode(void*,const int16_t*,unsigned char*,size_t);
extern "C" void at3core_destroy(void*);

namespace independent_aal {
namespace {
void check(const std::atomic_bool* cancelled){if(cancelled&&cancelled->load())throw std::runtime_error("Cancelled");}
void progress(const Progress& callback,int percent,const wchar_t* stage){if(callback)callback(percent,stage);}
std::string json_string(const std::string& value) {
    std::string out="\"";
    for(unsigned char c:value) {
        if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
        else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";
        else if(c<32){const char* d="0123456789abcdef";out+="\\u00";out+=d[c>>4];out+=d[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
}
StreamSummary encode_stream(const PcmRead& input,uint64_t frames,const ByteWrite& output,
                            const Progress& callback,const std::atomic_bool* cancelled,const EncodeOptions& options) {
    if(!input||!output||!frames)throw std::runtime_error("Invalid PCM streaming callbacks or sample length.");
    const auto& profile=profile_by_id(options.profile_id);
    const uint32_t count=profile_block_count(frames,profile);
    const size_t samples=profile.frame_samples,frame_words=samples*2;
    const bool plus=profile.codec==CoreCodec::Atrac3plus;
    check(cancelled);progress(callback,2,plus?L"编码 ATRAC3plus 音乐核心":L"编码 ATRAC3 音乐核心");check(cancelled);
    const auto destroy=plus?at3pcore_destroy:at3core_destroy;
    const auto encode=plus?at3pcore_encode:at3core_encode;
    std::unique_ptr<void,void(*)(void*)> encoder(plus?at3pcore_create(profile.core_bytes):at3core_create(),destroy);
    if(!encoder)throw std::runtime_error(plus?at3pcore_last_error(nullptr):"ATRAC3 initialization failed.");
    FixedPredictor at3_prediction;
    std::unique_ptr<FixedPlusPredictor> plus_prediction=plus?std::make_unique<FixedPlusPredictor>(profile.core_bytes):nullptr;
    const auto header=aal_header(frames,count,profile);std::array<uint8_t,96> header_array{};
    std::copy(header.begin(),header.end(),header_array.begin());PacketDecoder verifier(header_array);
    StreamSummary result;result.sample_frames=frames;result.profile_id=profile.id;result.bytes=header.size();
    output(header.data(),header.size());
    std::deque<std::vector<int16_t>> pending;
    uint64_t first_block=0,read_frames=0,verified_frames=0;uint32_t ordinal=0;Sha256 input_hash;
    const auto source_sample=[&](uint64_t position,size_t channel)->int16_t {
        if(position>=frames)return 0;
        const uint64_t block=position/samples;
        if(block<first_block||block-first_block>=pending.size())throw std::runtime_error("PCM/core streaming alignment failed.");
        return pending[size_t(block-first_block)][size_t(position%samples)*2+channel];
    };
    for(uint64_t feed=0;ordinal<count;++feed) {
        check(cancelled);
        if(feed>uint64_t(count)+8)throw std::runtime_error("The core encoder did not drain its declared output frames.");
        std::vector<int16_t> source(frame_words);
        const size_t available=size_t(std::min<uint64_t>(samples,frames-read_frames));
        if(available) {
            size_t obtained=0;
            while(obtained<available) {
                check(cancelled);const size_t n=input(source.data()+obtained*2,available-obtained);
                if(!n||n>available-obtained)throw std::runtime_error("Invalid or incomplete PCM read callback.");
                obtained+=n;
            }
            std::array<uint8_t,8192> little{};pcm_to_bytes(source.data(),available,little.data());
            input_hash.update(little.data(),available*4);read_frames+=available;
            if(pending.empty())first_block=feed;
            pending.push_back(source);
            if(pending.size()>32)throw std::runtime_error("Core encoder exceeded the bounded PCM lookahead queue.");
        }
        Bytes core(profile.core_bytes);
        const int bytes=encode(encoder.get(),source.data(),core.data(),core.size());
        if(bytes<0)throw std::runtime_error(plus?at3pcore_last_error(encoder.get()):"ATRAC3 encoding failed.");
        if(bytes!=0&&bytes!=profile.core_bytes)throw std::runtime_error("Unexpected compatibility core frame size.");
        if(bytes) {
            std::vector<int16_t> predicted;
            if(plus) {
                at3pcore_spectrum encoded{},parsed{};
                if(!at3pcore_last_spectrum(encoder.get(),&encoded)||!at3pcore_parse_spectrum(core.data(),core.size(),&parsed))
                    throw std::runtime_error(at3pcore_parse_last_error());
                if(std::memcmp(&encoded,&parsed,sizeof encoded))throw std::runtime_error("Written ATRAC3plus core differs from read-back.");
                const auto pcm=plus_prediction->decode(parsed);predicted.assign(pcm.begin(),pcm.end());
            }else {const auto pcm=at3_prediction.decode(core.data());predicted.assign(pcm.begin(),pcm.end());}
            compression::EnhancementPacket enhancement;
            if(ordinal) {
                const auto compress=[&](auto& errors) {
                    for(size_t j=0;j<samples;++j) {
                        const int64_t position=(int64_t(ordinal)-1)*int64_t(samples)+int64_t(j)-profile.pcm_trim;
                        if(position<0)continue;
                        for(size_t ch=0;ch<2;++ch) {
                            const uint16_t word=uint16_t(uint16_t(source_sample(uint64_t(position),ch))-uint16_t(predicted[j*2+ch]));
                            int16_t signed_word;std::memcpy(&signed_word,&word,2);errors[ch][j]=signed_word;
                        }
                    }
                };
                if(plus) {std::array<std::array<int32_t,2048>,2> errors{};compress(errors);enhancement=compression::encode_plus_enhancement(errors);}
                else {std::array<std::array<int32_t,1024>,2> errors{};compress(errors);enhancement=compression::encode_enhancement(errors);}
                if(enhancement.raw)++result.raw_blocks;else ++result.compressed_blocks;
            }
            const auto packet=aal_block(core.data(),ordinal,enhancement.bytes,ordinal+1==count,
                profile_tail(frames,profile),enhancement.padding_bytes,profile);
            // Independently parse the actual serialized packet and reconstruct
            // its PCM before releasing any source samples or publishing bytes.
            verifier.packet(packet.data(),packet.size(),[&](const int16_t* pcm,size_t n) {
                for(size_t j=0;j<n;++j)for(size_t ch=0;ch<2;++ch)
                    if(pcm[j*2+ch]!=source_sample(verified_frames+j,ch))throw std::runtime_error("Internal PCM round-trip mismatch.");
                verified_frames+=n;
            });
            output(packet.data(),packet.size());result.bytes+=packet.size();
            const uint64_t keep_from=uint64_t(ordinal)*samples>profile.pcm_trim?uint64_t(ordinal)*samples-profile.pcm_trim:0;
            while(!pending.empty()&&(first_block+1)*samples<=keep_from){pending.pop_front();++first_block;}
            ++ordinal;
            if(ordinal%16==0)progress(callback,2+int(uint64_t(ordinal)*91/count),L"按原版方法压缩：声道去相关、预测与 Rice 编码");
        }
    }
    if(read_frames!=frames||verified_frames!=frames)throw std::runtime_error("Incomplete streaming PCM input or verification.");
    const auto verified=verifier.finish();result.pcm_sha256=input_hash.hex_digest();
    if(verified.pcm_sha256!=result.pcm_sha256)throw std::runtime_error("Streaming PCM digest mismatch.");
    check(cancelled);progress(callback,94,L"读取实际输出位流并逐样本自校验");check(cancelled);return result;
}
EncodeBuffer encode_pcm16(const int16_t* pcm,size_t frames,const Progress& callback,
                          const std::atomic_bool* cancelled,const EncodeOptions& options) {
    if(!pcm||!frames||frames>std::numeric_limits<size_t>::max()/2)throw std::runtime_error("Invalid stereo PCM buffer.");
    size_t offset=0;EncodeBuffer output;
    const auto result=encode_stream([&](int16_t* p,size_t n) {
        n=std::min(n,frames-offset);if(n)std::memcpy(p,pcm+offset*2,n*4);offset+=n;return n;
    },frames,[&](const uint8_t* p,size_t n) {
        if(n>output.bytes.max_size()-output.bytes.size())throw std::runtime_error("Encoded AAL exceeds addressable buffer size.");
        output.bytes.insert(output.bytes.end(),p,p+n);
    },callback,cancelled,options);
    output.pcm_sha256=result.pcm_sha256;return output;
}
EncodeSummary encode_file(const std::filesystem::path& input,const std::filesystem::path& output,
                          const Progress& callback,const std::atomic_bool* cancelled,const EncodeOptions& options) {
    check(cancelled);const auto& profile=profile_by_id(options.profile_id);
    const auto source_path=std::filesystem::absolute(input).lexically_normal();
    const auto destination=std::filesystem::absolute(output).lexically_normal();auto report=destination;report+=".json";
    if(std::filesystem::exists(destination)||std::filesystem::exists(report))throw std::runtime_error("Output or report already exists; choose another name.");
    if(!std::filesystem::is_directory(destination.parent_path()))throw std::runtime_error("Output directory does not exist.");
    FileInput source(source_path);const auto wave=wave_info(source);
    profile_block_count(wave.frames(),profile);auto temporary=destination;temporary+=temporary_token();
    NewFile target(temporary);Sha256 file_hash;uint64_t remaining=wave.frames();
    const auto encoded=encode_stream([&](int16_t* pcm,size_t n) {
        n=size_t(std::min<uint64_t>(n,remaining));std::array<uint8_t,8192> little{};
        source.exact(little.data(),n*4);bytes_to_pcm(little.data(),n,pcm);remaining-=n;return n;
    },wave.frames(),[&](const uint8_t* bytes,size_t n) {
        target.write_bytes(bytes,n);file_hash.update(bytes,n);
    },callback,cancelled,options);
    check(cancelled);target.finish();
    std::ostringstream json;
    json<<"{\n  \"encoder\": \"AtracDEnc-AAL 0.5.0\",\n"
        <<"  \"input\": "<<json_string(source_path.u8string())<<",\n"
        <<"  \"output\": "<<json_string(destination.u8string())<<",\n"
        <<"  \"format\": \""<<profile.container_name<<" / OMA, unencrypted\",\n"
        <<"  \"profile\": \""<<profile.id<<"\",\n"
        <<"  \"compatibility_core\": \""<<(profile.codec==CoreCodec::Atrac3plus?"ATRAC3plus":"ATRAC3")<<"\",\n"
        <<"  \"sample_rate\": 44100,\n  \"bits_per_sample\": 16,\n  \"channels\": 2,\n"
        <<"  \"sample_frames\": "<<encoded.sample_frames<<",\n"
        <<"  \"nominal_core_bitrate\": "<<profile.nominal_bitrate<<",\n  \"core_bitrate\": "<<std::setprecision(12)<<profile.actual_core_bitrate()<<",\n"
        <<"  \"core_bytes_per_frame\": "<<profile.core_bytes<<",\n  \"samples_per_frame\": "<<profile.frame_samples<<",\n  \"pcm_trim\": "<<profile.pcm_trim<<",\n"
        <<"  \"enhancement_mode\": \"Original AAL channel decorrelation, fixed/LPC8 prediction, partitioned Rice; 95% raw fallback\",\n"
        <<"  \"compressed_blocks\": "<<encoded.compressed_blocks<<",\n  \"raw_blocks\": "<<encoded.raw_blocks<<",\n"
        <<"  \"ratio_vs_pcm\": "<<std::setprecision(10)<<double(encoded.bytes)/double(encoded.sample_frames*4)<<",\n"
        <<"  \"output_bytes\": "<<encoded.bytes<<",\n"
        <<"  \"pcm_sha256\": \""<<encoded.pcm_sha256<<"\",\n  \"oma_sha256\": \""<<file_hash.hex_digest()<<"\",\n"
        <<"  \"internal_pcm_round_trip\": true,\n  \"streaming\": true,\n"
        <<"  \"sony_decoder_checked_for_this_file\": false,\n  \"physical_walkman_tested\": false,\n"
        <<"  \"runtime\": \"Native C++17; bounded buffers; no Sony, FFmpeg or .NET dependencies\"\n}\n";
    const auto report_bytes=json.str();auto temporary_report=report;temporary_report+=temporary_token();
    bool report_published=false,output_published=false;
    try {
        progress(callback,95,L"保存已校验的 AAL 与报告");
        write_new(temporary_report,report_bytes.data(),report_bytes.size());check(cancelled);
        publish_new(temporary_report,report);report_published=true;
        publish_new(temporary,destination);output_published=true;
    }catch(...) {
        remove_owned_file(temporary_report);
        if(report_published&&!output_published)remove_owned_file(report);
        throw;
    }
    progress(callback,100,L"完成：PCM 逐样本自校验通过");
    EncodeSummary result;result.sample_frames=encoded.sample_frames;result.output_bytes=encoded.bytes;
    result.output_path=destination;result.report_path=report;result.pcm_sha256=encoded.pcm_sha256;result.verified=true;result.profile_id=profile.id;return result;
}
std::string self_test(const Progress& callback,const std::atomic_bool* cancelled,const EncodeOptions& options) {
    std::vector<int16_t> pcm(8443*2);uint32_t state=0x62b9a137;
    for(size_t i=0;i<pcm.size()/2;++i) {
        state=state*1664525u+1013904223u;
        if(i<1024){pcm[2*i]=int16_t(state>>16);pcm[2*i+1]=int16_t(~(state>>16));}
        else if(i<2048){pcm[2*i]=(i&1)?32767:-32768;pcm[2*i+1]=(i&2)?-32768:32767;}
        else {pcm[2*i]=int16_t(std::sin(double(i)*0.173)*27500);pcm[2*i+1]=int16_t(std::cos(double(i)*0.411)*15000);}
    }
    const auto result=encode_pcm16(pcm.data(),pcm.size()/2,callback,cancelled,options);
    progress(callback,100,L"自检通过：8443 个立体声采样帧无损往返");return result.pcm_sha256;
}
}
