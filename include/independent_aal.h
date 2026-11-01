#ifndef INDEPENDENT_AAL_H
#define INDEPENDENT_AAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum aal_status {AAL_OK=0,AAL_INVALID_OR_FAILED=1,AAL_CANCELLED=2} aal_status;
/* Return nonzero to cancel. Called synchronously on the caller's thread.
   stage_utf8 is valid only for the duration of this callback. */
typedef int (*aal_progress)(int percent,const char* stage_utf8,void* context);
typedef struct aal_buffer {
    uint8_t* data;
    size_t size;
    char pcm_sha256[65];
} aal_buffer;
typedef struct aal_profile_description {
    const char* id;          /* Static UTF-8 strings owned by this library. */
    const char* family;
    uint32_t nominal_bitrate;
    uint32_t core_bytes;
    uint32_t frame_samples;
    uint32_t pcm_trim;
} aal_profile_description;
size_t aal_profile_count(void);
int aal_profile_get(size_t index,aal_profile_description* output);
/* 44100 Hz, signed 16-bit, interleaved stereo; sample_frames counts L/R pairs.
   The function allocates output, performs a PCM round-trip, and never invokes
   external software. Pass an initially zeroed output; free it with aal_free.
   error_utf8 is optional and always NUL-terminated when error_capacity > 0. */
/* Default profile: AALP / ATRAC3plus 352 kbps. */
aal_status aal_encode_cd16(const int16_t* pcm,size_t sample_frames,aal_buffer* output,
                          aal_progress progress,void* context,
                          char* error_utf8,size_t error_capacity);
/* Explicit profiles are returned by aal_profile_get, e.g. "aalp-352" or
   "aal3-132". An unsupported identifier fails; it is never substituted. */
aal_status aal_encode_cd16_profile(const int16_t* pcm,size_t sample_frames,
                                  const char* profile_id,aal_buffer* output,
                                  aal_progress progress,void* context,
                                  char* error_utf8,size_t error_capacity);
void aal_free(aal_buffer* buffer);
#ifdef __cplusplus
}
#endif
#endif
