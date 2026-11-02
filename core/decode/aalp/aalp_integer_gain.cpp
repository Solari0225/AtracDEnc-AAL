#include "integer_gain.h"
#include "gain_math.inc"
static int16_t word(int32_t a){return (int16_t)(uint16_t)a;}
static int32_t sar32(int32_t x,uint32_t n){n&=31;if(!n)return x;return (int32_t)(((uint32_t)x>>n)|(x<0?(~0u<<(32-n)):0u));}
static int16_t sar16(int16_t x,uint32_t n){n&=31;if(n>=15)return x<0?-1:0;return (int16_t)sar32(x,n);}
static int16_t saturate(int32_t a){return (int16_t)(a>32767?32767:a<-32768?-32768:a);}
int sony_aalp_gain_curve(const sony_aalp_gain *g,int16_t coefficients[128],int16_t *scale){
    if(!g||!coefficients||!scale||g->count>7)return -1;
    for(uint32_t i=0;i<g->count;++i)if(g->locations[i]>31||g->levels[i]>15||(i&&g->locations[i]<=g->locations[i-1]))return -1;
    int16_t exponent[32];uint32_t at=0;int32_t current=g->count?aalp_gain_scale[g->levels[0]]:0;
    for(uint32_t p=0;p<g->count;++p){
        while(at<g->locations[p])exponent[at++]=(int16_t)(current+14);
        const int32_t next=p+1<g->count?aalp_gain_scale[g->levels[p+1]]:0;
        int32_t value=current+14;
        if(current>next){const int32_t candidate=next+aalp_gain_interpolation_shift[current-next];if(candidate<value)value=candidate;}
        exponent[at++]=(int16_t)value;current=next;
    }
    while(at<32)exponent[at++]=14;
    int16_t minimum=exponent[0];for(uint32_t i=1;i<32;++i)if(exponent[i]<minimum)minimum=exponent[i];*scale=minimum;
    at=0;current=g->count?aalp_gain_scale[g->levels[0]]:0;
    for(uint32_t p=0;p<g->count;++p){
        while(at<g->locations[p]){for(uint32_t j=0;j<4;++j)coefficients[at*4+j]=word(sar32(0x4000,(uint32_t)(exponent[at]-minimum)));++at;}
        const int32_t next=p+1<g->count?aalp_gain_scale[g->levels[p+1]]:0;
        if(current==next){for(uint32_t j=0;j<4;++j)coefficients[at*4+j]=word(sar32(0x4000,(uint32_t)(exponent[at]-minimum)));}
        else{
            const int32_t difference=current<next?next-current:current-next;
            const int32_t shift=aalp_gain_interpolation_shift[difference]-minimum+(current<next?current:next);
            coefficients[at*4]=word(sar32(0x4000,(uint32_t)(current-minimum+14)));
            for(uint32_t j=0;j<3;++j){const uint32_t order=current<next?2-j:j;coefficients[at*4+1+j]=sar16(aalp_gain_interpolation[(difference-1)*3+order],(uint32_t)shift);}
        }
        ++at;current=next;
    }
    while(at<32){for(uint32_t j=0;j<4;++j)coefficients[at*4+j]=word(sar32(0x4000,(uint32_t)(exponent[at]-minimum)));++at;}
    return 0;
}
int sony_aalp_apply_gain(const sony_aalp_gain *g,int16_t samples[128],int16_t *scale){
    if(!samples||!scale)return -1;int16_t coefficients[128],curve_scale;
    if(sony_aalp_gain_curve(g,coefficients,&curve_scale))return -1;
    const int32_t shift=curve_scale>14?curve_scale:14;
    for(uint32_t i=0;i<128;++i)samples[i]=saturate(sar32((int32_t)samples[i]*coefficients[i],(uint32_t)shift));
    *scale=word(*scale+curve_scale-shift);return 0;
}
