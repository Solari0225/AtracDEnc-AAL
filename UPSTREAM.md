# 上游来源 / Upstream provenance

**AtracDEnc-AAL 基于 [AtracDEnc](https://github.com/dcherednik/atracdenc) 项目开发。**

**AtracDEnc-AAL is developed on top of [AtracDEnc](https://github.com/dcherednik/atracdenc).**

## 引用代码 / Vendored code

- 上游项目 / Upstream: AtracDEnc, `dcherednik/atracdenc`.
- 来源 / Source: <https://github.com/dcherednik/atracdenc>.
- 许可证 / License: LGPL-2.1-or-later; original license and notices are retained.
- 存放目录 / Vendored directory: [`core/atracdenc-master/`](core/atracdenc-master/).
- 初始下载源码归档 SHA-256 / Original downloaded source archive SHA-256: `fb3d8b83870c7d17b5f83abe1ab1b0862f7ee39b1effde2f5c4cf3db06e06355`.

基线记录保留了源码归档哈希，未记录上游 Git 提交号。因此这里不将引用代码标记为某个未经证实的提交或“最新版本”。此目录是本项目构建所需的引用源码集合，不代表完整上游仓库。

The baseline records the source archive hash but not an upstream Git commit ID. This document therefore does not assign an unverified commit or describe the code as the latest upstream revision. The vendored directory contains the source collection used by this build, rather than a complete upstream repository.

## 使用与修改范围 / Use and modification scope

AtracDEnc 提供 ATRAC3 / ATRAC3plus 核心编码的基础，包括滤波、变换、量化和位流打包。项目根目录的构建脚本选择需要的源码，不使用上游音频文件 I/O 前端或 GHA 运行库。

AtracDEnc supplies the foundation for ATRAC3 / ATRAC3plus core encoding, including filtering, transforms, quantization and bitstream packing. Root build scripts select the required source without the upstream audio-file I/O frontend or GHA runtime.

上游目录中的 `src/atrac/at3p/at3p_bitstream.h` 和 `.cpp` 修改为导出实际写出帧的最终谱数据，用作独立位流解析器的附加一致性检查。这两处带有日期和修改说明。预测器使用实际编码字节的独立解析结果。

Within the vendored code, `src/atrac/at3p/at3p_bitstream.h` and `.cpp` expose the final emitted spectrum for an additional consistency check against the independent bitstream parser. Both files carry dated modification notices. Reconstruction uses independently parsed emitted bytes.

本项目新增内容包括：

This project adds:

- `core/core_adapter.cpp`、`core/plus/`：核心适配与确定性整数重建 / core adapters and deterministic integer reconstruction.
- `core/decode/`：根据官方行为独立重建的 AAL3 / AALP 解析器、增益与整数预测 / independently reconstructed AAL3 / AALP readers, gain and integer prediction.
- `compression/`：AAL 无损增强编码与解码 / AAL lossless enhancement coding and decoding.
- `src/`：AAL3 / AALP 封装、CLI、文件自校验及 Windows 测试界面 / framing, CLI, file verification and Windows test GUI.
- `include/`、`examples/`：宿主应用 C 接口与示例 / C API and integration example.
- `tests/`、`validation/`：合成数据夹具及验证记录 / synthetic fixtures and verification records.

AtracDEnc 的原始版权和依赖许可证归各自权利人所有。其他依赖及预构建运行库说明见 [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt)。Sony、ATRAC 和 Walkman 名称仅用于描述兼容目标；没有 Sony 程序或机器码随本项目分发。

Original AtracDEnc and dependency copyrights remain with their respective holders. See [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt) for other dependencies and prebuilt support runtimes. Sony, ATRAC and Walkman identify interoperability targets only; no Sony program or machine code is distributed.
