# Mick 对话停顿

原转换器把 Mick 英文及简体中文台词的 `[WAIT@]` 降为普通换行。长句自动打印、滚动，原版等待玩家确认的段落边界丢失。

已重新审查固定上游 `7d9246600fffe518408f5830d4848635019005a3` 的 `text_tools.gd`（wait@ → CHAR_WAIT + 换行 + bullet）、`AbstractDialogueBox.gd`（遇 CHAR_WAIT 停止并重置输入加速）和 `DialogueBox.gd`（重新按键才继续），以及 Mick 已支持对话树和对应英文、中文翻译。来源指纹由原生成器记录；上游只读。

Mick 编译器保留 U+2063 等待控制字符与换行，现有共享对话框的逐字打印器执行停顿。控制字符不绘制、不计宽度；说话人不允许控制字符。原选项和剧情命令仍在整句结束后处理，不能越过内部停顿提前选择或领取钥匙。Mick 资源能力由 2 升至 3，旧消费者明确拒绝；包格式、剧情规则和存档编码不变。Session IR 仅更新依赖指纹，生成的 Session 二进制不变。

只读模拟器观察 ABI v6（764 字节）增加对话框 active / finished / stopped / visible 状态；无指针、内存写入、随机数消耗或存档操作。旧/未知版本、错误长度、非法布尔等仍拒绝。

针对性 MSVC 共享核心检查已运行：新用例检查全部 Mick 英文、中文文本的 26 个来源等待点（49,382 项断言），每个停顿空等 600 次更新；原 Mick 用例和房屋演出用例（3,037 项断言）通过。22 项 Python 资源及观察协议检查通过。计时断言保留原版 WAIT 前剩余时间，不要求恢复时清零。日志在 `build/mick-dialogue-pacing/`。交叉构建、模拟器及真机结果需独立记录，不能由主机通过推定。

独立参考 `tools/run_mick_dialogue_reference.py` 已在固定官方 Godot 3.6.2 中实际运行抽取的原版 `AbstractDialogueBox` 方法：14 个英中短语、26 个等待点，每处空等 600 次更新均保持段落和可见字符数，再按 A 才恢复。参考仅使用原版逐字打印方法与原生 RichTextLabel；设置、完成回调及 bullet 绘制使用列明适配器，不代表原版整场景或像素对照。日志、来源、方法及引擎指纹在 `build/emulator-regression/mick-pacing-original-1/`。

首个真实构建 353860f 被命名资源对 Session IR 的旧依赖指纹拒绝（`Changed name dependency content/native-session.json`）。已用原命名工具 `reuse-session --base cfde7bbcc1d400d846cb3b2487506d27a663c1ba` 重新准入；工具验证命名相关内容不变，生成命名二进制逐字节不变，不手改 SHA 或跳过检查。
