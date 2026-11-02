#include "core_reader.h"
#include <cstring>

namespace {
struct huff {uint16_t code;uint8_t width;};
typedef huff plus_quant_huff;
typedef huff plus_code_huff;
typedef huff plus_gain_huff;
typedef huff tone_huff;
typedef huff spectrum_huff;
struct plus_spectrum_descriptor {
    uint32_t first_row,row_count;
    uint8_t coefficient_group,symbol_group,coefficient_shift,extra_signs;
    uint8_t coefficient_bits,coefficient_mask;
};
#include "../tables/quant_huffman.inc"
#include "../tables/code_table_huffman.inc"
#include "../tables/gain_huffman.inc"
#include "../tables/gain_location_huffman.inc"
#include "../tables/tone_math.inc"
#include "../tables/spectrum_math.inc"
#include "reader_math.inc"

struct reader {
    const uint8_t *data;uint32_t limit,pos;bool bad;
    uint32_t get(uint32_t width) {
        if(width>24||pos>limit||width>limit-pos){bad=true;return 0;}
        uint32_t value=0;
        for(uint32_t i=0;i<width;++i,++pos)value=(value<<1)|((data[pos>>3]>>(7-(pos&7)))&1);
        return value;
    }
    uint32_t symbol(const huff *table,uint32_t count) {
        if(bad)return 0;
        uint32_t available=limit-pos;if(available>16)available=16;
        uint32_t prefix=0;
        for(uint32_t i=0;i<available;++i)prefix=(prefix<<1)|((data[(pos+i)>>3]>>(7-((pos+i)&7)))&1);
        prefix<<=16-available;
        for(uint32_t i=0;i<count;++i){const uint32_t n=table[i].width;
            if(n&&n<=available&&(prefix>>(16-n))==table[i].code){pos+=n;return i;}}
        bad=true;return 0;
    }
    void flags(uint32_t *values,uint32_t count) {
        const uint32_t present=get(1),explicit_bits=present?get(1):0;
        for(uint32_t i=0;i<count;++i)values[i]=present?(explicit_bits?get(1):1):0;
    }
};
int32_t signed4(uint32_t x){return (x&8)?(int32_t)x-16:(int32_t)x;}
int32_t signed3(uint32_t x){return (x&4)?(int32_t)x-8:(int32_t)x;}
uint32_t band_count(uint32_t units){return (uint32_t)reader_qu_bands[units]+1;}
void wl(reader &r,sony_aalp_core_fields &c,uint32_t declared) {
    for(uint32_t ch=0;ch<c.channels;++ch){
        int32_t *v=c.word_length[ch];const uint32_t mode=r.get(2);
        if(mode==0){for(uint32_t q=0;q<declared;++q)v[q]=(int32_t)r.get(3);continue;}
        uint32_t weight=0;
        if((mode==1&&ch==0)||mode==3)weight=r.get(2);
        const uint32_t range=r.get(2),n=range?r.get(5):declared;
        const uint32_t fill=range==3?r.get(2)+(ch?3:1):0;
        if(n>declared){r.bad=true;return;}
        if(n){
            if(mode==1&&ch==0){
                const uint32_t prefix=r.get(5),width=r.get(2),base=r.get(3);
                if(prefix>n){r.bad=true;return;}
                for(uint32_t q=0;q<prefix;++q)v[q]=(int32_t)r.get(3);
                for(uint32_t q=prefix;q<n;++q)v[q]=(int32_t)(base+r.get(width));
            }else if(mode==2&&ch==0){
                const uint32_t grouped=r.get(1),book=r.get(1),anchor=r.get(3),shape=r.get(4);
                for(uint32_t q=0;q<n;++q){const uint32_t group=reader_qu_groups[q];
                    v[q]=(int32_t)anchor-(group?reader_wl_shapes[(anchor*16+shape)*9+group-1]:0);}
                uint32_t q=0;
                if(grouped)for(;q+1<n;q+=2){if(!r.get(1)){
                    v[q]+=(int32_t)r.symbol(wl_huffman[book],8);
                    v[q+1]+=(int32_t)r.symbol(wl_huffman[book],8);}}
                for(;q<n;++q)v[q]+=(int32_t)r.symbol(wl_huffman[book],8);
                for(q=0;q<n;++q)v[q]&=7;
            }else{
                const uint32_t book=r.get(2);
                if(mode==3){v[0]=(int32_t)r.get(3);
                    for(uint32_t q=1;q<n;++q)v[q]=(v[q-1]+(int32_t)r.symbol(wl_huffman[book],8))&7;
                }else if(mode==1){
                    for(uint32_t q=0;q<n;++q)v[q]=(c.word_length[0][q]+(int32_t)r.symbol(wl_huffman[book],8))&7;
                }else{
                    int32_t delta=(int32_t)r.symbol(wl_huffman[book],8);
                    v[0]=(c.word_length[0][0]+delta)&7;
                    for(uint32_t q=1;q<n;++q){delta+=(int32_t)r.symbol(wl_huffman[book],8);v[q]=(c.word_length[0][q]+delta)&7;}
                }
            }
        }
        uint32_t tail_end=declared;
        if(range==3){
            if(!ch){if(fill>declared||n>declared-fill){r.bad=true;return;}tail_end=declared-fill;}
            else{tail_end=n+fill;if(tail_end>declared){r.bad=true;return;}}
        }
        for(uint32_t q=n;q<declared;++q)v[q]=range==2?(ch?(int32_t)r.get(1):1):(range==3&&q<tail_end?1:0);
        if(weight)for(uint32_t q=0;q<declared;++q)v[q]+=reader_wl_weights[(ch*3+weight)*32+q];
        for(uint32_t q=0;q<declared;++q)if(v[q]<0||v[q]>7){r.bad=true;return;}
    }
    c.units=declared;
    while(c.units&&!c.word_length[0][c.units-1]&&(c.channels==1||!c.word_length[1][c.units-1]))--c.units;
    c.coefficient_subbands=band_count(c.units);
}
void sf(reader &r,sony_aalp_core_fields &c) {
    const uint32_t n=c.units;if(!n)return;
    for(uint32_t ch=0;ch<c.channels;++ch){
        int32_t *v=c.scale_factor[ch];const uint32_t mode=r.get(2);
        if(!mode){for(uint32_t q=0;q<n;++q)v[q]=(int32_t)r.get(6);continue;}
        if(ch){
            if(mode==3){for(uint32_t q=0;q<n;++q)v[q]=c.scale_factor[0][q];continue;}
            const uint32_t book=r.get(2);int32_t delta=0;
            for(uint32_t q=0;q<n;++q){const int32_t x=(int32_t)r.symbol(sf64_huffman[book],64);
                delta=mode==1?x:delta+x;v[q]=(c.scale_factor[0][q]+delta)&63;}
            continue;
        }
        uint32_t weight=0,book=0,anchor=0,shape=0;int32_t residual[32]={0};
        if(mode==1){
            weight=r.get(2);
            if(weight==3){anchor=r.get(6);shape=r.get(6);}
            const uint32_t prefix=r.get(5),width=r.get(weight==3?2:3);
            const int32_t base=(int32_t)r.get(weight==3?4:6)-(weight==3?7:0);
            if(prefix>n||width>6){r.bad=true;return;}
            for(uint32_t q=0;q<prefix;++q)residual[q]=(int32_t)r.get(weight==3?4:6)-(weight==3?7:0);
            for(uint32_t q=prefix;q<n;++q)residual[q]=base+(int32_t)r.get(width);
        }else if(mode==2){
            weight=3;book=r.get(2);anchor=r.get(6);shape=r.get(6);
            for(uint32_t q=0;q<n;++q)residual[q]=signed4(r.symbol(sf16_huffman[book],16));
        }else{
            weight=r.get(2);book=r.get(2);
            if(weight==3){anchor=r.get(6);shape=r.get(6);}
            residual[0]=((int32_t)r.get(weight==3?4:6)-(weight==3?8:0))&63;
            for(uint32_t q=1;q<n;++q){const int32_t x=weight==3?signed4(r.symbol(sf16_huffman[book],16)):(int32_t)r.symbol(sf64_huffman[book],64);
                residual[q]=(residual[q-1]+x)&63;}
        }
        for(uint32_t q=0;q<n;++q){
            if(weight==3){const uint32_t group=reader_qu_groups[q];
                const int32_t predicted=(int32_t)anchor-(group?reader_sf_shapes[shape*9+group-1]:0);
                v[q]=(predicted+residual[q])&63;
            }else{v[q]=residual[q]-(weight?(int32_t)reader_sf_weights[(weight-1)*32+q]:0);
                if(v[q]<0||v[q]>63){r.bad=true;return;}}
        }
    }
}
void books(reader &r,sony_aalp_core_fields &c,uint32_t &full,uint32_t bank[2]) {
    if(!c.units)return;full=r.get(1);
    const uint32_t width=code_table_direct_widths[full],mask=(1u<<width)-1u;
    const huff *absolute=full?code_table_full_absolute:code_table_short;
    const huff *delta=full?code_table_full_delta:code_table_short;
    const huff *stereo=full?code_table_full_stereo_delta:code_table_short;
    for(uint32_t ch=0;ch<c.channels;++ch){bank[ch]=r.get(1);const uint32_t mode=r.get(2);
        if(mode==3&&!ch)continue;
        const uint32_t partial=r.get(1),n=partial?r.get(5):c.units;
        if(n>c.units){r.bad=true;return;}uint32_t previous=0;
        for(uint32_t q=0;q<n;++q){
            if(c.word_length[ch][q]>0){uint32_t x;
                if(mode==0)x=r.get(width);
                else if(mode==1||(mode==2&&!q))x=r.symbol(absolute,mask+1);
                else if(mode==2)x=(r.symbol(delta,mask+1)+previous)&mask;
                else x=(r.symbol(stereo,mask+1)+(uint32_t)c.codebook[0][q])&mask;
                c.codebook[ch][q]=(int32_t)x;if(mode==2)previous=x;
            }else if(ch&&c.word_length[0][q]>0)c.codebook[ch][q]=(int32_t)r.get(1);
        }
    }
}
void spectrum(reader &r,sony_aalp_core_fields &c,uint32_t full,const uint32_t bank[2]) {
    for(uint32_t ch=0;ch<c.channels;++ch){
        for(uint32_t q=0;q<c.units;++q){const uint32_t word=(uint32_t)c.word_length[ch][q];if(!word)continue;
            uint32_t book=(uint32_t)c.codebook[ch][q];if(!full)book=spectrum_short_map[book+4*(7*bank[ch]+word)];
            const plus_spectrum_descriptor &d=spectrum_books[7*(book+8*bank[ch])+word];
            uint32_t at=spectrum_qu_start[q],end=at+spectrum_qu_count[q];
            if(!d.coefficient_group||!d.symbol_group){r.bad=true;return;}
            while(at<end){const uint32_t present=d.symbol_group>1?r.get(1):1;
                if(!present){at+=d.coefficient_group*d.symbol_group;continue;}
                for(uint32_t s=0;s<d.symbol_group;++s){const uint32_t packed=r.symbol(spectrum_codes+d.first_row,d.row_count);
                    for(uint32_t k=0;k<d.coefficient_group;++k){
                        const uint32_t magnitude=(packed>>((d.coefficient_group-1-k)*d.coefficient_bits))&d.coefficient_mask;
                        int32_t value=(int32_t)magnitude;
                        if(d.extra_signs){if(magnitude&&r.get(1))value=-value;}
                        else if(magnitude&(1u<<(d.coefficient_bits-1)))value-=(int32_t)(1u<<d.coefficient_bits);
                        if(at>=end){r.bad=true;return;}c.quantized[ch][at++]=(int16_t)value;
                    }
                }
            }
        }
        if(c.units>2)for(uint32_t i=0;i<(uint32_t)spectrum_trailing_count[c.coefficient_subbands]+1;++i)r.get(4);
    }
    if(c.channels==2){r.flags(c.swap,c.coefficient_subbands);r.flags(c.negative,c.coefficient_subbands);}
}
void within_levels(reader &r,sony_aalp_gain &b){
    if(!b.count)return;b.levels[0]=r.symbol(gain_level_absolute,16);
    for(uint32_t p=1;p<b.count;++p)b.levels[p]=(b.levels[p-1]+r.symbol(gain_level_within_band_delta,16))&15;
}
uint32_t direct_location(reader &r,const sony_aalp_gain &b,uint32_t point){
    if(!point||b.locations[point-1]<15)return r.get(5);
    const uint32_t prev=b.locations[point-1];
    if(prev<23)return prev+1+r.get(4);
    if(prev<27)return prev+1+r.get(3);
    if(prev<29)return prev+1+r.get(2);
    if(prev==29)return 30+r.get(1);
    return 31;
}
void within_locations(reader &r,sony_aalp_gain &b){
    if(!b.count)return;b.locations[0]=r.get(5);
    for(uint32_t p=1;p<b.count;++p)b.locations[p]=b.locations[p-1]+r.symbol(b.levels[p]>b.levels[p-1]?gain_location_within_rise:gain_location_within_nonrise,32);
}
void gain(reader &r,sony_aalp_core_fields &c){
    for(uint32_t ch=0;ch<c.channels;++ch){if(!r.get(1))continue;
        const uint32_t n=r.get(4)+1,repeat=r.get(1),end=repeat?r.get(4)+1:n;
        const uint32_t cm=r.get(2);uint32_t width=0,base=0;
        if(cm==3&&!ch){width=r.get(2);base=r.get(3);}
        for(uint32_t b=0;b<n;++b){sony_aalp_gain &v=c.gain[ch][b];
            if(cm==0)v.count=r.get(3);
            else if(cm==1||(cm==2&&!ch&&!b))v.count=r.symbol(gain_count_absolute,8);
            else if(cm==2)v.count=((ch?c.gain[0][b].count:c.gain[ch][b-1].count)+r.symbol(gain_count_delta,8))&7;
            else v.count=ch?c.gain[0][b].count:base+r.get(width);
            if(v.count>7){r.bad=true;return;}
        }
        const uint32_t lm=r.get(2);
        if(lm==3&&!ch){width=r.get(2);base=r.get(4);}
        for(uint32_t b=0;b<n;++b){sony_aalp_gain &v=c.gain[ch][b];const sony_aalp_gain &ref=c.gain[0][b];
            if(lm==0)for(uint32_t p=0;p<v.count;++p)v.levels[p]=r.get(4);
            else if(lm==1&&!ch)within_levels(r,v);
            else if(lm==1)for(uint32_t p=0;p<v.count;++p)v.levels[p]=((p<ref.count?ref.levels[p]:7)+r.symbol(gain_level_stereo_delta,16))&15;
            else if(lm==2&&ch){const uint32_t explicit_values=v.count?r.get(1):0;
                if(explicit_values)within_levels(r,v);else for(uint32_t p=0;p<v.count;++p)v.levels[p]=p<ref.count?ref.levels[p]:7;
            }else if(lm==2){
                if(!b)within_levels(r,v);else{const sony_aalp_gain &pred=c.gain[ch][b-1];
                    for(uint32_t p=0;p<v.count;++p)v.levels[p]=((p<pred.count?pred.levels[p]:7)+r.symbol(gain_level_band_delta,16))&15;}
            }else for(uint32_t p=0;p<v.count;++p)v.levels[p]=ch?(p<ref.count?ref.levels[p]:7):base+r.get(width);
            for(uint32_t p=0;p<v.count;++p)if(v.levels[p]>15){r.bad=true;return;}
        }
        const uint32_t pm=r.get(2);
        if(pm==3&&!ch){width=r.get(2)+1;base=r.get(5);}
        for(uint32_t b=0;b<n;++b){sony_aalp_gain &v=c.gain[ch][b];const sony_aalp_gain &ref=c.gain[0][b];
            if(pm==0||(pm==2&&!ch&&!b))for(uint32_t p=0;p<v.count;++p)v.locations[p]=direct_location(r,v,p);
            else if(pm==1&&!ch)within_locations(r,v);
            else if(pm==2&&ch){
                if(v.count){const uint32_t explicit_values=v.count<=ref.count?r.get(1):1;
                    if(explicit_values)within_locations(r,v);else for(uint32_t p=0;p<v.count;++p)v.locations[p]=ref.locations[p];}
            }else if(pm==3){for(uint32_t p=0;p<v.count;++p)v.locations[p]=ch?(p<ref.count?ref.locations[p]:direct_location(r,v,p)):base+p+r.get(width);
            }else{
                const sony_aalp_gain &pred=ch?ref:c.gain[ch][b-1];
                if(v.count)v.locations[0]=((pred.count?pred.locations[0]:0)+r.symbol(ch?gain_location_stereo:gain_location_band_nonrise,32))&31;
                for(uint32_t p=1;p<v.count;++p){const bool available=p<pred.count,rising=v.levels[p]>v.levels[p-1];
                    if(ch&&available&&rising)v.locations[p]=r.get(1)?direct_location(r,v,p):pred.locations[p];
                    else{const huff *table=available?(ch?gain_location_stereo:(rising?gain_location_band_rise:gain_location_band_nonrise)):(rising?gain_location_within_rise:gain_location_within_nonrise);
                        const uint32_t delta=r.symbol(table,32);v.locations[p]=available?(pred.locations[p]+delta)&31:v.locations[p-1]+delta;}
                }
            }
            for(uint32_t p=0;p<v.count;++p)if(v.locations[p]>31||(p&&v.locations[p]<=v.locations[p-1])){r.bad=true;return;}
        }
        for(uint32_t b=n;b<end;++b)c.gain[ch][b]=c.gain[ch][n-1];
    }
}
uint32_t tone_symbol(reader &r,uint32_t table){return r.symbol(tone_codes+tone_book_starts[table],tone_book_sizes[table]);}
void tones(reader &r,uint32_t channels){
    if(!r.get(1))return;
    const uint32_t amplitude_mode=r.get(1),bands=tone_symbol(r,0)+1;
    uint32_t shared[16]={0},unused[16]={0},counts[2][16]={{0}};
    if(bands>16){r.bad=true;return;}
    if(channels==2){r.flags(shared,bands);r.flags(unused,bands);r.flags(unused,bands);}
    for(uint32_t ch=0;ch<channels;++ch){
        const uint32_t envelope=ch?r.get(1):0;
        if(!envelope)for(uint32_t b=0;b<bands;++b)if(!ch||!shared[b]){if(r.get(1))r.get(5);if(r.get(1))r.get(5);}
        const uint32_t cm=r.get(ch?2:1);
        for(uint32_t b=0;b<bands;++b)if(!ch||!shared[b]){
            /* The stereo Huffman alphabet has8 signed deltas, but the
             * reconstructed count wraps at16 (native383b40), not at8. */
            counts[ch][b]=cm==0?r.get(4):(cm==1?tone_symbol(r,1):(cm==2?(counts[0][b]+signed3(tone_symbol(r,2)))&15:counts[0][b]));}
        uint32_t total=0;
        for(uint32_t cc=0;cc<=ch;++cc)for(uint32_t b=0;b<bands;++b)total+=counts[cc][b];
        if(total>48){r.bad=true;return;}
        const uint32_t fm=ch?r.get(1):0;
        for(uint32_t b=0;b<bands;++b)if(!ch||!shared[b]){const uint32_t n=counts[ch][b];
            if(fm){for(uint32_t p=0;p<n;++p)tone_symbol(r,3);}
            else{
                const uint32_t descending=n>1?r.get(1):0;uint32_t previous=0;
                for(uint32_t p=0;p<n;++p){uint32_t width=10,base=0;
                    if(p){if(descending){width=1;while(width<10&&previous>=(1u<<width))++width;}
                        else{while(width>1&&previous>=1024-(1u<<(width-1)))--width;base=1024-(1u<<width);}}
                    previous=r.get(width)+base;
                }
            }
        }
        const uint32_t am=r.get(ch?2:1);
        if(am!=3)for(uint32_t b=0;b<bands;++b)if(!ch||!shared[b]){
            const uint32_t n=amplitude_mode?counts[ch][b]:(counts[ch][b]!=0);
            for(uint32_t p=0;p<n;++p){if(am==0)r.get(6);else tone_symbol(r,am==1?(amplitude_mode?5:4):6);}}
        if(!amplitude_mode){const uint32_t fine=r.get(ch?2:1);
            if(fine!=3)for(uint32_t b=0;b<bands;++b)if(!ch||!shared[b])for(uint32_t p=0;p<counts[ch][b];++p){
                if(!fine)r.get(4);else tone_symbol(r,fine==1?(counts[ch][b]==1?7:8):9);}}
        for(uint32_t b=0;b<bands;++b)if(!ch||!shared[b])for(uint32_t p=0;p<counts[ch][b];++p)r.get(5);
    }
}
}

extern "C" int sony_original_aalp_read_core(const uint8_t *data,uint32_t bytes,
    sony_aalp_core_fields *out,uint32_t positions[10]) {
    if(!data||!out||!positions||bytes>0x1fffffffu)return -1;
    std::memset(out,0,sizeof(*out));std::memset(positions,0,40);
    reader r={data,bytes*8,0,false};
    if(r.get(1))return -1;const uint32_t type=r.get(2);if(type>1)return -1;out->channels=type+1;
    const uint32_t declared=r.get(5)+1;out->frame_subbands=band_count(declared);out->mute=r.get(1);positions[0]=r.pos;
    if(r.bad)return -1;
    wl(r,*out,declared);positions[1]=r.pos;if(r.bad)return -2;
    sf(r,*out);positions[2]=r.pos;if(r.bad)return -3;
    uint32_t full=0,bank[2]={0};books(r,*out,full,bank);positions[3]=r.pos;if(r.bad)return -4;
    spectrum(r,*out,full,bank);positions[4]=r.pos;if(r.bad)return -5;
    for(uint32_t ch=0;ch<out->channels;++ch)r.flags(out->window[ch],out->frame_subbands);
    positions[5]=r.pos;if(r.bad)return -6;
    gain(r,*out);positions[6]=r.pos;if(r.bad)return -7;
    tones(r,out->channels);positions[7]=r.pos;if(r.bad)return -8;
    if(r.get(1)){r.get(4);r.get(4);}positions[8]=r.pos;if(r.bad)return -9;
    const uint32_t end=r.get(2);positions[9]=r.pos;if(r.bad||end!=3)return -10;
    return 0;
}
