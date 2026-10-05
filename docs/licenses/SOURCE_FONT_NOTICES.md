# 已核对的运行字体来源

以下字体来自固定上游 `7d9246600fffe518408f5830d4848635019005a3`。版权和许可字段读取自原始 TTF 的 `name` 表，与 `content/asset-receipts/fonts/source.json` 和 `content/asset-receipts/fonts/introduction/source.json` 一致。

| 上游文件 | SHA-256 | 嵌入版权字串（原样） | 已确认许可范围 |
|---|---|---|---|
| `Fonts/EBMain.ttf` | `7145045d455981991b0af5a1ba2967fb0d34156809984c26cc65423855e22ec8` | `NintendoTeamEnco` | 无独立许可字段；保留上游游戏相关用途条件，原权利人的独立授权未确认 |
| `Fonts/EBMain_fw9.ttf` | `72dfce0af15df8de28ef581def64dad6abfff808dd0b7ebdeb1eb1a4de5cfcdc` | `Nintendo` | 同上 |
| `Fonts/EBMain_jaM3.ttf` | `f3dd8efc820ea5243b86827e0faf8b27e4366436af0f7a7719bedafcbc4d9089` | `Nintendo` | 同上 |
| `Fonts/EBMain_ko.ttf` | `275fb7e6c6534823ddfc194b55c9a5c2184779c7db78b3b36d1439b3ef036411` | `NintendoQuipleGa` | TTF 无独立许可字段；上游来源文档将韩文字形指向 Galmuri9 / OFL，其他来源不能由此一并授权 |
| `Fonts/EBMain_zh_cn.ttf` | `187143504eba58eab01fac1e661dda24f49873a2dbf74edb61dbb34b774f2e99` | 见下方完整 notice | 嵌入 SIL OFL 1.1 声明与官方许可链接 |

## 简体中文 / Fusion Pixel

原 TTF 嵌入的版权声明：

> Copyright (c) 2022, TakWolf (https://takwolf.com), with Reserved Font Name "Fusion Pixel"

作者和制造者字段均为 `TakWolf`；许可字段为 `This Font Software is licensed under the SIL Open Font License, Version 1.1`；许可链接为 `https://github.com/TakWolf/fusion-pixel-font/blob/master/LICENSE-OFL`。原字体 family name 是 `Fusion Pixel 10px Mono zh_hans`。

完整官方文本见 [Fusion-Pixel-LICENSE-OFL.txt](Fusion-Pixel-LICENSE-OFL.txt)，来自官方项目固定提交 `8ba7717c18a4023ac2162694aad10af72ebd9356` 的原文；并未证明它就是该 TTF 当年的具体构建提交。当前官方文本不再在版权行注明 RFN，本项目仍保留固定 TTF 中原有的 `Fusion Pixel` 保留字体名条件。

本项目把选定字形栅格化、汇总为可复用的字体 atlas 和二进制字形资源；按字体转换/子集处理，保留 OFL 与版权，不把它们作为 MIT 代码或仅一张普通插图。OFL 部分不添加仅限本游戏的再许可条件；改变格式不使字体退出 OFL。修改版不使用保留字体名作为面向用户的主字体名。依据为 [官方 OFL FAQ](https://openfontlicense.org/ofl-faq/)，特别是 1.10、1.20、1.21、5.1；不改变聚合的 C++ 代码许可。

## 韩语 / Galmuri

固定上游 [Fonts/doc_fonts.txt](https://github.com/motherencore/MOTHER-Encore-Source-Code/blob/7d9246600fffe518408f5830d4848635019005a3/Fonts/doc_fonts.txt)（SHA-256 `7936557bf77e34c2d559c6faf79659327d93a407e940d2b378364dfe2919f959`）将 EBMain 的韩文字形指向 DS 风格的 Galmuri9，明确写明 OFL。官方 Galmuri 项目当前固定提交 `71e1cacf1437a11220307120e63e30bc275312d4` 的完整许可和版权见 [Galmuri-LICENSE-OFL.md](Galmuri-LICENSE-OFL.md)。版权为 2019–2025 Lee Minseo；仅用于信用和许可说明，不代表作者背书。

这证明应保留 Galmuri 字形部分的 OFL，而不是证明当前混合字体每个字形均来自 Galmuri，或已经找到该 TTF 所使用的历史 Galmuri 版本。上游的合并工具只提供处理机制，没有完整的逐字形历史来源映射。不能由 Galmuri 的 OFL 推导 Nintendo 来源部分的许可。

## 尚待确认的权利

Latin、日本语、全宽字形的来源文档指向 EarthBound / MOTHER 3 等游戏，其 TTF 没有独立许可字段。本项目保留来源和上游条件，没有独立 Nintendo 授权文件。字体来源与混合字形的授权边界仍需单独确认。

Introduction 另使用原版 EarthboundZero 与 BottleRocket 定义及其完整 fallback 链；已核对来源如下。Proggy 尚未用于运行资源。上游游戏素材用途条件与已经核实的 OFL 许可分别保留，不能把整个字体集合概括为一个许可证。


## Introduction 字体

| 上游文件 | SHA-256 | 嵌入版权字串（原样） | 已确认许可范围 |
|---|---|---|---|
| `Fonts/BottleRocket.ttf` | `2363dad560703b9285c108a01255934a358f4ba8ff0043a80a8d43fd9bdc2ee1` | `NintendoTeamEnco` | 无独立许可字段；保留上游来源与用途条件，独立授权未确认 |
| `Fonts/BottleRocket_ja.ttf` | `79bd0926501b466d43b28fca4f3ab5ef98bc2f5a61cdf6abfafa03ea43fd65db` | `NumKadomaMisakiG` | 无独立许可字段；保留上游来源与用途条件，独立授权未确认 |
| `Fonts/BottleRocket_ko.ttf` | `0371809423fadc5f5a1abd5e0b3c3dbafd6fd12874264ee470a40ba9a5f873f5` | `NintendoQuipleGa` | 无独立许可字段；保留上游来源与用途条件，独立授权未确认 |
| `Fonts/BottleRocket_zh_cn.otf` | `529b7651c870779f321083e1f0d644a6db7d49c3a1aa4387a1ca1faa4e0e84f5` | `Copyright (c) 2022, TakWolf (https://takwolf.com), with Reserved Font Name "Fusion Pixel"` | 嵌入 SIL OFL 1.1；完整 Fusion Pixel 声明和许可见上文 |
| `Fonts/EarthboundZero.ttf` | `decf81bbe4f7c36bb7472be05c00db1dd86739bd14863b53d0d7d79c68315fb8` | `NintendoTeamEnco` | 无独立许可字段；保留上游来源与用途条件，独立授权未确认 |
| `Fonts/EarthboundZero_zh_cn.otf` | `10ae119fe7d1460444f78a312399bb0962846644cab94a2161456e4a019ac7b1` | `Copyright (c) 2022, TakWolf (https://takwolf.com), with Reserved Font Name "Fusion Pixel"` | 嵌入 SIL OFL 1.1；完整 Fusion Pixel 声明和许可见上文 |

EarthboundZero / BottleRocket 中文字体同样嵌入上文的 Fusion Pixel 完整版权声明、保留字体名和 OFL 1.1 链接，作者与制造者字段为 TakWolf；对应转换资源保留同一声明和完整 OFL 文件。BottleRocket 日文字体的 `NumKadomaMisakiG` 字串保留为原样来源证据，未据此推导独立许可。EBMain 的五项 fallback 来源与许可见首表。
