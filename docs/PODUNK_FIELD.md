# Podunk 室外场景（出门切片）

固定来源：Mother: Encore `7d9246600fffe518408f5830d4848635019005a3`，主要场景 `Maps/podunk/podunk.tscn`、`Maps/podunk/Nintens House.tscn`、`Tilesets/Podunk.tres`、`Scripts/Main/Door.gd`、`Scripts/global/SceneTransition.gd`、`Scripts/Main/Openable Door.gd`、`Scripts/Main/Flag Landmarks.gd`、`Scripts/misc/grass spawner.gd`。

本切片让房屋正门成为真正的跨场景门：和爸爸通话后（`DoorBlock` 由 `talked_to_dad` 移除，原有行为），走进 `Doors/Podunk` 即按原版转场进入 Podunk；从 Podunk 的 `Objects/Doors/NintensHouse` 可回到房屋。它不代表 Podunk 已可游玩。

## 可玩范围

- 房屋 → Podunk：原版 `Door.gd` 顺序：暂停、等待一个 idle 帧、`good_morning=false`、0.8 秒音乐淡出、Circle Focus 淡入（速度 1.5）、换场、落点 `(8, -39)` 朝南、两个 settle 帧、Circle Focus 淡出，在 fade-out 的 mostly-done 关键帧解除暂停。
- Podunk → 房屋：落点 `(136, 809)` 朝北，同样的 Circle Focus 转场；房屋按当前会话、旗标和背包经 LOAD 同一受检路径重建（不执行 LOAD 的源 UID 重分配）。
- Podunk 全部 44 个 TileMap 层、169,977 个格子、水面 AnimatedTexture 四帧动画、按原版 VisualServer 规则的 Y 排序与 z_index。
- 图块碰撞（凸形状、Godot 运行时凸分解、ConcavePolygonShape2D 线段）与场景/物件静态碰撞体；玩家碰撞掩码 4353。
- 1,058 丛草的空闲帧：贴图编号与翻转由 `seed(name.hash())` 后的 `randi()` 在编译时精确计算。
- 静态物件精灵（枯树丛、礼物盒、公用电话、ATM、自动售货机、动物园大门、掉落物）的初始帧与碰撞。
- 未上锁的可开启门：进入区域隐藏门板，离开 0.3 秒后关闭，与房屋同一脚本语义。
- 英文 / 简体中文路牌：`project.godot` 的 `translation_remaps` 在编译时解析为每种语言的图块。

## 明确的开发边界

按严格程序 / 数据分离与 fail-closed 规则，`podunk.tscn` 展开后的 8,686 个节点全部归入配方类别；未归类节点使编译失败。

- 阻挡边界（接触即暂停，下屏说明，B 回到上一个安全点）：13 扇通往建筑 / Merrysville 的门、除 Mick 南栅栏以外的过场区域、跳跃区、桥层切换、事件触发区、上锁的门。房门南侧 `Cutscene Area11` 在 `gave_treats` 之前改为 `mick_bark` 往北推回，给粮后消失，见 [Mick](MICK_TREATS.md)。
- 提示（下屏列出附近未移植对象，不生成）：其余 NPC、敌人生成器、鸟、蝴蝶、蒲公英、调查点、音乐区、楼梯、踏步音、闪光、野外礼物盒与公用电话等交互。Mick（npc21）已移植漫步与已审查对话，见 [Mick](MICK_TREATS.md)。房屋内四个礼物盒已绘制，仅 Present4 可领取狗粮，见 [房屋礼物盒](HOUSE_PRESENTS.md)。
- Podunk 中不开放 START 菜单、存档与战斗。

## 数据与格式

`content/podunk-field.json` 只保存审查过的策略：场景类别、物件精灵、路线、语言与输出路径。`tools/podunk_field.py` 每次编译直接读取固定上游（`Extractor` 核对清单 SHA-256 与 checkout 干净），不保存中间 JSON 转储。输出：

- `data/podunk.encmap`（ENCMAP01，格式 / 能力 1，21 段）：图集、帧、图片与语言变体、图块、碰撞模板、条件、图层、区块、格子、精灵、绘制条目与分组、可开启门、门、边界、提示、镜头区域、静态体。
- `data/podunk.encroom`（ENCRMD01 rules 7 / capabilities 8）：复用房屋 IR 的玩家配置、178 个旗标（同名同 ID）、规则与经验表，Podunk 场景身份 2，无剧情程序。
- `data/world.enclinks`（ENCLNK01）：场景（房屋 1、Podunk 2）与两条路线；所有门参数来自 `Door.gd` 默认值加实例覆盖。
- `graphics/world/podunk/atlas-*.t3x`：只打包实际用到的图像区域，tex3ds `-f rgba8 -z none`；收据 `content/asset-receipts/graphics/world/podunk/source.json` 记录解码后 RGBA 哈希、t3x 哈希与 tex3ds 哈希。当前为 1 页 1024×1024（t3x 4,194,325 字节，加载后约 4 MiB 线性内存）。

重新生成：`make podunk-field`（需要 devkitPro 的 tex3ds）。没有本机 devkitPro 时，可让主机 Python 绘制图集、由固定镜像中的同一个 tex3ds 转换：

```powershell
python tools/podunk_field.py compile --tex3ds build/tools/tex3ds --tex3ds-command "wslc run --rm -v D:/GameDev/3DS/mother-encore-3ds:/work -w /work encore-devkitpro:pinned /opt/devkitpro/tools/bin/tex3ds"
python tools/resource_catalog.py compile
```

`--tex3ds` 指向用于记录哈希的二进制（从镜像复制到 `build/tools/`，SHA-256 `58cecde4…c9e`，与其它贴图收据相同）；`--tex3ds-command` 只改变执行位置，文件参数为仓库相对路径。

资源目录 `native.encresources` 升至能力 2，新增 `FieldRoom=31`、`FieldMap=32`、`WorldLinks=33`；能力 2 必须同时包含三者，能力 1 拒绝它们。房屋 `.enchouse` 与存档格式不变。

这三个绑定声明在独立的 `content/field-resource-catalog.json`（kind `encore.field-resource-catalog.source-ir`，只允许场景角色，并固定 `Maps/podunk/podunk.tscn` 指纹），`tools/resource_catalog.py` 把它与 `content/native-resource-catalog.json` 合并编译为同一个二进制目录。原因：开局房间 IR `content/native-opening.json` 的来源记录固定了 `native-resource-catalog.json` 的字节哈希，并经由它级联到房屋、会话、恢复与三场回合的受审 IR；把室外场景放进该文件会迫使整条开局链重新提取。分离后开局链不依赖室外地图，基础配方保持与 main 相同的字节。

## 运行机制

- `FieldScene`（共享核心）拥有一个 `OpeningWorld`，通过 `MotionObstacleSource` 向 `StaticMotionSolver` 提供查询范围内的图块与静态体形状。
- `HouseRuntime::bind_scene_routes` 只把有受检路线的 `UnsupportedScene` 边界变成跨场景门，其余边界保持原开发停止。
- `SceneDoorTransition` 使用 `.encintro` 中同一套 uiManager fade 曲线与圆形遮罩着色参数；遮罩计算从 `Introduction` 抽出为共享函数，开场行为不变。
- 平台在换场时先准备新场景，失败则保留原场景并给出可按 B 退回的停止；进入 Podunk 时释放房屋地图贴图，回家时重新加载。

## 与原版的已知差异

- 进入 Podunk 时，原版在子节点先于父节点的 `_ready` 里抽取全局随机数，屏幕进入时草丛、鸟、蝴蝶、闪光、敌人生成器和漫步 NPC 再抽取。Mick 包把 `_ready` 之后的状态和首次视口重叠账本写进 `ENCMIK01`，只为对齐 Mick 自己的漫步等待；敌人不生成，其它 NPC 不走路。鸟离开屏幕后的再次 `_ready` 不在该账本里。
- 敌人生成器的 `randi` 已按 400×240 首次重叠写进 Mick 随机数账本；生成敌人与进入战斗仍是后续口径。
- 门音效（`M3/door_open.wav`、`Door_Short.mp3`）不在受检音频库中，只计数，与房屋同场景门现状一致；Podunk 音乐区未接入，场内无音乐。
- Y 坐标恰好相等时：TileMap 格子绘制项与子节点在 Godot 中按绘制下标排序且排序不稳定；本实现采用“格子按创建顺序、子节点按树顺序、玩家最后”。Mick 与玩家 Y 相同时也画在玩家下面。下标 0 的并列顺序未对照验证。
- 翻转的图块凸形状以世界坐标重新规范绕向，SAT 轴符号与 Godot 的变换法线相反，只影响恰好相等的平局选轴。
- 草丛晃动、闪光、其余 NPC、物件交互未实现；AnimatedTexture 相位从进入场景开始计时（原版为资源首次绘制）。
- 房门南侧栅栏开口处的 `Cutscenes/Cutscene Area11`（`mick_bark`，`disappear_flag=gave_treats`）在给粮前播放往北推回，不再把玩家停在开发边界上。`gave_treats` 之后该边界失效，见 [Mick](MICK_TREATS.md)。
- Y 排序绘制原先把全图约 1,058 个草丛条目每帧全部参与排序；现已在平台层按视口剔除后再排序（不改变可见结果）。

## 验证

已执行：

- 资源生成：Windows 主机 Python 3.9 + Pillow 11.3 运行 `podunk_field.py compile`，图集由 `encore-devkitpro:pinned`（devkitarm 摘要 `116afba8…`）中的 tex3ds 经 wslc 转换；随后 `resource_catalog.py compile`，`podunk_field.py verify` 与 `resource_catalog.py verify` 通过。CI 使用 Pillow 12.3.0，`make 3dsx` 中的 verify 会在 CI 上重新核对。
- 语法检查（`-fsyntax-only`，未链接、未生成目标文件）：同一镜像中 arm-none-eabi-g++ 检查 `platform/ctr/main.cpp`（默认渲染开关，未启用 CI 使用的实验性 GPU 开关）与修改过的 `runtime/` 文件；主机 g++ 12 检查共享核心与 `tests/field_data_tests.cpp`、`tests/resource_catalog_tests.cpp`。
- 用临时宿主探针加载编译输出，完成出生、行走、碰撞、边界停止与 B 返回、回到房门触发返回路线、转场计时。
- CI build 模式 run 37756233511（提交 `2459070`）在 `make native-content` 的 room 任务失败：`NATIVE CONTENT ERROR: Changed reviewed source: content/native-resource-catalog.json`。按上文拆分目录配方后，本地 `python tools/native_content.py verify` 与 `python tools/resource_catalog.py verify` 通过，合并后的 `native.encresources` 与拆分前逐字节相同（SHA-256 `95ced244…b908`）。
- CI build 模式 run 37759915317（提交 `2defb93`，不含测试）成功：`make native-content`、Linux + Pillow 12.3.0 下的 `podunk_field.py verify`、启用实验性 GPU 开关的 ARM 编译链接、3DSX、CIA、提取 CIA 后的 RomFS 逐文件比较与受检来源未改动检查；产物 `encore-3ds-2defb93…-37759915317`。该结果只覆盖该提交。

未执行：新增 `tests/field_data_tests.cpp`、`tests/test_godot_text.py`、`tests/test_podunk_field.py` 与目录测试更新尚未运行（按仓库规则，测试仅在明确要求时运行）；sanitizer、模拟器与 Old / New 3DS 真机均未验证。
