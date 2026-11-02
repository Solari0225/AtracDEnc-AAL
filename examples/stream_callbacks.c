/* Host integration sketch. All PCM is 44100 Hz / signed16 / stereo.
   Callback buffers are borrowed, and file/stream publication belongs to the
   host. Return nonzero from consume_* to abort on output failure. */
#include "../include/independent_aal.h"

aal_status encode_with_callbacks(uint64_t frames,
    aal_pcm_read read_pcm,void* source,aal_byte_write consume_aal,void* sink,
    aal_stream_result* result,char* error,size_t capacity) {
    return aal_encode_cd16_stream(frames,"aalp-352",read_pcm,source,
        consume_aal,sink,result,0,0,error,capacity);
}

aal_status decode_with_callbacks(uint64_t bytes,
    aal_byte_read read_aal,void* source,aal_pcm_write consume_pcm,void* sink,
    aal_stream_result* result,char* error,size_t capacity) {
    return aal_decode_cd16_stream(bytes,read_aal,source,
        consume_pcm,sink,result,0,0,error,capacity);
}
