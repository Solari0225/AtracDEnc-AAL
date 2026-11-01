// Independent integer coefficient reconstruction and per-band normalization.
#include "independent_plus_dequant.h"
#include <algorithm>
#include <cstring>

namespace {
#include "independent_plus_dequant_tables.inc"
int32_t from_bits(uint32_t v){int32_t r;std::memcpy(&r,&v,4);return r;}
int32_t sar(int32_t x,unsigned n){
    if(!n)return x;
    if(n>=32)return x<0?-1:0;
    const uint32_t u=uint32_t(x);return from_bits(x<0?~((~u)>>n):u>>n);
}
int16_t clamp16(int32_t x){return int16_t(std::max(-32768,std::min(32767,x)));}
int16_t scale_sample(int32_t x,int16_t shift){
    if(shift>=0)return clamp16(sar(x,unsigned(shift)));
    const unsigned n=unsigned(-int(shift));
    return clamp16(n>=32?0:from_bits(uint32_t(x)<<n));
}
unsigned leading_shift(int32_t x){
    uint32_t magnitude=x<0?uint32_t(-int64_t(x)):uint32_t(x);
    if(!magnitude)return 0;
    unsigned shift=0;while(magnitude<0x40000000u){magnitude<<=1;++shift;}
    return shift;
}
}

extern "C" int aal_plus_fixed_dequant(const int32_t word_length[32],
    const int32_t scale_factor[32],const int16_t quantized[2048],
    unsigned subband_count,int16_t bands[16][128],int16_t band_scales[16]){
    if(!bands||!band_scales||subband_count>16||
       (subband_count&&(!word_length||!scale_factor||!quantized)))return -1;
    for(unsigned unit=0;unit<kBandUnit[subband_count];++unit)
        if(word_length[unit]<0||word_length[unit]>7||scale_factor[unit]<0||scale_factor[unit]>63)return -1;
    std::memset(bands,0,2048*sizeof(int16_t));
    std::fill_n(band_scales,16,int16_t(24));
    for(unsigned band=0;band<subband_count;++band){
        int32_t products[128]{};int16_t unit_scale[32]{};int minimum=24;
        for(unsigned unit=kBandUnit[band];unit<kBandUnit[band+1];++unit){
            const unsigned wl=unsigned(word_length[unit]),sf=unsigned(scale_factor[unit]);
            const int16_t coefficient=kCoefficient[wl+unsigned(kSfMod3[sf])*8];
            uint32_t magnitude_or=0;
            for(unsigned i=0;i<kUnitSize[unit];++i){
                const int32_t p=int32_t(quantized[kUnitStart[unit]+i])*int32_t(coefficient);
                products[kUnitStart[unit]+i-band*128]=p;
                magnitude_or|=p<0?uint32_t(-int64_t(p)):uint32_t(p);
            }
            const int scale=magnitude_or?kScale[sf+kWordLengthShift[wl]]:24;
            const int normalization=magnitude_or?16-int(leading_shift(int32_t(magnitude_or))):0;
            unit_scale[unit]=int16_t(scale);
            minimum=std::min(minimum,scale-normalization);
        }
        band_scales[band]=int16_t(minimum);
        for(unsigned unit=kBandUnit[band];unit<kBandUnit[band+1];++unit){
            const int16_t shift=int16_t(int(unit_scale[unit])-minimum);
            for(unsigned i=0;i<kUnitSize[unit];++i){
                const unsigned at=kUnitStart[unit]+i-band*128;
                bands[band][at]=scale_sample(products[at],shift);
            }
        }
    }
    return 0;
}

#ifdef AAL_PLUS_DEQUANT_QA
extern "C" int16_t aal_plus_dequant_qa_shift(int32_t x,int16_t shift){return scale_sample(x,shift);}
extern "C" uint16_t aal_plus_dequant_qa_norm(int32_t x){return uint16_t(leading_shift(x));}
#endif
