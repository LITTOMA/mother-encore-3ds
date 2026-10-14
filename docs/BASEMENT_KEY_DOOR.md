# 地下室钥匙与往返通路

固定上游 7d9246600fffe518408f5830d4848635019005a3。依赖 #49 已整合的 #46–#48；不是完整游戏验收。

原版 Openable Door.gd 的 interact 自动调用 Inventory.find_item_for_all(key)：当前 party 背包之后查关键物品，不查仓库。地下室 KeyBasement 是已审查的关键物品，由 Mick 心灵感应取得。没有钥匙显示 Reusable/locklocked；有钥匙先写 ninten_basement_door，再 open / unlock，设置 Item 文本上下文，最后显示 Reusable/lockopened。remove_key 的原版默认值为 false，场景没有覆盖；钥匙及其 UID / 剂量保留。重复调查已解锁门直接返回。

共享核心从 House 二进制读取钥匙、旗标、对白路径、声音、几何和动画掩码。新 HouseKeyInventory 只查询已审查的保留型关键物品；未绑定身份、缺失或重复对白、未知通知或消耗型策略拒绝。没有加入手动 Goods 使用机制。两段原版对白用原工具追加为稳定 ID73/74，固定物品名称和冠词按原版 CSV 生成，PartyLead 仍使用实际昵称；英文 / 简体中文均支持。

House 来源绑定 v2 添加原版 Living_Basement / Living_Basement2，传送 ID9/10。缩放后的目标为 (88,977) / (96,705)，沿用现有原版淡入淡出与输入恢复。旧八个传送身份和 Podunk 边界 ID3 保留；v1 来源绑定仍受检接受。地图与图像不变，地下室 Present1–3 仍是明确未支持交互。

House 包格式 / 能力 / 规则仍为7，Session 资源格式仍为6；存档 schema / 内容家族 / rules 不变。Session 来源派生的可变旗标增加开锁旗标，允许保存区域增加实际连接的地下室 CameraArea6；原版默认值、角色、技能、奖励、获取策略不变。Restore 与 Naming 的依赖记录通过原生成器更新，命名二进制及纹理保持一致。

只读模拟器观察 ABI 升为 v3（716 字节）：增加战斗阶段、敌人 HP、房门开锁位图和剧情 phrase，供自然输入路线核对；不提供存档或内存写入口。旧版本及未知字段仍明确拒绝，旧凭据仅证明其记录的构建。

## 实际检查

- MSVC 共享核心门测试：1466 检查，覆盖缺钥匙、旗标 / 开门 / 解锁 / 文本顺序、延迟碰撞、重复调查、往返与恢复、查询拒绝；主机准备位置是明确的测试夹具，不代表自然游玩验收。
- 官方 Godot 3.6.2：提取实际门与库存查找方法执行五项原版对照。服务、物品容器及 UI 是显式适配器；真实 deferred 属性和 AnimationPlayer 运行，声音、原版完整存档加载及画面不在此对照内。日志 / 方法与工具 SHA 留在 build/basement-key-door/original-reference-4，成功后删除临时工作副本。
- 保存、来源绑定、解析负向检查和真实交叉构建的最终结果后续追加；旧 #49 全回归仍有已记录失败，不能据本段主机检查宣称全套通过。
- Old / New 3DS 真机未验证。
