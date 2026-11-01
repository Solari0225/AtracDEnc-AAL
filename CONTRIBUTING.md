# 贡献说明 / Contributing

欢迎改进编码、兼容性、性能、文档和测试。请先阅读 [上游说明](UPSTREAM.md)，保留 AtracDEnc 及依赖的原始归属和许可证。

Contributions to encoding, compatibility, performance, documentation and tests are welcome. Read [upstream provenance](UPSTREAM.md) and retain the original AtracDEnc and dependency notices.

## 开发与验证 / Development and checks

按 [构建文档](docs/BUILDING.md) 构建。修改编码或整数数学逻辑时，运行全部十档往返、两套官方增强夹具以及 C ABI 测试。新增兼容性结论应注明平台、配置、实际输入格式、比较方式和验证范围。

Build using [BUILDING.md](docs/BUILDING.md). Changes to encoding or integer mathematics should pass all ten round trips, both official enhancement fixtures and the C ABI test. Compatibility claims should state the platform, profile, actual input format, comparison method and tested scope.

```bash
python tools/run_checks.py --bin-dir bin
python tools/verify_repository.py
```

文件和压缩率的比较必须使用相同输入、相同核心实际码率，并注明元数据是否扣除。PCM 一致、增强包字节一致和完整文件字节一致是不同的验证结论。

Compare sizes using the same input and actual core rate, and state whether metadata is excluded. PCM equality, enhancement byte equality and full-file byte equality are distinct results.

## 问题与变更 / Issues and changes

报告问题时附系统与架构、版本、完整命令或配置、输入格式、预期与实际结果，以及可复现的小型合成音频或生成方法。提交变更时说明行为变化与已经执行的验证；不要把仅交叉编译记录为实机运行。

For issues, include the OS/architecture, version, command/profile, input format, expected/actual behavior and a small synthetic reproducer or generation method. Describe behavior changes and checks actually run. Cross-compilation should not be recorded as native execution.

项目目前没有 Sony 逐文件字节一致或真实 Walkman 兼容性的全面保证；相关工作请用可复查的测试结果推进。

The project does not currently establish general Sony full-file byte identity or physical Walkman compatibility; improvements should be supported by reviewable results.
