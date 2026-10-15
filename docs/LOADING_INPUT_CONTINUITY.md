# 后台读取后的持续输入

模拟器在同一 House 位置重现：持续按 DOWN 下地下室，玩家停在约 y997；CDB 仍报告 DOWN 按住，无游戏错误。共享核心同一碰撞路径可以到 y1057，松键再按也能继续，因此没有移除任何碰撞或放宽地图领取断言。

平台主循环在 loading_generation 改变时无条件调用 NativeInputAdapter::reset。音频或遭遇资源的后台准备也会改变此计数，于是下一次 HID 采样把仍按住的方向键 / 摇杆隔离到松手。这会让正常行走被资源读取意外打断。

新增共享核心 resume_after_loading：只有已配置、非零且与先前一致的输入控制方，才保留已接受的方向键 / 摇杆及原有隔离状态；控制方改变、禁用或未初始化时仍完全重置。读取中的触摸手势取消并等待抬手，抬手不产生确认；新的短触摸仍可确认。相同控制方的菜单重复计时器也保留，避免因重新初始化多移动一次。主循环仍丢弃 IO 耗时的物理补帧债务，不推进剧情、随机流或游戏时钟来代替读取。

这是 3DS 输入适配修复，不改变原版内容、数值、资产、包 / 规则 / 存档格式。实际检查见 build/input-loading-continuity；模拟器分别用连续按住方向的地下室路线及自然开局路线验收。触摸、暂停 / 菜单跨界的负向用例保留；真机仍未验证。依赖奖励交接修复 #52，本 PR 单独审查持续移动问题。

实际游戏源码 `44f1c0a2214697ecca36cc3d2fef0d4c684d7b9e` 的真实 ARM ELF、3DSX、CIA 和 323 项包内资源核对通过（Actions `38674235543`）。3DSX SHA-256 为 `907c33dd552cd69580e17d72b0e03e4af05c5a248bde65e2409689c182ca63ef`；CIA 为 `ec1c13a5164fdfac3e0bb97194cf5b1ca804557c2c24ea100acaf8a91bcde61c`。MSVC 输入 4637 检查、完整开局 46309 检查通过；同两项 Windows Clang ASan / UBSan 运行完成，但有两条拦截器 unhandled instruction 警告，不构成完整无警告 sanitizer 验收。

`tests/emulator/basement-map-continuous.json` 的两次独立冷启动已通过，每次 19 检查点，结果投影一致；持续按 DOWN 穿过后台准备，不需要松键重按。正常 LOAD、开锁、领取地图、空盒及地下室往返均通过，四项语义与固定上游的实际 Godot 方法执行对照。起点仍是明确预置钥匙的人工受检存档，不冒充自然取得钥匙。记录为 `build/emulator-regression/map-continuous-current-1/receipt.json`；临时模拟器目录已自动清除，保留日志、截图及来源收据。

自然开局 `tests/emulator/opening-reward-handoff.json` 另以两次冷启动、每次 56 检查点通过，持续按 LEFT 到 Doll 房间入口，不用重按掩盖资源读取后的停步；正常完成 Lamp / Doll 后到 11 经验、2 级并在 PSI 菜单选择心灵感应。结果及验证边界见 [奖励交接](OPENING_REWARD_HANDOFF.md)。没有个人存档或内存写入；要求 DSP 输出可用并检查音乐播放状态，不等于声音听感验收。

同游戏源码手动完整 CI `38674475729`：真实交叉构建与打包通过，Clang 完整主机回归 16 / 146 失败，Linux sanitizer 因前置失败未执行；记录时 GCC 作业仍在运行，不能宣称其通过。失败分布于 audio_data、audio_player_double / regression、dialogue_choices、Dad / House Record、region_background、pillow_sequence、session_migration、new_game_setup、settings_text_speed、localized_playable_slice、resource_catalog_sources、field_data、storage_source 和 item_details_source。日志保留在 `build/input-loading-continuity/clang-full-failed.log`，需要逐项修复而非放宽断言。后续文档 / 场景提交不改变已验收的游戏代码与资源；真机未验证。

本轮清理旧构建的 3DSX / CIA / SMDH 和两份失败原版测试工程，实际释放 489,911,986 字节；先验证完整路径、非重解析点、旧构建收据和文件指纹，保留日志、收据、当前构建与工具。记录为 `build/input-loading-continuity/cleanup.json`。两条最新路线的四份临时模拟器配置也已自动删除。
