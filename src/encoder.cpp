#include "encoder_api.hpp"
#include "aal_format.hpp"
#include "predictor.hpp"
#include "platform_io.hpp"
#include "sha256.hpp"
#include "../compression/enhancement.hpp"
#include "../core/plus/plus_core_adapter.h"
#include "../core/plus/parser/plus_core_parser.h"
#include "../core/plus/fixed_plus_predictor.hpp"
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <cmath>

extern "C" void* at3core_create();
extern "C" int at3core_encode(void*,const int16_t*,unsigned char*,size_t);
extern "C" void at3core_destroy(void*);

namespace independent_aal {
namespace {
void check(const std::atomic_bool* cancelled) {if(cancelled && cancelled->load())throw std::runtime_error("Cancelled");}
void progress(const Progress& callback,int percent,const wchar_t* stage) {if(callback)callback(percent,stage);}
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
Bytes read_file(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)throw std::runtime_error("Cannot open input file.");
    const auto length=input.tellg();
    if(length<0 || uint64_t(length)>1024ULL*1024*1024)throw std::runtime_error("Input exceeds this build's 1 GiB limit.");
    Bytes bytes(static_cast<size_t>(length));input.seekg(0);
    if(!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("Incomplete input read.");
    return bytes;
}
struct Encoded { Bytes bytes; std::string pcm_hash;uint32_t raw_blocks=0,compressed_blocks=0; };

template<size_t Samples>
compression::EnhancementPacket pack_residual(const std::vector<int16_t>& residual,uint32_t ordinal,unsigned trim) {
    std::array<std::array<int32_t,Samples>,2> channels{};
    for(size_t ch=0;ch<2;++ch)for(size_t j=0;j<Samples;++j) {
        const int64_t n=(int64_t(ordinal)-1)*int64_t(Samples)+int64_t(j)-int64_t(trim);
        if(n>=0&&uint64_t(n)<residual.size()/2)channels[ch][j]=residual[size_t(n)*2+ch];
    }
    if constexpr(Samples==1024)return compression::encode_enhancement(channels);
    else return compression::encode_plus_enhancement(channels);
}

Encoded encode_pcm(const Wave& wave,const EncodingProfile& profile,const Progress& callback,const std::atomic_bool* cancelled) {
    check(cancelled);
    const bool plus=profile.codec==CoreCodec::Atrac3plus;
    const size_t frame_words=size_t(profile.frame_samples)*2;
    const uint32_t count=profile_block_count(wave.frames(),profile);
    const wchar_t* core_stage=plus?L"编码 ATRAC3plus 音乐核心":L"编码 ATRAC3 音乐核心";
    progress(callback,2,core_stage);
    const auto destroy=plus?at3pcore_destroy:at3core_destroy;
    const auto encode=plus?at3pcore_encode:at3core_encode;
    std::unique_ptr<void,void(*)(void*)> encoder(plus?at3pcore_create(profile.core_bytes):at3core_create(),destroy);
    if(!encoder)throw std::runtime_error(plus?at3pcore_last_error(nullptr):"ATRAC3 initialization failed.");
    std::vector<Bytes> cores;cores.reserve(count);
    for(uint32_t feed=0;cores.size()<count;++feed) {
        check(cancelled);
        if(feed>count+8)throw std::runtime_error("The core encoder did not drain its declared output frames.");
        std::vector<int16_t> source(frame_words);Bytes core(profile.core_bytes);
        const uint64_t start=uint64_t(feed)*frame_words;
        if(start<wave.pcm.size())std::copy_n(wave.pcm.data()+start,std::min<uint64_t>(frame_words,wave.pcm.size()-start),source.data());
        const int size=encode(encoder.get(),source.data(),core.data(),core.size());
        if(size<0)throw std::runtime_error(plus?at3pcore_last_error(encoder.get()):"ATRAC3 encoding failed.");
        if(size==profile.core_bytes) {
            if(plus) {
                at3pcore_spectrum encoded{},read_back{};
                if(!at3pcore_last_spectrum(encoder.get(),&encoded)||!at3pcore_parse_spectrum(core.data(),core.size(),&read_back))
                    throw std::runtime_error(at3pcore_parse_last_error());
                if(std::memcmp(&encoded,&read_back,sizeof encoded))throw std::runtime_error("Encoded ATRAC3plus spectrum differs from physical bitstream read-back.");
            }
            cores.push_back(std::move(core));
        } else if(size!=0)throw std::runtime_error("Unexpected compatibility core frame size.");
        if(feed%16==0)progress(callback,std::min(40,2+int(uint64_t(feed)*38/count)),core_stage);
    }
    encoder.reset();progress(callback,42,L"计算精确整数预测与无损残差");
    const auto append_decoded=[&](std::vector<int16_t>& out,const Bytes& core,FixedPredictor& at3,FixedPlusPredictor* at3p){
        if(plus) {
            at3pcore_spectrum spectrum{};
            if(!at3pcore_parse_spectrum(core.data(),core.size(),&spectrum))throw std::runtime_error(at3pcore_parse_last_error());
            const auto pcm=at3p->decode(spectrum);out.insert(out.end(),pcm.begin(),pcm.end());
        } else {const auto pcm=at3.decode(core.data());out.insert(out.end(),pcm.begin(),pcm.end());}
    };
    FixedPredictor at3_prediction;
    std::unique_ptr<FixedPlusPredictor> plus_prediction=plus?std::make_unique<FixedPlusPredictor>(profile.core_bytes):nullptr;
    std::vector<int16_t> predicted;predicted.reserve(size_t(count)*frame_words);
    for(uint32_t i=0;i<count;++i) {
        check(cancelled);append_decoded(predicted,cores[i],at3_prediction,plus_prediction.get());
        if(i%32==0)progress(callback,42+int(uint64_t(i)*13/count),L"计算精确整数预测与无损残差");
    }
    const size_t priming=(size_t(profile.frame_samples)+profile.pcm_trim)*2;
    if(predicted.size()<wave.pcm.size()+priming)throw std::runtime_error("Insufficient prediction tail.");
    std::vector<int16_t> residual(predicted.size()-priming);
    for(size_t i=0;i<residual.size();++i) {
        const uint16_t source=i<wave.pcm.size()?uint16_t(wave.pcm[i]):0;
        const uint16_t word=uint16_t(source-uint16_t(predicted[i+priming]));std::memcpy(&residual[i],&word,2);
    }
    predicted.clear();predicted.shrink_to_fit();plus_prediction.reset();
    Encoded result;result.bytes=aal_header(wave.frames(),count,profile);
    result.bytes.reserve(96+size_t(count)*(24+profile.core_bytes+size_t(profile.frame_samples)*4+8));
    for(uint32_t i=0;i<count;++i) {
        check(cancelled);compression::EnhancementPacket enhancement;
        if(i) {
            enhancement=plus?pack_residual<2048>(residual,i,profile.pcm_trim):pack_residual<1024>(residual,i,profile.pcm_trim);
            if(enhancement.raw)++result.raw_blocks;else ++result.compressed_blocks;
        }
        const auto packet=aal_block(cores[i].data(),i,enhancement.bytes,i+1==count,profile_tail(wave.frames(),profile),enhancement.padding_bytes,profile);
        result.bytes.insert(result.bytes.end(),packet.begin(),packet.end());
        if(i%16==0)progress(callback,56+int(uint64_t(i)*22/count),L"按原版方法压缩：声道去相关、预测与 Rice 编码");
    }
    residual.clear();residual.shrink_to_fit();progress(callback,80,L"读取实际输出位流并逐样本自校验");
    const auto parsed=parse_aal(result.bytes);FixedPredictor at3_verifier;
    std::unique_ptr<FixedPlusPredictor> plus_verifier=plus?std::make_unique<FixedPlusPredictor>(profile.core_bytes):nullptr;
    if(std::string_view(parsed.profile->id)!=profile.id)throw std::runtime_error("Written AAL profile differs from request.");
    predicted.reserve(size_t(count)*frame_words);
    for(uint32_t i=0;i<count;++i){check(cancelled);append_decoded(predicted,parsed.packets[i].core,at3_verifier,plus_verifier.get());}
    uint64_t checked=0;
    for(uint32_t i=1;i<count;++i) {
        compression::EnhancementPacket payload;payload.bytes=parsed.packets[i].enhancement;payload.padding_bytes=parsed.packets[i].padding;
        const auto verify=[&](const auto& errors){
            for(unsigned ch=0;ch<2;++ch)for(unsigned j=0;j<profile.frame_samples;++j) {
                const int64_t n=(int64_t(i)-1)*profile.frame_samples+j-profile.pcm_trim;
                if(n<0||uint64_t(n)>=wave.frames())continue;
                const size_t index=size_t(n)*2+ch;
                const uint16_t restored=uint16_t(uint16_t(errors[ch][j])+uint16_t(predicted[index+priming]));
                if(restored!=uint16_t(wave.pcm[index]))throw std::runtime_error("Internal PCM round-trip mismatch.");++checked;
            }
        };
        if(plus)verify(compression::decode_plus_enhancement(payload));else verify(compression::decode_enhancement(payload));
        check(cancelled);if(i%32==0)progress(callback,80+int(uint64_t(i)*13/count),L"读取实际输出位流并逐样本自校验");
    }
    if(checked!=wave.pcm.size())throw std::runtime_error("PCM verification sample count mismatch.");
    result.pcm_hash=sha256(wave.pcm.data(),wave.pcm.size()*2);return result;
}

}

EncodeSummary encode_file(const std::filesystem::path& input,const std::filesystem::path& output,
                          const Progress& callback,const std::atomic_bool* cancelled,const EncodeOptions& options) {
    check(cancelled);
    const auto& profile=profile_by_id(options.profile_id);
    const auto source=std::filesystem::absolute(input).lexically_normal();
    const auto destination=std::filesystem::absolute(output).lexically_normal();
    std::filesystem::path report=destination;report+=".json";
    if(std::filesystem::exists(destination)||std::filesystem::exists(report))throw std::runtime_error("Output or report already exists; choose another name.");
    if(!std::filesystem::is_directory(destination.parent_path()))throw std::runtime_error("Output directory does not exist.");
    progress(callback,0,L"读取 CD PCM WAV");
    auto input_bytes=read_file(source);const auto wave=read_wave(input_bytes);
    input_bytes.clear();input_bytes.shrink_to_fit();
    auto encoded=encode_pcm(wave,profile,callback,cancelled);check(cancelled);
    const std::string file_hash=sha256(encoded.bytes.data(),encoded.bytes.size());
    std::ostringstream json;
    json<<"{\n  \"encoder\": \"AtracDEnc-AAL 0.2.0\",\n"
        <<"  \"input\": "<<json_string(source.u8string())<<",\n"
        <<"  \"output\": "<<json_string(destination.u8string())<<",\n"
        <<"  \"format\": \""<<profile.container_name<<" / OMA, unencrypted\",\n"
        <<"  \"profile\": \""<<profile.id<<"\",\n"
        <<"  \"compatibility_core\": \""<<(profile.codec==CoreCodec::Atrac3plus?"ATRAC3plus":"ATRAC3")<<"\",\n"
        <<"  \"sample_rate\": 44100,\n  \"bits_per_sample\": 16,\n  \"channels\": 2,\n"
        <<"  \"sample_frames\": "<<wave.frames()<<",\n"
        <<"  \"nominal_core_bitrate\": "<<profile.nominal_bitrate<<",\n  \"core_bitrate\": "<<std::setprecision(12)<<profile.actual_core_bitrate()<<",\n"
        <<"  \"core_bytes_per_frame\": "<<profile.core_bytes<<",\n  \"samples_per_frame\": "<<profile.frame_samples<<",\n  \"pcm_trim\": "<<profile.pcm_trim<<",\n"
        <<"  \"enhancement_mode\": \"Original AAL channel decorrelation, fixed/LPC8 prediction, partitioned Rice; 95% raw fallback\",\n"
        <<"  \"compressed_blocks\": "<<encoded.compressed_blocks<<",\n  \"raw_blocks\": "<<encoded.raw_blocks<<",\n"
        <<"  \"ratio_vs_pcm\": "<<std::setprecision(10)<<double(encoded.bytes.size())/double(wave.pcm.size()*2)<<",\n"
        <<"  \"output_bytes\": "<<encoded.bytes.size()<<",\n"
        <<"  \"pcm_sha256\": \""<<encoded.pcm_hash<<"\",\n"
        <<"  \"oma_sha256\": \""<<file_hash<<"\",\n"
        <<"  \"internal_pcm_round_trip\": true,\n"
        <<"  \"sony_decoder_checked_for_this_file\": false,\n"
        <<"  \"physical_walkman_tested\": false,\n"
        <<"  \"runtime\": \"Native C++17; no Sony, SMLE, TraConv, FFmpeg or .NET dependencies\"\n}\n";
    const std::string report_bytes=json.str();
    const auto token=temporary_token();
    std::filesystem::path temporary=destination;temporary+=token;
    std::filesystem::path temporary_report=report;temporary_report+=token;
    bool report_published=false,output_published=false;
    try {
        progress(callback,95,L"保存已校验的 AAL 与报告");
        write_new(temporary,encoded.bytes.data(),encoded.bytes.size());
        write_new(temporary_report,report_bytes.data(),report_bytes.size());check(cancelled);
        publish_new(temporary_report,report);
        report_published=true;
        publish_new(temporary,destination);
        output_published=true;
    }catch(...) {
        remove_owned_file(temporary);remove_owned_file(temporary_report);
        if(report_published&&!output_published)remove_owned_file(report);
        throw;
    }
    progress(callback,100,L"完成：PCM 逐样本自校验通过");
    EncodeSummary result;result.sample_frames=wave.frames();result.output_bytes=encoded.bytes.size();
    result.output_path=destination;result.report_path=report;result.pcm_sha256=encoded.pcm_hash;result.verified=true;result.profile_id=profile.id;
    return result;
}

EncodeBuffer encode_pcm16(const int16_t* pcm,size_t frames,const Progress& callback,const std::atomic_bool* cancelled,const EncodeOptions& options) {
    if(!pcm||!frames||frames>256ULL*1024*1024)throw std::runtime_error("Invalid or oversized stereo PCM buffer.");
    const auto& profile=profile_by_id(options.profile_id);
    Wave wave;wave.pcm.assign(pcm,pcm+frames*2);auto encoded=encode_pcm(wave,profile,callback,cancelled);
    return {std::move(encoded.bytes),std::move(encoded.pcm_hash)};
}

std::string self_test(const Progress& callback,const std::atomic_bool* cancelled,const EncodeOptions& options) {
    Wave wave;wave.pcm.resize(8443*2);uint32_t state=0x62b9a137;
    for(size_t i=0;i<wave.pcm.size()/2;++i) {
        state=state*1664525u+1013904223u;
        if(i<1024){wave.pcm[2*i]=int16_t(state>>16);wave.pcm[2*i+1]=int16_t(~(state>>16));}
        else if(i<2048){wave.pcm[2*i]=(i&1)?32767:-32768;wave.pcm[2*i+1]=(i&2)?-32768:32767;}
        else {wave.pcm[2*i]=int16_t(std::sin(double(i)*0.173)*27500);wave.pcm[2*i+1]=int16_t(std::cos(double(i)*0.411)*15000);}
    }
    const auto result=encode_pcm(wave,profile_by_id(options.profile_id),callback,cancelled);
    progress(callback,100,L"自检通过：8443 个立体声采样帧无损往返");
    return result.pcm_hash;
}
}
