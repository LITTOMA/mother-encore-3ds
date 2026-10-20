extends SceneTree
var target
var shakers=[]
var mirror=RandomNumberGenerator.new()
var count=0
var calls=[]
var rows=[]
var result=[]
func _init():call_deferred("run")
func real32(value):
    var buffer=StreamPeerBuffer.new()
    buffer.put_float(value)
    buffer.seek(0)
    return buffer.get_float()
func random_range(low,high):
    var value=rand_range(low,high)
    var raw=[str(mirror.randi())]
    if raw[0]!="0":
        raw.append(str(mirror.randi()))
        raw.append(str(mirror.randi()))
    count+=raw.size()
    calls.append({"value":"%.17f"%value,"raw":raw})
    return value
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var cases=JSON.parse(file.get_as_text()).result
    file.close()
    for test in cases:
        seed(test.seed)
        mirror.seed=int(test.seed)
        count=0
        calls=[]
        rows=[]
        shakers=[]
        target=load("res://target.gd").new()
        get_root().add_child(target)
        for tick in test.deltas.size():
            for action in test.actions:
                if action.tick==tick:
                    var shaker=load("res://shaker.gd").new(target,"shake_value")
                    shaker.set_physics_process(false)
                    shaker.runner=self
                    shaker.set_shake_direction(Vector2(action.direction[0],action.direction[1]))
                    shaker.set_shake_magnitude(action.magnitude)
                    shaker.set_shake_length(action.duration)
                    shaker.set_shake_interval(action.interval)
                    shaker.set_shake_weight(action.weight)
                    shaker.set_shake_diminish(action.diminish).start()
                    shakers.append(shaker)
            var before=target.shake_value
            for shaker in shakers:
                if shaker._shaked_object!=null:shaker._physics_process(real32(test.deltas[tick]))
            rows.append({"tick":tick,"before":[before.x,before.y],"after":[target.shake_value.x,target.shake_value.y],"raw_count":count,"state":str(mirror.state),"calls":calls.duplicate(true)})
            calls=[]
        var tail=[]
        for _i in range(3):
            var actual=str(randi())
            assert(actual==str(mirror.randi()))
            tail.append(actual)
        result.append({"name":test.name,"rows":rows,"tail":tail})
        target.free()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":result},"  "))
    file.close()
    print("CAMERA_SHAKER_REFERENCE_COMPLETE")
    quit()
