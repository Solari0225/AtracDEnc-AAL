# 验证记录 / Validation records

本目录区分已经完成的基线编码验收与当前更名发行版的检查。Sony 仅是外部参考工具，不是项目的运行依赖；项目、CI 和离线测试均不分发或调用 Sony 程序。

This directory distinguishes predecessor encoding conformance from checks on the current renamed release. Sony was an external reference, not a runtime dependency. The project, CI and offline tests neither distribute nor invoke Sony programs.

## 0.3.0 解码器 / Decoder

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

## 当前发行版 / Current release

[release-verification.json](release-verification.json) 记录当前 0.3.0 程序版本、二进制哈希、完整文件解码验收与离线检查；历史编码输出一致性保留在 0.2.0 记录中。它不把旧版二进制哈希当作新程序哈希。

[release-verification.json](release-verification.json) records the current 0.3.0 version, binary hashes, complete-file decoder acceptance and offline checks; historical encoder output equivalence remains in the0.2.0 record. It does not present predecessor binary hashes as current ones.

## 范围 / Scope

两套离线夹具共 **4583 包 / 9,927,136 payload 字节**，包括 AAL3 3017 包和 AALP 1566 包，要求完整 payload 及 padding 相同。当前内部逐文件自检重新解析实际核心和增强位流，再逐样本核对 PCM。

The fixtures contain **4,583 packets / 9,927,136 payload bytes**: 3,017 AAL3 and 1,566 AALP packets, requiring full payload and padding equality. Internal per-file checks reparse actual core and enhancement bitstreams before comparing PCM sample-for-sample.

有损核心决策不同于 Sony；因此完整文件字节和压缩体积可能不同。0.2.0 发布后的 [GitHub CI](github-ci-2026-11-01.json) 已通过 Windows x64、macOS arm64 和 macOS Intel 的原生构建与全部离线检查。该结果验证 CI 现场构建的程序；不表示交叉编译的 universal 发行文件已经在 Mac 上直接执行，也不表示 NW-X1050 / X1060 实机播放、跳转或传输已验证。

Lossy-core decisions differ from Sony's; full-file bytes and compression sizes can differ. The historical 0.2.0 [GitHub CI](github-ci-2026-11-01.json) passed native builds and all offline checks on Windows x64, macOS arm64 and macOS Intel. This validates the programs built in CI; it does not establish direct Mac execution of the cross-compiled universal release file or physical NW-X1050 / X1060 playback, seeking or transfer.
