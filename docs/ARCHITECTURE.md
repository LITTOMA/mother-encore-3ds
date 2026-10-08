# 原生移植架构与严格程序/数据分离

## 当前数据流

固定只读上游 → 官方Godot3.6.2原生解析/精确导出与行为参考 → 审查后的外部content/native-opening.json → tools/native_content.py → RomFS/data/opening.encroom。

图片经tex3ds编译为独立t3x。opening.encroom含资源引用与指纹，不把全游戏所有资源强塞进单文件。
目前25,024字节、26类section覆盖当前开局切片：几何、body规则、flags/defaults、trigger、出生点、演员实例/profile、动画/方向帧、相机区域、Y-sort/底图布局、YAML命令、对象绑定、战斗请求和运动/动作/相机/经验规则。
这不是通用Godot场景运行时，也不是全游戏内容格式已全部实现。

3DS执行C++17机制、libctru/Citro2D。没有Godot引擎或通用GDScript解释器。
开局资源已迁移到外部包；真实内容、绑定和调参的完整范围迁移仍需完成，不能新增真实游戏数据常量表或实体名分派。
演员使用外部实例/profile索引，程序只保留明确的行为类型、操作码、校验上限和平台契约。
M0虚构世界/战斗仅供主机历史测试，不编入3DS发行程序。

## 加载与生命周期

原生入口通过独立 `native.encresources` 解析稳定角色与资源路径，启动核对引用文件，再使用既有语义加载器。战斗准备读取目录中的显式 battle / round 配对。来源、格式和剩余迁移范围见 [内容与绑定迁移](CONTENT_MIGRATION.md)。

RoomData拥有一个连续字节缓冲区；RoomView按固定字节偏移解码只读标量记录，目录查找O(1)。
不把文件强转成带指针、vector、虚表的C++类，也不构造一棵可变对象树。
加载先验证magic、版本、能力、大小、CRC、目录、UTF-8、有限数值、唯一ID、引用及操作码参数；失败不替换原有有效内容。
CRC用于损坏检测，不是密码学认证。来源指纹在编译环节核对，不能把CRC当签名。
无内嵌数据回退。禁异常构建下OOM可能终止进程，不声称可恢复分配失败。

RoomData必须长于借用它的RoomView、世界、角色和调度器。当前只启动时加载，未实现热重载。
动态flags、角色位置、动画时钟、任务/Timer、相机状态与内容分离。
碰撞顶点直接读包，派生法线为单一平坦缓存；没有每形状顶点复制/节点堆图。

## 执行机制

OpeningWorld连接共享移动、常规动画、静态凸碰撞、初始flags、Area时序、DialoguePlayer、ActorAction与CutsceneCamera。
平台只提供输入、60Hz物理/idle时间和渲染。400×240为平台viewport，开发文字只在下屏。

lamp_attack是YAML。离线前端遵守DialogueBox固定命令处理顺序，不按YAML键顺序逐条阻塞。
类型化指令显式表达actor_ready屏障、idle让出、WaitTimer与并行动作；QueueBattle先登记，最后phrase结束后RequestBattle。
异步细节、45弧度转向、原版忽略movecam.time等保留原生参考，不借重构悄悄改语义。
战斗请求现进入独立BattleEntry与opening.encbattle，包含源驱动入场/首菜单；已支持的动作与战后恢复范围见 BATTLE_ROUND.md 和 BATTLE_VICTORY.md。
NDSP后端从独立opening.encaudio映射PCM流；缺DSP组件时明确不可用，不能把请求计数当播放成功。
CPU 背景使用受检的精确采样路径；性能按具体背景、构建与平台分别记录，见 BATTLE_RESIDENCY_CHECKPOINT.md，不承诺全游戏或真机帧率。

## 数据更新

make native-content仅从外部IR编译并核对来源，不调用C++编译器，也不自动重新提取覆盖IR。
上游变更须固定commit、inventory diff、审查依赖、显式重新提取、受影响原生对照，再发布。
修改包后可用原ELF重新打RomFS；3DSX/CIA整体哈希变化是正常的，不代表C++重新编译。
数据独立性检查曾用同一 host 执行文件读取 A/B 包，核对出生点、动画帧、wait、布局、偏移、flag 数量和 XP 规则改变，而执行文件哈希不变。

全量兼容门禁仍不代表整个上游适配完成；局部审查在compatibility/reviews。
测试golden头只存在tests/fixtures，不进入生产链接。模拟器、真实构建、真机与完整兼容是不同验收层。

## 真实首回合

BattleRound读取独立RoundView，持有HP/行动队列/阶段；BattleActionPresentation通过同步事件接口共享SourceRandom，保留演出与逻辑的全局随机顺序。原版技能、翻译文本、规则、媒体轨道位于opening.encround；新增内容不生成C++常量表。目标选择、源Timer域、独立HP滚动与文字信号详见BATTLE_ROUND.md。BattleOutcome继续驱动原版胜利、手动EXP确认、奖励及世界返回；外部胜利记录和媒体决定数值/时序，世界桥接保留坐标、旗标和删除后的碰撞。周期Room Shaker使用外部规则和同一随机流，详见BATTLE_VICTORY.md。

## 室外场景与批量图块数据

Podunk 这类 TileMap 场景的格子数据量（约 17 万格）不适合保存为审查用 JSON。例外规则：`content/podunk-field.json` 只保存审查过的策略与路线，`tools/podunk_field.py` 每次编译直接从固定上游读取场景、TileSet 与脚本默认值并核对清单哈希，产物与 t3x 图集由收据记录 SHA-256，`verify` 重新生成并逐字节比较。任何未归类的场景节点、未审查属性、形状类型或转场类型都使编译失败。

`FieldScene` 复用 `OpeningWorld`（玩家移动、动画、镜头），碰撞经 `MotionObstacleSource` 在每次滑动前按范围查询；凸形状遵循 Godot 运行时凸分解与 ConvexPolygonShape2DSW 法线，凹形状按 ConcavePolygonShape2D 线段与静态 AABB 裁剪。跨场景转场由 `SceneDoorTransition` 驱动，换场使用先准备后提交，失败保留原场景。详见 [PODUNK_FIELD](PODUNK_FIELD.md)。

## 同场景门与普通对话

HouseView读取独立opening.enchouse；HouseRuntime控制原版门信号、触发几何、交互射线、暂停与seen状态，HousePresentation处理NPC与普通DialogueBox演出。传送只改变现有玩家坐标/朝向及相机区域，不重载或重置奖励。全部文本、资源、角色参数与过渡时间来自外部包，详见HOUSE_INTERACTIONS.md。
