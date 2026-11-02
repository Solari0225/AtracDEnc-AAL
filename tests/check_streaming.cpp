#include "../include/independent_aal.h"
#include "../src/stream_codec.hpp"
#include "../src/stream_io.hpp"
#include "../src/sha256.hpp"
#include "../src/decoder_api.hpp"
#include "streaming_vectors.hpp"
#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
using namespace independent_aal;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
extern "C" int check_stream_c_abi(void);
struct Io {
    std::vector<int16_t> pcm,decoded;
    Bytes oma;
    size_t read_frames=0,read_bytes=0,max_read_frames=0,max_write_bytes=0,max_pcm_frames=0;
};
static size_t read_pcm(int16_t* output,size_t capacity,void* context) {
    auto& io=*static_cast<Io*>(context);io.max_read_frames=std::max(io.max_read_frames,capacity);
    const auto n=std::min({size_t(73),capacity,io.pcm.size()/2-io.read_frames});
    if(n)std::memcpy(output,io.pcm.data()+io.read_frames*2,n*4);io.read_frames+=n;return n;
}
static int write_bytes(const uint8_t* data,size_t bytes,void* context) {
    auto& io=*static_cast<Io*>(context);io.max_write_bytes=std::max(io.max_write_bytes,bytes);
    io.oma.insert(io.oma.end(),data,data+bytes);return 0;
}
static size_t read_bytes(uint8_t* output,size_t capacity,void* context) {
    auto& io=*static_cast<Io*>(context);
    const auto n=std::min({size_t(17),capacity,io.oma.size()-io.read_bytes});
    if(n)std::memcpy(output,io.oma.data()+io.read_bytes,n);io.read_bytes+=n;return n;
}
static int write_pcm(const int16_t* pcm,size_t frames,void* context) {
    auto& io=*static_cast<Io*>(context);io.max_pcm_frames=std::max(io.max_pcm_frames,frames);
    io.decoded.insert(io.decoded.end(),pcm,pcm+frames*2);return 0;
}
static int cancel(int percent,const char*,void*){return percent>=50;}
int main() {
    std::filesystem::path directory;bool owned=false;
    try {
        Sha256 digest;digest.update("a",1);digest.update(nullptr,0);digest.update("bc",2);
        require(digest.hex_digest()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","Incremental SHA-256 known answer");
        require(digest.hex_digest()==digest.hex_digest(),"Digest finalization is non-mutating");
        char error[256];unsigned checked=0;
        for(const auto& vector:streaming_vectors) {
            Io io;io.pcm.resize(size_t(vector.frames)*2);uint32_t seed=0x62b9a137;
            for(unsigned i=0;i<vector.frames;++i) {
                seed=seed*1664525u+1013904223u;int left,right;
                if(i<513){left=int(seed>>16)-32768;right=-left-1;}
                else if(i<1024){left=(i&1)?32767:-32768;right=(i&2)?-32768:32767;}
                else {left=int((uint64_t(i)*317)%65536)-32768;right=int((uint64_t(i)*719+9001)%65536)-32768;}
                io.pcm[i*2]=int16_t(left);io.pcm[i*2+1]=int16_t(right);
            }
            aal_stream_result encoded{},decoded{};
            require(aal_encode_cd16_stream(vector.frames,vector.profile,read_pcm,&io,write_bytes,&io,&encoded,nullptr,nullptr,error,sizeof error)==AAL_OK,error);
            require(io.oma.size()==vector.bytes&&sha256(io.oma.data(),io.oma.size())==vector.oma,"Whole AAL bytes differ from frozen 0.4.0 baseline");
            require(encoded.sample_frames==vector.frames&&encoded.output_bytes==io.oma.size()&&std::string(encoded.pcm_sha256)==vector.pcm,"Encode stream result");
            require(aal_decode_cd16_stream(io.oma.size(),read_bytes,&io,write_pcm,&io,&decoded,nullptr,nullptr,error,sizeof error)==AAL_OK,error);
            require(io.decoded==io.pcm&&decoded.sample_frames==vector.frames&&decoded.output_bytes==uint64_t(vector.frames)*4&&
                    std::string(decoded.pcm_sha256)==vector.pcm,"Fragmented streaming decode PCM");
            require(io.max_read_frames<=2048&&io.max_write_bytes<=10272&&io.max_pcm_frames<=2048,"Unbounded callback request");
            ++checked;
        }
        require(check_stream_c_abi()==0,"Streaming C ABI integration");
        Io invalid;invalid.pcm.assign(65536,123);aal_stream_result result{};result.sample_frames=99;
        require(aal_encode_cd16_stream(32768,"aalp-352",read_pcm,&invalid,write_bytes,&invalid,&result,cancel,nullptr,error,sizeof error)==AAL_CANCELLED&&
                result.sample_frames==99,"Mid-stream cancellation/result ownership");
        const auto eof=[](int16_t*,size_t,void*)->size_t{return 0;};
        const auto sink=[](const uint8_t*,size_t,void*)->int{return 0;};
        require(aal_encode_cd16_stream((1ULL<<28)+1,"aalp-352",eof,nullptr,sink,nullptr,&result,nullptr,nullptr,error,sizeof error)==AAL_INVALID_OR_FAILED&&
                std::string(error).find("read callback")!=std::string::npos,"Length above former 1 GiB threshold must reach bounded input callback");
        require(aal_encode_cd16_stream(UINT64_MAX,"aalp-352",eof,nullptr,sink,nullptr,&result,nullptr,nullptr,error,sizeof error)==AAL_INVALID_OR_FAILED,
                "Container counter overflow");
        const auto oversized=[](int16_t*,size_t n,void*)->size_t{return n+1;};
        require(aal_encode_cd16_stream(1,"aalp-352",oversized,nullptr,sink,nullptr,&result,nullptr,nullptr,error,sizeof error)==AAL_INVALID_OR_FAILED,
                "Invalid read callback count");
        Io failure;failure.pcm={123,-456};
        const auto broken=[](const uint8_t*,size_t,void*)->int{return 1;};
        require(aal_encode_cd16_stream(1,"aalp-352",read_pcm,&failure,broken,nullptr,&result,nullptr,nullptr,error,sizeof error)==AAL_INVALID_OR_FAILED,
                "Output callback failure");
        failure.read_frames=0;
        require(aal_encode_cd16_stream(1,"aalp-352",read_pcm,&failure,write_bytes,&failure,&result,nullptr,nullptr,error,sizeof error)==AAL_OK,error);
        result.sample_frames=99;
        const auto broken_pcm=[](const int16_t*,size_t,void*)->int{return 1;};
        require(aal_decode_cd16_stream(failure.oma.size(),read_bytes,&failure,broken_pcm,nullptr,&result,nullptr,nullptr,error,sizeof error)==AAL_INVALID_OR_FAILED&&
                result.sample_frames==99,"Decode I/O failure leaves result unchanged");
        const uint64_t large_frames=(1ULL<<30)+17;
        auto header=wave_header(large_frames);
        require(header.size()==80&&!std::memcmp(header.data(),"RF64",4)&&le64(header.data()+28)==large_frames*4&&
                le64(header.data()+36)==large_frames&&le32(header.data()+76)==UINT32_MAX,"64-bit RF64 output header");
        directory=std::filesystem::temp_directory_path()/("atracdenc-aal-stream-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        owned=std::filesystem::create_directory(directory);require(owned,"Own test directory");
        // Exercise RF64 input with a real small file; >4 GiB header arithmetic
        // above is separate from the opt-in complete large-file acceptance run.
        le64put(header.data()+20,76);le64put(header.data()+28,4);le64put(header.data()+36,1);
        const auto path=directory/"small-rf64.wav";
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(header.data()),header.size());const char pcm[4]={1,0,2,0};file.write(pcm,4);}
        {FileInput input(path);const auto info=wave_info(input);require(info.rf64&&info.frames()==1&&info.data_offset==80,"RF64 file preflight");}
        const auto encoded_path=directory/"rf64.oma",decoded_path=directory/"decoded.wav";
        const auto encoded_file=encode_file(path,encoded_path);
        const auto decoded_file=decode_file(encoded_path,decoded_path);
        const uint8_t tiny_pcm[4]={1,0,2,0};
        require(encoded_file.pcm_sha256==sha256(tiny_pcm,4)&&decoded_file.pcm_sha256==encoded_file.pcm_sha256,"Complete RF64 encode/decode PCM");
        // Mid-file cancellation must remove only our partial output, including
        // its temporary name; the same directory's completed files survive.
        const auto cancel_path=directory/"cancel-source.wav",cancel_oma=directory/"cancel.oma",cancel_wav=directory/"cancel.wav";
        {std::ofstream file(cancel_path,std::ios::binary);const auto h=wave_header(65536);file.write(reinterpret_cast<const char*>(h.data()),h.size());
         const std::array<char,4096> zeros{};for(unsigned i=0;i<64;++i)file.write(zeros.data(),zeros.size());}
        std::atomic_bool stopped{false};bool cancelled_file=false;
        const auto stop=[&](int percent,const std::wstring&){if(percent>=50)stopped=true;};
        try{encode_file(cancel_path,cancel_oma,stop,&stopped);}catch(const std::exception& e){cancelled_file=std::string(e.what())=="Cancelled";}
        require(cancelled_file&&!std::filesystem::exists(cancel_oma)&&!std::filesystem::exists(directory/"cancel.oma.json"),"Cancelled encode publishes no file/report");
        stopped=false;encode_file(cancel_path,cancel_oma);cancelled_file=false;
        try{decode_file(cancel_oma,cancel_wav,stop,&stopped);}catch(const std::exception& e){cancelled_file=std::string(e.what())=="Cancelled";}
        require(cancelled_file&&!std::filesystem::exists(cancel_wav),"Cancelled decode publishes no file");
        for(const auto& entry:std::filesystem::directory_iterator(directory))
            require(entry.path().filename().string().find(".tmp-")==std::string::npos,"Partial streaming temporary file leaked");
        for(const auto& file:{cancel_path,cancel_oma,directory/"cancel.oma.json"})std::filesystem::remove(file);
        {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);const std::array<uint8_t,4> wrong={4,0,0,0};file.seekp(76);file.write(reinterpret_cast<const char*>(wrong.data()),4);}
        bool rejected=false;try{FileInput bad(path);wave_info(bad);}catch(...){rejected=true;}
        require(rejected,"RF64 cannot bypass ds64 with an ordinary data length");
        {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);const std::array<uint8_t,4> sentinel={255,255,255,255};file.seekp(76);file.write(reinterpret_cast<const char*>(sentinel.data()),4);}
        // A lying 64-bit size must be rejected without allocating its extent.
        {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);std::array<uint8_t,8> n{};le64put(n.data(),large_frames*4);file.seekp(28);file.write(reinterpret_cast<const char*>(n.data()),8);}
        rejected=false;try{FileInput bad(path);wave_info(bad);}catch(...){rejected=true;}
        require(rejected,"Truncated RF64 rejected");
        for(const auto& file:{path,encoded_path,decoded_path,directory/"rf64.oma.json"})std::filesystem::remove(file);
        std::filesystem::remove(directory);owned=false;
        std::cout<<"PASS: "<<checked<<" frozen 0.4.0 whole-file hashes; fragmented streaming C/C++ I/O, exact PCM, bounded callback sizes, cancellation, I/O errors, >1 GiB length admission, RF64 and 64-bit overflow\n";
        return 0;
    }catch(const std::exception& error) {
        if(owned){std::error_code ec;for(const char* file:{"small-rf64.wav","rf64.oma","rf64.oma.json","decoded.wav","cancel-source.wav","cancel.oma","cancel.oma.json","cancel.wav"})std::filesystem::remove(directory/file,ec);std::filesystem::remove(directory,ec);}
        std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}
