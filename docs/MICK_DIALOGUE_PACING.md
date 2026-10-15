# Mick 对话停顿

原转换器把 Mick 英文及简体中文台词的 `[WAIT@]` 降为普通换行。长句自动打印、滚动，原版等待玩家确认的段落边界丢失。

已重新审查固定上游 `7d9246600fffe518408f5830d4848635019005a3` 的 `text_tools.gd`（wait@ → CHAR_WAIT + 换行 + bullet）、`AbstractDialogueBox.gd`（遇 CHAR_WAIT 停止并重置输入加速）和 `DialogueBox.gd`（重新按键才继续），以及 Mick 已支持对话树和对应英文、中文翻译。来源指纹由原生成器记录；上游只读。

Mick 编译器保留 U+2063 等待控制字符与换行，现有共享对话框的逐字打印器执行停顿。控制字符不绘制、不计宽度；说话人不允许控制字符。原选项和剧情命令仍在整句结束后处理，不能越过内部停顿提前选择或领取钥匙。Mick 资源能力由 2 升至 3，旧消费者明确拒绝；包格式、剧情规则和存档编码不变。Session IR 仅更新依赖指纹，生成的 Session 二进制不变。

只读模拟器观察 ABI v6（764 字节）增加对话框 active / finished / stopped / visible 状态；无指针、内存写入、随机数消耗或存档操作。旧/未知版本、错误长度、非法布尔等仍拒绝。

针对性 MSVC 共享核心检查已运行：新用例检查全部 Mick 英文、中文文本的 26 个来源等待点（49,382 项断言），每个停顿空等 600 次更新；原 Mick 用例和房屋演出用例（3,037 项断言）通过。22 项 Python 资源及观察协议检查通过。计时断言保留原版 WAIT 前剩余时间，不要求恢复时清零。日志在 `build/mick-dialogue-pacing/`。交叉构建、模拟器及真机结果需独立记录，不能由主机通过推定。

独立参考 `tools/run_mick_dialogue_reference.py` 已在固定官方 Godot 3.6.2 中实际运行抽取的原版 `AbstractDialogueBox` 方法：14 个英中短语、26 个等待点，每处空等 600 次更新均保持段落和可见字符数，再按 A 才恢复。参考仅使用原版逐字打印方法与原生 RichTextLabel；设置、完成回调及 bullet 绘制使用列明适配器，不代表原版整场景或像素对照。日志、来源、方法及引擎指纹在 `build/emulator-regression/mick-pacing-original-1/`。

首个真实构建 353860f 被命名资源对 Session IR 的旧依赖指纹拒绝（`Changed name dependency content/native-session.json`）。已用原命名工具 `reuse-session --base cfde7bbcc1d400d846cb3b2487506d27a663c1ba` 重新准入；工具验证命名相关内容不变，生成命名二进制逐字节不变，不手改 SHA 或跳过检查。

修复后的 `b5a838c40f2fde0022f82f95637ffccddef9f127` 已通过实际完整 CI [38730191106](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38730191106)：GCC 普通 162 项及 sanitizer C++ 147 项、Clang 普通及 sanitizer 各 147 项全部通过，保持默认泄漏检查。真实 3DSX / CIA 与 323 项打包资源检查通过。日志保存在 `build/mick-dialogue-pacing/`。

这份真实 3DSX 已完成英、中文各两次独立冷启动。每次正常 LOAD、出门、走到 Mick，经历首次对话的 5 个等待点、二选一、再次交谈的 2 个等待点并返回自由行走。每处空等 3 秒，可见字符与角色、剧情状态不变；随后持续按 A 3 秒也不能越过下一等待点。四次关键结果相同，不要求位置、绝对帧或像素相同；截图已人工查看。数据、输入与结果保存在 `build/emulator-regression/mick-pacing-{en,zh}-{1,2}/`，四个临时 profile 均已删除。

上述模拟器测试使用共享核心验证、编码并往返读取的人工房屋出口存档（`talked_to_dad=true`），不是自然通关开局的证据。测试只读观察状态、正常输入，禁止 CDB 内存写入；没有读取个人存档。已有私有 DSP 组件只用于临时 profile，不进入 Git 或测试版。CIA 安装、设备听感和 Old/New 3DS 真机仍未验证。

## 与最新主线集成

#57 合入后，已通过普通 merge 同步 `main` 的 `3db0c9cd08413274698da48521ca8c6d7f7dbdf5`，保留院子碰撞性能修复。集成提交为 `4fb7152c6c796cb201de20d248460baaa935f281`；[真实交叉构建 38732971554](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38732971554) 已成功，完整共享核心 / sanitizers 记录见 [38732977917](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38732977917)。

这份集成构建另外通过英文、中文各一次独立冷启动，均经过同样的 7 个等待点、选项及返回行走。两次关键结果与之前四次相同，等待期间应用仍持续更新；没有按像素或绝对帧匹配。日志及截图在 `build/emulator-regression/mick-pacing-main-{en,zh}-1/`，汇总为 `build/mick-dialogue-pacing/validation-receipt.json`。共六个临时 profile 均已清理，约 490 MB 的旧中间安装包已在核验目录及两版安装包 SHA 后移除，保留了旧构建凭据与实际测试日志。

保留的测试版位于 `dist/mick-dialogue-4fb7152/`，来源为上述集成提交，含原第三方声明和构建凭据：

| 文件 | 字节数 | SHA-256 |
|---|---:|---|
| 3DSX | 244273184 | `b3fd393d7debc434e0aa466e7c7d3fa155d0bcdde78914fb3eb044fe79685611` |
| CIA | 245625792 | `7281b6af132f7c7d0124caa8f690c8fd3ae0d369e9435e6416e096b3ec36069e` |

后续仅更新此验证文档，不改变已验证的游戏代码、内容、资源或测试。
