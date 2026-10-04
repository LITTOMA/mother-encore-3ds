# RomFS 资源目录

RomFS 按运行用途归类，目录名称不记录开发阶段。实际打包由受检资源依赖清单决定，不直接复制整个源码目录。

```text
romfs/
  graphics/
    actors/           # 世界人物图集
    world/            # 地图背景、房屋分层
    battle/           # 遭遇背景、敌人、回合演出
    ui/               # 标题、命名、菜单和提示图像
  sound/
    music/            # 音乐 PCM
    effects/          # 音效 PCM
    banks/            # 受检音频索引、循环点与播放参数
  fonts/              # 字体二进制元数据及字形图集
  data/               # 场景、剧情、战斗、菜单、输入及目录等二进制资源
  licenses/           # 打包时从受检许可文件生成
```

`graphics/` 内只放 `.t3x` 图集与 `.bpx` 战斗背景；`fonts/` 将字形图集和对应字体元数据放在一起。剧情程序已编译进受检二进制数据，因此没有运行时脚本目录或 GDScript 解释器。资源使用 RomFS 相对路径，稳定对象 ID、规则与存档版本不随目录调整改变。

转换来源、参数、输出指纹及字体 / 音频生成记录存放在 `content/asset-receipts/`，不进入游戏 RomFS。许可来源清单随 `licenses/` 保留。M0 测试资源生成到 `build/fixtures/`，与实际游戏资源分开。

`tools/stage_native_romfs.py` 校验来源、二进制资源和纹理指纹后写入 `build/ctr/native-romfs/`。目录检查拒绝旧 `*-preview` 路径、越界路径、未分类文件和混入的构建 JSON。3DSX / CIA 使用这个暂存目录；变更资源位置时必须同步作者输入及生成工具，并通过原转换器重生成包，不能手改二进制字符串或来源清单。
