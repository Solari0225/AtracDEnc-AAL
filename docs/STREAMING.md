# 流式编解码 / Streaming codec

## 设计 / Design

0.5.0 借鉴 FFmpeg 的持续送入/取出和 libFLAC 的流式回调生命周期，重新组织本项目已有的独立 AAL 算法。没有引入 FFmpeg/libFLAC 依赖，也没有复制其编码算法；十档核心与增强压缩方法保持。

Version 0.5.0 adopts the lifecycle ideas of FFmpeg's ongoing input/output and libFLAC's streaming callbacks to reorganize this project's existing independent AAL algorithms. No FFmpeg/libFLAC dependency or codec algorithm is incorporated; the ten cores and enhancement methods are retained.

参考 / References: [FFmpeg send/receive API](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/avcodec.h), [FFmpeg transcode example](https://ffmpeg.org/doxygen/trunk/transcode_8c_source.html), [libFLAC stream callbacks and verify mode](https://xiph.org/flac/api/group__flac__stream__encoder.html), [FFmpeg RIFF/RF64 output](https://ffmpeg.org/ffmpeg-formats.html#toc-wav).

| 阶段 / Stage | 保留的数据 / Retained data |
| --- | --- |
| 输入 / Input | 当前 PCM 帧及有界对齐队列 / current PCM frame and bounded alignment queue |
| 编码 / Encode | 连续 ATRAC 核心、整数预测状态、当前残差与当前包 / persistent core/predictor state, current residual and packet |
| 自校验 / Verify | 独立解析序列化包、另一套预测状态、当前还原 PCM / independent serialized-packet parser/predictor and current PCM |
| 解码 / Decode | 当前 AAL 包、连续整数预测状态、当前 PCM / current packet, persistent integer prediction and current PCM |
| 摘要 / Hash | SHA-256 状态及最多一个待填块 / SHA-256 state and a partial block |
| 文件输出 / File output | 同目录独占临时文件，逐块写入 / exclusive same-directory temporary file, incremental writes |

PCM 队列硬边界为 32 个核心输入块；现有核心的前瞻/输出延迟在此范围内。AALP 单块 PCM 为 8 KiB，因此该队列最多约 256 KiB。还需编解码状态、工作区、运行库及系统 I/O 缓冲；这个数字不是进程总内存。CLI/GUI 不再保存整段核心列表、完整残差、完整预测或完整输出文件。

The PCM queue is bounded to 32 core-input blocks, covering the existing cores' lookahead/output delay. An AALP PCM block is 8 KiB, so this queue is at most about 256 KiB. Codec state, work areas, libraries and I/O buffering add overhead; this is not total process memory. CLI/GUI retain no whole-track core list, residual, prediction or output-file buffer.

每个生成包先从实际序列化字节独立解码，逐样本比较仍在队列中的原始 PCM，才交给输出。有效采样数、priming/flush、AAL3 138 / AALP 368 的对齐和末包余数保持；结束时检查全部样本和增量摘要。文件失败/取消清理临时结果。已有 70 项 0.4.0 摘要回归覆盖十档与边界长度。

Every packet is independently decoded from its serialized bytes and compared sample-for-sample with queued source PCM before output. Valid sample counts, priming/flush, AAL3/AALP delay alignment and final remainders are preserved. Completion checks total samples and incremental digests. File failures/cancellation remove temporary results. Seventy frozen 0.4.0 whole-file hashes cover all ten profiles and boundary lengths.

## 回调 C API / Callback C API

完整声明见 [independent_aal.h](../include/independent_aal.h)。函数在调用线程同步执行，输入总长度必须已知。

See the [public header](../include/independent_aal.h). Calls are synchronous on the caller's thread and require a known total input length.

- `aal_encode_cd16_stream`：已知 PCM 帧数、配置、PCM 读取回调及 AAL 字节写入回调。Known PCM frame count, profile, PCM reader and AAL byte writer.
- `aal_decode_cd16_stream`：已知 AAL 字节数、字节读取回调及 PCM 写入回调。Known AAL byte count, byte reader and PCM writer.
- PCM 数量单位是交错 L/R 对，不是单声道样本数。读取可返回较短块；0 表示 EOF，超过容量或 SIZE_MAX 表示错误。PCM counts are interleaved stereo frames. Short reads are accepted; zero is EOF, and an oversized count/SIZE_MAX is an error.
- 写回调成功返回 0；非零导致 I/O 失败。进度回调非零表示取消。A writer returns zero on success; nonzero is an I/O failure. A nonzero progress result requests cancellation.
- 回调缓冲只在该次回调内有效，不能保存地址；不同流可使用独立上下文。Callback buffers are borrowed for that call only; independent streams use separate contexts.
- 流式输出失败/取消时可能已写入前缀，宿主必须丢弃结果；结果结构仅在成功时赋值。On failure/cancellation, a prefix may have been written: discard it. The result structure is assigned only on success.
- 旧 `aal_encode_cd16_profile` / `aal_decode_cd16` ABI 保留且取消固定 1 GiB 限制；由于仍返回完整缓冲，其内存随结果大小增长。Legacy ABI remains with no fixed cap, but whole results still require proportional memory.

最小 C 调用示例 / Minimal C integration: [stream_callbacks.c](../examples/stream_callbacks.c). 实际 C 编译与往返检查见 [check_stream_c_api.c](../tests/check_stream_c_api.c)。

流式接口按顺序解码，可用于向播放器持续提供 PCM；随机跳转和无损 seek 索引未在此版本实现。宿主若把所有回调输出积累在内存中，其内存仍会增长。

Sequential decode callbacks can continuously supply PCM to a player. Random seeking/indexing is not implemented here. A host that accumulates all callback output still consumes proportional memory.

## 验证边界 / Validation scope

发布前要求三平台原生九项检查，以及真实大于 1 GiB 的输入、压缩文件和还原 PCM 往返，长度/摘要精确一致。大文件工具限制 CLI 峰值工作集/RSS ≤64 MiB，具体结果记录在发行附件；不将该阈值宣称为所有系统/GUI 的保证。RF64 >4 GiB 当前有头部边界测试，小 RF64 有完整往返；本轮大文件完整验收约 1 GiB。

Publication requires nine native checks and a real >1 GiB input/AAL/PCM round trip on all three platforms, with exact length/digest. The large-file tool requires peak CLI working set/RSS ≤64 MiB and attaches observations; it is not a universal GUI/system guarantee. RF64 >4 GiB fields have boundary tests and small RF64 files have full round trips; current complete large-file acceptance is about 1 GiB.

压缩算法保持不代表与 Sony 完整文件一致；相同残差增强层的逐包一致与完整 Sony 编码决策是不同验收范围。Standard 模式、其他 PCM 规格和实机兼容性边界保持，见 [AAL-MODES.md](AAL-MODES.md)。

Preserving this project's compression does not establish complete Sony byte/size equality. Same-residual enhancement conformance and full Sony encoding decisions remain separate scopes. Standard, other PCM specifications and device validation are unchanged; see [modes](AAL-MODES.md).
