# 简易测试文件转换器 / Simple test file converters

| 平台 / Platform | 程序 / Application | 实现 / Implementation |
| --- | --- | --- |
| Windows x64 | `AtracDEnc-AAL-GUI.exe` | Win32 原生界面 / native Win32 |
| macOS 11+，Apple Silicon / Intel | `AtracDEnc-AAL-GUI.app` | AppKit 原生界面 / native AppKit |

两者均标注为**简易测试图形界面**，用于基本文件转换和手动验证，直接静态链接同一独立 AAL 编解码核心。无需 Sony、TraConv、FFmpeg、.NET、Python 或 Node 运行环境，不启动外部转换命令。

Both are labeled **simple test GUIs** for basic conversion and manual testing. They statically link the same independent engine. No Sony component, TraConv, FFmpeg, .NET, Python or Node runtime is needed, and no external conversion process is launched.

## 输入 / Input

**WAV：44,100 Hz / signed 16-bit / 双声道整数 PCM RIFF/WAV，format tag 1，≤1 GiB。AAL：未加密的 AAL3/AALP OMA/AA3，十档配置，≤1 GiB；解码 PCM ≤1 GiB。** 不自动重采样、转换位深或混合声道；不支持 Standard、DRM、普通有损 ATRAC、MP3、FLAC 等直接输入。完整要求见 [INPUT-FORMATS.md](INPUT-FORMATS.md)。

**WAV: 44,100 Hz / signed 16-bit / stereo integer PCM RIFF/WAV, format tag 1, up to 1 GiB. AAL: unencrypted AAL3/AALP OMA/AA3 in the ten supported profiles, up to 1 GiB; decoded PCM up to 1 GiB.** No resampling, bit-depth conversion or channel mixing. Standard, DRM, ordinary lossy ATRAC, MP3 and FLAC direct input are unsupported. See [INPUT-FORMATS.md](INPUT-FORMATS.md).

## 使用 / Use

1. 添加或拖入文件；不合规格的文件会被跳过并在日志中说明。Add or drop files; unsupported inputs are skipped with a log entry.
2. 编码选择核心，默认 `aalp-352`；解码自动识别，忽略该选项。Choose an encoding core (default `aalp-352`); decoding detects the profile automatically.
3. 选择输出目录，点击“开始转换 / Convert”。首次加入文件后，空的默认目录设为首个音源旁的 `AAL` 文件夹。Choose an output folder and start conversion. Initially, an empty output field defaults to an `AAL` folder beside the first source.
4. WAV→OMA 并生成 `.oma.json`；AAL→WAV，日志提供 PCM SHA-256。WAV encodes to OMA plus a JSON report; AAL decodes to WAV with its PCM SHA-256 in the log.

同名音频或编码报告存在时自动选择带序号的新文件名。取消或关闭正在处理的窗口会先请求引擎停止并清理临时文件，再退出；已完成结果保留。内置自检使用当前选择的配置执行内存往返，不需要输入文件。

Existing audio/report names cause a numbered output name. Cancellation or closing a busy window first stops the engine and cleans up temporary files before exit; completed results remain. Self-test performs an in-memory round trip using the selected profile, without an input file.

## 语言 / Language

按 Windows 或 macOS 的系统首选语言顺序选择首个支持的中文/英文；其他语言回退英文。中文地区均使用简体中文翻译。重新启动后重新判断，不修改系统设置。

The first supported Chinese or English language in the system's ordered preferences is selected; other languages fall back to English. Chinese preferences use Simplified Chinese. This is reevaluated on restart and does not modify system settings.

```powershell
.\AtracDEnc-AAL-GUI.exe --lang auto
.\AtracDEnc-AAL-GUI.exe --lang zh "input.wav"
.\AtracDEnc-AAL-GUI.exe --lang en
```

```bash
open AtracDEnc-AAL-GUI.app --args --lang zh
open AtracDEnc-AAL-GUI.app --args --lang en
```

文件名、路径及底层系统/编解码诊断保持原文。Filenames, paths and underlying system/codec diagnostics retain their original text.

## 检查边界 / Validation boundary

无窗口检查覆盖语言选择、翻译、两平台共用的文件预检查、Unicode 路径、编码→完整解码文件往返、防覆盖与取消。`--self-test` 可在程序入口执行链接核心的诊断并返回退出码，不创建窗口。

Headless checks cover language selection/translations, shared input preflight, Unicode paths, complete-file round trips, collision protection and cancellation. `--self-test` runs linked-engine diagnostics from the application entry point and returns an exit code without creating a window.

**编译成功和无窗口检查不等于 GUI 视觉或交互验证；窗口操作由用户自行测试。** macOS `.app` 发行构建使用本地 ad-hoc 签名，没有 Developer ID 签名或公证。核心库集成见 [INTEGRATION.md](INTEGRATION.md)。

**Compilation and headless checks do not establish visual/interactive GUI validation; the user performs window testing.** The macOS app is ad-hoc signed, without Developer ID signing or notarization. See [INTEGRATION.md](INTEGRATION.md) for library use.
