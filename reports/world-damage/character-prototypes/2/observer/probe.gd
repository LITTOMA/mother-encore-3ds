extends Node
var events=[]
var retained=[]
func _ready():call_deferred("run")
func stat_changed(stat,value,max_value):events.append(["stat",stat,value,max_value])
func status_changed():events.append(["status"])
func remember(actor):
    for status in actor.get_status_ailments():
        if !retained.has(status):retained.append(status)
func capture(actor,case_id,index):
    remember(actor)
    var row={"case":case_id,"index":index,"hp":actor.get_hp(),"base_hp":actor.get_base_stat(Character.HP),"maxhp":actor.get_max_hp(),"status":actor.status_to_array(),"incapacitated":actor.is_incapacitated(),"scroll":actor.get_combined_status_effect("hp_scroll_multiplier"),"plate":actor.get_combined_status_effect("info_plate_color"),"world_damage":actor.get_combined_status_effect("overworld_damage"),"events":events.duplicate(true)}
    events.clear()
    return row
func run():
    var file=File.new()
    assert(file.open("res://cases.json",File.READ)==OK)
    var cases=JSON.parse(file.get_as_text()).result
    file.close()
    var rows=[]
    for case_id in cases.size():
        var spec=cases[case_id]
        var actor=load("res://member.gd").new()
        actor.init_from_dict({"name":"ninten","level":1,"exp":0,"hp":spec.hp,"maxhp":spec.maxhp,"pp":0,"maxpp":0,"offense":1,"defense":1,"speed":1,"iq":1,"guts":1})
        actor.connect("stat_changed",self,"stat_changed")
        actor.connect("status_changed",self,"status_changed")
        rows.append(capture(actor,case_id,-1))
        for index in spec.ops.size():
            var op=spec.ops[index]
            remember(actor)
            match op[0]:
                "hp":actor.set_hp(int(op[1]))
                "max":actor.set_stat(Character.MAXHP,int(op[1]))
                "add":actor.add_status(op[1])
                "remove":actor.remove_status(op[1])
            rows.append(capture(actor,case_id,index))
        actor.free()
        for status in retained:
            if is_instance_valid(status):status.free()
        retained.clear()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    get_tree().quit()
