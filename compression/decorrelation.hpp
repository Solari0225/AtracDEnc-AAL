#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace independent_aal { namespace compression {
inline int32_t wrap_sample(uint32_t value,unsigned width) {
    if(!width||width>32)throw std::invalid_argument("Invalid AAL sample width");
    if(width<32){const uint32_t mask=(uint32_t(1)<<width)-1;value&=mask;if(value&(uint32_t(1)<<(width-1)))value|=~mask;}
    return value<=INT32_MAX?int32_t(value):-1-int32_t(UINT32_MAX-value);
}
// Mode 0: L,R. Mode 1: L,L-R. Mode 2: R,L-R. All arithmetic
// is explicitly wrapped to the declared PCM width, as in the native codec.
inline void decorrelate(unsigned mode,const int32_t* left,const int32_t* right,
                        int32_t* first,int32_t* second,size_t count,unsigned width=16) {
    if(mode>2||!left||!right||!first||!second)throw std::invalid_argument("Invalid AAL decorrelation input");
    for(size_t i=0;i<count;++i){const int32_t l=left[i],r=right[i];first[i]=mode==2?r:l;second[i]=mode?wrap_sample(uint32_t(l)-uint32_t(r),width):r;}
}
inline void correlate(unsigned mode,const int32_t* first,const int32_t* second,
                      int32_t* left,int32_t* right,size_t count,unsigned width=16) {
    if(mode>2||!left||!right||!first||!second)throw std::invalid_argument("Invalid AAL decorrelation input");
    for(size_t i=0;i<count;++i){const int32_t a=first[i],b=second[i];left[i]=mode==2?wrap_sample(uint32_t(a)+uint32_t(b),width):a;right[i]=mode==1?wrap_sample(uint32_t(a)-uint32_t(b),width):(mode==2?a:b);}
}
unsigned choose_decorrelation(const int32_t* left,const int32_t* right,size_t count);
}}
