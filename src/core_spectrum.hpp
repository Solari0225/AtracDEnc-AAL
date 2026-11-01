#pragma once
// Independent syntax reader for this encoder's gain-free, tonal-free ATRAC3
// sound units. The Huffman/quantizer tables come from LGPL atracdenc.
#include "atrac/at3/atrac3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace independent_aal {
using At3Data = NAtracDEnc::NAtrac3::TAtrac3Data;
struct QuantizedSpectrum {
    std::array<int16_t,1024> mantissa{};
    std::array<uint8_t,1024> scale_factor{};
    std::array<uint8_t,1024> word_length{};
};
class CoreBits {
    const uint8_t* data;size_t count;size_t position=0;
public:
    CoreBits(const uint8_t* p,size_t bytes):data(p),count(bytes*8){}
    uint32_t get(unsigned n) {
        if(n>32 || n>count-position)throw std::runtime_error("Truncated ATRAC3 sound unit.");
        uint32_t value=0;
        for(unsigned i=0;i<n;++i,++position)value=(value<<1)|((data[position/8]>>(7-position%8))&1);
        return value;
    }
    int32_t signed_value(unsigned n) {
        if(!n || n>31)throw std::runtime_error("Invalid signed code length.");
        uint32_t v=get(n);return v&(uint32_t(1)<<(n-1)) ? int32_t(v)-int32_t(uint32_t(1)<<n) : int32_t(v);
    }
    unsigned huffman(unsigned selector) {
        if(selector<1 || selector>7)throw std::runtime_error("Invalid ATRAC3 codebook.");
        const auto& table=At3Data::HuffTables[selector-1];uint32_t code=0;
        for(unsigned bits=1;bits<=16;++bits) {
            code=(code<<1)|get(1);
            for(unsigned i=0;i<table.Sz;++i)
                if(table.Table[i].Bits==bits && table.Table[i].Code==code)return i;
        }
        throw std::runtime_error("Invalid ATRAC3 Huffman symbol.");
    }
};

inline QuantizedSpectrum read_core_channel(const uint8_t* p,size_t bytes=192) {
    CoreBits bits(p,bytes);QuantizedSpectrum out;
    if(bits.get(6)!=0x28)throw std::runtime_error("Invalid ATRAC3 channel sync.");
    unsigned qmf_bands=bits.get(2)+1;
    for(unsigned b=0;b<qmf_bands;++b)
        if(bits.get(3)!=0)throw std::runtime_error("This deterministic decoder requires a gain-free core.");
    if(bits.get(5)!=0)throw std::runtime_error("This deterministic decoder requires a tonal-free core.");
    unsigned bands=bits.get(5)+1,clc=bits.get(1);
    std::array<unsigned,32> word{},sf{};
    for(unsigned b=0;b<bands;++b)word[b]=bits.get(3);
    for(unsigned b=0;b<bands;++b)if(word[b])sf[b]=bits.get(6);
    constexpr int clc_pair[4]={0,1,-2,-1};
    constexpr int vlc_pair[18]={0,0,0,1,0,-1,1,0,-1,0,1,1,1,-1,-1,1,-1,-1};
    for(unsigned b=0;b<bands;++b) {
        const unsigned first=At3Data::BlockSizeTab[b],last=At3Data::BlockSizeTab[b+1];
        if(first>=last || last>1024)throw std::runtime_error("Invalid ATRAC3 spectral band.");
        for(unsigned i=first;i<last;++i) {out.word_length[i]=uint8_t(word[b]);out.scale_factor[i]=uint8_t(sf[b]);}
        if(!word[b])continue;
        if(word[b]==1) {
            if((last-first)%2)throw std::runtime_error("Invalid pair-coded ATRAC3 band.");
            for(unsigned i=first;i<last;i+=2) {
                const auto code=clc?bits.get(4):bits.huffman(1);
                if(!clc && code>=9)throw std::runtime_error("Invalid paired Huffman symbol.");
                out.mantissa[i]=int16_t(clc?clc_pair[code>>2]:vlc_pair[2*code]);
                out.mantissa[i+1]=int16_t(clc?clc_pair[code&3]:vlc_pair[2*code+1]);
            }
        } else for(unsigned i=first;i<last;++i) {
            if(clc)out.mantissa[i]=int16_t(bits.signed_value(At3Data::ClcLengthTab[word[b]]));
            else {const auto symbol=bits.huffman(word[b])+1;out.mantissa[i]=int16_t((symbol&1)?-int(symbol>>1):int(symbol>>1));}
        }
    }
    return out;
}
}
