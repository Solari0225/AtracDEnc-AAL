#include "integer_gain.h"
#include <limits.h>
/* Exactly8unsigned Q16 values from the proven native table; see captured
 * gain-runtime-evidence.json. The multiplier is unsigned16, unlike IMDCT's
 * signed16 coefficient multiplier. */
static const uint16_t interpolation[8]={65535,60096,55108,50535,46340,42494,38967,35733};
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);}
static int32_t sar32(int32_t value,uint32_t count){
    count&=31;if(!count)return value;
    uint32_t bits=(uint32_t)value;bits=(bits>>count)|((bits&0x80000000u)?(~0u<<(32-count)):0);
    return signed_word(bits);
}
static int16_t clip16(int32_t value){
    return value>32767?32767:value< -32768?-32768:(int16_t)value;
}
static int32_t unsigned_coefficient_multiply(int32_t value,uint32_t coefficient,uint32_t shift){
    const int64_t product=(int64_t)value*(coefficient&65535u);
    return signed_word((uint32_t)((uint64_t)product>>(shift&31)));
}
int sony_aal_integer_gain(const int32_t input[256],const int16_t points[16],
    uint32_t mode,int16_t output[256])
{
    if(!input||!points||!output)return -1;
    uint32_t last=0;int terminal=0;
    for(uint32_t p=0;p<8;++p){
        const int32_t location=points[2*p],level=points[2*p+1];
        if(location<0||location>255||level<0||level>15||(p&&(uint32_t)location<last))return -1;
        last=(uint32_t)location;
        if(location==255){if(level!=4)return -1;terminal=1;break;}
    }
    if(!terminal)return -1;
    uint32_t position=0;mode=(uint16_t)mode!=0;
    for(uint32_t p=0;p<8;++p){
        const uint32_t endpoint=(uint32_t)points[2*p]+1;
        const int32_t level=points[2*p+1];
        for(;position<endpoint;++position){
            int32_t value=input[position];
            if(!mode)value=sar32(value,(uint32_t)(level+4));
            else if(level<4)value=signed_word((uint32_t)value<<((4-level)&31));
            else if(level>4)value=sar32(value,(uint32_t)(level-4));
            output[position]=clip16(value);
        }
        if(position>=256)return 0;
        if(p==7)return -1;
        const int32_t difference=points[2*p+3]-level;
        int32_t phase=level*8;
        for(uint32_t j=1;j<8;++j){
            if(position>=256)return -1;phase+=difference;
            const uint32_t shift=(uint32_t)(phase/8)+(mode?12u:20u);
            output[position]=clip16(unsigned_coefficient_multiply(input[position],interpolation[phase&7],shift));
            ++position;
        }
    }
    return -1;
}
