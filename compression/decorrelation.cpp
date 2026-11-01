#include "decorrelation.hpp"
#include <algorithm>

namespace independent_aal { namespace compression {
unsigned choose_decorrelation(const int32_t* left,const int32_t* right,size_t count) {
    if(!left||!right||!count||count>2048||count%4)throw std::invalid_argument("Invalid AAL decorrelation block");
    // The codec's vector path accumulates even/odd double-precision lanes.
    // Difference energy is measured before narrowing to the PCM word width.
    double l[2]={0,0},r[2]={0,0},d[2]={0,0};
    for(size_t i=0;i<count;++i) {
        const double a=left[i],b=right[i],difference=wrap_sample(uint32_t(left[i])-uint32_t(right[i]),32);
        const unsigned lane=unsigned(i&1);
        l[lane]+=a*a;r[lane]+=b*b;d[lane]+=difference*difference;
    }
    const double le=l[0]+l[1],re=r[0]+r[1],de=d[0]+d[1];
    if(le<0.1||re<0.1||de/std::max(le,re)>0.9)return 0;
    return le<re?1:2; // Exact ties retain the right channel.
}
}}
