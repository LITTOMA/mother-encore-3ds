# 内容与绑定迁移

严格程序 / 数据分离覆盖已接入开局的实际内容，以及转换工具中的绑定和调参。JSON 是构建时的来源、IR 和有限转换配方；3DS 不解析这些 JSON。构建器生成独立二进制，共享核心受检加载后驱动实际游戏流程。资源格式、能力、规则和存档身份各自版本化。

## 原生入口资源绑定

`content/native-resource-catalog.json` 声明原生入口的稳定资源角色、相对路径和显式 battle / round 配对。它是已支持范围的原生连接适配数据，不是上游 Godot 文件格式，也不增加玩法范围。`tools/resource_catalog.py` 核对来源指纹，从现有受检二进制生成 `data/native.encresources`，记录各文件大小与 CRC。普通编译不重新提取或覆盖 IR。

共享加载器拒绝未知版本、能力、来源身份和角色、重复身份 / 路径、非法路径 / 后缀、坏配对、截断、损坏与非零保留字段。失败保留既有目录。启动在构造游戏流程前检查目录引用的文件；内容仍需经过各自的语义加载器。CRC 用于损坏检测，不是认证。目录拥有路径字符串，启动后保持不变，RomFS 不提供覆盖或热更新。

设备入口的标题与语言、六字段命名与设置、Introduction、房屋、战斗、Items、电话、Dad Record、Continue / LOAD 和加载指示器使用目录角色。遭遇驻留准备和切换使用显式配对，不能根据文件名猜测伴随资源。平台仅保留 RomFS 前缀与目录引导位置，以及 SD 存档路径等平台契约。

主机 A/B 回归用同一执行文件读取重新绑定的 round 资源，并驱动实际 `BattleRound` 消费者。它证明资源绑定可在不编译 C++ 的情况下改变；不代表模拟器或真机运行。目录格式与能力独立于既有 pack、rules 和 save schema，本次不改变存档身份、随机流或剧情时序。

## 设置文字预览

`TextSpeed.gd:_is_animation_worth_it` 的三项翻译标签共同长度阈值已提取到设置 IR，并由 `ENCSETUI` 格式 2 加载后交给命名流程的实际设置预览消费者。它按 Unicode 字符数比较；任意标签未超过阈值就立即显示整条文字，否则保留源严格时间门和逐字预览。受检资源默认仍为 5，改变阈值无需重新编译 C++。新增字段缺失、越界、损坏、未知格式 / 能力明确拒绝，失败保留原设置。能力 1、rules 和存档 schema 独立保持原值。

法语省音元音、有限大小写映射、德语词尾及输出后缀已迁入独立语言 IR 和受检词缀二进制块，实际 House / 战斗文本消费者使用该资源。完整源元音集合修复了原先只匹配 ASCII 元音的差异；法语 / 德语语言选择仍未启用。来源审计、格式边界与 A/B 消费验证见 [LOCALE_AFFIX_DATA.md](LOCALE_AFFIX_DATA.md)。

## 回合动作来源绑定

`content/battle-round-bindings.json` 独立声明已有回合的技能稳定顺序、角色、来源文件、基本 / 防御动作常量、菜单角色及 Boss shake 默认参数。提取工具按绑定读取原来源，核对常量值、实际动作分支与 shake 默认 / 机制片段；普通回合编译核对绑定与既有 IR，不能仅修改来源指纹就放行语义变化。基本 / 防御技能和菜单不再由提取工具内的名字列表或索引猜测；Doll shake 默认权重不再是手写数值。

既有 IR 与 `ENCRND01` 输出逐项、逐字节保持一致，实际 `BattleRound` / 演出消费者继续使用同一资源格式；本切片不扩展技能、Boss 生命周期或存档 schema。Boss 演出及世界对象 / 程序顺序分别使用下述独立绑定。

## 回合演出与其他已接入绑定

演出配方已迁入 `content/round-presentation-recipe.json`：完整 40 个媒体、127 项操作、160 项来源事实覆盖胜利前、胜利与返回，媒体 / 绑定 / 轨道 / 事件 / 布局 / 参数 / 技能媒体由来源表达式、节点、动画轨道、图集单元和明确的原生呈现政策组成。`round-animation-bindings.json` 独立声明动画属性 / 方法 / 图集 / 插值及已审计音频限制。编译器只执行受限事实引用与坐标计算，未知字段 / 操作 / 引用拒绝；不执行 GDScript。

普通回合编译先重新提取并核对完整演出报告和现有回合 IR，配方合法但 IR 陈旧也明确拒绝，既有输出保留。默认报告、IR 和二进制保持一致。同一真实消费者分别执行原版攻击对白，以及胜利 → EXP 文字与确认 → 返回回调 → 400×240 世界投影；编译出的布局 / 目的地差异资源改变姿态而不改变源事件顺序或随机流。音频来源核对尚不等同于相应原生播放实现。

设置行 / 面板顺序、值翻译键、确认卡片 / 图标、继承标签源、九宫格角色及已有皮肤 manifest 映射已迁入 `content/startup-settings-bindings.json`。`startup_settings_bindings.py` 在提取、普通编译和 staging 前核对实际节点顺序、设置索引分支、命名 scenario、标签继承、材质继承和纹理身份；未知、缺失、重复或不完整映射拒绝。默认设置 IR 与 `ENCSETUI` 格式 2 字节不变。


Boss 演出 / 进度来源绑定已迁入 `content/boss-presentation-bindings.json`：场景创建与实例引用、动画 / 属性 / 方法 / signal / 音频来源、保留角色 / collision body、升级与学习身份、统计顺序及文本键都受来源语义核对。提取器和普通回合编译共用门控，默认 Doll IR 与 19,844 字节资源保持一致。已审计的 Boss 音频轨道仅保留来源核对，尚无对应原生播放映射；本次不宣称实现或验收该音频。未知轨道 / 方法 / 来源仍拒绝。

电话纹理、声音、Idle / Ring 动画与事件绑定已迁入 `content/phone-presentation-bindings.json`。提取器从完整动画键生成事件；普通电话编译重新核对脚本表达式、来源轨道及 Carol 的调用 / 旗标关联。未知、损坏及合法但陈旧的映射在替换输出前拒绝。默认呈现 IR 与 `ENCPHN` 字节不变；显式重新生成的资源进入既有 `PhoneRuntime`，与原始 AnimationPlayer 的 360 帧参考对照。电话剧情前端使用下述 programme 配方。

Items 的资产源 / 输出 / 裁剪、节点 / 布局、cursor 与 Info tween、声音和库存身份已迁入 `content/items-presentation-bindings.json`。普通编译重新提取并核对来源与 IR；坏映射或陈旧输出在替换包前拒绝。默认纹理与 5,596 字节资源保持一致。同一真实 Items 消费者读取改变 Info anchor / hint offset 的资源，库存身份、声音、时间线与 RNG 保持一致。

世界程序、对象、演员、NPC receipt 顺序、动画方向、资源身份与遭遇连接已迁入 `content/world-program-bindings.json`。实际提取器与普通 rules7 编译核对绑定、完整来源及 Resource / Program / Actor / Profile / Animation / Encounter 引用。保持现有身份和内容载荷；来源与编译器改变产生的 header / build-input 摘要如实重新生成，独立的 rules6 历史资源保持冻结。剧情前端发出的命令使用下述独立配方。

## 世界命令、入场与房屋

`programme-lowering-recipe.json` 声明已审查的剧情命令模板、演员、文本、声音和时间参数，包含 Lamp、Doll、Melody 与电话线性节目。Dad-normal 的节点、选项、旗标例外和 leader 分支直接读取同一受检来源元数据。转换器只执行受限引用与分支语法；普通 Room 编译重新提取并比较完整命令和字符串，不能只刷新来源摘要而保留陈旧内容。

`battle-entry-bindings.json` 覆盖三种已有遭遇共享的入场动画、菜单、UI 资源角色和源参数。它驱动真实提取器与普通编译门控；同一 `BattleEntry` 消费者读取改变 plate margin 的资源。默认入场内容保持一致，不增加缺失战斗机制。

`house-source-bindings.json` 声明 NPC、房门、可开启对象、剧情触发、文本和已有呈现绑定。House 的提取、追加连接与普通编译核对源与身份；真实 Carol / Phone programme 以及改变显示 anchor 的 House 资源进入同一实际核心，加载损坏数据保留原状态。

## 命名与菜单

追加连接使用 `phone-linker-bindings.json` 和 `pillow-source-bindings.json`：Carol 演员身份、已有 16 项世界音频的来源 / PCM 路径 / 增益 / 转换政策，以及 Pillow / Minnie 的命令、教程选项和遭遇身份均在独立数据中。连接器核对实际 Room 资源、NPC 默认方向及已有世界 / House 命名空间；普通 Room 和音频编译拒绝陈旧内容或不匹配的引用。

`naming-presentation-bindings.json` 声明六字段命名的源节点、输入路径、资源、画布、布局、文本、声音、有限键盘导航和原生呈现政策。`ENCNAMES` 格式 3 的受检尾部交给共享 `NewGameSetup` 和 3DS 渲染器；能力仍为 2。旧正文保持逐字节等价；旧格式 2、未知格式和不完整 / 非法引用明确拒绝。尾部角色变化无需改 C++。

Continue、Save 和按钮提示分别使用 `continue-presentation-bindings.json`、`save-presentation-bindings.json`、`prompts-presentation-bindings.json`。转换器核对实际来源、引用与源布局；普通编译及 staging 拒绝合法但陈旧的 IR。Godot 仅用于离线参考探针，设备不运行 Godot。

## 原版 Introduction

`introduction-bindings.json` 与 `introduction-assets.json` 声明两场景来源、字幕键与语言字体、图片、动画、门过渡及音频身份。受检 IR 编译为 `opening.encintro`；CTR 使用原版 11 张纹理、31 帧云图集和独立字幕字体，按 400×240 居中保留图素 1:1。历史介绍与 Mt. Itoi 的自然播放、逐字字幕、共享音调 RNG、源跳过与最终进入房屋均由消费者执行。五项音频追加到既有受检 bank；JSON 与来源收据位于构建输入目录。

房屋创建与原版最终门口换场点一致，玩家在淡入的 mostly-done 信号后解锁。历史动画长度 116 秒，116.5 秒键保留为来源事实并不执行。格式、能力、来源、路径、引用、浮点数和未知轨道 / 方法均受检，失败保留已加载资源。

## 验收边界

`playable_opening` 自动测试从标题的 NewGame 边界进入真实六字段命名与设置，同一 House owner 完成 Lamp / Doll 实际回合、奖励与战后演出，再执行 Melody / 电话 / Dad Record，写入真实临时文件，经 Continue LOAD 读取同一文件并准备、提交新的 House owner。该入口现经真实 Introduction / Mt. Itoi 消费者及原版两段跳过，再准备 FreshHouse；独立 Introduction 测试另跑完整英文 / 中文自然时间线。后半 NPC / 电话定位使用公开 warp 接口，不能证明全程步行或设备画面。

加载上限、schema 编号、stride、sentinel、RNG 算法、插值数学、物理帧契约和平台预算属于机制或校验边界。M0 fixture 与上述实际流程分开。绑定迁移不代表完成 Podunk、完整战斗或尚未映射的音频播放；Introduction 的设备显示与声音仍需实测。

电脑上的共享核心正常 / 负向自动测试和同一执行文件的资源变化检查证明核心消费者行为；真实 3DSX / CIA 构建和嵌入资源检查证明交叉构建与打包。它们不能代替模拟器、Old / New 3DS 的画面、声音、输入和存档验收。对应提交的实际检查结果见 PR 与 Actions，原始日志保留在仓库外的私有构建目录。

## Podunk 室外场景

`content/podunk-field.json` 声明场景类别、物件精灵、跨场景路线、语言与输出路径；格子、图块形状、精灵、门与边界在每次编译时直接从固定上游提取（批量数据例外见 [ARCHITECTURE](ARCHITECTURE.md)）。`tools/podunk_field.py` 生成 `data/podunk.encmap`（ENCMAP01）、`data/podunk.encroom`、`data/world.enclinks`（ENCLNK01）与 `graphics/world/podunk` 图集，收据位于 `content/asset-receipts/graphics/world/podunk/source.json`。资源目录能力 2 新增 FieldRoom / FieldMap / WorldLinks 三个角色。房屋 `.enchouse`、存档格式与既有身份不变；旗标按名称与稳定 ID 跨场景，Podunk 房间必须携带与房屋相同的旗标表。范围与限制见 [PODUNK_FIELD](PODUNK_FIELD.md)。

## 自由行走装备菜单

`native-field-equipment.json` / `field-equipment-assets.json` 从固定上游 Pause / Equip 场景、脚本、动画、翻译、库存与角色属性提取完整局部来源。`field_equipment.py` 普通编译重新对照来源与 IR，编译独立 `data/opening.encfield`（ENCFIE01，格式 / 能力 1），纹理归入 `graphics/ui/equipment`。`field-audio-binding.json` 补入两个原版 Pause 音效，其他菜单音效复用受检音频 bank 的稳定身份；PCM 位于 `sound/effects`。JSON、转换配方及收据均不进入 RomFS。

共享消费者执行六项 Pause 命令的导航、已开放 Equip 的四槽、适用库存与 None、来源七项属性预览及确认。3DS 平台以独立候选库存与既有 Session 派生机制校验后提交，UID、剂量、物品数量、存档编码与随机流保持原有身份。其他命令、队员、装备、物品使用仍受限；具体范围见 [FIELD_EQUIPMENT](FIELD_EQUIPMENT.md)。
