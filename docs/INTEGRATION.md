# C 接口集成 / C API integration

项目的公开名称是 AtracDEnc-AAL。现有 `aal_*` 函数、`independent_aal.h` 头文件名和 C++ `independent_aal` 命名空间保留，以便已有调用方继续集成。

The public project name is AtracDEnc-AAL. Existing `aal_*` functions, the `independent_aal.h` header name and the C++ `independent_aal` namespace are retained for existing integrations.

## 编码 / Encoding

```c
#include "independent_aal.h"

int encode_audio(const int16_t* pcm, size_t stereo_frames) {
    aal_buffer output = {0};
    char error[512];
    aal_status status = aal_encode_cd16_profile(
        pcm, stereo_frames, "aalp-352", &output,
        NULL, NULL, error, sizeof(error));
    if (status != AAL_OK) return (int)status;

    /* Write or consume output.data[0..output.size) in the host application. */
    aal_free(&output);
    return 0;
}
```

输入必须是 44.1 kHz、signed16、交错双声道 PCM：`L0,R0,L1,R1,...`。`stereo_frames` 表示 L/R 对数，不是单声道样本总数；输入缓冲区在函数返回前保持有效。

Input must be 44.1 kHz signed16 interleaved stereo PCM: `L0,R0,L1,R1,...`. `stereo_frames` counts L/R pairs, not individual channel samples. The input buffer must remain valid until the call returns.

输出 `aal_buffer` 必须初始化为零。成功后获得完整未加密 OMA 内存及源 PCM SHA-256；使用 `aal_free()` 释放，不能改用宿主应用的其他分配器。调用失败或取消时不发布部分编码结果。

Zero-initialize `aal_buffer`. Success returns a complete unencrypted OMA buffer and the input PCM SHA-256. Release it with `aal_free()`, rather than another allocator. Failed or cancelled calls do not publish partial encoded output.

## 配置与取消 / Profiles and cancellation

- `aal_profile_count()`、`aal_profile_get()` 枚举十档。枚举结构中的字符串由库持有，调用方不释放。These functions enumerate ten profiles; returned strings belong to the library.
- `aal_encode_cd16_profile()` 显式选择配置，未知配置返回错误。Explicit profile selection rejects unknown identifiers.
- `aal_encode_cd16()` 默认使用 `aalp-352`。The default entry point uses `aalp-352`.
- `aal_progress` 回调在调用者线程同步执行；返回非零请求取消，最终得到 `AAL_CANCELLED`。The progress callback runs synchronously on the caller's thread; return nonzero to request `AAL_CANCELLED`.
- GUI 宿主应在工作线程执行编码，并自行将进度送回界面线程。输入和回调参数的生命周期由宿主负责；同一输出结构不要并发使用。GUI hosts should encode on a worker thread and dispatch progress to their UI thread. The host manages input and callback lifetimes; do not share an output structure concurrently.

函数签名和状态值以 [头文件](../include/independent_aal.h) 为准，完整示例见 [encode_buffer.c](../examples/encode_buffer.c)。

See the [header](../include/independent_aal.h) for exact signatures and status codes, and [encode_buffer.c](../examples/encode_buffer.c) for a complete example.

## CMake

```cmake
add_subdirectory(path/to/AtracDEnc-AAL atracdenc-aal-build)
target_link_libraries(your_application PRIVATE AtracDEncAAL::encoder)
```

将 `path/to/AtracDEnc-AAL` 和 `your_application` 替换为宿主工程中的实际目录与目标名称。接口是 C ABI，但库包含 C++17 代码，最终需要 C++ 链接器及对应标准库。Windows 的预构建 `.a` 面向 Zig / GNU ABI。

Replace the path and application target with those in the host project. The API is C-compatible, but the implementation uses C++17 and needs a C++ linker and its standard library. Prebuilt Windows `.a` archives target the Zig / GNU ABI.

该接口返回内存形式的整个文件，当前不提供增量流式编码接口。The API returns an entire in-memory file; incremental streaming is not currently exposed.
