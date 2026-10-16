# Podunk 桥头相机位置修复

原版固定提交为 7d9246600fffe518408f5830d4848635019005a3。桥头约 (314,279) → (351,274) 的画面跳走来自地图编译错误：ReferenceRect 是 CanvasItem，margin_left/top 构成它的位置；旧编译器将它当作单位变换，漏算 (1952,1064)，把相机碰撞区中心从 (2390,1308) 放到了 (438,244)。触发后却应用远处的原版限制，因此主角移出了画面。

仅实现已审查静态 ReferenceRect 的位置变换；未知锚点、旋转、缩放、非有限 margin 等继续拒绝。资源通过原生成器重建，纹理、碰撞、门连接和相机限制不变，格式不变。C++ 游戏执行逻辑不含新增坐标或规则。

实际 Godot 3.6.2 隔离运行了未修改的原版 camarea.gd，确认中心 (2390,1308)、大小 (520,456)、限制 (1952,1064,2472,1520)。这是源脚本/布局的隔离验证，未自然运行整个原版地图。

来源及负向 Python 回归 11 项通过。当前主线共享核心重新编译，场景/资源测试 1,979 项通过，包含 400×240 和 320×180 桥头往返、真实区域进入/退出与限制保留。真实 make 3dsx / make cia 和 CIA 解包资源校验已通过（运行 38853991909，提交 51979af）。完整运行 38854064478 全部通过：GCC 普通 164/164、ASan/UBSan 149/149；Clang 普通 149/149、ASan/UBSan 149/149，均保留默认泄漏检查；真实 3DSX/CIA 和 323 项包内资源检查通过。实际原始日志保存在 build/podunk-camera，摘要见 [CI](../reports/podunk-camera/ci.json)。真机尚未验证。

旧实际安装包已在 Azahar 以普通 LOAD/移动复现：约 (317,279) → (356,279) 后画面跳到远处、主角不可见。起点是共享核心验收过的人工 post-treats 房屋出口存档；未通过内存改写跳转。

[资源比较](../reports/podunk-camera/resource-comparison.json) 证明仅 Cameras 表的两个中心坐标改变，其他全部地图段、纹理收据、房屋/室外链接完全相同。[原版参照](../reports/podunk-camera/original-reference.json) 可用 tools/check_podunk_camera_reference.py 重新运行。旧图负向回归会在相机中心断言失败，并保留实际日志。

修正版实际 Azahar 冷启动三次（英文、中文、进一步边界返回）全部通过，相同语义投影；玩家在旧触发点后持续可见。第三次继续到 (450,279)，按 B 从尚未移植的 beat_dog_bridge 桥头狗战事件返回，再次往返正常。相关剧情仍是明确开发边界，完整过桥剧情和真机未完成。参见 [模拟器比较](../reports/podunk-camera/emulator-comparison.json)、[修复后画面](../reports/podunk-camera/fixed-crossing.png) 和 [剧情边界](../reports/podunk-camera/story-boundary.png)。

已清理独立临时模拟器配置、私人 DSP 副本和旧对照安装包 490,183,567 字节；保留来源收据、截图、日志及新真实安装包 dist/podunk-camera-51979af。
