# Mick（院子 npc21）

固定来源：Mother: Encore `7d9246600fffe518408f5830d4848635019005a3`，场景 `Maps/podunk/podunk.tscn` 实例 `Objects/NPCS/npc21`、`Nodes/Reusables/npc.tscn`、`Scripts/Main/npc.gd`、`Scripts/Main/actor.gd`、`Data/Animations/4dir.yaml`、`Graphics/Character Sprites/Npcs/4dir/mick.png`，以及 `Data/Dialogue/Podunk/` 下的 `woof.yaml`、`woof_secret.yaml`、`woof_deal.yaml`、`woof_treats.yaml`、`woof_animals.yaml` 和 `cutscenes/mick_bark.yaml`。

本切片让院子里的 Mick 按已审查的漫步、凝视和口语句走路并对话，并用南栅栏 `mick_bark` 在给粮前把玩家往北推回。它不是完整 NPC 系统，也不生成其它 NPC 或野外敌人。

## 可玩范围

- 出生点 `(-88, 8)`，初始朝南。速度 64，`walk_frequency` 1，在半径 44 的圆内漫步。脚底矩形挡住玩家，精灵与玩家按当前 `position.y` 做 Y 排序。
- 玩家进入半径 44 的视野圆时 Mick 转向玩家并停止走路；靠近脚边矩形时也不再走开。离开视野后不恢复初始朝向。
- 面向 Mick 的射线（长度 16）或玩家碰到交互区时按 A。按最后一条为真的旗标选择节目：无旗标 `woof`（含二选一，取消等于第二项）、`mick_scratch` → `woof_secret`、`mick_telepathy` → `woof_deal`、`got_dog_treats` → `woof_treats`、`gave_treats` → `woof_animals`。
- `woof_treats` 在进入第三句时先设置 `gave_treats` 再移除钥匙物品 `DogTreats`，然后显示该句并等待确认。设置后立刻重算野外条件。
- 南栅栏 `Cutscenes/Cutscene Area11`（`disappear_flag=gave_treats`）在给粮前不再是硬停。走进该区域播放 `mick_bark`：Mick 跳两下、转向玩家、两句文本，并把玩家沿北推 16 像素。推完后若仍在区域内，须先离开再进入才会再次触发。`gave_treats` 之后该边界失效，可以直接离开院子。
- 台词用房屋里同一套原版对话框画在上屏：姓名条、开合、逐字打印和继续光标。姓名来自短语的 `name` 译文；没有 `name` 时姓名条合上。同一说话人的下一句接在后面，换人或无名则清掉旧字。二选一等字打完后出现在框内的原版选项格上，左右移动箭头，A 确认，B 选第二项。`[PartyLead]` 在运行时替换为当前昵称。英文与简体中文写入 `ENCMIK01`。下屏只留开发状态，不放对白。
- 进场随机数是字段本地账本：草丛、鸟、蝴蝶、闪光和其它会漫步的 NPC 在首次与 400×240 视口重叠时按实例化顺序消耗抽取，使 Mick 自己的漫步等待与原版同一条流对齐。敌人生成器的 `randi` 只被消耗，不生成敌人。

## 数据与消费者

`content/mick-treats.json` 为来源 IR；`tools/mick_treats.py` 编译 `data/podunk.encmick`（ENCMIK01，演员记录 200 字节，九个段）。贴图由真实 `tex3ds` 生成到 `graphics/world/mick/mick.t3x`。

资源目录能力 4 的 MickTreats 角色仍指向该包。`gave_treats` 与 `mick_scratch` 进入会话 `mutable_flags`。钥匙获取策略仍是 DogTreats 绑定 `consumed_flag_id=gave_treats`。

## 明确边界

- `woof_food` / `woof_key` 心灵感应树未接入，因此 `mick_telepathy` 不会被本切片设置。`talkeremote: heart` 被识别但不显示。ButtonPrompt 未接入。
- 汪汪与呜咽只计数，不播放。对话框沿用房屋 `DialogueBox` 的九宫格、字体和选项箭头；野外保留这套贴图。`showbox: false` 的推回句会先关上对话框。
- 漫步探路用脚底盒的 `StaticMotionSolver`，不是 `RayCast2D`。鸟离开屏幕后的再次 `_ready` 重抽不在首次重叠账本里。其它 `wander=true` 的 NPC 只贡献进场 `rand_range`，自己不走路、不说话。
- 其余 Podunk NPC、音乐区和野外敌人仍是下一步。Mick 贴图由 CI / `make 3dsx` 的 tex3ds 生成，不作为已审查二进制入库。

## 验证

对话框这轮以资源编译和 `clang++ -fsyntax-only` 为准。未运行 `make test`，未做 3DSX / CIA。模拟器与真机未验证。
