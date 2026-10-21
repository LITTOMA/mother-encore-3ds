extends Node
func _ready():call_deferred("run")
func run():
 var input=File.new()
 assert(input.open("res://areas.json",File.READ)==OK)
 var config=JSON.parse(input.get_as_text()).result
 input.close()
 var source=load("res://source.tscn").instance()
 add_child(source)
 var rows=[]
 for path in config:
  var area=source.get_node(path)
  var shapes=[]
  for child in area.get_children():
   var e=child.shape.extents
   var corners=[]
   for local in [Vector2(-e.x,-e.y),Vector2(e.x,-e.y),Vector2(e.x,e.y),Vector2(-e.x,e.y)]:
    var point=child.global_transform.xform(local)
    corners.append([point.x,point.y])
   shapes.append({"path":path+"/"+child.name,"disabled":child.disabled,"points":corners})
  rows.append({"path":path,"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable,"visible_in_tree":area.is_visible_in_tree(),"music":area.music,"loop":area.loop,"gain":area.volume_db,"fadein":area.fadein_length,"fadeout":area.fadeout_length,"appear":area.appear_flag,"disappear":area.disappear_flag,"disabled":area.disabled,"diegetic":area.diegetic,"shapes":shapes})
 var output=File.new()
 assert(output.open("res://../geometry.json",File.WRITE)==OK)
 output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
 output.close()
 source.free()
 get_tree().quit()
