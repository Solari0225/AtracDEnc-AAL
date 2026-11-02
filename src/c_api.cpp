#include "../include/independent_aal.h"
#include "encoder_api.hpp"
#include "platform_io.hpp"
#include "codec_profiles.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include "stream_codec.hpp"

extern "C" aal_status aal_encode_cd16_profile(const int16_t* pcm,size_t frames,const char* profile_id,aal_buffer* output,
                                             aal_progress callback,void* context,char* error,size_t capacity) {
    if(error&&capacity)error[0]=0;
    bool cancelled=false;
    try {
        if(!output||output->data)throw std::runtime_error("Output must be a zero-initialized aal_buffer.");
        if(!profile_id)throw std::runtime_error("A profile identifier is required.");
        independent_aal::EncodeOptions options;options.profile_id=profile_id;
        independent_aal::profile_by_id(options.profile_id);
        auto progress=[&](int percent,const std::wstring& stage){
            if(callback){const auto text=independent_aal::utf8(stage);if(callback(percent,text.c_str(),context)){cancelled=true;throw std::runtime_error("Cancelled");}}
        };
        auto result=independent_aal::encode_pcm16(pcm,frames,progress,nullptr,options);
        auto* bytes=static_cast<uint8_t*>(std::malloc(result.bytes.size()));
        if(!bytes)throw std::bad_alloc();
        std::memcpy(bytes,result.bytes.data(),result.bytes.size());
        output->data=bytes;output->size=result.bytes.size();
        std::memcpy(output->pcm_sha256,result.pcm_sha256.c_str(),65);return AAL_OK;
    }catch(const std::exception& exception){
        if(error&&capacity){const size_t n=std::min(capacity-1,std::strlen(exception.what()));std::memcpy(error,exception.what(),n);error[n]=0;}
        return cancelled?AAL_CANCELLED:AAL_INVALID_OR_FAILED;
    }catch(...){
        if(error&&capacity){const char* message="Unknown encoder failure.";const size_t n=std::min(capacity-1,std::strlen(message));std::memcpy(error,message,n);error[n]=0;}
        return AAL_INVALID_OR_FAILED;
    }
}
extern "C" aal_status aal_encode_cd16(const int16_t* pcm,size_t frames,aal_buffer* output,
                                     aal_progress callback,void* context,char* error,size_t capacity) {
    return aal_encode_cd16_profile(pcm,frames,"aalp-352",output,callback,context,error,capacity);
}
extern "C" size_t aal_profile_count(void) {return independent_aal::kOfficialProfiles.size();}
extern "C" int aal_profile_get(size_t index,aal_profile_description* output) {
    if(!output||index>=independent_aal::kOfficialProfiles.size())return 0;
    const auto& profile=independent_aal::kOfficialProfiles[index];
    *output={profile.id,profile.container_name,profile.nominal_bitrate,profile.core_bytes,profile.frame_samples,profile.pcm_trim};return 1;
}
extern "C" void aal_free(aal_buffer* buffer) {
    if(!buffer)return;std::free(buffer->data);buffer->data=nullptr;buffer->size=0;buffer->pcm_sha256[0]=0;
}

namespace {
template<class Operation>
aal_status stream_call(aal_stream_result* output,aal_progress callback,void* context,
                       char* error,size_t capacity,Operation operation) {
    if(error&&capacity)error[0]=0;
    bool cancelled=false;
    try {
        if(!output)throw std::runtime_error("Missing stream result.");
        const auto progress=[&](int percent,const std::wstring& stage) {
            if(callback){const auto text=independent_aal::utf8(stage);if(callback(percent,text.c_str(),context)){cancelled=true;throw std::runtime_error("Cancelled");}}
        };
        const auto value=operation(progress);aal_stream_result result{};
        result.sample_frames=value.sample_frames;result.output_bytes=value.bytes;
        std::memcpy(result.profile_id,value.profile_id.c_str(),value.profile_id.size()+1);
        std::memcpy(result.pcm_sha256,value.pcm_sha256.c_str(),65);*output=result;return AAL_OK;
    }catch(const std::exception& failure) {
        if(error&&capacity){const size_t n=std::min(capacity-1,std::strlen(failure.what()));std::memcpy(error,failure.what(),n);error[n]=0;}
        return cancelled?AAL_CANCELLED:AAL_INVALID_OR_FAILED;
    }catch(...) {
        if(error&&capacity){const char* message="Unknown streaming failure.";const size_t n=std::min(capacity-1,std::strlen(message));std::memcpy(error,message,n);error[n]=0;}
        return AAL_INVALID_OR_FAILED;
    }
}
}
extern "C" aal_status aal_encode_cd16_stream(uint64_t frames,const char* profile,
    aal_pcm_read input,void* input_context,aal_byte_write output,void* output_context,
    aal_stream_result* result,aal_progress callback,void* context,char* error,size_t capacity) {
    return stream_call(result,callback,context,error,capacity,[&](const independent_aal::Progress& progress) {
        if(!input||!output||!profile)throw std::runtime_error("Missing encode stream callback or profile.");
        independent_aal::EncodeOptions options;options.profile_id=profile;
        return independent_aal::encode_stream([&](int16_t* p,size_t n){return input(p,n,input_context);},frames,
            [&](const uint8_t* p,size_t n){if(output(p,n,output_context))throw std::runtime_error("AAL write callback failed.");},
            progress,nullptr,options);
    });
}
extern "C" aal_status aal_decode_cd16_stream(uint64_t bytes,
    aal_byte_read input,void* input_context,aal_pcm_write output,void* output_context,
    aal_stream_result* result,aal_progress callback,void* context,char* error,size_t capacity) {
    return stream_call(result,callback,context,error,capacity,[&](const independent_aal::Progress& progress) {
        if(!input||!output)throw std::runtime_error("Missing decode stream callback.");
        return independent_aal::decode_stream([&](uint8_t* p,size_t n){return input(p,n,input_context);},bytes,
            [&](const int16_t* p,size_t n){if(output(p,n,output_context))throw std::runtime_error("PCM write callback failed.");},
            progress);
    });
}
