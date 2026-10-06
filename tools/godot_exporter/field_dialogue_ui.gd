extends SceneTree
var rows = []
func visit(node):
 if node is Control:
  var value = {"node": str(scene_root.get_path_to(node)), "minimum": [node.get_combined_minimum_size().x,node.get_combined_minimum_size().y]}
  if node is GridContainer:
   value.hseparation=node.get_constant("hseparation")
   value.vseparation=node.get_constant("vseparation")
  if node is HBoxContainer: value.separation=node.get_constant("separation")
  if node is Label or node is RichTextLabel:
   var f = node.get_font("font" if node is Label else "normal_font")
   value.font={"path": f.resource_path,"height": f.get_height(),"ascent": f.get_ascent(),"descent": f.get_descent()}
   value.line_separation=node.get_constant("line_spacing" if node is Label else "line_separation")
   if node is RichTextLabel:
    var style=node.get_stylebox("normal")
    value.normal_style={"minimum":[style.get_minimum_size().x,style.get_minimum_size().y],"offset":[style.get_offset().x,style.get_offset().y]}
  rows.append(value)
 for child in node.get_children(): visit(child)
var scene_root
func _init():
 scene_root=load("res://Nodes/Ui/DialogueBox.tscn").instance()
 visit(scene_root)
 var out=File.new()
 out.open("res://dialogue-ui-native.json",File.WRITE)
 out.store_string(JSON.print({"schema":1,"scene":"Nodes/Ui/DialogueBox.tscn","scene_entered":false,"engine":Engine.get_version_info(),"controls":rows},"\t"))
 out.close()
 scene_root.free()
 quit()
