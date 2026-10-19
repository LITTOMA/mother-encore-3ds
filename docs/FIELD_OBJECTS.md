# 场景对象的独立资源与生命周期

当前只完成共享核心的独立数据加载与 DroppedItem 的进场、空闲帧及删除子集，尚未接入正式地图、取铲剧情、领取道具或 3DS 绘制。树根打断及 Pippi 救援仍待完成。C++ 不保存铲子的路径、旗标、位置、帧序列或调参数值；测试夹具从已记录的固定上游审查及真实纹理转换记录构造。

ENCFOB01 使用独立 format1 / capability1，五段分别保存文本、纹理身份及尺寸、稳定对象记录、矩形帧和动画序列。当前仅接受已映射的 DroppedItem 种类；交互模式必须为零，不能据绘制记录推导已实现的拾取行为。加载检查来源 pin、校验和、全部保留位、规范段布局、已消费内容、身份及路径唯一性、有限数值、动画范围与 Room 场景／旗标域。未知版本、能力、种类、交互和坏引用明确拒绝，失败不覆盖原对象。

prepare 只验证和建立拥有者，不消耗随机数。调用者必须按原场景树顺序逐个 enter_ready，并与其它节点的 Ready 交错；Sparkles 子节点先执行原版全局 rand_range，再由 DroppedItem 父节点检查进场旗标。即使父节点被旗标抑制而加入删除队列，子节点的随机调用仍发生。序列帧先截断整数，再按 AnimatedSprite 的帧数夹紧；没有另建随机流。

原 DroppedItem 的 hide、等待 idle_frame、show 与显式 queue_free 分开。原剧情设置 got_shovel 不会立即隐藏现场铲子；对象只在进场检查此旗标，或收到明确删除指令。queued 对象在删除队列提交前仍然存在。隐藏状态及玩家剧情暂停不停止其子 AnimatedSprite；时钟保留原 Godot 3.6.2 的 binary32 remaining / timeout，以及下一次处理发现 timeout<=0 才切帧的顺序。

原引擎探针使用未经修改的 sparkles.gd 和逐字提取的 DroppedItem._ready；隔离了旗标读取、Player 信号连接和纹理赋值依赖，不代表完整节点继承或原游戏运行。实际499个观察点包含32种种子在进场旗标真／假两种条件下的帧夹紧与后续 randi、430帧动画、延迟显示、运行中旗标改变及队列删除。来源、原始日志与脚本保留在 reports/iggy-tutorial/field-objects-reference-1。

GCC9.4 的 ASan/UBSan（泄漏检查）已通过元数据和生命周期检查；具体数量、初次及最终结果、编译文件指纹保留在 field-objects-host 记录。CMake 与控制台 Makefile 使用同一执行文件；Windows仅配置及方法收集，不宣称 Windows C++、当前交叉构建、GPU、模拟器或真机通过。Room、规则、Session、游戏存档及正式资源尚未改版；后续正式接线必须先完成对应资源迁移和负向测试。
