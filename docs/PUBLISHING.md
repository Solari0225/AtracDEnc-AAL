# 发布与维护 / Publishing and maintenance

Repository: [Solari0225/AtracDEnc-AAL](https://github.com/Solari0225/AtracDEnc-AAL).

提供基于 AtracDEnc 的独立 AAL 编解码器、CLI、C API 和 Windows/macOS 中英文自适应简易测试转换器。仅支持 High-Speed Transfer 双层 AAL；输入规格见 [INPUT-FORMATS.md](INPUT-FORMATS.md)，模式与实际应用证据边界见 [AAL-MODES.md](AAL-MODES.md)。

An independent AtracDEnc-based codec, CLI, C API and bilingual native test converters for Windows/macOS. High-Speed Transfer two-layer AAL only; see [input requirements](INPUT-FORMATS.md) and [modes/evidence](AAL-MODES.md).

## 验证 / Validation

Windows 使用 `build.ps1 -Gui`；macOS 使用 `build.sh` 或带 `-DAAL_BUILD_GUI=ON` 的 CMake。运行 `python tools/run_checks.py --bin-dir <build-directory> --require-gui` 和 `python tools/verify_repository.py`。全部九项必须通过；GUI 入口使用 `--self-test` 诊断，不创建窗口。

Build with `build.ps1 -Gui` on Windows, or `build.sh`/CMake with GUI enabled on macOS. Run the combined checks with `--require-gui` and verify the repository. All eight checks must pass. The GUI entry point is diagnosed with `--self-test`, without creating a window.

涉及编解码算法的改动还需要私有官方参考对照，记录样本值、长度和实际验证范围；不能把 Sony 工具、DLL 或运行时依赖加入公开项目。GUI 外观和交互、物理 Walkman 行为分别由用户测试。

Codec algorithm changes also need private official-reference comparison of exact samples and lengths. Sony tools/DLLs must not become public source or runtime dependencies. GUI interaction and physical Walkman behavior are tested separately by the user.

## GitHub Actions 发布 / Actions publication

普通 push/PR 仅执行三平台原生构建、无窗口检查及产物上传，默认不发布。由项目维护者明确授权发布后，在 main 上手动运行 **Build and offline conformance**，将 **publish_release** 设为 true。

Normal push/PR runs build, execute headless checks on three native platforms and upload artifacts; they do not publish by default. After explicit maintainer authorization, manually run **Build and offline conformance** on main with **publish_release=true**.

发布任务仅在 Windows x64、macOS arm64、macOS Intel 均通过后执行。它校验原生 ZIP 内的文件摘要，使用系统 `lipo` 合并 macOS CLI 和 `.app`，ad-hoc 签名，检查双架构/签名，并在 Apple Silicon 执行最终通用 CLI 和 GUI 的无窗口诊断。

The publication job requires successful Windows x64, macOS arm64 and macOS Intel jobs. It verifies native package hashes, assembles universal CLI/app binaries with system `lipo`, ad-hoc signs and checks architectures/signatures, then executes final universal CLI/GUI headless diagnostics on Apple Silicon.

最后创建当前提交的版本标签与 draft Release，上传并通过 GitHub 返回的 SHA-256 验证三个 ZIP、`SHA256SUMS.txt` 和 `RELEASE-VALIDATION.json`，再发布为实验版。已公开的 Release 不会被脚本覆盖；失败时保留 draft 以便检查和重试。写权限仅授予手动发布任务的 job-scoped GitHub token。

It then creates a version tag and draft release for the checked commit, uploads three ZIPs, checksums and the validation record, verifies GitHub-reported SHA-256 digests, and publishes a prerelease. Already published releases are never overwritten; failed uploads retain a draft for inspection/retry. Only the manual publishing job receives a job-scoped token with write permission.

## 发行内容 / Release contents

Windows 包含 GUI、CLI、GNU/Zig 静态库；macOS universal 包含原生双架构 GUI、CLI 和两架构静态库；源码包包含完整生产代码、许可证、文档与离线夹具。Python、Node、CMake 和编译器只用于开发/打包，不是产品运行依赖。

Windows packages contain the GUI, CLI and GNU/Zig static library. macOS universal packages contain both native architectures of the app/CLI and per-architecture libraries. Source includes production code, licenses, documentation and offline fixtures. Python, Node, CMake and compilers are development tools, not product runtime dependencies.

不能将 PCM 一致性写成完整文件与 Sony 逐字节或压缩体积相同；不能将广播 AAL 试验、容器枚举或 RFC 示例写成 Standard 已落地的证据。具体源提交、原生执行、通用附件执行和未执行的测试以发布记录为准。

Exact PCM equality is not complete Sony-byte or compression-size equality. Broadcast AAL trials, container enums and RFC examples do not establish Standard deployment. Record the exact source, native/universal execution and unperformed tests accurately.


## 0.5.0 大文件与内存验收 / Large-file and memory acceptance

三平台原生 CI 均运行 `python tools/check_large_file.py --cli <CLI-path> --output-dir build/large-file`。默认生成 1 GiB + 4 MiB 的确定性噪声 PCM，实际编码大于 1 GiB 的 AAL、完整解码并比较 PCM 长度/摘要；进程峰值工作集/RSS 需 ≤64 MiB。结果保存到 `build/large-file-validation.json`，打包进入 BUILD-INFO，发布任务逐平台核验并附入 RELEASE-VALIDATION。输入/输出仅在新建测试目录生成，成功后只清理该工具创建的音频文件。Python 3.9+ 仅用于测试。

All three native CI jobs run the large-file tool. Its default fixture is 1 GiB + 4 MiB deterministic noise PCM. It actually encodes an AAL file larger than 1 GiB, fully decodes it, checks exact PCM length/hash and requires peak CLI working set/RSS ≤64 MiB. Native package metadata and the final release validation retain the observed results. Test audio is created only in a new directory and removed on success; `--keep-files` retains it. Python 3.9+ is a testing dependency only.
