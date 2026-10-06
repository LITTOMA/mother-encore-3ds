extends SceneTree
# Source declaration conversion only. Original constructors/Ready are absent.
func typed(value):
    match typeof(value):
        TYPE_NIL: return {"kind":0}
        TYPE_BOOL: return {"kind":1,"value":value}
        TYPE_INT: return {"kind":2,"value":str(value)}
        TYPE_REAL:
            var b=StreamPeerBuffer.new()
            b.put_double(value)
            return {"kind":3,"f64_le":b.data_array.hex_encode()}
        TYPE_STRING: return {"kind":4,"value":value}
        TYPE_VECTOR2:
            var coordinates=[]
            for x in [value.x,value.y]:
                var b=StreamPeerBuffer.new()
                b.put_double(x)
                coordinates.append(b.data_array.hex_encode())
            return {"kind":7,"coordinates":coordinates}
        TYPE_ARRAY:
            var values=[]
            for x in value: values.append(typed(x))
            return {"kind":5,"values":values}
        TYPE_DICTIONARY:
            var entries=[]
            for key in value:
                if typeof(key)!=TYPE_STRING:
                    push_error("Non-string source Dictionary key")
                    quit(1)
                    return null
                entries.append([key,typed(value[key])])
            return {"kind":6,"entries":entries}
    push_error("Unsupported source declaration type")
    quit(1)
    return null
func _init():
    var holder=load("res://pure.gd").new()
    var values=[]
    for pair in holder.declaration_values(): values.append([pair[0],typed(pair[1])])
    var input=File.new()
    if input.open("res://manifest.json",File.READ)!=OK:
        quit(1)
        return
    var manifest=JSON.parse(input.get_as_text()).result
    input.close()
    var output=File.new()
    if output.open("res://result.json",File.WRITE)!=OK:
        quit(1)
        return
    output.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"source_scene_entered":false,"original_constructor_entered":false,"pure_sha256":manifest.pure_sha256,"values":values},"  ")+"\n")
    output.close()
    quit()
