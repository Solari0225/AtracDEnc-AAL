// Standalone adapter around public LGPL AtracDEnc PQF/MDCT/quantizer/packer.
// It intentionally omits GHA tone extraction. Real audio is encoded through
// all spectral bands; no Sony library, FFmpeg library or process is called.
#include "plus_core_adapter.h"
#include "atrac/at3p/at3p_bitstream.h"
#include "atrac/at3p/at3p_mdct.h"
#include "atrac/at3p/at3p_tables.h"
#include "atrac/atrac_scale.h"
#include "atrac/atrac3plus_pqf/atrac3plus_pqf.h"
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
thread_local std::string creation_error;
struct CoreOutput : ICompressedOutput {
    std::vector<char> frame;
    std::string GetName() const override { return "Independent ATRAC3plus core"; }
    size_t GetChannelNum() const override { return 2; }
    void WriteFrame(std::vector<char> data) override { frame=std::move(data); }
};
struct PqfState {
    at3plus_pqf_a_ctx_t context=at3plus_pqf_create_a_ctx();
    ~PqfState(){if(context)at3plus_pqf_free_a_ctx(context);}
    PqfState() {if(!context)throw std::bad_alloc();}
    PqfState(const PqfState&)=delete;
    PqfState& operator=(const PqfState&)=delete;
};
struct CoreEncoder {
    const size_t frame_bytes;
    at3pcore_spectrum spectrum{};
    CoreOutput output;
    NAtracDEnc::TAt3PBitStream packer;
    NAtracDEnc::TAt3pMDCT mdct;
    NAtracDEnc::TScaler<NAtracDEnc::NAt3p::TScaleTable> scaler;
    std::array<PqfState,2> pqf;
    std::array<NAtracDEnc::TAt3pMDCT::THistBuf,2> history{};
    std::string error;
    explicit CoreEncoder(size_t bytes):frame_bytes(bytes),packer(&output,uint16_t(bytes),&spectrum){}
    void encode(const int16_t* pcm) {
        std::vector<NAtracDEnc::TAt3PBitStream::TSingleChannelElement> channels(2);
        for(unsigned ch=0;ch<2;++ch) {
            float input[2048],subbands[2048];
            std::vector<float> spectra(2048);
            for(size_t i=0;i<2048;++i)input[i]=pcm?pcm[2*i+ch]/32768.0f:0.0f;
            at3plus_pqf_do_analyse(pqf[ch].context,input,subbands);
            // Preserve upstream's spectral amplitude normalization. The PQF
            // implementation already applies its corresponding DCT scaling.
            for(float& value:subbands)value*=1.122018f/32768.0f;
            NAtracDEnc::TAt3pMDCT::TPcmBandsData bands;
            for(unsigned band=0;band<16;++band)bands[band]=subbands+128*band;
            mdct.Do(spectra.data(),bands,history[ch],channels[ch].SubbandInfo.Win);
            channels[ch].ScaledBlocks=scaler.ScaleFrame(spectra,NAtracDEnc::NAt3p::TScaleTable::TBlockSizeMod());
        }
        output.frame.clear();
        // Null tonal block produces no tone, gain, noise, channel swap or
        // coefficient-negation side information, while encoding real spectra.
        packer.WriteFrame(2,nullptr,channels);
        if(output.frame.size()!=frame_bytes)throw std::runtime_error("Unexpected ATRAC3plus frame size");
    }
};
}

extern "C" size_t at3pcore_frame_bytes(uint32_t bitrate) {
    switch(bitrate) {
    case 48000:return 280;
    case 64000:return 376;
    case 96000:return 560;
    case 128000:return 744;
    case 160000:return 936;
    case 192000:return 1120;
    case 256000:return 1488;
    case 320000:return 1864;
    case 352000:return 2048;
    default:return 0;
    }
}
extern "C" void* at3pcore_create(uint32_t frame_bytes) {
    try {
        bool valid=false;
        for(uint32_t rate:{48000u,64000u,96000u,128000u,160000u,192000u,256000u,320000u,352000u})
            if(at3pcore_frame_bytes(rate)==frame_bytes)valid=true;
        if(!valid)throw std::invalid_argument("Unsupported AALP core frame size");
        // Public PQF/window initialization uses process-wide numerical tables.
        // Serialize construction; subsequent per-context encoding is independent.
        static std::mutex initialization;
        std::lock_guard<std::mutex> lock(initialization);
        creation_error.clear();
        return new CoreEncoder(frame_bytes);
    }catch(const std::exception& error){creation_error=error.what();return nullptr;}
}
extern "C" int at3pcore_encode(void* context,const int16_t* pcm,uint8_t* output,size_t capacity) {
    if(!context||!output)return -1;
    auto& encoder=*static_cast<CoreEncoder*>(context);
    try {
        if(capacity<encoder.frame_bytes)throw std::invalid_argument("ATRAC3plus output buffer is too small");
        encoder.encode(pcm);
        std::memcpy(output,encoder.output.frame.data(),encoder.frame_bytes);
        encoder.error.clear();
        return int(encoder.frame_bytes);
    }catch(const std::exception& error){encoder.error=error.what();return -1;}
}
extern "C" const char* at3pcore_last_error(const void* context) {
    return context?static_cast<const CoreEncoder*>(context)->error.c_str():creation_error.c_str();
}
extern "C" int at3pcore_last_spectrum(const void* context,at3pcore_spectrum* output) {
    if(!context||!output)return 0;
    const auto& encoder=*static_cast<const CoreEncoder*>(context);
    if(encoder.output.frame.empty())return 0;
    *output=encoder.spectrum;
    return 1;
}
extern "C" void at3pcore_destroy(void* context){delete static_cast<CoreEncoder*>(context);}
