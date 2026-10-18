# Pippi 母亲求助与重复交谈

固定上游：`7d9246600fffe518408f5830d4848635019005a3`。本切片在 #64 合并后的主线继续，接入 Podunk `Objects/NPCS/npc20` 的首次求助和重复交谈；不是完整 Podunk 或完整游戏。

## 来源与执行映射

- `Maps/podunk/podunk.tscn`、`Scripts/Main/npc.gd`：默认 `Podunk/cutscenes/pippismom_help`，`pippis_mom_help` 为真后选择 `Podunk/pippismom`；后面的满足条件组覆盖前面的组。两个组都只有一个候选。角色 `lindgren` 绑定已有 npc20 动作资源，不借用 Mick 对话。
- `Scripts/Main/party/party_object.gd` 的现行 `try_to_turn` / `_turn_to` 使用相对向量；注释中的旧基数方向逻辑不代表该来源版本。交互入口使用独立的归一化转向接口，门和网格方向检查保留。
- `pippismom_help.yaml`：替换玩家和 talker 为剧情角色，惊叹表情与 1 秒等待；五段手动确认对白；dot 表情、左右转身与 0.8 / 1.5 秒等待；玩家两次高度 6、长度 0.2 的跳跃与 0.6 秒等待；最后一段开始时设置 `pippis_mom_help` 并播放 heart。
- `pippismom.yaml`：一段普通重复求助，包含两个 `[WAIT@]`。结束恢复玩家、NPC 动作与移动，不重复首次剧情或重置进度。
- 两张原版 Podunk CSV 保留英文和简体中文。`[@]`、`[WAIT@]`、`[color]`、`[/color]`、`[DELAY]` 与命名角色由受限解析器转换为共享对白框令牌。颜色、延迟量、分隔符、bullet 来自受检本地化资源；姓名替换是字面值，不再次解析玩家输入。未知令牌、嵌套颜色、跨等待的颜色和未闭合标签拒绝。

所有来源文档、SHA、角色、程序、条件、文本身份、动画和参数在 `content/podunk-story.json` 审查后由原生成器编译；C++ 只承载模式、检查、执行和加载。不是动态 YAML/GDScript VM。

## 兼容

`ENCFST01` 容器格式仍为 1，独立能力升至 2；增加第六个交互规则节，程序 flags=1 表示 NPC 交互而非自动区域。旧能力 1 的五节资源仍可读，原始旧资源保留为测试夹具。旧 bridge/call 程序索引 0/1、已有对话身份和所有道具身份保持不变；新增身份只追加。Session 资源仍为 v10，新增已审查可写旗标；游戏存档编码没有变化。命名依赖由 `reuse-session` 重新审查，命名二进制与纹理保持原样。

## 验证与限制

针对性 GCC9 ASan/UBSan 覆盖旧资源读取、未知能力/规则/演员/保留位、截断、两种语言和自定义名字、富文本 WAIT/颜色/延迟、首次五段及重复一段、旗标与角色恢复。现有桥头真实战斗、呼喊、房屋对白和 Session 回归也运行；实际日志保存在 `reports/pippi-mom-help/`。真实交叉构建与模拟器验证完成后另行记录，真机尚未验证。

目前仅接入救出 Pippi 之前的 npc20 普通交谈；其他 NPC、Telepathy 思绪、建筑和后续救援仍待移植。单候选组的 seen-dialogue 记录尚未持久化，这不影响这两组的原版选择结果，但不能据此声称一般 NPC 的候选轮换已完成。表情帧与时长沿用来源，既有 offset / 镜像差异仍见桥头文档。
