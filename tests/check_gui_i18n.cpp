#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "../src/gui_i18n.hpp"
#include <iostream>
#include <stdexcept>

namespace ui=atracdenc_aal_gui;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool contains_chinese(const wchar_t* text){for(;*text;++text)if(*text>=0x3400&&*text<=0x9fff)return true;return false;}
int main(){
    try {
        require(ui::preferred_language({L"zh-CN",L"en-US"})==ui::Language::Chinese,"Chinese preference");
        require(ui::preferred_language({L"zh-TW"})==ui::Language::Chinese,"Traditional Chinese preference");
        require(ui::preferred_language({L"EN-gb",L"zh-CN"})==ui::Language::English,"English priority");
        require(ui::preferred_language({L"de-DE",L"zh-Hans"})==ui::Language::Chinese,"Supported-language fallback");
        require(ui::preferred_language({L"fr-FR"})==ui::Language::English,"English fallback");
        ui::Startup startup;
        require(ui::parse_arguments({L"--lang",L"en",L"中文音频.wav"},ui::Language::Chinese,startup),"English override parse");
        require(startup.language==ui::Language::English&&startup.paths==std::vector<std::wstring>{L"中文音频.wav"},"Filename preserved");
        require(ui::parse_arguments({L"--lang=zh"},ui::Language::English,startup)&&startup.language==ui::Language::Chinese,"Chinese override");
        require(ui::parse_arguments({L"--lang",L"auto"},ui::Language::Chinese,startup)&&startup.language==ui::Language::Chinese,"Automatic mode");
        require(ui::parse_arguments({L"--",L"--lang.wav"},ui::Language::English,startup)&&startup.paths[0]==L"--lang.wav","Literal path");
        require(!ui::parse_arguments({L"--lang"},ui::Language::English,startup),"Missing language rejected");
        require(!ui::parse_arguments({L"--lang=fr"},ui::Language::English,startup),"Unknown language rejected");
        require(!ui::parse_arguments({L"--lang=en",L"--lang=zh"},ui::Language::English,startup),"Repeated language rejected");
        require(!ui::parse_arguments({L"--unknown"},ui::Language::English,startup),"Unknown option rejected");
        size_t entries=0;
        for(const auto& entry:ui::catalog){
            ui::language=ui::Language::English;
            require(std::wcscmp(ui::tr(entry.chinese),entry.english)==0,"English catalog lookup");
            require(!contains_chinese(entry.english),"Untranslated English text");
            ui::language=ui::Language::Chinese;
            require(std::wcscmp(ui::tr(entry.chinese),entry.chinese)==0,"Chinese catalog lookup");
            ++entries;
        }
        ui::language=ui::Language::English;
        require(ui::progress_text(L"计算精确整数预测与无损残差")==L"Computing exact integer prediction and lossless residuals","Engine stage translation");
        require(ui::progress_text(L"self-test-中文.wav")==L"self-test-中文.wav","Arbitrary names untouched");
        const wchar_t* filter=ui::tr(L"PCM WAV 音频 (*.wav)\0*.wav\0所有文件 (*.*)\0*.*\0\0");
        require(std::wstring(filter)==L"PCM WAV audio (*.wav)","File filter title");
        filter+=std::wcslen(filter)+1;
        require(std::wstring(filter)==L"*.wav","File filter pattern");
        filter+=std::wcslen(filter)+1;
        require(std::wstring(filter)==L"All files (*.*)","Second file filter preserved");
#ifdef _WIN32
        const auto detected=ui::system_language();
        require(detected==ui::Language::Chinese||detected==ui::Language::English,"System language detection");
#endif
        std::cout<<"PASS: GUI language selection, overrides, "<<entries<<" bilingual catalog entries, engine progress and Unicode paths; no window launched\n";
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
