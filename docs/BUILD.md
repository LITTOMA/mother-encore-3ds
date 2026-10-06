# 构建

项目包含共享 C++17 核心、桌面测试和原生 3DS 平台实现。SDK、上游 checkout、缓存和构建产物不纳入源码。

## 依赖

- 主机：C++17 编译器、CMake 3.16+、Python 3.9+、Git、FFmpeg / ffprobe。
- 3DS：devkitARM、libctru、Citro2D、Citro3D、tex3ds，正确设置 `DEVKITPRO`、`DEVKITARM` 和 `PATH`。
- CIA：makerom 0.19.0、bannertool 1.2.2。CTRTool 1.3.0 可用于验包。
- 重新生成贴图：Pillow、fontTools 和 FreeType；独立上游行为对照使用 Godot 3.6.2。

Windows 使用 devkitPro MSYS2；需要时可通过 `make PYTHON=python` 指定 Python。隔离 SDK 获取脚本与固定工具来源见 [tools/cloud-sdk](../tools/cloud-sdk/README.md)。

## 上游与资源

```sh
python3 tools/ci_bootstrap.py
python3 tools/ci_bootstrap.py --verify-only
```

恢复工具检查固定官方 URL、提交、树和 5,839 个文件的内容与大小。已有 checkout 只核对；未知文件、字节变化或版本不一致会停止。`--verify-only` 不下载。不要使用 `upstream.py fetch` 改变当前固定子模块。

源码包含受检的外部内容 IR、生成包和纹理。21 项 PCM 从固定上游按指纹恢复，不纳入 Git。FFmpeg 仅用于离线转换，不链接进设备程序；DSP 组件不随项目提供。详见 [音频恢复](AUDIO_RESTORATION.md)。

```sh
make native-content
```

该目标在隔离目录中编译受检的外部游戏内容，以至少四个进程执行依赖图，不编译 C++，也不启动测试。它比较已登记的 IR、二进制和来源记录；过期或缺失的受检输入会明确阻断，不会修改 Git 中的文件。缺失的非 Git PCM 仍通过固定来源和转换指纹恢复。本地与 Actions 都使用这个入口。

已审查的输入或转换器修改后，统一更新完整依赖链：

```sh
make regenerate-content GODOT3=/path/to/Godot-3.6.2 TEX3DS=/path/to/tex3ds PICASSO=/path/to/picasso CONTENT_JOBS=4
```

显式更新先验证固定上游的全部字节，在独立本地 checkout 中提取已支持的来源、生成 IR、运行真实贴图 / 字体转换器、编译资源，最后生成目录和遭遇指纹。已登记的来源派生和贴图任务与普通编译共享 `make/native-content.mk` / `make/refresh-content.mk` 的依赖顺序；新增生产器必须登记其输入、来源派生或贴图任务及消费者边。来源提取不能批准新上游、未知语义或未支持能力，原有校验继续拒绝这些变化。记录过期不能靠手改 SHA、pack 或 manifest 消除。

入口资源只记录实际引用的三条战斗目录绑定；新增不相关目录条目不再改变入口来源身份。目录仍独立校验全部类型、引用和固定来源，完整目录资源在所有消费者生成完毕后更新。

开场字体直接使用独立的 `tools/godot_exporter/introduction_font_metrics.gd`，来源记录绑定该探针，不再绑定库存等玩法文字生成器，也不再通过替换另一套探针的字符串生成脚本。真实 Godot 字符间距、逐行宽度、字体回退、原始字体属性和固定引擎版本仍须全部核对；旧字体记录版本明确拒绝，必须通过真实转换器重新生成。

任何生产器失败，隔离生成均不发布文件。完整候选通过后，发布前再次核对工作目录输入，更新产物与来源记录并保存恢复日志；发布中断或失败会回滚，尚未恢复的中断会阻断后续生成和 RomFS 暂存。全部原始日志和产物清单位于 `build/content-generation/<run>/`，其中 `result.json` 列出本次发布文件；将完整 IR、受检来源记录、二进制与素材一起审查和提交。转换器源码必须保持最终字节，再生成其来源记录。

强制退出导致未完成的发布时，使用该次实际日志恢复；并发编辑冲突会明确阻断，不能覆盖其他人的修改：

```sh
python3 tools/content_pipeline.py recover --journal build/content-generation/<run>/publication.json
```

## 主机检查

```sh
make test
make sanitize
```

`make sanitize` 启用 ASan / UBSan，保持默认泄漏检查。具体覆盖和平台验证区别见 [测试说明](TESTING.md)。

## 3DS 构建

```sh
make 3dsx
make cia
```

源码默认使用 CPU 背景路径。Actions 默认下载包显式启用背景、证书和枕头纹理条带（`1:1:1`）。本地使用相同配置，保留精确 spans 与 CPU fallback：

```sh
make 3dsx EXPERIMENTAL_GPU_BACKGROUND=1 EXPERIMENTAL_GPU_CERTIFICATES=1 EXPERIMENTAL_GPU_TEXTURE_STRIPS=1
make cia EXPERIMENTAL_GPU_BACKGROUND=1 EXPERIMENTAL_GPU_CERTIFICATES=1 EXPERIMENTAL_GPU_TEXTURE_STRIPS=1
```

纹理条带恢复用于支持的 400×240 枕头背景；不支持的背景或未获纹理资源时沿用已有精确 spans / CPU 路径。战斗下屏的 `Background` 行显示实际后端。保留原 spans 配置可显式设 `EXPERIMENTAL_GPU_TEXTURE_STRIPS=0`；纯 CPU 配置为三个开关全部 `0`。此变更不构成新的模拟器帧率或真机采样精度结论。设备程序与主机测试共享游戏核心，M0 fixture 不进入生产设备程序。

## 打包

```sh
python3 tools/release.py
```

SD ZIP 包含真实 3DSX/CIA、所需资源及逐项核对的许可文件。运行时资源置于 RomFS，不支持 SD 覆盖或热重载。CIA 使用未全局登记的测试 TitleID `000400000F3E2100` 和 homebrew 测试签名；构建成功不代表安装、真机运行或官方认证。

## GitHub runner 构建

每次 main 更新（包括合并 PR）自动构建真实 3DSX / CIA，完成必要的来源、许可和打包校验后上传 Actions artifact。PR 创建 / 更新 / 重开、转为 Ready 和标签变化不启动工作流；测试仅在明确选择手动全面验证时运行，没有定时任务。

在 Actions 的 `3DS artifacts and manual full verification` 中选择目标分支和模式，再点击 **Run workflow**。默认 `build` 只构建下载包；明确选择 `full` 才运行全面测试：

```sh
gh workflow run build.yml --ref <候选分支> -f mode=build
# 仅在明确要求全面测试时：
gh workflow run build.yml --ref <候选分支> -f mode=full
```

一次 `full` 手动运行覆盖 GCC / Clang 共享核心、ASan / UBSan、受检来源与资源重生成，以及真实 3DSX / CIA 构建和提取资源比较。生成、编译和已审查测试保持四路并发；共享写入与计时检查保留必要顺序。检查结论只覆盖实际提交；未手动运行的候选明确标为未验证，不把缺少 CI 当作通过。

电话与 Dad 来源转换在单次只读操作中复用已校验的剧情配方，避免每个命令重新读取来源并启动 Git 查询。首次使用及返回结果前均执行完整来源校验；配方改变或来源不匹配会阻断结果，操作结束或失败后不保留缓存。此优化不删减正常／负向测试，也不改变二进制中的游戏内容。

Actions 的 `Real 3DSX and CIA (GPU 1:1:1)` 任务从仓库直接完成真实 `make 3dsx` 和 `make cia`，不要求先在本地编译。Ubuntu runner 恢复已有清单固定的官方 devkitPro SDK layer，强制核对 bannertool archive SHA-256，并从固定官方 Project_CTR 源码构建 makerom / CTRTool。

任务先核对只读上游，恢复受检 PCM，再编译 ARM ELF 与嵌入 RomFS 的 3DSX，生成 CIA。CTRTool 提取真实 CIA 并逐文件比较完整 RomFS 与受检 staging；另检查 3DSX 嵌入边界及资源目录消费者依赖。FFmpeg 使用 Ubuntu 包，输出必须逐项匹配既有 PCM 长度、SHA-256 和 CRC；不同版本不允许刷新期望指纹来通过检查。

成功后在对应运行页面的 **Artifacts** 下载 `encore-3ds-<完整提交 SHA>-<运行 ID>`，保留 30 天。解压后包含 `3ds/encore-native/encore-native.3dsx`、SMDH、`cias/encore-native.cia`、安装说明、许可文件、`SHA256SUMS`、提交与产物哈希收据及构建配置。不同 main 提交的构建不会互相取消；失败时不上传不完整安装包。此流程只上传 Actions artifact，不创建 GitHub Release。实际结果以对应提交的 Actions 运行记录为准；维护者下载完整运行日志到私有验证目录。CI 文件存在不代表构建通过，交叉构建 / 打包通过也不代表模拟器或真机验收。已有本地构建保留用于开发调试与设备验证。
