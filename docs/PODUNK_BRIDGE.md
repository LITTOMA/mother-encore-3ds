# Podunk 桥头推进

固定上游 `7d9246600fffe518408f5830d4848635019005a3`。本阶段将正常行走触发的桥头流浪狗战斗和随后 Pippi 妈妈的呼喊接入共享核心；没有直接授予胜利旗标，也没有复用 Lamp 的敌人数值。

## 来源到实现

| 来源 | 独立内容 | 执行与检查 |
|---|---|---|
| bridge_fight.yaml、pippismom_call.yaml、podunk.tscn | podunk-story.json → Field Room 与 ENCSTY01 | FieldStory 的绑定和纯预检、既有类型化剧情 VM、演员接管 / 释放、五段手动推进对白 |
| straydog.yaml、bite / bash / attack_bat.yaml、straydog.bbg | bridge-entry / bridge-round → ENCBTL01 / ENCRND01 v6 | 实际 84 HP、13 攻、20 防、5 速；bite:bash=2:1；原背景、Bite 回调、塑料球棒准备与挥击动画 / Swing SE |
| BattleItemPool.gd、BattleSystem.gd、FleaBag.yaml | Loot / PreHit / ActionMedia 三个受检 v6 section | 原候选选择和 <=40 概率，胜利后入包，8 EXP、20 cash、beat_dog_bridge；来源不会注册 straydog_fought，不伪造该旗标 |
| DialogueBox.gd、audioManager.gd、BattleSystem.gd | 独立声音绑定、返回音乐 3 秒 QuartOut 参数 | 野外 NDSP 区域通道暂停 / 保留队列，战斗使用独立音乐，返回按原曲位置渐入 |
| source defaults / 收集与仓库策略 | Session v9，快照载荷格式不变 | 增加受限野外遭遇 / 掉落 / 旗标身份；命名默认值、初始物品与命名二进制不变 |

所有新增实际内容、绑定、时长、规则值保存在独立资源。资源由原生成器重新提取、编译和核验；命名依赖更新核对默认数据及命名包字节一致；字体复用核对原生成器、字体许可、字形需求、像素页、字形度量和实际二进制。未修改 upstream。

## 实测层次

- 本地 Python 来源 / 编码 / ABI / 命名与地下室回归初轮 40 项通过；后续检查保留实际日志。
- GCC 9 ASan / UBSan：FieldStory 4,992、BridgeRound 3,457、NativeSession 20,230 检查通过；区域音乐核心、生产 NDSP 适配器替身、目录、人物、地图与碰撞测试通过。详见 reports/bridge-battle/host-* 和 host-source.json；不是硬件或实际扬声器验证。
- 原版 Godot 3.6.2 的未修改 PreHitEffect.gd、BattleItemPool.gd 与 emotes.tscn 分别执行。Bite 手动 60 Hz 的原生 deferred 回调在 tick 16；256 个受控池 seed 有 97 个选中。物品 UID oracle 使用一次抽样的受控替身，不能称为原场景全部 RNG 等价。已结束但仍可见的感叹号能被生气表情替换，修正移植核心对此误拒绝的问题。
- 真实 3DSX/CIA、包内核验与新链路模拟器回归等待本次提交实际执行后记录。真机未验证。

## 明确边界

全背包掉落地面分支未映射，预检在消耗随机数和改进度前明确拒绝。FleaBag 可以领取和携带，战斗使用未开放。日常 NPC 对话、其他建筑入口、随机敌群、多角色、状态与完整败北演出未完成；败北保留明确诊断及 B 返回标题。野外存档仍经受检 House 入口恢复，不支持任意地图位置快照。

新增剧情保留原时长、文案、帧和动作；表情偏移 / 镜像等视觉细节尚有差异。NPC 的局部运动 RNG 不能代表原场景共享 RNG。自动化接受语义结果，不要求像素、存档 UID 或原版字节一致。
