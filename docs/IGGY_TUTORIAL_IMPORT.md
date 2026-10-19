# Iggy 挥棒教学移植

完整取铲来源候选通过6784项GCC ASan/UBSan：十个标签／68指令、五组双语文字、半秒路径等待、延迟删除、Iggy离场和只还原Ninten；两次核心实例最终结果一致。七项来源／负向检查、三项源文件表检查通过，正式Room与游戏资源保持不变。对象绘制、树根接触和正式回调仍未接线；同时记录了原教学缺少显式idle_frame交接的已知差异，正式整合时须补齐。详见 [取铲候选](GRAVEYARD_SHOVEL_IMPORT.md)。

场景对象删除指令已完成组件接线：Room14／49与独立对象拥有者支持场景／路径／存活预检及延迟删除，95项核心检查通过。已有三个Room的游戏数据完全不变，10项依赖／身份文件由原生成器更新，445项资源逐字不变；374项源资源核对、教学2334／Session20742／Restore811项消毒器回归通过。正式取铲节目、树根命中及绘制仍待接入，正式Room仍13。

新增场景对象独立元数据与Ready／空闲动画／队列删除组件：原引擎499观察点、GCC ASan/UBSan元数据187项及生命周期3022项、4项Python通过。尚未接入正式地图／Room删除指令／取铲或拾取。版本、来源及边界见 [场景对象](FIELD_OBJECTS.md)；本组件的完整构建另行核对。

2026-10-20 当前接线：完整 Iggy 教学已经正式生成，并以 f0296ba 的真实3DSX完成两次普通输入冷启动验收：一次直接答“是”，一次先“否／取消”再“是”，最终角色、库存和旗标投影一致，均只领取一次球棒，B可挥棒且结束后恢复移动。c95ade5 新增原引擎对照的接触时序组件，但尚未接入树根。c95完整GCC普通248／消毒器233和真实3DSX/CIA通过，Clang普通233／消毒器233也通过；f029的GCC普通247／消毒器232及Clang232／232均通过。树根Break/取铲、Pippi及完整游戏未完成，PR继续草稿。以下“候选／未接线”段落记录各历史组件，不代表当前发布范围。

墓地入口 PR #68 和测试调度 PR #70 已经用户分别授权合入 main，本分支包含该主线，PR #69 继续保留草稿。正式教学的来源、兼容和主机结果见“正式教学发布”节。

## 正式教学发布

普通输入模拟器证据见 reports/iggy-tutorial/ordinary-teaching-f029。实际游戏SHA为9493fd99…；两轮均从真实电话Record槽2普通LOAD，步行经过地图、触发教学、完成13／17次文本访问和1／3次选项，领取一次KEY并解锁bat，最终站位1432,4719，挥棒期间移动受阻、结束后可继续移动。两轮私有模拟器目录均已移除。这是人工市政厅后初始检查点的后续路线，仍不是自然新游戏全程、物理可听性或真机验收。

保留了所有失败：前两条接近路线被树挡住；第三轮完成教学但控制器把观察协议的发布次数误当60Hz物理帧；第四轮在启动前误把AwaitTimer的0时长当作StartWait的3秒。主程序用osGetTime毫秒推进空闲计时，发布次数不等于物理帧。已修正控制器，独立保留核心180物理帧及实际游戏包StartWait=3／AwaitTimer=0校验，没有改变游戏计时或旧trace。c95安装包回执的SHA与f029不同，不能把已测试的f029文件重标为c95；当前c95前端没有单独模拟器结果。新接触组件的原引擎与核心检查见 [接触时序](FIELD_AREA_CONTACT_TIMING.md)。

原 FieldStory 生成器的配方5加入已审查教学，复用同一83指令编译器、合并选项命名空间和双语文本，不再只生成 build 候选。原六个 Room 程序、表和字符串保持前缀完全相同；旧故事、遭遇和NPC交互语义相同。新增程序6、28行双语文本和选项组1。未知配方6、越界路径、坏来源和丢失拥有者仍拒绝，配方4继续读取。

Room 格式1／规则7不变，仅实际含 GrantKeyItem48 的 Room 使用能力13。FieldStory仍能力4，目录仍12和67项绑定。Session仍资源v11／既有存档格式，defaults逐字不变；只按原剧情22→29加入BatOld和bat完成策略。地图、Props、命名与Restore二进制完全不变。来源指纹通过原提取器、reuse-session及有严格AST／源素材／二进制／纹理一致证明的Props readmit更新，无手改包或重新伪造转换凭据。

实际 GCC9.4 ASan/UBSan（泄漏检查）通过：正式教学2334、独立候选2334、旧故事11690、Session20742、Restore811、墓地6230项。三条教学分支仅授予一次球棒，最终等待180帧；按 actor.gd:update_npcs 的空闲帧交接，在交接前拒绝挥棒、交接后可正常挥棒。首次测试过早输入的失败日志保留，没有通过提前释放玩家绕过原时序。37项来源测试、最后16项教学／故事测试和5项市政厅测试通过；374项来源准入通过；方法精确分配1191=826+365，Windows仅配置／收集。证据见 reports/iggy-tutorial/formal-publication-host。

7d2e552 的完整CI结果另行保留：GCC普通244/246、Clang230/231，旧Props目录11断言在a475修复，旧市政厅63项断言按四份实际动作元数据更新为67并检查角色覆盖；不能声称该次完整消毒器通过。真实3DSX/CIA和374项打包已成功。同一真实3DSX（SHA c64e7604…）两次独立冷启动，用实际电话Record槽2普通LOAD，检查教学前B门控、房屋／室外Goods、出入房屋及区域DSP持续推进，结果投影一致。首轮控制器错误地断言旧房屋音轨，失败观察与修复控制器均保留；游戏没有为此改动。证据见 reports/iggy-tutorial/player7d-emulator。检查点最初人工设置，尚不能代表自然开局全程、教学已跑通、物理可听性或真机。

来源为固定提交 7d9246600fffe518408f5830d4848635019005a3 的 gravedigger_swing.yaml、DialogueBox.gd、Actor.gd、BatOld、Podunk 场景及两种语言文本。16 段剧情编译为83条受检指令、14组双语文本和1组选项。是到18，否/取消到17，经玩家确认回16；现有初始 Iggy 选项保持组0，新教学使用组1。第22段先交付 KEY BatOld，再显示无说话人/无文字声音的领取提示；bat 旗标只在29段最后走位开始时写入。第8段的一秒最短等待与最终三秒自动等待分别保留，不能用一次确认跳过。

Room 格式仍1、规则7，capability13 的 GrantKeyItem 指令48由前一组件提供；旧12拒绝新指令，未知14、无实际交付的13、坏身份/数量/字段、未绑定或不支持的拥有者均拒绝。交付复用现有重要物品及 UID 事务。候选从冻结的旧 Room 经原编译器生成，原来六个程序、表和字符串前缀保持不变；候选只写 build/，不手改正式资源。本切片不变更 Session v11、存档格式、目录或正式资源。

快速确认对白暴露了演员改向错误：核心在旋转中拒绝直接 set_direction，原版则立即改向并保留已有 turn_to 的角度、目标和计时器。现已只去除该多余限制，非有限输入仍原子拒绝。官方 Godot3.6.2 的三个独立原函数场景共92个方向/旋转/完成信号观察与共享核心一致；没有修改原版源码或旧 trace。

本切片实际 GCC9 ASan/UBSan（启用泄漏检查）：教学2177项、改向298项通过；另12个既有 Room、Session、FieldStory、墓地、场景、门传送、排队转身、NPC和开局演员目标通过。教学三条路径分别显示13/15/15次文本，均只交付一次球棒、最终等待180帧并返回原版站位。6项 Python 方法通过；方法分配1138=774+364保持完整。来源、实际原引擎观察、候选来源凭据和测试日志见 reports/iggy-tutorial/actor-direction-reference-1 与 full-programme-host。新代码的完整 Linux CI、真实3DSX/CIA、模拟器和真机结果须按对应提交另行记录，不能用早先墓地 Record/LOAD 代替。

前一 KEY 组件：59项交付检查及 Room71939、Session20659、故事11690、墓地6229、场景37、门传送21通过；4项 Python 来源/编解码通过，原内容生成和368项来源验证通过。三个正式Room只更新编译来源指纹；Session、命名、地图、故事、Props、连接、桥头战斗八项输出逐字相同。Restore/Session通过原提取器更新必要依赖，命名通过原reuse-session处理。证据保留在 reports/iggy-tutorial/typed-key-grant。915daed真实3DSX/CIA构建已通过；其主机CI仍须检查最终结果。

下一步接通正式来源编译、两种语言/Session/菜单/音频拥有者、持久玩家的正确输入与挥棒绘制/时序、原版胶囊接触、树根Break与取铲回调，再推进Pippi救援。未完成这些接线前，候选和组件检查均不代表教学或完整墓地已可游玩。

挥棒几何组件：官方Godot3.6.2用原版胶囊及三层节点变换，对8×6树根矩形执行8766次查询，1775次重叠；共享核心GCC9 ASan/UBSan零差异。另测禁用接触、坏坐标/尺寸/角度和输出保持；原6499项/314观察仍通过，两项Python来源检查通过。原生JSON只改变小数拼写，real_t位完全相同，来源准入按float32位精确比较，未放宽碰撞期望。证据见 reports/iggy-tutorial/swing-contact-reference-2。这是几何组件，Area2D进入/退出的物理时序、正式挥棒与Break仍须接线。

紧凑挥棒图集候选保留两张原图各40个64×64完整帧，所有80个帧块（含透明像素）逐字相同，不修剪、缩放或重绘。原Sprite定位、居中和可见性来自来源记录。实际tex3ds RGBA8未压缩文件从两张合计8,388,650字节降到2,097,194字节；完整图基准转换后已清理，保留SHA和日志。6项Python（含错误来源、重复身份、路径与版本拒绝）通过，方法分配1146=782+364。相关原版许可已检查并记录。候选纹理只在build，未测量GPU/堆驻留；证据见 reports/iggy-tutorial/swing-graphics-atlas。

独立 ENCSGX01 格式1/capability1 的1061字节图形元数据现在由原转换凭据和实际两份T3X生成，记录40个原帧到紧凑图集的映射、两图层的来源身份、父子关系、可见性/翻转/居中、坐标与字节凭据。共享核心加载器原子校验版本、大小、目录、来源固定点、字符串、帧覆盖和资源身份，再与未改变的 ENCSWG01 动作资源绑定；纹理接口检查实际字节大小及CRC，不冒充T3X解析或加密认证。所有游戏帧、路径和定位均来自独立资源。桌面和3DS源列表均登记新模块，未改变存档、Session、规则或旧动作格式。

本地GCC9 ASan/UBSan（泄漏检查开启）2352项通过，包含全部截断、坏版本/目录/路径/帧/图层与失败后保持、错误动作绑定，以及实际两份T3X的校验和损坏拒绝。14项相关Python通过，方法分配1150=786+364。完整生成的元数据测试fixture和凭据见 reports/iggy-tutorial/swing-graphics-metadata。此组件尚无正式GPU绘制、原材质、输入或接触事件拥有者，未据此宣称教学可玩。

历史提交 af00c9c 的完整Clang普通及ASan/UBSan各215项、真实3DSX/CIA构建与打包通过，原始日志保留在 reports/iggy-tutorial/af-verification。其GCC作业在本条记录时仍运行；这些结果不代表其后的新图形元数据模块已通过完整CI。

GCC完整检查另暴露Windows生成凭据使用反斜杠的问题：ffb3b4f的普通检查228/229通过，聚合Python中演员方向来源凭据在Linux无法定位四个工具路径，sanitizer未执行。三个相关生成器改为明确输出POSIX相对路径，再以真实Godot和tex3ds重新生成凭据；未手改旧凭据、原版源码或行为期望。新92行方向表、两份T3X、1061字节元数据与原结果逐字相同，Windows相关16项通过；实际Linux核对旧四路径缺失、新路径及来源SHA全部匹配。新凭据与审查见 actor-direction-reference-2、swing-graphics-atlas-2、swing-graphics-metadata-2；旧凭据和失败日志保留。完成的本地测试ELF与已证明相同的旧候选纹理共51,150,322字节已清理，当前纹理、游戏、存档和来源证据保留。

挥棒动作拥有者进一步对照了真实原版逻辑输入方法及 Bat/Idle 动画树分支。官方Godot两次独立运行各25组、683个观察，稳定投影逐字相同；原版方法、动画、混合空间、速度节点及两个直接转场资源保持原字节，场景只取该受限分支，单例与单人技能由测试夹具提供。记录包含八向、正常/异常速度、攻击中重复输入、未习得技能、暂停、进门、攀爬、旋转、非移动状态、松开及其他按键、无效旧技能选择，以及结束后的再次挥棒。UI取消键触发技能，跑步键不触发挥棒；第一动画树处理只提交转场，后续处理才执行动作键；结束时先恢复Idle显示，延迟方法执行后才恢复移动。真实音频播放器的异步playing属性保留原始观察，但不属于稳定结果投影或可听性声明。

新 ENCSWA01 格式1/capability1 的389字节独立资源记录输入/技能/状态身份、准入条件、开始/结束清除字段、八向Idle帧与可见性及普通图像帧数。共享核心执行器复用原 ENCSWG01 稀疏动作及其延迟完成方法，提供单次成功触发输出供后端执行原声音/震动，执行器不含游戏路径、帧表或数值调参。旧动作、图形格式、Session、存档和正式资源未改；新模块已登记桌面及3DS构建。GCC9 ASan/UBSan（泄漏检查开启）3125项通过，包含683项原引擎观察、坏格式/跨度/版本/方向/字段及原子拒绝、重复和延迟控制；6项Python含来源、双运行幂等、未知源轨道和编解码拒绝通过，分配1156=792+364。额外开启Wall/Wextra/Wpedantic的新模块编译无诊断。证据见 reports/iggy-tutorial/swing-action-reference-5、swing-action-reference-6、swing-action-host。它仍是候选组件，实际主循环、物理接触、GPU材质/绘制、SE后端和模拟器教学尚待接线，不能由组件检查推定这些已完成。
原版进入挥棒的路径已扩展为独立 ENCSWE01 格式1/capability1 候选：8个状态、56个方向动画和169个帧键，共4087字节。Run依次经过Walk、Idle，Walk/Blink/FaintedIdle经Idle，FaintedWalk经FaintedIdle、Idle；每条边消耗一次动画树处理。Crouch到Idle是原版AtEnd转场，等待源动画结束。Walk与Bat不同的混合方向坐标、Blink的20秒循环，以及原Idle动画中6个超过片长而不可达的键均有显式来源记录，未以忽略未知内容处理。

官方Godot3.6.2两次独立运行各80组、6560个观察，稳定结果逐字相同；完整18节点原拓扑单独求路径，动画夹具保留8个相关原分支，未替代完整Player物理循环。共享核心GCC9 ASan/UBSan共28186项通过，包含全部截断、未知版本、坏图跨度/循环/目标、动画帧与原子快照拒绝；6项Python通过，分配1162=798+364，新模块Wall/Wextra/Wpedantic编译无诊断。证据见 swing-entry-reference-4、swing-entry-reference-5、swing-entry-host。资源、主循环、持久动作拥有者、GPU、声音后端和接触事件仍未接通，完整教学与真机仍未验证。

历史CI的精确结果：ec48b48的GCC普通230/231通过，聚合python_tools在1800.09秒超时，GCC sanitizer未执行；72f02ab同样231/232通过，聚合在1800.01秒超时。72f02ab的Clang普通217项及ASan/UBSan217项、真实3DSX/CIA均通过。负面调度测试中有意触发的worker失败不是实际失败原因。原始日志保存在 action-ci-verification；当前逐项耗时诊断仍在进行，未删除测试、扩大超时或宣称完整CI全绿。

持久动作交接现在提供受检的rebase接口：仅在未攻击且无延迟完成回调时采用实际移动快照，保留稀疏球棒帧、角度、前后图层、已选技能及累计启动次数；未知状态、坏方向/时钟、攻击中和结束回调未完成时原子拒绝。不会通过重建玩家重置这些值，也不在逐帧路径复制资源。原引擎两次各8组连续保留玩家、80次动作、6560个观察，稳定投影逐字相同；GCC9 ASan/UBSan 20044项通过，原进入动作28186项及直接动作3125项回归通过，14项相关Python通过，方法分配1164=800+364。证据见swing-rebase-reference-3、swing-rebase-reference-4与swing-rebase-host。该接口仍待实际PersistentPlayerState/主循环采用，未宣称正式教学、模拟器或真机通过。

资源目录的下一步接线已加入capability12的严格准入：FieldSwing=45、FieldSwingGraphics=46、FieldSwingAction=47、FieldSwingEntry=48为四个全局单例角色，必须完整出现，ID固定且类型/路径各自校验，并要求已有FieldProps拥有者。目录格式仍1，旧1～11的读入分支保留；目录capability12与Room指令capability13、各动作资源格式及Session/存档独立。正式目录和全部正式资源均未改变；3288字节cap12测试fixture由实际编译器以明确的合成fingerprint生成，不能作为正式内容或文件通过证明。GCC9 ASan/UBSan新目录6650项、既有真实目录和原BattleRound重定位回归7016项通过；24项Python通过，含正式目录逐字再生成，以及旧8/10、坏新组、身份/类型/版本和全部截断；方法分配1169=805+364。证据见swing-catalog-host，完整CI与实际资源组加载仍待后续接线验证。

四资源现由共享核心FieldSwingResources统一从类型目录加载。元数据拥有者仅在四包均通过指纹/格式以及SWG↔图形、动作、进入路径的交叉绑定后发布；已发布拥有者不可重新加载，失败候选没有可见的部分内容。GCC9 ASan/UBSan 111项通过，覆盖四份实际冻结原版数据包、缺文件、CRC损坏、文件与目录双重重签后仍拒绝的未知版本、各自可解析但互不兼容的动作/进入路径、不可变发布及实际动作执行/再次移动交接。19项相关Python通过，方法分配1169=805+364。fixture由编译器使用实际字节生成目录，无新纹理/音频；正式资源未改变，GPU与物理/主循环仍待接线。日志、当前全部核心指纹和fixture包SHA见swing-resource-owner。

接入实际World暂停时发现现有单一attacking标志不足以复现原版。原Player.pause立即把控制状态改回MOVE、停用自身音效/暂停其流与MiscTimer、清除移动字段并请求Idle，但Bat非循环动画仍可继续，命中形状也不因pause直接关闭；暂停前尚未进入Bat的请求则可能被Idle覆盖。很快unpause后再次按B会再发出声音/震动请求，若当前仍是Bat，其播放进度不归零。只把动画冻住或重建动作都会改变游戏结果。原版仅把Walk/Run/FaintedWalk当前节点的TimeScale设0，unpause恢复1；异常Idle等其他节点并非统一冻结。

官方Godot两次各28组/3556个原函数观察的稳定投影逐字相同。分别查询完整18状态拓扑及8状态动画子图，8个来源状态到Idle/FaintedIdle的16条路径在两种拓扑中一致。原Player.pause/unpause及PartyObject的动画暂停、set_idle/set_anim_state/get_paused_anim_state原方法保持原字节；party服务、AreaRoom类型守卫、碰撞掩码/事件查询边界与MiscTimer由明确夹具提供。2项Python来源/方法指纹、双运行、实际控制/时钟结果和输出目录拒绝通过，分配1171=807+364。证据见swing-pause-reference-4、swing-pause-reference-5、swing-pause-audit；早期夹具失败未当成通过结果，音频playing异步字段只保留原始观察、不列入稳定投影。此为新发现的必要实现依赖，不能把现有动作组件或元数据拥有者据此记为已实现暂停或已接通实战。

完整八状态转移现编译为 ENCSWE01 格式1/capability2：保留旧五段动画数据，新增64条受检路径、异常Idle角色、自动转移和逐状态暂停策略；旧capability1读取保留，未知3明确拒绝。两次真实Godot查询中，完整18状态图和八状态子图的全部64条路径一致且闭合；这只是拓扑查询，未据此宣称执行了全部玩家方法。来源生成器、绑定和原始查询见 field-player-tree.json、field_player_tree.py 与 player-state-paths-1/-2；正式RomFS仍未改变。

共享FieldPlayerTree将控制ATTACK与当前动画独立保存：pause释放控制但保留已运行Bat及其命中形状，尚未进入的Bat请求可被Idle覆盖；unpause后再次输入同一Bat不会重置动画时钟，重复开始只更新来源速度和开始次数。Walk/Run/FaintedWalk冻结与恢复、正常/异常Idle路径、Bat自动结束和延迟回调均使用受检数据。原离散帧工具在循环边界保留length用于访问末尾键，新树在访问后把循环时钟归零，避免反复从length相加带来的浮点漂移；符合[Godot AnimationNodeAnimation](https://github.com/godotengine/godot/blob/3cd3caab/scene/animation/animation_blend_tree.cpp)与[StateMachine的剩余时长计算](https://github.com/godotengine/godot/blob/3cd3caab/scene/animation/animation_node_state_machine.cpp)，未改旧期望或旧帧工具。

实际GCC9 ASan/UBSan（泄漏检查开启）42461项通过：28组/3556个原暂停、恢复、重复输入观察，以及未改变的80组/6560个进入挥棒观察；含全部截断、旧/新版本、角色/目录/路径循环/边/绑定负向准入。旧进入组件28186项、四资源拥有者111项和三个受影响核心单元严格编译通过；相关Python13项通过，方法分配1176=812+364；Windows仅重新配置CMake成功，没有据此宣称Windows编译或执行通过。原版可听音频、碰撞掩码、完整物理、World/GPU接线和模拟器仍待完成。证据见 player-tree-host；测试驱动末尾的Python3.8清理API错误与实际测试通过分开保留，没有把失败驱动记为通过。

#70测试调度修复经用户逐项授权已于2026-10-19合入main，合并提交7163a65；#69保留草稿并同步该主线。#70的d81bfb3完整运行39251936650成功：GCC普通224/ASan+UBSan209、Clang普通209/ASan+UBSan209、真实3DSX/CIA；#69历史4d4bcc3完整运行39252074017也成功：GCC普通234/ASan+UBSan219、Clang普通219/ASan+UBSan219、真实3DSX/CIA。实际原始日志和完整运行身份保存在scheduling-full-verification；两者都不覆盖其后的资源组加载、暂停对照及新树改动。

持久玩家已加入可选的实际World动作绑定：四资源拥有者必须先发布完整cap2图，Room的32个普通移动绑定与可执行帧曲线逐项相符后才绑定；旧cap1拥有者可读但在此处明确拒绝。Room的角色4是独立的Cutscene Actor walk，仍由原Room/Actor校验；Idle Right的六个越过循环长度的已知帧键显式不可达，比较完整可达曲线而非截取前缀。改动可达帧会拒绝绑定，失败保持原玩家。正式目录与资源尚未启用该绑定。

World在控制ATTACK期间保留位置、速度和步态快照，跳过MOVE/碰撞恢复查询，继续独立区域信号与idle动画；强制剧情pause可以打断攻击。音效开始/暂停/恢复通过有界事件队列按原顺序交付，尚无新DSP播放。ScenePlayerTransfer携带持久图状态并校验双方动作修订号，拒绝没有物理/idle帧变化的过期动作快照；旧场景不能再消耗或写动作事件。交互朝向只更新Idle混合参数，正常set_direction更新全部参数。原引擎还证明，离散方向切换把Bat时钟移回零时，StateMachine将其视作end_loop并可立即走AtEnd边，即使非循环片段尚未播完；该过渡保留旧可见性直到下一次目的节点处理，不能直接套用普通结束姿态。

原版set_direction/blend_position/_turn_to及PartyObject.blend_animation方法保持原字节。两次官方Godot各14组/2058个观察的稳定投影与TSV逐字相同；共享核心GCC9 ASan/UBSan（泄漏检查开启）6232项完全对齐。World626项覆盖实际移动、B逻辑输入、保留速度、延迟结束、暂停/重复按键、顺序事件、旧cap1拒绝、帧曲线不匹配、队列边界及持久换场；攻击中仍进入原Lamp区域剧情，完成原13段演出并请求战斗，音频请求仍为5次。旧静态树42461项、World原Lamp路线、持久拥有者42项、旧换场442项通过；相关Python16项通过，分配1179=815+364；Windows仅CMake配置。首次方向夹具的重复方法解析失败及55秒超时日志已无损归档，没有当成原版行为或通过结果。证据见player-motion-reference-2/-3和player-action-world-host。

该组件尚未接入正式HID/START/A归属、GPU和玩家独立音效，树根接触/Break/取铲仍待完成；实际游戏仍无完整教学或新模拟器验收。UI菜单/A应按原MOVE控制状态分别门控，不能为方便接线而把所有强制剧情pause或Area事件禁止在攻击中。b0d550d的完整运行39266888939已成功：GCC普通239/ASan+UBSan224、Clang普通224/ASan+UBSan224及真实3DSX/CIA；原始日志见player-tree-b0-full-verification。这是本条World改动之前的来源，不能替代其完整CI或模拟器检查。

## Persistent player sound backend (2026-10-20)

ENCSWA01 capability 2 adds the actual Ninten Bat source SHA, SFX bus, gain, pitch and process pause policy; capability 1 remains readable. Defaults were reviewed against official Godot 3.6.2 AudioStreamPlayer (3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8). The source sound already exists in the opening bank; no PCM conversion or asset duplication is required.

The persistent player voice reserves its own pooled channel and bounded buffers before input. Ordered World events start, stop or resume that voice, retaining failed events for retry and retaining identity across scene handoff. Ordinary repeated swings restart this voice. Pausing it and changing its SFX volume preserve BGM and run queues. Existing audio trace remains TRACE 529 71a30145b163903b.

GCC9 ASan/UBSan with leak checking: data 979, new NDSP double 306, legacy regression 1096, old NDSP double 1445, run/region 983, World 626 checks; Python 4 tests passed. CMake configuration and exact Python partition 1183=819+364 passed (not a Windows build/test claim). Raw logs, source hashes and drivers are in reports/iggy-tutorial/player-audio-host. This is backend preparation: formal content, HID, GPU, temporal root contact and ordinary-input emulator acceptance are still pending. No audible output or hardware equivalence is claimed.

## 正式玩家资源与前端接线（2026-10-20）

生成器从实际tex3ds凭据重建独立SWG1、SGX1、SWA capability2和SWE capability2，四份元数据加两张纹理合计2,106,624字节，音效复用原bank。目录capability12使用45–48稳定角色；旧capability11通过迁移前原编译器生成的独立夹具保留读取，未手改pack。游戏规则和存档格式不变。正式构建任务新增独立field-player来源准入，最终目录等待它完成。

来源SpecialAnimations的Outline.shader、材质width=0/inside=false/pattern=0与原版swing分支已审查；该零宽材质是纹理恒等采样，其他描边状态明确拒绝。纹理不预旋转，pivot只用于受检碰撞。绘制沿玩家原排序槽，球棒遵循show_behind_parent，子节点继承父位置但不继承Sprite绘制offset；主Sprite隐藏时独立Shadow仍绘制。持久纹理不借用临时场景Player；失败纹理准入清理已分配资源。3DS没有原版手柄震动的对应输出，未声称该功能等价。

共享World保留MOVE/ATTACK与原速度，B只分发一次逻辑动作；X对应原toggle，菜单关闭的B不泄漏到技能。主程序不把ATTACK加入输入拥有者，因此持有方向在动作结束后可继续移动。强制剧情和Area暂停仍由既有世界生命周期处理。LOAD/新游戏重置玩家音效拥有者，普通换场保留持久身份；音频场景重置后在下一输入前重新保留原音效通道与已有缓冲。当前NativeSession仅接受空状态集合，其他角色/状态不以默认值假装支持。

针对性GCC9 ASan/UBSan（泄漏检查开启）：绘制2450、实际FreshHouse/Record/LOAD 18155、既有Props绘制33、目录7419、旧目录6650、资源拥有者111；更新音效314及数据979通过。Python第一组40项、随后更新来源/音效10项通过；374项正式资源通过，方法分配1189=825+364保持完整，Windows仅配置/收集。首次绘制编译因测试替身缺少实际API默认参数与测试uint16转换失败，修复测试接口并保留失败日志，没有改玩法或图像期望。证据见reports/iggy-tutorial/player-presentation-host。主程序HID、真实GPU/声音、普通输入模拟器及真机不能由这些替身测试推定通过，PR仍为草稿。

183f6b8完整检查39277979484已全部成功，实际GCC/Clang普通及ASan/UBSan和真实3DSX/CIA日志在player-world-183-full-verification；它仅验证前一World组件，不能代替本节后续接线。
