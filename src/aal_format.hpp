#pragma once
// Independently written OMA/AAL framing. No Sony code or runtime calls.
// Verified field layout is recorded in ../inventory and ../bitstream.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "codec_profiles.hpp"

namespace independent_aal {
using Bytes = std::vector<uint8_t>;
constexpr size_t kFrameSamples = 1024;
constexpr size_t kStereoSamples = 2048;
constexpr size_t kCoreBytes = 384;
constexpr size_t kRawEnhancementBytes = 4104;
constexpr size_t kDelay = 138;

inline uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
inline uint32_t le32(const uint8_t* p) { return uint32_t(le16(p)) | uint32_t(le16(p+2)) << 16; }
inline uint16_t be16(const uint8_t* p) { return uint16_t(p[1]) | uint16_t(p[0]) << 8; }
inline uint32_t be32(const uint8_t* p) { return uint32_t(be16(p+2)) | uint32_t(be16(p)) << 16; }
inline void be16put(uint8_t* p, uint16_t n) { p[0] = uint8_t(n >> 8); p[1] = uint8_t(n); }
inline void be32put(uint8_t* p, uint32_t n) { be16put(p,uint16_t(n >> 16)); be16put(p+2,uint16_t(n)); }
inline void le16put(uint8_t* p, uint16_t n) { p[1] = uint8_t(n >> 8); p[0] = uint8_t(n); }
inline void le32put(uint8_t* p, uint32_t n) { le16put(p,uint16_t(n)); le16put(p+2,uint16_t(n >> 16)); }

struct Wave {
    std::vector<int16_t> pcm;
    uint32_t frames() const { return uint32_t(pcm.size()/2); }
};

inline Wave read_wave(const Bytes& data) {
    if (data.size()<44 || std::memcmp(data.data(),"RIFF",4) || std::memcmp(data.data()+8,"WAVE",4))
        throw std::runtime_error("Input must be RIFF/WAVE PCM.");
    bool format_ok=false;
    size_t data_pos=0, data_size=0;
    for(size_t p=12;p+8<=data.size();) {
        uint32_t n=le32(data.data()+p+4);
        if(n>data.size()-p-8) throw std::runtime_error("Truncated WAVE chunk.");
        const uint8_t* q=data.data()+p+8;
        if(!std::memcmp(data.data()+p,"fmt ",4)) {
            format_ok=n>=16 && le16(q)==1 && le16(q+2)==2 && le32(q+4)==44100 && le32(q+8)==176400 && le16(q+12)==4 && le16(q+14)==16;
        }
        if(!std::memcmp(data.data()+p,"data",4)) { data_pos=p+8; data_size=n; }
        p+=8+size_t(n)+(n&1);
    }
    if(!format_ok) throw std::runtime_error("This build requires 44100 Hz / 16-bit / stereo integer PCM.");
    if(!data_pos || !data_size || data_size%4) throw std::runtime_error("WAVE has no complete stereo PCM frames.");
    Wave w; w.pcm.resize(data_size/2);
    for(size_t i=0;i<w.pcm.size();++i) w.pcm[i]=int16_t(le16(data.data()+data_pos+i*2));
    return w;
}

inline Bytes write_wave(const Wave& w) {
    if(w.pcm.empty() || w.pcm.size()%2 || w.pcm.size()>(uint64_t(UINT32_MAX)-36)/2)
        throw std::runtime_error("Invalid or oversized PCM.");
    Bytes b(44+w.pcm.size()*2);
    std::memcpy(b.data(),"RIFF",4);le32put(b.data()+4,uint32_t(b.size()-8));
    std::memcpy(b.data()+8,"WAVEfmt ",8);le32put(b.data()+16,16);le16put(b.data()+20,1);le16put(b.data()+22,2);
    le32put(b.data()+24,44100);le32put(b.data()+28,176400);le16put(b.data()+32,4);le16put(b.data()+34,16);
    std::memcpy(b.data()+36,"data",4);le32put(b.data()+40,uint32_t(w.pcm.size()*2));
    for(size_t i=0;i<w.pcm.size();++i) le16put(b.data()+44+i*2,uint16_t(w.pcm[i]));
    return b;
}

inline uint32_t block_count(uint32_t frames) { return uint32_t((uint64_t(frames)+kDelay+kFrameSamples-1)/kFrameSamples+1); }
inline uint16_t final_samples(uint32_t frames) { return uint16_t(frames%kFrameSamples ? frames%kFrameSamples : kFrameSamples); }

inline Bytes aal3_header(uint32_t frames, uint32_t blocks) {
    if(!frames || blocks<2) throw std::runtime_error("AAL requires nonempty PCM and a priming packet.");
    Bytes h(96);
    std::memcpy(h.data(),"EA3",3);h[3]=3;h[5]=96;h[6]=0xff;h[7]=0x80;
    h[32]=0x22;h[33]=0x52;be16put(h.data()+34,final_samples(frames));
    // Record real duration here. Exact trailing sample count also lives in BLK.
    be32put(h.data()+36,uint32_t((uint64_t(frames)*1000)/44100));
    be32put(h.data()+40,blocks);h[50]=0x20;h[51]=0x30;
    return h;
}

inline Bytes aal_header(uint32_t frames,uint32_t blocks,const EncodingProfile& profile) {
    if(!frames||blocks!=profile_block_count(frames,profile))throw std::runtime_error("Invalid AAL sample and packet counts.");
    Bytes h(96);std::memcpy(h.data(),"EA3",3);h[3]=3;h[5]=96;h[6]=0xff;h[7]=0x80;
    h[32]=profile.oma_codec_id;h[33]=profile.oma_frame_parameters;be16put(h.data()+34,profile_tail(frames,profile));
    h[48]=profile.codec==CoreCodec::Atrac3plus?1:0;
    // Preserve the official duration convention, including its flush-packet
    // rounding. Actual source length is verified from the decoded PCM too.
    const uint64_t declared=uint64_t(blocks-2)*profile.frame_samples+profile_tail(frames,profile);
    be32put(h.data()+36,uint32_t(declared*1000/44100));be32put(h.data()+40,blocks);
    h[49]=uint8_t(profile.core_parameters>>16);be16put(h.data()+50,uint16_t(profile.core_parameters));return h;
}

inline Bytes aal_block(const uint8_t* core,uint32_t ordinal,const Bytes& enhancement,bool last,uint16_t tail,
                       uint8_t padding,const EncodingProfile& profile) {
    const size_t raw_limit=size_t(profile.frame_samples)*4+8;
    if(!core||enhancement.size()>raw_limit||enhancement.size()%8||padding>7||tail==0||tail>profile.frame_samples||
       (!enhancement.empty()&&padding>=enhancement.size())||(!ordinal&&!enhancement.empty()))throw std::runtime_error("Invalid AAL packet.");
    Bytes b(24+profile.core_bytes+enhancement.size());std::memcpy(b.data(),"BLK",3);
    b[3]=enhancement.empty()?0:padding;be16put(b.data()+4,uint16_t(profile.core_bytes+enhancement.size()));be32put(b.data()+8,ordinal);
    be16put(b.data()+12,uint16_t(last?(0x8000|tail):profile.frame_samples));
    std::memcpy(b.data()+24,core,profile.core_bytes);
    if(!enhancement.empty())std::memcpy(b.data()+24+profile.core_bytes,enhancement.data(),enhancement.size());
    return b;
}

class BitWriter {
    Bytes& data;size_t position=0;
public:
    explicit BitWriter(Bytes& b):data(b){}
    void put(uint32_t value,unsigned bits) {
        if(position+bits>data.size()*8) throw std::runtime_error("Bit writer overflow.");
        for(unsigned j=bits;j;--j,++position)
            if((value>>(j-1))&1) data[position/8]|=uint8_t(1u<<(7-position%8));
    }
};

class BitReader {
    const Bytes& data;size_t position=0;
public:
    explicit BitReader(const Bytes& b):data(b){}
    uint32_t get(unsigned bits) {
        if(bits>32 || position+bits>data.size()*8) throw std::runtime_error("Truncated bitstream.");
        uint32_t value=0;
        for(unsigned j=0;j<bits;++j,++position) value=(value<<1)|((data[position/8]>>(7-position%8))&1);
        return value;
    }
};

inline Bytes raw_enhancement(const std::vector<int16_t>& residual,uint32_t ordinal) {
    if(ordinal==0 || residual.size()%2) throw std::runtime_error("Invalid enhancement input.");
    Bytes b(kRawEnhancementBytes);BitWriter writer(b);writer.put(9,5);
    for(size_t ch=0;ch<2;++ch) for(size_t i=0;i<kFrameSamples;++i) {
        int64_t n=(int64_t(ordinal)-1)*int64_t(kFrameSamples)+int64_t(i)-int64_t(kDelay);
        uint16_t value=n>=0 && uint64_t(n)<residual.size()/2 ? uint16_t(residual[size_t(n)*2+ch]) : 0;
        writer.put(value,16);
    }
    return b;
}

inline Bytes block(const uint8_t* core,uint32_t ordinal,const Bytes& enhancement,bool last,uint16_t tail,uint8_t padding=7) {
    if(!core || enhancement.size()>kRawEnhancementBytes || enhancement.size()%8 || padding>7 || tail>1024 || tail==0)
        throw std::runtime_error("Invalid AAL3 block.");
    Bytes b(24+kCoreBytes+enhancement.size());std::memcpy(b.data(),"BLK",3);
    b[3]=enhancement.empty()?0:padding;be16put(b.data()+4,uint16_t(kCoreBytes+enhancement.size()));be32put(b.data()+8,ordinal);
    be16put(b.data()+12,uint16_t(last ? 0x8000|tail : 1024));
    std::memcpy(b.data()+24,core,kCoreBytes);
    if(!enhancement.empty())std::memcpy(b.data()+24+kCoreBytes,enhancement.data(),enhancement.size());
    return b;
}

struct Packet { uint32_t ordinal;bool last;uint16_t tail;uint8_t padding;Bytes core;Bytes enhancement; };
struct ParsedAal { Bytes header; std::vector<Packet> packets;const EncodingProfile* profile=nullptr; };
inline ParsedAal parse_aal(const Bytes& data) {
    size_t offset=0;
    if(data.size()>=10&&(!std::memcmp(data.data(),"ea3",3)||!std::memcmp(data.data(),"ID3",3))) {
        for(size_t i=6;i<10;++i){if(data[i]&128)throw std::runtime_error("Invalid metadata length.");offset=(offset<<7)|data[i];}
        offset+=10+((data[5]&0x10)?10:0);
    }
    if(offset>data.size()||data.size()-offset<96||std::memcmp(data.data()+offset,"EA3",3)||data[offset+5]!=96)throw std::runtime_error("Invalid OMA header.");
    const auto* h=data.data()+offset;
    const uint32_t parameters=(uint32_t(h[49])<<16)|be16(h+50);
    const auto& profile=profile_by_parameters(h[32],parameters);
    if(h[48]!=(profile.codec==CoreCodec::Atrac3plus?1:0))throw std::runtime_error("Invalid AAL core-header version.");
    if((be16(h+33)&0xfff0)!=(uint16_t(profile.oma_frame_parameters)<<8))throw std::runtime_error("OMA frame parameters do not match the core.");
    if(be16(h+6)!=0xff80&&be16(h+6)!=0xffff)throw std::runtime_error("Encrypted OMA is unsupported.");
    ParsedAal result;result.profile=&profile;result.header.assign(h,h+96);offset+=96;
    while(offset<data.size()) {
        if(data.size()-offset<24||std::memcmp(data.data()+offset,"BLK",3))throw std::runtime_error("Invalid BLK header.");
        const auto* p=data.data()+offset;const size_t size=be16(p+4);
        if(size<profile.core_bytes||size>data.size()-offset-24||p[3]>7)throw std::runtime_error("Truncated AAL payload.");
        const uint16_t flags=be16(p+12);Packet packet;
        packet.ordinal=be32(p+8);packet.last=(flags&0x8000)!=0;packet.tail=flags&0xfff;packet.padding=p[3];
        if(packet.ordinal!=result.packets.size()||!packet.tail||packet.tail>profile.frame_samples)throw std::runtime_error("Invalid packet order or sample count.");
        packet.core.assign(p+24,p+24+profile.core_bytes);packet.enhancement.assign(p+24+profile.core_bytes,p+24+size);
        if(packet.padding>packet.enhancement.size())throw std::runtime_error("Invalid enhancement padding.");
        if(!packet.ordinal&&!packet.enhancement.empty())throw std::runtime_error("The priming core must not carry an enhancement.");
        result.packets.push_back(std::move(packet));offset+=24+size;
    }
    if(result.packets.size()<2||be32(h+40)!=result.packets.size()||!result.packets.back().last)throw std::runtime_error("Missing AAL packets or final marker.");
    for(size_t i=0;i+1<result.packets.size();++i)if(result.packets[i].last)throw std::runtime_error("Premature final marker.");
    return result;
}
inline ParsedAal parse_aal3(const Bytes& data) {
    size_t offset=0;
    if(data.size()>=10 && (!std::memcmp(data.data(),"ea3",3)||!std::memcmp(data.data(),"ID3",3))) {
        for(size_t i=6;i<10;++i){if(data[i]&128)throw std::runtime_error("Invalid synchsafe metadata length.");offset=(offset<<7)|data[i];}
        offset+=10+((data[5]&0x10)?10:0);
    }
    if(offset>data.size() || data.size()-offset<96 || std::memcmp(data.data()+offset,"EA3",3) || data[offset+5]!=96)
        throw std::runtime_error("Invalid OMA header.");
    const uint8_t* h=data.data()+offset;
    if(h[32]!=0x22 || (be16(h+33)&0xfff0)!=0x5200 || h[49]!=0 || h[50]!=0x20 || h[51]!=0x30)
        throw std::runtime_error("Expected 44.1 kHz stereo AAL3.");
    if(be16(h+6)!=0xff80 && be16(h+6)!=0xffff) throw std::runtime_error("Encrypted OMA is unsupported.");
    ParsedAal out;out.header.assign(h,h+96);offset+=96;
    while(offset<data.size()) {
        if(data.size()-offset<24 || std::memcmp(data.data()+offset,"BLK",3)) throw std::runtime_error("Invalid BLK header.");
        const uint8_t* p=data.data()+offset;size_t bytes=be16(p+4);
        if(bytes<kCoreBytes || bytes>data.size()-offset-24 || p[3]>7) throw std::runtime_error("Truncated AAL block.");
        Packet q;q.ordinal=be32(p+8);q.padding=p[3];uint16_t flags=be16(p+12);q.last=(flags&0x8000)!=0;q.tail=flags&0xfff;
        if(q.ordinal!=out.packets.size() || q.tail>1024 || q.tail==0) throw std::runtime_error("Invalid block order or tail count.");
        q.core.assign(p+24,p+24+kCoreBytes);q.enhancement.assign(p+24+kCoreBytes,p+24+bytes);
        if(q.padding>q.enhancement.size())throw std::runtime_error("Invalid enhancement padding.");
        out.packets.push_back(std::move(q));offset+=24+bytes;
    }
    if(out.packets.size()<2 || be32(h+40)!=out.packets.size() || !out.packets.back().last)
        throw std::runtime_error("Missing packets or final marker.");
    for(size_t i=0;i+1<out.packets.size();++i) if(out.packets[i].last) throw std::runtime_error("Premature final marker.");
    return out;
}
}
