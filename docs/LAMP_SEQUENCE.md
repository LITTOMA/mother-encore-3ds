# lamp_attack原版演出接入

本阶段只处理固定Act2v0.4.1.0的实际
`Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml`，共13个phrase。
它是DialogueBox执行的YAML，不是`.ecs`文件。没有开发通用GDScript翻译器，也没有把M0八操作码沙盒VM当成原版剧情系统。

## 数据与运行时

`tools/lamp_dialogue.py`通过原版YAML解析器导出审查过的输入，生成外部JSON，再由native_content.py编译为encroom中的类型化命令表。
`DialoguePlayer`负责原版命令顺序、Actor.ready信号、idle_frame让出、WaitTimer和战斗排队。
`OpeningWorld`绑定真实Ninten/lamp、相机和场景对象；`ActorActionState`分别推进物理动作、动画与SceneTreeTimer。
未知命令、目标、输入变体或源码指纹变化会拒绝。现有全量兼容门禁仍不批准整个游戏。

当前实现的演出动作包括灯打开/震动/冲撞，Ninten依次转身、惊讶表情与跳动，镜头切换/移动/震动，
以及最后同时起跳移动、登记lamp战斗并在剧情结束后发出战斗请求。
声音命令保留为类型化请求，尚没有实际音频后端；不得把它称作原版有声演出。

## 不能改成“看起来合理”的原版细节

- Area2D新接触在下一物理tick开头发出，Player仍移动该tick，然后idle中的CutsceneArea暂停。
  普通向左走触发演出时为(430,397)，不是把静态重叠阈值(431,397)直接当触发坐标。
  奔跑/斜向仍按相同物理回调顺序处理，不硬编码停止位置。
- YAML的键顺序不决定执行顺序；等待、延迟对象调用、音频、动作和镜头遵循DialogueBox源码顺序。
- `movecam.time`实际上未被读取；原版读取`length`，本脚本恰好使用默认1秒。
- `turn_to`使用原版45弧度旋转后round及0.08秒计时步骤，不能擅自改成45度或瞬间转向。
- 灯与Ninten被替换为无碰撞Actor；移动、jump、shake、WaitTimer并行推进。
- `startbattle`只排队。参考中phrase12在相对帧363登记，帧384经过清理/done后才请求battle。
- `update_npcs`恢复Ninten坐标/方向/相机后仍让出到下一idle；当前终点不能伪称Actor销毁已完成。
- `Room Shaker.delayed_start()`默认5秒，首次实际震动在本次战斗请求终点之后；未擅自立即播放。

## 阶段终点

原来的lamp触发区开发阻断已被此演出替代。真实战斗请求为enemy=lamp、actor=lamp、
advantage=0、can_run=false、winflag=poltergeist、overworldBattleMusic=true。
`poltergeist`只应在将来真正胜利后设置，本阶段不会提前置位或伪造胜利。

进入`BattleRequested`后保留最后画面，下屏明确说明战斗/音频后端待实现。
这不是已实现战斗画面、转场或战斗系统；也不是可任意继续穿过剧情边界的完整开局。

## 运行阶段和限制

平台60Hz物理调用`advance`，每渲染帧另调用一次`idle_frame`。
idle信号→Actor动画节点→Dialogue WaitTimer→deferred调用→SceneTreeTimer依次处理。
平台最多100ms的idle步，睡眠/大积压丢弃；更大步长可能造成未审查的并发shake，显式拒绝。
固定步长与较大idle步长不应被宣称像素级相同。

原生参考分别验证命令派发、动作曲线/队列/计时、连续入口与相机。它们不是完整原版游戏宿主。
本阶段没有音频混音、真实战斗、完整AnimationTree、任意NPC/剧情、退出碰撞滞后或战后世界状态的批准。

主要源码：`runtime/dialogue.cpp`、`actor_actions.cpp`、`world.cpp`、`opening_trigger.cpp`。
动作参考：`reports/actor-actions-reference-9/`。
命令参考见 `reports/m5-lamp-dialogue-reference/`。
