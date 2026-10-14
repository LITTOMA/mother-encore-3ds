# 安装与操作

这是开发中的局部移植。支持范围见 [STATUS](STATUS.md)；完整游戏、音频可听性和 Old / New 3DS 真机验证尚未完成。

## Homebrew Launcher

构建或从 SD ZIP 取得文件，放入：

```text
sd:/3ds/encore-native/encore-native.3dsx
sd:/3ds/encore-native/encore-native.smdh
```

游戏资源已嵌入 RomFS，无需额外资源目录。资源损坏或版本不兼容时加载会停止，下屏显示原因。更新时替换完整构建，不支持运行中的资源热替换。DSP 组件需由设备环境提供，项目不附带该组件。

## 操作

| 输入 | 操作 |
| --- | --- |
| D-pad / Circle Pad | 移动、选择菜单 |
| A | 确认、互动、推进文字 |
| B | 返回；世界中的奔跑 / 蹲下切换 |
| START | 房屋或 Podunk 室外打开 Pause / Equip；开场动画中跳过；退出使用标题菜单 Exit |
| SELECT | 当前尚未映射游戏动作，保留给后续输入适配 |
| 下屏拖动 | 虚拟摇杆移动 |
| 下屏轻触 | 确认 |

上屏默认显示 400×240、1:1 图素的游戏画面；下屏保留开发状态。拖动结束不会触发轻触确认。

普通构建关闭画面参考模式的调试快捷键。仅显式设置 `DEBUG_REFERENCE_VIEW=1` 的开发构建，允许在标题页按住 L＋R 后按 SELECT，切换 400×240 / 320×180 并重新初始化标题会话；游玩、命名、读档及开场过程中不能触发。见 [构建说明](BUILD.md)。

固定原作的手柄 SELECT（[Godot `JOY_SELECT=10`](https://docs.godotengine.org/en/3.5/classes/class_%40globalscope.html#enum-globalscope-joysticklist)）绑定 `ui_select`，用于打开／关闭暂停菜单及跳过开场等；当前 3DS 版将这些已支持动作映射到 START，物理 SELECT 尚未接入游戏动作。来源见 `project.godot` 的 `[input]`、`Scripts/global/global.gd`、`Scripts/UI/Pausemenu.gd` 与 `Scripts/UI/Introduction.gd`。

Record 通过已接入的电话流程写入原生存档；退出不会自动保存。Continue / LOAD 只接受已支持的内容和格式。其他已识别但尚未移植的出口会显示开发边界，这不是原作的门锁效果。

## CIA

CIA 使用 homebrew 测试签名和未全局登记的 TitleID `000400000F3E2100`。安装前检查同 ID 冲突并保留现有存档。CIA 安装与真机运行仍待验证；项目不提供系统破解流程。
