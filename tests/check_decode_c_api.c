#include "../include/independent_aal.h"
#include <string.h>
int check_decode_c_abi(void) {
    aal_pcm_buffer output = {0};
    char message[2] = {'x','x'};
    if (aal_decode_cd16(0,0,&output,0,0,message,sizeof(message)) != AAL_INVALID_OR_FAILED) return 1;
    if (output.data || output.sample_frames || message[1] != 0) return 2;
    if (aal_decode_cd16(0,0,0,0,0,0,0) != AAL_INVALID_OR_FAILED) return 3;
    aal_pcm_free(&output); aal_pcm_free(0);
    return 0;
}
