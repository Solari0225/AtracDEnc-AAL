# Standard 技术资料与未确认项 / Technical references and unknowns

**本项目未实现 Standard 编码或解码。目前没有实际证据证明 RFC 定义的 Standard 模式曾在真实产品或服务中应用。** 这不等于证明它从未被实现；证据分类与来源见 [AAL-MODES.md](../AAL-MODES.md)。

**Standard encoding/decoding is not implemented. There is currently no verified actual evidence of product/service use of the RFC-defined Standard mode.** This is not proof that it was never implemented; see [evidence and sources](../AAL-MODES.md).

## RFC 能确认的内容 / What the RFC establishes

- RFC 5584 §3 将 Standard 无损格式描述为顺序单音频帧，与双层 High-Speed Transfer 分开。Standard lossless uses sequential single audio frames, separately from two-layer High-Speed Transfer.
- §7.3 规定原始采样率可为 24/32/44.1/48/64/88.2/96/176.4/192 kHz，帧长为 512/1024/2048 samples。These original sample rates and block lengths are listed transport parameters.
- Standard 的 `baseLayer` 参数为 0，表示没有有损基础层；不是音频总码率为零。`baseLayer=0` denotes no lossy base layer, not zero total audio bitrate.
- §7.8–7.9 提供 SDP 会话示例，而没有给出产品型号、部署报告或音频样本。SDP examples are provided, not a product, deployment report or audio sample.

主来源 / Primary source: [RFC 5584](https://www.rfc-editor.org/rfc/rfc5584.html). 其参考文献 [11] 指向 *ATRAC Advanced Lossless Standard Specification ver.1.1*, Sony, 2007。这里的文献标题中的 “Standard Specification” 意为“标准规范”，不能仅凭标题推断某个工具支持 **Standard mode**。

Reference [11] names Sony's 2007 codec specification. “Standard Specification” in that document title is not by itself a claim of **Standard-mode** support in any tool.

## 仍缺少的技术证据 / Missing technical evidence

本项目未取得 Standard 的完整码流语法、可核验样本及可用官方编解码入口，因而不宣称支持其预测器、系数选择、熵编码、位深上限、OMA 映射或所有声道配置。不能把双层增强层当作完整 Standard 位流，也不能通过删除基础层或修改 codec-ID 把 AAL3/AALP 变为 Standard。

This project lacks the full Standard bitstream syntax, a verifiable sample and a demonstrated official codec entry point. It therefore makes no implementation claim about its predictor, coefficient decisions, entropy coding, bit-depth maximum, OMA mapping or channel configurations. A two-layer enhancement payload cannot simply be treated as a complete Standard stream, and removing the core or changing a codec ID does not establish a Standard conversion.

Sony 组件内 `straight lossless`／文件头处理分支仅提供进一步研究入口；RTP 参数和容器字段也不能代替压缩算法与完整解码。当前解码器明确拒绝超出已支持配置的文件。

Sony-component `straight lossless`/header branches are research leads only. RTP parameters and container fields do not replace the compression algorithm or complete decoding. The decoder explicitly rejects configurations outside the implemented set.

## 广播试验的边界 / Broadcast-trial boundary

2008 年作业班报告记录的 **AAL for Broadcasting** 是一个真实软件试验线索，包含线性预测/熵编码框图、压缩性能测试及 TS 生成与实时播放。该名称不自动等于 RFC Standard；尚无已取得的对应码流、试验可执行文件或互操作结果可证明两者相同。不能据此宣称 Standard 已正式用于广播或已实现某个高规格上限。详细来源及转载限制见 [S4](../AAL-MODES.md#来源--sources)。

The 2008 **AAL for Broadcasting** report records a software experiment with a prediction/entropy-coding diagram, compression tests and TS playback. The name does not establish identity with RFC Standard. No acquired trial stream, executable or interoperability result proves equivalence. It does not establish Standard broadcast adoption or a verified Standard format maximum. See [S4 and its reproduction limits](../AAL-MODES.md#来源--sources).
