# 输入规格 / Input requirements

适用于 Windows/macOS 简易测试转换器与命令行。0.5.0 起，文件编码、解码使用有界流式缓冲，移除固定 1 GiB 上限。

Applies to the native Windows/macOS test converters and CLI. Since 0.5.0, file encoding/decoding uses bounded streaming buffers with no fixed 1 GiB cap.

## WAV → AAL

| 项目 / Item | 必须满足 / Required |
| --- | --- |
| 容器 / Container | Little-endian RIFF/WAVE 或 RF64，扩展名 `.wav` / RIFF/WAVE or RF64, extension `.wav` |
| 编码 / Encoding | 整数 PCM，WAVE format tag **1** / integer PCM, format tag **1** |
| 采样率 / Sample rate | **44,100 Hz** |
| 位深 / Bit depth | **16-bit signed** |
| 声道 / Channels | **2，交错双声道 / interleaved stereo** |
| 内容 / Content | 至少一个完整立体声采样帧 / at least one complete stereo sample frame |
| 文件大小 / File size | **无固定 1 GiB 上限**；64 位字节/采样计数，检查容器字段和实际文件长度 / **no fixed 1 GiB cap**, checked 64-bit counters and container fields |

不支持 24/32-bit、浮点 WAV、单声道、多声道、其他采样率或 WAVE_FORMAT_EXTENSIBLE。MP3、FLAC、AAC、ALAC、AIFF 和 DSD 不能直接输入。程序不会自动重采样、截断位深或混合声道；不符合规格时直接拒绝。其他软件中的格式预处理不属于本项目的无损往返承诺。

24/32-bit, float, mono, multichannel, other sample rates and WAVE_FORMAT_EXTENSIBLE are unsupported. MP3, FLAC, AAC, ALAC, AIFF and DSD are not direct inputs. There is no resampling, bit-depth reduction or channel mixing. External preprocessing is outside this project's lossless round-trip guarantee.

RF64 必须含有效的首个 `ds64` 块，PCM 数据使用其 64 位长度；检查 sample count、RIFF 长度、块边界和填充。额外元数据块若使用 0xffffffff 扩展长度，目前不支持，明确拒绝；普通长度元数据可跳过。RF64 小文件有完整往返测试，超过 4 GiB 的长度字段有边界检查；当前发布的大文件端到端验收约 1 GiB。

RF64 requires a valid first `ds64` chunk. Its 64-bit data size, sample count, RIFF extent, chunk bounds and padding are checked. Additional metadata chunks using extended 0xffffffff sizes are explicitly unsupported; ordinary-sized metadata can be skipped. Small RF64 files have end-to-end tests and >4 GiB fields have boundary checks; current complete large-file acceptance is about 1 GiB.

输出为未加密 `.oma` 和 `.oma.json` 校验报告；默认 `aalp-352`。352 kbps 是有损核心码率，不是 AAL 总码率。见 [十档配置](../README.md#支持配置--supported-profiles)。

Output is unencrypted `.oma` plus a verification report. The default `aalp-352` describes the core rate, not total AAL bitrate. See the [ten profiles](../README.md#支持配置--supported-profiles).

## AAL → WAV

输入为未加密 AAL3/AALP 双层 OMA/AA3，十档配置，44.1 kHz / 16-bit / 双声道。普通有损 ATRAC、DRM、Standard 及其他规格不支持。扩展名本身不能证明格式正确。

Input must contain unencrypted two-layer AAL3/AALP in one of the ten CD-format profiles. Ordinary lossy ATRAC, DRM, Standard and other specifications are unsupported; an extension alone does not establish support.

输入和 PCM 无固定 1 GiB 上限。输出保留精确样本数和 PCM SHA-256。PCM 与文件头可由普通 RIFF 的 32 位长度字段表示时写普通 WAV；否则自动使用 RF64，扩展名仍为 `.wav`。大 RF64 输出需要接收软件支持该容器。

There is no fixed 1 GiB input/PCM cap. Exact sample length and PCM SHA-256 are preserved. Output uses ordinary RIFF when its 32-bit lengths suffice, otherwise RF64, still named `.wav`. Consumers of large output must support RF64.

## 容量边界 / Capacity boundaries

移除 1 GiB 不等于无限容量。当前 OMA 写入与解码检查 32 位 BLK 计数、32 位毫秒时长字段和 64 位偏移，不能表示时明确拒绝溢出。RIFF 自身约 4 GiB 的长度边界由 RF64 处理。可用磁盘、文件系统和宿主地址空间也影响实际容量；这些检查不是 AAL 所有模式的统一最大规格。

Removing 1 GiB does not imply unlimited capacity. This implementation checks OMA's 32-bit packet-count/millisecond-duration fields and 64-bit offsets, rejecting unrepresentable values. RF64 handles RIFF's approximately 4 GiB size boundary. Disk space, filesystem and host addressability still apply; these are not universal maximum specifications for all AAL modes.

旧 C API 返回完整缓冲区，需要足够内存容纳输入/输出；新回调接口与 CLI/GUI 文件流程才是有界工作内存路径。见 [STREAMING.md](STREAMING.md) 和 [集成说明](INTEGRATION.md)。

Legacy C APIs return complete buffers and need corresponding memory. New callback APIs and CLI/GUI file conversion provide bounded codec working memory. See [streaming](STREAMING.md) and [integration](INTEGRATION.md).

GUI 自动为重名输出加序号；CLI/引擎拒绝覆盖。文件流程仅在完整处理和校验成功后发布结果；失败或取消清理本次未发布临时文件，保留之前已完成的文件。

The GUI numbers colliding names; CLI/engine reject replacement. File conversion publishes only after successful processing/verification, cleans up unpublished temporary output on error/cancellation, and retains previously completed files.
