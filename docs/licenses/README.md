# 第三方许可材料

本目录保存第三方许可原文、字体来源与转换说明。官方原文保持原始字节；来源、版本和 SHA-256 见 [license-sources.json](license-sources.json)。根目录 MIT 不重新授权这些内容。

| 文件 | 来源与范围 |
|---|---|
| `MOTHER-Encore-0.4.1.0-LICENSE.txt` | 固定上游 7d924660…；代码声明和游戏素材条件 |
| `Fusion-Pixel-LICENSE-OFL.txt` | TakWolf 官方完整 SIL OFL 1.1 与版权；原 TTF 的保留字体名见字体说明 |
| `Galmuri-LICENSE-OFL.md` | Quiple 官方完整 SIL OFL 1.1 与版权 |
| `SOURCE_FONT_NOTICES.md` | 固定字体的版权、来源、转换范围及未确认权利 |
| `Godot-3.6.2-LICENSE.txt` | Godot 3.6.2 MIT，适用于派生机制 |
| `PCG-APACHE-2.0.txt`、`PCG-NOTICE.txt` | Apache 2.0、PCG / Godot 来源及本项目派生说明 |
| `libctru-2.7.0-README.txt` | 官方 v2.7.0 README，含 zlib 许可 |
| `citro2d-1.7.0-LICENSE.txt` | 官方 v1.7.0 zlib 许可 |
| `citro3d-1.7.1-LICENSE.txt` | 官方 v1.7.1 zlib 许可 |
| `COPYING3`、`COPYING.RUNTIME` | GCC 16.1.0 GPLv3 与 Runtime Library Exception 3.1 |
| `COPYING.NEWLIB` | 官方 newlib 版权汇编 |
| `CMake-3.31.10-Copyright.txt` | 官方 CMake 3.31.10 BSD-3 版权与免责声明 |

项目使用上游派生的地图、人物、剧情、战斗、音乐和字体资源，其范围见 [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)。字体的独立 OFL、来源信用及尚待确认的授权分别记录于 [SOURCE_FONT_NOTICES.md](SOURCE_FONT_NOTICES.md)。

SDK 许可材料对应 libctru 2.7.0、Citro2D 1.7.0、Citro3D 1.7.1 和 GCC 16.1.0。`COPYING.NEWLIB` 尚未逐个对照 devkitARM newlib 的补丁和实际链接对象；更新构建依赖时应分别核对版本与适用条款。

设备包携带根 `LICENSE`、`THIRD_PARTY_NOTICES.md` 及适用许可材料，构建和打包时校验原文指纹。开发工具、SDK、设备固件和个人字体不因提供许可文本而成为发行包内容。
