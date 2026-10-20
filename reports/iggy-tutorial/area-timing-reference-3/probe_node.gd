extends Node2D
var tick=0
var rows=[]
var area
var shape
var rect
var rshape
var started=false
func _ready():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var p=config.policy
    var pivot=Node2D.new()
    pivot.position=Vector2(p[4],p[5])
    pivot.rotation_degrees=config.angle
    add_child(pivot)
    area=Area2D.new()
    area.collision_layer=2
    area.collision_mask=2
    area.position=Vector2(p[7],p[8])
    area.rotation=p[9]
    pivot.add_child(area)
    shape=CollisionShape2D.new()
    shape.position=Vector2(p[10],p[11])
    shape.rotation=p[12]
    shape.disabled=true
    var capsule=CapsuleShape2D.new()
    capsule.radius=p[13]
    capsule.height=p[14]
    shape.shape=capsule
    area.add_child(shape)
    rect=Area2D.new()
    rect.collision_layer=0
    rect.collision_mask=2
    rect.monitorable=false
    rect.position=Vector2(0,-10)
    add_child(rect)
    rshape=CollisionShape2D.new()
    var rectangle=RectangleShape2D.new()
    rectangle.extents=Vector2(8,6)
    rshape.shape=rectangle
    rect.add_child(rshape)
    rect.connect("area_entered",self,"entered")
    rect.connect("area_exited",self,"exited")
    started=true
func record(phase):
    rows.append({"tick":tick,"phase":phase,"disabled":shape.disabled,"overlaps":rect.get_overlapping_areas().size(),"attack_x":area.global_position.x,"attack_y":area.global_position.y,"physics":Engine.get_physics_frames()})
func entered(other):
    assert(other==area)
    record("entered")
func exited(other):
    assert(other==area)
    record("exited")
func _physics_process(_delta):
    if !started:return
    record("physics")
    tick+=1
func _process(_delta):
    if !started:return
    if tick==3:
        shape.disabled=false
        record("enable-idle")
    if tick==4:
        shape.disabled=true
        record("disable-idle")
    if tick==8:
        shape.disabled=false
        record("enable-idle")
    if tick==12:
        area.position.x+=200
        record("move-idle")
    if tick==16:
        area.position.x-=200
        record("return-idle")
    if tick==24:
        shape.disabled=true
        record("disable-inside-idle")
    if tick==28:
        shape.disabled=false
        record("reenable-idle")
    if tick==32:
        rshape.disabled=true
        record("disable-target-idle")
    if tick==36:
        rshape.disabled=false
        record("reenable-target-idle")
    record("idle")
    if tick>=41:
        var f=File.new()
        assert(f.open("res://result.json",File.WRITE)==OK)
        f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
        f.close()
        get_tree().quit()
