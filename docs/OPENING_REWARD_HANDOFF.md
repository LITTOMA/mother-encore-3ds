# 开局胜利奖励交接

自然新游戏模拟器探针在游戏源码 c5e1dc3 上重现：正常打赢 Lamp 和 Doll、完成 Doll 战后剧情后，持续会话仍为 1 级。共享核心奖励模块本身已计算原版数值，但 3DS 主循环只在 PostWinRequested 分支复制其结果；没有后续过场的 Lamp 直接进入 Complete，丢失了持续会话中的 3 经验及 5 元银行收入。随后 Doll 从旧的 0 经验开始，只增加 8，无法达到来源的 9 经验升级门槛。

固定上游仍为 7d9246600fffe518408f5830d4848635019005a3。原版 BattleSystem 胜利流程以及已审查 native-round / doll-round 资源没有改变。本修复让共享核心在 Complete 与 PostWinRequested 两种完整提交边界提供 committed_state，平台立即复制；未初始化、未确认经验和尚在返回演出时不提供可提交状态。不增加奖励，也不改学习技能或升级阈值。

期望正常结果是 Lamp 后 3 经验 / 银行 5，Doll 后 11 经验 / 银行 15、2 级且学会心灵感应。只读观测 ABI 5 增加实际持续会话经验，拒绝旧或未知协议，避免仅凭胜利画面或下屏诊断判断升级已成功。

原有 house_runtime / playable_opening 测试曾从 Lamp 目录寻找已搬迁的 Doll 包，实际失败为无法打开受检战斗资源。本修复仅改为当前 battle/doll 目录并保留原断言。playable_opening 经同一 committed_state 边界携带奖励，继续执行原版 Doll 战斗 / 升级、Melody、Carol、Dad Record 实际文件以及 Continue LOAD。此主机用例包含明确的 NPC / 电话定位捷径，不冒充模拟器自然游玩。

实际验证日志在 build/opening-reward-handoff。模拟器用 tests/emulator/opening-reward-handoff.json，从正常标题、新游戏、两场战斗走到 11 经验 / 2 级及已学 PSI；禁止改写经验预期来消除失败。真实构建与模拟器结果需要单独记录，真机仍未验证。此修复依赖 #51；完整项目回归还有已记录的其它失败，仍保持草稿。
