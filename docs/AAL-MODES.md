# AAL 两种模式与证据 / AAL modes and evidence

调查日期 / Research date: **2026-11-02**.

**本项目仅支持 High-Speed Transfer 双层 AAL，不支持 Standard 模式。AAL3 与 AALP 是双层模式使用不同有损核心的两种配置，不等于 AAL 的两种模式。**

**This project supports only High-Speed Transfer two-layer AAL. Standard mode is unsupported. AAL3 and AALP select different lossy cores within the two-layer mode; they are not the two AAL modes.**

## 模式概览 / Overview

| 项目 / Item | High-Speed Transfer | Standard |
| --- | --- | --- |
| 结构 / Structure | 有损基础层＋无损增强层 / lossy base + lossless enhancement | 顺序单音频帧的独立无损格式 / standalone lossless, sequential single audio frames |
| 有损核心 / Lossy core | ATRAC3 或 ATRAC3plus（RFC 称 ATRAC-X） / ATRAC3 or ATRAC3plus (ATRAC-X in the RFC) | 不依赖有损基础层 / no lossy base layer |
| 无损还原 / Lossless reconstruction | 需要完整基础层和增强层 / requires both layers | 使用完整 Standard 数据 / uses the complete Standard stream |
| 兼容用途 / Compatibility | 可提取核心供旧设备有损播放 / extract the core for lossy playback on older devices | 没有可直接提取的 ATRAC3/plus 核心 / no embedded ATRAC3/plus core to extract |
| RFC 5584 原始采样率 / Original sample rates | 44,100 Hz | 24,000 / 32,000 / 44,100 / 48,000 / 64,000 / 88,200 / 96,000 / 176,400 / 192,000 Hz |
| RFC 帧长 / Block lengths | ATRAC3: 1024；ATRAC-X: 2048 samples | 512 / 1024 / 2048 samples |
| 本项目 / This project | **支持已验证的十档 CD 规格 / ten verified CD-format profiles** | **未实现 / not implemented** |

模式结构来自 RFC 5584 §3，参数来自 §7.3。[S1] 两种模式在正确编码、完整解码的条件下都以恢复原始 PCM 为目标；不能将 Standard 名称解释为更高音质。RTP 的 `baseLayer=0` 也不是“文件总码率为零”，更不等于将 TraConv 的 `BitRate` 参数设为零即可启用 Standard。RFC 的多会话双层示例中，依赖基础层的增强会话也可能使用零值；必须结合会话依赖关系判断。

Structure follows RFC 5584 §3 and parameters follow §7.3.[S1] Both modes aim at exact PCM reconstruction when correctly encoded and fully decoded. Standard is not an inherently higher-quality setting. RTP `baseLayer=0` does not mean zero total bitrate or a working TraConv `BitRate=0` switch. An enhancement session in the RFC's dependent two-session examples may also use zero; session dependencies matter.

分模式技术说明 / Technical notes: [High-Speed Transfer](modes/HIGH-SPEED-TRANSFER.md) · [Standard](modes/STANDARD.md).

## 本项目实际支持范围 / Implemented scope

- 编码、完整无损解码：AAL3 / ATRAC3 **132 kbps**；AALP / ATRAC3plus **48、64、96、128、160、192、256、320、352 kbps**。默认 AALP 352。所有数字都是**核心**码率。
- 仅未加密的 OMA/AA3，以及 44.1 kHz / signed 16-bit / 双声道整数 PCM。输入限制、拒绝规则与 1 GiB 资源上限见 [输入规格](INPUT-FORMATS.md)。
- Windows/macOS CLI、C API 和简易测试文件转换器共用独立引擎。没有 Sony 运行时依赖；没有 Standard、RTP 传输、DRM、普通有损 ATRAC 解码、独立核心提取或设备传输 API。
- 完整文件以精确 PCM 往返验收；有损核心的编码决策及完整文件大小不保证与 Sony 相同。增强层逐字节对照仅适用于已记录的相同残差输入，见 [验证记录](../validation/README.md)。

Encoding and complete lossless decoding cover AAL3 / ATRAC3 at **132 kbps**, and AALP / ATRAC3plus at **48, 64, 96, 128, 160, 192, 256, 320 and 352 kbps**; the default is AALP 352. These are **core** rates. Only unencrypted CD-format stereo is supported. The CLI, C API and both native converters share the independent engine. Standard, RTP transport, DRM, ordinary lossy ATRAC decoding, a core-extraction API and device transfer are not implemented. Complete-file acceptance is exact PCM reconstruction, not Sony byte or compression-size equality; see the [validation record](../validation/README.md).

## Standard 是否实际应用过 / Was Standard deployed?

**截至上述调查日期，尚无已取得、可核验的实际证据证明 RFC 5584 所定义的 AAL Standard 模式曾在真实产品或服务中实际应用过。** 本项目没有可核验的官方 Standard 文件样本、已打通的官方 Standard 编解码入口或已确认采用该模式的量产产品。这个结论描述当前证据状态，不能改写成“Standard 从未被实现或内部使用”。

**As of the research date, no acquired, verifiable evidence establishes actual use of the AAL Standard mode defined in RFC 5584 in a real product or service.** This project has no verified official Standard audio sample, demonstrated official Standard encode/decode entry point, or confirmed shipping product using that mode. This is an evidence limit, not proof that Standard was never implemented or used internally.

| 依据 / Evidence | 能说明什么 / What it establishes | 不能说明什么 / What it does not establish |
| --- | --- | --- |
| RFC 5584，2009，Sony 作者参与编写 [S1] | Standard 的明确参数、帧结构概述与 RTP/SDP 示例 / explicit Standard parameters and transport examples | 具体产品、可运行编解码器或部署记录 / a working codec, named product or deployment |
| Sony SonicStage 3.3 说明、Walkman NW-ZX100 规格 [S2][S3] | ATRAC3/plus 基础层、基础码率与高速转移用途 / base layers, core rates and fast transfer | Standard 支持 / Standard support |
| 2008-06-16 音声符号化方式作业班报告副本 [S4] | AAL for Broadcasting 软件编解码与 TS 实验记录 / recorded software codec and TS experiments | 与 RFC Standard 位流的等价或互通 / equivalence or interoperability with RFC Standard |
| Pioneer Cyber Navi 产品页 [S5] | AAL 的量产 CD→HDD 录音应用 / shipping AAL CD-to-HDD recording | 具体采用哪种 AAL 模式；菜单“标准”实际为 ATRAC3 / which AAL mode; its “standard” recording menu means ATRAC3 |
| Sound it! 5.0 Mac 产品页 [S6] | 第三方软件声明支持 AAL/AA3 / third-party software advertises AAL/AA3 | 明确的 Standard 实现 / an explicitly identified Standard implementation |

**广播试验必须单独说明。** [S4] 记载 2008-06-13 在 ARIB 会议室进行实验，参与方包括 Sony、NTT、NHK，使用软件 AAL 编解码器进行 TS 生成、实时分离/复号/播放和输入输出一致性检查。其框图采用线性预测及熵编码，属于独立无损路线的技术线索；报告使用的是 **AAL for Broadcasting** 名称，并未证明其与 RFC Standard 相同。报告将 MPEG-4 ALS 列为第一候选、AAL 列为第二候选。不能把这次试验写成 Standard 已商用或已被广播服务采用。[S4]

**The broadcast trial is a separate finding.** [S4] records a 2008-06-13 ARIB experiment involving Sony, NTT and NHK, with software AAL codecs, TS generation, real-time demultiplexing/decoding/playback and input/output checks. The linear-prediction/entropy-coding diagram suggests a standalone lossless approach, but the document calls it **AAL for Broadcasting** and does not demonstrate identity with RFC Standard. MPEG-4 ALS was the first candidate and AAL the second. This trial is not evidence of Standard commercialization or broadcast-service adoption.[S4]

本机 Sony 组件分析中的 `straight lossless` 参数与 `0x20` 文件头分支仅为实现线索；文件头读写支持、字符串和枚举值不能代替完整编解码执行证据。测试中的非支持 codec-ID 拒绝用例是故意构造的无效头，**不是 Standard 合法样本**。

The locally observed `straight lossless` parameter handling and `0x20` header branch in Sony components are implementation clues only. Header support, strings and enum values do not establish a working codec. The converter's unsupported-codec rejection fixture is a deliberately invalid header, **not a valid Standard audio sample**.

## 来源 / Sources

- **[S1]** [RFC 5584](https://www.rfc-editor.org/rfc/rfc5584.html), July 2009: §3, §4.5, §7.3, §7.8–7.9. 它是 RTP 负载规范，不是完整音频压缩算法源码或码流语法规范；文献 [11] 引用 Sony 的 *ATRAC Advanced Lossless Standard Specification ver.1.1*（2007），本项目未取得可公开提供的全文。This is a transport specification, not a full codec implementation or bitstream syntax specification. Its reference [11] names Sony's 2007 codec specification; this project has no distributable full copy.
- **[S2]** Sony: [What is ATRAC Advanced Lossless?](https://www.sony.com.au/electronics/support/articles/S500024368). SonicStage 3.3、ATRAC3/plus 基础层与兼容说明 / base-layer compatibility explanation.
- **[S3]** Sony: [NW-ZX100 playable file types](https://helpguide.sony.net/dmp/nwzx100/v1/ja/contents/TP0000905840.html). AAL 44.1 kHz、基础码率及注释 *4 / AAL base rates and fast-transfer footnote.
- **[S4]** 音声符号化方式作業班: *高度衛星デジタル放送 音声符号化方式 報告書案（技術的条件）*, 2008-06-16, pp.12, 26–30, 39–42: [报告转载副本 / reproduced report](https://123deta.com/document/zwvegolq-ba-bb-fb-eae-dc-c-a-bc.html). 本次可读取的是第三方转载，未取得原始官方 PDF 或试验可执行文件；不将转载网站上传时间当作报告日期。A third-party reproduction was available; the original official PDF and trial executables were not acquired. The host's upload date is not the report date.
- **[S5]** Pioneer: [AVIC-ZH0777 music server](https://jpn.pioneer/ja/carrozzeria/carnavi/cybernavi/avic-zh0777_line/av/msv.php). AAL 高音质录音与 ATRAC3 标准录音 / AAL high-quality vs ATRAC3 standard recording.
- **[S6]** INTERNET Co.: [Sound it! 5.0 for Macintosh](https://www.ssw.co.jp/products/sit/mac/sit50m/). AA3/AAL 支持声明；通用软件的 24-bit/192 kHz 能力不能直接外推为 AAL 支持规格。Generic application-wide 24-bit/192 kHz capabilities do not establish those AAL capabilities.

这些来源可证实规范内容、产品声明和报告所记载的试验；它们不取代实际文件、编解码器执行和互操作验证。

These sources establish specifications, product claims and recorded experiments; they do not replace actual samples, codec execution and interoperability verification.
