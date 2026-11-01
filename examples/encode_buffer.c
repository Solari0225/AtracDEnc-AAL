#include "independent_aal.h"
#include <stdio.h>

/* Call from the host application with its own CD PCM buffer. The application
   controls its UI and thread scheduling; this function performs no UI work. */
int encode_buffer_example(const int16_t* stereo_pcm,size_t frame_count) {
    aal_buffer output={0};char error[512];
    /* Change this explicit profile or enumerate aal_profile_get() in the host.
       aal_encode_cd16() without a profile also defaults to aalp-352. */
    const aal_status status=aal_encode_cd16_profile(stereo_pcm,frame_count,"aalp-352",&output,NULL,NULL,error,sizeof error);
    if(status!=AAL_OK){fprintf(stderr,"AAL encode failed: %s\n",error);return (int)status;}
    /* Use output.data[0..output.size) here, e.g. write it through your app's
       own file API. output.pcm_sha256 is the original PCM digest. */
    printf("Encoded %zu bytes, PCM SHA-256 %s\n",output.size,output.pcm_sha256);
    aal_free(&output);return 0;
}
