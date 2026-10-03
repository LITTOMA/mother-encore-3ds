# 从固定上游恢复音频

GitHub 源码仓库不重复保存从 Mother: Encore 上游转换的 21 个 PCM 文件（119,032,268 字节）。这不改变运行包：构建的 3DSX/CIA 仍包含运行时所需音频。

保留 `content/native-audio.json`、`content/podunk-audio.json`、两个受检 ENCAUD 数据包和 `romfs/data/*-audio-manifest.json`。它们记录固定上游提交、原始路径与 SHA-256、Godot 导入设置、采样率/声道/循环点、固定转换参数，以及期望 PCM 长度、SHA-256、CRC。原音频仍受 `THIRD_PARTY_NOTICES.md` 与上游许可约束。本项目原创的静音 `assets/silence.wav` 继续入库。

## 干净检出

先按 README 恢复上游，再构建：

```sh
python3 tools/ci_bootstrap.py
make test
make 3dsx
```

`make native-content` 和 CMake 的原生内容目标都会先执行 `python3 tools/restore_audio.py`。已存在且哈希正确的 PCM 只验证，不重复解码；仅缺失的 PCM 会恢复。上游已存在时无须网络。可显式单独运行恢复命令。

缺少 PCM 时需要 FFmpeg 和 ffprobe。已逐文件验证的参考环境为 FFmpeg 7.1.5-0+deb13u1；使用固定 `pcm_s16le / s16le / -bitexact / -threads 1`，仅按配方的明确要求重采样。其他版本只有在输出仍与既有指纹完全一致时才会被接受。转换失败或输出不一致会阻止构建，不更新期望哈希，不改写配方/音频包/清单。已有但损坏的 PCM 也会拒绝覆盖，需先检查该文件。

转换在临时文件完成并验证后，以仅创建缺失文件的原子方式发布。不会执行清单中的历史命令文本，不会导入、重置或修改上游 checkout。所有图形、文本、字体与音频数据的许可边界保持。

## 验证范围

本次从空输出目录恢复全部 21 个 PCM，逐个匹配原有长度、SHA-256 和 CRC；再次执行恢复零文件。11 项独立回归覆盖原样文件不重复解码、缺失恢复、上游/配方/版本/清单损坏、解码输出差异、路径越界、缺少工具和拒绝覆盖既有异常文件。

这些是来源和构建验证，不是实际音频听感或 Old/New 3DS 硬件验收。字体重建/控制台资源 staging 还需要 FontTools（已记录版本 4.61.1）、Pillow 及现有官方工具链。
