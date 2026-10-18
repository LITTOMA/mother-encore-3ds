# 市政厅导入（开发中）

依赖 #66 的受检多场景引用和所有权切换，来源仍固定 7d9246600fffe518408f5830d4848635019005a3。目前只完成候选路由，没有把未完成的市政厅加入目录或发行包，也不声称可进入该建筑。

独立 content/world-scene-routes.json 提供场景身份、资源绑定和门节点身份，tools/world_scene_routes.py 读取实际 Scene／Door／SceneTransition 来源。保留原房屋出入两条路由，加入 Podunk TownHall→市政厅、MayorLobby→Podunk 和两个内部门。跨场景采用 Door.change_scene 的 targetY−7；同场景采用 Position2D 全局变换减 (0,7)，并按原版省略音乐淡出。工具只允许候选输出到 build，缺失目的场景、错类型、未知字段、重复身份、越界参数或 source 行为变化都拒绝。

已直接核对：MayorLobby2 的 (-88,-73)+(4,1)×(24,-439)−(0,7)=(8,-519)；MayorLobby3 的 (-16,-504)+(6,1)×(-9.33334,448)−(0,7)≈(-72,-63)。保留来源浮点误差，测试容差 0.0001，不按整数修补。六项 Python 测试通过，含原路由语义逐字段相等、实际内部门变换、版本／类型／重复／来源／参数负向路径。共享 C++ 解码器在 GCC9 ASan/UBSan 下读取实际候选，得到 3 场景／6 路由，并正确拒绝把缺少 Mayor 资源的既有目录绑定到它；记录在 reports/mayor-office-import。

后续必须完成：市政厅静态 Sprite、TileMap 与碰撞、镜头／可开启门、Town Hall 音乐、NPC 的 no_shadow／no_collision／sprite_offset／交互几何及初始对话和选项。Pippi 的 PartyMember 动画与后续救援不能当作普通 4dir NPC 忽略。AreaRoom 的地图偏移 (110,240)、室内地图位置固定、visited_podunk、背景色及跨区域行为也需外部数据映射。门的 stop_music 时机／淡出长度必须与区域所有者接通后再验收，不能仅靠树退出淡出声称完全一致。其余未实现行为保持明确边界；不以静态路由解析通过代替玩法。

NPC 机制增量：ENCMIK01 能力 5 仅允许静止 no_collision 动作所有者；必有独立功能位 8，保留原能力 3／4 读取和字节输出。共享 FieldScene 停止为该 NPC 加实体脚部障碍，但保留其可见性、动画、转向与交互区域。未知版本／位、缺失位、带剧情的动作资源，以及漫游和无碰撞的组合明确拒绝。npc.gd 的来源行为是禁用 CollisionShape2D 而不禁用 interact；未把无碰撞解释为删除角色。13 个 Python 方法和 1,541 个 GCC ASan/UBSan 检查通过，后者在实际 Podunk 碰撞图中比较同一路径的实体／非实体 NPC，并确认非实体 NPC 仍能触发原有交互边界。当前已发布资源及 Mick 审查 IR 字节保持相同；尚未用此能力发布市政厅人物，影子／几何及 PartyMember 适配仍待完成。
