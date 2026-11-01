#include "../include/independent_aal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int condition,const char* message) {
    if(!condition){fprintf(stderr,"FAIL: %s\n",message);exit(1);}
}
static int cancel_progress(int percent,const char* stage,void* context) {
    (void)percent;(void)stage;(*(int*)context)++;return 1;
}
int main(void) {
    int16_t pcm[2053*2];uint32_t random=42;
    for(size_t i=0;i<sizeof(pcm)/sizeof(pcm[0]);++i){
        random=random*1664525u+1013904223u;pcm[i]=(int16_t)(random>>16);
    }
    require(aal_profile_count()==10,"All ten official AAL profiles must be exposed");
    char reference_hash[65]={0},error[256];
    for(size_t i=0;i<aal_profile_count();++i){
        aal_profile_description profile;aal_buffer out={0};
        require(aal_profile_get(i,&profile),"Profile enumeration failed");
        require(aal_encode_cd16_profile(pcm,2053,profile.id,&out,NULL,NULL,error,sizeof(error))==AAL_OK,error);
        require(out.data&&out.size>96,"Encoded output missing");
        require(out.data[32]==(strcmp(profile.family,"AALP")==0?0x21:0x22),"Wrong family header");
        const uint32_t params=((uint32_t)out.data[49]<<16)|((uint32_t)out.data[50]<<8)|out.data[51];
        require(((params&0x3ffu)+(out.data[32]==0x21?1u:0u))*8u==profile.core_bytes,"Wrong core profile in emitted header");
        if(!i)memcpy(reference_hash,out.pcm_sha256,65);
        require(strcmp(reference_hash,out.pcm_sha256)==0,"Input PCM hash differs across profiles");
        aal_free(&out);require(!out.data&&!out.size&&!out.pcm_sha256[0],"Free did not reset buffer");
        aal_free(&out);
    }
    {aal_profile_description p;require(!aal_profile_get(10,&p)&&!aal_profile_get(0,NULL),"Invalid profile index accepted");}
    {
        aal_buffer out={0};
        require(aal_encode_cd16(pcm,2053,&out,NULL,NULL,error,sizeof(error))==AAL_OK,error);
        require(out.data[32]==0x21&&out.data[50]==0x28&&out.data[51]==0xff,"Default must be AALP352");
        aal_free(&out);
        require(aal_encode_cd16_profile(pcm,2053,"aal3-66",&out,NULL,NULL,error,sizeof(error))==AAL_INVALID_OR_FAILED,"Invalid rate silently substituted");
        require(!out.data&&!out.size,"Failed operation published output");
        int calls=0;
        require(aal_encode_cd16(pcm,2053,&out,cancel_progress,&calls,error,sizeof(error))==AAL_CANCELLED,"Cancellation ignored");
        require(calls==1&&!out.data&&!out.size,"Cancelled operation published output");
        require(aal_encode_cd16(NULL,2053,&out,NULL,NULL,error,sizeof(error))==AAL_INVALID_OR_FAILED,"Null PCM accepted");
        require(aal_encode_cd16(pcm,0,&out,NULL,NULL,error,sizeof(error))==AAL_INVALID_OR_FAILED,"Empty PCM accepted");
        require(aal_encode_cd16(pcm,2053,NULL,NULL,NULL,error,sizeof(error))==AAL_INVALID_OR_FAILED,"Null output accepted");
    }
    puts("PASS: C ABI, 10 profiles, default AALP352, headers, PCM hashes, cancellation and invalid input");
    return 0;
}
