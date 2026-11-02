#pragma once
#include "aal_format.hpp"
#include <array>
#include <limits>
#include <filesystem>

namespace independent_aal {
inline uint64_t le64(const uint8_t* p){return uint64_t(le32(p))|(uint64_t(le32(p+4))<<32);}
inline void le64put(uint8_t* p,uint64_t value){le32put(p,uint32_t(value));le32put(p+4,uint32_t(value>>32));}
class FileInput {
    std::ifstream file;
    uint64_t length=0,offset=0;
public:
    explicit FileInput(const std::filesystem::path& path):file(path,std::ios::binary|std::ios::ate) {
        if(!file)throw std::runtime_error("Cannot open input file.");
        const auto end=file.tellg();
        if(end<0)throw std::runtime_error("Cannot determine input size.");
        length=uint64_t(end);seek(0);
    }
    uint64_t size() const{return length;}
    uint64_t position() const{return offset;}
    void seek(uint64_t position) {
        if(position>length||position>uint64_t(std::numeric_limits<std::streamoff>::max()))
            throw std::runtime_error("Invalid 64-bit input offset.");
        file.clear();file.seekg(std::streamoff(position));
        if(!file)throw std::runtime_error("Input seek failed.");
        offset=position;
    }
    size_t read(uint8_t* output,size_t requested) {
        const auto n=size_t(std::min<uint64_t>(length-offset,requested));
        if(!n)return 0;
        if(!file.read(reinterpret_cast<char*>(output),std::streamsize(n)))throw std::runtime_error("Incomplete input read.");
        offset+=n;return n;
    }
    void exact(uint8_t* output,size_t count) {
        if(read(output,count)!=count)throw std::runtime_error("Truncated input.");
    }
};
struct WaveInfo {
    uint64_t data_offset=0,data_bytes=0;
    bool rf64=false;
    uint64_t frames() const{return data_bytes/4;}
};
// Scans chunk headers with fixed buffers and 64-bit seeks. PCM is not loaded.
inline WaveInfo wave_info(FileInput& input) {
    input.seek(0);std::array<uint8_t,12> riff{};input.exact(riff.data(),riff.size());
    const bool rf64=!std::memcmp(riff.data(),"RF64",4);
    if((!rf64&&std::memcmp(riff.data(),"RIFF",4))||std::memcmp(riff.data()+8,"WAVE",4))
        throw std::runtime_error("Input must be RIFF/WAVE or RF64 PCM.");
    uint64_t end=input.size(),data64=0,samples64=0;
    bool have_ds64=false,format=false,data=false;
    if(!rf64) {
        end=uint64_t(le32(riff.data()+4))+8;
        if(end<12||end>input.size())throw std::runtime_error("Invalid RIFF size.");
    }else if(le32(riff.data()+4)!=UINT32_MAX)throw std::runtime_error("Invalid RF64 sentinel.");
    WaveInfo result;result.rf64=rf64;
    uint64_t position=12;
    while(position+8<=end) {
        input.seek(position);std::array<uint8_t,8> chunk{};input.exact(chunk.data(),8);
        uint64_t size=le32(chunk.data()+4);const uint64_t payload=position+8;
        const bool is_data=!std::memcmp(chunk.data(),"data",4);
        if(rf64&&!std::memcmp(chunk.data(),"ds64",4)) {
            if(have_ds64||position!=12||size<28||size>end-payload)throw std::runtime_error("Invalid RF64 ds64 chunk.");
            std::array<uint8_t,28> ds{};input.exact(ds.data(),ds.size());
            const uint64_t riff64=le64(ds.data());
            if(riff64<40||riff64>input.size()-8||28ULL+12ULL*le32(ds.data()+24)>size)
                throw std::runtime_error("Invalid RF64 length table.");
            end=riff64+8;data64=le64(ds.data()+8);samples64=le64(ds.data()+16);have_ds64=true;
        }
        if(rf64&&is_data&&(!have_ds64||size!=UINT32_MAX))
            throw std::runtime_error("RF64 data must use its ds64 length and size sentinel.");
        if(size==UINT32_MAX) {
            if(!rf64||!have_ds64||!is_data)throw std::runtime_error("Unsupported extended WAV metadata chunk size.");
            size=data64;
        }
        if(size>end-payload)throw std::runtime_error("Truncated WAVE chunk.");
        if(!std::memcmp(chunk.data(),"fmt ",4)) {
            std::array<uint8_t,16> fmt{};
            if(format||size<16)throw std::runtime_error("Invalid WAVE format chunk.");
            input.exact(fmt.data(),fmt.size());
            if(le16(fmt.data())!=1||le16(fmt.data()+2)!=2||le32(fmt.data()+4)!=44100||
               le32(fmt.data()+8)!=176400||le16(fmt.data()+12)!=4||le16(fmt.data()+14)!=16)
                throw std::runtime_error("This build requires 44100 Hz / 16-bit / stereo integer PCM.");
            format=true;
        }
        if(is_data) {
            if(data||!size||size%4)throw std::runtime_error("WAVE needs one nonempty stereo PCM data chunk.");
            result.data_offset=payload;result.data_bytes=size;data=true;
        }
        if(size>UINT64_MAX-payload-(size&1))throw std::runtime_error("WAVE chunk offset overflow.");
        position=payload+size+(size&1);
        if(position>end)throw std::runtime_error("Missing WAVE chunk padding.");
    }
    if(position!=end||!format||!data||(rf64&&!have_ds64)||(rf64&&samples64&&samples64!=result.frames()))
        throw std::runtime_error("Incomplete RIFF/RF64 PCM headers.");
    input.seek(result.data_offset);return result;
}
inline Bytes wave_header(uint64_t frames) {
    if(!frames||frames>(UINT64_MAX-80)/4)throw std::runtime_error("Invalid WAV sample length.");
    const uint64_t bytes=frames*4;
    const bool rf64=bytes>UINT32_MAX-36ULL;
    Bytes header(rf64?80:44);
    std::memcpy(header.data(),rf64?"RF64":"RIFF",4);
    le32put(header.data()+4,rf64?UINT32_MAX:uint32_t(bytes+36));std::memcpy(header.data()+8,"WAVE",4);
    size_t fmt=12;
    if(rf64) {
        std::memcpy(header.data()+12,"ds64",4);le32put(header.data()+16,28);
        le64put(header.data()+20,bytes+72);le64put(header.data()+28,bytes);le64put(header.data()+36,frames);
        fmt=48;
    }
    auto* p=header.data()+fmt;std::memcpy(p,"fmt ",4);le32put(p+4,16);
    le16put(p+8,1);le16put(p+10,2);le32put(p+12,44100);le32put(p+16,176400);
    le16put(p+20,4);le16put(p+22,16);std::memcpy(p+24,"data",4);
    le32put(p+28,rf64?UINT32_MAX:uint32_t(bytes));return header;
}
inline void pcm_to_bytes(const int16_t* input,size_t frames,uint8_t* output) {
    for(size_t i=0;i<frames*2;++i)le16put(output+i*2,uint16_t(input[i]));
}
inline void bytes_to_pcm(const uint8_t* input,size_t frames,int16_t* output) {
    for(size_t i=0;i<frames*2;++i)output[i]=int16_t(le16(input+i*2));
}
}
