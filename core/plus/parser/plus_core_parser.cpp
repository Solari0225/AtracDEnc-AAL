// Independent restricted syntax reader. Only public numerical Huffman /
// quantization-unit tables are shared with the encoder. No encoder state,
// snapshot, quantizer call, Sony library or FFmpeg runtime is consulted.
#include "plus_core_parser.h"
#include "atrac/at3p/at3p_tables.h"
#include "atrac/at3p/ff/atrac3plus_data.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Tables=NAtracDEnc::NAt3p::THuffTables;
using Element=NAtracDEnc::NAt3p::TVlcElement;
using Scale=NAtracDEnc::NAt3p::TScaleTable;
thread_local std::string parse_error;
struct Reader {
    const uint8_t* data;
    size_t bytes,position=0;
    uint32_t get(unsigned count) {
        if(count>32||position>bytes*8||count>bytes*8-position)
            throw std::runtime_error("Truncated ATRAC3plus frame");
        uint32_t value=0;
        for(unsigned i=0;i<count;++i,++position)value=(value<<1)|((data[position/8]>>(7-position%8))&1);
        return value;
    }
    void expect(uint32_t value,unsigned count,const char* detail) {
        if(get(count)!=value)throw std::runtime_error(detail);
    }
};
struct Huffman {
    struct Node {int child[2]={-1,-1};int value=-1;};
    std::vector<Node> nodes{1};
    template<size_t N>void build(const std::array<Element,N>& table) {
        for(size_t value=0;value<N;++value) {
            const unsigned length=unsigned(table[value].Len);
            if(!length)continue;
            if(length>16)throw std::runtime_error("Invalid public Huffman length");
            int node=0;
            for(unsigned bits=length;bits;--bits) {
                const unsigned bit=(uint16_t(table[value].Code)>>(bits-1))&1u;
                if(nodes[node].value>=0)throw std::runtime_error("Ambiguous public Huffman prefix");
                if(nodes[node].child[bit]<0) {
                    const int next=int(nodes.size());
                    nodes[node].child[bit]=next;
                    nodes.emplace_back();
                }
                node=nodes[node].child[bit];
            }
            if(nodes[node].value>=0||nodes[node].child[0]>=0||nodes[node].child[1]>=0)
                throw std::runtime_error("Ambiguous public Huffman code");
            nodes[node].value=int(value);
        }
    }
    unsigned read(Reader& bits)const {
        int node=0;
        for(unsigned length=0;length<16;++length) {
            node=nodes[node].child[bits.get(1)];
            if(node<0)throw std::runtime_error("Invalid ATRAC3plus Huffman symbol");
            if(nodes[node].value>=0)return unsigned(nodes[node].value);
        }
        throw std::runtime_error("Excessive ATRAC3plus Huffman length");
    }
};
struct Decoders {
    std::array<Huffman,4> word;
    std::array<Huffman,56> spectral;
    Decoders() {
        // The public constructor fills only the defined word-length symbols.
        // Static zero-initialization leaves their unused slots at Len == 0,
        // matching the encoder's immutable table instance.
        static const Tables source;
        for(unsigned i=0;i<4;++i)word[i].build(source.WordLens[i]);
        for(unsigned i=0;i<56;++i)spectral[i].build(source.VlcSpecs[i]);
    }
};
const Decoders& decoders(){static const Decoders value;return value;}

at3pcore_spectrum parse(const uint8_t* frame,size_t size) {
    Reader reader{frame,size};
    at3pcore_spectrum output{};
    reader.expect(0,1,"Unsupported ATRAC3plus start bit");
    reader.expect(1,2,"Restricted ATRAC3plus reader requires one stereo block");
    const unsigned units=reader.get(5)+1;
    reader.expect(0,1,"Muted blocks are outside the emitted syntax");
    output.num_quant_units=units;
    output.num_subbands=unsigned(atrac3p_qu_to_subband[units-1])+1;
    const auto& huffman=decoders();
    reader.expect(3,2,"Unsupported first-channel word-length mode");
    reader.expect(0,2,"Unsupported word-length weighting");
    reader.expect(0,2,"Unsupported word-length value count");
    const unsigned first_table=reader.get(2);
    output.word_length[0][0]=uint8_t(reader.get(3));
    for(unsigned qu=1;qu<units;++qu)
        output.word_length[0][qu]=uint8_t((output.word_length[0][qu-1]+huffman.word[first_table].read(reader))&7u);
    reader.expect(1,2,"Unsupported second-channel word-length mode");
    reader.expect(0,2,"Unsupported second-channel word-length count");
    const unsigned second_table=reader.get(2);
    for(unsigned qu=0;qu<units;++qu)
        output.word_length[1][qu]=uint8_t((output.word_length[0][qu]+huffman.word[second_table].read(reader))&7u);
    for(unsigned ch=0;ch<2;++ch) {
        reader.expect(0,2,"Unsupported scale-factor encoding mode");
        for(unsigned qu=0;qu<units;++qu) {
            if(!output.word_length[ch][qu])throw std::runtime_error("Zero-word units are outside the emitted syntax");
            output.scale_factor[ch][qu]=uint8_t(reader.get(6));
        }
    }
    reader.expect(1,1,"Unsupported reduced spectral code table");
    for(unsigned ch=0;ch<2;++ch) {
        reader.expect(0,1,"Unsupported spectral table type");
        reader.expect(0,2,"Unsupported spectral table index encoding");
        reader.expect(0,1,"Unsupported spectral table index count");
        for(unsigned qu=0;qu<units;++qu)output.code_table[ch][qu]=uint8_t(reader.get(3));
    }
    for(unsigned ch=0;ch<2;++ch) {
        for(unsigned qu=0;qu<units;++qu) {
            const unsigned table=7*output.code_table[ch][qu]+output.word_length[ch][qu]-1;
            const auto& description=atrac3p_spectra_tabs[table];
            const unsigned group=description.group_size,packed=description.num_coeffs,bits=description.bits;
            const bool signed_values=description.is_signed;
            const unsigned first=Scale::BlockSizeTab[qu],last=Scale::BlockSizeTab[qu+1];
            if(!group||!packed||packed>4||!bits||bits>8||bits*packed>8)
                throw std::runtime_error("Invalid public spectral table description");
            for(unsigned position=first;position<last;) {
                if(group!=1)reader.expect(1,1,"Skipped spectral groups are outside the emitted syntax");
                for(unsigned j=0;j<group;++j) {
                    const unsigned symbol=huffman.spectral[table].read(reader);
                    int16_t values[4]={};
                    for(unsigned k=0;k<packed;++k) {
                        const unsigned value=(symbol>>(bits*k))&((1u<<bits)-1u);
                        values[k]=int16_t(signed_values&&(value&(1u<<(bits-1)))?int(value)-int(1u<<bits):int(value));
                    }
                    if(!signed_values)for(unsigned k=0;k<packed;++k)
                        if(values[k]&&reader.get(1))values[k]=int16_t(-values[k]);
                    for(unsigned k=0;k<packed;++k) {
                        if(position>=last)throw std::runtime_error("Spectral codeword exceeds quantization unit");
                        output.quantized[ch][position++]=values[k];
                    }
                }
            }
        }
        const unsigned power_groups=atrac3p_subband_to_num_powgrps[output.num_subbands-1];
        for(unsigned group=0;group<power_groups;++group)
            reader.expect(15,4,"Power compensation is outside the emitted syntax");
    }
    reader.expect(0,1,"Channel swapping is outside the emitted syntax");
    reader.expect(0,1,"Coefficient negation is outside the emitted syntax");
    for(unsigned ch=0;ch<2;++ch)
        reader.expect(0,1,"Steep windows are outside the emitted syntax");
    for(unsigned ch=0;ch<2;++ch)
        reader.expect(0,1,"Gain control is outside the emitted syntax");
    reader.expect(0,1,"Tonal components are outside the emitted syntax");
    reader.expect(0,1,"Noise side information is outside the emitted syntax");
    reader.expect(3,2,"Missing ATRAC3plus block terminator");
    while(reader.position<size*8) {
        const unsigned count=unsigned(std::min<size_t>(32,size*8-reader.position));
        reader.expect(0,count,"Nonzero ATRAC3plus frame padding");
    }
    return output;
}
}

extern "C" int at3pcore_parse_spectrum(const uint8_t* frame,size_t frame_bytes,at3pcore_spectrum* output) {
    try {
        if(!frame||!output||frame_bytes<8||frame_bytes>8192)
            throw std::invalid_argument("Invalid ATRAC3plus frame buffer");
        *output=parse(frame,frame_bytes);
        parse_error.clear();return 1;
    }catch(const std::exception& error){parse_error=error.what();return 0;}
}
extern "C" const char* at3pcore_parse_last_error(){return parse_error.c_str();}
