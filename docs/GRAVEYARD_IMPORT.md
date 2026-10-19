# 墓地推进（开发中）

基于已合并 #67 的主线 `943636d5f4c427448b42aa362ebf2680320d9438`。原作固定为 `7d9246600fffe518408f5830d4848635019005a3`，上游保持只读。本阶段尚不能称为墓地或地下墓穴可玩。

首先接入真实依赖链：僵尸与树根过场、Iggy、旧球棒与野外挥击、破坏树根和拾取/归还铲子，随后推进 Catacombs 与 Pippi。旧球棒 BatOld 是关键物品，没有装备槽或攻击加成；swing 为 Ninten 的 bat 旗标技能。gravedigger_getshovel 是 Iggy 取回铲子的过场，不向玩家发新铲子。其物件动画、碰撞、音效、相机和剧情调用不能以单独设置旗标替代。

## 确认后重问

来源 `gravedigger_swing.yaml` 的16项选项按原顺序指向18（接受）、17（拒绝/取消）；17是普通手动对白，goto16。DialogueBox 每段默认 can_input=true、auto_advance=false，完成文字后需确认才执行下一段。当前 FieldStory 的选项目标本身仍向前，不需要放宽其校验；需要支持的只有17确认后的 Jump16。

Room 格式仍为1，规则仍为7，独立 capability11允许 Jump.flags=1 的确认循环。它必须倒跳到此前的 ShowDialogue 段入口，且紧邻的上一条命令必须是同段 AwaitDialogue.flags=0。未标记倒跳、自动/禁止输入/缺失确认、跨段确认、自跳、越界、向前使用循环标记、坏标记和未知能力继续拒绝；只改旧包能力而没有合法循环也拒绝。其他 Jump/Branch 的原向前段入口限制保留。原 DialoguePlayer 的32条分发上限和确认/选项停顿直接复用，不引入通用脚本解释器，也不改变存档格式。

GCC9 ASan/UBSan：dialogue_branch 516项、RoomData 71,939项通过。机制夹具反复20次拒绝/确认后重问，时间与自动回调不能代替确认，接受后正常结束；坏加载保留旧owner。两项Python格式测试通过，覆盖往返和负例。这是合成机制测试，不是完整Iggy、3DS或模拟器墓地流程。来源限定审查见 `compatibility/reviews/confirmed-dialogue-loop-v0410.json`。

生成器改变后的既有发货资源由原内容DAG重生成，语义对比与完整构建结果另记；当前没有把未完成Iggy或墓地资源放入发货包。后续必须把实际动作与内容编译到独立资源、接入消费者，并从正常走路/交互验证结果，不能以本节解析通过当作可玩。


原内容DAG已完成重生成。对比证明House/Podunk/Mayor三份Room的所有数据段、字符串、规则和能力不变，仅编译器输入SHA及头部CRC改变；两份地图、Links、Session与来源IR逐字节不变。Restore经原extract只更新House Room依赖与其重复指纹，所有NPC、音乐、库存和保存政策不变；目录仅更新这四个已核对资源的CRC，身份/绑定/遭遇完全相同。最初Restore旧指纹阻断完整DAG，失败日志保留，未手改pack或放宽验证。生成器语义对比见reports/graveyard-import。

新增cap11和未知cap12的正负路径后，相关Python原始模块29项全部通过；GCC9 ASan/UBSan另有旧八方向人物752项通过。严格Python登记、当前ARM构建及GCC/Clang完整矩阵仍须单独记录；本地结果不替代它们。没有新增可玩的墓地内容或新真机结论。
