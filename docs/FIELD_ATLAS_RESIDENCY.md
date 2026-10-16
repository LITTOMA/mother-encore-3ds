# 房屋出口的图集驻留修复

用户截图在领取狗粮后出门时停在 `Field atlas missing or invalid: romfs:/graphics/world/podunk/atlas-00.t3x`。这是图形资源准备失败；该提示本身不能区分文件缺失、解码失败或线性内存不足，也不能证明狗粮的领取旗标有误。

## 改动

房屋到 Podunk 的交接原先取消待准备和驻留的遭遇资源，并释放房屋图集，但仍保留最近实际使用的 `BattleRenderer` 和 `RoundRenderer`。室外不绘制这些战斗资源，却需要加载一页 1024×1024 RGBA8 图集（约 4 MiB 连续 LINEAR）。本次在等待 GPU 完成、释放房屋图形后，先释放回合媒体，并将战斗绘制器恢复为既有的对话字体模式，再准备室外图形。回屋后仍通过原有遭遇准备流程按需加载战斗资源。

加载失败继续拒绝换场并走原有房屋恢复路径；图集加载器现在保留底层文件、解码、分配等具体错误。增加三处有界纹理所有权日志，便于确认清理前后可用内存。未改变上游、游戏内容、资源、图集格式、剧情规则、RNG 或存档格式。

这修正了代码中可确认的资源驻留问题。旧构建在本次 Azahar 开场路线中也通过，因此没有把用户旧 Citra 环境中的失败声称为已完成自动化复现，更没有声称唯一原因已经得到实验证实。

## 真实模拟器验证

使用用户改版 Azahar 的 CDB、Vulkan、只读语义探针和普通按键。固定上游为 `7d9246600fffe518408f5830d4848635019005a3`，模拟器运行的真实 3DSX 来自 `ea7354a617f7ee3679f35e47c1fd4a84ca5fc5cc`。后续测试脚本和文档提交不改变该构建的运行时、平台代码或资源。详细凭据见 [证据目录](../reports/field-atlas-residency/emulator-summary.json)。

- [狗粮路线](../tests/emulator/dog-treats-field-exit.json)：简体中文，两次独立冷启动，每次 34 个检查点。明确使用人工检查点预置地下室钥匙与 `talked_to_dad`，不声称这些历史已经自然游玩。随后正常 LOAD、开门、领取地图、离开并重进地下室、实际领取狗粮、出门、行走、开关 START 菜单和回屋。关键物品数 4、礼物盒位图 12、地图物件旗标数 1 在出门和回屋后保持；两次投影一致。
- [开场路线](../tests/emulator/opening-field-exit.json)：简体中文，两次独立冷启动，每次 89 个检查点，从新游戏经正常命名和开场跳过，实际完成 Lamp、Doll、玩偶旋律、Carol 和首次 Dad 电话，再走房屋 → Podunk → 房屋 → Podunk。没有检查点或物品、旗标、内存注入。两次关键语义投影一致，且与旧构建同一路线的投影相同；这不表示两者的全部状态、RNG、HP 或帧时间相同。两条路线全部检查点的房屋、室外、菜单和音频错误字段均为空。

狗粮路线两次实际日志均显示，交接清理前可用 LINEAR 为 4,565,888 字节，清理后为 26,772,480，室外图形准备后为 19,366,400。这个增量包括房屋和预热资源的原有释放，不能全部归功于本次新增的战斗绘制器清理。

Godot 3.6.2 实际运行了固定上游的 Present 方法参考，狗粮检查点路线比较其中四项地图领取 / 重复 / 重建 / 旗标命名空间结果。这里采用同一套 Present 机制，未把参考适配器当作完整原作通关。开场路线复用固定来源的 Pause / PSI 方法参考；实际战斗与出入场景结果单独记录。音频模式检查实际 DSP 消费，未验收主观音质。

测试导航曾在进入室外前因窄坐标窗口及标题字体准备期间的过早确认而失败，原始日志与原因另存 [历史尝试](../reports/field-atlas-residency/attempts/summary.json)。修正的是按键路线：窗口仍在原版可行走通道内，保留剧情、菜单、物品和门的断言，没有修改游戏碰撞或原作期望。

## 构建与主机检查

真实 ARM / 3DSX / CIA 构建和 CIA RomFS 提取比较已通过，共 326 项资源。[构建收据](../reports/field-atlas-residency/cross-build.json) 固定 3DSX SHA-256 为 `182ae8886b86df075c081c8a2712d80d63de604250cf958530cbb5bd399413f2`。实际构建可在 [Actions 38904054845](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38904054845) 下载；安装包不是本地主机构造的替代文件。

本地 GCC ASan / UBSan 已实际编译全部 76 个 runtime 源文件并运行定向检查：房屋地图礼物 73 项、遭遇驻留、纹理加载的有界读取 / 损坏输入 / 失败清理。纹理检查使用主机 API 替身，不是 GPU 测试。另有只读观察器 14 个 Python 测试、Present 来源 17 个 Python 测试通过。[来源说明](../reports/field-atlas-residency/host/provenance.json) 明确区分编译时 HEAD 与已包含 Dad 检查点前提的测试源提交。

完整 GCC / Clang `make test` 与 ASan / UBSan 由 [Actions 38905188193](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38905188193) 独立运行，实际状态与原始日志哈希保留在 [CI 凭据](../reports/field-atlas-residency/ci.json)，不能用定向测试替代完整套件结果。

本次凭据快照中，Clang 作业已完成，当前 CI 配置下的 152 项主机检查和 152 项 sanitizer 检查均通过；其命令排除了由 GCC 作业负责的 Python 单元全量，不能将其称为两套编译器的完整矩阵已通过。GCC 作业仍在运行，PR 保持草稿等待结果。

## 重跑与边界

本次为比较旧菜单构建（探针 v7）与 main（v6），复用 `7a0f1156dead098724ff7cb5eba2ff6cae639ecf` 的严格双版本测试读取器；其源哈希打印在原始运行日志中。[包装脚本](../reports/field-atlas-residency/run_compatible_probe.py) 只修改临时模拟器的 Vulkan 和调试日志配置，不修改游戏内存或生产代码。PR 的运行时代码不依赖菜单 PR 合并；重新执行比较需要该测试读取器的 Git 对象可用。

先创建 `build/field-atlas`，按 `tools/emulator_save_fixture.py` 生成与构建收据匹配的人工检查点。将包装脚本复制到 `build/field-atlas/run_compatible_probe.py`，以 `--scenario tests/emulator/dog-treats-field-exit.json --save-fixture <fixture.json> --original-reference <Present参考/reference.json>` 执行；开场路线改用 `opening-field-exit.json` 和 Pause / PSI 参考，不传 `--save-fixture`。其余模拟器、真实游戏、收据、私有 DSP 和全新输出目录参数同 [模拟器语义验收](EMULATOR_SEMANTICS.md)。

每次测试创建的临时 profile 在结束后删除。已清理本次对象文件、三项主机可执行文件与核验后的下载压缩包，约 491 MiB；保留真实 3DSX、收据及日志。没有分发私有 DSP、个人存档或模拟器 profile。CIA 安装、Old / New 3DS 真机、用户旧 Citra 环境与完整游戏尚未验收。
