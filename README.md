# Mother: Encore 3DS

Mother: Encore 的原生 Nintendo 3DS 移植，使用 C++17 共享游戏核心和 devkitPro、libctru、Citro2D。项目处于开发阶段，目前支持部分开局流程，尚不能游玩完整游戏。

上屏按 **400×240** 适配，图素保持 1:1；下屏显示开发信息并提供触摸输入。320×180 仅用于可选的原作参考模式。本项目与 Team Encore、Nintendo 无官方隶属关系。

## 当前支持

- 标题、六字段命名、设置和最终确认。
- 开局房屋移动、碰撞、交互与局部剧情。
- 已审查的 Lamp、Doll、Pillow / Minnie 流程及部分原版战斗机制。
- Dad Record，以及受限的 Continue / LOAD。
- 英文和简体中文。

Introduction、Podunk 及其余地图与剧情、完整战斗机制、音频可听性和 Old / New 3DS 真机验证仍待完成。详细范围见 [项目状态](docs/STATUS.md)，后续工作见 [开发计划](docs/DEVELOPMENT_PLAN.md)。

## 获取源码

```sh
git -c core.autocrlf=false clone --recurse-submodules https://github.com/LITTOMA/mother-encore-3ds.git
cd mother-encore-3ds
python3 tools/ci_bootstrap.py
```

上游使用只读子模块，固定为 Act2 v0.4.1.0 的提交 `7d9246600fffe518408f5830d4848635019005a3`。恢复工具核对锁定版本及全部来源文件；已有 checkout 不会被重置或清理。显式禁用换行转换可避免 Windows 改变受检字节。

## 构建

电脑上的共享核心自动测试需要 C++17 编译器、CMake 3.16+、Python 3.9+ 和 FFmpeg / ffprobe。这些测试不制作 PC 游戏版本；游戏构建目标是 3DSX / CIA。

```sh
make test
make sanitize
```

安装 devkitPro 和相应 3DS 库后：

```sh
make 3dsx
make cia
```

CIA 另需 makerom 和 bannertool。生成文件位于 `dist/`。完整依赖、资源恢复和 GPU 配置见 [构建说明](docs/BUILD.md)，运行方式见 [安装与操作](docs/INSTALL.md)。

游戏资源按用途统一组织到 `graphics/`、`sound/`、`fonts/`、`data/` 和 `licenses/`，构建记录留在 RomFS 外。见 [资源目录](docs/ROMFS_LAYOUT.md)。

## 来源与许可证

本项目自行编写的代码、工具和原创文档采用 [MIT](LICENSE)。上游代码、游戏素材、音乐、字体和运行库保留各自版权与许可；根目录 MIT 不覆盖这些第三方内容。素材与音乐遵循上游的游戏相关用途条件，已核实字体部分保留 OFL。Nintendo 来源字形的独立授权仍未确认。

详见 [第三方来源与许可](THIRD_PARTY_NOTICES.md)、[许可索引](docs/licenses/README.md) 和 [上游来源](docs/SOURCES.md)。

参与开发请阅读 [贡献指南](CONTRIBUTING.md)、[架构](docs/ARCHITECTURE.md) 和 [测试说明](docs/TESTING.md)。安全问题见 [SECURITY](SECURITY.md)。
