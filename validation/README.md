# 验证记录 / Validation records

本目录区分已经完成的基线编码验收与当前更名发行版的检查。Sony 仅是外部参考工具，不是项目的运行依赖；项目、CI 和离线测试均不分发或调用 Sony 程序。

This directory distinguishes predecessor encoding conformance from checks on the current renamed release. Sony was an external reference, not a runtime dependency. The project, CI and offline tests neither distribute nor invoke Sony programs.

## 0.5.0 流式实现 / Streaming implementation

[流式回归清单](../tests/streaming-vectors.json) 与 [冻结摘要](../tests/streaming_vectors.hpp) 包含十档 70 个整数合成 PCM 边界用例，要求编码完整 AAL 的长度与 SHA-256 保持 0.4.0 基线，并完整解码比较 PCM。新增 C/C++ 分片回调、已知大长度准入、溢出、I/O 失败、文件中途取消及临时文件清理、RF64 完整往返和长度边界检查。九项离线检查全部为无窗口测试。

The streaming regression list and frozen hashes cover 70 integer-generated PCM boundary cases across all ten profiles. Complete AAL length/SHA-256 must match the 0.4.0 baseline, and reconstructed PCM must match exactly. Checks also cover fragmented C/C++ callbacks, admission of large declared lengths, overflow, I/O failure, mid-file cancellation/temporary cleanup, RF64 round trips and length fields. All nine offline checks are headless.

[0.5.0 私有 Sony 参考摘要](streaming-sony-reference-v0.5.0.json) 记录 Windows 下十档各 8443 帧：Sony 解码新文件、项目解码 Sony 文件均精确还原长度和 PCM；同输入新文件仍与 0.4.0 相同。Sony 程序只用于开发者外部验证，不进入仓库、产品、公开测试或 CI。

The private Sony-reference summary covers ten 8443-frame cases on Windows: Sony decodes the new output exactly, and the new decoder restores Sony output exactly, including length. Same-input complete bytes remain equal to 0.4.0. Sony software is an external development reference only.

发行附件 `RELEASE-VALIDATION.json` 和原生包 `BUILD-INFO.json` 保存该次源提交的实际 CI 结果；发布门槛包括 Windows x64、macOS arm64、macOS Intel 各自完成真实 >1 GiB PCM → >1 GiB AAL → >1 GiB PCM，并校验长度/SHA-256、CLI 峰值工作集/RSS ≤64 MiB。实测值仅说明指定进程与用例，不含 GUI、系统文件缓存或实机。RF64 >4 GiB 目前验证头部字段和小文件往返，本轮完整大文件约 1 GiB。设计与重现步骤见 [STREAMING.md](../docs/STREAMING.md)、[BUILDING.md](../docs/BUILDING.md)。

Release validation and native package metadata retain actual results for the checked commit. Publication requires all three native platforms to complete real >1 GiB PCM/AAL/PCM round trips with exact lengths/hashes and peak CLI working set/RSS ≤64 MiB. These observations concern the measured processes/cases, excluding GUI, system file caches and hardware. RF64 >4 GiB fields and small complete RF64 files are tested; the complete large-file case is about 1 GiB. See the linked design and reproduction instructions.

## 0.3.0 解码器 / Decoder

[0.3.0 GitHub CI](github-ci-v0.3.0.json)：Windows x64、macOS arm64 和 macOS Intel 原生构建及全部六项离线检查通过。All three native builds and all six offline checks passed.

[编码回归](encoder-regression-v0.3.0.json)：最终程序重新编码 122 例，AAL 字节与旧版相同；另验证 Unicode 路径、已有输出保护及错误处理。All 122 re-encoded AAL files match the prior version byte-for-byte; CLI path and failure behavior checks also passed.

[decoder-validation.json](decoder-validation.json)：469 例完整文件、18,633,240 个 int16 样本和长度全部精确一致。包括 338 个官方文件、9 个特殊核心语法文件和 122 个本项目 0.2 编码文件；每例均重新运行 Sony 解码器。最终构建又对全部输入重放，并在复用 Sony PCM 前核对输入和参考输出哈希。

All 469 complete-file cases and 18,633,240 signed16 samples matched exactly, including length: 338 official files, 9 varied-core-syntax files and 122 independently encoded files. Sony decoding was freshly executed for every case. The final build replayed every input, checking input and reference-output hashes before reusing Sony PCM.

公开 `decoder-vectors.bin` 包含 38 项、973,416 个 int16 样本；`aal-decoder-test` 逐样本比较，同时检查 C ABI、所有权、取消、11 项错误输入及 512 次有界位流变异。`decoder-source-provenance.json` 记录独立移入源码的来源与哈希；`decoder-native-imports.json` 记录最终 Windows 程序的静态导入表检查。

The public decoder fixture contains 38 cases and 973,416 int16 samples. The decoder test also checks the C ABI, ownership, cancellation, 11 malformed inputs and 512 bounded bitstream mutations. Source provenance and final Windows static-import inspection are recorded separately.

实际运行 Sony 的私有验收 harness 与程序分离；公开源码、CLI、GUI、库、CI 和离线检查均不需要官方组件。GUI 的实际交互由用户验证。0.2.0 的历史更名记录保留在 [release-verification-v0.2.0.json](release-verification-v0.2.0.json)。

The private acceptance harness that executes Sony is separate from the product. Public source, CLI, GUI, library, CI and offline checks require no official component. GUI interaction testing remains with the user. Historical 0.2.0 branding validation is retained separately.

## 基线 / Baseline

基线为 Independent AAL 0.2，当前版本将公开名称改为 AtracDEnc-AAL 0.2.0。基线 CLI SHA-256：

The predecessor was Independent AAL 0.2, renamed publicly to AtracDEnc-AAL 0.2.0. Baseline CLI SHA-256:

```text
b23059203b4003606d51d3f9119f6770790fc02467314bcbc0f4e6e3b6145b46
```

- [baseline-summary.json](baseline-summary.json)：十档 122 个正例、13 个拒绝用例、增强层与额外长流验证摘要。Summary of 122 positive cases, 13 rejection cases, enhancement and additional long-stream checks.
- [baseline-pcm-cases.json](baseline-pcm-cases.json)：逐项 PCM 与编码输出哈希。Per-case PCM and encoded-output hashes.
- [compression-benchmark.json](compression-benchmark.json)：70 组同输入、同核心实际码率的体积与 PCM 结果，官方体积比较扣除其 ID3 元数据。Seventy same-input, same-actual-core-rate size/PCM pairs; official ID3 metadata is excluded for the size comparison.
- [fixture-hashes.json](fixture-hashes.json)：三套离线数据文件的长度与 SHA-256。Lengths and SHA-256 of all three offline data fixtures.

所有公开记录已移除或归一化本机路径。数据夹具字节保持原样，不包含 Sony 机器码。

Local paths were removed or normalized in public records. Fixture bytes are unchanged and contain no Sony machine code.

## 历史 0.3.0 发行版 / Historical 0.3.0 release

[release-verification.json](release-verification.json) 记录历史 0.3.0 程序版本、二进制哈希、完整文件解码验收与离线检查；历史编码输出一致性保留在 0.2.0 记录中。它不把旧版二进制哈希当作新程序哈希。

[release-verification.json](release-verification.json) records the historical 0.3.0 version, binary hashes, complete-file decoder acceptance and offline checks; historical encoder output equivalence remains in the 0.2.0 record. It does not present predecessor binary hashes as current ones.

## 范围 / Scope

两套离线夹具共 **4583 包 / 9,927,136 payload 字节**，包括 AAL3 3017 包和 AALP 1566 包，要求完整 payload 及 padding 相同。当前内部逐文件自检重新解析实际核心和增强位流，再逐样本核对 PCM。

The fixtures contain **4,583 packets / 9,927,136 payload bytes**: 3,017 AAL3 and 1,566 AALP packets, requiring full payload and padding equality. Internal per-file checks reparse actual core and enhancement bitstreams before comparing PCM sample-for-sample.

有损核心决策不同于 Sony；因此完整文件字节和压缩体积可能不同。0.2.0 发布后的 [GitHub CI](github-ci-2026-11-01.json) 已通过 Windows x64、macOS arm64 和 macOS Intel 的原生构建与全部离线检查。该结果验证 CI 现场构建的程序；不表示交叉编译的 universal 发行文件已经在 Mac 上直接执行，也不表示 NW-X1050 / X1060 实机播放、跳转或传输已验证。

Lossy-core decisions differ from Sony's; full-file bytes and compression sizes can differ. The historical 0.2.0 [GitHub CI](github-ci-2026-11-01.json) passed native builds and all offline checks on Windows x64, macOS arm64 and macOS Intel. This validates the programs built in CI; it does not establish direct Mac execution of the cross-compiled universal release file or physical NW-X1050 / X1060 playback, seeking or transfer.

## 0.4.0 转换器 / Converters

[本地 Windows 无窗口记录](converter-local-v0.4.0.json) 包含八项检查；未创建或操作 GUI 窗口。新转换器共用预检查与安全命名，不修改编解码算法或 C ABI。发行版附带 `RELEASE-VALIDATION.json`，记录三个原生平台的实际结果及最终通用附件的 Apple Silicon 执行。GUI 视觉/交互与物理 Walkman 验证不包含在这些检查中。

The [local Windows record](converter-local-v0.4.0.json) contains eight headless checks; no GUI window was created or controlled. The converters share preflight/naming and do not change codec algorithms or the C ABI. Releases attach `RELEASE-VALIDATION.json` with actual three-platform results and final universal execution on Apple Silicon. GUI interaction and physical Walkman validation are outside these checks.
