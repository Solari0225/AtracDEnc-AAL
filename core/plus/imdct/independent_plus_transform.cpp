// Independent scalar reconstruction of the integer 128-bin plus transform.
// No native instruction bytes, dynamic codec dependencies or floating point.
#include "independent_plus_transform.h"
#include <algorithm>
#include <cstring>
#include "plus_transform_tables.inc"
namespace {
int32_t signed32(uint32_t u){int32_t x;std::memcpy(&x,&u,4);return x;}
int16_t word16(int32_t x){const uint16_t u=uint16_t(x);return u<32768?int16_t(u):int16_t(int32_t(u)-65536);}
int32_t sar(int32_t x,unsigned n){if(n>=31)return x<0?-1:0;if(!n)return x;uint32_t u;std::memcpy(&u,&x,4);return signed32((u>>n)|(x<0?(~uint32_t(0)<<(32-n)):0));}
int32_t shift_left(int32_t x,unsigned n){return n>=32?0:signed32(uint32_t(x)<<n);}
int16_t clamp16(int32_t x){return int16_t(std::max(-32768,std::min(32767,x)));}
int16_t multiply_shift(int16_t a,int16_t b,unsigned n){return clamp16(sar(int32_t(a)*b,n));}
int leading_shift(int32_t value){uint32_t x=value<0?uint32_t(-int64_t(value)):uint32_t(value);int n=0;if(x)while(x<0x40000000u){x<<=1;++n;}return n;}
}
extern "C" void aal_plus_transform_reset(AalPlusTransformState* state){if(state){std::memset(state,0,sizeof(*state));state->scale=24;}}
extern "C" int aal_plus_fixed_imdct(const int16_t spectrum[128],int16_t spectrum_scale,
                                    int16_t samples[128],int16_t* sample_scale,
                                    AalPlusTransformState* state,int gain_scale,
                                    unsigned window_shape,unsigned reverse){
    if(!spectrum||!samples||!sample_scale||!state)return -1;
    int16_t data[128],post[128];
    for(unsigned i=0;i<128;++i)data[k_permutation[reverse?127-i:i]]=int16_t(sar(spectrum[i],1));
    for(unsigned half=1;half<64;half*=2){
        for(unsigned group=0;group<64;group+=half*2)for(unsigned j=0;j<half;++j){
            const unsigned a=2*(group+j),b=2*(group+j+half),t=half-1+j;
            const int32_t br=data[b],bi=data[b+1],ar=data[a],ai=data[a+1];
            const int32_t re=sar(br*k_fft_sin[t],15)+sar(bi*k_fft_cos[t],15);
            const int32_t im=sar(br*k_fft_cos[t],15)-sar(bi*k_fft_sin[t],15);
            data[a]=clamp16(sar(ar+re,1));data[a+1]=clamp16(sar(ai+im,1));
            data[b]=clamp16(sar(ar-re,1));data[b+1]=clamp16(sar(ai-im,1));
        }
    }
    for(unsigned i=0;i<64;++i){
        const unsigned p=2*i,t=126+p;
        const int32_t c0=sar(int32_t(data[p])*k_post_cos[t],15),c1=sar(int32_t(data[p+1])*k_post_cos[t+1],15);
        const int32_t s0=sar(int32_t(data[p])*k_post_sin[t],15),s1=sar(int32_t(data[p+1])*k_post_sin[t+1],15);
        post[p]=clamp16(c0+c1);post[127-p]=clamp16(s0-s1);
    }
    const int16_t* window=window_shape?k_window_b:k_window_a;
    int16_t current[128],previous[128];
    for(unsigned i=0;i<64;++i){
        current[i]=multiply_shift(post[64+i],window[i],14);
        current[64+i]=multiply_shift(post[127-i],window[64+i],14);
        previous[i]=multiply_shift(state->overlap[63-i],window[128+i],14);
        previous[64+i]=multiply_shift(state->overlap[i],window[192+i],14);
    }
    const int16_t old_scale=state->scale,unscaled=word16(int32_t(spectrum_scale)-7);
    const int16_t new_scale=word16(int32_t(unscaled)+gain_scale),delta=word16(int32_t(old_scale)-new_scale);
    std::copy(post,post+64,state->overlap);state->scale=unscaled;
    int16_t scale;
    if(delta>=16){std::copy(current,current+128,samples);scale=new_scale;}
    else if(delta<=-16){std::copy(previous,previous+128,samples);scale=old_scale;}
    else if(delta==0){for(unsigned i=0;i<128;++i)samples[i]=clamp16(sar(int32_t(current[i])+previous[i],1));scale=word16(int32_t(new_scale)-1);}
    else if(delta>0){for(unsigned i=0;i<128;++i)samples[i]=clamp16(sar(shift_left(current[i],unsigned(delta))+previous[i],unsigned(delta)+1));scale=word16(int32_t(new_scale)-1);}
    else{for(unsigned i=0;i<128;++i)samples[i]=clamp16(sar(int32_t(current[i])+shift_left(previous[i],unsigned(-delta)),unsigned(1-delta)));scale=word16(int32_t(old_scale)-1);}
    uint32_t mask=0;for(unsigned i=0;i<128;++i)mask|=uint32_t(samples[i]==-32768?32767:(samples[i]<0?-int32_t(samples[i]):samples[i]));
    if(!mask){std::fill(samples,samples+128,0);*sample_scale=24;return 0;}
    const int normalize=leading_shift(int32_t(mask))-16;
    if(normalize>0){for(unsigned i=0;i<128;++i)samples[i]=word16(shift_left(samples[i],unsigned(normalize)));scale=word16(int32_t(scale)+normalize);}
    *sample_scale=scale;return 0;
}
