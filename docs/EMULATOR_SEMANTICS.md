# 模拟器语义验收

用户已明确要求本次持续开发按进度运行自动化验收；这属于 AGENTS.md 的明确测试授权。工具按需执行，不建立后台定时任务，也不改变其它工作的默认手动测试策略。

以固定上游 `7d9246600fffe518408f5830d4848635019005a3` 为原作来源。原版运行于 Godot 3.6.2，真实 3DSX 运行于用户提供的 Azahar CDB 改版。两侧分别保留来源和日志，比较关键结果，不要求像素、字节、绝对帧号或随机 UID 相同。

## 工作顺序

1. 审查切片依赖，先执行独立原版方法参考，记录真实来源 SHA、引擎 SHA、方法 SHA、适配器及未覆盖范围。
2. 把可比结果写成受检测试场景。真实内容仍由原生成器编译到独立资源；测试不修改上游或游戏内存。
3. 核对真实交叉构建的提交、3DSX 大小和 SHA；通过 CDB 在全新的便携存档环境重复执行至少两次。
4. 检查各检查点的剧情/菜单结果，以及取消后恢复控制、HP/PP、物品数量和位置等守恒。发现差异先定位来源，不能直接改期望使其通过。
5. 保留每次成功/失败凭据及必要截图，停止本次创建的模拟器进程后删除临时 profile/cache。独立登记视觉、声音、CIA 安装和真机验收。

## 当前工具

`tools/run_field_menu_reference.py` 提取原版 Pause/PSI 方法，在真实 Godot 3.6.2 中运行。原版技能 YAML、技能筛选、空选择、关闭/返回及零 PP 心灵感应调度均有明确来源；AnimationPlayer 使用受检时长。字符/世界/音频服务及没有比较的布局采用列明的适配器。这不是原版完整游戏自动通关。

`tools/run_emulator_semantics.py` 通过回环 TCP JSON-RPC 控制 CDB，检查只读 `EncoreSemanticProbe` ABI v3（716 字节）。记录有界且没有指针，不消费 RNG、没有存档 IO 或行为入口。奇数提交序号、截断、旧/未知版本、文本溢出和非有限位置明确拒绝。当前字段覆盖菜单、开场场景、玩家、HP/PP、库存计数、战斗阶段 / 敌人 HP、门开锁位图以及实际 NDSP 音乐播放、采样位置和缓冲序号。没有宣称已经观察全部旗标、物品 UID、敌人和剧情事件。

测试场景 `tests/emulator/opening-menu.json` 从正常标题入口完成命名、两段原版开场跳过和房屋菜单操作，包含 START/B 开关、摇杆选择 PSI、1 级空列表确认和 SELECT 保护。命名取消保留原版 0.5 秒保护，不把早于该边界的输入当成失败。最终结果对照独立 Godot 参考；测试计时和 DSP 游标不参与跨次幂等比较。音频模式另检查标题 Mother Earth，以及沿正常移动触发 Lamp 第 2 段和进入战斗后的 Poltergeist 身份、实际 DSP 播放及采样/缓冲持续变化，不能用已排队声音代替实际消费。

## 运行

Windows 的 CDB Azahar 需要启用 `ENABLE_CDB`。普通未修改的 Azahar 不提供该接口。用户改版来源是 `D:/GameDev/3DS/Tools/azahar-fork`；针对真实构建/启动失败的修复保存在独立分支，见本地构建日志与修复提交。

先执行参考：

```powershell
python -X utf8 tools/run_field_menu_reference.py --godot <official-Godot-3.6.2.exe> --out build/emulator-regression/reference-unique
```

再执行对应提交的真实构建：

```powershell
python -X utf8 tools/run_emulator_semantics.py --emulator <CDB-Azahar.exe> --game <verified.3dsx> --receipt <build-receipt.json> --scenario tests/emulator/opening-menu.json --original-reference build/emulator-regression/reference-unique/reference.json --out build/emulator-regression/run-unique
```

可加 `--dsp-component <existing-private-dspfirm.cdc> --require-audio`。仅复制明确传入的本地 DSP 组件到本次临时 profile，不读取个人游戏存档，不下载 firmware/key，不将组件入库或打包。未提供时如实记录 NDSP 不可用；无音频模式不能验证 BGM。

每个输出目录必须全新且位于 `build/emulator-regression`。2–4 次重复，每次最多 256 步、8 张 PNG，单张 2 MiB、CDB 单条 8 MiB、事件 128 条，日志 8 MiB，运行前至少 2 GiB 空闲。正常结束或断言失败均清理已标记的临时 profile；绝不清理个人 Azahar 目录或源代码。历史凭据可以长期保留，过期大型安装包和重新生成的中间文件应及时清理。

## 验证边界

ABI v2 的真实构建 `4ca6ccfb480d0c5bf8e6a428e0fa02bb26e41659`（3DSX SHA256 `48dcab4fee49e684ee4f1eaca7585c482ffd3fe8d22cc78c09c59a774867f617`）已实际完成两次带 DSP 的冷启动，每次 43 个检查点得到相同语义结果，并对照 4 个独立原版菜单结果。标题 ID1202、Lamp 剧情与战斗 ID21 均观察到 DSP 播放与持续消费。刚进入房屋、尚未触发 Lamp 时的静音符合原版旗标条件；没有为测试提前播放音乐。每次结束后的临时 profile 已核实不存在。私有原始日志与凭据留在 `build/emulator-regression/v2-audio-source-flow/`，源参考留在 `build/emulator-regression/menu-reference-final/`。

首版 ABI v1 的真实构建 `c3feb10e0be67e319989dbf31cfe753295d2fd72` 已完成无 DSP 的两次冷启动、菜单与 PSI 空列表相同结果检查。该凭据仅证明它记录的构建与场景；v2 的新 DSP/开场字段按上面记录独立验收，没有复用 v1 结果。

本机已成功构建 CDB 模拟器并实际启动 3DSX。修复了 CDB 公共头文件的 JSON 依赖和外层 CLI 把 CDB 参数误当作压缩请求的问题。独立 Godot 菜单参考已实际通过。手动共享核心检查暴露了 CMake 的重复条件块；修复后 Windows MSVC 仍因既有 POSIX `unistd.h` 依赖不能构建整个核心，应与 Linux GCC/Clang、sanitizers 结果分开登记。

Mick 领取钥匙、已学会 PSI 的目标与对白、地下室钥匙使用、战斗和其余场景仍需逐项加入真实输入用例。抽取方法参考、模拟器运行和 Old/New 3DS 真机是不同证据层次，不能由一层推定另一层已经通过。

全面回归还暴露了旧 Items fixture 把已经追加的喷雾图标再次传入原生成器的问题，以及 PresentData 测试调用不存在的 valid()。前者严格核对唯一受检扩展后只传入原 UI 前缀，未知/重复/错误哈希仍拒绝；后者检查实际 view().valid()，原损坏加载/回滚断言保留，已实际通过 3737 项 PresentData 检查。全面 GCC/Clang/sanitizers 必须以修复后重新执行的具体结果为准。
