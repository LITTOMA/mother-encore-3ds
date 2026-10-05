# Podunk 完整静态地图资源

固定上游 `7d9246600fffe518408f5830d4848635019005a3` 的完整 Podunk 包含 44 个 TileMap、173,211 个原生格子。`tools/podunk_map.py` 从官方 Godot 3.6.2 的完整隔离导出和补充 TileSet 导出生成来源 IR，保留实例覆盖、世界变换、原生图素区域、翻转/转置、tile z、Canvas 祖先层级与条件节点。原版脚本被隔离，来源导出本身不批准游戏行为。

运行时加载 `romfs/data/podunk.encfieldmap` 和 `graphics/world/podunk/map-*.t3x`，不加载 authoring JSON。全部原地图都存在于资源中。原版缺 tile ID 或无纹理的 3,234 个格子保留为明确 native skip，按照原生 TileMap 不绘制、不创建 shape 的行为处理。

13,312 条 shape 保留真实原生凸分解。kind 1 是严格凸包，kind 2 是 473 条原始 ConcavePolygonShape2D 的端点对数组，kind 3 是 195 条原版退化 ConvexPolygonShape2D，保留重复顶点和源顺序；不能将后两者静默丢弃或替换成矩形。`collision_polygons()` 仅做空间筛选并返回世界坐标及 source order，由实际物理消费者执行对应形状语义。单独加载地图资源不能批准完整场景。

大图按源像素裁切为最多 1024×1024 的 GPU 页，跨页区域在编译时拆成图元；不缩放原图。实际 7 个 tex3ds 页承载 6 个源纹理（含水纹的 4 个帧），169,981 个打包图元保留最近点 1:1 绘制。图元 prototype 去重与有界 cell 记录将资源从约 22 MB 降至约 11 MB（包含编译期 BVH），地图覆盖、源坐标、图素和碰撞均不删减。`load_file()` 移动候选字节所有权，避免正常加载再复制整个资源。

纹理区域坐标相对于各裁切页。Draw.flags 为 flip X=1、flip Y=2、transpose=4；Godot 的负 rect size 编码翻转，不移动世界锚点。tile z 保留 0/1 的原值，Canvas 祖先与 YSort 信息供场景宿主统一排序；静态地图不能覆盖动态人物排序规则。

水纹来源默认 fps=4、frame delay=0，数值保存在二进制。`FieldMapAnimationState` 按每次实际画面提交更新，复用同一 group 状态；f32 累加、严格大于帧时长、每次最多推进 frame_count 帧，首次绘制不计 delta。此机制遵循 [Godot 3.6.2 AnimatedTexture 实现](https://github.com/godotengine/godot/blob/3.6.2-stable/scene/resources/texture.cpp)，不能用总 elapsed 的取模替代停顿后的原生时钟行为。

FlagLandmark 来源的 appear/disappear/delete_if_hidden 规则也在资源中。场景宿主必须先执行真实 Ready 和 flag 通知，再提供 Visible/Hidden/Deleted 状态。queue_free 请求尚未经过 deferred 边界时，原节点仍保持原来的可见性和碰撞；Deleted 只能在实际删除边界返回。单纯 Hidden 只隐藏图形，不删除碰撞。未提供消费者或 Ready 未执行时，查询明确拒绝。

格式使用 128 字节头、18 个 24 字节目录项，family `0x454e0019`、版本 1、静态地图 capability 1。固定记录包括 Layer 64、Cell 26、Draw 28、Prototype 52、Polygon 56、Point 8、Texture 104、Canvas 36、FlagGate 20、Quadrant 44、SpatialNode 32、LocalGeometry 28、LocalPoint 8、NativeConcaveNode 32、NativeLeaf 4、ShapeTransform 24 字节。加载器检查期望场景/上游/来源身份、CRC、目录、范围、字符串、稳定 ID、源 owner、纹理与 UV、几何类型、凸性、空间索引、动画序列以及有限数值；失败时保留旧资源。

可见图元与运动区域先遍历编译期 BVH，仅检查附近 quadrant 中的实际图元或碰撞。查询结果恢复 source quadrant 顺序；不会每帧扫描 169,981 个图元，也不会对每个敌人的运动扫描完整碰撞表。空间索引检查树连接、叶唯一性、范围包围与深度，容量不足明确拒绝。此索引是筛选实现，不能冒充 Godot 原版 broadphase 的接触顺序证明。

Concave 的局部 BVH 与地图筛选树独立。官方引擎中的 `Array.sort_custom` 调用该版本实际 `SortArray`，按原生 `_generate_bvh` 的最长轴、中心比较和左右递归规则生成局部节点与叶序；没有额外 stable tie-break。原 local endpoint pair ID 与 local→world 变换独立保存，加载器核对其与世界点的一一对应。`concave_segments()` 在每次真实 shape-pair 查询的局部范围内，按原生严格 Rect2 相交和左右 DFS 给出端点对序号，翻转/转置不会把它改成世界坐标排序。

`identity()` 返回实际已受检的场景身份；`source_scene()` 返回二进制内来源字符串。场景/运动后端应与其所属 Room 的来源场景和 pin 交叉绑定，不能让调用者传入一个任意字符串来伪造已批准场景。

重新导出补充数据需使用已核对并完成资源导入的隔离工程：

```sh
python3 tools/podunk_map.py export-script --script /private/native-project/field_map_detail.gd
Godot_v3.6.2-stable_linux_headless.64 --path /private/native-project --script res://field_map_detail.gd
python3 tools/podunk_map.py extract --native /private/podunk-exact.json --detail /private/native-project/field_map_detail.json --source build/podunk-scene-native-reference/source.json
python3 tools/podunk_map.py compile --tex3ds /opt/devkitpro/tools/bin/tex3ds
python3 tools/podunk_map.py verify
```

新的 source/IR SHA 必须经过语义审查，不能只刷新 `compatibility/reviews/podunk-scene-map-v0410.json` 来接受变化。TileMap 变换遵循 [Godot 3.6.2 tile_map.cpp](https://github.com/godotengine/godot/blob/3.6.2-stable/scene/2d/tile_map.cpp)，物理形状语义依照该版本的软件物理后端。

当前完成官方来源导出、真实 tex3ds 转换、C++17 编译和生成资源的正常格式准入。手动正常/负向案例源位于 `tests/field_map_tests.cpp`；未执行测试套件或 sanitizer。本资源结果不能证明 3DS 交叉构建、模拟器性能或真机运行。完整场景仍需要全部非 TileMap 身体/Area、动态工厂、事件/剧情、源层级排序与余下脚本消费者；`scene_admitted()` 明确为 false。
