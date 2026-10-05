# 波克顿 Sparkles 来源与单实例消费者

固定上游 `7d9246600fffe518408f5830d4848635019005a3`，完整官方 Godot 3.6.2 Podunk 导出中的 22 个 `Scripts/misc/sparkles.gd` 实例均进入独立来源 IR 和 `.encsparkles`。游戏不读取 JSON。资源 family、版本、capability、场景身份、来源证明和 CRC 独立校验；失败不替换之前已加载数据。完整 Podunk 尚未准入。

原版 `_ready` 使用全局 `int(rand_range(0,47))`，动画实际只有 **40 帧**，因此最后一步是夹到第 39 帧，不能取模或扩成 48 帧。Ready 保留原始 postorder，与草地、敌人等机制共用一个 `SourceRandom`；绑定失败不提前消费随机数。

16 个 Present、3 个 DroppedItem 的 Sparkles 已由各自父消费者持有。资源显式承载 source owner，桥接器核对实际父 binding 的节点、稳定身份、Ready、源 pin、位置、完整帧序、速率、随机范围和纹理。`FieldSparklesRuntime::ready` 对这 19 个子节点只委托父 `ready_sparkles` 一次，读写同一个权威状态；其 idle 只能由父消费者推进，新的泛化 setters 对借用节点明确拒绝。父领取、停止、隐藏等操作因而不会与第二份动画状态分叉。ZooSignSparkle 的 3 个嵌套实例由新消费者独立持有。

新自有实例保留 AnimatedSprite 的 float32 timeout、准确边界、末帧 / 反向播放、原版同步 `animation_finished` 后 `frame_changed` 信号顺序，以及 stop 保留帧和 play 仅在状态改变时重置 timeout。隐藏不停止内部 idle，树暂停、实际父生命周期和 OS update 状态由受检 Host 提供。源 signal 中的即时删除尚未准入，明确拒绝；正常 queue_free 在实际场景 deferred 阶段清理。

GPU 原语复用已有真实 tex3ds `graphics/story/present-sparkles/sparkles.t3x`，核对原图、producer、tex3ds 与输出 receipt。按来源帧直接采样，无 CPU 图片生成；原版先 floor 局部 centered offset，再在 Canvas 阶段 round 每个已变换顶点，包含掉落物收集时的分数缩放。完整 Canvas / YSort 队列、实际父 visibility / transform / material 与顶层场景宿主仍由集成层提供；未知 shader 或旋转 / shear 会拒绝。

公开编译入口 `python tools/field_sparkles.py compile` 不依赖私有 native export 或忽略的 reports。重新提取必须显式提供完整的官方 native export。手动负向 parser / owner cases 保存在 `manualtests/field_sparkles_tests.cpp`；日常开发仅编译这些源码，不运行测试。正式场景桥接、设备画面和可玩流程须由主代理集成后分别验收。
