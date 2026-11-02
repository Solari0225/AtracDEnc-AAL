#include "fixed_predictor.h"
#include "../../plus/dequant/independent_plus_dequant.h"
#include <cstring>
namespace {
const uint16_t starts[33]={0,16,32,48,64,80,96,112,128,160,192,224,256,288,320,352,384,448,512,576,640,704,768,896,1024,1152,1280,1408,1536,1664,1792,1920,2048};
}
extern "C" void sony_original_aalp_predictor_reset(sony_aalp_predictor_state *s){
    if(!s)return;std::memset(s,0,sizeof(*s));for(uint32_t ch=0;ch<2;++ch)for(uint32_t b=0;b<16;++b)aal_plus_transform_reset(&s->transform[ch][b]);
}
extern "C" int sony_original_aalp_predict(const sony_aalp_core_fields *c,uint32_t cap,
    sony_aalp_predictor_state *s,int16_t output[2][2048]){
    if(!c||!s||!output||c->channels<1||c->channels>2||c->units>32||c->frame_subbands>16||c->coefficient_subbands>16||cap>16)return -1;
    const uint32_t active=cap<c->frame_subbands?cap:c->frame_subbands;
    int32_t wl[2][32];int16_t quantized[2][2048],frequency[2][16][128],frequency_scale[2][16];
    std::memcpy(wl,c->word_length,sizeof(wl));std::memcpy(quantized,c->quantized,sizeof(quantized));
    if(c->channels==2)for(uint32_t q=0;q<c->units;++q)if(wl[1][q]==0&&wl[0][q]>0&&c->codebook[1][q]==0){
        for(uint32_t i=starts[q];i<starts[q+1];++i)quantized[1][i]=quantized[0][i];wl[1][q]=wl[0][q];
    }
    for(uint32_t ch=0;ch<c->channels;++ch)if(aal_plus_fixed_dequant(wl[ch],c->scale_factor[ch],quantized[ch],c->coefficient_subbands,frequency[ch],frequency_scale[ch]))return -1;
    if(c->channels==2)for(uint32_t b=0;b<c->coefficient_subbands;++b){
        if(c->swap[b]==1){for(uint32_t i=0;i<128;++i){const int16_t x=frequency[0][b][i];frequency[0][b][i]=frequency[1][b][i];frequency[1][b][i]=x;}const int16_t x=frequency_scale[0][b];frequency_scale[0][b]=frequency_scale[1][b];frequency_scale[1][b]=x;}
        if(c->negative[b]==1)for(uint32_t i=0;i<128;++i)frequency[1][b][i]=frequency[1][b][i]==-32768?32767:(int16_t)-frequency[1][b][i];
    }
    if(c->mute==1){std::memset(frequency,0,sizeof(frequency));for(uint32_t ch=0;ch<c->channels;++ch)for(uint32_t b=0;b<16;++b)frequency_scale[ch][b]=24;}
    for(uint32_t ch=0;ch<c->channels;++ch){
        int16_t time[16][128]={},scales[16];
        for(uint32_t b=0;b<16;++b){
            scales[b]=24;
            /* Native37f640 clears unused overlap and writes24 to its scales
             * after processing the active prefix. This matters when header
             * subband counts shrink and later grow again. */
            if(b>=active){aal_plus_transform_reset(&s->transform[ch][b]);continue;}
            const int gain_scale=c->gain[ch][b].count?(int)c->gain[ch][b].levels[0]-6:0;
            if(c->gain[ch][b].count>7||(c->gain[ch][b].count&&c->gain[ch][b].levels[0]>15)||s->previous_gain[ch][b].window>1)return -1;
            if(aal_plus_fixed_imdct(frequency[ch][b],frequency_scale[ch][b],time[b],scales+b,&s->transform[ch][b],gain_scale,s->previous_gain[ch][b].window,b&1))return -1;
            if(s->previous_gain[ch][b].count&&sony_aalp_apply_gain(&s->previous_gain[ch][b],time[b],scales+b))return -1;
        }
        const int16_t *pointers[16];for(uint32_t b=0;b<16;++b)pointers[b]=time[b];
        if(aal_plus_fixed_ipqf(pointers,scales,output[ch],&s->synthesis[ch],active))return -1;
        for(uint32_t b=0;b<16;++b){
            /* The physical gain reader zeroes all16 complete64B records
             * before37f340 copies only the declared window flag prefix. */
            const uint32_t now=b<c->frame_subbands?c->window[ch][b]:0;s->older_window[ch][b]=s->previous_gain[ch][b].window;s->previous_gain[ch][b]=c->gain[ch][b];s->previous_gain[ch][b].window=now;
        }
    }
    return 0;
}
