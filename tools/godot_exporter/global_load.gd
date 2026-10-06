extends SceneTree
# Original YAML data conversion only. No autoload/scene/LOAD is run.
func typed(value):
    match typeof(value):
        TYPE_NIL: return {"kind":0}
        TYPE_BOOL: return {"kind":1,"value":value}
        TYPE_INT: return {"kind":2,"value":str(value)}
        TYPE_REAL:
            var buffer=StreamPeerBuffer.new()
            buffer.put_double(value)
            return {"kind":3,"f64_le":buffer.data_array.hex_encode()}
        TYPE_STRING: return {"kind":4,"value":value}
        TYPE_ARRAY:
            var array=[]
            for entry in value: array.append(typed(entry))
            return {"kind":5,"values":array}
        TYPE_DICTIONARY:
            var pairs=[]
            for key in value:
                if typeof(key)!=TYPE_STRING:
                    push_error("Unsupported source Dictionary key")
                    quit(1)
                    return null
                pairs.append([key,typed(value[key])])
            return {"kind":6,"entries":pairs}
    push_error("Unsupported source Variant")
    quit(1)
    return null
func _init():
    var input=File.new()
    if input.open("res://manifest.json",File.READ)!=OK:
        quit(1)
        return
    var manifest=JSON.parse(input.get_as_text()).result
    input.close()
    var parser=load("res://yaml_parser.gd")
    var documents=[]
    for row in manifest:
        var parsed=parser.parse_file("res://"+row.source)
        if parsed==null:
            push_error("Rejected original save document "+row.source)
            quit(1)
            return
        documents.append({"source":row.source,"value":typed(parsed)})
    var output=File.new()
    if output.open("res://result.json",File.WRITE)!=OK:
        quit(1)
        return
    output.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"source_scene_entered":false,"documents":documents},"  ")+"\n")
    output.close()
    quit()
