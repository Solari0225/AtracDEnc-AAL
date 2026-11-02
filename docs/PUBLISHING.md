# 发布与维护 / Publishing and maintenance

项目仓库 / Repository: [Solari0225/AtracDEnc-AAL](https://github.com/Solari0225/AtracDEnc-AAL).

AtracDEnc-AAL 是基于 AtracDEnc 开发的 AAL3 / AALP 编解码器，提供 Windows/macOS 命令行、C API 与中英文简易 Windows 测试界面。An AAL3 / AALP encoder/decoder based on AtracDEnc, with a Windows/macOS CLI, C API and simple bilingual Windows test GUI.

## 发布检查 / Release checks

从项目根目录构建并运行 `python tools/run_checks.py --bin-dir bin` 与 `python tools/verify_repository.py`。GitHub CI 在 Windows x64、macOS arm64 和 macOS Intel 上独立构建、执行离线检查并打包 Actions 产物；工作流不自动发布 Release。

Build from the project root, run the combined offline checks and repository verifier. GitHub CI independently builds, tests and packages artifacts on Windows x64, macOS arm64 and macOS Intel. It does not publish a Release automatically.

涉及解码行为的改动还需要对照官方文件、原始 PCM 和外部 Sony 解码参考，记录实际样本值与长度。Sony 参考软件只用于私有开发验收，不能加入公开源码、运行路径或离线测试依赖。更新 `validation/` 中的真实结果；保留历史记录与已发布版本。

Decoder changes also require comparison against official files, original PCM and an external Sony decode reference, including exact samples and lengths. Reference software is used only by a separate private acceptance harness; do not add it to public source, runtime paths or offline dependencies. Record observed results and retain release history.

## 打包 / Packaging

```bash
python tools/package_release.py --platform windows-x64
python tools/package_release.py --platform macos-universal
python tools/package_release.py --platform source
```

脚本读取 `VERSION`，输出三个 ZIP 和 `SHA256SUMS.txt`。macOS universal 打包前先构建两个架构并运行 `python tools/make_universal_macos.py`。Windows 包包含 CLI、简易测试 GUI 和静态库；macOS 包包含通用 CLI 及两个架构的静态库；源码包包含完整生产源、许可证、文档与离线夹具。

The packager reads `VERSION` and writes ZIPs plus checksums. Assemble the macOS universal executable from both built slices first. Binary packages include the CLI and static libraries, plus the Windows test GUI. Source includes all production code, licenses, documentation and offline fixtures.

## GitHub Release

通过构建与验收后，为相应提交创建 `v<VERSION>` 标签和 Release。正文使用 `docs/releases/v<VERSION>.md`，上传三个 ZIP 和 `SHA256SUMS.txt`。当前项目保留实验版标识；实际测试结果决定说明中的兼容范围。

After build and acceptance checks pass, tag the selected commit and create a Release using its versioned notes. Upload all three ZIPs and the checksum file. Retain experimental status and describe the observed compatibility scope.

编码器的 PCM 一致性不能改写为与 Sony 完整编码字节或压缩体积一致。交叉编译、原生 CI 执行、特定预构建附件执行和 Walkman 实机测试各自记录。

Exact PCM reconstruction is separate from complete Sony encoding-byte or compression-size equality. Record cross-compilation, native CI execution, execution of a specific prebuilt asset and physical Walkman testing separately.
