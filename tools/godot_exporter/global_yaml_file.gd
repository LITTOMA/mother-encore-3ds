extends SceneTree
# Source compilation only; no original scene or autoload is constructed.
func typed(v):
    match typeof(v):
        TYPE_NIL: return {"kind":0}
        TYPE_BOOL: return {"kind":1,"value":v}
        TYPE_INT: return {"kind":2,"value":str(v)}
        TYPE_REAL:
            var buffer=StreamPeerBuffer.new()
            buffer.put_double(v)
            return {"kind":3,"f64_le":buffer.data_array.hex_encode()}
        TYPE_STRING: return {"kind":4,"value":v}
        TYPE_ARRAY:
            var a=[]
            for x in v: a.append(typed(x))
            return {"kind":5,"values":a}
        TYPE_DICTIONARY:
            var a=[]
            for k in v:
                if typeof(k)!=TYPE_STRING:
                    push_error("Unknown YAML source key")
                    quit(1)
                a.append([k,typed(v[k])])
            return {"kind":6,"entries":a}
    push_error("Unknown YAML source Variant")
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
    var rows=[]
    for source in manifest:
        var value=parser.parse_file("res://"+source)
        if value==null:
            push_error("Original YAML source compilation failed: "+source)
            quit(1)
            return
        rows.append({"source":source,"value":typed(value)})
    var output=File.new()
    if output.open("res://result.json",File.WRITE)!=OK:
        quit(1)
        return
    output.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"source_scene_entered":false,"documents":rows},"  ")+"\n")
    output.close()
    quit()
