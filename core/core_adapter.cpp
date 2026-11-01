// Adapter around the LGPL-2.1-or-later atracdenc ATRAC3 encoder.
// Original adapter code for the independent AAL research build.
#include "atrac3denc.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

struct CoreOutput : ICompressedOutput {
    std::vector<char> Frame;
    std::string GetName() const override { return "AAL ATRAC3 core"; }
    size_t GetChannelNum() const override { return 2; }
    void WriteFrame(std::vector<char> data) override { Frame = std::move(data); }
};

struct CoreEncoder {
    CoreOutput* Output;
    std::unique_ptr<NAtracDEnc::TAtrac3Encoder> Encoder;
    TPCMEngine::TProcessLambda Process;
    CoreEncoder() {
        std::unique_ptr<CoreOutput> out(new CoreOutput());
        Output = out.get();
#ifdef CORE_LOWBAND
        NAtracDEnc::NAtrac3::TAtrac3EncoderSettings settings(132300, true, true, 2, 26);
#elif defined(CORE_INDEPENDENT)
        // A full-band compatibility core with a simpler deterministic decoder:
        // no gain points or separately coded tonal components.
        NAtracDEnc::NAtrac3::TAtrac3EncoderSettings settings(132300, true, true, 2, 0);
#else
        NAtracDEnc::NAtrac3::TAtrac3EncoderSettings settings(132300, false, false, 2, 0);
#endif
        Encoder.reset(new NAtracDEnc::TAtrac3Encoder(TCompressedOutputPtr(out.release()), std::move(settings)));
        Process = Encoder->GetLambda();
    }
};

extern "C" void* at3core_create() {
    try { return new CoreEncoder(); } catch (...) { return nullptr; }
}
// Feed 1024 stereo int16 sample frames. Return 0 for the initial lookahead,
// 384 for an output frame, and -1 on failure. Feed one zero block to drain.
extern "C" int at3core_encode(void* context, const int16_t* pcm, unsigned char* output, size_t capacity) {
    if (!context || !pcm || !output || capacity < 384) return -1;
    try {
        CoreEncoder& ctx = *static_cast<CoreEncoder*>(context);
        float samples[2048];
        for (size_t i=0; i<2048; ++i) samples[i] = pcm[i] / 32768.0f;
        ctx.Output->Frame.clear();
        ctx.Process(samples, TPCMEngine::ProcessMeta{2});
        if (ctx.Output->Frame.empty()) return 0;
        if (ctx.Output->Frame.size() != 384) return -1;
        std::memcpy(output, ctx.Output->Frame.data(), 384);
        return 384;
    } catch (...) { return -1; }
}
extern "C" void at3core_destroy(void* context) { delete static_cast<CoreEncoder*>(context); }

#ifdef CORE_CLI
static uint16_t le16(const unsigned char* p) { return p[0] | (uint16_t(p[1])<<8); }
static uint32_t le32(const unsigned char* p) { return le16(p) | (uint32_t(le16(p+2))<<16); }
int main(int argc, char** argv) {
    if (argc!=3) { std::fprintf(stderr,"Usage: atrac3-core input.wav output.raw\nStereo 44100 Hz 16-bit WAV -> 384-byte ATRAC3 frames\n"); return 1; }
    FILE* in=std::fopen(argv[1],"rb"); if(!in) return 2;
    unsigned char root[12];
    if(std::fread(root,1,12,in)!=12 || std::memcmp(root,"RIFF",4) || std::memcmp(root+8,"WAVE",4)) return 3;
    bool valid=false; uint32_t size=0;
    while(true) {
        unsigned char h[8]; if(std::fread(h,1,8,in)!=8) return 4;
        uint32_t len=le32(h+4);
        if(!std::memcmp(h,"fmt ",4)) {
            unsigned char fmt[16]; if(len<16 || std::fread(fmt,1,16,in)!=16) return 4;
            valid=le16(fmt)==1 && le16(fmt+2)==2 && le32(fmt+4)==44100 && le16(fmt+14)==16;
            std::fseek(in,(len-16)+(len&1),SEEK_CUR);
        } else if(!std::memcmp(h,"data",4)) { size=len; break; }
        else std::fseek(in,len+(len&1),SEEK_CUR);
    }
    if(!valid || !size || size%4) return 5;
    FILE* out=std::fopen(argv[2],"wb"); if(!out) return 6;
    void* context=at3core_create(); if(!context) return 7;
    const size_t frames=(size/4+1023)/1024;
    size_t outputs=0; unsigned char encoded[384];
    for(size_t i=0;i<=frames;i++) {
        int16_t samples[2048]={};
        if(i<frames) {
            size_t count=std::min<size_t>(4096,size);
            if(std::fread(samples,1,count,in)!=count) return 8;
            size-=uint32_t(count);
        }
        int n=at3core_encode(context,samples,encoded,sizeof(encoded));
        if(n<0) return 9;
        if(n>0) { if(std::fwrite(encoded,1,n,out)!=size_t(n)) return 10; outputs++; }
    }
    at3core_destroy(context); std::fclose(in); std::fclose(out);
    std::fprintf(stderr,"ATRAC3 core: %zu frames, 384 bytes/frame, 132300 bps\n",outputs);
    return outputs==frames?0:11;
}
#endif
