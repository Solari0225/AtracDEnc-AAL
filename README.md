# AtracDEnc-AAL

**AtracDEnc-AAL 是基于 [AtracDEnc](https://github.com/dcherednik/atracdenc) 项目开发的 ATRAC Advanced Lossless（AAL）编解码器。**

**AtracDEnc-AAL is an ATRAC Advanced Lossless (AAL) encoder/decoder built on the [AtracDEnc](https://github.com/dcherednik/atracdenc) project.**

提供独立命令行、C 接口与 **Windows / macOS 双平台简易测试文件转换器**。当前版本：**0.5.0，实验版**。

A standalone CLI, C API and **small native test file converters for Windows and macOS**. Current version: **0.5.0, experimental**.

## 转换器与输入要求 / Converter and input requirements

从 [GitHub Releases](https://github.com/Solari0225/AtracDEnc-AAL/releases) 下载：Windows x64 ZIP 内打开 **`AtracDEnc-AAL-GUI.exe`**；macOS universal ZIP 内打开 **`AtracDEnc-AAL-GUI.app`**（Apple Silicon / Intel，macOS 11+）。两者均为**简易测试图形界面**，按系统首选语言自动显示中文或英文，不需要安装 Sony 组件或其他音频工具。

Download a [release](https://github.com/Solari0225/AtracDEnc-AAL/releases). Open **`AtracDEnc-AAL-GUI.exe`** on Windows x64, or **`AtracDEnc-AAL-GUI.app`** from the macOS universal package (Apple Silicon / Intel, macOS 11+). Both are **simple test GUIs**, automatically selecting Chinese or English, with no Sony component or external audio-tool installation.

| 转换 / Conversion | 输入规格 / Required input | 输出 / Output |
| --- | --- | --- |
| WAV → AAL | **44,100 Hz / signed 16-bit / 双声道 stereo / 整数 integer PCM RIFF/WAV 或 RF64，format tag 1；流式处理 / streaming** | 未加密 AAL3/AALP `.oma` ＋ JSON 校验报告 / unencrypted AAL plus verification report |
| AAL → WAV | **未加密 unencrypted AAL3/AALP `.oma` / `.aa3`，本项目十档配置 / ten supported profiles；流式处理 / streaming** | **44,100 Hz / signed 16-bit / stereo PCM WAV**，大文件自动使用 RF64 / RF64 for large output |

**24/32-bit、浮点、单声道、多声道和其他采样率的 WAV 会被拒绝；不自动重采样或转换位深。MP3/FLAC/AAC/AIFF 等不是直接输入。** 0.5.0 已移除固定 1 GiB 上限；CLI/GUI 使用有界缓冲。普通 RIFF 容量不足时解码自动输出 RF64。详见 [输入规格与拒绝规则](docs/INPUT-FORMATS.md)。

**24/32-bit, float, mono, multichannel and other-rate WAV are rejected; there is no automatic resampling or bit-depth conversion. MP3/FLAC/AAC/AIFF are not direct inputs.** Version 0.5.0 removes the fixed 1 GiB cap. CLI/GUI conversion uses bounded buffers and switches decoded output to RF64 when RIFF size fields are insufficient. See [input requirements](docs/INPUT-FORMATS.md).

## 0.5.0 流式处理 / Streaming

编码和解码按帧推进，保留有界 PCM 队列、编解码状态和当前包；编码逐包独立重建并逐样本比较，SHA-256 增量计算。十档配置的 70 项冻结用例检查完整 AAL 与 0.4.0 的摘要和长度一致。新增回调式 C API；旧整缓冲 ABI 保留，但其返回结果仍需调用方提供足够内存。

Encoding/decoding advance frame by frame with bounded PCM queues, persistent codec state and a current packet. Encoding independently reconstructs each serialized packet and checks every sample; hashing is incremental. Seventy frozen cases cover all ten profiles and check complete AAL hashes/lengths against 0.4.0. New callback C APIs provide bounded working memory. Existing whole-buffer ABI remains available and still needs memory for its complete result.

发布流程另外要求在 Windows x64、macOS arm64、macOS Intel 上完成 **PCM 输入、AAL 文件及还原 PCM 均超过 1 GiB** 的实际往返，长度和 SHA-256 一致，CLI 峰值工作集/RSS 不超过 64 MiB。具体实测值以发行附件 `RELEASE-VALIDATION.json` 为准；这不是所有环境的内存保证。RF64 超过 4 GiB 的长度字段有边界检查，小 RF64 有完整往返测试；当前大文件验收约 1 GiB，不宣称已完整测试所有最大容量。

Publication also requires a real **>1 GiB input PCM, >1 GiB AAL and >1 GiB reconstructed PCM** round trip on all three native platforms, exact length/SHA-256, and CLI peak working set/RSS below 64 MiB. Actual observations are attached in `RELEASE-VALIDATION.json`, not a universal memory guarantee. RF64 >4 GiB length fields have boundary checks and small RF64 files have end-to-end tests; the current large-file acceptance is about 1 GiB, not every maximum capacity.

详见 / See [流式设计与接口](docs/STREAMING.md)、[输入规格](docs/INPUT-FORMATS.md)。

## AAL 两种模式 / Two AAL modes

- **High-Speed Transfer**：有损 ATRAC3/ATRAC3plus 核心＋无损增强层。**本项目支持此模式的十档 CD 规格配置。** A lossy ATRAC3/ATRAC3plus core plus lossless enhancement. **This project implements ten CD-format profiles of this mode.**
- **Standard**：不依赖有损基础层的独立无损模式。**本项目未实现；目前没有可核验的实际证据证明 RFC 定义的 Standard 模式曾在真实产品或服务中应用。** Standalone lossless without a lossy base layer. **Not implemented here; no verified actual evidence currently establishes product/service use of the RFC-defined Standard mode.**

AAL3 与 AALP 是第一种模式中使用不同核心的配置，并非上述两种模式。广播 **AAL for Broadcasting** 的实验记录也未证明与 RFC Standard 等价，不能用来宣称 Standard 已实际部署。定义依据为 [RFC 5584 §3 / §7.3](https://www.rfc-editor.org/rfc/rfc5584.html)。完整说明、技术资料和证据边界见 [模式与来源](docs/AAL-MODES.md)、[High-Speed Transfer 技术说明](docs/modes/HIGH-SPEED-TRANSFER.md)、[Standard 技术资料](docs/modes/STANDARD.md)。

AAL3 and AALP are different core configurations of the first mode, not the two modes above. The **AAL for Broadcasting** trial does not establish equivalence with RFC Standard or its deployment. Definitions follow [RFC 5584 §3 / §7.3](https://www.rfc-editor.org/rfc/rfc5584.html). See [modes and evidence](docs/AAL-MODES.md), [High-Speed Transfer notes](docs/modes/HIGH-SPEED-TRANSFER.md) and [Standard references](docs/modes/STANDARD.md).

## 项目说明 / About

本项目使用 AtracDEnc 的 ATRAC3 / ATRAC3plus 兼容编码实现，并在此基础上增加 AAL3 / AALP 封装、确定性整数预测、无损增强层、位流自校验、独立 AAL 解码和应用集成接口。上游归属、引用代码与修改范围见 [UPSTREAM.md](UPSTREAM.md) 和 [第三方声明](THIRD-PARTY-NOTICES.txt)。

This project uses AtracDEnc's compatible ATRAC3 / ATRAC3plus encoding implementations and adds AAL3 / AALP framing, deterministic integer prediction, lossless enhancement coding, bitstream verification, independent AAL decoding and application integration. See [UPSTREAM.md](UPSTREAM.md) and [third-party notices](THIRD-PARTY-NOTICES.txt) for attribution and the modification scope.

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

- 输入 / Input：**44,100 Hz、16-bit、双声道整数 PCM RIFF/WAV 或 RF64**；无固定 1 GiB 上限 / no fixed 1 GiB cap.
- 输出 / Output：未加密的 **OMA / AAL3 或 AALP**，以及包含配置、PCM / OMA SHA-256 和校验结果的 JSON 报告。Unencrypted **OMA / AAL3 or AALP**, plus a JSON report with profile, PCM / OMA SHA-256 and verification results.
- 编码器在保存前重新读取实际输出位流，逐样本核对 PCM；不覆盖已有输出。The encoder rereads its emitted bitstream and checks every PCM sample before saving; existing outputs are never overwritten.
- 解码 / Decoding：通过 `--decode` 将官方或本项目生成的未加密 AAL OMA/AA3 还原为 **44.1 kHz / 16-bit / 双声道 PCM WAV**，自动识别全部十档，输出 PCM SHA-256。Decode official or independently encoded unencrypted AAL to PCM WAV, with automatic detection of all ten profiles and a PCM SHA-256.
- 文件解码逐包输出，精确保留源采样长度；无固定 1 GiB 上限。File decoding emits PCM incrementally with exact source length and no fixed 1 GiB cap.
- 当前不包含重采样、位深转换、DRM、普通有损 ATRAC 解码或播放器传输。Resampling, bit-depth conversion, DRM, ordinary lossy ATRAC decoding and device transfer are not included.

## 快速开始 / Quick start

从发行包解压后，在程序所在目录执行。Extract a release package, then run these commands from its directory.

Windows：

```powershell
.\atracdenc-aal.exe --list-profiles
.\atracdenc-aal.exe --self-test-all
.\atracdenc-aal.exe --profile aalp-352 "input.wav" "output.oma"
.\atracdenc-aal.exe --profile aal3-132 "input.wav" "output.oma"
.\atracdenc-aal.exe --decode "input.oma" "decoded.wav"
```

macOS：

```bash
chmod +x atracdenc-aal
./atracdenc-aal --self-test-all
./atracdenc-aal --profile aalp-352 "input.wav" "output.oma"
./atracdenc-aal --decode "input.oma" "decoded.wav"
```

不指定配置时使用 `aalp-352`。也可使用 `--codec aalp --bitrate 352`；`352000` 同样有效。`--profile` 不与 `--codec` / `--bitrate` 混用。`Ctrl+C` 可取消编码或解码。解码不使用编码配置选项，详见 [DECODING.md](docs/DECODING.md)。

The default is `aalp-352`. `--codec aalp --bitrate 352` is equivalent; `352000` is also accepted. Do not combine `--profile` with `--codec` / `--bitrate`. Press `Ctrl+C` to cancel encoding or decoding. Decode mode uses no encoding profile options; see [DECODING.md](docs/DECODING.md).

Windows 的 `AtracDEnc-AAL-GUI.exe` 和 macOS 的 `AtracDEnc-AAL-GUI.app` 均为**简易测试图形界面**，直接链接同一引擎，支持文件队列、输出目录、十档核心选择、进度、取消和自检。WAV 自动编码为 AAL，OMA/AA3 自动解码为 WAV。输入要求直接显示在界面中。启动时按系统首选语言顺序选择中文或英文，无匹配时回退英文；可使用 `--lang auto|zh|en` 覆盖。重名输出自动加序号。

Both native **simple test GUIs** directly link the same engine, with a queue, output folder, ten core choices, progress, cancellation and self-test. WAV encodes to AAL; OMA/AA3 decodes to WAV. Input requirements are displayed in the window. System language preferences select Chinese or English, with English fallback and an optional `--lang auto|zh|en` override. Output collisions receive a numbered filename.

```powershell
.\AtracDEnc-AAL-GUI.exe --lang en
```

```bash
open AtracDEnc-AAL-GUI.app --args --lang zh
```

界面仅用于测试与基本文件转换；实际窗口显示和交互由用户测试。其他应用可以继续直接使用 C API。详见 [转换器操作与语言说明](docs/GUI.md)。

The interfaces are for testing and basic file conversion. Window appearance and interaction are left for user testing; other applications can use the C API. See [converter usage](docs/GUI.md).

## 与官方一致的范围 / Conformance scope

增强层按原版方法独立实现，包括声道去相关、固定预测、8 阶 LPC、分区 Rice 编码、95% 原样回退及填充。离线夹具包含本项目合成音频的完整残差与官方编码数据，可逐字节验证 **4583 个增强包**，无需 Sony 软件。

The enhancement stage independently reproduces channel decorrelation, fixed prediction, LPC8, partitioned Rice coding, the original 95% raw fallback and padding. Offline fixtures contain complete synthetic residuals and original encoded data for **4,583 enhancement packets**, requiring no Sony software.

基线验证覆盖全部十档：122 个正例经 Sony 解码后，2,205,346 个 int16 采样值与长度均一致；13 个拒绝用例通过。另有 70 组同输入、同核心实际码率的双向对照，两边均精确还原 PCM。当前更名版的构建、自检及输出一致性记录见 [验证说明](validation/README.md)。

Baseline validation covers all ten profiles: 122 positive cases decoded by Sony matched all 2,205,346 int16 samples and their lengths; 13 rejection cases passed. A further 70 same-input, same-actual-core-rate pairs reconstructed PCM exactly on both sides. See [validation notes](validation/README.md) for the branded build's checks and output-equivalence records.

**有损核心采用 AtracDEnc 的兼容实现，其编码决策不同于 Sony；完整文件不与官方逐字节相同，压缩体积也不保证相同。** 本项目的完整文件验收标准是 PCM 精确还原，增强层逐字节一致仅针对已记录的相同残差输入。

**The lossy core uses AtracDEnc's compatible implementations, whose encoding decisions differ from Sony's. Complete files are not Sony byte-identical, and compression sizes can differ.** Full-file acceptance is exact PCM reconstruction; enhancement byte equality refers to the recorded identical residual inputs.

0.3.0 解码验收记录见 [decoder-validation.json](validation/decoder-validation.json)；包含官方文件、自制文件和特殊核心语法的逐样本对照。Decoder acceptance includes official files, independent files and varied core syntax; see the linked record.

0.3.0 的 [GitHub CI](https://github.com/Solari0225/AtracDEnc-AAL/actions/runs/41380633560) 已通过 Windows x64、macOS arm64 和 macOS Intel 的原生构建及全部六项离线检查。Native builds and all six offline checks passed on all three platforms. See [CI record](validation/github-ci-v0.3.0.json).

以下为 0.2.0 的历史验证记录：Windows x64 已执行本地验证。[首次 GitHub CI](https://github.com/Solari0225/AtracDEnc-AAL/actions/runs/41318128062) 也已通过 Windows x64、macOS arm64 和 macOS Intel 的原生构建与完整离线测试。发行附件中的 macOS universal 二进制仍是交叉编译版本，最低 macOS 11；该预构建文件本身尚未在 Mac 上直接执行。NW-X1050 / X1060 的实机播放、跳转和传输数据库尚未验证。详细范围见 [CI 记录](validation/github-ci-2026-11-01.json)。

For the historical 0.2.0 release, the [first GitHub CI run](https://github.com/Solari0225/AtracDEnc-AAL/actions/runs/41318128062) passed native builds and all offline tests on Windows x64, macOS arm64 and macOS Intel, in addition to local Windows checks. The macOS universal release asset remains a cross-compiled binary targeting macOS 11 or later; that specific prebuilt file has not been directly executed on a Mac. Physical NW-X1050 / X1060 playback, seeking and transfer remain untested. See the [CI record](validation/github-ci-2026-11-01.json) for the exact scope.

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

其他应用可静态链接库，调用 [C 接口](include/independent_aal.h)：`aal_profile_count`、`aal_profile_get`、`aal_encode_cd16_profile`、`aal_encode_cd16`、`aal_free`、`aal_decode_cd16` 和 `aal_pcm_free`。现有 `aal_*` ABI 及头文件名称保留，默认配置为 `aalp-352`。示例见 [encode_buffer.c](examples/encode_buffer.c)，线程、内存和 CMake 集成说明见 [INTEGRATION.md](docs/INTEGRATION.md)。

Applications can statically link the library through the [C API](include/independent_aal.h): `aal_profile_count`, `aal_profile_get`, `aal_encode_cd16_profile`, `aal_encode_cd16`, `aal_free`, `aal_decode_cd16` and `aal_pcm_free`. Existing `aal_*` ABI identifiers and the header name are retained; the default is `aalp-352`. See [encode_buffer.c](examples/encode_buffer.c) and [INTEGRATION.md](docs/INTEGRATION.md).

## 许可证与致谢 / License and credits

项目采用 **LGPL-2.1-or-later**，与所使用的 AtracDEnc 代码保持一致。完整许可证见 [LICENSE](LICENSE)，上游及依赖的版权和许可证见 [CREDITS.md](CREDITS.md)、[UPSTREAM.md](UPSTREAM.md)、[第三方声明](THIRD-PARTY-NOTICES.txt) 和 [licenses/](licenses/)。

The project is licensed under **LGPL-2.1-or-later**, matching the AtracDEnc code used here. See [LICENSE](LICENSE), [CREDITS.md](CREDITS.md), [UPSTREAM.md](UPSTREAM.md), [third-party notices](THIRD-PARTY-NOTICES.txt) and [licenses/](licenses/).

感谢 AtracDEnc 项目及其贡献者提供 ATRAC 编码基础。Thank you to the AtracDEnc project and its contributors for the ATRAC encoding foundation.

[更新记录 / Changelog](CHANGELOG.md) · [贡献说明 / Contributing](CONTRIBUTING.md) · [发布说明 / Release notes](docs/releases/v0.5.0.md)
