# 内容与绑定迁移

严格程序 / 数据分离覆盖实际运行内容，也覆盖编译工具中的绑定和调参。已存在的开局、战斗、房屋、物品、电话、命名、设置、语言和存档资源继续使用各自独立格式；不能把这些资源的存在视为全范围迁移完成。

## 原生入口资源绑定

`content/native-resource-catalog.json` 声明原生入口的稳定资源角色、相对路径和显式 battle / round 配对。它是已支持范围的原生连接适配数据，不是上游 Godot 文件格式，也不增加玩法范围。`tools/resource_catalog.py` 核对来源指纹，从现有受检二进制生成 `data/native.encresources`，记录各文件大小与 CRC。普通编译不重新提取或覆盖 IR。

共享加载器拒绝未知版本、能力、来源身份和角色、重复身份 / 路径、非法路径 / 后缀、坏配对、截断、损坏与非零保留字段。失败保留既有目录。启动在构造游戏流程前检查目录引用的文件；内容仍需经过各自的语义加载器。CRC 用于损坏检测，不是认证。目录拥有路径字符串，启动后保持不变，RomFS 不提供覆盖或热更新。

设备入口的标题与语言、六字段命名与设置、房屋、战斗、Items、电话、Dad Record、Continue / LOAD 和加载指示器使用目录角色。遭遇驻留准备和切换使用显式配对，不能根据文件名猜测伴随资源。平台仅保留 RomFS 前缀与目录引导位置，以及 SD 存档路径等平台契约。

主机 A/B 回归用同一执行文件读取重新绑定的 round 资源，并驱动实际 `BattleRound` 消费者。它证明资源绑定可在不编译 C++ 的情况下改变；不代表模拟器或真机运行。目录格式与能力独立于既有 pack、rules 和 save schema，本次不改变存档身份、随机流或剧情时序。

## 剩余范围

| 范围 | 当前位置 | 下一步 |
|---|---|---|
| 设置预览长度阈值 | `runtime/new_game_setup.cpp`，`TextSpeed.gd` | 提取源阈值到设置 IR / 二进制，校验并替换消费者 |
| 法语与德语文本规则 | `runtime/localized_presentation.cpp`，`text_tools.gd` | 元音 / 词尾集合和输出后缀迁到语言来源绑定；保留未知标签拒绝 |
| 战斗演出编译配方 | `tools/round_assets.py` | 已编入外部 IR 的手写轨道、布局、绑定和调参进一步迁到独立受检配方 |
| Boss 演出及对象 / 程序顺序 | `tools/doll_round.py`、`tools/extract_battle_round.py`、`tools/extract_native_content.py` | 外置剩余来源绑定、默认调参及稳定顺序，保持现有身份 |
| 设置编译绑定 | `tools/startup_settings_assets.py` | 外置角色顺序与皮肤角色映射，逐项核对继承来源 |

加载上限、schema 编号、stride、sentinel、RNG 算法、插值数学、物理帧契约和平台预算属于机制或校验边界，不应机械地当作游戏内容迁移。M0 fixture 独立于以上实际游戏流程。完整迁移和 Old / New 3DS 真机验收仍未完成。
