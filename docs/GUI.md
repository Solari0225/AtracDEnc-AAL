# 简易测试图形界面 / Simple test GUI

`AtracDEnc-AAL-GUI.exe` 是用于手动验证编码器的**简易测试图形界面**，直接链接 AtracDEnc-AAL 编码核心。它不启动外部命令行程序或 Sony 组件。

`AtracDEnc-AAL-GUI.exe` is a **simple test GUI** for manually exercising the encoder. It directly links the AtracDEnc-AAL engine and does not launch an external CLI or Sony component.

## 语言 / Language

启动时读取 Windows 首选显示语言，按其顺序选取支持的中文或英文；没有匹配时使用英文。中文显示语言使用简体中文界面，英文显示语言使用英文界面。重启程序后会重新判断。

At startup, the app reads Windows' ordered preferred display languages and chooses the first supported Chinese or English language, falling back to English. Chinese uses the Simplified Chinese translation. The selection is evaluated again when the app is restarted.

```powershell
# 跟随 Windows / Follow Windows
.\AtracDEnc-AAL-GUI.exe
.\AtracDEnc-AAL-GUI.exe --lang auto

# 仅对本次程序启动指定语言 / Override this app launch only
.\AtracDEnc-AAL-GUI.exe --lang zh
.\AtracDEnc-AAL-GUI.exe --lang en

# 同时加入文件 / Also queue files
.\AtracDEnc-AAL-GUI.exe --lang en "input.wav"
```

此选项不修改 Windows 的语言设置。界面标签、状态、日志、选择器标题和编码进度均随所选语言变化。文件名和路径保持原样；底层系统或编解码器诊断保留其原始文本。

This option does not change Windows language settings. Labels, status, logs, picker titles and encoding progress use the selected language. Filenames and paths are preserved; underlying system or codec diagnostics retain their original text.

## 手动操作 / Manual use

1. 添加或拖入 44.1 kHz、16-bit、双声道 PCM WAV。Add or drop 44.1 kHz, 16-bit, stereo PCM WAV files.
2. 选择核心配置，默认 `aalp-352`。Choose a profile; the default is `aalp-352`.
3. 选择输出文件夹并开始编码；取消按钮可终止当前批次。Choose an output folder and start encoding; Cancel stops the current batch.
4. 查看 OMA 及 JSON 校验报告。Review the OMA file and JSON verification report.

“内置自检 / Self-test”使用当前选定配置执行内存编码与 PCM 精确往返，不需要音频文件或 Sony 软件。界面是测试辅助工具；其他应用可通过 [C API](INTEGRATION.md) 接入相同核心。

Self-test performs an in-memory encode and exact PCM round trip with the selected profile, without an input file or Sony software. The GUI is a test aid; other applications can use the same core through the [C API](INTEGRATION.md).

## 已执行的检查 / Checks performed

语言优先级、`--lang` 参数、86 项翻译、进度映射、文件选择器过滤字符串和 Unicode 文件名保留有独立的无窗口测试。该测试不创建、显示或操作 GUI 窗口；实际显示与交互交由用户验证。

Headless tests cover language priority, `--lang` arguments, 86 translation entries, progress mapping, file-filter strings and Unicode filename preservation. They do not create, show or control a GUI window. Visual and interaction testing is left to the user.
