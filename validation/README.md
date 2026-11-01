# 验证记录 / Validation records

本目录区分已经完成的基线编码验收与当前更名发行版的检查。Sony 仅是外部参考工具，不是项目的运行依赖；项目、CI 和离线测试均不分发或调用 Sony 程序。

This directory distinguishes predecessor encoding conformance from checks on the current renamed release. Sony was an external reference, not a runtime dependency. The project, CI and offline tests neither distribute nor invoke Sony programs.

## 基线 / Baseline

基线为 Independent AAL 0.2，当前版本将公开名称改为 AtracDEnc-AAL 0.2.0。基线 CLI SHA-256：

The predecessor was Independent AAL 0.2, renamed publicly to AtracDEnc-AAL 0.2.0. Baseline CLI SHA-256:

```text
b23059203b4003606d51d3f9119f6770790fc02467314bcbc0f4e6e3b6145b46
```

- [baseline-summary.json](baseline-summary.json)：十档 122 个正例、13 个拒绝用例、增强层与额外长流验证摘要。Summary of 122 positive cases, 13 rejection cases, enhancement and additional long-stream checks.
- [baseline-pcm-cases.json](baseline-pcm-cases.json)：逐项 PCM 与编码输出哈希。Per-case PCM and encoded-output hashes.
- [compression-benchmark.json](compression-benchmark.json)：70 组同输入、同核心实际码率的体积与 PCM 结果，官方体积比较扣除其 ID3 元数据。Seventy same-input, same-actual-core-rate size/PCM pairs; official ID3 metadata is excluded for the size comparison.
- [fixture-hashes.json](fixture-hashes.json)：两套离线数据文件的长度与 SHA-256。Lengths and SHA-256 of both offline data fixtures.

所有公开记录已移除或归一化本机路径。数据夹具字节保持原样，不包含 Sony 机器码。

Local paths were removed or normalized in public records. Fixture bytes are unchanged and contain no Sony machine code.

## 当前发行版 / Current release

[release-verification.json](release-verification.json) 记录当前程序版本、二进制哈希、源码改动范围、离线检查，以及重新编码基线用例后是否得到相同的 OMA 字节。它不把旧版二进制哈希当作新程序哈希。

[release-verification.json](release-verification.json) records the current version, binary hashes, source-change scope, offline checks and whether re-encoding baseline cases yields the same OMA bytes. It does not present predecessor binary hashes as current ones.

## 范围 / Scope

两套离线夹具共 **4583 包 / 9,927,136 payload 字节**，包括 AAL3 3017 包和 AALP 1566 包，要求完整 payload 及 padding 相同。当前内部逐文件自检重新解析实际核心和增强位流，再逐样本核对 PCM。

The fixtures contain **4,583 packets / 9,927,136 payload bytes**: 3,017 AAL3 and 1,566 AALP packets, requiring full payload and padding equality. Internal per-file checks reparse actual core and enhancement bitstreams before comparing PCM sample-for-sample.

有损核心决策不同于 Sony；因此完整文件字节和压缩体积可能不同。当前公开资料不宣称 Mac 原生执行或 NW-X1050 / X1060 实机播放、跳转和传输已通过。GitHub CI 尚须在仓库发布后实际运行。

Lossy-core decisions differ from Sony's; full-file bytes and compression sizes can differ. These records do not establish native Mac execution or NW-X1050 / X1060 playback, seeking and transfer. GitHub CI still needs an actual run after publication.
