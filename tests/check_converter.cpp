// Complete-file checks for the logic shared by both native converter UIs.
// No application window is created or controlled.
#include "../src/gui_jobs.hpp"
#include "../src/encoder_api.hpp"
#include "../src/decoder_api.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs=std::filesystem;
namespace ui=atracdenc_aal_gui;
static void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }
static void u16(std::vector<uint8_t>& b,size_t p,uint16_t n){b[p]=uint8_t(n);b[p+1]=uint8_t(n>>8);}
static void u32(std::vector<uint8_t>& b,size_t p,uint32_t n){u16(b,p,uint16_t(n));u16(b,p+2,uint16_t(n>>16));}
static void save(const fs::path& p,const std::vector<uint8_t>& b){std::ofstream f(p,std::ios::binary);f.write((const char*)b.data(),b.size());require(bool(f),"Fixture write");}
static std::vector<uint8_t> load(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::vector<uint8_t> wave(unsigned rate=44100,unsigned bits=16,unsigned channels=2,unsigned format=1) {
    const unsigned samples=2053,block=channels*(bits/8);
    std::vector<uint8_t> b(44+samples*block);
    std::memcpy(b.data(),"RIFF",4);u32(b,4,uint32_t(b.size()-8));std::memcpy(b.data()+8,"WAVEfmt ",8);
    u32(b,16,16);u16(b,20,format);u16(b,22,channels);u32(b,24,rate);u32(b,28,rate*block);
    u16(b,32,block);u16(b,34,bits);std::memcpy(b.data()+36,"data",4);u32(b,40,samples*block);
    uint32_t seed=0x35a17;
    for(size_t i=44;i<b.size();++i){seed=1664525*seed+1013904223;b[i]=uint8_t(seed>>24);}
    return b;
}
int main() {
    fs::path directory;
    bool owned=false;
    try {
        directory=fs::temp_directory_path()/("atracdenc-aal-converter-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        owned=fs::create_directory(directory);require(owned,"Own a new test directory");
        const fs::path input=directory/fs::u8path(u8"音频 test Ω.WAV");
        const auto pcm=wave();save(input,pcm);require(!ui::input_problem(input),"Valid CD WAV accepted");
        unsigned checked=0;
        for(const char* id:{"aal3-132","aalp-128","aalp-352"}) {
            const auto folder=directory/id;fs::create_directory(folder);
            auto oma=ui::vacant_output(folder,input);require(oma.extension()==".oma","WAV routes to OMA");
            independent_aal::EncodeOptions options;options.profile_id=id;
            const auto encoded=independent_aal::encode_file(input,oma,{},nullptr,options);
            require(encoded.verified && fs::exists(encoded.report_path),"Verified encode plus report");
            require(!ui::input_problem(oma),"Produced AAL accepted by shared preflight");
            auto output=ui::vacant_output(folder,oma);require(output.extension()==".wav","OMA routes to WAV");
            const auto decoded=independent_aal::decode_file(oma,output);
            require(decoded.pcm_sha256==encoded.pcm_sha256,"PCM digest agrees across conversion directions");
            require(load(output)==pcm,"Complete WAV round trip and exact sample length");
            require(ui::vacant_output(folder,input)!=oma,"Existing audio is not selected for overwrite");
            const auto next=ui::vacant_output(folder,input);auto report=next;report+=".json";save(report,{1,2,3});
            require(ui::vacant_output(folder,input)!=next,"Existing report also reserves its audio name");
            require(load(report)==std::vector<uint8_t>({1,2,3}),"Existing report preserved");
            ++checked;
        }
        unsigned rejected=0;
        for(auto invalid:{wave(48000),wave(44100,24),wave(44100,16,1),wave(44100,32,2,3)}) {
            const auto bad=directory/("bad-"+std::to_string(rejected)+".wav");save(bad,invalid);
            require(ui::input_problem(bad)!=nullptr,"Unsupported PCM format rejected before queuing");++rejected;
        }
        auto truncated=pcm;truncated.resize(24);save(directory/"truncated.wav",truncated);
        require(ui::input_problem(directory/"truncated.wav"),"Truncated format chunk rejected");
        save(directory/"unsupported.flac",pcm);require(ui::input_problem(directory/"unsupported.flac"),"Unknown extension rejected");
        // Deliberately invalid header, NOT a valid Standard-mode audio sample.
        std::vector<uint8_t> header(96);std::memcpy(header.data(),"EA3",3);header[32]=0x20;
        save(directory/"unsupported-codec.oma",header);
        require(ui::input_problem(directory/"unsupported-codec.oma"),"Non-AAL3/AALP codec is rejected");
        std::atomic_bool cancel{true};
        const auto cancelled=directory/"cancelled.oma";
        bool stopped=false;
        try{independent_aal::encode_file(input,cancelled,{},&cancel);}catch(const std::exception&){stopped=true;}
        require(stopped&&!fs::exists(cancelled),"Cancellation publishes no incomplete audio");
        fs::remove_all(directory);owned=false;
        std::cout<<"PASS: shared native converter; "<<checked<<" Unicode file round trips, PCM format rejection, collision protection and cancellation; no window launched\n";
        return 0;
    }catch(const std::exception& error){
        if(owned){std::error_code ec;fs::remove_all(directory,ec);}
        std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}
