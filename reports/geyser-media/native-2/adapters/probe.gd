extends Node
var actor
var player
var rows=[]
var finished=[]
var tick=0
var initial={}
var operations={}
var rumble_path
var named_slot
var active=false
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    process_priority=100
    set_process(false)
    call_deferred("setup")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func done(name):finished.append(name)
func snapshot():
    var anim=actor.get_node("AnimationPlayer")
    var audio=actor.get_node("AudioStreamPlayer2D")
    var result={"time":bits(anim.current_animation_position),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"queued":actor.is_queued_for_deletion(),"finished":finished.duplicate(),"commands":audio.commands.duplicate(true),"global_calls":audioManager.commands.duplicate(true),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen(),"ui":[uiManager.battle,uiManager.cutscene,uiManager.game_over]}
    audio.commands.clear()
    audioManager.commands.clear()
    return result
func setup():
    print("observer setup starting")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    operations=config.operations
    rumble_path=config.rumble
    named_slot=config.slot
    player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    actor=load("res://geyser.tscn").instance()
    actor.pause_mode=Node.PAUSE_MODE_STOP
    actor.position=Vector2(100,100)
    add_child(actor)
    var anim=actor.get_node("AnimationPlayer")
    assert(anim.connect("animation_finished",self,"done")==OK)
    actor.pause()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    player.set_observed_pause(false)
    initial=snapshot()
    print("observer setup ready:",initial)
    active=true
    set_process(true)
func _process(delta):
    if !active:return
    var row={"tick":tick,"delta":bits(delta),"tree_paused":get_tree().paused,"before":snapshot(),"actions":operations.get(str(tick),[])}
    if tick%50==0:print("observer tick:",tick," time:",row.before.time," paused:",row.tree_paused)
    # Record after real AnimationPlayer internal and source script process.
    # Actions affect the next idle frame;MessageQueue still sees CURRENT UI.
    for action in row.actions:
        match action[0]:
            "player_pause":player.set_observed_pause(action[1])
            "tree_pause":get_tree().paused=action[1]
            "visible":actor.visible=action[1]
            "ui":
                uiManager.battle=action[1]==1
                uiManager.cutscene=action[1]==2
                uiManager.game_over=action[1]==3
            _:assert(false)
    rows.append(row)
    call_deferred("after",tick)
    tick+=1
    if actor.is_queued_for_deletion():
        active=false
        set_process(false)
    if tick>300:
        active=false
        set_process(false)
func after(index):
    rows[index].after=snapshot()
    if !active:call_deferred("finish")
func finish():
    get_tree().paused=false
    var lifecycle=[]
    for old in audioManager.get_node("Sfx").get_children():old.free()
    get_tree().paused=true
    var node=audioManager.play_sfx(load(rumble_path),named_slot)
    lifecycle.append(sound_state(node,"born_paused"))
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    lifecycle.append(sound_state(node,"settled_paused"))
    get_tree().paused=false
    lifecycle.append(sound_state(node,"unpause"))
    get_tree().paused=true
    lifecycle.append(sound_state(node,"pause_existing"))
    node.free()
    node=audioManager.play_sfx(load(rumble_path),named_slot)
    lifecycle.append(sound_state(node,"reborn_paused"))
    get_tree().paused=false
    lifecycle.append(sound_state(node,"unpause_reborn"))
    node.pause_mode=Node.PAUSE_MODE_PROCESS
    get_tree().paused=true
    lifecycle.append(sound_state(node,"processable_paused"))
    get_tree().paused=false
    lifecycle.append(sound_state(node,"unpause_processable"))
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"initial":initial,"rows":rows,"tree_lifecycle":lifecycle},"  "))
    file.close()
    get_tree().quit()
func sound_state(node,phase):
    return {"phase":phase,"paused":node.stream_paused,"playing":node.playing,"can_process":node.can_process(),"manager_can_process":audioManager.can_process(),"mode":node.pause_mode,"bus":node.bus,"source":node.stream.resource_path}
