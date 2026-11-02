# 输入规格 / Input requirements

本页适用于 Windows/macOS 简易测试转换器及命令行。图形界面和命令行调用相同的独立编解码引擎。

This applies to the Windows/macOS simple test converters and CLI. Both interfaces use the same independent codec engine.

## WAV → AAL

| 项目 / Item | 必须满足 / Required |
| --- | --- |
| 容器 / Container | Little-endian RIFF/WAVE，扩展名 `.wav` / extension `.wav` |
| 编码 / Encoding | 整数 PCM，WAVE format tag **1** / integer PCM, format tag **1** |
| 采样率 / Sample rate | **44,100 Hz** |
| 位深 / Bit depth | **16-bit signed** |
| 声道 / Channels | **2，交错双声道 / interleaved stereo** |
| 文件大小 / File size | **≤ 1 GiB = 1,073,741,824 bytes**，包含文件头 / including headers |
| 内容 / Content | 至少一个完整立体声采样帧 / at least one complete stereo sample frame |

不支持 24/32-bit、浮点 WAV、单声道、多声道、48/88.2/96/192 kHz、RF64 或 WAVE_FORMAT_EXTENSIBLE。MP3、FLAC、AAC、ALAC、AIFF 和 DSD 不能直接输入。**程序不会自动重采样、截断位深或混合声道；不符合规格时直接拒绝。** 如需使用这些音源，应在其他软件中明确转换为上表规格；该预处理不属于本项目的无损往返承诺。

24/32-bit, floating-point WAV, mono, multichannel, other sample rates, RF64 and WAVE_FORMAT_EXTENSIBLE are unsupported. MP3, FLAC, AAC, ALAC, AIFF and DSD are not direct inputs. **The app does not resample, reduce bit depth or mix channels: unsupported input is rejected.** Convert such sources explicitly in another application first; that preprocessing is outside this project's lossless round-trip guarantee.

输出为未加密 `.oma`，另写入 `output.oma.json` 校验报告。默认配置 `aalp-352`。352 kbps 表示 ATRAC3plus 核心码率，不是完整 AAL 的总码率。支持配置详见 [README](../README.md#支持配置--supported-profiles)。

Output is an unencrypted `.oma` plus an `output.oma.json` verification report. The default `aalp-352` uses a 352 kbps ATRAC3plus core; this is not the complete AAL bitrate. See the [profile list](../README.md#支持配置--supported-profiles).

## AAL → WAV

输入为 `.oma` 或 `.aa3` 内的**未加密 AAL3/AALP 双层 AAL**，对应本项目十档配置及 44.1 kHz / 16-bit / 双声道 PCM。扩展名本身不能证明文件是受支持的 AAL：普通有损 ATRAC、DRM 内容、Standard 模式及其他配置均不支持。

Input must contain **unencrypted two-layer AAL3/AALP**, with one of this project's ten profiles and 44.1 kHz / 16-bit / stereo PCM. An `.oma` or `.aa3` extension alone does not establish support. Ordinary lossy ATRAC, DRM, Standard mode and other configurations are unsupported.

输入文件和还原后的 PCM 数据**各自**上限为 1 GiB。输出是 44,100 Hz / signed 16-bit / 双声道整数 PCM WAV，保留精确采样长度；GUI 日志及 CLI 提供 PCM SHA-256。

The input file and decoded PCM data are **each** limited to 1 GiB. Output is 44,100 Hz / signed 16-bit / stereo integer PCM WAV with exact sample length; the GUI log and CLI report its PCM SHA-256.

## 限制的含义 / What these limits mean

1 GiB 是当前整文件内存处理实现的资源上限，**不是 AAL 格式上限**。44.1 kHz / 16-bit / 双声道是本项目已实现和验证的规格，不能外推为所有 AAL 模式的最大规格。规范、模式与证据边界见 [AAL-MODES.md](AAL-MODES.md)。

The 1 GiB guard belongs to the current whole-file memory implementation; **it is not an AAL format limit**. CD-format stereo is this project's implemented and tested scope, not a universal limit of every AAL mode. See [AAL-MODES.md](AAL-MODES.md).

两平台 GUI 使用相同的有界文件头预检查；完整合法性仍由引擎检查。输出重名时 GUI 自动加序号；CLI 和引擎拒绝覆盖已有音频或对应报告。取消操作会清理当前未发布的临时输出，已经完成的文件保留。

Both GUIs share a bounded header preflight; the engine performs complete validation. The GUIs choose a numbered output name on collisions. The CLI and engine refuse to replace existing audio or its report. Cancellation cleans up the current unpublished temporary output and retains completed files.
