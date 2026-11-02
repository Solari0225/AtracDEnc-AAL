# 构建与验证 / Building and testing

从项目根目录构建，所需 AtracDEnc 源码已经随仓库提供。运行编解码器不需要安装音频组件。

Build from this repository's root. The required AtracDEnc source is vendored. No external audio component is required to run the encoder/decoder.

## Windows x64

本发行版使用 Zig 0.14.1 构建；需要 PowerShell 和该编译器。`-Gui` 同时构建测试界面。

This release uses Zig 0.14.1 and PowerShell. `-Gui` also builds the test interface.

```powershell
.\build.ps1 -Zig "C:\tools\zig\zig.exe" -Gui
.\bin\atracdenc-aal.exe --version
.\bin\atracdenc-aal.exe --self-test-all
.\bin\aal-conformance.exe tests\enhancement-vectors.bin
.\bin\aal-conformance.exe tests\plus-enhancement-vectors.bin
.\bin\aal-c-api-test.exe
```

输出包含 `atracdenc-aal.exe`、`AtracDEnc-AAL-GUI.exe`、测试程序和 `libatracdenc_aal-x86_64-windows-gnu.a`。预构建静态库用于 GNU / Zig 工具链，不宣称兼容 MSVC ABI；其他工具链需要自行构建和验证。

Outputs include the CLI, test GUI, test programs and `libatracdenc_aal-x86_64-windows-gnu.a`. The prebuilt static library targets GNU / Zig tooling; MSVC ABI compatibility is not claimed. Other toolchains require their own build and validation.

## macOS

需要 Apple clang（Xcode Command Line Tools）及系统的 `ar`。最低目标系统为 macOS 11。

Requires Apple clang from Xcode Command Line Tools and the system `ar`. The deployment target is macOS 11 or later.

```bash
bash build.sh
./bin/atracdenc-aal --self-test-all
./bin/aal-conformance tests/enhancement-vectors.bin
./bin/aal-conformance tests/plus-enhancement-vectors.bin
./bin/aal-c-api-test
./bin/aal-gui-i18n-test
./bin/aal-decoder-test tests/decoder-vectors.bin
```

可通过 `CC` / `CXX` 指定编译器，或向脚本追加编译选项，例如 `bash build.sh -arch arm64`。脚本为当前选定架构生成 CLI、`bin/AtracDEnc-AAL-GUI.app` 及 `bin/libatracdenc_aal.a`；设置 `AAL_BUILD_GUI=0` 可只构建命令行和测试。GUI 使用系统 AppKit，不需要第三方界面框架。

Use `CC` / `CXX` to select compilers or pass extra compiler options, such as `bash build.sh -arch arm64`. The script produces the CLI, `bin/AtracDEnc-AAL-GUI.app` and static library for the selected architecture; set `AAL_BUILD_GUI=0` to omit the GUI. The interface uses system AppKit, with no third-party GUI framework.

## CMake

提供 CMake 3.16+ / C++17 工程。以下命令适用于 macOS 原生构建；Windows 本地验证使用上面的 Zig 脚本。

A CMake 3.16+ / C++17 project is included. These commands cover a native macOS build; local Windows validation uses the Zig script above.

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release -DAAL_BUILD_GUI=ON
cmake --build build/cmake --config Release --parallel
ctest --test-dir build/cmake -C Release --output-on-failure
```

使用 `-DAAL_BUILD_GUI=ON` 启用 Windows 或 macOS 原生简易测试转换器；核心库 CMake 目标保留为 `independent_aal`，同时提供 `AtracDEncAAL::encoder` 和 `AtracDEncAAL::decoder` 别名，输出库名为 `atracdenc_aal`。

`-DAAL_BUILD_GUI=ON` enables the native Windows or macOS simple test converter. The library target retains `independent_aal`, also available as `AtracDEncAAL::encoder` and `AtracDEncAAL::decoder`, with the output name `atracdenc_aal`.

## macOS 交叉编译 / macOS cross-compilation

Windows 上可使用同一 Zig 编译器构建两种 Mac 架构：

The same Zig compiler can cross-compile both Mac architectures on Windows:

```powershell
.\build.ps1 -Zig "C:\tools\zig\zig.exe" -Target aarch64-macos
.\build.ps1 -Zig "C:\tools\zig\zig.exe" -Target x86_64-macos
python tools/make_universal_macos.py
```

该交叉编译入口仅构建 CLI、库和控制台测试，不能在没有 Apple SDK 的 Windows 环境中构建 AppKit `.app`。通用 CLI 为 `bin/atracdenc-aal-macos-universal`。发行版 GUI 则由原生 macOS CI 构建两个架构并使用系统 `lipo` 合并。

This cross-build entry builds the CLI, library and console tests only; it cannot build AppKit without an Apple SDK. The universal CLI is `bin/atracdenc-aal-macos-universal`. Release GUIs are built on both native macOS architectures and assembled with system `lipo`.

## 离线检查 / Offline checks

Python 3.9+ 可统一执行检查；Python 仅为开发工具，不是编码器运行依赖：

Python 3.9+ can run the combined checks; it is a development tool, not an encoder runtime dependency:

```bash
python tools/run_checks.py --bin-dir bin
python tools/verify_repository.py
```

`run_checks.py` 执行十档内存往返、两套官方增强数据对照、C ABI、GUI 语言、完整文件解码、转换器文件流程和流式回归测试；流式回归包含十档 70 项冻结 AAL 摘要、分片回调、取消清理及 RF64 检查。存在 GUI 时还执行 `--self-test` 入口诊断。CI 使用 `--require-gui` 要求全部九项通过，均不创建窗口。两套夹具必须分别得到 **3017 包 / 5,855,736 字节**、**1566 包 / 4,071,400 字节**全部一致。

`run_checks.py` runs ten-profile round trips, both enhancement fixtures, C ABI, GUI language, complete decoding, converter file-pipeline and streaming regression checks. Streaming checks include 70 frozen AAL hashes across ten profiles, fragmented callbacks, cancellation cleanup and RF64. If the GUI exists its headless entry point is checked too. CI requires all nine checks with `--require-gui`; no window is created. The fixture results must be **3,017 packets / 5,855,736 bytes** and **1,566 packets / 4,071,400 bytes**, with exact payload and padding matches.

## GitHub CI

[CI 工作流](../.github/workflows/ci.yml) 在 Windows x64、macOS arm64 和 macOS Intel 上构建与执行离线测试，并保存构建产物。普通 push/PR 构建不安装 Sony 工具、不测试 Walkman 硬件、不发布 Release。只有手动运行并明确启用 `publish_release` 时，才在三平台检查全部通过后，合并 macOS 通用程序、执行最终附件诊断并发布实验版。Actions 与编译器版本固定；首次远程运行之前不将其记为已通过。

The [CI workflow](../.github/workflows/ci.yml) builds and runs offline tests on Windows x64, macOS arm64 and macOS Intel, then uploads artifacts. Normal push/PR builds do not install Sony tools, test Walkman hardware or publish releases. A manual run with explicit `publish_release` enabled assembles the universal macOS app, checks the final executables and publishes an experimental release only after all three platforms pass. Action and compiler versions are pinned; CI is not recorded as passing before an actual run.

## 本地发行打包 / Local release packaging

```bash
python tools/package_release.py --platform windows-x64
python tools/package_release.py --platform macos-universal
python tools/package_release.py --platform source
```

按实际已构建的平台选择命令，文件写入 `dist/` 并附 SHA-256。源码包排除 `bin/`、`build/`、`dist/` 和本地 Git 元数据，保留完整生产源、许可证和两套数据夹具。

Choose the platform actually built. Packages and SHA-256 checksums are written to `dist/`. Source archives exclude build outputs and local Git metadata while retaining the complete production source, licenses and both data fixtures.


## 0.5.0 大文件与内存验收 / Large-file and memory acceptance

三平台原生 CI 均运行 `python tools/check_large_file.py --cli <CLI-path> --output-dir build/large-file`。默认生成 1 GiB + 4 MiB 的确定性噪声 PCM，实际编码大于 1 GiB 的 AAL、完整解码并比较 PCM 长度/摘要；进程峰值工作集/RSS 需 ≤64 MiB。结果保存到 `build/large-file-validation.json`，打包进入 BUILD-INFO，发布任务逐平台核验并附入 RELEASE-VALIDATION。输入/输出仅在新建测试目录生成，成功后只清理该工具创建的音频文件。Python 3.9+ 仅用于测试。

All three native CI jobs run the large-file tool. Its default fixture is 1 GiB + 4 MiB deterministic noise PCM. It actually encodes an AAL file larger than 1 GiB, fully decodes it, checks exact PCM length/hash and requires peak CLI working set/RSS ≤64 MiB. Native package metadata and the final release validation retain the observed results. Test audio is created only in a new directory and removed on success; `--keep-files` retains it. Python 3.9+ is a testing dependency only.
