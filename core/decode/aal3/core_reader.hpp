#pragma once
// Original132 independent-stereo syntax, using the independently observed
// numerical codewords retained by the new original encoder research. No
// AtracDEnc headers, tables or runtime are used by this parser.
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace independent_aal_decoder {
namespace tables {
struct codeword {uint32_t word;union{uint32_t length;uint32_t bits;};};
struct codebook {uint32_t count;const codeword *words;};
#include "../tables/atrac3_spectrum.inc"
#include "../tables/atrac3_tones.inc"
#include "../../../src/dequant_tables.inc"
}
inline int32_t signed_word(uint32_t v){int32_t r;std::memcpy(&r,&v,4);return r;}
inline int32_t arithmetic_right(int32_t value,unsigned shift){
    shift&=31;if(!shift)return value;uint32_t bits=uint32_t(value);
    return signed_word((bits>>shift)|((bits&0x80000000u)?(~0u<<(32-shift)):0));
}
inline int32_t dequantize(int value,unsigned sf,unsigned word){
    if(!word)return 0;
    if(word>7||sf>63)throw std::runtime_error("Invalid original inverse quantization parameters");
    unsigned magnitude=unsigned(value<0?-value:value);
    if(magnitude>=tables::kMagnitudeCount[word-1])throw std::runtime_error("Out-of-domain original mantissa");
    int32_t coefficient=tables::kMagnitude[sf%3][word-1][magnitude];
    if(value<0)coefficient=-coefficient;
    return arithmetic_right(coefficient,30-sf/3);
}
struct Reader {
    const uint8_t *data;unsigned bit=0,capacity;
    Reader(const uint8_t *p,unsigned bytes):data(p),capacity(bytes*8){}
    unsigned get(unsigned width){
        if(width>32||bit+width>capacity)throw std::runtime_error("Truncated original ATRAC3 unit");
        unsigned v=0;for(unsigned i=0;i<width;++i,++bit)v=(v<<1)|((data[bit/8]>>(7-bit%8))&1);return v;
    }
    int signed_value(unsigned width){unsigned v=get(width);return v&(1u<<(width-1))?int(v)-int(1u<<width):int(v);}
    unsigned symbol(const tables::codeword *words,unsigned count,unsigned alignment,unsigned word,bool paired=false){
        unsigned value=0;
        for(unsigned width=1;width<=16;++width){
            value=2*value+get(1);
            for(unsigned index=0;index<count;++index){
                if(paired){if((index/4==2)||(index%4==2))continue;}
                else{
                    unsigned half=count/2;int signed_index=index<half?int(index):int(index)-int(count);
                    if(unsigned(signed_index<0?-signed_index:signed_index)>=tables::kMagnitudeCount[word-1])continue;
                }
                if(words[index].length==width&&(words[index].word>>(alignment-width))==value)return index;
            }
        }
        throw std::runtime_error("Invalid original ATRAC3 codeword");
    }
};
struct Gain {
    unsigned count=0;
    std::array<int16_t,16> points={255,4};
};
struct Tone {
    unsigned position,sf,word,count;
    std::array<int,8> values{};
};
struct Channel {
    unsigned subbands=0,quantization_units=0,tonal_mode=0;
    std::array<Gain,4> gains{};
    std::array<int32_t,1024> frequency{};
    std::vector<Tone> tones;
    unsigned end_bit=0;
};
inline Channel read_channel(const uint8_t *data,unsigned bytes=192){
    Reader r(data,bytes);Channel result;
    if(r.get(6)!=0x28)throw std::runtime_error("Expected independent-stereo original ATRAC3 sync");
    result.subbands=r.get(2)+1;
    for(unsigned band=0;band<result.subbands;++band){
        Gain&g=result.gains[band];g.count=r.get(3);
        unsigned previous=0;
        for(unsigned j=0;j<g.count;++j){
            unsigned level=r.get(4),location=r.get(5)*8;
            if(j&&location<=previous)throw std::runtime_error("Unordered original gain points");
            g.points[2*j]=int16_t(location);g.points[2*j+1]=int16_t(level);previous=location;
        }
        g.points[2*g.count]=255;g.points[2*g.count+1]=4;
    }
    const unsigned groups=r.get(5);
    result.tonal_mode=groups?r.get(2):0;
    if(result.tonal_mode>1)throw std::runtime_error("Unsupported original tonal mode");
    for(unsigned group=0;group<groups;++group){
        std::array<unsigned,4> flags{};for(unsigned b=0;b<result.subbands;++b)flags[b]=r.get(1);
        unsigned count=r.get(3)+1,word=r.get(3);
        if(word<2||word>7)throw std::runtime_error("Invalid tonal word length");
        const auto&book=tables::tonal_books[2*word+result.tonal_mode];
        if(!book.words)throw std::runtime_error("Absent original tonal codebook");
        for(unsigned quarter=0;quarter<result.subbands*4;++quarter){
            if(!flags[quarter/4])continue;
            const unsigned components=r.get(3);
            for(unsigned j=0;j<components;++j){
                Tone t{};t.sf=r.get(6);t.position=quarter*64+r.get(6);t.word=word;t.count=count;
                if(t.position+count>1024)throw std::runtime_error("Tonal span outside original spectrum");
                for(unsigned k=0;k<count;++k){
                    unsigned symbol=r.symbol(book.words,book.count,24,word);
                    t.values[k]=symbol<book.count/2?int(symbol):int(symbol)-int(book.count);
                    result.frequency[t.position+k]=signed_word(uint32_t(result.frequency[t.position+k])+uint32_t(dequantize(t.values[k],t.sf,word)));
                }
                result.tones.push_back(t);
            }
        }
    }
    result.quantization_units=r.get(5)+1;unsigned clc=r.get(1);
    std::array<unsigned,32> wl{},sf{};
    for(unsigned q=0;q<result.quantization_units;++q)wl[q]=r.get(3);
    for(unsigned q=0;q<result.quantization_units;++q)if(wl[q])sf[q]=r.get(6);
    constexpr unsigned widths[8]={0,0,3,3,4,4,5,6};
    for(unsigned q=0;q<result.quantization_units;++q){
        const unsigned word=wl[q];if(!word)continue;
        for(unsigned i=tables::spectrum_starts[q];i<tables::spectrum_starts[q+1];){
            if(word==1){
                unsigned symbols=clc?r.get(4):r.symbol(tables::spectrum_books[1],16,32,word,true);
                for(unsigned k=0;k<2;++k){unsigned code=k?symbols%4:symbols/4;int value=code<2?int(code):int(code)-4;result.frequency[i]=signed_word(uint32_t(result.frequency[i])+uint32_t(dequantize(value,sf[q],word)));++i;}
            }else{
                int value;
                if(clc)value=r.signed_value(widths[word]);
                else{unsigned count=1u<<widths[word],symbol=r.symbol(tables::spectrum_books[word],count,24,word);value=symbol<count/2?int(symbol):int(symbol)-int(count);}
                result.frequency[i]=signed_word(uint32_t(result.frequency[i])+uint32_t(dequantize(value,sf[q],word)));++i;
            }
        }
    }
    result.end_bit=r.bit;return result;
}
}
