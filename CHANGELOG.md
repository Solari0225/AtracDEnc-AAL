# 更新记录 / Changelog

## 0.4.0 — 2026-11-02

- 新增 macOS AppKit 原生简易测试文件转换器，配套 Windows Win32 版本；两者直接链接同一核心并中英文自适应。Added a native AppKit converter alongside the Win32 converter, sharing the codec and automatic Chinese/English selection.
- 界面与文档明确 WAV 的 44.1 kHz / signed16 / 双声道整数 PCM、未加密双层 AAL 及 1 GiB 限制；无自动格式降级。Displayed strict input requirements and resource limits; no automatic conversion of unsupported PCM formats.
- 两平台共享文件头预检查与防覆盖命名，新增实际文件往返、Unicode 路径、不支持规格与取消检查。Shared bounded preflight and collision-safe naming, with complete-file, Unicode-path, rejection and cancellation checks.
- 新增双语 AAL 两种模式综述、分模式技术资料与来源；明确仅支持 High-Speed Transfer，Standard 未实现且无已核验产品/服务应用证据。Added bilingual mode/evidence documentation; only High-Speed Transfer is implemented and Standard deployment is unverified.
- 编解码算法及 C ABI 不变；不新增 Sony 运行时依赖。Codec algorithms and C ABI are unchanged; no Sony runtime dependency is introduced.

## 0.3.0 — 2026-11-02

- 新增不依赖 Sony 或第三方音频组件的 AAL3 / AALP 独立解码器，支持全部十档 44.1 kHz / 16-bit / 双声道未加密文件。Independent, component-free decoding for all ten unencrypted CD-format AAL3 / AALP profiles.
- CLI 新增 `--decode` / `-d`，输出 PCM WAV、精确样本长度和 PCM SHA-256。CLI decode mode writes PCM WAV and reports exact sample length and hash.
- C API 新增 `aal_decode_cd16`、`aal_pcm_buffer`、`aal_pcm_free`，保留编码 API ABI。New decoder C API preserves the encoder ABI.
- 逆向并接入官方核心的增益、窗口、音调语法、声道信息和跨帧整数预测状态。Integrated reconstructed gain, windows, tonal syntax, stereo handling and stateful integer prediction.
- 简易测试 GUI 自动按输入类型进行 WAV→AAL 或 AAL→WAV，继续提供中英文自适应；窗口操作由用户测试。The bilingual test GUI routes WAV→AAL and AAL→WAV automatically; visual testing remains with the user.
- 增加官方合成音频的完整文件解码夹具、C ABI / 取消 / 错误输入测试，以及双语解码与集成文档。Added complete-file synthetic oracle fixtures, C ABI/cancellation/malformed-input tests and bilingual documentation.

## 0.2.0 — 2026-11-01

- 正式命名为 **AtracDEnc-AAL**，明确标注基于 AtracDEnc 项目开发。Named **AtracDEnc-AAL**, with explicit AtracDEnc attribution.
- 从已经验证的私有基线 Independent AAL 0.2 整理为公开仓库；更名不改变编码算法。Prepared a public repository from the validated Independent AAL 0.2 baseline; branding does not change the encoding algorithms.
- 提供十种 AAL3 / AALP 配置，默认 AALP / ATRAC3plus 352 kbps。Ten AAL3 / AALP profiles, defaulting to AALP / ATRAC3plus 352 kbps.
- 提供命令行、保留兼容标识的 C ABI 和 Windows 测试 GUI。CLI, existing C ABI identifiers and a Windows test GUI.
- Windows 界面标注为简易测试图形界面，按系统显示语言自动使用中文或英文，并支持 `--lang auto|zh|en`。The Windows interface is labeled as a simple test GUI, adapts to Chinese/English display preferences and accepts `--lang auto|zh|en`.
- 加入中英文 README、上游来源、构建与集成文档、贡献模板、GitHub CI、发行打包工具及校验记录。Bilingual README, upstream provenance, build/integration guides, contribution templates, GitHub CI, release packaging and verification records.
- 保留 4583 个官方增强包的离线数据夹具，并清理公开资料中的本机路径。Retained offline fixtures for 4,583 official enhancement packets and normalized local paths in public metadata.

完整文件与 Sony 官方字节不一致；历史版本中的验证范围以各版本记录为准。GUI 视觉交互与 Walkman 实机验证仍由用户执行。Full files are not Sony byte-identical. Historical validation follows each release record; GUI interaction and physical Walkman testing remain with the user.
