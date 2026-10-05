# Podunk 草动态资源与运行消费者

本切片从固定上游 `7d9246600fffe518408f5830d4848635019005a3` 的完整 Podunk 场景提取来源绑定，并接入共享核心的草动态执行器。它尚未开放正常游戏中的 Podunk 换场；地图渲染、碰撞、其他动态工厂和剧情仍需继续接入。

官方 Godot 3.6.2 在隔离工程中解析原生实例、嵌套覆盖、节点顺序、完整 TileMap 与资源图。原版脚本和信号保留来源声明但不执行，因此导出不能批准兼容性。完整结果包含 8,686 个节点、44 个 TileMap 和 173,211 个格子。315 MB 的精确导出及原始日志保存在开发者私有 `build/`，不提交到仓库。

`content/podunk-scene.json` 是编译输入，保留 1,058 个草生成器的精确位置、可见性边界、原生名称哈希及子节点优先的 Ready 顺序。其余 1,099 个脚本绑定逐项保留为 Pending；包括内嵌脚本，不会静默忽略。运行时只加载独立 `romfs/data/podunk.encfield` 和两个经来源核对、由 tex3ds 转换的草纹理。

`FieldData` 必须由受检目录提供期望场景身份、上游提交和来源 SHA。它检查格式、capability、目标、CRC、目录顺序与范围、字符串、重复身份、Ready 顺序、原生 seed、纹理和帧索引，以及有限数值。失败的加载保留已有资源。Capability 1 仅承载草适配器，即使 Pending 列表为空，也不能自行升级为完整场景支持。

`FieldRuntime` 准备资源时不推进随机流。草 Ready 由场景调度者按原版序号调用，并使用同一 `SourceRandom` 执行 `seed(name.hash())`、纹理选择和翻转的两次 `randi()`。不能改成每个草节点独立随机流，因为原作会重设共享全局随机状态。

消费者接受实际可见性通知与 Area2D 身体进入、离开事件，保存进入顺序和当前全局 X。它保留平均位置的整数截断、原始最近点 BlendSpace 帧绑定和翻转。物理步更新请求的动画状态；默认 idle 时钟处理 AnimationTree、Timer 和 Tween。Timer 使用 float 等待时间、double 剩余时间，并保留小于零才触发的边界。重入创建的多个 Tween 不取消旧超时协程。

屏幕离开清空生成器当前对象，但旧 Area、重叠身体和 Tween 留到显式 deferred 删除边界。边界前再次进入可生成新对象；独立实例 generation 区分新旧身体回调。删除边界之后，旧回调明确拒绝。`grass_draws()` 提供纹理、帧、位置、翻转和缩放，供 3DS 原生渲染后端消费。完整场景激活当前仍明确拒绝。

格式使用 128 字节头、24 字节目录项；固定记录为 Profile 120 字节、Grass 56 字节、Texture 88 字节、Pending 56 字节。规则、坐标、帧、资源路径与哈希均在二进制中，C++ 保存 schema 与执行机制。

可重新导出和生成资源：

```sh
python3 tools/podunk_scene.py export --godot /path/to/Godot_v3.6.2-stable_linux_headless.64 --work build/podunk-source-reference --logs /private/build/podunk-source-logs
python3 tools/podunk_scene.py compile --tex3ds /opt/devkitpro/tools/bin/tex3ds
python3 tools/podunk_scene.py verify
```

新的来源导出必须经语义审查并更新对应 review；不能只刷新哈希来接受变化。资源生成会进行必要来源与格式准入，这不是运行测试套件。

本切片已完成官方原生来源导出、实际 tex3ds 转换、资源编译与一致性准入；共享核心和手动正常/负向用例源已通过 C++17 编译。测试程序未运行，3DS 交叉构建、模拟器帧率与真机均不由这些结果证明。手动用例位于 `tests/field_scene_tests.cpp`，覆盖格式、来源、CRC、范围、重复身份、Ready/RNG、完整场景拒绝与 deferred 实例失效。
