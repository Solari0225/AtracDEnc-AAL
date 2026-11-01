#pragma once
#include "core_spectrum.hpp"
#include "independent_fixed_transform.h"
#include "independent_fixed_qmf.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace independent_aal {
namespace predictor_tables {
#include "dequant_tables.inc"
}
inline int32_t signed_shift(int32_t v,unsigned shift) {
    shift&=31;if(!shift)return v;
    const uint32_t bits=uint32_t(v);
    const uint32_t shifted=(bits>>shift)|((bits&0x80000000u)?(~uint32_t(0)<<(32-shift)):0);
    int32_t result;std::memcpy(&result,&shifted,4);return result;
}
inline int32_t dequantize(int m,unsigned sf,unsigned word) {
    if(word==0)return 0;
    if(word>7 || sf>63)throw std::runtime_error("Invalid inverse-quantization parameters.");
    const unsigned magnitude=unsigned(m<0?-m:m);
    if(magnitude>=predictor_tables::kMagnitudeCount[word-1])throw std::runtime_error("ATRAC3 mantissa outside the supported codebook.");
    int32_t coefficient=predictor_tables::kMagnitude[sf%3][word-1][magnitude];
    if(m<0)coefficient=-coefficient;
    return signed_shift(coefficient,22-sf/3+8);
}
inline int16_t clamp_pcm(int32_t v) {return int16_t(std::max<int32_t>(-32768,std::min<int32_t>(32767,v)));}

class FixedPredictor {
    struct Channel {
        std::array<int16_t,1162> synthesis{};
        std::array<std::array<int32_t,128>,2> overlap{};
    };
    std::array<Channel,2> channels{};
public:
    std::array<int16_t,2048> decode(const uint8_t* core) {
        std::array<int16_t,2048> output{};
        for(unsigned ch=0;ch<2;++ch) {
            const auto spectrum=read_core_channel(core+ch*192);
            auto& state=channels[ch];
            // Same effective synthesis history as the observed channel loader.
            // The native loader also copies an additional 138 words that are
            // immediately replaced by current-frame subband samples.
            std::memmove(state.synthesis.data(),state.synthesis.data()+1024,138*sizeof(int16_t));
            std::fill(state.synthesis.begin()+138,state.synthesis.end(),int16_t(0));
            for(unsigned band=0;band<2;++band) {
                std::array<int32_t,256> samples{};
                for(unsigned i=0;i<256;++i) {
                    const unsigned index=band*256+i;
                    samples[i]=dequantize(spectrum.mantissa[index],spectrum.scale_factor[index],spectrum.word_length[index]);
                }
                // Our encoder always writes no gain points. The default
                // terminator gain level is 4, used as this transform exponent.
                aal_fixed_imdct(state.overlap[band].data(),band,4,samples.data());
                for(unsigned i=0;i<256;++i)state.synthesis[138+4*i+band]=clamp_pcm(samples[i]);
            }
            std::array<int16_t,1024> pcm{};
            aal_fixed_iqmf(state.synthesis.data(),pcm.data(),1);
            for(unsigned i=0;i<1024;++i)output[2*i+ch]=pcm[i];
        }
        return output;
    }
};
}
