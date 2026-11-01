#include "../compression/enhancement.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>

namespace {
uint32_t read32(std::ifstream& input) {
    uint8_t b[4];if(!input.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("Truncated fixture");
    return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);
}
int run(const std::filesystem::path& path) {
    try {
        std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot open enhancement-vectors.bin");
        if(read32(input)!=0x564c4141)throw std::runtime_error("Invalid fixture signature");
        const uint32_t version=read32(input);
        if(version!=1&&version!=2)throw std::runtime_error("Invalid fixture version");
        const uint32_t count=read32(input);if(!count||count>1000000)throw std::runtime_error("Unexpected conformance case count");
        uint64_t bytes=0;
        for(uint32_t i=0;i<count;++i) {
            const uint32_t samples=version==1?1024:read32(input);
            if(samples!=1024&&samples!=2048)throw std::runtime_error("Invalid residual sample count");
            const uint32_t size=read32(input),padding=read32(input);
            if(size<8||size>samples*4+8||padding>7)throw std::runtime_error("Invalid expected payload");
            auto read_residual=[&](auto& residual){for(auto& channel:residual)for(auto& x:channel){
                uint8_t b[2];if(!input.read(reinterpret_cast<char*>(b),2))throw std::runtime_error("Truncated residual fixture");
                const uint32_t word=uint32_t(b[0])|(uint32_t(b[1])<<8);x=word<32768?int32_t(word):int32_t(word)-65536;
            }};
            independent_aal::compression::StereoFrame residual;
            independent_aal::compression::PlusStereoFrame plus_residual;
            if(samples==1024)read_residual(residual);else read_residual(plus_residual);
            std::vector<uint8_t> expected(size);
            if(!input.read(reinterpret_cast<char*>(expected.data()),size))throw std::runtime_error("Truncated expected bitstream");
            const auto actual=samples==1024?independent_aal::compression::encode_enhancement(residual):
                independent_aal::compression::encode_plus_enhancement(plus_residual);
            if(actual.padding_bytes!=padding||actual.bytes!=expected)throw std::runtime_error("Official payload mismatch at case "+std::to_string(i+1));
            if(samples==1024){if(independent_aal::compression::decode_enhancement(actual)!=residual)throw std::runtime_error("Residual reconstruction mismatch");}
            else if(independent_aal::compression::decode_plus_enhancement(actual)!=plus_residual)throw std::runtime_error("Plus residual reconstruction mismatch");
            bytes+=size;
        }
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected trailing fixture data");
        std::cout<<"PASS: "<<count<<" official enhancement packets, "<<bytes<<" payload bytes and all padding fields are exact.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {if(argc!=2){std::cerr<<"Usage: aal-conformance enhancement-vectors.bin\n";return 2;}return run(argv[1]);}
#else
int main(int argc,char** argv) {if(argc!=2){std::cerr<<"Usage: aal-conformance enhancement-vectors.bin\n";return 2;}return run(argv[1]);}
#endif
