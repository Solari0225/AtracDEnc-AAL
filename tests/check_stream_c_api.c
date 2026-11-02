#include "../include/independent_aal.h"
#include <string.h>
struct stream_io {unsigned char bytes[16384];size_t size,position,source,decoded;};
static size_t input_pcm(int16_t* pcm,size_t count,void* context) {
    struct stream_io* io=(struct stream_io*)context;
    if(io->source||!count)return 0;
    pcm[0]=123;pcm[1]=-456;io->source=1;return 1;
}
static int output_aal(const uint8_t* bytes,size_t count,void* context) {
    struct stream_io* io=(struct stream_io*)context;
    if(count>sizeof(io->bytes)-io->size)return 1;
    memcpy(io->bytes+io->size,bytes,count);io->size+=count;return 0;
}
static size_t input_aal(uint8_t* bytes,size_t count,void* context) {
    struct stream_io* io=(struct stream_io*)context;
    if(count>io->size-io->position)count=io->size-io->position;
    if(count>13)count=13;
    memcpy(bytes,io->bytes+io->position,count);io->position+=count;return count;
}
static int output_pcm(const int16_t* pcm,size_t count,void* context) {
    struct stream_io* io=(struct stream_io*)context;
    if(count!=1||pcm[0]!=123||pcm[1]!=-456)return 1;
    io->decoded+=count;return 0;
}
int check_stream_c_abi(void) {
    struct stream_io io={0};aal_stream_result encoded={0},decoded={0};char error[128];
    if(aal_encode_cd16_stream(1,"aalp-352",input_pcm,&io,output_aal,&io,&encoded,0,0,error,sizeof error)!=AAL_OK)return 1;
    if(aal_decode_cd16_stream(io.size,input_aal,&io,output_pcm,&io,&decoded,0,0,error,sizeof error)!=AAL_OK)return 2;
    return io.decoded!=1||encoded.sample_frames!=1||decoded.sample_frames!=1||strcmp(encoded.pcm_sha256,decoded.pcm_sha256);
}
