#include "fixed_plus_predictor.hpp"
#include "dequant/independent_plus_dequant.h"
#include "imdct/independent_plus_transform.h"
#include "ipqf/independent_plus_ipqf.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace independent_aal {
namespace {
unsigned profile_subband_limit(unsigned frame_bytes){
    switch(frame_bytes){
    case 280:case 376:case 560:case 744:return 8;
    case 936:case 1120:case 1488:return 12;
    case 1864:case 2048:return 16;
    default:throw std::invalid_argument("Unsupported AALP core frame size");
    }
}
}
struct FixedPlusPredictor::Impl {
    AalPlusTransformState transform[2][16];
    AalPlusIpqfState synthesis[2];
};
FixedPlusPredictor::FixedPlusPredictor(unsigned core_frame_bytes):max_subbands_(profile_subband_limit(core_frame_bytes)),state_(new Impl){reset();}
FixedPlusPredictor::~FixedPlusPredictor()=default;
FixedPlusPredictor::FixedPlusPredictor(FixedPlusPredictor&&) noexcept=default;
FixedPlusPredictor& FixedPlusPredictor::operator=(FixedPlusPredictor&&) noexcept=default;
void FixedPlusPredictor::reset(){
    if(!state_)state_.reset(new Impl);
    std::memset(state_.get(),0,sizeof(Impl));
    for(auto& channel:state_->transform)for(auto& band:channel)aal_plus_transform_reset(&band);
}
std::array<int16_t,4096> FixedPlusPredictor::decode(const at3pcore_spectrum& spectrum){
    if(!state_||spectrum.num_quant_units>32||spectrum.num_subbands>16)
        throw std::invalid_argument("Invalid deterministic ATRAC3plus spectrum");
    for(unsigned band=0;band<16;++band)if(spectrum.swap_channels[band]||spectrum.negate_coefficients[band])
        throw std::invalid_argument("This predictor requires the encoder's unswapped spectrum profile");
    std::array<int16_t,4096> result;
    // Sony's deterministic AL decoder caps inverse synthesis by codec profile,
    // independently of how many subbands the ordinary core packet declares.
    const unsigned synthesis_bands=std::min(max_subbands_,unsigned(spectrum.num_subbands));
    for(unsigned ch=0;ch<2;++ch){
        int32_t wl[32],sf[32];
        for(unsigned qu=0;qu<32;++qu){wl[qu]=spectrum.word_length[ch][qu];sf[qu]=spectrum.scale_factor[ch][qu];
            if(wl[qu]>7||sf[qu]>63)throw std::invalid_argument("Invalid plus quantization metadata");}
        int16_t frequency[16][128],frequency_scale[16];
        if(aal_plus_fixed_dequant(wl,sf,spectrum.quantized[ch],spectrum.num_subbands,frequency,frequency_scale))
            throw std::runtime_error("Plus integer dequantization failed");
        int16_t time[16][128],time_scale[16];
        for(unsigned band=0;band<16;++band){
            if(band<synthesis_bands){
                if(spectrum.window_shape[ch][band]>1)throw std::invalid_argument("Invalid plus window shape");
                if(aal_plus_fixed_imdct(frequency[band],frequency_scale[band],time[band],&time_scale[band],
                    &state_->transform[ch][band],0,spectrum.window_shape[ch][band],band&1))
                    throw std::runtime_error("Plus integer inverse transform failed");
            }else{
                std::fill(time[band],time[band]+128,0);time_scale[band]=24;
                aal_plus_transform_reset(&state_->transform[ch][band]);
            }
        }
        const int16_t* pointers[16];for(unsigned band=0;band<16;++band)pointers[band]=time[band];
        int16_t output[2048];
        if(aal_plus_fixed_ipqf(pointers,time_scale,output,&state_->synthesis[ch],synthesis_bands))
            throw std::runtime_error("Plus integer inverse polyphase synthesis failed");
        for(unsigned i=0;i<2048;++i)result[i*2+ch]=output[i];
    }
    return result;
}
}
