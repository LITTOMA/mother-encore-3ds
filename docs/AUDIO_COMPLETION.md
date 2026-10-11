# 已支持场景的音频补全

基于主线 ed09c5d（PR #39）继续开发；上游仍固定为 7d9246600fffe518408f5830d4848635019005a3。项目是局部原生移植：受检来源经独立二进制资源交给共享 C++ 核心，3DS 负责 NDSP、绘制与输入。本次沿用这条路径补齐已有事件的音频消费者，不扩展战斗或场景玩法。

## 来源与接线

- 开局 bank 从 27 项扩为 53 项；原 27 项 PCM SHA、帧数及循环起点保持一致。Podunk 原有五首曲目另成一库，合计 58 项；PCM 从固定上游恢复，不纳入 Git。上游 LICENSE 对相关游戏修改的许可继续保留，素材不重新许可为 MIT。
- gameplay-audio-bindings.json 从 audioManager.gd 和 BattleSystem.gd 的字典提取 24 个角色；另枚举并核对已审查房屋门、对白和 Mick 命令需要的声音。文件名只存在于资源；C++ 只使用执行角色和校验后的路径。
- 房屋与室外门的开启、关闭、楼梯声；对白 voice start/stop 和菜单开合；电话请求；Mick 吠叫与呜咽接入实际播放。Text 与 Phone 分别使用 NDSP 22/23，区域音乐保留 4–19。电话暂未复现空间衰减；战后恢复音乐的原版三秒淡入尚未映射。
- 战斗按现有事件接入命令光标与确认、返回、敌人回合、命中、SMASH、受伤、濒死、闪避与普通击败；不消耗额外随机数。胜利按 boss / overworldBattleMusic 选择结算音乐，升级叠加 Ninten 轨，学习 PSI 播提示。升级两轨先完成 PCM 校验后提交；不宣称样本级同步。Boss 击败动画内部音轨、完整战斗文字语音仍需单独映射。
- Podunk 的 13 个 MusicChanger 使用原场景矩形和凸分解多边形，与实际玩家碰撞体做 SAT 相交。以场景顺序提交进入/退出，交给现有 MusicRegionController 处理旗标、过场、共享曲目和渐变；换场发 tree_exit 并排空。这里没有实现 Godot 的通用区域系统，也不承诺同时边界事件的 Godot broadphase 顺序。

## 资源与兼容

新增 ENCCUE01 v1 与 ENCZON01 v1，校验 magic、版本、长度、CRC、计数、路径、有限凸几何及尾随字节；失败不替换已加载状态。资源目录 capability 5 必须同时提供 GameplayAudio、FieldMusic、FieldAudio 和 FieldMusicZones。目录迁移见 [ROMFS_LAYOUT](ROMFS_LAYOUT.md)，不改变稳定身份、玩法规则与存档 schema。

原 M3/bump.wav 的 96 kHz → 48 kHz 转换用本机 FFmpeg 6.0 时不匹配旧 PCM；改用已安装的 FFmpeg 7.0 后 27 项全部与旧指纹相同。新增 EB/miss.wav 也明确记录 96 kHz → 48 kHz 的转换策略，其他资产沿用原导入循环元数据。收据记录实际解码器，禁止通过刷新旧哈希掩盖差异。

## 验证边界

本地资源图使用原编译器进行来源与格式准入；日志保存于 reports/audio-layout/。共享核心新增文件已通过 MSVC 编译，但整个主机目标因既有 locale_preference.cpp 的 unistd.h 在 Windows 缺失而失败。WSL 启动报 CO_E_SERVER_EXEC_FAILURE，本地 Linux 工具链不可用。真实 3DSX / CIA 交由本分支推送后的 Actions 构建，结果另行记录。

依仓库约定，本次未启动手动测试套件或 sanitizer；已添加新格式、目录和来源迁移的负面测试，并更新资源目录能力用例。模拟器、真机听感、混音、边界切换、内存和加载时间均未验收。

## 用户回报后的修正（2026-10-11）

用户回报标题及其它 BGM 静音、音效正常；房屋出门显示 "Invalid music region preparation state/byte budget"。出门调用提交 256 KiB，而适配器拒绝超过 64 KiB 的预算，已改为与校验器共享上限；保留原有逐块 CRC 准入和失败回滚。

暂停状态现在由声道显式持有：只在状态变化时调用 NDSP，暂停时不补给或推进淡变，停止时清空队列并解除暂停。未暂停的标题播放恢复主线原有的 NDSP 启动调用顺序；这项修正尚不能证明用户回报的静音根因。下屏增加实际 BGM 资源 ID、逻辑活动状态、DSP 播放/暂停状态、样本位置和队列序号，每半秒采样。

本地用户 3DSX 的开局音频银行、标题菜单包和 22,311,936 字节标题 PCM 与当前资源完全一致；原 27 个音频记录和 master/silence 增益也与主线一致。标题 PCM 含非零采样。检查记录见 reports/audio-layout/title-resource-inspection.json；这些是资源检查，不是听感验证。标题静音仍待运行状态诊断，不宣称已恢复。

手动音频替身补齐暂停接口、8 声道边界、AuxiliaryEffect 20/21 与 Text/Phone 22/23 映射；添加标题暂停/恢复/停止/重播检查，区域准备使用生产相同上限。未运行测试套件或 sanitizer。
