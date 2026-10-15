# Mick（院子 npc21）

固定来源：Mother: Encore `7d9246600fffe518408f5830d4848635019005a3`，场景 `Maps/podunk/podunk.tscn` 实例 `Objects/NPCS/npc21`、`Nodes/Reusables/npc.tscn`、`Scripts/Main/npc.gd`、`Scripts/Main/actor.gd`、`Data/Animations/4dir.yaml`、`Graphics/Character Sprites/Npcs/4dir/mick.png`，以及 `Data/Dialogue/Podunk/` 下的 `woof.yaml`、`woof_secret.yaml`、`woof_deal.yaml`、`woof_treats.yaml`、`woof_animals.yaml`、`woof_food.yaml`、`woof_key.yaml` 和 `cutscenes/mick_bark.yaml`。

本切片让院子里的 Mick 按已审查的漫步、凝视和口语句走路并对话，并用南栅栏 `mick_bark` 在给粮前把玩家往北推回。它不是完整 NPC 系统，也不生成其它 NPC 或野外敌人。

## 可玩范围

- 出生点 `(-88, 8)`，初始朝南。速度 64，`walk_frequency` 1，在半径 44 的圆内漫步。脚底矩形挡住玩家，精灵与玩家按当前 `position.y` 做 Y 排序。
- 玩家进入半径 44 的视野圆时 Mick 转向玩家并停止走路；靠近脚边矩形时也不再走开。离开视野后不恢复初始朝向。
- 面向 Mick 的射线（长度 16）或玩家碰到交互区时按 A。按最后一条为真的旗标选择节目：无旗标 `woof`（含二选一，取消等于第二项）、`mick_scratch` → `woof_secret`、`mick_telepathy` → `woof_deal`、`got_dog_treats` → `woof_treats`、`gave_treats` → `woof_animals`。
- 走到 Mick 的交互区并让事件射线碰到他时，在他头顶画房屋里同一张静态 ButtonPrompt（描边 A 和箭头）。位置用已审查 `npc.tscn` 的共享偏移，类别跟随标题画面的 Both / Objects / NPCs / None。暂停、下蹲或背对时不画。Show / Float / Hide / Press 动画仍未接入。
- `woof_treats` 在进入第三句时先设置 `gave_treats` 再移除钥匙物品 `DogTreats`，然后显示该句并等待确认。设置后立刻重算野外条件。
- 南栅栏 `Cutscenes/Cutscene Area11`（`disappear_flag=gave_treats`）在给粮前不再是硬停。走进该区域播放 `mick_bark`：Mick 跳两下、转向玩家、两句文本，并把玩家沿北推 16 像素。推完后若仍在区域内，须先离开再进入才会再次触发。`gave_treats` 之后该边界失效，可以直接离开院子。
- 台词用房屋里同一套原版对话框画在上屏：姓名条、开合、逐字打印和继续光标。姓名来自短语的 `name` 译文；没有 `name` 时姓名条合上。同一说话人的下一句接在后面，换人或无名则清掉旧字。二选一等字打完后出现在框内的原版选项格上，方向键、摇杆或下屏触摸摇杆左右移动箭头，使用受检的菜单连移间隔；上下输入不切换二选一。选项出现时仍推住的摇杆需先回中，避免把行走或打字阶段的输入带入菜单。A 确认，B 选第二项。`[PartyLead]` 与 `[Ninten]` 在运行时替换为当前昵称。`[ui_accept]`、`[ui_toggle]`、`[ui_select]` 按已接入的 3DS 键位换成对话框字体里的 `A`、`B`、`+`（确认、奔跑、START 菜单；`+` 是原版任天堂键位里 Start 的字形）。英文与简体中文都写入 `ENCMIK01`；进场绑定完成后使用标题画面当前语言，初始化不会把已选语言清回英文。下屏只留开发状态，不放对白。
- 进场随机数是字段本地账本：草丛、鸟、蝴蝶、闪光和其它会漫步的 NPC 在首次与 400×240 视口重叠时按实例化顺序消耗抽取，使 Mick 自己的漫步等待与原版同一条流对齐。敌人生成器的 `randi` 只被消耗，不生成敌人。

## 数据与消费者

`content/mick-treats.json` 为来源 IR；`tools/mick_treats.py` 编译 `world/podunk/podunk.encmick`（ENCMIK01，能力 3，演员记录 200 字节，九个段）。贴图由真实 `tex3ds` 生成到 `graphics/world/mick/mick.t3x`。

英文及简体中文长段文字保留来源 `[WAIT@]` 确认停顿，不再降为普通换行。每处停顿显示继续光标，重新按 A 才继续；此前输入的打字加速在停顿处重置，长按不会连续跳过。选项及整句后的剧情命令仍等待内部所有停顿与最后一段完成。来源、兼容性及实际验证见 [对话停顿修复](MICK_DIALOGUE_PACING.md)。

资源目录能力 4 的 MickTreats 角色仍指向该包。`gave_treats` 与 `mick_scratch` 进入会话 `mutable_flags`。增加 mick_telepathy 可变旗标及 KeyBasement 获取策略；DogTreats 仍绑定 `consumed_flag_id=gave_treats`。

## 明确边界

- PSI 心灵感应已接入 woof_food / woof_key、获得钥匙音效、KeyBasement 真实 UID 与 mick_telepathy；节目和指令能力升为 2，格式 / 原九段布局不变。详见 [野外 PSI](FIELD_PSI.md)。原版读心 focus 效果与其它 NPC 思绪仍未接入。`talkeremote: heart` 被识别但不显示。ButtonPrompt 只画静态可见姿态，不播放原版出现、浮动、隐藏和按下动画。
- 汪汪与呜咽使用既有受检音频 bank；可听性仍待设备验收。对话框沿用房屋 `DialogueBox` 的九宫格、字体和选项箭头；野外保留这套贴图。`showbox: false` 的推回句会先关上对话框。
- 漫步探路用脚底盒的 `StaticMotionSolver`，不是 `RayCast2D`。鸟离开屏幕后的再次 `_ready` 重抽不在首次重叠账本里。其它 `wander=true` 的 NPC 只贡献进场 `rand_range`，自己不走路、不说话。
- 其余 Podunk NPC、音乐区和野外敌人仍是下一步。Mick 贴图由 CI / `make 3dsx` 的 tex3ds 生成，不作为已审查二进制入库。

## 验证

对话框、语言保留和 Mick 的静态 ButtonPrompt 这轮以 `clang++ -fsyntax-only` 为准。未运行 `make test`，未做 3DSX / CIA。模拟器与真机未验证。
