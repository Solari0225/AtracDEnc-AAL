// Independent scalar integer inverse polyphase synthesis, reconstructed from
// mathematical dataflow. No reference instructions or external codec calls.
#include "independent_plus_ipqf.h"
#include <algorithm>
#include <cstring>

namespace {
#include "independent_plus_ipqf_tables.inc"
int32_t from_bits(uint32_t v) { int32_t r;std::memcpy(&r,&v,4);return r; }
int16_t short_from_bits(uint16_t v) { int16_t r;std::memcpy(&r,&v,2);return r; }
int32_t sar(int32_t value,unsigned n) {
    if(!n)return value;
    if(n>=32)return value<0?-1:0;
    const uint32_t bits=uint32_t(value);
    return from_bits(value<0?~((~bits)>>n):bits>>n);
}
int16_t clamp16(int32_t x) { return int16_t(std::max(-32768,std::min(32767,x))); }
int16_t normalize(int16_t x,int16_t delta) {
    if(delta>=16)return 0;
    if(delta>=0)return int16_t(sar(x,unsigned(delta)));
    const unsigned shift=unsigned(std::min(16,-int(delta)));
    return clamp16(from_bits(uint32_t(int32_t(x))<<shift));
}
}

extern "C" int aal_plus_fixed_ipqf(const int16_t* const bands[16],
    const int16_t scales[16],int16_t pcm[2048],AalPlusIpqfState* state,
    unsigned band_count) {
    if(!pcm||!state||state->position>=24||band_count>16||
       (band_count&&(!bands||!scales)))return -1;
    for(unsigned b=0;b<band_count;++b)if(!bands[b])return -1;
    int min_scale=100;
    for(unsigned b=0;b<band_count;++b)min_scale=std::min(min_scale,int(scales[b]));
    min_scale=std::max(min_scale,-12);
    const unsigned dot_shift=unsigned(min_scale+12);
    int16_t normalized[128][16]{};
    for(unsigned b=0;b<band_count;++b) {
        const int16_t delta=short_from_bits(uint16_t(uint16_t(scales[b])-uint16_t(min_scale)));
        for(unsigned t=0;t<128;++t)normalized[t][b]=normalize(bands[b][t],delta);
    }
    for(unsigned time=0;time<128;++time) {
        const unsigned pos=state->position;
        for(unsigned plane=0;plane<2;++plane)for(unsigned row=0;row<8;++row) {
            uint32_t dot=0;
            if(dot_shift<31)for(unsigned b=0;b<band_count;++b)
                dot+=uint32_t(int32_t(normalized[time][b])*int32_t(kDct[plane][row][b]));
            state->history[plane][pos][row]=dot_shift>=31?0:clamp16(sar(from_bits(dot),dot_shift));
        }
        uint32_t accum[16]{};
        for(unsigned tap=0;tap<12;++tap) {
            const unsigned a=(pos+2*tap)%24,b=(pos+2*tap+1)%24;
            for(unsigned sample=0;sample<16;++sample) {
                const unsigned index=sample<8?sample:15-sample;
                accum[sample]+=uint32_t(int32_t(state->history[0][a][index])*int32_t(kFirA[tap][sample]));
                accum[sample]+=uint32_t(int32_t(state->history[1][b][index])*int32_t(kFirB[tap][sample]));
            }
        }
        for(unsigned sample=0;sample<16;++sample)
            pcm[time*16+sample]=clamp16(sar(from_bits(accum[sample]),13));
        state->position=pos?pos-1:23;
    }
    return 0;
}
