# 上游更新工作流

## 0. 当前能力边界

工具已经支持文件清单、差异、完整文件哈希审查与默认拒绝。
已实现真实游戏的局部适配：原生场景数据、精确碰撞几何/动画外部数据包、底图/人物纹理，以及共享C++的局部机制。
这些是`compatibility/reviews/`下的逐项范围审查，不是全量游戏兼容。全量门禁仍只注册`sandbox-json-v1`，
`bindings.json`保持空的真实绑定，lamp_attack有限YAML前端另有来源审查；通用Godot场景/完整剧情/.ecs到运行时的前端尚待实现。
因此首次把真实仓库交给门禁时出现大量 `unmapped_source` 是正确结果，不能通过添加“全部忽略”绕过。

## 1. 固定子模块与只读恢复

官方上游作为 `upstream/MOTHER-Encore` 子模块引入，`.gitmodules` 只声明官方 HTTPS URL 与固定路径；gitlink、`upstream.lock` 与全量 inventory 必须指向同一完整提交。子模块保持原样，不在官方源码上修改 3DS 逻辑；移植执行机制在共享核心，受检内容与绑定在独立资源，Godot 导入和参考探针在 `build/` 中的副本。

从本工程普通 Git clone 后，使用受检恢复入口，而非追踪上游分支：

```sh
python3 tools/ci_bootstrap.py
python3 tools/ci_bootstrap.py --verify-only
```

初始化前检查 `.gitmodules`、gitlink、lock 与官方来源，初始化后检查 HEAD/tree 与全部 5839 文件字节。已存在的源码只验证，不 fetch/reset/clean。现代子模块 `.git` 文件必须指向正确 Git 元数据并绑定本 checkout；不放宽源文件和目录的 symlink 检查。`--verify-only` 不写文件、Git 配置或下载缺失内容。源码 ZIP 缺少本工程 Git 根时，才允许从官方精确 pin 恢复独立 clone；有本工程 Git 根时不能绕过 gitlink。

### 首次审计与候选更新

本机已固定最新公开 v0.4.1.0，见 `UPSTREAM_AUDIT.md`。官方仓库无 release/tag，
确认下载版本与源码 GAME_VERSION 后固定完整 SHA，不直接把任意 main 当稳定版。
`compatibility/upstream-inventory.json` 只作字节清单，不是已批准的兼容 baseline。
Git tracked 的缓存/导入文件也全部记录；有脏源文件、额外未跟踪文件或特殊 Git mode 时拒绝。
已经取得干净 checkout 时可使用 `pin-existing --ref <完整SHA> --version <核对后的游戏版本>`，
该命令核对实际HEAD、origin、版本及文件清单，不跳过语义审查。

在有网络的本机，工程根目录：

```sh
python3 tools/upstream.py fetch --ref <核实后的完整SHA> \
  --checkout build/upstream-candidate --lock build/upstream-candidate.lock
python3 tools/upstream.py snapshot \
  --root build/upstream-candidate --out build/upstream-candidate.json
```

先将现有 `upstream.lock` 复制到 `build/upstream-candidate.lock`；获取命令只更新该候选 lock，清除旧版本身份字段。开发约定禁止用默认 fetch 入口改变固定子模块，应始终明确传入独立候选 checkout 和候选 lock。现有 fetch 工具拒绝未提交变化与 origin 不一致，不承担子模块保护；完整候选 snapshot 另会拒绝额外文件。完成下列语义审查后，才在明确的更新提交中同时更新 gitlink、工程 lock、完整 inventory 和受影响的审查记录；不能单独更新 reviewed SHA 使门禁通过。本轮保留 `tools/upstream.py` 与主线逐字节一致，避免改变它在当前与 legacy rules6 内容中已审查的来源指纹。

外部插件、LFS等依赖需要单独取得并记录，fetch脚本不假装已解决它们。
以这个commit的原版运行结果为对照基线。

## 2. 建立真实绑定

`compatibility/bindings.json` 中每个上游路径必须声明：

- `native`：手工实现的逻辑。需要 `reviewed_sha256`、存在的 `native_files`、`tests` 和显式 `dependencies` 哈希。
- `exported`：必须由已实现导出器处理。当前仅支持 `sandbox-json-v1`；真实游戏导出器需要开发后注册。
- `ignored`：确实不在移植运行范围内的内容。需要具体理由和审查哈希；内容变更仍要求重新审查。

示例位于 `binding.example.json`，不会自动载入，也不会因为文件名看起来像C++模块就宣称已实现。
工具只验证测试映射存在，不证明测试充分；测试执行仍是独立发布步骤。

## 3. 检查候选更新

先保留旧baseline，再明确获取候选commit。

```sh
python3 tools/upstream.py fetch --ref <候选完整commit或明确选择的ref> \
  --checkout build/upstream-candidate --lock build/upstream-candidate.lock
python3 tools/upstream.py snapshot \
  --root build/upstream-candidate --out build/upstream-candidate.json
python3 tools/upstream.py diff \
  --base compatibility/baseline.json --candidate build/upstream-candidate.json \
  --out build/upstream-diff.json
python3 tools/upstream.py gate \
  --base compatibility/baseline.json --candidate build/upstream-candidate.json \
  --candidate-root build/upstream-candidate \
  --out build/upstream-gate.json
```

gate退出码：0表示静态审查/已注册导入验证通过，2表示有阻断项，1表示工具/参数错误。
不会在每次diff后自动覆盖baseline，也不会自动更新审查指纹。

报告包含新增、修改、删除文件、分类、自动内容更新列表，以及来源/依赖变化和未支持项。
快照必须匹配当前checkout字节，防止对旧快照审查却打包新内容。
删除文件需要 `reviewed_removals` 登记原哈希与迁移理由，否则阻断。

## 4. 更新处理

对数据变化：经已实现适配器重新导出、完整校验、按依赖使缓存失效，再运行受影响回归。
对native变化：逐项审查语义，更新C++或确认不影响运行，再更新指纹和测试。
对新机制：先实现并测试能力，再注册导出/运行时绑定，最后导入引用它的内容。
对资源移动：迁移稳定ID别名，不能让文件路径变化自动清空存档身份。

父资源默认值、解析器、执行器、常量和辅助函数也是依赖。
当前自动`res://`扫描不保证依赖完整；在完整依赖图建立前应保守扩大审查范围。

## 5. 对照和发布

重新运行**候选新上游**与新C++宿主的对照，不是只与旧golden比。
相同初始存档、输入、时钟和带位置标签的RNG事件，比较规则状态/命令顺序/任务完成。
已建立局部原版函数/原生引擎参考（移动、动画、碰撞、flags等）；它们不构成完整游戏可重放宿主。
`compare_traces.py`另提供精确JSONL比较框架。

存档迁移、程序/包版本拒绝测试、硬件资源验收通过后，才把候选snapshot提升为baseline。
提交记录应包含“为什么更新reviewed hash”，不能只附hash变化。
声明的兼容范围内不能保留未分类项；不在范围的内容必须显式拒绝进入，而不能让玩家走进去后死锁。

## 6. 更新演练测试

`tests/test_upstream.py` 在临时原创fixture目录验证：不变通过、数据变化自动导入、native变化阻断、新脚本阻断、
未知剧情指令阻断、删除文件阻断、依赖变化阻断、快照过期阻断、虚构真实游戏适配器阻断、缺少C++实现阻断、路径穿越拒绝。
这些测试证明门禁机制，没有声称已经跟随过某次真实游戏更新。
