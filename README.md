# AtracDEnc-AAL

**AtracDEnc-AAL 是基于 [AtracDEnc](https://github.com/dcherednik/atracdenc) 项目开发的 ATRAC Advanced Lossless（AAL）编码器。**

**AtracDEnc-AAL is an ATRAC Advanced Lossless (AAL) encoder built on the [AtracDEnc](https://github.com/dcherednik/atracdenc) project.**

提供独立命令行、C 接口与简易 Windows 测试界面，目标平台为 Windows 和 macOS。当前版本：**0.2.0，实验版**。

A standalone CLI, C API and small Windows test GUI for Windows and macOS. Current version: **0.2.0, experimental**.

## 项目说明 / About

本项目使用 AtracDEnc 的 ATRAC3 / ATRAC3plus 兼容编码实现，并在此基础上增加 AAL3 / AALP 封装、确定性整数预测、无损增强层、位流自校验和应用集成接口。上游归属、引用代码与修改范围见 [UPSTREAM.md](UPSTREAM.md) 和 [第三方声明](THIRD-PARTY-NOTICES.txt)。

This project uses AtracDEnc's compatible ATRAC3 / ATRAC3plus encoding implementations and adds AAL3 / AALP framing, deterministic integer prediction, lossless enhancement coding, bitstream verification and application integration. See [UPSTREAM.md](UPSTREAM.md) and [third-party notices](THIRD-PARTY-NOTICES.txt) for attribution and the modification scope.

运行时不需要安装或调用 Sony SMLE、TraConv、Sony 编解码 DLL、FFmpeg 或 .NET。Sony 软件仅用作开发阶段的外部验证参考，不包含在项目或发行包中。本项目不由 Sony 开发或背书。

No Sony SMLE, TraConv, Sony codec DLL, FFmpeg or .NET installation is needed at runtime. Sony software was used only as an external development reference and is not distributed. This project is not developed or endorsed by Sony.

## 支持配置 / Supported profiles

| 配置 Profile | 格式 Format | 有损核心 Lossy core | 标称核心码率 Nominal kbps | 核心字节/帧 Core bytes/frame |
| --- | --- | --- | ---: | ---: |
| `aal3-132` | AAL3 | ATRAC3 | 132 | 384 |
| `aalp-48` | AALP | ATRAC3plus | 48 | 280 |
| `aalp-64` | AALP | ATRAC3plus | 64 | 376 |
| `aalp-96` | AALP | ATRAC3plus | 96 | 560 |
| `aalp-128` | AALP | ATRAC3plus | 128 | 744 |
| `aalp-160` | AALP | ATRAC3plus | 160 | 936 |
| `aalp-192` | AALP | ATRAC3plus | 192 | 1120 |
| `aalp-256` | AALP | ATRAC3plus | 256 | 1488 |
| `aalp-320` | AALP | ATRAC3plus | 320 | 1864 |
| **`aalp-352`** | **AALP** | **ATRAC3plus** | **352（默认 / default）** | **2048** |

上述十种配置均经 Sony SMLE 7.1 编码工厂逐档确认。码率仅指有损核心，不含无损增强数据；AAL 总体积取决于音频内容。程序拒绝不支持的配置，不自动替换核心或降档。普通 ATRAC3 的 66 / 105 kbps 不属于这里的 AAL3 配置。

All ten configurations were individually confirmed against the Sony SMLE 7.1 encoding factory. Rates refer to the lossy core, excluding lossless enhancement data; total AAL size depends on the audio. Unsupported profiles fail explicitly. Ordinary ATRAC3 rates of 66 / 105 kbps are not AAL3 profiles here.

## 输入与输出 / Input and output

- 输入 / Input：**44,100 Hz、16-bit、双声道整数 PCM RIFF/WAV**；文件上限 / maximum input file size：**1 GiB**。
- 输出 / Output：未加密的 **OMA / AAL3 或 AALP**，以及包含配置、PCM / OMA SHA-256 和校验结果的 JSON 报告。Unencrypted **OMA / AAL3 or AALP**, plus a JSON report with profile, PCM / OMA SHA-256 and verification results.
- 编码器在保存前重新读取实际输出位流，逐样本核对 PCM；不覆盖已有输出。The encoder rereads its emitted bitstream and checks every PCM sample before saving; existing outputs are never overwritten.
- 当前不包含重采样、位深转换、DRM、播放器传输或独立解码命令。Resampling, bit-depth conversion, DRM, device transfer and a standalone decode command are not included.

## 快速开始 / Quick start

从发行包解压后，在程序所在目录执行。Extract a release package, then run these commands from its directory.

Windows：

```powershell
.\atracdenc-aal.exe --list-profiles
.\atracdenc-aal.exe --self-test-all
.\atracdenc-aal.exe --profile aalp-352 "input.wav" "output.oma"
.\atracdenc-aal.exe --profile aal3-132 "input.wav" "output.oma"
```

macOS：

```bash
chmod +x atracdenc-aal
./atracdenc-aal --self-test-all
./atracdenc-aal --profile aalp-352 "input.wav" "output.oma"
```

不指定配置时使用 `aalp-352`。也可使用 `--codec aalp --bitrate 352`；`352000` 同样有效。`--profile` 不与 `--codec` / `--bitrate` 混用。`Ctrl+C` 可取消命令行编码。

The default is `aalp-352`. `--codec aalp --bitrate 352` is equivalent; `352000` is also accepted. Do not combine `--profile` with `--codec` / `--bitrate`. Press `Ctrl+C` to cancel CLI encoding.

Windows 的 `AtracDEnc-AAL-GUI.exe` 是**简易测试图形界面**，提供配置选择、文件队列、输出目录、进度和取消操作，直接调用同一编码引擎。启动时按 Windows 首选显示语言在中英文之间自适应，无匹配时回退英文；界面、日志和编码进度均提供中英文本。也可显式选择语言：

On Windows, `AtracDEnc-AAL-GUI.exe` is a **simple test GUI** providing profile selection, a file queue, an output directory, progress and cancellation through the same directly linked engine. At startup it selects Chinese or English from Windows' preferred display languages, falling back to English. Interface text, logs and encoding progress have both translations. An explicit override is also available:

```powershell
.\AtracDEnc-AAL-GUI.exe --lang auto
.\AtracDEnc-AAL-GUI.exe --lang zh
.\AtracDEnc-AAL-GUI.exe --lang en
```

详细操作与语言行为见 [GUI.md](docs/GUI.md)。See [GUI.md](docs/GUI.md) for usage and language behavior.

## 与官方一致的范围 / Conformance scope

增强层按原版方法独立实现，包括声道去相关、固定预测、8 阶 LPC、分区 Rice 编码、95% 原样回退及填充。离线夹具包含本项目合成音频的完整残差与官方编码数据，可逐字节验证 **4583 个增强包**，无需 Sony 软件。

The enhancement stage independently reproduces channel decorrelation, fixed prediction, LPC8, partitioned Rice coding, the original 95% raw fallback and padding. Offline fixtures contain complete synthetic residuals and original encoded data for **4,583 enhancement packets**, requiring no Sony software.

基线验证覆盖全部十档：122 个正例经 Sony 解码后，2,205,346 个 int16 采样值与长度均一致；13 个拒绝用例通过。另有 70 组同输入、同核心实际码率的双向对照，两边均精确还原 PCM。当前更名版的构建、自检及输出一致性记录见 [验证说明](validation/README.md)。

Baseline validation covers all ten profiles: 122 positive cases decoded by Sony matched all 2,205,346 int16 samples and their lengths; 13 rejection cases passed. A further 70 same-input, same-actual-core-rate pairs reconstructed PCM exactly on both sides. See [validation notes](validation/README.md) for the branded build's checks and output-equivalence records.

**有损核心采用 AtracDEnc 的兼容实现，其编码决策不同于 Sony；完整文件不与官方逐字节相同，压缩体积也不保证相同。** 本项目的完整文件验收标准是 PCM 精确还原，增强层逐字节一致仅针对已记录的相同残差输入。

**The lossy core uses AtracDEnc's compatible implementations, whose encoding decisions differ from Sony's. Complete files are not Sony byte-identical, and compression sizes can differ.** Full-file acceptance is exact PCM reconstruction; enhancement byte equality refers to the recorded identical residual inputs.

Windows x64 已执行本地验证。macOS arm64 / x86_64 通用二进制已交叉编译，最低 macOS 11；真实 Mac 执行尚待验证。NW-X1050 / X1060 是目标设备，实机播放、跳转和传输数据库尚未验证。随附 CI 会在 Windows 和两种 Mac 架构上构建并运行离线测试，其实际结果以首次 GitHub Actions 运行为准。

Windows x64 has been tested locally. macOS arm64 / x86_64 universal binaries are cross-compiled for macOS 11 or later; native Mac execution remains unverified. NW-X1050 / X1060 are target devices; physical playback, seeking and transfer databases remain untested. The supplied CI builds and runs offline tests on Windows and both Mac architectures; its results depend on the first actual GitHub Actions run.

## 编译与集成 / Build and integration

macOS 原生编译 / Native macOS build，使用 Apple clang：

```bash
bash build.sh
./bin/atracdenc-aal --self-test-all
./bin/aal-conformance tests/enhancement-vectors.bin
./bin/aal-conformance tests/plus-enhancement-vectors.bin
./bin/aal-c-api-test
```

Windows，使用 Zig 0.14.1 / Windows with Zig 0.14.1：

```powershell
.\build.ps1 -Zig "C:\tools\zig\zig.exe" -Gui
.\bin\atracdenc-aal.exe --self-test-all
.\bin\aal-conformance.exe tests\enhancement-vectors.bin
.\bin\aal-conformance.exe tests\plus-enhancement-vectors.bin
.\bin\aal-c-api-test.exe
```

源码也提供 CMake 工程；详细说明见 [BUILDING.md](docs/BUILDING.md)。这些是本项目根目录的构建入口，不需要编译上游的音频 I/O 前端。

A CMake project is also included; see [BUILDING.md](docs/BUILDING.md). Use this project's root build entry points; the upstream audio-I/O frontend is not required.

其他应用可静态链接库，调用 [C 接口](include/independent_aal.h)：`aal_profile_count`、`aal_profile_get`、`aal_encode_cd16_profile`、`aal_encode_cd16` 和 `aal_free`。现有 `aal_*` ABI 及头文件名称保留，默认配置为 `aalp-352`。示例见 [encode_buffer.c](examples/encode_buffer.c)，线程、内存和 CMake 集成说明见 [INTEGRATION.md](docs/INTEGRATION.md)。

Applications can statically link the library through the [C API](include/independent_aal.h): `aal_profile_count`, `aal_profile_get`, `aal_encode_cd16_profile`, `aal_encode_cd16` and `aal_free`. Existing `aal_*` ABI identifiers and the header name are retained; the default is `aalp-352`. See [encode_buffer.c](examples/encode_buffer.c) and [INTEGRATION.md](docs/INTEGRATION.md).

## 许可证与致谢 / License and credits

项目采用 **LGPL-2.1-or-later**，与所使用的 AtracDEnc 代码保持一致。完整许可证见 [LICENSE](LICENSE)，上游及依赖的版权和许可证见 [CREDITS.md](CREDITS.md)、[UPSTREAM.md](UPSTREAM.md)、[第三方声明](THIRD-PARTY-NOTICES.txt) 和 [licenses/](licenses/)。

The project is licensed under **LGPL-2.1-or-later**, matching the AtracDEnc code used here. See [LICENSE](LICENSE), [CREDITS.md](CREDITS.md), [UPSTREAM.md](UPSTREAM.md), [third-party notices](THIRD-PARTY-NOTICES.txt) and [licenses/](licenses/).

感谢 AtracDEnc 项目及其贡献者提供 ATRAC 编码基础。Thank you to the AtracDEnc project and its contributors for the ATRAC encoding foundation.

[更新记录 / Changelog](CHANGELOG.md) · [贡献说明 / Contributing](CONTRIBUTING.md) · [发布说明 / Release notes](docs/releases/v0.2.0.md)
