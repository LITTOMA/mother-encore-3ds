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

该目标编译受检的外部游戏内容，不编译 C++。普通构建会验证资源引用与来源，缺失或改变时停止。不要手改 pack、manifest，或以 `extract_native_content.py` 覆盖未经重新审查的 IR。

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

源码默认使用 CPU 背景路径。常规 GPU 开发配置显式启用背景与证书，保留 CPU fallback：

```sh
make 3dsx EXPERIMENTAL_GPU_BACKGROUND=1 EXPERIMENTAL_GPU_CERTIFICATES=1 EXPERIMENTAL_GPU_TEXTURE_STRIPS=0
make cia EXPERIMENTAL_GPU_BACKGROUND=1 EXPERIMENTAL_GPU_CERTIFICATES=1 EXPERIMENTAL_GPU_TEXTURE_STRIPS=0
```

纹理条带是独立实验选项，不能从上述配置推导其兼容性。设备程序与主机测试共享游戏核心，M0 fixture 不进入生产设备程序。

## 打包

```sh
python3 tools/release.py
```

SD ZIP 包含真实 3DSX/CIA、所需资源及逐项核对的许可文件。运行时资源置于 RomFS，不支持 SD 覆盖或热重载。CIA 使用未全局登记的测试 TitleID `000400000F3E2100` 和 homebrew 测试签名；构建成功不代表安装、真机运行或官方认证。

## GitHub runner 构建

GitHub Actions 只保留手动全面验证。Push、PR 创建 / 更新 / 重开、转为 Ready、添加标签和 main 更新都不启动检查；没有自动快速测试或定时任务。

需要全面验证时，在 Actions 的 `Manual full Encore Native verification` 中选择目标分支并点击 **Run workflow**，或显式执行：

```sh
gh workflow run build.yml --ref <候选分支>
```

一次手动运行覆盖 GCC / Clang 共享核心、ASan / UBSan、受检来源与资源重生成，以及真实 3DSX / CIA 构建和提取资源比较。生成、编译和已审查测试保持四路并发；共享写入与计时检查保留必要顺序。检查结论只覆盖实际提交；未手动运行的候选明确标为未验证，不把缺少 CI 当作通过。

电话与 Dad 来源转换在单次只读操作中复用已校验的剧情配方，避免每个命令重新读取来源并启动 Git 查询。首次使用及返回结果前均执行完整来源校验；配方改变或来源不匹配会阻断结果，操作结束或失败后不保留缓存。此优化不删减正常／负向测试，也不改变二进制中的游戏内容。

Actions 的 `Real 3DSX and CIA (GPU 1:1:0)` 任务从仓库直接完成真实 `make 3dsx` 和 `make cia`，不要求先在本地编译。Ubuntu runner 恢复已有清单固定的官方 devkitPro SDK layer，强制核对 bannertool archive SHA-256，并从固定官方 Project_CTR 源码构建 makerom / CTRTool。

任务先核对只读上游，恢复受检 PCM，再编译 ARM ELF 与嵌入 RomFS 的 3DSX，生成 CIA。CTRTool 提取真实 CIA 并逐文件比较完整 RomFS 与受检 staging；另检查 3DSX 嵌入边界及资源目录消费者依赖。FFmpeg 使用 Ubuntu 包，输出必须逐项匹配既有 PCM 长度、SHA-256 和 CRC；不同版本不允许刷新期望指纹来通过检查。

此任务不上传游戏二进制、素材或发行包。实际结果以对应提交的 Actions 运行记录为准；维护者下载完整运行日志到私有验证目录。CI 文件存在不代表构建通过，交叉构建 / 打包通过也不代表模拟器或真机验收。已有本地构建保留用于开发调试与设备验证。
