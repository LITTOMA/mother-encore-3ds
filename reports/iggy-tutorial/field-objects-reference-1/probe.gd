extends SceneTree
var rows=[]
var live
func make_item(value):
    var parent=load("res://life.gd").new()
    parent.ready_flag=value
    var effect=AnimatedSprite.new()
    effect.name="Sparkles"
    var frames=SpriteFrames.new()
    for i in range(40):
        frames.add_frame("default",ImageTexture.new())
    frames.set_animation_speed("default",12.0)
    frames.set_animation_loop("default",true)
    effect.frames=frames
    effect.frame=22
    effect.playing=true
    effect.set_script(load("res://sparkles.gd"))
    parent.add_child(effect)
    get_root().add_child(parent)
    return parent
func snapshot(phase,tick):
    if is_instance_valid(live):
        rows.append({"phase":phase,"tick":tick,"alive":true,"visible":live.visible,"queued":live.is_queued_for_deletion(),"frame":live.get_node("Sparkles").frame})
    else:
        rows.append({"phase":phase,"tick":tick,"alive":false,"visible":false,"queued":true,"frame":-1})
func _init():
    call_deferred("run")
func run():
    for s in range(32):
        for flag in [false,true]:
            seed(s)
            var obj=make_item(flag)
            rows.append({"phase":"ready","seed":s,"flag":flag,"alive":true,"visible":obj.visible,"queued":obj.is_queued_for_deletion(),"frame":obj.get_node("Sparkles").frame,"next_randi":randi()})
            obj.free()
    seed(9)
    live=make_item(false)
    snapshot("initial",0)
    for tick in range(1,431):
        yield(self,"idle_frame")
        if tick==3:
            live.ready_flag=true
        snapshot("clock",tick)
    live.queue_free()
    snapshot("queue",430)
    yield(self,"idle_frame")
    snapshot("deleted",431)
    seed(9)
    live=make_item(true)
    snapshot("suppressed-ready",0)
    yield(self,"idle_frame")
    snapshot("suppressed-deleted",1)
    var f=File.new()
    if f.open("res://result.json",File.WRITE)!=OK:
        quit(2)
        return
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    f.close()
    quit()
