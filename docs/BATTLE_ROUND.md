> Current continuation: victory/rewards/world return are implemented for the fresh Lamp encounter; see BATTLE_VICTORY.md. The first-round evidence below remains scoped to its recorded historical build.

# 原版灯战斗首回合

当前可从Bash菜单进入原版目标选择，执行Ninten行动、Lamp按权重选择的回应，并回到下一回合菜单。伤害致死时进入明确的胜利请求边界；尚未处理奖励、经验、金钱、战后返回或world win flag。Items仍明确阻断，可按B返回。Guard机制已接入；本阶段主要交互/原生对照覆盖Bash。

## 来源与独立数据

固定上游commit7d9246600fffe518408f5830d4848635019005a3。真正的新游戏Bash绑定是attack.yaml，power10、variance5、crit bonus5；不能因菜单名字误用bash.yaml。Lamp技能tackle/float权重2/1。绑定、所有技能参数、译文、动画/特效/布局与规则位于opening.encround，14,696字节，59个受审查源文件。图片另存，C++无真实内容常量表或名字分支。

程序读取RoundView的只读缓冲区，BattleRound拥有可变HP、行动队列和阶段；BattleActionPresentation拥有动画、文字、滚动HP与效果状态。数据编译用tools/native_round.py，纹理用tools/round_assets.py。不得手改包或重置上游身份。

## 原版行为与随机数

采用官方Godot3.6.2全局PCG包装规则。global randf先做float32运算再提升成Variant double；rand_range通常消耗三个原始随机数，极少数分支只消耗一个。逻辑和演出共享同一实例，不在每回合重新播种。

零失败率/命中失败率不消耗随机数，初始Lamp的零guts也跳过SMASH判定；只有实际miss才触发躲闪。FlyingNumber消耗rand_range+randi。HitEffect.gd在实际Battle.tscn未绑定，不能错误执行其随机方法。Lamp即使只有一个可选敌人，重选目标仍消耗randi。

受控menu seed/state的原生对照证明：seed0为attack16+tackle1，HP61/14；seed2为attack15+float，HP62/15；seed59为SMASH67，Lamp0，下一敌方行动先进入win边界而不发done。普通tackle分支总计15次原始draw，float/致死分支9次。

这些比较从显式菜单随机状态开始。原版启动及之前未移植的随机演出尚未完整重现，所以没有宣称整游戏同种子轨迹一致。生产程序只初始化一次共享流。

## 异步边界

Ninten基本攻击等待apply_damage信号，不等整个动画结束；Lamp攻击等待原始tween的apply_damage。即使没有pre-hit效果，源码仍让出一帧。文字、攻击、每目标等待、行动结束等待和新回合等待分别调度。

SceneTreeTimer输入与每次减法按float32舍入，time_left严格小于0才触发。HP滚动独立于行动完成，不能为等HP或效果结束而推迟下一菜单。普通行动尾部hide_away是异步的。SMASH使用源Slowmo的真实时间曲线，不是固定时长的常量慢放。

初次菜单交接会发生在人物show tween尚未结束时；新运行时继续该外部轨道，而非把中途坐标当最终坐标。文字按原版pixel snap取整。

## 验证与限制

完整主机25/25组、ASan/UBSan25/25组通过；两处最终交接/文字修正后，相关presentation与integrated-round组分别重跑通过。LeakSanitizer在ptrace环境不可用，未声称泄漏检查通过。

六个原生回合结果与原始下一随机值通过；真实HP/文字100帧对照通过，最终presentation470项检查通过。完整原生oracle隔离了无状态/无被动技能的灯首回合；对话和 SMASH 时间域只在这一有限 oracle 范围内验证，不把局部对照称为整个引擎/GPU一致。

Old/New3DS真机、CIA安装与扬声器均未验收。原版动作/敌人败退音效尚未映射到音频bank，本环境NDSP组件缺失。原图着色与字形尚无完整Godot GPU framebuffer逐像素证明。
