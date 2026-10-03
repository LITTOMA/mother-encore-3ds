extends SceneTree
func _init():
 call_deferred("run")
func run():
 var scene = load("res://Blackbars.tscn").instance()
 get_root().add_child(scene)
 var player=scene.get_node("AnimationPlayer")
 var out=File.new()
 out.open("res://samples.tsv",File.WRITE)
 for clip in ["Open","Close"]:
  for step in range(301):
   player.play(clip)
   player.seek(step/600.0,true)
   out.store_line("%s\t%.9f\t%.9f\t%.9f" % [clip,step/600.0,scene.get_node("ColorRect").rect_position.y,scene.get_node("ColorRect2").rect_position.y])
 out.close()
 quit()
