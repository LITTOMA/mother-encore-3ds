# 市政厅导入（开发中）

当前：草稿候选，尚未开放市政厅入口。已完成来源地图／人物与替换演员、驻足人物交谈／朝向／动画，以及首批普通对白、市长是／否／取消和 Pippi 徽章条件分支的共享核心接入。发行目录仍只有原先场景。完整场景生命周期、Town Hall 音乐／脚步、室内菜单地图、平台选项界面与实际入场回归待完成；后续剧情明确阻断。

依赖 #66 的受检多场景引用和所有权切换，来源仍固定 7d9246600fffe518408f5830d4848635019005a3。目前完成候选路由、地图与 NPC 动作资源，没有把未完成的市政厅加入目录或发行包，也不声称可进入该建筑。

独立 content/world-scene-routes.json 提供场景身份、资源绑定和门节点身份，tools/world_scene_routes.py 读取实际 Scene／Door／SceneTransition 来源。保留原房屋出入两条路由，加入 Podunk TownHall→市政厅、MayorLobby→Podunk 和两个内部门。跨场景采用 Door.change_scene 的 targetY−7；同场景采用 Position2D 全局变换减 (0,7)，并按原版省略音乐淡出。工具只允许候选输出到 build，缺失目的场景、错类型、未知字段、重复身份、越界参数或 source 行为变化都拒绝。

已直接核对：MayorLobby2 的 (-88,-73)+(4,1)×(24,-439)−(0,7)=(8,-519)；MayorLobby3 的 (-16,-504)+(6,1)×(-9.33334,448)−(0,7)≈(-72,-63)。保留来源浮点误差，测试容差 0.0001，不按整数修补。六项 Python 测试通过，含原路由语义逐字段相等、实际内部门变换、版本／类型／重复／来源／参数负向路径。共享 C++ 解码器在 GCC9 ASan/UBSan 下读取实际候选，得到 3 场景／6 路由，并正确拒绝把缺少 Mayor 资源的既有目录绑定到它；记录在 reports/mayor-office-import。

后续必须完成：市政厅静态 Sprite、TileMap 与碰撞、镜头／可开启门、Town Hall 音乐、NPC 的 no_shadow／no_collision／sprite_offset／交互几何及初始对话和选项。Pippi 的 PartyMember 动画与后续救援不能当作普通 4dir NPC 忽略。AreaRoom 的地图偏移 (110,240)、室内地图位置固定、visited_podunk、背景色及跨区域行为也需外部数据映射。门的 stop_music 时机／淡出长度必须与区域所有者接通后再验收，不能仅靠树退出淡出声称完全一致。其余未实现行为保持明确边界；不以静态路由解析通过代替玩法。

地图候选已从原版生成：两个 TileMap、两组静态碰撞体／37 个凸形或线段、三扇路由门、两个镜头区域、五个人物身份，另有三张原版背景／柜台 Sprite 和可开启门 Sprite。独立 mayor-field.json（schema2）声明这些绑定；新增只允许显式枚举的静态 Sprite，未分类节点／错误人物来源仍拒绝，旧 schema1 仍可用。真实 tex3ds 生成 1024×256 RGBA8 纹理 1,048,597 字节，地图 7,176 字节，临时 PNG 自动清理。共享 C++ 解码器的首次实际读取失败（Field map static group item rejected）：原静态绘制组不允许 Actor，而市政厅前厅的人物确实位于普通根节点。12 个 Python 方法通过，但这不等于 C++ 解码通过；需要增加受检能力并接通静态人物绘制后复验。测试包含 Podunk 三项原输出逐字节相等、来源缺失／类型／版本、路径隔离及拒绝用未完成通用 Room 进入发行构建。产物只在 build/mayor-import/map-candidate；没有发布缺少人物／剧情／音乐的场景。

静态人物现通过独立 Map 能力 4 表达，格式仍为 1。能力 1／2／3 的原限制保留；能力 4 必须有静态组人物，旧资源不能仅改能力而被接受。平台静态组按来源顺序绘制 Actor，保持前厅 Abbott 在门 Sprite 之后、柜台 Sprite 之前；四个 Objects 人物继续参与 Y 排序。实际生成的能力 4 地图已通过共享 C++ 读取，33 项新 GCC ASan/UBSan 检查覆盖降级、无静态人物的伪升级、未知能力 5、坏身份／数量／保留位／截断并保持旧加载结果，已有 Field／Scene 3,107 项也通过。12 项 Python 方法通过，并从来源重新生成冻结夹具核对完整字节。初次能力 3 的拒绝日志保留，未用忽略规则绕过。新的绘制分支还需真实交叉构建与实际场景 GPU 验收，不把主机元数据检查当作画面通过。


来源复审修正：先前把 no_collision 当作永久禁用碰撞不成立。npc.gd 在 _ready 设置该值后，末尾调用 update_visibility_changed；该函数以 !is_visible_in_tree() 覆写碰撞 disabled。三个位于可见层级的市政厅 NPC 因而最终仍有实体碰撞。已撤回未用于任何发行数据的临时 ENCMIK 能力 5／功能位 8 和对应合成用例，保留其历史日志并标明撤回；不能用此前 1,541 项候选机制测试来证明原版无碰撞行为。既有能力 3／4 和资源字节保持，后续 NPC 导入须按实际生效状态、源码调用顺序映射，不能只复制 export 标志。

独立 mayor-npcs.json 与 field_scene_npcs.py 现从真实实例读取五个人物，核对 Map 人物身份。保留市长两种不同的交互形状、ViewArea、sprite_offset 与运行时自动偏移；场景中编辑器的旧 offset 不能取代 set_spritesheet／set_sprite_offset 的结果。三人的 no_shadow、声明 no_collision 与最终 visibility 驱动的碰撞分开记录；没有对白的办公区 Abbott 明确记录为不可交谈，不能借用 Mick 对话。生成器目前只支持原版驻足人物，遇到 wander／扩展交互／事件位置等未审查变体拒绝。没有把 Podunk 的随机数账本复制进该室内场景。

Pippi 的 PartyMember 动作使用 ENCMIK01 独立能力 6；原能力 3／4 不变，撤回的 5 仍拒绝。头部 +56 的方向数必须是 8，+60 保留 0；三个动作范围必须连续、恰好覆盖全部 Clips，每种动作都覆盖八方向并有序，未知功能、未审查八方向漫步与版本降级均拒绝。Runtime 使用原版 Discrete BlendSpace2D 的最近单位方向，包含浅斜向；不把八方向折成四方向。GCC9 ASan/UBSan 新目标 1,947 项、已有 NPC 1,457 项和静态人物 33 项通过。Python 源与负向测试此前 30 项通过，最新新增的输出路径及来源依赖复验另记。五个动作包、四张真实 tex3ds 纹理只生成在 build/mayor-import/npc-candidate，没有写发行 RomFS；纹理共约 1.63 MiB，不包含工具二进制。

这仍不是市政厅玩法验收：原版结束交谈／离开视野后的 1 秒朝向恢复、Pippi 的八方向剧情替换 Actor、无对白人物的交互过滤、各剧情选项／后续分支及 AreaRoom／菜单地图／脚步／音乐还待整合。FieldScene 暂拒绝把八方向动作绑定为现有四方向 NPC／Player 替换演员，避免在缺少 Room 执行能力时提交半个场景。没有新增存档编码或改动既有实际资源；不能由这批动作解码通过推定可进入市政厅或实际 GPU 通过。

真实交叉构建 fed5917 的编译／打包完成后，CI 在检查 tracked source 不应改变的步骤拒绝：Podunk source.json 的 compiler_sha256 仍是原生成器，实际生成器已为 Mayor schema2 扩展。保留失败日志，通过原 compile_all 重新生成收据；候选对比仅这一来源字段改变，原纹理页面／输入 SHA／Map／Room／Links 全相同，romfs 无字节变化。Podunk 未重复编译纹理：原生成器校验并复用既有页面及 tex3ds 来源；不是伪造新的纹理编译或修改游戏内容来消除差异。修复后的控制台验收待下一次真实构建记录。

候选 Room 已加入独立 NpcPartyDirectionalProxy=4，Room 格式 1／能力 10 必须实际含该执行器，旧能力仍拒绝它。每个新演员必须有八方向、24 项 Idle／Walk／Talk 绑定与 NPC replacement 归属；不能作为 Player 或仅改旧包能力。共享 Actor 按原版 Actor.gd 的 is_party_member 接受斜向，仍保留初始化时的主轴修正、Talk→Idle 的结束切换和原时序。实际 Mayor 角色候选 Room 16,200 字节，包含共享玩家和五个人物；正确使用显式 Mayor recipe 来源，普通 Podunk 默认构建路径保持。753 项新 GCC9 ASan/UBSan 通过，RoomData 71,939 项与旧演员／Doll／Pillow／排队转向回归分别 10,199／13,730／15,791／10,036 项通过；61 项 Python 解析／来源回归、随后 3 项最新演员来源检查通过。新负向测试自身首次写错记录步长被 ASan 拦截，已按真实 8 字节布局及边界修正；原失败与其他本地测试构建失败均保留。

native_content 编译器来源 SHA 改变后，通过原生成流程刷新 House／Podunk Room、Restore 的来源记录与资源目录。逐字段对比两份旧 Room 的全部数据节、规则、身份、字符串完全相等，解码字段仅 build_input_sha256 改变；旧实际能力仍为 9。Restore 的音乐条件／时长／几何和库存规则相同；Session 兼容身份与默认存档未改。make content 在显式 UTF-8 环境通过，Windows 默认 GBK 的首次失败保留。新能力与候选室内场景尚待完整主机／真实控制台／模拟器整合验证，不把这个候选元数据测试写成 Mayor 可玩。

原版 NPC 朝向恢复的 native Godot 3.6.2 基准已实际运行：原 stop_interaction／ViewArea 进入退出／return_to_init_dir 四个函数按原字节提取，8 个并行案例共 1,440 条帧状态，来源方法 SHA／引擎 SHA／生成脚本 SHA 和完整日志保留。普通结束在 frame60（从 frame0 算第61次 tick）恢复；重新交谈会让旧回调因暂停而无效，但不会取消回调；重叠两次计时在 frame60、90 各自恢复；注视者仅离开区域后计时，再次进入会阻止旧回调生效，漫步者退出不计时，其他 body 身份不会触发。这里注入的只有玩家身份及 UI 开始交谈状态，原 Timer 和四个函数不变；不是完整 NPC 场景／游戏或 3DS 运行。新 oracle 的 3 项 schema／domain／负向测试通过，两次探针准备失败（inline 赋值语法、主 SceneTree 脚本未注册 autoload）保留。私有工程每次 finally 清理。原生 C++ NPC 的延时行为尚待接入，不能把基准完成称为功能完成。

NPC 交互／朝向恢复现通过独立 ENCMIK01 能力 7 接入，旧 3／4／6 保留，5 继续拒绝。头部记录方向数、源计时毫秒和对白可用性；不把 1 秒写进 C++。新能力限已审查的驻足 NPC，漫步、动态空对白、未知功能／版本／坏时长／截断均拒绝。Source Stop 和视野退出登记独立计时器，按 float32 倒数，回调时检查正在交谈／仍在注视；不取消旧计时。FieldScene 在菜单暂停和替换演员期间也推进这些 SceneTreeTimer；交谈实际结束后解锁 NPC，原本没有对白的办公区 Abbott 不进入“未移植对白”停顿，碰撞仍按可见性生效。

GCC9 ASan/UBSan 新目标 6,080 项通过，其中 1,260 条原引擎朝向状态逐帧相同，漫步案例的 180 行明确由能力准入拒绝而非宣称已执行。实际 Podunk help／repeat 使用受检策略夹具，交谈关闭→离开视野→第61次 tick 恢复原朝向共 2,419 项通过；旧交互 2,041 项和 Mick 26 个原版等待／49,382 项节奏回归通过，13 项 Python 来源与版本负向检查通过。--interaction-policy 明确生成五个能力7候选及真实纹理，仍只在 build 下；发行资源未改。NPC 对白时的动画状态、初始对白／选项、区域与菜单地图元数据、Town Hall 音乐／脚步和实际入场仍待整合，因此没有开放 Mayor 入口。


未被剧情 Actor 替换的驻足 NPC 现使用 Room 的原版方向动画键和独立时钟；只由显式 ENCMIK 能力7启用，旧实际 NPC 资源保持原样。人物物理回调先把旧 input 混合到 Sprite，再更新注视朝向；交互时的立即转身单独应用。正在印字的 original NPC talker 通过现有 presenter 的 talking 状态驱动 Talk，停顿后依原版 Talk→Idle 结束切换，菜单暂停与隐藏替换期间仍推进其原 Sprite 时钟。四方向和八方向均使用最近单位方向；没有借 Actor 的圆整方向替代 NPC 的 Discrete BlendSpace。

为保留办公区 Abbott 的 Walk 待机，候选 Room 的 motion0 改为真正 Idle、motion4 保持 Walk，由受检策略选择 Walk；不再把两个状态当成同一个别名。候选 Room 为16,104字节，只改候选，发行 Room 未动。未知 talk_idle_animation、非默认过渡和有对白的 Walk 待机先明确拒绝。原引擎实际执行未改的 NPC _physics_process 和 CharacterSprite 建树／travel／blend，8案例960条帧／动画状态与共享 C++ 一致，4,816项 GCC9 ASan/UBSan 通过；朝向恢复6,080、交互策略2,419、旧交互2,041及候选 Party Actor 750项回归通过，20项 Python 来源／负向检查通过。首次基准因注入的未归属玩家 Node 泄漏被验收器拒绝，改成服务自有子节点后通过，失败日志保留。此基准不运行原地图、漫步或完整对白服务，不能等同整场景通过。

真实制品 d33d478（3DSX SHA256 3fd3ec1cae9fcd638c51c7f7f42aba3d9c06dcb52dbac2d27acb6a8e247aef9d）的 Azahar 正常输入回归已通过房屋往返与音乐、战前战后菜单、桥头战斗／奖励、Pippi 妈妈首次求助／重复交谈、重复触发幂等和西侧道路。人工地下室后存档含已完成电话／旋律标志；没有自然开局或真机声明，也不覆盖随后新增的能力7或 NPC Sprite 实现。原始结果、确切制品身份与模拟器日志已保留，私有运行目录自动清理。


初始四个来源文档 abbott／mayor_help／mayor_side_desk／pippi_town_hall 现有独立 mayor-story.json 与离线有界编译器。ENCFST01 格式1／能力3明确用于无遭遇的 NPC 场景：六个旧数据节保留含义，Interaction 扩为32字节，记录实际 source 与0可执行／1未实现状态，后者不得有 programme；第7节内嵌受检 ENCCHOIC。旧能力1／2的要求不放宽，未知版本、错误状态／源引用、嵌套 Choice CRC／能力／programme身份、截断均拒绝，失败保留旧资源。所有原 _all_dialog 覆盖按来源顺序记录；后续 pippi_rescued／pippi_delivered／zoo_freed 分支不再误用初见对白，未支持者显示实际路径并允许 B 返回。单条来源对话组不需要选取未读多条对白；该候选不声明全局 seen-dialogue 存档兼容。

四个实际文本程序不替换 NPC，印字状态驱动原人物 Sprite，等待玩家确认后才关闭；市长 Yes→7、No／cancel→10，Pippi 在首句后依据 courage_badge_received=false 才转入第二句。共享核心已通过2,980项 GCC9 ASan/UBSan，包括五个原人物绑定、文字保持、是／否／取消、徽章两路、旗标守恒、明确后续边界和新增解析负向路径；14项 Python 来源／版本／图结构检查、旧野外交互2,041和策略2,419项通过。候选 Room18,256／Story3,694字节仅在 build 与测试夹具，未进入 RomFS。

整合中实测发现旧 field face_interaction 使用任意角度单位向量，讲话完恢复 Walking 后被 movement 校验拒绝；这是旧移植错误，上游未改变。实际 Player._turn_to 覆盖 party_object 基类，按主轴选择正交方向、同幅时优先纵轴。已映射此覆盖并检查六种浅斜／主轴／同幅方向在结束后仍可 advance；原失效日志保留。其他失败分别为测试缺少 IO 声明、闭合路径没有独立内部 phrase entry，以及测试猜错 choice PC，均修复并保留实际记录，没有改上游或忽略准入。

完整 Clang 普通与 ASan/UBSan 在 daef29a 源码各169项通过，真实3DSX/CIA也通过；对应原始 job 日志已留存。这覆盖较早的替换后端和 CTest 工作目录修复，不覆盖此后能力7／NPC Sprite／Mayor Story 新增；GCC 完整任务仍运行，不宣称全量绿色。最新切片的交叉构建／模拟器和真机状态仍分别待验证。


野外 Story 选项已接入共享核心与平台调用：准备时检查来源场景／程序／组和向前 phrase 目标，真实 WAIT 与文字完成前保持 WaitingText；选中时先清旧字再执行来源目标，旧 generation、未完成文字、未知目标均拒绝且保留现场。平台复用现有摇杆／方向键 repeat 与完整输入归属，文字变成选项的首帧不继承确认或一直按住的方向。House 回调只处理 House，Field 交由活动 world；现有 renderer 仅在 arrow／font 纹理身份一致时复用，坐标、颜色、动画和本地化标签从当前选择包取得，声音与翻译键在入场准备时校验。真实首段市长文字的 WAIT、确认等待、No 选择、清字及旧回调共计3,735项 GCC9 ASan/UBSan 通过，旧交互2,041与策略2,419项回归通过；尚未据此声明实际 GPU 选项画面或模拟器市政厅入场通过。

较早 daef29a 的完整 GCC 普通184／ASan-UBSan169项也已通过，原始 job 日志已保存；与 Clang169／169及真实控制台共同完成这份较早源码的完整检查。随后新增代码仍按其自己的目标验证和控制台任务记录。替换制品 d33 模拟器回归完成后删除旧387制品及已完成的主机 ELF，共421,633,516字节；保留源收据、全部日志和当前真实制品，没有清理仍需使用的编译缓存。
