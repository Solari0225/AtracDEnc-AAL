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

## 解码 / Decoding

```c
aal_pcm_buffer pcm = {0};
char error[512];
aal_status status = aal_decode_cd16(oma_bytes, oma_size, &pcm,
                                  NULL, NULL, error, sizeof(error));
if (status == AAL_OK) {
    /* Consume pcm.data: pcm.sample_frames interleaved L/R pairs. */
    aal_pcm_free(&pcm);
}
```

`aal_decode_cd16()` 自动识别全部十档未加密 AAL，返回 44.1 kHz、signed16 交错双声道 PCM 及精确帧数、配置和 PCM SHA-256。输入为完整 OMA / AA3 文件；输入与输出 PCM 各自上限 1 GiB。原有编码 ABI 保持不变；解码结果使用独立的 `aal_pcm_free()`，不能传给编码缓冲区的 `aal_free()`。

`aal_decode_cd16()` detects all ten unencrypted AAL profiles and returns 44.1 kHz signed16 interleaved stereo PCM, exact frame count, profile and PCM SHA-256. Supply the complete OMA / AA3 file; input and output PCM are each limited to 1 GiB. The encoder ABI is unchanged. Free decoded PCM with `aal_pcm_free()`, not the encoder buffer's `aal_free()`.

解码也支持相同的进度与取消回调。输出结构先初始化为零，调用结束前保持输入有效；失败或取消不返回部分 PCM。完整示例见 [decode_buffer.c](../examples/decode_buffer.c)，算法与支持范围见 [DECODING.md](DECODING.md)。

Decoding supports the same progress and cancellation callback. Zero-initialize the output and keep input valid until return; failure or cancellation returns no partial PCM. See [decode_buffer.c](../examples/decode_buffer.c) and [DECODING.md](DECODING.md).

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
# The same library also exports the decoder API:
# target_link_libraries(your_application PRIVATE AtracDEncAAL::decoder)
```

将 `path/to/AtracDEnc-AAL` 和 `your_application` 替换为宿主工程中的实际目录与目标名称。接口是 C ABI，但库包含 C++17 代码，最终需要 C++ 链接器及对应标准库。Windows 的预构建 `.a` 面向 Zig / GNU ABI。

Replace the path and application target with those in the host project. The API is C-compatible, but the implementation uses C++17 and needs a C++ linker and its standard library. Prebuilt Windows `.a` archives target the Zig / GNU ABI.

编码接口返回完整文件，解码接口返回完整 PCM；当前未公开增量流式接口。Encoding returns an entire file and decoding returns the entire PCM buffer; incremental streaming is not currently exposed.
