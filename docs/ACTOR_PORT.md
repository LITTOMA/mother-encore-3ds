# 原版人物机制

基线为官方 Act2 v0.4.1.0，提交 `7d9246600fffe518408f5830d4848635019005a3`。本页说明共享核心中已审查的单人移动与离散动画机制。人物、静态碰撞、房屋交互与局部剧情已接入 3DS；完整支持范围见 [STATUS](STATUS.md)。局部函数对照不代表整个 Player.gd 或 AnimationTree 已兼容。

## 移动

`runtime/movement.cpp` 映射 Player.gd 的 `_move`、`_movement` 中开局单人 MOVE 分支，以及 controlsManager.gd 的 `_get_vector_sign`。源文件、函数指纹和审查范围在 `compatibility/reviews/movement-v0410.json`。

机制保留八方向归一化、上一帧速度参与移动、两次物理 delta、取整前的 substantial 判定、位置取整、奔跑 / 蹲下切换、tap-run、暂停 / 过门输入边缘与 moved 信号次数。行走、奔跑速度和 movement divisor 从受检 RoomView 规则读取，不由 C++ 内容表提供。

方向函数的 `threshold := 0` 在 GDScript 中推断为整数，C++ 接口也使用整数；不增加另一个浮点死区规则。Godot round 的半值向远离零取整。

移动通过 `MotionSolver` 请求物理结果。`FreeMotionSolver` 仅用于无碰撞对照；房屋实际移动使用独立的静态碰撞后端。零击退速度下的第二次 slide 请求仍保留，因为物理后端可能执行穿透恢复。未知输入、损坏状态及后端非有限输出会失败，状态保持不变。

这项 MOVE 映射不覆盖队伍跟随、teleport / relay、攀爬、击退或完整玩家脚本；交互与剧情动作由各自受检模块处理。

参考 harness 保留原函数正文，只替换明确列出的非运动服务。输入提前一帧送入 Godot physics callback，符合 action_press / release 的物理边缘时序；不能在刚设置 Input 的同次回调中假定 just_pressed 已生效。局部参考包含 14 段轨迹、227 帧和 605 组方向输入。

## 离散动画

Ninten 源纹理为 310×580、10×20 格，每帧 31×29。受检人物纹理已接入房屋渲染。动画数据保留实际 NodePath、关键帧和可见性绑定，不假定轨道编号固定。

`runtime/animation.cpp` 执行 Idle / Walk / Run / Crouch 的八方向离散帧轨道。Run 只有 frame 轨道，保留前一状态的可见性；其他状态按受检数据控制 main 与 SpecialAnimations。Idle Right 源轨道在 0.1 秒循环以外的六个关键帧也保留，不删除或改写原时间。

Godot 离散更新按区间事件执行，通常采用 `[from,to)`；恰好在 to 的事件留到下一次 advance。零 delta seek、片尾和循环整倍数另有规则，不能用“当前时间之前最后一帧”替代。规则从固定 Godot 3.6.2 Animation / AnimationPlayer 源码核对，并与实际播放比较。局部参考包含 1,216 项取样和 2,400 次连续播放推进。

场景导出可能缩短小数；转换时同时读取原生播放参考的精确时间，核对缩短值后生成外部动画 profile，再由内容编译器形成受检资源。`tools/character_animation.py` 输出 JSON，禁止生成 C++ 内容头文件。未知轨道、方法、属性、插值、导入状态、帧范围、更新模式、版本与缺失参考均拒绝转换。

完整 AnimationTree 转移、混合、随机眨眼、服装及其他人物状态不在这组离散运动轨道的兼容声明内。

## 参考复现

使用已核实的官方 Godot 3.6.2，通过 `GODOT3` 指定可执行文件。工作目录使用新路径，官方上游 checkout 保持只读：

```sh
python3 tools/run_actor_reference.py --godot "$GODOT3" --work build/actor-reference --reports build/actor-reference-report
python3 tools/character_animation.py --reference build/actor-reference-report/player-data.json --playback build/actor-reference-report/animation.json --out build/native/animation-profile.json
```

工具检查提交、源文件指纹与资源导入，输出参考结果及来源记录。更新 profile 或 reviewed SHA 需要语义审查和受影响回归；生成参考不自动扩大兼容范围。平台呈现见 [OPENING_RENDERER](OPENING_RENDERER.md)，实际验证要求见 [TESTING](TESTING.md)。
