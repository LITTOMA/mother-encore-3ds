extends SceneTree
# Data-only source parser export. No original autoload or scene is instantiated.
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
                    push_error("Unsupported native YAML Dictionary key")
                    quit(1)
                pairs.append([key,typed(value[key])])
            return {"kind":6,"entries":pairs}
    push_error("Unsupported native YAML Variant")
    quit(1)
    return null
func _init():
    print("Cache source exporter init")
    var input=File.new()
    if input.open("res://manifest.json",File.READ)!=OK:
        quit(1)
        return
    var manifest=JSON.parse(input.get_as_text()).result
    input.close()
    print("Cache source manifest ",manifest.size())
    var parser=load("res://yaml_parser.gd")
    print("Cache original parser loaded")
    var documents=[]
    for row in manifest:
        if row["role"]==4: continue # Items' actual fields have their own owner.
        print("Cache parse ",row["source"])
        var value=parser.parse_file("res://"+row["source"])
        if value==null:
            push_error("Invalid original YAML "+row["source"])
            quit(1)
            return
        documents.append({"source":row["source"],"value":typed(value)})
    print("Cache original parsed documents ",documents.size())
    var out=File.new()
    if out.open("res://result.json",File.WRITE)!=OK:
        quit(1)
        return
    out.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"source_scene_entered":false,"documents":documents},"  ")+"\n")
    out.close()
    quit()
