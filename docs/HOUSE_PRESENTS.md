# 房屋礼物盒（Present4 狗粮）

固定来源：Mother: Encore `7d9246600fffe518408f5830d4848635019005a3`，场景 `Maps/podunk/Nintens House.tscn`、实例 `Nodes/Overworld/Objects/Present.tscn`、`Scripts/Main/Present.gd` / `ItemHolder.gd` / `FlaggableObject.gd`、`Scripts/misc/sparkles.gd`。

本切片在房屋内画出全部四个原版礼物盒，并接入 Present4 的狗粮领取。它不是完整礼物盒系统，也不包含 Mick 对话或南栅栏 `mick_bark`。

## 可玩范围

- 四个礼物盒使用原版精灵、打开帧、Sparkles 与 `got_*` 旗标状态；已打开的盒子显示第 4 帧并隐藏 Sparkles。
- 房屋 Ready 时每个 Sparkles 按树顺序调用 `int(rand_range(0, 47))`，与后续提交的同一 SourceRandom 流衔接。
- 仅 Present4（`DogTreats`，`keyitem: true`，默认 `ItemDialogue/presentcheck`）可交互：先播 Unwrapped（Gift Box 音效）、将狗粮写入 `key_items`（容量永不满）、设置 `got_dog_treats`，再显示领取文本并播放 Item Received。
- 文本在编译期用固定物品降低 `[ItemName]` / `[ItemArt3]`；`[ItemReceiver]` 映射为单人队伍领袖。英文与简体中文绑定写入 locale 目录。
- 已打开的 Present4 显示原版 `ItemDialogue/presentempty`。

## 数据与消费者

`content/house-presents.json` 为来源 IR；`tools/house_presents.py` 编译 `data/opening.encpresent`（ENCPRS01）。礼物盒文本追加进既有 `.enchouse`（稳定对话 ID 70–72），不重复字体图集除非新增码位。贴图由真实 `tex3ds` 在 `make 3dsx` 时生成到 `graphics/world/presents/`。

资源目录能力 3 增加 HousePresents 角色，绑定位于 `content/extension-resource-catalog.json`，与开局房间配方分开以免改写其来源指纹。Session 资源格式 / 能力 6 增加钥匙物品获取策略：`got_dog_treats` 为真时钥匙栏在默认 CashCard 之后持有 DogTreats；格式 1–5 仍按旧钥匙栏范围加载。

Gift Box 音效经 `content/present-audio-binding.json` 并入既有 opening 音频库，不改写 `tools/phone_linker_bindings.py`。

## 明确边界

- Present1（日记过场）、Present2（塑料球棒）、Present3（地图）只绘制，交互为明确开发边界。
- 礼物盒上的 ButtonPrompt、射线打到 StaticBody2D 时的 noproblem 提示未接入。
- 狗粮尚未从钥匙栏移除，Mick 对话、漫步和南栅栏 `mick_bark` / `gave_treats` 未接入。
- 礼物盒贴图由 CI / `make 3dsx` 的 tex3ds 生成，不作为已审查二进制入库；没有 tex3ds 时不能 staging。

## 验证

新增解析器负向用例在 `tests/test_house_presents.py` 与 `tests/present_data_tests.cpp`。日常不运行主机测试；本切片的 3DSX / CIA 由手动 `build` 模式 Actions 记录。模拟器与真机未验证。
