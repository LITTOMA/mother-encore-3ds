# Mick 狗粮（gave_treats）

固定来源：Mother: Encore `7d9246600fffe518408f5830d4848635019005a3`，场景 `Maps/podunk/podunk.tscn` 实例 `Objects/NPCS/npc21`、`Nodes/Reusables/npc.tscn`、`Data/Dialogue/Podunk/woof_treats.yaml`、`Data/Animations/4dir.yaml`、`Graphics/Character Sprites/Npcs/4dir/mick.png`。

本切片在 Podunk 院子画出静止的 Mick（南向 Idle 第 2 帧），并在持有狗粮且尚未 `gave_treats` 时用 A 键射线交互跑通 `woof_treats`：移除 `DogTreats`、设置 `gave_treats`，使南栅栏 `Cutscene Area11` 条件失效从而可以离开院子。它不是完整 Mick / NPC 系统。

## 可玩范围

- Mick 固定在原版 spawn `(-88, 8)`；漫步与 `walk_frequency` 未接入。
- 仅当故事旗 `got_dog_treats` 为真且 `gave_treats` 为假时，面向 Mick 的射线（长度 16，与房屋交互一致）可启动节目。
- 节目顺序对齐 `woof_treats.yaml`：短语 0 → 2 → 移除 DogTreats → 设置 `gave_treats` → 短语 3。
- 文本为编译期去掉 DialogueBox 控制标签后的纯文本，显示在下屏；`[PartyLead]` 在运行时替换为当前昵称。英文与简体中文写入 `ENCMIK01`。
- 设置 `gave_treats` 后立刻重算野外条件：`Area11`（`disappear_flag=gave_treats`）不再作为阻挡边界。

## 数据与消费者

`content/mick-treats.json` 为来源 IR；`tools/mick_treats.py` 编译 `data/podunk.encmick`（ENCMIK01）。贴图由真实 `tex3ds` 生成到 `graphics/world/mick/mick.t3x`。

资源目录能力 4 增加 MickTreats 角色，绑定位于 `content/extension-resource-catalog.json`。Session 钥匙获取策略为 DogTreats 绑定 `consumed_flag_id=gave_treats`，且 `gave_treats` 进入 `mutable_flags`。

## 明确边界

- `woof` / `woof_secret` / `woof_deal` / `woof_animals` 与心灵感应树未接入。
- `talkeremote: heart`、ButtonPrompt、NPC 碰撞体未接入。
- 南栅栏 `mick_bark` 过场（原版往北推回 16 像素）未接入；给粮后门洞直接可过。
- 野外未恢复房屋 DialogueBox 图集；下屏纯文本为登记差异。
- Mick 贴图由 CI / `make 3dsx` 的 tex3ds 生成，不作为已审查二进制入库。

## 验证

新增解析器负向用例在 `tests/test_mick_treats.py` 与 `tests/mick_treats_tests.cpp`。日常不运行主机测试；本切片的 3DSX / CIA 由手动 `build` 模式 Actions 记录。模拟器与真机未验证。
