# 开局野外 PSI 与 Mick 心灵感应

固定来源：Mother: Encore Act2 v0.4.1.0，提交 `7d9246600fffe518408f5830d4848635019005a3`。审查 PSIMenuUI / PSISelect / PSISkillLine、Pausemenu、PartyMember 的技能学习与过滤、Player 的 Telepathy 目标分派、npc 的最后匹配思绪行、DialogueBox 的 item / soundeffect / setflags，以及原版技能、对白和英中翻译。上游只读。

START → PSI → A 使用当前已学会的野外技能；B 返回 Pause。当前会话只支持单人 Ninten 的 1 / 2 级，原版 2 级学会心灵感应，消耗 0 PP。1 级列表为空，没有伪造技能。战斗技能不进入该列表，未知等级 / 技能身份拒绝；PP 和学习状态在打开、选中和实际执行时检查。方向键和摇杆沿用已有菜单输入与接管隔离。Podunk 还可按原作提示按住 B 后按 A 使用同一读心入口，每次 HID 采样最多执行一次。

PSI 列表、标题、肖像、说明、分隔线与光标使用原版字体、已有受检贴图和来源矩形。开合保留原版动画键；使用时静默关闭窗口，动画完成后只发出一次野外动作。空列表确认与取消不改技能、PP、背包或随机流。PSI 复用菜单已准备的素材，不引入二次贴图读取。

读心使用来源 EventDetector 的固定位置偏移 (0,6)、当前朝向和 16px 射线，不能仅凭邻近读取 Mick。命中他的交互区 / 脚底体后，按原作转玩家朝向，不额外强制 Mick 转身。思绪依次是默认 woof_food、mick_scratch → woof_key、mick_telepathy → woof_food，最后为真的行生效。未命中受支持目标且射线为空时执行原版 noproblem。其它未移植演员和更近的未移植碰撞明确拒绝；静态碰撞检测复用已受检的野外形状，不宣称覆盖任意 Godot 射线排序。

woof_key 保留四句、姓名条切换、原版 Item Received 音效与等待确认。物品先经现有来源 UID 分配器加入钥匙库存，再开始获得物品那句；最后一句开始时设置 mick_telepathy。Session 的第二条获取策略将 KeyBasement 绑定该完成旗标，保存保留真实 UID / 剂量，LOAD 接受已完成进度；对话中途不能 Record。已经完成后再次读心回到 woof_food，不重复发钥匙。地下室门的钥匙使用、地下室地图和后续剧情仍未接入，本改动不伪造它们已实现。

ENCFIE01 保持格式 v2、规则 1，能力升为 3，新增 PSI 策略 / 文本 / 布局 / 动画；旧能力 1 / 2 与未知能力拒绝。ENCMIK01 保持格式 v1 / 原九段布局，能力升为 2，新增 Thought / NoProblem 节目和带检查的 GrantKeyItem 指令，旧能力 1 与未知指令拒绝。Session 资源保持 v6，存档 schema 与规则兼容身份不变。所有实际内容和调参由原生成器编译到二进制，C++ 只执行已检查的结构。

PSI 角色条底饰、其它队员 / 后续 PSI / 目标治疗、房屋 NPC 读心、原版 Telepathy focus shader、dot / heart 表情仍未实现。未补充本地环境字体或第三方二进制；原版贴图 / 音频保留上游许可和来源检查。Equip 的原 tex3ds 贴图复用要求原有全部来源 SHA、转换配方、转换函数 AST 及实际输出尺寸 / SHA / CRC 相同；额外的 PSI 逻辑来源必须经过完整来源 IR 重建和审查。

本轮已完成资源生成 / 格式准入与 MSVC 共享核心语法检查。新增手动正常及负向用例覆盖技能未学会、未知技能、错误 PP / 能力 / 指令 / 节目、空射线、读心暂停归属、领取失败、不重复授予与保存 UID。按仓库约定未运行测试套件或 sanitizers。真实 3DSX / CIA 构建以候选提交 Actions 结果为准；模拟器和 Old / New 3DS 的实际显示、操作、音效和存档仍未验收。
