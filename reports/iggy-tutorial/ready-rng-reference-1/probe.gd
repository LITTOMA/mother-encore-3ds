extends SceneTree
func _init():
    var input=File.new()
    if input.open("res://input.json",File.READ)!=OK:
        quit(2)
        return
    var parsed=JSON.parse(input.get_as_text())
    input.close()
    if parsed.error!=OK:
        quit(3)
        return
    var ops=parsed.result.ops
    var output=File.new()
    if output.open("res://reference.bin",File.WRITE)!=OK:
        quit(4)
        return
    output.store_buffer("ERDY362\n".to_ascii())
    output.store_32(ops.size())
    var mirror=RandomNumberGenerator.new()
    seed(0)
    mirror.seed=0
    for index in range(ops.size()):
        var op=ops[index]
        var value=0.0
        var kind=0
        if op.kind=="Seed":
            seed(int(op.seed))
            mirror.seed=int(op.seed)
        elif op.kind=="Randi":
            kind=1
            value=randi()
            if value!=mirror.randi():
                quit(5)
                return
        elif op.kind=="Randf":
            kind=2
            value=randf()
            mirror.randi()
        elif op.kind=="RandRange":
            kind=3
            value=rand_range(op.low,op.high)
            var exponent=mirror.randi()
            if exponent!=0:
                mirror.randi()
                mirror.randi()
        else:
            quit(6)
            return
        output.store_32(index)
        output.store_32(kind)
        output.store_double(value)
        output.store_64(mirror.state)
    output.store_64(mirror.state)
    var actual_next=randi()
    var mirror_next=mirror.randi()
    output.store_32(actual_next)
    output.store_32(mirror_next)
    output.close()
    var meta=File.new()
    meta.open("res://engine.json",File.WRITE)
    meta.store_string(JSON.print(Engine.get_version_info()))
    meta.close()
    if actual_next!=mirror_next:
        quit(7)
    else:
        quit()
