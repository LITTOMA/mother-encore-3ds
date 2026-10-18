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


a8168a9 的真实控制台构建在 field_story.hpp 编译处失败：play_source_audio 的 lane／gain／pitch 默认参数只在稍后的函数定义出现，头文件被包含的位置只有不含这些默认值的前置声明。已按原默认值显式传入 Effect、0、1、false，不改音效来源／音量规则；保留真实失败日志。此修复待后续真实交叉构建，不以共享核心通过代替平台编译通过。


Town Hall 音乐候选现从原 MusicChanger 类／实例默认值与真实矩形编译：MusicArea、Town Hall.mp3、+6dB、淡入1秒／淡出1.5秒；PCM自身增益0，保留原 import 循环。实际 FFmpeg 解码产生5,345,280字节 PCM，源／import／解码器与命令／输出 SHA 保留；配套 Audio225、Music246、Zones77字节。只有 build 下的候选与小型测试元数据夹具，原 Podunk producer 和发行音频未变。

原 Zones v1 专限 Music/ 路径，真实 Mayor 根节点 MusicArea 触发共享 C++ 拒绝。新增 ENCZON01 v2 明确允许安全的通用场景路径且必须实际使用新特性，v1原限制保持，未知3、降级、伪升级、坏路径与截断均拒绝并保留旧对象；没有改名伪造来源节点。456项 GCC9 ASan/UBSan 检查包含真实音源绑定／空间、重复进入、原淡出与 all-tweens 完成后释放；6项 Python 来源／编码／负向检查、旧 MusicRegions4,087项和 GameplayAudio 回归通过。首次来源工具属性名错误、测试 loops accessor 错误、v1真实拒绝与淡出后测试误读已释放 voice 均保留记录。此处不宣称可听性；在开放 Mayor 前还须完成跨场景的原 MusicChanger 注册列表停止顺序及全局音乐身份、区域元数据与实际入场验证。


门口音乐已按固定来源重新核对：SceneTransition 在遮罩开始前遍历 audioManager.musicChangers，MusicChanger.stop_music 从同一个数组删除自己；Godot 实际遍历因此跳过交替项。原生基准执行未改的门处理前缀和停止方法，0至6个区域／跨场景／同场景／禁用淡出共21种注册结果。共享核心保持该顺序、门指定的淡出长度以及已注销区域在 tree_exiting 不重启默认淡出的行为；旧 first-player 描述不准确，已更正，固定上游未变。此局部基准注入空 attached_player，仅证明注册次序；音量／释放由独立核心与平台双件验证，不把注入服务称为整场景原生运行。

共享核心259项、NDSP 双件播放器774／服务1,058／分步准备46,894项与9项 Python检查通过。坏时长不修改现场，旧场景 epoch 无害，停止回调不做文件读取或立即改变 NDSP 队列。平台在 begin 后、idle_frame 和 perform_scene_swap 前消费 FadeMusic，故使用的是原场景音乐身份；切换后的事件只有解锁与结束声音。1f53e91 的实际3DSX／CIA构建已通过（run39124130097），a8168a9 的编译失败日志保留。2ffd16b控制台任务亦成功，不能据此声明 Mayor 已可进入或真机通过。


AreaRoom 候选现由来源元数据启用 ENCMAP01 格式1／能力5，新增单个80字节 Area 节；旧能力1至4的21节目录和实际资源保持原样。编译实例 (110,240)／sub-area=true、原背景 #1c1c1c、Podunk 对应 MapPodunk、拥有后覆盖为 Merrysville，以及 visited_podunk 和单人 NPC-party 边界。地图定位与 marker 共用核心坐标；同一室内移动不会移动地图上的位置，未知区域地图明确拒绝。背景在场景视口内按来源色绘制。准备中的世界先持有 Ready 写入，发布失败不修改旧世界；重复 Ready 仍按原版发出 flags_updated。飞人 NPC 队伍尚未移植，相关状态拒绝，不作静默删除。

原引擎执行未改 AreaRoom Ready／定位和 globalData.set_flag，16组原生结果覆盖原有访问标记真假、子区域真假和四个位置；使用注入的空 NPC 队伍、当前地图身份与地图大小，不是整场景信号图。共享 GCC9 ASan/UBSan 场景663／菜单414／会话20,531／旧地图3,107／市政厅剧情3,735项通过，Python21项及最后来源／会话6项通过。首次测试调用了不存在的 FieldScene.advance、来源路径沿用纹理限制而拒绝空格，均保留失败并修正。早期原生适配器多次超时；诊断暴露注入 set_flag 的单行多语句语法错误，修正适配器后原文件方式通过，未改测试中的原版函数。

Session 资源仍为v10，仅增加实际 visited_podunk 可变标记，编码多18字节；原默认状态、开局角色和游戏存档编码不变。经既有命名 re-admission 工具重建来源依赖，命名二进制不变。原生成器核对／重建旧 Podunk 后地图、Room和链接均逐字节不变；来源收据记录新的工具指纹。Mayor候选7,380字节仍未进入发行目录。对应来源、实际日志、未通过尝试与差异证明见 reports/mayor-office-import/area-*。


脚步候选采用独立 ENCSTEP1 格式1／能力1：初始 wood／shadow、命名 run 槽、2个真实循环音源及2个原矩形区（Wood进入wood，Carpet进入stone／离开wood），保留源纹理过滤和可启用状态，cap1仅允许不改变默认 shadow 的区域。普通步行不播放这一音轨，原 Player._set_running 在跑步时选择／恢复循环；还须实现并核对执行回调，不能把元数据当作已播放。来源查看包含 _move_state 暂停停止、_movement 普通行走停止与阻挡停止，后续执行按这些调用次序接入。

实际生成544字节步区包、416字节音频元数据和 wood／stone各70,560字节PCM（来源import loop=true／offset=0，PCM增益0）；全部候选在build，测试仅保留小型二进制元数据。原音频编译器收据及不改字节的bank重命名映射完整保留。C++数据加载／媒体匹配与负向路径1,209项 GCC9 ASan/UBSan、Python5项通过；未知格式／能力、坏身份／摘要／默认索引／几何／shadow／过滤／截断保持拒绝，坏加载不破坏原对象。首次编译工具参数倒置与验证器误用变量的失败也已保留。没有声称跑步回调、NDSP播放、市政厅入场或真机验收通过。


跑步循环的共享状态消费者已接入受检脚步包：_set_running 的运行布尔值、暂停／非MOVE／攀爬的开始门控、相同流从停止态重播、不同流替换、显式停止，以及步区选择／启用禁用。地面变化只改变 run_sound，下一次原调用才替换流；_move_state 在暂停或进门时停止声音但不修改 running。尚未连接实际移动回调／NDSP，不把这一局部消费者算作已能听到脚步。

原 Godot 3.6.2 使用真实源MP3的新导入（原loop=true／offset=0经原生对象断言），执行未改 Player._set_running、暂停／进门 _move_state 分支和 Stepping Sounds 的进入／离开／启用／禁用，共29组状态与事件顺序。命名SFX管理是注入的流身份／播放状态服务，普通移动体是 assert-false 适配器，未执行区域物理或真正音频输出；所有适配范围与原始脚本SHA留存。共享 GCC9 ASan/UBSan 共1,351项（含原数据检查）与2项来源／结果负向检查通过。首次直接复制原import元数据因缺少生成缓存而被拒绝，改用真实新导入并核对原设置后通过；失败保留。CTR Makefile也加入同一个共享步骤模块，真实交叉构建待本提交自己的任务，不把此前控制台通过继承到此新增模块。


NDSP后端新增可选 Run 音轨；固定0至3／20至23通道保持，跑步和区域音乐共同租用4至19，不减少跑步关闭时的16声区域容量。租约先检查再提交，停止／失败／重置不清除其他区域队列，预热缓冲在再次播放时重新取得有效通道。采用现有循环PCM进行接口双件检查，跑步与区域并发／替换／音量／暂停／15+1容量拒绝／释放重试／带活动区域的场景重置／退出共786项 GCC9 ASan/UBSan通过；旧音频1,096项保持 TRACE529／71a30145b163903b，区域774／服务1,058／分步准备46,894回归通过。首次旧负向用例把8当作未知音轨；8现在明确定义为Run，负向边界改为9并继续检查255，未更改trace预期。游戏尚未调用这条音轨，真实脚步音源仍待并入受检全局音频和绑定移动；不据此声明可听或真机通过。


e3e3af2 请求的完整 GCC 检查在普通 CTest191／192通过后失败：python_tools 的严格登记表没有同步 field_area_source，拒绝 Unmapped dedicated unittest registration；该GCC任务未进入 sanitizer，不能记为完整通过。现将实际Area、脚步及原生跑步参考模块逐项加入既有DEDICATED登记，保留未知／重复／缺方法拒绝，未新增忽略规则。登记单测14项与真实CMake配置／CTest收集通过：1,025个方法恰好划分为669聚合＋356单独方法；这仅是收集验证，不是全部方法执行通过。原始GCC失败日志和Windows配置缺少nmake／变量未展开的失败也已保留，后两项通过使用已存在的工具绝对路径修正。该修复待新的完整重跑。


真实 wood／stone 循环音源已由受检追加生成器并入应用音频库，57增至59，元数据9,891增至10,243字节；保留旧57个解码记录和全部PCM的精确SHA／CRC，不重解码旧声音。新增2个源MP3仍保留loop=true／offset=0和0dB媒体增益，各70,560字节；通过既有extension_audio入口派生新IR，未改变固定的Phone linker或Room来源指纹。追加生成器拒绝删除／改写旧声明、旧媒体／收据不符、混合参考解码器及未知字段，真实数据由原音频编译器产生，再重建整份bank与收据，没有手工补字节。发布失败恢复旧元数据并移除本次新文件；成功重复执行不改文件。

首次追加验证误把声明中的output_sample_rate当作收据同名字段，原收据存的是生效sample_rate；按实际字段映射修正并增加正／负测试，原转换声明和旧记录不变。44项Python来源／追加负向检查、GCC9 ASan/UBSan音频数据30,919／脚步及全局音源匹配1,353／NDSP跑步786／旧音频1,096项通过，旧TRACE529／71a30145b163903b保持。恢复器核对应用及Podunk合计64个PCM无缺失；仍未接入实时步区／移动，也未把此数据收录算作已能听到或真机通过。


追加器最终8项单测还覆盖发布验证失败的回滚、保留已存在文件、清理中断写入的局部PCM，以及拒绝从只读旧快照直接发布。最终生成器从5fe7491的57条原始输入在build重新生成候选，完整bank与已发布的59条bank逐字节一致；该重建没有覆盖现场或重新解码旧PCM。此前e3e3af2的Clang普通／sanitizer任务已通过，GCC仍是单独登记失败；新的完整重跑以5fe7491为独立身份，后续新增音源不能继承旧任务的完整通过结论。


步区元数据现可由场景资源集独立选择：目录能力10增加FieldSteps=43且要求独立绑定ID≥1024；WorldLinks能力3使用原SceneAssets尾部保留字段绑定步区，必须实际使用新特性。旧目录能力1至9、链接1／2和现有发行资源字节保持原义，不把保留位偷偷改成新格式。每个步区资源必须有地图场景归属，类型／引用／scene身份不符均拒绝；资源所有者保持独立、不可变，失败不替换已准备场景。

当前实际Mayor候选链接指向1030；尚未发布。专用元数据夹具将已审查步区的scene刻意改为2，以单独测试资源归属，不是原版Podunk步区或可玩市政厅。真实来源与schema测试13项、GCC9 ASan/UBSan新增77／旧场景所有者32／旧多场景66项通过。新增CMake夹具只复制目录绑定的元数据，不复制纹理或PCM；Windows配置及CTest收集1,037个Python方法（681聚合＋356单独）通过，这仍只是收集。第一次准备暴露Python后缀表和capability上限映射两处缺项，均修复并保留失败日志。实时步区、移动、循环声音和场景持久run_sound仍待接线，不据此声明新增场景可玩、可听或真机通过。


5fe7491 full rerun: Clang normal and ASan/UBSan each passed all 180 CTest targets; real 3DSX/CIA packaging passed. Raw completed job logs are retained. GCC sanitizer status is checked independently; these results do not cover the subsequent audio append or scene-step binding changes.


共享MOVE现在输出有序、仅成功时提交的声音指令：_set_running(false／true)和条件停止；暂停／进门只停止流，不改running；普通步行保留“Nasty fix”停止，撞停后保留原false调用，tap切换中的false→true→阻挡false不合并。FieldRunLoop以受检指令执行并拒绝未知／超长序列，失败保留原状态与事件。原14组227帧位置／状态与逐帧调用一致，GCC9 ASan/UBSan新2,308／旧移动5,499／脚步1,353项通过；Python来源／结果／坏输入5项通过。暂未绑定真实world或NDSP，不能当成游戏已播放。

原生Godot执行未改_move／_movement／_move_state，保留原物理Input边沿和自由KinematicBody位移；原_set_running是记录布尔调用适配器，SFX playing getter记录条件停止指令且返回false，不输出声音。真实_set_running与播放器状态另有29组原生基准，不能把捕获适配器当作完整音频服务。首次独立脚本无法识别autoload音频服务，添加editor导入仍失败；改为显式注入且由场景树拥有测试服务后通过，函数原文不改，失败与临时项目清理均留存。该基准不执行墙体／完整Party／场景互动。


5fe7491 full rerun completed successfully: GCC normal195 / ASan-UBSan180, Clang180/180, real console packaging. The strict suite-registration failure on e3e3af2 remains a separate historical failure. 00cb71d genuine console packaging also passed; current MOVE call additions still require their own console job.


真实World MOVE现可绑定场景自有的声音消费者；运动事务成功后才交付调用，未准备／重复绑定与消费者拒绝均受检，重置World移除旧借用。FieldScene选择受检场景／媒体一致的步区，按上一已完成物理位置、原Hull与编译顺序分发接触变化，选择不会立即换音轨；当前只有明确的metadata夹具测试，尚无原地图区域物理通过声明。编译时额外确认原区域mask确实观察该受检Player层；既有544字节步区包逐字节保持不变。

暂停按原pause参数保留差异：默认暂停保留tap_run时的running，下一MOVE暂停回调才停止流；unpause对仍在running的状态恢复循环。主动Walking NPC交互对应原uiManager.open_dialogue_box的pause(true,true)，立即清除tap_run和run；未被Actor替换的剧情玩家仍执行暂停MOVE停止，替换后禁用原Player物理。新增原生基准执行未改Player.pause／unpause／_set_running／暂停_move_state，共16次操作；实际MP3导入，动画／计时器／碰撞／party与AreaRoom身份是显式注入服务，不等同整个引擎场景。UI入口、NPC.interact、Player物理分派与Actor.init只有来源审查，不冒称执行通过。共享World结果逐项对照相关原生状态／事件，GCC9 ASan/UBSan123项通过；开场World、旧Mayor Story3,735和MOVE2,308回归通过，Python13项通过。测试夹具的ifstream与WalkInput重名导致一次编译失败，修复并保留真实日志。

尚未把此消费者绑定到主程序和实际Run音轨，也未实现跨场景run_sound保存准入。场景步区的原引擎接触时机、实际音频、正常市政厅进出仍待验证；不由局部核心通过推定完整游戏或真机通过。


Session资源v11在完整v10之后仅追加21字节wood／stone身份表，来源为受检Player默认值和Mayor步区；旧v1至v10默认只准入其原run_sound。C++拒绝未知v12、空／超长／重复／坏语法／漏默认名表、CRC正确但缺尾、降级带尾及坏加载。游戏ENCSNAP1编码、compatibility、默认／startup模板和命名包逐字节不变，真实v10包保留独立夹具。扩展身份使原属性可以跨场景及Record／LOAD保持；目录只更新Session绑定的实际size／CRC。命名工具新增明确10→11来源重新准入，禁止跳版、改默认或菜单／物品范围；全量来源与生成器核对，不手改pack或manifest。

主程序收集快照与回屋候选时，从已准备的FieldRunLoop读取wanted_sound名称，验证新Session准入后再提交候选；没有步区的场景保留原state。Session20,606／命名与新游戏965／六项菜单414项GCC9 ASan/UBSan通过，Python来源／版本／负向27项通过。实际音轨仍未由主程序调用，步区接触与真实入场尚待验收。

本机WSL1的session_save文件故障测试22,823项检查中7项失败：内核4.4.0-26100-Microsoft／wslfs在报告RLIMIT_FSIZE=128后仍完成512字节真实写入，使预期写失败／回滚的分支不发生。独立微探针复现并清理临时文件，失败日志和环境保留；存档实现与测试均未改，也未关闭该用例。此项不能记为本地通过，须由本提交对应的真实Linux完整任务确认。更早5fe7491的Linux GCC／Clang完整通过只覆盖原确切源码，不继承到v11。

补充旧六项菜单与地下室来源回归14项Python通过；历史7→8→9→10命名准入用例现明确构造v10投影，仍验证原菜单／物品前缀，并保留9→11跳版拒绝，不把新run-sound字段塞进旧格式。当前命名及目录的真实生成器verify再次通过。


76f2d12的完整Clang普通检查183／184通过后失败，未进入sanitizer：resource_catalog_sources旧断言把EXTENSION_ROLES允许的全部类型等同于当前已发布清单；新FieldSteps仍是候选，发行目录没有它。修正为逐项声明当前14种已发布扩展类型并核对其均在允许范围内；新能力10／绑定归属仍由独立正负测试覆盖，没有给未知类型添加忽略规则、也没有提前发布候选。目录来源15项实际通过。该次Linux session_save正常通过，说明WSL1文件大小故障不能被当成该Linux结果；它仍不代表v11整批检查通过。原始Clang失败日志保留。


FieldRunAudioService已连接主程序：场景准备阶段直接核对实际AudioPlayer的bank并预热2条PCM；FieldScene绑定同一元数据和继承run_sound，提交后按顺序消费Play／Replace／Stop。控制器仍在演出与暂停时记录来源调用；播放失败保留已消费游标，只重试失败的请求，批次全部通过才清空。所有事件先检查类型／音源，错误后不破坏现有BGM；不会把无事件的持续跑步变成每帧重播。离开野外／重置／回屋先移除该消费者并停止其实际Run音轨，候选准备失败不停止旧场景Run。

AudioPlayer增加只读实际bank和配置generation；重新初始化、换播放器或换场景controller都不能复用旧消费者。实际wood／stone PCM的NDSP队列、来源匹配、连续播放、换地面、停止／恢复、分配失败重试、部分消费后失败、坏事件、丢失待处理事件、旧配置和无DSP已报告路径共983项GCC9 ASan/UBSan通过；旧音频1,096项及TRACE529／71a30145b163903b、LEGACY_TRACE516／56ffdd52e57c7f69不变。此处是API双件与真实媒体检查，不等于扬声器可听或GPU／模拟器入场通过；本提交自己的实际3DSX/CIA仍待任务完成。

两个C++资源消费者共享生成夹具，其中一个负例短暂改写步区文件；给这两个消费者添加同一个CTest资源锁，避免另一个正例同时读取负例文件。真实CMake配置／CTest生成计划确认该锁，其他测试继续并行，没有跳过测试；Python收集仍1,045个方法＝689聚合＋356单独。当前发行世界仍未加入Mayor资源和入口，原引擎步区接触、跨场景音乐身份／玩家瞬时状态及真正进出验收仍待完成。


步区接触另有20组真实Godot物理结果：原Stepping Sounds回调／连接、原矩形与实际Player局部多边形，在独立Area2D／KinematicBody2D树中执行；传送后稳定4个后续物理帧，覆盖进出／重复停留、禁用后进入、区内启用不凭空重触发、玩家mask=0仍被Area单向mask观察，以及非玩家进入不改run_sound。几何直接核对已编译共享核心Hull，未模拟完整地图或精确边界帧／阴影渲染／音频。

首次探针误把Player.tscn根初始position(-3纵轴)加入碰撞形状，和真实玩家局部Hull不符；核对时拒绝该基准，修正为保留Collider本身+3偏移、由传送输入指定玩家原点。游戏核心与原版未改，失败几何及旧收据保留。修正后的20组稳定输出与共享FieldScene选择一致，新增共569项GCC9 ASan/UBSan、相关9项Python来源／结果负向检查通过；旧Mayor剧情3,735／MOVE2,308回归仍通过。C++使用明确Scene2元数据夹具隔离接触消费者，不把该夹具称为原版Podunk或Mayor整场景验收。


持久玩家准备阶段先移除摄像机对旧Room缓冲的借用：当前只需的camera-area记录和6个参数在受检加载后由摄像机持有。所有数值仍来自二进制资源，不加入游戏调参常量。新rebind_scene只重绑场景边界；玩家自身Shaker／offset／恢复时钟保留，旧场景actor cameras清理。未准备、空Room、坏位置／视口、正在跟随Actor／移动或不同玩家摄像机参数均拒绝，失败保留原相机。

Room原缓冲销毁后继续定位／震动，以及重绑真实Mayor候选后再次销毁来源缓冲，共866项GCC9 ASan/UBSan通过；原Camera原生轨迹7,246项、持久玩家基础42项、World与步区569、Mayor剧情3,735、MOVE2,308回归通过，未改原轨迹预期。仍只是持久交接的前置所有权修正，实际跨场景玩家激活／摄像机切换未接入主程序，不宣称完整生命周期已完成。


真实SceneTransition._deferred_goto_scene和单人global辅助函数现有6次原引擎换场结果：Objects／YSort父节点选择、局部position、非零方向更新／零方向保留、相同Player／Camera2D／Shaker与额外persistent节点、碰撞关→开、旧场景释放、参数／UI／leave_for顺序，以及opaque tap_run／running／speed／velocity／substantial／run_sound值保留。真实House／Podunk／Mayor父变换均为单位变换；额外带(100,-50)的YSort只用于证明局部坐标规则。玩家方向、碰撞函数原文执行；blend_position只作参数更新语义审查，动画、Shaker脚本、门淡入／音乐与多人跟随没有在此探针中执行。

初次注入global数组缺少原版的推断类型，使create_party_followers的:=无法推断；未开debug时工具只见超时，开启debug定位原错误。修正wrapper为来源一致的Array／Node类型，原函数不改；再修正外部scene_changed声明的精确unused_signal注解及在发射信号的调用栈退出后释放对象。没有全局关闭警告或放宽引擎错误门；原失败日志、最终原生结果和收据留存，所有临时原生项目均自动清理。

共享核心新增显式受检交接：只有独立、健康、已暂停且无Actor／战斗接管的目标才能接收；逐项匹配资源路径／SHA／尺寸、Hull、Profile、全部规则、方向表和动作帧绑定，再复制／重绑摄像机边界，最后转移同一玩家对象。失败不写任一玩家或退役旧World；成功后旧World不可再执行移动／方向／pause／身份／camera回调，目标保留tap/run、速度、边沿和Shaker。新Scene Player 247项GCC9 ASan/UBSan通过，原移动5,499／持久所有者42／camera绑定866项通过，相关Python12项通过。第一次跑步夹具起点撞墙，导致tap_run按来源正确停止；改用明确无墙的机制夹具点，未改游戏规则或源结果。主程序换场尚未调用该交接，不能据此称正常游戏生命周期已修复。

4fc70e2对应完整Clang普通与ASan/UBSan各184项全部通过，保留默认泄漏检查；console真实3DSX／CIA及包内核对也通过。76ae06d的真实console任务亦通过，原始日志已保存。以上精确提交不覆盖后续camera绑定和本次Player交接；记录时GCC完整任务仍运行。Mayor入口、跨bank音乐、模拟器与真机仍未验收，PR继续草稿。


主程序的房屋→野外、野外→野外与野外→房屋现使用同一持久Player。先在独立目标准备全部玩家兼容性和拷贝camera数据，拿到只含受检状态的move-only提交凭据，再完成纹理／音乐准备。最终提交不分配内存或读资源；逐项拒绝已改变的玩家、animation、完整camera／Shaker／恢复队列、身份、场景、物理／idle帧和story revision。凭据持有两边Player强引用，不持有World指针；两边World销毁后取消凭据仍安全。空、重复、错主人或过期提交不写两边状态，旧World仅在成功后退役。

House停放状态已改读出门前快照中的房屋位置，野外进度和输入owner检查则读取真实活动World，避免共享玩家后把野外坐标混入受限House存档或因旧World退役而丢失摇杆输入。零方向回屋先继承活动朝向再准备受检快照；新游戏／LOAD仍创建新的Player。此次不改游戏资源、Session编码、媒体或兼容版本，也不声称未支持的移动中camera／多人跟随换图已经完成。

GCC9 ASan/UBSan的玩家交接442项（含12种过期状态和House／Podunk／Mayor候选资源循环交接）、旧移动5,499、camera866和旧持久owner42项通过；原World剧情／步区569项、真实House恢复／Dad Record18,140项、Session20,606项回归通过。测试中idle_frame(0)按原有核心约定完全不推进状态，第一次错误地期望其令凭据失效；改为真实非零帧作为负例，并保留零帧不变的正例和失败日志。实际主程序交叉构建、模拟器进出与可听性尚待该提交产物验收。


4fc70e2的GCC完整作业已结束：普通197／199个CTest目标通过，聚合python_tools和phone_linker_bindings失败，未进入sanitizer。真实原因是3个过期验证假设：Actor历史路线夹具仍是能力2而当前生成器输出能力3；Phone引入音频的断言仍固定57而受检wood／stone使总数为59；生成的Phone链接报告仍引用追加前AudioIR。日志中的intentional worker failure是测试运行器的预期负例，不能混同这3个真实失败。

历史Actor路线包不改，测试从当前配方显式投影回schema1，仅去除新可选steps字段，再核对旧cap2全部字节；当前cap3正负来源测试继续保留。Phone断言核对59条、末尾1701／1702的真实source，并用原AudioIR整体SHA核对前57条投影，未放宽旧声明。Phone报告由原report／write生成器重建，Linux原序列化器保持既有LF；逐项语义核对只增加wood／stone各source及import的4个记录，以及native-audio.json输出SHA，旧输入／来源／其他输出和机制说明全部不变。没有手改二进制包或更新原版轨迹预期。

修正后相关Python41项通过，新增原57条整体SHA断言单独1项通过；hash seed 1／789的真正link_phone_content --check均由该41项覆盖。该结果不是GCC完整重跑通过，也不继承旧Clang184／184到新提交；最新完整矩阵和主程序真实模拟器验收仍待完成。
