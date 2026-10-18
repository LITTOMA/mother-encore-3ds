# 市政厅导入（开发中）

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
