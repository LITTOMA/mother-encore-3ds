# 来源、依赖与许可

本项目自编代码、工具、原创文档和原创测试数据采用根目录的标准 [MIT](LICENSE)。上游派生代码、游戏内容、字体及第三方库各自保留其版权和许可。

## Mother: Encore

固定上游为 Act2 v0.4.1.0，提交 `7d9246600fffe518408f5830d4848635019005a3`，Copyright (c) 2025 Team Encore。官方源码以只读子模块位于 `upstream/MOTHER-Encore`，原始许可保存在 [MOTHER-Encore-0.4.1.0-LICENSE.txt](docs/licenses/MOTHER-Encore-0.4.1.0-LICENSE.txt)。

上游声明只有代码采用 MIT；素材和音乐仅允许用于与游戏有关的 fork、修改或翻译，不能用于无关项目。其许可文件没有标准 MIT 的完整授权段落，代码授权表述仍需向上游确认。上游派生代码保留 Team Encore 版权和原许可。

本项目使用上游派生的地图、人物、战斗图形、文本、剧情和字体。21 项 PCM 音频在构建时从固定上游恢复，未纳入 Git，运行包包含所需音频。源路径、指纹和转换依据见 `compatibility/reviews/` 与 `romfs/` 的来源清单；这些游戏内容不由本项目的 MIT 重新授权。

Mother: Encore / Mother / EarthBound 名称用于说明移植目标与来源。本项目没有官方背书或独立 Nintendo 授权文件；上游的游戏相关用途条款不能代替原权利人的授权。

## 字体

EBMain 字体资源包含 Latin、日本语、韩语和简体中文字形。五个固定源 TTF 的嵌入版权和许可字段见 [SOURCE_FONT_NOTICES.md](docs/licenses/SOURCE_FONT_NOTICES.md)。

- 简体中文 `Fonts/EBMain_zh_cn.ttf` 保留 TakWolf 的 2022 年版权、保留字体名 `Fusion Pixel` 和 SIL OFL 1.1。完整文本见 [Fusion-Pixel-LICENSE-OFL.txt](docs/licenses/Fusion-Pixel-LICENSE-OFL.txt)。转换生成的字体 atlas / 字形资源保留 OFL，不添加仅限本游戏的再许可条件，不使用保留字体名推广修改版本。
- 上游 `Fonts/doc_fonts.txt` 指明韩文字形来自 Quiple 的 Galmuri9，采用 OFL。完整版权与许可见 [Galmuri-LICENSE-OFL.md](docs/licenses/Galmuri-LICENSE-OFL.md)。混合字体中的其他来源不能一并由 OFL 授权，逐字形来源映射仍未完整确认。
- Latin、日本语及全宽字形的嵌入字串含 Nintendo / Team Encore，没有独立许可字段。上游文档将其来源指向 EarthBound / MOTHER 游戏；原权利人的独立授权尚未确认，不能将这些字形认定为 MIT 或 OFL 字体。

子模块中的其他字体不自动进入本项目运行包；新增使用须核对各自来源与许可。

## 运行库与开发工具

Godot 3.6.2 的派生碰撞与行为机制保留 [Godot MIT](docs/licenses/Godot-3.6.2-LICENSE.txt) 版权。源兼容 PCG 随机机制来自 Godot 随附的 PCG32，Copyright (c) 2014 M. E. O'Neill，按 Apache 2.0 保留 [notice](docs/licenses/PCG-NOTICE.txt) 和 [完整许可](docs/licenses/PCG-APACHE-2.0.txt)。

控制台构建使用 devkitARM、libctru、Citro2D、Citro3D 及 newlib / GCC 运行库。许可目录保存 libctru 2.7.0、Citro2D 1.7.0、Citro3D 1.7.1 的 zlib 条款，GCC 16.1.0 的 GPLv3 与 Runtime Library Exception 3.1，以及 newlib 版权汇编。newlib 汇编与具体所链接对象的完整对应仍需核对；更新 SDK 时需检查相应版本的许可。

CMake、makerom / CTRTool / bannertool、Godot、FFmpeg、FreeType、Pillow 和 fontTools 用于开发、导出或核验，工具二进制不随设备程序分发。FFmpeg 离线解码 PCM，设备程序不链接 FFmpeg。设备 DSP 固件和 Nintendo SDK 不随本项目提供。

`assets/icon.png`、`banner.png` 是本项目工具生成的原创几何图形，`silence.wav` 为静音；CIA 不包含 Nintendo logo。

## 分发材料

设备包携带根 `LICENSE`、本文及 [docs/licenses](docs/licenses/README.md) 中的许可材料，官方原文的来源和 SHA-256 记录于 [license-sources.json](docs/licenses/license-sources.json)。独立运行包保留可阅读的版权与许可，不仅提供源码链接。素材、混合字体和具体运行库对象的未确认范围见以上说明。
