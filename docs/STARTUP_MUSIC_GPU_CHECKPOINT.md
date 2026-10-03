# 启动字段、Dormant音乐与GPU纹理实验检查点

2026-10-03。当前开发检查点；不是稳定版或完整游戏。

## 已集成

- 原版Ninten、Ana、Lloyd、Pippi、Teddy和favorite food六字段；L返回并保留待定值。五个角色记录仅Ninten在队。源UID按CashCard、BaseballCap、KickMeNote、GlassesCelluloid顺序，在隔离RNG/ledger中准备；所有名字、食物、会话与新owner一起提交。取消不污染旧会话。
- Podunk音乐controller/service与分段准备进入源码，当前House/New Game/battle全程Dormant，无准备IO、额外缓冲或区域NDSP调用。每步PCM读取上限8KiB，旧AudioPlayer只补队列，不推进淡入淡出或游戏。13区域/5曲的77,419,288字节PCM保留为源码/测试资源，当前生产RomFS不携带。
- Pillow texture-strip独立显式实验开关。源码默认0:0:0；常规开发包GPU/证书/texture为1:1:0，CPU兼容包0:0:0，纹理实验包1:1:1另存。原CPU和精确span fallback保留。

## 本次验收

一次合并后host87/87与ASan/UBSan87/87通过，LeakSanitizer未运行。真实devkitARM为上述三配置生成3DSX/CIA；全部CIA资源与受检staging逐字节一致，Podunk资源未提前装入。

一次明确生成入口的组合ARM诊断通过六字段、Ab/Apple pie输入、L返回、四UID原子提交和Dormant音乐，再从明确生成post-Lamp起点进入实际Pillow texture菜单，Bash目标/取消及稳定owner均验证。无用户存档读写、截图、完整剧情重跑或新的像素读回/性能基准。

## 合并修复与资源安全

main/Makefile逐hunk合并；成功named commit中region.shutdown紧邻audio.reset_scene之前。原adapter测试通过仅测试用begin/step完成器保留所有旧断言；没有恢复同步生产API。Podunk音频生成器现在隔离通用opening.encaudio中间输出，只提交明确Podunk-owned文件，正/负所有权测试保护原House bank。四个诊断工具补齐新音乐平台对象链接；独立音乐验证工具以当前工程为默认根。

存档schema仍为1，rules7；精确rules6及旧singleton LOAD保持只读兼容，旧槽Record继续拒写。新五角色存档不能承诺由旧可执行文件读取。

## 后续约束

Settings、最终确认、原版intro尚未移植。Podunk真正激活前必须实现源序Area信号、真实scene commit、显式staging清单登记及构建资源门禁；禁止依赖缺失路径默默启用。未合入BVH/Area/NPC factory和暂停的recovery工具。

独立纹理候选曾有768000 paired readback像素与59.835FPS，均不是本最终程序的新验收。PICA真机纹理采样/光栅精度、Old/New3DS、DSP听感、CIA安装与整进程内存余量仍未验证。
