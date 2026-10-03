# 原版灯战斗入场检查点

固定源commit7d9246600fffe518408f5830d4848635019005a3。
真实lamp_attack请求现在进入独立BattleEntry：source pause/idle/deferred阶段、25帧遮罩、原版Battle.tscn入场、人物跳跃/落地震动、敌人出现、原版指令菜单。
本文件描述入场层；之后已连接真实首回合运行时，见BATTLE_ROUND.md。Items和胜利奖励/战后flag仍未实现。

## 独立数据

content/native-battle.json记录精确动画轨道、source方法参数、初始人物/敌人数据、UI布局、背景参数和来源指纹。
tools/native_battle.py编译opening.encbattle；图片/BPX/glyph图集另存，AudioBank和PCM也独立。
C++只保存schema、执行状态和校验，不包含角色名/坐标/帧表内容回退。

原版新游戏Ninten为HP62/62、PP26/26，防御12包含BaseballCap5点；save中的100经Character上限夹取。
当前菜单只显示Bash、Items、Defend；无可用武器技能/PSI、禁逃跑，无SP/Encore。
敌人Lamp初始HP30。所有这些值均来自外部数据。

## 时间与画面

官方Godot3.6.2隔离原方法/AnimationPlayer/SceneTreeTimer/Tween参考150帧，原生核心1,169检查通过。
嵌套.2+.1计时与延迟方法使第一次跳跃在参考frame30/动画位置约.483334开始，不能简单按.45触发。
菜单激活在参考frame115，入场动画末尾1.9；没有改变golden来迁就实现。

源项目使用GLES2，其blend_disabled实际上被该后端忽略；白色遮罩按130/255蓝色alpha叠加当前房间，绿不透明区显示背后的BBG，透明区保留房间。
STRETCH_TILE使背景使用504×180纹理UV；400×240画布仍按原纹理像素域取样，在畸变之后repeat。
该合成语义依据引擎源码审查，不是完整 GPU 逐像素对照。

默认400×240扩大视野，图素1:1；layout anchor由content/display-adapter.json控制。
SELECT重置切换320×180参考模式，居中黑边；不把320×180非整数拉伸到400×240。
遮罩原始像素居中，新增边缘范围按边界覆盖延展；跳跃落点按底部信息框锚点适配，仅涉及显示，不修改世界物理坐标/剧情位置/时序。

## 音频

NDSP后端和72KiB流式缓冲已接入，原版Poltergeist循环点6.382秒、bash替换同一音效声道、Encounter Enemy独立声道。
原版Lamp没有战斗主题；既有Poltergeist继续播放。
本云模拟器没有DSP组件，程序明确显示Audio unavailable；没有声称听到声音，也没有获取/打包固件。
音频bank/流/衰减/适配器模拟测试不能代替实际扬声器验收。

## 已知限制

- 软件背景在本云Azahar稳态菜单400×240约60FPS（320×180参考模式约60FPS），开局房间约60FPS；这只是该切片/环境观察，不是真机或全游戏性能结论
- 已修复背景overwrite混合状态污染后续文字/精灵的问题；下方黑条遮住HP/PP的问题用外部layout draw-order修正，ELF未改变
- 敌人入场随机抖动尚未移植；居中人物随机横移需要明确注入方向，当前固定灯剧情入口不触发该分支
- 没有完整战斗动作、目标选择子菜单、战后恢复、存档、完整原版UI/字体GPU逐像素验收
- 输入当前菜单支持D-pad；模拟器默认H/F是D-pad，方向键是Circle Pad，仍用于房间移动

当前精确CPU加速已经集成；稳态菜单的全分辨率帧预算已在模拟器达到60FPS，后续仍需过渡与硬件验收，保留冻结旧CPU采样作为对照，再实现真实战斗动作。
PICA/TEV或UV网格需要单独验证采样误差，不能为了帧率偷偷替换成静态背景或模糊缩放。
