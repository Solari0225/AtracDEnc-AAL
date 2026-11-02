#pragma once
#include "core_reader.hpp"
#include "integer_gain.h"
#include "../../independent_fixed_transform.h"
#include "../../independent_fixed_qmf.h"
#include <algorithm>
namespace independent_aal_decoder {
class OriginalFixedPredictor {
    struct State {
        std::array<int16_t,1162> synthesis{};
        std::array<std::array<int32_t,128>,2> overlap{};
        std::array<Gain,4> previous_gain{};
    };
    std::array<State,2> channels{};
public:
    // Test diagnostic: full per-channel/per-band input to recovered integer
    // gain after independent inverse quantization and inverse transform.
    std::array<std::array<int32_t,256>,4> latest_transform{};
    std::array<int16_t,2048> decode(const uint8_t core[384]){
        std::array<int16_t,2048> output{};
        for(unsigned ch=0;ch<2;++ch){
            const Channel meta=read_channel(core+192*ch);State&s=channels[ch];
            std::memmove(s.synthesis.data(),s.synthesis.data()+1024,138*sizeof(int16_t));
            std::fill(s.synthesis.begin()+138,s.synthesis.end(),int16_t(0));
            for(unsigned band=0;band<2;++band){
                std::array<int32_t,256> samples{};
                std::copy_n(meta.frequency.data()+256*band,256,samples.data());
                aal_fixed_imdct(s.overlap[band].data(),band,meta.gains[band].points[1],samples.data());
                latest_transform[2*ch+band]=samples;
                std::array<int16_t,256> placed{};
                if(sony_aal_integer_gain(samples.data(),s.previous_gain[band].points.data(),1,placed.data()))throw std::runtime_error("Invalid original gain application");
                for(unsigned i=0;i<256;++i)s.synthesis[138+4*i+band]=placed[i];
            }
            s.previous_gain=meta.gains;
            std::array<int16_t,1024> pcm{};aal_fixed_iqmf(s.synthesis.data(),pcm.data(),1);
            for(unsigned i=0;i<1024;++i)output[2*i+ch]=pcm[i];
        }
        return output;
    }
};
}
