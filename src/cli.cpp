#include "encoder_api.hpp"
#include "decoder_api.hpp"
#include "platform_io.hpp"
#include "codec_profiles.hpp"
#include <csignal>
#include <iostream>
#include <vector>

namespace {
std::atomic_bool cancelled(false);
#ifdef _WIN32
BOOL WINAPI stop(DWORD type) {
    if(type==CTRL_C_EVENT||type==CTRL_BREAK_EVENT){cancelled.store(true);return TRUE;}return FALSE;
}
#else
void stop(int){cancelled.store(true);}
#endif
void help() {
    std::cout<<"AtracDEnc-AAL 0.4.0\n"
        <<"Usage:\n  atracdenc-aal input.wav output.oma\n  atracdenc-aal --profile aalp-352 -i input.wav -o output.oma\n"
        <<"  atracdenc-aal --codec aal3 --bitrate 132 input.wav output.oma\n"
        <<"  atracdenc-aal --decode input.oma output.wav\n"
        <<"  atracdenc-aal --list-profiles\n  atracdenc-aal --self-test [--profile aalp-352]\n  atracdenc-aal --self-test-all\n\n"
        <<"Input: 44100 Hz, 16-bit, stereo integer PCM RIFF/WAV; at most 1 GiB.\n"
        <<"Decode: unencrypted 44100 Hz / 16-bit / stereo AAL3 or AALP; automatic profile detection.\n"
        <<"Mode: High-Speed Transfer (two-layer AAL); Standard is not supported.\n"
        <<"No resampling or bit-depth conversion; 24-bit, float, mono and non-44100 Hz WAV are rejected.\n"
        <<"Default: AALP / ATRAC3plus 352 kbps core with lossless enhancement.\n"
        <<"AAL3: 132 kbps. AALP: 48/64/96/128/160/192/256/320/352 kbps.\n"
        <<"--bitrate accepts nominal kbps or bps, e.g. 352 or 352000.\n"
        <<"Unsupported settings fail; the encoder never substitutes another core.\n"
        <<"Original AAL decorrelation / fixed or LPC8 / partitioned Rice coding.\n"
        <<"Incompressible input uses the original 95% raw fallback.\n"
        <<"Compatibility cores are independent implementations; complete files are not Sony byte-identical.\n"
        <<"No Sony / SMLE / TraConv / FFmpeg / .NET runtime dependency.\n"
        <<"Existing output and output.oma.json files are never overwritten.\n"
        <<"Physical X1050/X1060 playback has not been tested.\n";
}
uint32_t bitrate(const std::string& text) {
    uint64_t value=0;if(text.empty())throw std::runtime_error("Empty bitrate.");
    for(unsigned char c:text){if(c<'0'||c>'9')throw std::runtime_error("Bitrate must be an unsigned integer.");value=value*10+(c-'0');if(value>1000000)throw std::runtime_error("Unsupported core bitrate.");}
    if(value<1000)value*=1000;
    if(!value||value%1000)throw std::runtime_error("Use an official nominal core bitrate, e.g. 352 or 352000.");
    return uint32_t(value);
}
void list_profiles() {
    std::cout<<"profile family nominal_bps actual_bps core_bytes frame_samples\n";
    for(const auto& profile:independent_aal::kOfficialProfiles)std::cout<<profile.id<<' '<<profile.container_name<<' '<<profile.nominal_bitrate<<' '<<profile.actual_core_bitrate()<<' '<<profile.core_bytes<<' '<<profile.frame_samples<<'\n';
}
}
int run(const std::vector<std::string>& args) {
    const int argc=int(args.size());
    try {
        if(argc==1){help();return 0;}
        std::filesystem::path input,output;bool test=false,all_tests=false,list=false,explicit_profile=false,decode=false;
        independent_aal::EncodeOptions options;std::string codec;uint32_t requested_rate=0;
        std::vector<std::string> positional;
        for(int i=1;i<argc;++i) {
            const std::string arg=args[i];
            if(arg=="--help"||arg=="-h"){help();return 0;}
            if(arg=="--decode"||arg=="-d"){if(decode)throw std::runtime_error("Repeated --decode.");decode=true;continue;}
            if(arg=="--self-test"){test=true;continue;}
            if(arg=="--self-test-all"){test=true;all_tests=true;continue;}
            if(arg=="--list-profiles"){list=true;continue;}
            if(arg=="--version"){std::cout<<"AtracDEnc-AAL 0.4.0\n";return 0;}
            if(arg=="--profile"||arg=="--codec"||arg=="--bitrate") {
                if(++i>=argc)throw std::runtime_error("Missing profile option value.");
                if(arg=="--profile") {if(explicit_profile)throw std::runtime_error("Repeated --profile.");options.profile_id=args[i];explicit_profile=true;}
                else if(arg=="--codec") {if(!codec.empty())throw std::runtime_error("Repeated --codec.");codec=args[i];}
                else {if(requested_rate)throw std::runtime_error("Repeated --bitrate.");requested_rate=bitrate(args[i]);}
                continue;
            }
            if(arg=="-i"||arg=="--input"||arg=="-o"||arg=="--output") {
                if(++i>=argc)throw std::runtime_error("Missing option value.");
                auto& target=(arg=="-i"||arg=="--input")?input:output;
                if(!target.empty())throw std::runtime_error("Repeated input/output option.");target=std::filesystem::u8path(args[i]);
            } else if(!arg.empty()&&arg[0]=='-')throw std::runtime_error("Unknown option. Use --help.");
            else positional.push_back(arg);
        }
        if(decode&&(test||list||explicit_profile||!codec.empty()||requested_rate))throw std::runtime_error("--decode cannot select encoding profiles or self-tests.");
        if(list){list_profiles();return 0;}
        if(explicit_profile&&(!codec.empty()||requested_rate))throw std::runtime_error("Choose --profile or --codec/--bitrate, not both.");
        if(!explicit_profile&&(!codec.empty()||requested_rate)) {
            if(codec.empty())codec="aalp";
            if(codec!="aalp"&&codec!="aal3")throw std::runtime_error("--codec must be aalp or aal3.");
            if(!requested_rate)requested_rate=codec=="aalp"?352000:132000;
            options.profile_id=codec+"-"+std::to_string(requested_rate/1000);
        }
        independent_aal::profile_by_id(options.profile_id);
        if(!positional.empty()) {
            if(positional.size()!=2||!input.empty()||!output.empty())throw std::runtime_error("Supply exactly an input and an output.");
            input=std::filesystem::u8path(positional[0]);output=std::filesystem::u8path(positional[1]);
        }
        int last=-1;
        auto progress=[&](int percent,const std::wstring& stage){if(percent!=last){std::cerr<<percent<<"% "<<independent_aal::utf8(stage)<<'\n';last=percent;}};
        if(test) {
            if(!input.empty()||!output.empty())throw std::runtime_error("--self-test cannot be combined with file arguments.");
            if(all_tests) {
                if(explicit_profile||!codec.empty()||requested_rate)throw std::runtime_error("--self-test-all cannot select a single profile.");
                for(const auto& profile:independent_aal::kOfficialProfiles){options.profile_id=profile.id;last=-1;const auto hash=independent_aal::self_test(progress,&cancelled,options);std::cout<<"PASS "<<options.profile_id<<" pcm_sha256="<<hash<<'\n';}
            } else {const auto hash=independent_aal::self_test(progress,&cancelled,options);std::cout<<"PASS "<<options.profile_id<<" pcm_sha256="<<hash<<'\n';}
            return 0;
        }
        if(input.empty()||output.empty())throw std::runtime_error("Input and output are required. Use --help.");
        if(decode) {
            const auto result=independent_aal::decode_file(input,output,progress,&cancelled);
            std::cout<<"OK "<<result.output_path.u8string()<<"\n"
                     <<"profile="<<result.profile_id<<" sample_frames="<<result.sample_frames<<" bytes="<<result.output_bytes<<" pcm_sha256="<<result.pcm_sha256<<"\n";
            return 0;
        }
        const auto result=independent_aal::encode_file(input,output,progress,&cancelled,options);
        std::cout<<"OK "<<result.output_path.u8string()<<"\n"
                 <<"profile="<<result.profile_id<<" sample_frames="<<result.sample_frames<<" bytes="<<result.output_bytes<<" pcm_sha256="<<result.pcm_sha256<<"\n"
                 <<"report="<<result.report_path.u8string()<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"Error: "<<error.what()<<'\n';return cancelled.load()?130:1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);SetConsoleCtrlHandler(stop,TRUE);
    std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(independent_aal::utf8(argv[i]));return run(args);
}
#else
int main(int argc,char** argv) {
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    return run(std::vector<std::string>(argv,argv+argc));
}
#endif
