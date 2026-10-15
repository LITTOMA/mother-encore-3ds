# 地下室地图礼物与物件状态

固定来源为 Mother: Encore Act2 v0.4.1.0，提交 `7d9246600fffe518408f5830d4848635019005a3`。本切片依赖地下室钥匙与往返通路，开放 `Objects/Present3` 的 MapPodunk 领取，不代表完整地下室剧情或地图菜单已经完成。

## 原版语义

来源为 `Maps/podunk/Nintens House.tscn`、`Scripts/Main/FlaggableObject.gd`、`ItemHolder.gd`、`Present.gd`、`Data/Items/MapPodunk.yaml` 和 presentcheck / presentempty 对白及翻译 CSV。原版场景根节点名称是 **Ninten's House**；没有显式 flag 的地图盒使用 **Ninten's House/Present3** 保存对象身份，不能用场景文件名或省略撇号的名称代替。

交互先启动 Unwrapped，再通过 key_items 领取地图（普通背包容量不限制钥匙物品），随后设置物件状态、隐藏并停止 Sparkles、打开领取对白。第二段对白使用原版 Item Received 音源。重复调查显示原版空盒对白。重建房屋时，已领取盒直接显示第 4 帧且没有 Sparkles；不会再次领取或分配 UID。

物件状态与剧情旗标分开。受检会话要求地图持有状态与对应物件状态一致；未知、重复、错误命名空间或未支持物件状态拒绝加载。地图和狗粮可以按任意顺序领取，保存保留真实领取顺序和 UID。源内容、稳定文本 ID 及追加位置在独立 `content/house-present-bindings.json` 中；已有 1–74 对白与其所有段落保持原样，新地图文本为 75 / 76。

## 资源和兼容

- ENCPRS01 保持格式 1，能力升至 2，执行已绑定的物件旗标程序；能力 1 仍只执行剧情旗标程序。未知能力、命令归属冲突、外部物件旗标引用明确拒绝。
- ENCNSESS 格式 / 能力升至 7，为钥匙获取策略增加独立的旗标命名空间。版本 1–6 保持旧读取路径；版本 6 不接受地图对象状态。
- ENCSNAP1 存档编码仍为 1，既有剧情规则仍为 7。原始默认快照、角色、等级、装备、普通库存策略和相机范围未改变；使用已存在的 object_flags 字段，不注入新默认状态。
- House 仍为格式 / 能力 / 规则 7，139 段、76 条对白。地图文字用来源物品名称和冠词降低并接入英语、简体中文目录。
- 原生成器更新 House、Session、Restore、Prompt、Naming 和资源目录。原字形集合完全相同时，通过已存在的严格复用工具更新字体目录来源；字体二进制、所有图集、命名二进制与提示图均保留。

## 验证和限制

本地验证记录位于 `build/basement-map/`。`tools/run_present_reference.py` 在官方 Godot 3.6.2 执行原版三个脚本的方法体，对照领取顺序、重复调查、JSON 状态重建与命名空间隔离。它使用明确的服务、钥匙库存和 Sparkles 适配器，固定测试 UID，手动推进真实 AnimationPlayer；不声称运行完整原版存档、声音混音或 UI。

共享核心回归覆盖领取和空盒文本、物件旗标、拒绝绑定不覆盖旧状态、拒绝领取不提交旗标、存档 UID 往返、两种获取顺序，以及损坏 / 未知资源和旧格式。测试、真实 3DS 构建、模拟器和真机结果必须分别记录；真机仍未验证。原版日记睡梦演出及学习 PSI、塑料球棒、地图查看界面尚未接入。

本次实际记录（游戏源码 `c5e1dc32d229dd79d983cdf56417b579f5331baa`）：真实 3DSX / CIA 及 323 项包内资源核对通过，Actions `38668731249`。MSVC 地图 67、礼物资源 4347、会话 19444、House 23227、钥匙门 1474 检查通过；来源 Python 21 项另加绑定负向 7 项，协议最终 14 项通过。Clang ASan / UBSan 的地图、礼物资源和会话针对性运行完成，但 Windows 拦截器报告两条 unhandled instruction 警告，不能称为完整无警告 sanitizer 通过。

模拟器 `build/emulator-regression/map-boundary-4/receipt.json` 记录两次冷启动、每次 19 检查点，结果相同，按真实 LOAD、开锁、地图领取、空盒、地下室出入行走；四项结果与原版方法对照一致。起点是明确标记的人工测试存档，预置了地下室钥匙，不证明正常取得钥匙的剧情链已通过。UID 与完整保存 / 恢复在共享核心验证；模拟器本路线没有执行电话 Record 后再次 LOAD。

检查点必须由 `tools/emulator_save_fixture.py` 调用实际编译的 `encore_house_map_present_tests` 生成，记录生成器、工具、日志、快照、资源指纹和真实游戏源码身份。测试存档只复制进带标记的临时模拟器目录，不读取或覆盖个人存档。用 `tools/run_emulator_semantics.py --save-fixture <生成的 fixture.json>` 配合 `tests/emulator/basement-map-save-boundary.json` 与原版 present reference 复现。

同源码手动完整 Linux 回归 `38669037134` 仍有 GCC 22 / 161、Clang 18 / 146 项失败；新地图、礼物解析、会话测试通过。Linux sanitizer 因前置主机回归失败而未运行。自然新游戏探针已进入并打赢 Doll，但因持续会话未接收 Lamp 奖励而仍为 1 级，该路线明确失败；不能放宽 2 级预期或改写原版奖励。另发现加载完成会无条件重置输入，持续行走需要重按；地图边界测试使用有界重按且保留全部结果断言。后续分别修复这两个实际交接问题。
