# 固定上游与初始移植审查

本文记录初始 M1/M2 阶段的来源和技术审查范围，不代表当前完整功能状态。
后续已集成的房屋、战斗、字体和音频范围见 [STATUS](STATUS.md)。

当前来源固定为官方 Act2 v0.4.1.0；后续上游更新须重新审查实际差异。
官方 itch 下载提供 Act 2 v0.4.1.0，发布公告介绍这次更新；官方源码声明同一版本。
本项目固定实际提交
`7d9246600fffe518408f5830d4848635019005a3`（tree `dab3bb25b417d2273a35679a3edfbc45a597dcf9`）。
对应关系来自公开版本名称和源码常量；没有声称官方认证下载二进制对应这个提交。
来源：[官方下载](https://mother-encore.itch.io/mother-encore)、
[更新公告](https://mother-encore.itch.io/mother-encore/devlog/1630037/fall-2026-news-update)、
[固定源码](https://github.com/motherencore/MOTHER-Encore-Source-Code/tree/7d9246600fffe518408f5830d4848635019005a3)。

## 已取得与审计范围

`upstream/MOTHER-Encore/` 为真实干净 Git checkout，只读使用。
独立官方下载归档逐文件与 Git 比较：5,839 blobs、267,289,095 bytes 全部相同。

`compatibility/upstream-inventory.json` 包含所有 tracked blobs，包括上游 tracked `.godot/` 文件、
2,022 个导入元数据、342 个场景、315 个 GDScript、951 个 YAML、4 个 ECS、25 个字体。
拒绝脏 checkout、未跟踪/ignored 额外文件、特殊 Git mode、无效 UTF-8 脚本；不静默跳过 tracked 缓存。
分类不代表语义兼容；全量内容仍需逐项绑定、审查和实现，未知行为由门禁阻断。
上游无扩展名 `Graphics/Rooms/Spookane_Hotel` 经 PNG signature 识别为纹理，仍须明确路径绑定。

初始静态扫描记录的能力下界为：62 类节点标签（含继承/实例项）、
10,277 个保存的节点记录、97 个顶层脚本类、377 个保存的信号连接。
记录实例、TileMap、动画轨道、音频、Shader、等待、Tween、反射、动态加载、输入、存档和 RNG 的位置。
注释/字符串可能产生过计数，实例默认行为与动态依赖也可能漏计，不能把正则统计当作语义解析。
2 个 MBG 是二进制，明确保留待审查；导入缓存引用、目录/拼接路径及未解析资源分别列出。
字体来源表包含 MOTHER 游戏字形；独立许可和授权范围见 [字体说明](licenses/SOURCE_FONT_NOTICES.md)。
Distortionator 插件已存在并在项目启用，但 plugin.cfg 的 version 为空；README 要求 1.0.5，
目前未把嵌入插件称为已经独立证实的 1.0.5。

## 第一项原版规则：经验与等级

逐项审查 `Scripts/global/PartyMember.gd` 的 `LEVEL_CAP=30`、`_level_to_exp`、`_exp_to_level`。
等级1阈值为0，超过等级上限使用等级30阈值，整数转换截断，经验恰好达到下一阈值时升级。
原函数与常量原样抽取进独立 Godot Reference 类，排除未审查的库存/globaldata 等依赖；
这属于孤立函数参考执行，不等于完整原版游戏运行。
官方 Godot 3.6.2 Windows/Linux 对 20,927 个连续 XP、61 个等级及3个有符号边界输出完全相同。
生成 `tests/fixtures/progression_v0410.hpp`，共享核心 `runtime/progression.cpp` 对照 21,058 checks 通过。
非正等级在原生 API 明确拒绝，不声称支持上游公式的非游戏输入域。
完整来源指纹、符号范围与排除项见 `compatibility/reviews/progression-v0410.json`。
未批准整个 PartyMember 类，也未将这套规则冒充为 M0 训练战斗规则。

## M2 首步：开局底图与 3DS 查看模式

`Maps/podunk/Nintens House.tscn` 的背景 Sprite 引用
`Graphics/Rooms/Podunk/Ninten's House.png`，中心 (312,552)，原图 624×1104。
已核对 `Data/save_new_game.yaml` 从这张地图的 (520,404) 开始。
独立查看器以该位置附近为初始窗口；这不是原版 CameraArea 行为实现。
底图按256像素网格拆成15片，RGBA像素重建逐字节一致，由真实 tex3ds 转为 RGBA8，
无有损压缩，绘制使用 nearest 采样。避免超过3DS单张纹理尺寸。
每张纹理独立加载，保留 tex3ds 原始 UV，由上屏400×240窗口裁剪，像素1:1；
无自加标题、说明条或留白边框。
输入只平移查看窗口，不模拟角色移动。源地图中房间之间的空白保留。
源图、场景、转换配方、PNG片及T3X都有指纹，打包前验证对应的 pin 和全部纹理字节。
未知版本、变化后的来源、残缺/额外文件均阻断，加载异常明确报错。

带纹理的构建启动优先进入真实底图查看模式，SELECT 可回到原创沙盒。
这是资产转换/平台显示的第一步；TileMap家具、继承对象、角色、碰撞、交互、剧情和动画尚未转换。
没有将未支持行为从场景中忽略后宣称可玩；尚无完整场景导入适配器。
纹理权限范围与源图/场景哈希见 `compatibility/reviews/house-background-v0410.json`。
该早期底图查看器只使用地图纹理，许可按游戏相关用途条件保留。

重新生成底图（需要 Pillow，工具不进入运行包）：

```powershell
python tools/map_asset.py prepare
```

在已配置的 devkitPro 环境中执行 `make map-assets` 后构建 3DSX / CIA。
图片转换、设备呈现和完整游戏行为对照是不同的验证范围。

## M2 场景数据进展

新增 `tools/scene_reference.py`、`run_scene_reference.py` 和 Godot `scene_data.gd`。
源文件只读，71个静态依赖先经过清单哈希校验；只在 build/ 副本中隔离脚本和信号。
嵌入的 MusicChanger GDScript、原始脚本挂载、自定义字段和连接均保留位置/指纹，仍列为机制阻断。
字体 font_path 的原生加载依赖单独收集；临时引用的字体、声音没有进入发布路径。

原生数据导出已经解出497节点、381资源、21个 SceneState 和三个 TileMap 的1/14/276格子。
此处替代上文“家具/继承/碰撞未转换”的数据层状态，3DS绘制和游戏行为状态不变。
小型样本核对继承、实例覆盖、脚本样本默认值、世界坐标、shape owner、int64和信号；
不支持的 Variant 和 InstancePlaceholder 均失败且没有部分输出。
特别记录了未进树时 shape owner 坐标缓存没有追上实例覆盖的问题；
导出保留缓存，并用展开后子节点的变换提供几何坐标，没有通过改期望掩盖差异。

该阶段的受检场景参考位于 `reports/m2-scene-reference-reviewed/`。
这不是完整原版运行参考，也不是可玩地图或 M2 整体验收；
25个脚本挂载、29条信号声明以及动态依赖/方法轨道/脚本默认值仍有明确阻断。
具体数据格式、边界、复现方式和下一项运行时接入见 `SCENE_CONVERSION.md`。
