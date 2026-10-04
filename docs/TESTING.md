# 测试

## 手动全面验证

测试仅在明确手动请求时执行，不随日常开发、提交、PR 更新或 main 合并自动运行。GitHub Actions 只有手动完整入口，覆盖 GCC / Clang、sanitizers 和真实 3DSX / CIA；操作见 [BUILD](BUILD.md)。以下命令保留为手动验证工具，代理不得自动调用。

## 共享核心与工具

```sh
make test
make sanitize
```

检查包括外部资源加载、剧情调度、人物移动与动画、碰撞、摄像机、局部战斗、文字、存档和资源编译工具。M0 沙盒只验证 fixture 规则，不代表原版玩法。

解析器、opcode 和版本路径必须包含负向测试：截断、损坏、未知版本或来源、无效身份和失败回滚。未知内容须明确拒绝，不能以忽略规则或修改参考期望消除失败。

ASan / UBSan 在 GCC、Clang 上运行，保持泄漏检查。若 sanitizer 在进入程序前启动失败，应保留日志并用独立程序验证工具链环境；不能据此将游戏测试记为通过或关闭检查。

## 上游行为对照

`upstream.lock` 和 `compatibility/` 固定真实来源和已审查机制。独立 Godot 3.6.2 对照及保留的参考数据用于检查位置、速度、动画、旗标、动作完成和 Timer / idle / physics 顺序。

上游更新须检查语义差异、对应实现和相关测试，再同步 gitlink、lock、inventory 与受影响的审查记录。不要仅更新 SHA。像素与帧对照的结论只适用于记录中的路径与配置。

## 设备与打包

```sh
make 3dsx
make cia
python3 tools/release.py
```

实际工具链构建、许可与资源暂存核对、CIA 解包、模拟器交互、真机运行是不同层次的验证。保留具体提交、配置、工具版本、产物哈希和原始日志；检查存在不等于已经运行。

手动完整 CI 执行主机与消毒器检查。3DS 交叉构建和打包见 [BUILD](BUILD.md)。硬件验收还需覆盖 Old / New 3DS 的帧率、内存、音频、输入、休眠恢复、存档与 CIA 安装；当前尚未完成。

## CI 的受限多进程调度

构建与手动完整 CI 使用至少 4 路：GNU make 调度资源生成，CMake 并行编译，CTest 使用 `--parallel 4`。已审查文件读写的来源检查和七项生产消费者可并行；共享文件写入、未审查测试和计时探针继续通过 `RUN_SERIAL` 独占执行。并发上限不是每个时刻都有四个可运行任务；存在依赖或剩余任务不足时会减少。

Python 聚合测试占用四个 CTest 槽，将六个已审查只读模块分配给最多四个独立解释器，隔离模块状态与 mock。其他模块等待子进程全部退出再执行。收集与实际执行的方法身份必须逐项一致；重复、缺失、导入失败或子进程失败均使检查失败。日志与 PID 记录保存在 `build/host/python-workers/`，旧成功记录不能用于新运行。新模块默认串行。

`make/native-content.mk` 将原有 27 条资源命令组织为独立目标与依赖。房间来源核验在入口 / 效果包完成后进行，并在重新写入 Lamp 战斗包之前完成；Restore 等待房间与 House；目录和遭遇指纹等待所有生产者。Settings 的生成与验证保持顺序。CMake 直接构建也使用同一图，并让读取资源的 fixture 准备和编译等待生成完成。所有目标仍为 phony，逐次保留来源核验，不通过时间戳跳过检查。

资源日志与起止时间 / PID / 退出码保存在 `build/content-jobs/`，CI 上传诊断日志。多个 `CONTENT START` 先于对应 `CONTENT END` 表示重叠执行；记录中的区间可核对实际并发峰值。这些属于构建诊断，不进入 RomFS。

默认 `BUILD_JOBS=4`、`CONTENT_JOBS=4`；独立 CMake 的 `ENCORE_CONTENT_JOBS=4` 和测试的 `ENCORE_TEST_JOBS=4` 可调高，CTest 的并发参数应与后者一致。单进程资源基准可显式使用 `make native-content CONTENT_JOBS=1`；测试串行对照可使用 `-DENCORE_TEST_PARALLEL=OFF` 和 `CTEST_ARGS="--parallel 1"`。完整 CI 耗时仍须以当前提交的 runner 记录衡量，不把局部资源基准当作整体加速结果。
