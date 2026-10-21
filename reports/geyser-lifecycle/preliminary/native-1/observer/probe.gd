extends Node
var player
var holes=[]
var actors={}
var events=[]
var rows=[]
var counter=0
var tick=0
var first_gate=false
var tree_gate=false
var finishing=false
var config
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func register_created(actor):
    counter+=1
    actors[counter]=actor
    events.append(["created",counter])
    return counter
func observed_ready(actor):events.append(["ready",actor.observer_ticket,player.is_paused(),get_tree().paused])
func observed_finished(actor,name):events.append(["finished",actor.observer_ticket,name,actor.is_queued_for_deletion()])
func observed_exit(ticket):
    if !finishing:events.append(["exit",ticket])
    actors.erase(ticket)
func timeout(index):
    actors[counter].set_meta("hole",index)
    events.append(["timeout",index,counter])
    if !first_gate:
        first_gate=true
        player.set_observed_pause(true)
        events.append(["player_pause",true])
    elif tick>=220 and !tree_gate:
        tree_gate=true
        get_tree().paused=true
        events.append(["tree_pause",true])
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    process_priority=100
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    global.currentScene=self
    var objects=Node2D.new()
    objects.name="Objects"
    objects.pause_mode=Node.PAUSE_MODE_STOP
    add_child(objects)
    var factory=load("res://geyser.tscn")
    for index in config.holes.size():
        var binding=config.holes[index]
        var hole=load("res://hole.gd").new()
        hole.pause_mode=Node.PAUSE_MODE_STOP
        hole.interval=binding.interval
        hole.delay=binding.delay
        hole.position=Vector2(binding.position[0],binding.position[1])
        hole.poison=factory
        var timer=Timer.new()
        timer.name="Timer"
        timer.wait_time=8.0
        assert(timer.connect("timeout",hole,"_on_Timer_timeout")==OK)
        hole.add_child(timer)
        add_child(hole)
        assert(timer.connect("timeout",self,"timeout",[index])==OK)
        holes.append(hole)
func snapshot():
    var result=[]
    var ids=actors.keys()
    ids.sort()
    for id in ids:
        var actor=actors[id]
        assert(is_instance_valid(actor))
        var anim=actor.get_node("AnimationPlayer")
        var audio=actor.get_node("AudioStreamPlayer2D")
        var assigned=anim.assigned_animation
        result.append({"ticket":id,"hole":actor.get_meta("hole"),"position":[bits(actor.position.x),bits(actor.position.y)],"inside":actor.is_inside_tree(),"ready":actor.observer_ready,"time":bits(anim.current_animation_position) if assigned!="" else 0,"assigned":assigned,"playing":anim.is_playing(),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"queued":actor.is_queued_for_deletion(),"commands":audio.commands.duplicate(true),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen()})
        audio.commands.clear()
    var clocks=[]
    for hole in holes:clocks.append([bits(hole.timer.time_left),bits(hole.timer.wait_time),hole.timer.paused])
    var value={"actors":result,"clocks":clocks,"events":events.duplicate(true),"global_calls":audioManager.commands.duplicate(true),"player_paused":player.is_paused(),"tree_paused":get_tree().paused,"ui":[uiManager.battle,uiManager.cutscene,uiManager.game_over]}
    events.clear()
    audioManager.commands.clear()
    return value
func _process(delta):
    var row={"tick":tick,"delta":bits(delta),"before":snapshot(),"actions":config.operations.get(str(tick),[])}
    for action in row.actions:
        match action[0]:
            "player_pause":player.set_observed_pause(action[1])
            "tree_pause":get_tree().paused=action[1]
            "ui":
                uiManager.battle=action[1]==1
                uiManager.cutscene=action[1]==2
                uiManager.game_over=action[1]==3
            _:assert(false)
    rows.append(row)
    call_deferred("after",tick)
    tick+=1
    if tick==int(config.ticks):set_process(false)
func after(index):
    rows[index].after=snapshot()
    if tick==int(config.ticks):call_deferred("finish")
func finish():
    finishing=true
    get_tree().paused=false
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows,"spawn_count":counter}))
    file.close()
    get_tree().quit()
