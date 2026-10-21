extends SceneTree
var identities={}
var next_identity=0
var tree_exits=[]
func _init():call_deferred("run")
func identity(player):
    if not is_instance_valid(player):return -1
    var key=player.get_instance_id()
    if not identities.has(key):
        identities[key]=next_identity
        next_identity+=1
    return identities[key]
func record_exit(path):tree_exits.append(path)
func snapshot(manager,areas):
    var voices=[]
    for player in manager.get_audio_player_list():
        voices.append({"id":identity(player),"stream":player.stream.resource_path if player.stream else "","playing":player.playing,"gain":player.volume_db,"queued":player.is_queued_for_deletion()})
    var registered=[]
    for changer in manager.musicChangers:registered.append(changer.get_meta("path"))
    var states=[]
    for path in areas:
        var area=areas[path]
        states.append({"path":path,"inside":area.player_inside,"attached":identity(area.attached_player),"registered":area in manager.musicChangers})
    return {"voices":voices,"registered":registered,"areas":states,"exits":tree_exits.duplicate()}
func make_area(config,manager,context,ui,flags,owner):
    var area=load("res://changer.gd").new()
    area.set_meta("path",config.path)
    area.global=context
    area.audioManager=manager
    area.uiManager=ui
    area.globaldata=flags
    for field in ["music","loop","volume_db","fadein_length","fadeout_length","disabled","appear_flag","disappear_flag"]:area.set(field,config[field])
    var shape=CollisionShape2D.new()
    var rectangle=RectangleShape2D.new()
    rectangle.extents=Vector2(1,1)
    shape.shape=rectangle
    area.add_child(shape)
    area.connect("tree_exiting",area,"_on_MusicArea_tree_exiting")
    area.connect("tree_exiting",self,"record_exit",[config.path])
    owner.add_child(area)
    return area
func run():
    var input=File.new()
    assert(input.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(input.get_as_text()).result
    input.close()
    var streams=[]
    for settings in config.imports:
        var stream=load(settings.source)
        stream.loop=settings.loop
        stream.loop_offset=settings.offset
        streams.append(stream)
    var rows=[]
    for test in config.cases:
        identities={}
        next_identity=0
        tree_exits=[]
        var owner=Node.new()
        get_root().add_child(owner)
        var manager=load("res://manager.gd").new()
        manager.audioManager=manager
        var tween=Tween.new()
        tween.name="Tween"
        manager.add_child(tween)
        var players=Node.new()
        players.name="AudioPlayers"
        manager.add_child(players)
        owner.add_child(manager)
        var context=load("res://context.gd").new()
        var player=load("res://player.gd").new()
        context.player=player
        var ui=load("res://ui.gd").new()
        var flags=load("res://flags.gd").new()
        var other=Node2D.new()
        for child in [context,player,ui,flags,other]:owner.add_child(child)
        var body=player
        var scene=null
        var areas={}
        var views=[snapshot(manager,areas)]
        for action in test.actions:
            tree_exits=[]
            match action[0]:
                "scene":
                    if scene:scene.free()
                    areas={}
                    scene=Node.new()
                    owner.add_child(scene)
                    for binding in config.scenes[int(action[1])-1]:areas[binding.path]=make_area(binding,manager,context,ui,flags,scene)
                "enter":areas[action[1]]._on_Area2D_body_entered(body)
                "exit":areas[action[1]]._on_MusicArea_body_exited(body)
                "stop":areas[action[1]].stop_music(action[2])
                "door":
                    for changer in manager.musicChangers:changer.stop_music(action[1])
                "flag":flags.flags[action[1]]=action[2]
                "context":
                    body=player if action[1] else other
                    ui.cutscene=action[2]
                    ui.battle=action[3]
                    player.collisions=action[4]
                "external":
                    manager.add_audio_player()
                    var index=manager.get_latest_audio_player_index()
                    manager.play_music("",config.imports[int(action[1])-1].source.get_file(),index)
                    manager.set_audio_player_volume(index,action[3])
                    tween.set_active(false)
                    yield(self,"idle_frame")
                    yield(self,"idle_frame")
                    if not action[2]:manager.stop_audio_player(index)
                "settle", "flush":
                    yield(self,"idle_frame")
                    yield(self,"idle_frame")
                "idle":yield(self,"idle_frame")
                "finish":
                    tween.set_active(true)
                    yield(tween,"tween_all_completed")
            tween.set_active(false)
            views.append(snapshot(manager,areas))
        rows.append({"case":test,"views":views})
        scene.free()
        owner.free()
    var output=File.new()
    assert(output.open("res://../reference.json",File.WRITE)==OK)
    output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    quit()
