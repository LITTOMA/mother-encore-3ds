# Mick 读心交付钥匙卡住

实际 3DS 端的 `MickEffects` 漏接 `grant_key_item`，执行 `woof_key` 第 4 句前落入默认的 `Mick key grant host is unsupported`，钥匙未加入、剧情无法继续。先前 #58 的模拟器验证只覆盖口语对话，没有走到读心交钥匙。

修复将这个接口设为纯虚函数，遗漏实现会在构建时失败。3DS 端复用 Present 已有的钥匙发放事务：按受检 Session 策略验证物品与剂量、拒绝重复持有，在共享随机流和 UID 账本上分配真实编号，再加入当前钥匙背包。物品名和剂量仍来自独立二进制资源。没有修改原剧情、资源、来源指纹或存档格式。

固定上游 `7d9246600fffe518408f5830d4848635019005a3` 的 `Data/Dialogue/Podunk/woof_key.yaml` 第 4 句要求先发 KeyBasement、播放 Item Received 再显示领取文字，第 6 句设置 mick_telepathy。`Scripts/global/Inventory.gd` 的 `add_item_available` 将 keyitem 放入全局钥匙背包；不占角色普通背包。继续复用已审查的 Present 路径，没有添加替代剧情。

该修复基于 #58 的对话停顿修复，包含 #57 的碰撞优化；#58 已合并到 main（`b64594d2af3a75999246a115a6590e49874bc288`），#59 直接面向 main。

## 可重复验证

`tools/emulator_mick_fixture.cpp` 在共享核心中从 Session 的真实等级行构造 2 级房屋出口起点，校验并编码/解码往返后输出。只设置通话完成以便出门，不预发地下室钥匙，也不预设 Mick 剧情。它是人工前置条件，不是自然游玩开局的证据。

```powershell
cmake --build build/host --target encore_emulator_mick_fixture
build/host/encore_emulator_mick_fixture.exe build/mick-key-grant/no-key.encsave
python -X utf8 tools/run_mick_key_semantics.py --package dist/<verified-package> --fixture build/mick-key-grant/no-key.encsave --emulator <CDB-Azahar.exe> --out build/emulator-regression/mick-key-en-1 --locale en
```

同一受检真实构建再以 `--locale zh`、新的输出目录冷启动。工具正常 LOAD、出门、与 Mick 交谈并选择挠痒、读心、确认全部领取对白、再次读心。断言钥匙数从 1 变 2、再次读心不重复发放、HP/PP/等级/经验/普通背包守恒，并通过正常方向输入实际移动来确认控制恢复。等待点另检查空等与持续按键不能跳过。只读观察 ABI v6，不写游戏内存；结束或断言失败后删除本次临时 profile。可明确传入 `--dsp-component` 使用本地私有 DSP，组件不进入源码或安装包。

旧构建可加 `--expect-grant-rejection`，只接受相同的缺失接口报错及钥匙数不变，不能把该负向复现当作修复通过。

本地针对性 MSVC 编译及 Mick 用例已通过，人工起点通过真实 Session 校验和存档往返。真实构建 `64940c4b783683e26d40fb6d94aafe14c9d5fa9d` 的 3DSX/CIA 及 323 项打包资源检查已通过（[38739091243](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38739091243)）；3DSX SHA256 为 `27beb2c5233dde2df37f7e279d7c7f7474af6ee9f069e39af02ed7e470dcea99`。

该真实构建的英、中文冷启动已分别完成交付、剩余对白、再次读心与无暂停的 Walking 状态，钥匙数都从 1 变 2 且不重复增加；凭据在 `build/emulator-regression/mick-key-fixed-{en,zh}-1/`。随后驱动补充了实际移动与属性守恒断言，追加运行须单独记录，不能把先前通过当作新断言已通过。完整主机/内存检查见手动运行 [38739102383](https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38739102383) 与 PR 的最终验证记录；不能由交叉构建推定。后续提交仅修改回归工具与说明，游戏执行代码和资源保持与该构建一致。真机、CIA 安装和设备听感仍未验证。

旧版真实 3DSX `4fb7152c6c796cb201de20d248460baaa935f281` 已实际复现相同报错（`build/emulator-regression/mick-key-old-3/`）：钥匙数仍为 1，Mick 和野外均进入失败状态，截图与报告一致。前两次脚本尝试分别误用菜单限定的 `psi_learned` 观察值、漏确认 `woof_key` 的第三个来源等待点而失败；日志保留在 `mick-key-old-{1,2}`。核对观察值生产代码与固定上游翻译后纠正驱动，游戏行为和期望未改写。
