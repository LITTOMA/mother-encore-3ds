# 完整回归与音频通道修复

固定 Mother: Encore 上游仍为 7d9246600fffe518408f5830d4848635019005a3。依赖 #53；这一步修复完整回归及其揭示的真实 3DS 通道错误，不扩展原版内容，不改资源、玩法规则或存档格式。基线游戏 44f1c0a 的完整运行 38674475729 实际为 GCC 20 / 161、Clang 16 / 146 失败，Linux sanitizer 未执行，真实交叉构建 / 打包通过。旧记录写明 GCC 当时仍在运行；完成日志已另存 build/full-content-regressions/gcc-baseline-failed.log。

## 真实通道错误

主声音占用 NDSP 0–3，区域音乐占用 4–19，额外音效、对白、电话应占用 20–23。原 hardware_channel 对逻辑 4–7 加 18，错误地得到 22–25；对白和电话越过 24 通道边界，额外音效也不在声明位置。修正为加 16。正负替身检查覆盖实际通道边界、八条声音线路、区域保留通道、同时播放、音高、暂停、失败不打断已有声音和完整释放。不由此宣称已经完成原版音频图、听感或真机验收。

## 测试与来源修复

- Dad、旧版迁移、命名、本地化和战斗来源用例按当前分类目录读取；冻结 rules6 资源仍在原位置，不搬动或重写。Dad 临时目录使用本机 API，避免 Windows 下硬编码 /tmp。
- 礼物 / 地下室增量已改变已支持的 House 对白、门和选项组数量；按已经审查的追加内容验证当前数目，原有稳定身份及路线保持断言。House 文本和速度 fixture 使用其真实 schema 7；独立旧格式正负用例继续保留。
- 物品导出测试复用既有、严格校验的基础资源前缀，避免把已添加喷雾图标的结果再次作为输入追加；保留完整导出结果与二进制逐字节比较。音频测试验证已支持的 53 项，以及仍在原位置的 Introduction 五项前缀。
- 字符串偏移 1 已是合法字符串起点，负向用例改为实际非边界位置及越界；Atlas 后缀测试不再把 x 改回 x。Storage 浮点往返按格式规定的 IEEE float32 精确表示比较，并要求重新编码字节完全相同，不使用宽泛误差忽略差异。
- Python 覆盖登记显式接纳现有多模块专用命令；每个发现方法必须恰好属于汇总或一个专用任务。未知、缺失、改变、重复注册、方法丢失和重复归属拒绝；新模块仍自动归入汇总，不跳过新测试。

## 不盲目更新 trace

旧音频 TRACE 为 516 / 56ffdd52e57c7f69。116fe2b 已将 PCM 准入延迟到首次使用，旧的 missing-PCM 初始化失败只有六个事件；当前路径先分配四条缓存，再在 prepare 拒绝并清理，产生十九个事件，净增十三。实际新完整记录为 529 / 71a30145b163903b。

现在既断言完整新记录，也精确断言这段启动事件，并仅将该段替回已记录的旧六事件后重新计算历史 hash；其余所有事件必须仍为 516 / 56ffdd52e57c7f69。审查结果在 build/full-content-regressions/audio-trace-review.json。替身原来只增加 allocated 而不在释放时扣除，现按真实指针与大小记录，并验证 shutdown 后无存活缓存；没有把计数直接清零。

## 背景内核来源审查

原区域对照工具拒绝当前头文件。实际 git diff 9078a67^ → 9078a67 仅增加 CertifiedTextureBackgroundKernel friend、prepared_width / prepared_height 字段、clear 时归零及 prepare 时记录尺寸。机器核对将这四处改动逆转后与旧文件完全相同；UV、浮点表达式、调色板、合成及正常遍历顺序未变。

旧规范化 SHA 2c33c6934889a8a48951571da0391e2de041474b39b1fd141faa151f81319dcb，对应当前规范化 SHA e90d39bd25aa7ae02450bd4505143751fecf1c40763fb8993b3d2e059662cf19。保留旧两项，只追加这一个实际审查值；未知值仍拒绝。审查记录 build/full-content-regressions/background-review.json。工具的新输出归入 build，停止覆盖历史报告，也不再关闭默认泄漏检查。

## 独立碰撞预期

field_data 原手写断言要求矩形脚部停在墙边 0.2 内。官方 Godot 3.6.2 实际重复返回根位置 y=-8.027061，即脚部重叠约 0.972939，原生后端相同；这与凹线段静态 AABB 剔除和恢复行为有关。没有为迎合手写范围而修改碰撞后端。

tools/run_segment_reference.py 使用校验过的官方 Windows 引擎（引擎源码 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8）独立运行明确的矩形演员 / 凹线段 fixture，生成新的 segment_v0362.hpp。测试按 60 帧向下、10 帧滑行、移除障碍后 30 帧逐帧比较原版位置和速度。旧凸多边形预期不变。生成日志、源脚本、引擎 / 工具 / 输出指纹位于 build/full-content-regressions/segment-reference-verified；不是完整原版场景或真实玩家 hull 的运行。格式负向测试拒绝未知行、错误数量、非法向量和未知引擎。

## 实际检查边界

本地 MSVC：音频数据 27729、音频替身 1417、选项、House Record 2090、文字速度 385、Pillow 27914、迁移 220、命名 965、本地化 78 段、Field 1193 检查通过。来源模块 House 14、Items 7、BattleEntry 10、PhoneLinker 13 项通过；格式批次 90、覆盖登记 13、线段引用负向 6 项通过，这些批次不能和后续完整回归重复相加。

Clang 普通区域内核 1183542 检查通过。Windows Clang 音频替身和 trace 的 ASan / UBSan 分别完成 1417 / 1096 检查，但仍有 interception_win 警告。区域测试的自定义 nothrow new[] 与 Windows ASan 运行库重复定义，未声称其 sanitizer 通过；原 MSVC 也不接受该头文件既有 GNU attribute。Dad 在 Windows 默认 C rename 后端的重复替换失败，本地该用例未通过；不放宽断言，需要 Linux 完整运行分别确认。日志都保留。

完整 Linux 回归、真实新版 3DSX / CIA 和模拟器结果另行记录；此时仍为草稿。没有硬件、CIA 安装、完整原版移植或实际听感通过声明。

游戏源码 b965d54a6e20e0e4ff72d61981578862babcc02c 的实际 3DSX / CIA 与 323 项包内资源核对通过（Actions 38680431240）。3DSX SHA-256 c48fdbeb9c60735bc39cef6f555ee9cf4043abe14ff743103e453815f941f10b；CIA 8a7d782b6b6556eaf49bf336bd1b5ef5cc7bafb09db8ff400f79135d38a8ab81。实际下载文件核对一致。

对应模拟器两条路线各两次冷启动、结果投影一致：地图边界每次 19 检查点（人工受检存档预置钥匙），自然开局每次 56 检查点（没有剧情 / 库存 / 经验注入），完成正常 Lamp / Doll、11 经验 / 2 级及摇杆选择心灵感应。分别保留 map-audio-channels-1 与 opening-audio-channels-1 收据；四份临时模拟器目录已清除。源预期与前述已审查 Godot 方法 / 内容对照，没有增加像素或完整原版运行声明。此路线没有自然领取 Mick 钥匙或执行电话 Record / LOAD。

906 方法覆盖再次核对为 595 汇总 + 311 专用。另安全移除已被 44f 受测构建替代的 c5 旧二进制，释放 489,907,076 字节（cleanup.json）；核对本轮 Dad 用例独占临时目录的时间、文件名与固定测试快照标记后，另释放 8,320 字节（cleanup-dad.json）。新 b965 构建四次冷启动验收后，再移除被替代的 44f 二进制 489,907,280 字节（cleanup-retire44.json）。均保留来源收据、指纹和日志；加此前另一批 489,911,986 字节，本轮累计清理 1,469,734,662 字节，不重复计算。

完整 Clang 作业 116243726980（Actions 38680506778，游戏源码仍为 b965d54）现已成功：普通 make test 146 / 146，Linux ASan / UBSan 146 / 146，默认泄漏检查保留；重新生成未改变受检资源或上游。完整日志 build/full-content-regressions/clang-full-success.log。GCC 作业在此记录时仍执行完整主机阶段，尚不能宣称全矩阵通过。

GCC 的 160/161 是 CTest 组数，不能理解为只有一个 Python 用例失败。完整日志还包含聚合串行阶段 493 项中的 11 个失败、9 个错误；旧能力 8 负例另在并行 NativeContent 组中失败。0f8c5a3 已将该负例改为未知能力 9，并保留能力 8 的明确正例，NativeContent 24 项通过。游戏 78d9261 的 GCC 重跑继续暴露其余旧 fixture；以下修复覆盖这些实际失败。

## GCC 聚合测试的后续修复

游戏 78d9261 的完整 Linux Clang 再次通过：普通和默认泄漏检查下的 ASan/UBSan 均 146/146（Actions 38686620514，job 116262837987；原始日志 build/postwin-npc-restoration/clang-full-success.log）。GCC 为 160/161，失败集中于 python_tools 聚合；不是全矩阵通过。其完整失败日志保留在 build/postwin-npc-restoration/gcc-full-failed.log。

- 资源 DAG 实际有 37 次执行：34 个声明生产者、catalog/encounters 两个最终屏障以及 settings 内的 settings-check。测试核对完整任务名集合、每项只执行一次、四进程重叠和全部依赖，未知任务拒绝；使用实际 Python 解释器生成运行身份。
- 旧音频前缀只比较其真实 16:21 范围，完整银行仍严格核对 53 项。原版 PK Thunder 资源 23 与 Intro 音频 1305 是同一来源的显式复用，检查其来源 SHA，未重编号任何已保存身份。Items 严格检查帽子与喷雾两项定义及五项来源依赖。
- 临时 House 纹理负例复制已迁出的来源收据，再损坏输出或收据；不存在的收据不再替代原本应执行的拒绝断言。Intro 字体 fixture 包含真实受审查 Storage 计数文本、字形和逐对宽度，没有复制整个游戏或替换字体。
- GPU 替身补齐 C2D_AlphaImageTint；严格渲染编译保留 -Werror 并显示完整编译诊断、尊重 CXX。修复实际 renderer 的歧义缩进（仅加大括号，语义不变）以及 loading fixture 的 pair 循环复制警告。Windows 本地编译包装器只补平台 ssize_t 类型和 CRT 的 fopen 兼容声明，不用于 Linux CI，不关闭编译警告。渲染器/加载指示器 17 项通过。
- Phone 测试从原提取器隔离原 Phone/Pillow 追加边界；有界 House 格式 fixture 使用匹配的有界 Room，纹理来源另由真实资产测试覆盖。完整生成器仍检查所有当前输出，并以两个独立哈希种子核对。Phone 全部 17 项通过。
- Phone 来源报告由原 report/write 生成器更新，审查差异为 7 个已迁移工具/绑定输入指纹、74 个既有已集成音频/Storage 来源记录和 3 个当前输出指纹。原有来源 SHA、机制说明、Dad 图、选项及稳定命名空间完全相同；没有新增 broad handler 许可。候选和逐项差异保存在 build/full-content-regressions/phone-report-review.json。

历史前缀黄金值没有更新。取仓库 33e246f 的两个真实旧战斗包，与当前 Lamp/Doll 包逐节解析并解引用路径后，全部资源、布局、动画、参数、角色、菜单、元数据、背景、事件和字形完全相同。只有已声明的目录搬迁和随之改变的字符串偏移/CRC/SHA；详细对照位于 build/full-content-regressions/legacy-battle-review/proof.json。测试投影要求冻结 recipe 指纹、当前精确 SHA 和实际包字节均匹配，再还原历史路径/两个包摘要。未知字符串、身份或摘要明确拒绝，新增 3 项正负测试。已接入的 Mom/地下室门须与冻结边界的原版几何逐一一致，再投影旧边界集合；不会泛化忽略字段。

本批源码/格式集合（含 GNU make 调度）94/94、Phone 17/17、严格渲染/加载 17/17 已通过，原始日志均在 build/full-content-regressions；完整 GCC/Clang 矩阵将在提交后重跑。当前始终保持草稿；没有真机、CIA 安装或完整原版兼容声明。
