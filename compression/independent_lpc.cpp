// Independent reconstruction of the bounded order-8 Q10 prediction function.
// Evidence: runtime/dsp-verified-0x4be6a20.asm, region offset 0x38ca20.
#include "independent_lpc.h"
#include <cstring>

namespace {
int32_t signed_word(uint32_t value) {
    int32_t result;
    std::memcpy(&result,&value,sizeof result);
    return result;
}
int32_t signed_half(int32_t value) {
    const uint16_t low = uint16_t(value);
    return low < 32768u ? int32_t(low) : int32_t(low)-65536;
}
int32_t wrap_width(uint32_t value,unsigned width) {
    if(width==32)return signed_word(value);
    const uint32_t mask=(uint32_t(1)<<width)-1u;
    value&=mask;
    if(value&(uint32_t(1)<<(width-1)))value|=~mask;
    return signed_word(value);
}
int32_t arithmetic_shift(uint32_t value,unsigned shift) {
    return signed_word((value>>shift)|((value&0x80000000u)?(~uint32_t(0)<<(32-shift)):0));
}
bool valid(const int32_t* input,const int32_t* output,const int32_t* coef,
           const int32_t* history,unsigned order,unsigned count,unsigned width) {
    return input&&output&&coef&&history&&order<=8&&count<=2048&&width>=1&&width<=32;
}
}

extern "C" int aal_lpc_forward(const int32_t* input,int32_t* residual,
                               const int32_t coefficients[8],const int32_t history[8],
                               unsigned order,unsigned count,unsigned width) {
    if(!valid(input,residual,coefficients,history,order,count,width))return -1;
    for(unsigned i=0;i<count;++i) {
        uint64_t sum=512;
        for(unsigned j=0;j<order;++j) {
            const int32_t previous=i>j?input[i-j-1]:history[7+i-j];
            const int64_t product=int64_t(signed_half(coefficients[j]))*signed_half(previous);
            sum+=uint64_t(product);
        }
        const uint32_t prediction=uint32_t(sum>>10);
        residual[i]=wrap_width(uint32_t(input[i])-prediction,width);
    }
    return 0;
}

extern "C" int aal_lpc_inverse(const int32_t* residual,int32_t* output,
                               const int32_t coefficients[8],const int32_t history[8],
                               unsigned order,unsigned count,unsigned width) {
    if(!valid(residual,output,coefficients,history,order,count,width))return -1;
    for(unsigned i=0;i<count;++i) {
        uint64_t sum=512;
        for(unsigned j=0;j<order;++j) {
            const int32_t previous=i>j?output[i-j-1]:history[7+i-j];
            const int64_t product=int64_t(coefficients[j])*previous;
            sum+=uint64_t(product);
        }
        const uint32_t prediction=uint32_t(sum>>10);
        output[i]=wrap_width(uint32_t(residual[i])+prediction,width);
    }
    return 0;
}

extern "C" int aal_fixed_prediction(const int32_t* input,int32_t* output,
                                    const int32_t history[8],unsigned mode,
                                    unsigned count,unsigned width,int inverse) {
    if(!input||!output||!history||mode<1||mode>7||count>2048||width<1||width>32)return -1;
    static constexpr int coefficients[8]={0,32,28,16,0,-16,-28,-32};
    for(unsigned i=0;i<count;++i) {
        int32_t recent=i? (inverse?output[i-1]:input[i-1]):history[7];
        int32_t older=i>1? (inverse?output[i-2]:input[i-2]):history[6+i];
        if(!inverse){recent=signed_half(recent);older=signed_half(older);}
        uint32_t sum=8u;
        sum+=uint32_t(uint64_t(int64_t(recent)*coefficients[mode]));
        sum+=uint32_t(uint64_t(int64_t(older)*-16));
        const uint32_t prediction=uint32_t(arithmetic_shift(sum,4));
        const uint32_t result=inverse?uint32_t(input[i])+prediction:uint32_t(input[i])-prediction;
        output[i]=wrap_width(result,width);
    }
    return 0;
}

extern "C" int aal_lpc_reflection_from_codes(const int32_t* codes,
                                            int32_t* reflection,unsigned order) {
    if(!codes||!reflection||order<2||order>8)return -1;
    // Exact bounded table: codec region +0x3d0460, 32 signed half-words.
    static constexpr int16_t sine[32]={
        0,50,100,150,199,248,297,344,391,437,482,526,568,609,649,687,
        724,758,791,822,851,878,903,925,946,964,979,993,1004,1012,1019,1022
    };
    for(unsigned i=0;i<2;++i) {
        if(codes[i]<0||codes[i]>=2048)return -1;
        reflection[i]=codes[i]-(codes[i]<1024?1023:1024);
    }
    for(unsigned i=2;i<order;++i) {
        if(codes[i]<0||codes[i]>=64)return -1;
        reflection[i]=codes[i]<32?-sine[31-codes[i]]:sine[codes[i]-32];
    }
    return 0;
}

extern "C" int aal_lpc_direct_from_reflection(const int32_t* reflection,
                                               int32_t* coefficients,unsigned order) {
    if(!reflection||!coefficients||order>8)return -1;
    for(unsigned i=0;i<order;++i) {
        coefficients[i]=reflection[i];
        for(unsigned j=0;j<i;++j) {
            const uint32_t product=uint32_t(uint64_t(int64_t(reflection[i])*coefficients[i-j-1]));
            const uint32_t rounded=product+512u;
            const uint32_t prediction=uint32_t(arithmetic_shift(rounded,10));
            const int32_t result=signed_word(uint32_t(coefficients[j])-prediction);
            if(result>32768||result< -32768)return 1;
            coefficients[j]=result;
        }
    }
    return 0;
}
