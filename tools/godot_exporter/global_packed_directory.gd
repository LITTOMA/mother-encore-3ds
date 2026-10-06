extends SceneTree
# Data-only original file projection; never enters original scenes or scripts.
func _init():
 var m = File.new()
 if m.open("res://manifest.json", File.READ) != OK:
  quit(1)
  return
 var manifest = parse_json(m.get_as_text())
 m.close()
 var extensions = ResourceLoader.get_recognized_extensions_for_type("")
 var pack = PCKPacker.new()
 if pack.pck_start("res://cache-source.pck") != OK:
  quit(2)
  return
 var rows = []
 for row in manifest:
  var extension = row.source.get_extension().to_lower()
  var included = row.source.ends_with(".yaml") or extension in extensions
  if included:
   if pack.add_file("res://" + row.source, "res://" + row.source) != OK:
    quit(3)
    return
  rows.append({"source":row.source,"sha256":row.sha256,"included":included,"size":row.size})
 if pack.flush() != OK:
  quit(4)
  return
 var out = File.new()
 if out.open("res://packed-source.json", File.WRITE) != OK:
  quit(5)
  return
 out.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"source_tree_entered":false,"recognized_extensions":Array(extensions),"files":rows},"  "))
 out.close()
 quit(0)
