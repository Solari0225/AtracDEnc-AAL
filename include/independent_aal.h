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

typedef struct aal_pcm_buffer {
    int16_t* data;           /* Interleaved L/R, sample_frames * 2 samples. */
    size_t sample_frames;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    char profile_id[16];
    char pcm_sha256[65];
} aal_pcm_buffer;
/* Decode unencrypted CD-format AAL3/AALP from a complete OMA/AA3 buffer.
   Profile detection is automatic. No fixed 1 GiB cap; this whole-buffer API
   needs memory for its result. Use the stream API below for bounded memory.
   Output must initially be zeroed; on success free with aal_pcm_free.
   Ordinary lossy ATRAC and encrypted OMA are not supported by this entry.
   Existing encoder ABI and aal_free remain unchanged. */
aal_status aal_decode_cd16(const uint8_t* oma,size_t oma_bytes,aal_pcm_buffer* output,
                          aal_progress progress,void* context,
                          char* error_utf8,size_t error_capacity);
void aal_pcm_free(aal_pcm_buffer* buffer);

/* Streaming callbacks are synchronous. Buffers are borrowed for the duration
   of a call only. Reads return an item count <= capacity (0 means EOF);
   SIZE_MAX or any count above capacity is an I/O error. PCM items are L/R
   pairs, not individual channel samples. Writes return 0 on success.
   Do not re-enter the same stream or retain callback buffer addresses.
   A failed/cancelled operation may have written a prefix: discard that sink.
   No Sony/FFmpeg/libFLAC runtime is used. The codec keeps bounded working
   buffers; caller callbacks decide their own storage and buffering. */
typedef size_t (*aal_pcm_read)(int16_t* pcm,size_t capacity_frames,void* context);
typedef size_t (*aal_byte_read)(uint8_t* bytes,size_t capacity_bytes,void* context);
typedef int (*aal_byte_write)(const uint8_t* bytes,size_t byte_count,void* context);
typedef int (*aal_pcm_write)(const int16_t* pcm,size_t sample_frames,void* context);
typedef struct aal_stream_result {
    uint64_t sample_frames;
    uint64_t output_bytes; /* Encode: AAL bytes. Decode: raw PCM bytes. */
    char profile_id[16];
    char pcm_sha256[65];
} aal_stream_result;
/* Declared input length must be known. Results are assigned only on success.
   Encode independently reparses each serialized packet and checks every PCM
   sample before writing that packet. Container count/duration overflow fails. */
aal_status aal_encode_cd16_stream(uint64_t sample_frames,const char* profile_id,
    aal_pcm_read read_pcm,void* read_context,aal_byte_write write_aal,void* write_context,
    aal_stream_result* result,aal_progress progress,void* progress_context,
    char* error_utf8,size_t error_capacity);
aal_status aal_decode_cd16_stream(uint64_t oma_bytes,
    aal_byte_read read_aal,void* read_context,aal_pcm_write write_pcm,void* write_context,
    aal_stream_result* result,aal_progress progress,void* progress_context,
    char* error_utf8,size_t error_capacity);
#ifdef __cplusplus
}
#endif
#endif
