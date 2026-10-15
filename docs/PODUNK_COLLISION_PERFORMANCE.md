# 院子原点附近的碰撞查询退化

以主线 48433a4 为基础，保留所有 ENCMAP01、资源、碰撞形状、条件、形状顺序及窄相求解规则。

tools/podunk_field.py 将无碰撞区块的包围盒写成四个零。旧版 FieldCollision::collect 在查询覆盖世界原点时，仍遍历这些区块中的装饰格子，再逐格发现没有碰撞。当前 Podunk 的一次小范围原点查询会检查超过十万格。院子及 Mick 的扫掠查询可以覆盖这一位置；模拟器中实际游戏帧率约 1–2 Hz，但模拟刷新和速度仍约 60 Hz / 100%。

场景绑定时现在从已审查资源建立只含碰撞格子的派生缓存。无形状区块不会加入缓存；真正位于原点的碰撞仍完整保留。每次查询依次检查原有区块边界与缓存的形状范围，保持图层、区块、格子、形状、静态体、Mick 的原顺序。条件刷新会重建缓存；存档和资源格式不变，没有手改生成资源。

field_collision_index 将缓存查询与修复前的完整枚举逐项比较：数量、预算结果、顺序、偏移、包围盒、顶点和法线，以及原点、院子、区块边界、远处地形、禁用条件、重绑定和动态角色。MSVC 与 Windows Clang ASan/UBSan 各通过 220,251 项断言。Windows sanitizer 保留两条 interception_win 警告，不能代替 Linux 结果。Linux GCC、Clang 的同一目标普通及 ASan/UBSan 检查也均通过，默认保留泄漏检查；对应 full run 38714474150 的作业 116353689219、116353689228。

性能检查必须记录真实 GSP 游戏提交频率，不能使用 frame.current 的模拟刷新计数替代。配套 Azahar 小补丁从 system.info.performance 暴露已有的只读性能采样；不重置计数，不写游戏内存，也不改变 CPU/GPU 速度或游戏逻辑。输入和存档测试在隔离目录运行，保留有界收据、日志和截图后清理临时 profile。

完整检查首先暴露主线原有的 CMake 配置错误：gameplay_audio 区块重复打开 BUILD_TESTING，缺少对应结束，make test 在配置阶段失败。现移除重复条件，保持目标意图与 BUILD_TESTING=OFF 的作用域，另行重跑完整检查；这不改变游戏代码。初次失败日志保存在 build/yard-performance/gcc-full.log 和 clang-full.log。

配置修复后的完整检查继续在主线旧测试夹具报错：ValueError: Drawer item icon already appended。完整检查未绕过；为独立核验本修复，手动 full 模式现在先运行新碰撞目标的 GCC/Clang 普通及 ASan/UBSan 检查，再继续原有全量构建和测试。资源在同一源码检出先由 make content 生成并审查，后续 make test -o content 复用它；没有修改夹具期望或加入忽略。

游戏 53765f5 的真实 ARM / 3DSX / CIA 构建与提取后的 323 项资源核对通过。此后的提交只涉及 CMake、CI 和文档，runtime、include、platform、content、romfs 保持一致。已核验安装包留在 dist/podunk-collision-53765f5/，来源收据随包保留。

Azahar Vulkan、100% CPU 时钟下，用正常 LOAD、房门和方向键，从同一个明确的人工出口存档进行两次独立冷启动。原始本地游戏及已知游戏 78d9261 在院子实测约 1–2 FPS，热点采样落在 FieldCollision::collect 的装饰格子枚举中。修复包两次原地各采样 12 个窗口，实际 GSP 游戏帧率分别为 59.443–60.533、59.452–60.513；随后各做四次左右往返并再次达到稳定帧率。两轮均确认活动 FIELD RUNTIME 标记，结果投影相同。图像仅用于核对地点，不作像素等价判断；未写游戏内存或注入 RNG。

人工存档明确把 talked_to_dad 置真、位置设在房门内，其余使用源默认值；当前 main 的共享核心验证、编码、解码再次验证，生成字节与修复前使用的存档完全相同。此测试不证明自然游玩到该进度，也不证明完整原版、CIA 安装或真机通过。完整验证收据、截图和日志位于 build/yard-performance/validation-receipt.json 及 build/emulator-regression/yard-fixed-auto-{1,2}/。

在所有图层/静态体启用的对照用例中，一次原点查询的格子访问从 121,855 降至 71；缓存内容约 219,480 字节，不含分配器容量与元数据。没有靠 (0,0) 坐标忽略真实形状。两次临时下载压缩包合计 711,066,810 字节已删除，另清理不再使用的 244,241,908 字节主线下载产物；本轮全部隔离模拟器临时 profile 已自动清理。原有用户游戏、个人存档和 DSP 来源保持原位。
