# 外部技术依据

以下为上游来源与外部接口、工具依据。
固定上游为 Act2 v0.4.1.0，提交 `7d9246600fffe518408f5830d4848635019005a3`，树 `dab3bb25b417d2273a35679a3edfbc45a597dcf9`。具体文件清单与来源校验见 `upstream.lock` 和 `compatibility/upstream-inventory.json`。

- devkitPro Getting Started: https://devkitpro.org/wiki/Getting_Started
- devkitPro pacman: https://devkitpro.org/wiki/devkitPro_pacman
- devkitPro 3DS examples: https://github.com/devkitPro/3ds-examples
- devkitPro application Makefile (architecture/specs/tool invocation): https://github.com/devkitPro/3ds-examples/blob/master/templates/application/Makefile
- libctru: https://github.com/devkitPro/libctru
- Citro2D base API: https://github.com/devkitPro/citro2d/blob/master/include/c2d/base.h
- Citro2D text API: https://github.com/devkitPro/citro2d/blob/master/include/c2d/text.h
- Project_CTR makerom format/CLI documentation: https://github.com/3DSGuy/Project_CTR/blob/master/makerom/README.md
- makerom v0.19.0: https://github.com/3DSGuy/Project_CTR/releases/tag/makerom-v0.19.0
- makerom release assets and published SHA256: https://github.com/3DSGuy/Project_CTR/releases/expanded_assets/makerom-v0.19.0
- bannertool v1.2.2: https://github.com/Epicpkmn11/bannertool/releases/tag/v1.2.2
- Public homebrew RSF reference: https://github.com/TricksterGuy/3ds-template/blob/master/resources/template.rsf
- Mother: Encore official source: https://github.com/motherencore/MOTHER-Encore-Source-Code
- Godot 3.6 SceneState API: https://docs.godotengine.org/en/3.6/classes/class_scenestate.html

工具链锁应在实际成功构建后记录准确版本和镜像digest，不能只保留“latest”。
