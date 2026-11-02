# High-Speed Transfer 技术说明 / Technical notes

**本项目支持的 AAL 模式。** 规范依据及来源编号见 [模式与证据](../AAL-MODES.md)。

**The AAL mode implemented here.** Source references are listed in [modes and evidence](../AAL-MODES.md).

## 结构和用途 / Structure and purpose

完整数据由 ATRAC3/ATRAC3plus 有损基础层和无损增强层构成。基础层可独立产生有损音频；完整无损重建需要两层。向只支持有损 ATRAC 的旧设备转移时，可提取已经编码好的核心，免去再次进行有损编码。支持完整 AAL 的设备则可还原原始 PCM。[S1 §3/§4.5][S2][S3]

The complete stream combines a lossy ATRAC3/ATRAC3plus base and a lossless enhancement. The base alone produces lossy audio; both layers are needed for lossless reconstruction. Extracting the existing core can avoid another lossy encode when transferring to an older ATRAC-only device. A complete AAL decoder reconstructs PCM.[S1 §3/§4.5][S2][S3]

RFC 5584 将原始采样率限定为 44,100 Hz；ATRAC3 基础层帧长 1024，ATRAC-X 基础层帧长 2048。RFC 列出的基础码率不是 SMLE、Walkman 或本项目逐档支持的证明。项目以实际验证的十档配置为准。

RFC 5584 specifies 44,100 Hz original sampling, 1024 samples for the ATRAC3-based mode and 2048 for ATRAC-X. Its bitrate list does not establish support in a particular SMLE version, Walkman or this project. This implementation exposes only its ten verified profiles.

## 本项目的数据路径 / This implementation

1. 读取符合 [输入规格](../INPUT-FORMATS.md) 的 PCM WAV，用 AtracDEnc 的兼容实现生成所选有损核心。Read compliant WAV and encode the selected compatible AtracDEnc core.
2. 从核心位流计算 AAL 所需的确定性整数预测，计算与原始 PCM 的精确残差。Derive the AAL-specific deterministic integer prediction from the core and compute the exact PCM residual.
3. 对残差进行声道去相关、固定预测或 LPC8、分区 Rice 编码、原样回退和填充。Compress residuals with decorrelation, fixed/LPC8 prediction, partitioned Rice coding, raw fallback and padding.
4. 写入未加密 OMA/BLK 封装；读取实际输出位流并逐样本自校验后，发布音频与 JSON 报告。Frame unencrypted OMA/BLK, reread the emitted stream, verify every PCM sample and publish audio plus its report.
5. 解码时读取完整核心语法，计算整数预测并合并增强残差，按记录的长度去掉预滚和尾部填充，写入 PCM WAV。Decode complete core syntax, combine the integer prediction with residuals, trim the recorded pre-roll/tail and write PCM WAV.

**AAL 的整数预测器不能用普通有损解码输出替换。** 项目仅提供完整 AAL→PCM；没有将本说明中的核心提取、RTP 流传输或 Walkman 数据库转移实现为工具/API。

**Ordinary perceptual lossy-decoder output cannot replace the AAL integer predictor.** The tool exposes complete AAL→PCM; core extraction, RTP streaming and Walkman database transfer described as possible applications above are not implemented tool/API features.

## 代码与验证 / Code and verification

- 配置 / Profiles: [`src/codec_profiles.hpp`](../../src/codec_profiles.hpp).
- 编码 / Encoding: [`src/encoder.cpp`](../../src/encoder.cpp), [`compression/`](../../compression/).
- 完整解码 / Complete decoding: [`src/decoder.cpp`](../../src/decoder.cpp), [`core/decode/`](../../core/decode/), [解码说明](../DECODING.md).
- 已记录的增强层、完整文件与原生平台验证 / Recorded conformance: [`validation/README.md`](../../validation/README.md).
- 更详细的研究笔记 / Detailed research notes: [配置与预测](../AAL-profiles-and-prediction.txt), [AAL3 增强格式](../AAL3-enhancement-format.txt), [LPC 分析](../LPC-analysis.txt).

完整输出不保证与 Sony 逐字节相同或达到完全相同体积；验收目标为源 PCM 精确还原。物理 Walkman 兼容性仍由实机测试决定。

Complete output is not guaranteed Sony byte-identical or equal in size. Acceptance is exact source PCM reconstruction. Physical Walkman compatibility still requires device testing.
