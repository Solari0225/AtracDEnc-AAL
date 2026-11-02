/* Example host integration. The host reads OMA bytes and consumes PCM. */
#include "independent_aal.h"
#include <stdio.h>
int decode_audio(const uint8_t* oma, size_t oma_bytes) {
    aal_pcm_buffer pcm = {0};
    char error[512];
    aal_status status = aal_decode_cd16(oma, oma_bytes, &pcm,
                                      NULL, NULL, error, sizeof(error));
    if (status != AAL_OK) {
        fprintf(stderr, "%s\n", error);
        return (int)status;
    }
    /* pcm.data contains pcm.sample_frames L/R pairs at pcm.sample_rate.
       Copy/consume it before freeing. Raw PCM is NOT a WAV container. */
    printf("%s: %zu frames, PCM SHA-256 %s\n",
           pcm.profile_id, pcm.sample_frames, pcm.pcm_sha256);
    aal_pcm_free(&pcm);
    return 0;
}
