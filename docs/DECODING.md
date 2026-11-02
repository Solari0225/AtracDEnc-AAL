# 独立 AAL 解码器 / Independent AAL decoder

从 0.3.0 开始，AtracDEnc-AAL 可将未加密的 AAL3 / AALP OMA 或 AA3 文件还原为 PCM WAV。解码不需要 Sony SMLE、TraConv、Sony DLL、FFmpeg 或其他音频组件。

Starting with 0.3.0, AtracDEnc-AAL reconstructs PCM WAV from unencrypted AAL3 / AALP OMA or AA3 files. Decoding requires no Sony SMLE, TraConv, Sony DLL, FFmpeg or other audio component.

## 命令行 / CLI

```powershell
.\atracdenc-aal.exe --decode "input.oma" "output.wav"
.\atracdenc-aal.exe -d -i "input.aa3" -o "output.wav"
```

```bash
./atracdenc-aal --decode input.oma output.wav
```

配置从文件自动识别，不需要 `--profile`、`--codec` 或 `--bitrate`。输出为 44,100 Hz、16-bit、双声道 PCM WAV；终端同时输出配置、精确采样帧数及 PCM SHA-256。输出文件已存在时返回错误；解码不生成 JSON 旁车文件。`Ctrl+C` 可取消，失败或取消不会发布部分 WAV。

The profile is detected from the file. Do not select `--profile`, `--codec` or `--bitrate` for decoding. Output is 44,100 Hz, 16-bit stereo PCM WAV. The CLI prints the profile, exact sample-frame count and PCM SHA-256. Existing output files cause an error; decoding creates no JSON sidecar. `Ctrl+C` cancels; failures or cancellation do not publish a partial WAV.

## 范围 / Scope

- AAL3 / ATRAC3 132 kbps，以及 AALP / ATRAC3plus 48、64、96、128、160、192、256、320、352 kbps。AAL3 / ATRAC3 132 kbps and all nine listed AALP / ATRAC3plus profiles.
- 当前支持 44.1 kHz、16-bit、双声道、未加密的完整文件。Input must be a complete, unencrypted 44.1 kHz, 16-bit stereo file.
- 0.5.0 起文件解码逐包读写，无固定 1 GiB 上限；大 WAV 自动使用 RF64。旧内存 API 返回完整 PCM，新回调 API 可逐块消费。Since 0.5.0, file decoding uses bounded packet/PCM buffers with no fixed 1 GiB cap and automatic RF64 for large WAV output. The legacy memory API returns complete PCM; the new callback API emits blocks.
- 普通有损 ATRAC1 / ATRAC3 / ATRAC3plus、DRM、其他采样率、其他位深、多声道和随机 seek 接口不在此入口范围内。Ordinary lossy ATRAC, DRM, other sample rates or bit depths, multichannel audio and random seeking are outside this entry point's scope.

## 重建逻辑 / Reconstruction

1. 读取可选 ID3 / ea3 元数据、EA3 配置与连续 BLK 包，检查核心尺寸、顺序、末尾标记和样本计数。Read optional ID3 / ea3 metadata, EA3 configuration and ordered BLK packets; validate core sizes, ordering, final markers and sample counts.
2. 从实际 ATRAC3 或 ATRAC3plus 位流读取谱数据、增益、窗口及声道信息，执行 Sony AAL 使用的定点重建过程，并保持跨帧变换和滤波状态。Parse actual core bytes and run the AAL-specific fixed-point reconstruction with persistent transform, gain and synthesis state.
3. 解压无损增强层：声道去相关逆变换、固定或 LPC8 预测逆变换、分区 Rice 解码及原样模式。Decode enhancement data: inverse channel decorrelation, fixed or LPC8 prediction, partitioned Rice data and raw mode.
4. 将整数核心预测与残差按 16 位模加相加，再按 AAL3 的 138 或 AALP 的 368 个采样帧对齐，剔除 priming 和 flush。Add prediction and residual modulo 16 bits, align by the 138-frame AAL3 or 368-frame AALP delay, and remove priming/flush samples.
5. 根据末包余数和实际包数恢复精确长度。末尾落在合成延迟内时存在额外 flush 包，不能仅用毫秒时长或简单拼接决定 PCM 长度。Recover exact length from the final remainder and packet count, including an additional flush packet when the source tail falls inside the synthesis delay. Millisecond duration alone is insufficient.

Sony 的 AAL 定点预测与普通感知解码不同。例如 AALP 原版整数预测图不进行完整的音调振荡器、功率补偿和噪声合成；这些语法仍须正确消费以定位后续字段。不能把普通浮点有损解码输出与无损残差直接相加。AAL3 的增益与音调谱语法也必须正确解析。

Sony's AAL fixed-point predictor differs from ordinary perceptual decoding. The original AALP integer graph omits full tonal-oscillator, power-compensation and noise synthesis; their syntax must still be consumed correctly. Adding enhancement residuals to an ordinary floating-point music decoder does not guarantee exact PCM. AAL3 gain and tonal-spectrum syntax must also be parsed correctly.

本项目继续基于 AtracDEnc 开发；所引用版本的上游解码命令主要针对 ATRAC1，不能直接充当 AAL3 / AALP 解码器。`core/decode/` 是根据官方行为独立重建的物理位流解析器及整数预测逻辑，复用本项目已有的定点数学与增强层实现；不链接或嵌入官方程序指令。[源码来源清单](../validation/decoder-source-provenance.json)记录移入源码的哈希。

The project remains based on AtracDEnc. The vendored upstream decode command primarily targets ATRAC1 and is not an AAL3 / AALP decoder. `core/decode/` contains independently reconstructed physical-bitstream readers and integer predictors, reusing this project's fixed-point mathematics and enhancement implementation. No official program instructions are linked or embedded. The [source provenance manifest](../validation/decoder-source-provenance.json) records imported source hashes.

## 验证 / Validation

验证分别使用官方编码文件、本项目编码文件和覆盖特殊核心语法的合成文件。每个案例比较原始 PCM、独立解码 PCM 与实际 Sony 解码 PCM 的样本值和长度。公开离线夹具使用合成信号及参考 PCM，可在 Windows 和 macOS 上运行，不需要官方组件。

Validation compares original PCM, independently decoded PCM and actual Sony-decoded PCM for official-encoded files, this project's files and deliberately varied synthetic core syntax. Public offline fixtures contain synthetic signals and reference PCM and run without official components on Windows and macOS.

结果与限制见 [解码验收记录](../validation/decoder-validation.json) 和 [验证说明](../validation/README.md)。解码成功只能说明位流被完整解析；没有源 PCM 哈希时，程序不能额外断言文件从未被修改。GUI 的实际显示和交互由用户测试。

See the [decoder acceptance record](../validation/decoder-validation.json) and [validation notes](../validation/README.md). A successful decode establishes complete parsing and reconstruction; without an original PCM hash, it does not independently prove that a file was never modified. Actual GUI appearance and interaction are tested by the user.
