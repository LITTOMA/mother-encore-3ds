# Continue / LOAD 检查点

2026-10-02。固定上游Act2v0.4.1.0，提交`7d9246600fffe518408f5830d4848635019005a3`。本文件说明当前有限房屋切片的原生实现，不能当作完整游戏或任意原版存档兼容声明。

## 已接入的路径

启动进入静态标题，选择Load、已占用槽位、Play，执行淡出、受检读取、会话准备、新房屋场景构造、提交及世界揭示。Continue菜单使用外部opening.enccontinue，场景恢复规则使用opening.encrestore，槽位卡片和十槽Record复用受检SaveMenu资源。空槽不能Play，B可以返回；未移植动作显示明确边界。

原版标题开场、命名流程、Settings、Copy/Delete/Options尚未实现。New Game仅进入默认名字的开发开局。通过Dad的Record手动保存，退出不会自动保存。保存槽位与独立settings.encprefs记录最近选择；偏好文件不是游戏进度，也不改变存档格式。LOAD只接收当前兼容身份和受检原生格式的房屋切片；不是Godot原版存档导入器。

读取会重新校验文件、兼容身份及当前原生域，卡片扫描也必须完成显示字段校验。未知场景、未支持的设置/进度/修正值/物品身份、坏文件和缺失字体字形明确失败。当前字节文本渲染只支持受检字体中具有正advance的可打印ASCII；不替换字符、不截断、不偷偷开新游戏。一个现存但无效的槽位会阻断当前扫描，而非伪装成空槽。

## 会话恢复与场景所有权

`prepare_session_restore`产生独立的PreparedSessionRestore，先做结构和领域校验，再从受检资源解析stats、物品定义、flags和seen-dialogue绑定。完整snapshot仍保留名字、设置、playtime、存档时间/版本、party、keys、key inventory、affinities、encountered和显式false条目。等级对应基础stats来自受检ENCNSESS资源行；当前HP/PP、EXP、货币与已学技能保留。物品UID、装备状态和doses保留，UID 0与UINT32_MAX均合法；不重新分配存档物品身份。坐标直接使用存档值，不套门传送的Y偏移；四方向、整数对角和归一化对角朝向都保持有效，静止首帧不应改写它。

`prepare_fresh_house`构造新的FreshHouseState，world/house/presentation/phone固定在同一个owner内。以unique_ptr交换整个owner提交，不能移动含内部指针的成员或运动求解器。所有受检数据owner和共享SourceRandom必须长于借用它们的对象，不能让view指向临时资源。

存档flags在新世界构造前可用；按源顺序初始化NPC事件位置和碰撞，不从旧世界拷贝NPC姿态、脚本、战斗、电话、菜单、相机震动或剧情代理。新场景的Lamp重新实例化并由当前flags决定行为，不能继承旧场景被擦除的actor集合。`finish_scene_ready`才执行延迟DoorBlock清理和区域音乐请求，重复调用幂等；文件解码/槽位卡片/准备阶段不得调用它。区域音乐请求不是实际音频播放验收。

成功提交后平台替换会话、stats、库存和scene owner，清理旧的战斗/菜单/特效/音频请求，再在世界揭示边界释放玩家输入。验证、构造或时钟阶段失败保留旧场景与会话；生成随机流和UID账本也先暂存，成功才替换。

## LOAD随机副作用

存档不包含RNG状态。源Inventory.init_from_serialized即使保留已保存UID，也会计算一个随机fallback UID，并把生成值加入Item.used_uids_tab。源顺序为key_items、storage、Ninten、Ana、Lloyd、Teddy、Pippi，之后三个无库存PartyNPC不分配UID。未入队PartyMember的库存也参与。

原生单人快照是完整原版存档的有限受检投影；外部恢复资源把未保存角色固定为审查过的默认库存。常见开局投影因此分配四次：CashCard、BaseballCap及未入队Lloyd的KickMeNote、GlassesCelluloid。不能把它误写成两次，也不能声称原版缺失角色会自动补同样默认物品。

每件物品单独读取Unix秒和引擎相对微秒，按无符号64位取模执行源公式：

```text
seed = (unix_seconds + engine_ticks_usec) * current_pcg_state
       + 1442695040888963407
```

随后反复抽取直到不在生成UID账本中，只追加一次。碰撞重试不再次randomize，不设任意抽取上限。原生平台时钟起点、时机和此前随机历史不同，明确属于平台熵适配；不声称跨机器时序随机数相同、恢复保存时随机流或复现完整原版autoload启动。

生成UID账本由进程持有并跨LOAD保留；不能清空，也不能把存档UID插入其中。扫描卡片、解码、失败准备不得采样时钟或消耗随机数。UID副作用在解码/域校验/新场景构造成功后、平台提交前执行；helper暂存随机流及账本，失败回滚这些状态，但已经发生的外部时钟读取无法撤回。原生初始稳定UID及原版其他启动分配不由此helper重放。

## 初始集成的历史验证范围

- 全套主机64/64通过
- 全套ASan/UBSan 64/64通过；LeakSanitizer关闭，不能声称通过泄漏检查
- 实际ARM链接、3dsxtool及141个原生RomFS资源暂存
- 当次CIA资源141/141逐字节一致；不代表安装/运行验收
- 会话准备，818项独立检查
- fresh scene与真实文件闭环，400×240及320×180合计18,140项检查；从明确构造的有效post-Dad存档出发，执行读取、新owner、真实Dad-normal、Record覆盖/备份及第二次恢复，不声称初始fixture通过完整游戏流程获得
- UID/RNG，官方Godot3.6.2有限方法oracle及不改动的官方RandomPCG源公式对照；不是完整原版场景启动证明

本次没有GUI截图、标题/Load/Play实际输入、模拟器完整可玩闭环、SD卡、实际音频、真机或性能验收。历史Doll约15FPS和早期Lamp约60FPS不能当作当前Continue构建的测量结果。继续开发应保留这些明确边界，并按原版流程扩展尚未移植的内容。
