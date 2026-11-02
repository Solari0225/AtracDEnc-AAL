#pragma once
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

namespace atracdenc_aal_gui {
enum class Language { Chinese, English };
inline Language language = Language::English; // Selected once before UI/worker creation.
struct Entry { const wchar_t* chinese; const wchar_t* english; };
inline const Entry catalog[] = {
    {L"（解码完成，", L" (decoded, "},
    {L"读取 AAL 文件", L"Read AAL container"},
    {L"解码 AAL 核心与无损增强层", L"Decode AAL core and lossless enhancement"},
    {L"计算解码 PCM 校验值", L"Hash decoded PCM"},
    {L"保存解码后的 PCM WAV", L"Save decoded PCM WAV"},
    {L"等待", L"Pending"},
    {L"处理中", L"Processing"},
    {L"已完成", L"Done"},
    {L"失败", L"Failed"},
    {L"已取消", L"Cancelled"},
    {L" 个文件  ·  已完成 ", L" files  ·  Completed "},
    {L"不是可读取的音频文件", L"Not a readable audio file"},
    {L"请选择 PCM WAV 或 AAL OMA/AA3 文件", L"Select a PCM WAV or AAL OMA/AA3 file"},
    {L"WAV 文件头不完整", L"Incomplete WAV header"},
    {L"需要标准 RIFF / PCM WAV 文件", L"A standard RIFF / PCM WAV file is required"},
    {L"WAV 格式信息不完整", L"Incomplete WAV format information"},
    {L"仅支持 44.1 kHz、16 位、双声道 PCM WAV", L"Only 44.1 kHz, 16-bit, stereo PCM WAV is supported"},
    {L"未找到有效的 PCM 格式信息", L"No valid PCM format information found"},
    {L"已跳过：", L"Skipped: "},
    {L"已添加 ", L"Added "},
    {L" 个文件。可继续添加或开始转换。", L" files. Add more files or start conversion."},
    {L"文件未加入队列，请查看下方日志", L"File was not queued; see the log below"},
    {L"WAV / AAL 音频\0*.wav;*.oma;*.aa3\0所有文件 (*.*)\0*.*\0\0", L"WAV / AAL audio\0*.wav;*.oma;*.aa3\0All files (*.*)\0*.*\0\0"},
    {L"添加 PCM WAV 或 AAL OMA/AA3", L"Add PCM WAV or AAL OMA/AA3"},
    {L"本次选择的文件过多，请分批添加。", L"Too many files selected; add them in smaller batches."},
    {L"选择转换结果的保存文件夹", L"Choose a folder for converted audio"},
    {L"音频", L"Audio"},
    {L"请先设置输出文件夹。", L"Choose an output folder first."},
    {L"正在进行内置自检…", L"Running the built-in self-test..."},
    {L"正在准备转换…", L"Preparing conversion..."},
    {L"开始内置自检：检查独立编码、解码与 PCM 一致性。", L"Starting self-test: independent encoding, decoding and exact PCM reconstruction."},
    {L"开始转换：WAV 编码为 AAL，OMA/AA3 解码为 WAV。", L"Processing started. Each file is saved after exact PCM verification."},
    {L"本次编码核心配置：", L"Encoding profile: "},
    {L"自检通过：编码、解码及 PCM 一致性检查完成。", L"Self-test passed: encoding, decoding and exact PCM reconstruction verified."},
    {L"自检 SHA-256：", L"Self-test SHA-256: "},
    {L"正在处理 ", L"Processing "},
    {L"：", L": "},
    {L"完成：", L"Done: "},
    {L"（无损校验通过，", L" (lossless verification passed, "},
    {L"（", L" ("},
    {L"）", L")"},
    {L"已取消：", L"Cancelled: "},
    {L"转换失败：", L"Processing failed: "},
    {L"自检失败：", L"Self-test failed: "},
    {L"无法开始转换：", L"Cannot start conversion: "},
    {L"处理失败，请检查音频文件和输出目录后重试。", L"Processing failed. Check the audio file and output folder, then try again."},
    {L"已取消。已完成的文件已保留，可再次开始处理其余文件。", L"Cancelled. Completed files were kept; start again to process the remaining files."},
    {L"自检未通过，请查看日志。", L"Self-test failed; see the log."},
    {L"内置自检通过。", L"Built-in self-test passed."},
    {L"本次处理结束：成功 ", L"Finished: "},
    {L" 个，失败 ", L" succeeded, "},
    {L" 个。", L" failed."},
    {L"无法启动后台任务：", L"Cannot start the background task: "},
    {L"简易测试图形界面  ·  基于 AtracDEnc 的 AAL 编解码器", L"Simple test GUI  ·  AAL codec based on AtracDEnc"},
    {L"PCM WAV → AAL；AAL → WAV。44.1 kHz / 16-bit / 双声道，解码自动识别配置。", L"PCM WAV to AAL; AAL to WAV. 44.1 kHz / 16-bit / stereo. Decoding detects the profile."},
    {L"添加音频(&A)…", L"Add audio (&A)..."},
    {L"移除选中", L"Remove"},
    {L"清空队列", L"Clear queue"},
    {L"编码核心", L"Core"},
    {L"文件名称", L"File name"},
    {L"状态", L"Status"},
    {L"进度", L"Progress"},
    {L"输出文件", L"Output file"},
    {L"输出文件夹（重名自动加序号；编码时保存校验报告）", L"Output folder (unique names; reports are saved for encoding)"},
    {L"选择文件夹…", L"Browse..."},
    {L"准备就绪：添加 PCM WAV 或 AAL OMA/AA3", L"Ready: add PCM WAV or AAL OMA/AA3 files"},
    {L"开始转换(&S)", L"Convert (&S)"},
    {L"取消", L"Cancel"},
    {L"内置自检", L"Self-test"},
    {L"打开输出文件夹", L"Open output folder"},
    {L"处理日志", L"Log"},
    {L"实验版 0.3.0  ·  Sony X1050 / X1060 等老款 Walkman 的实机兼容性待验证", L"Experimental 0.3.0  ·  Physical X1050 / X1060 compatibility remains unverified"},
    {L"就绪。首次添加文件后，默认保存到首个音源目录下的 AAL 文件夹。", L"Ready. The default output is the AAL folder beside the first input file."},
    {L"正在取消并清理，请稍候…", L"Cancelling and cleaning up; please wait..."},
    {L"已请求取消，正在等待当前任务安全结束。", L"Cancellation requested; waiting for the current task to stop safely."},
    {L"输出文件夹尚未创建，开始转换后将自动建立。", L"The output folder will be created when conversion starts."},
    {L"当前任务结束后，可继续拖入文件。", L"Drop more files after the current task finishes."},
    {L"正在取消并清理，完成后自动关闭…", L"Cancelling and cleaning up; the window will close when finished..."},
    {L"操作未完成：", L"Operation failed: "},
    {L"窗口初始化失败。", L"Window initialization failed."},
    {L"AtracDEnc-AAL · 简易测试图形界面 0.3.0", L"AtracDEnc-AAL · Simple test GUI 0.3.0"},
    {L"语言参数无效。请使用 --lang auto、zh 或 en。", L"Invalid language option. Use --lang auto, zh or en."},
    {L"编码 ATRAC3plus 音乐核心", L"Processing the ATRAC3plus core"},
    {L"编码 ATRAC3 音乐核心", L"Processing the ATRAC3 core"},
    {L"计算精确整数预测与无损残差", L"Computing exact integer prediction and lossless residuals"},
    {L"按原版方法压缩：声道去相关、预测与 Rice 编码", L"Compressing: channel decorrelation, prediction and Rice coding"},
    {L"读取实际输出位流并逐样本自校验", L"Reading the emitted bitstream and verifying every PCM sample"},
    {L"读取 CD PCM WAV", L"Reading CD PCM WAV"},
    {L"保存已校验的 AAL 与报告", L"Saving verified AAL and report"},
    {L"完成：PCM 逐样本自校验通过", L"Done: exact PCM round-trip passed"},
    {L"自检通过：8443 个立体声采样帧无损往返", L"Self-test passed: 8443 stereo sample frames reconstructed exactly"},
};
inline const wchar_t* tr(const wchar_t* chinese) {
    if (language == Language::Chinese) return chinese;
    for (const auto& entry : catalog)
        if (std::wcscmp(entry.chinese, chinese) == 0) return entry.english;
    return chinese;
}
inline Language preferred_language(const std::vector<std::wstring>& tags) {
    for (auto tag : tags) {
        std::transform(tag.begin(),tag.end(),tag.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
        if (tag == L"zh" || tag.rfind(L"zh-",0) == 0) return Language::Chinese;
        if (tag == L"en" || tag.rfind(L"en-",0) == 0) return Language::English;
    }
    return Language::English;
}
#ifdef _WIN32
inline Language system_language() {
    ULONG count=0, size=0;
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,&count,nullptr,&size) && size>=2 && size<=65536) {
        std::vector<wchar_t> buffer(size,L'\0');
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,&count,buffer.data(),&size)) {
            std::vector<std::wstring> tags;
            for (const wchar_t* p=buffer.data(); *p; p+=std::wcslen(p)+1) tags.emplace_back(p);
            if (!tags.empty()) return preferred_language(tags);
        }
    }
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE ? Language::Chinese : Language::English;
}
#endif
struct Startup { Language language=Language::English; std::vector<std::wstring> paths; };
inline bool parse_arguments(const std::vector<std::wstring>& arguments,Language automatic,Startup& output) {
    output={automatic,{}}; bool literal_paths=false, seen_language=false;
    for (size_t index=0; index<arguments.size(); ++index) {
        const auto& argument=arguments[index];
        if (!literal_paths && argument==L"--") {literal_paths=true;continue;}
        if (!literal_paths && (argument==L"--lang" || argument.rfind(L"--lang=",0)==0)) {
            if (seen_language) return false;
            seen_language=true;
            std::wstring value;
            if (argument==L"--lang") {if (++index==arguments.size())return false;value=arguments[index];}
            else value=argument.substr(7);
            std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
            if (value==L"auto") output.language=automatic;
            else if (value==L"zh") output.language=Language::Chinese;
            else if (value==L"en") output.language=Language::English;
            else return false;
        } else {
            if (!literal_paths && argument.rfind(L"--",0)==0) return false;
            output.paths.push_back(argument);
        }
    }
    return true;
}
inline std::wstring progress_text(const std::wstring& stage) {
    // Engine stages contain no user paths. Translate exact stage values only;
    // filenames and arbitrary diagnostic text are never rewritten.
    if (language == Language::Chinese)
        for (size_t i = 1; i <= 4; ++i)
            if (stage == catalog[i].english) return catalog[i].chinese;
    return tr(stage.c_str());
}
} // namespace atracdenc_aal_gui
