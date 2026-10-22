extends Node
var stats_events=[]
func stat_event(stat,value,max_value):stats_events.append(["stat",stat,value,max_value])
func status_event():stats_events.append(["status"])
func dialog(key,target,value=0,stat="",item=null):
    global.events.append(["dialog",key,target.get_name(),value,stat,item!=null])
func snapshot(member,item):
    var inventory=[]
    for value in member.inv.get_items():inventory.append(value.to_dict())
    return {"hp":member.get_hp(),"pp":member.get_pp(),"status":member.status_to_array(),"inventory":inventory,"selected_doses":item.doses,"selected_equipped":item.equipped}
func make_member(row):
    var member=load("res://Member.gd").new()
    member._name="Ninten"
    member._maxhp=row.maxhp
    member._maxpp=row.maxpp
    member._hp=row.hp
    member._pp=row.pp
    member._offense=3
    member._defense=5
    member._speed=5
    member._iq=5
    member._guts=5
    member.connect("stat_changed",self,"stat_event")
    member.connect("status_changed",self,"status_event")
    for status in row.status:member.add_status(status)
    var retained=member._status.duplicate()
    var before=Item.new("BaseballCap",true,1,1000)
    var item=Item.new(row.item,false,row.doses,2000+row.index)
    var after=Item.new("Fries",false,1,3000)
    assert(member.inv.add_item(before))
    assert(member.inv.add_item(item))
    assert(member.inv.add_item(after))
    globaldata.characters={"ninten":member}
    stats_events=[]
    global.events=[]
    return [member,item,retained]
func retire(parts):
    globaldata.characters.clear()
    for status in parts[2]:status.free()
    parts[0].free()
func _ready():call_deferred("run")
func run():
    var input=File.new()
    assert(input.open("res://input.json",File.READ)==OK)
    var cases=JSON.parse(input.get_as_text()).result.cases
    input.close()
    var target=load("res://Target.gd").new()
    target.connect("show_dialogbox",self,"dialog")
    var rows=[]
    for row in cases:
        var parts=make_member(row)
        var member=parts[0]
        var item=parts[1]
        var before=snapshot(member,item)
        var actions=member.inv.consume_item(item,member)
        var direct=snapshot(member,item)
        var direct_events=stats_events.duplicate(true)
        retire(parts)
        parts=make_member(row)
        member=parts[0]
        item=parts[1]
        target._messages_stack.clear()
        target.consume_item(member,item,{},member)
        var after=snapshot(member,item)
        if after!=direct or stats_events!=direct_events:
            print("CONSUME_COMPARISON "+JSON.print({"input":row,"direct":direct,"target":after,"direct_signals":direct_events,"target_signals":stats_events}))
        assert(after==direct)
        assert(stats_events==direct_events)
        var queued=[]
        if target._messages_stack.has(member):
            for text in target._messages_stack[member]:queued.append(JSON.parse(text).result)
        rows.append({"index":row.index,"before":before,"actions":actions,"after":after,"signals":stats_events.duplicate(true),"requests":global.events.duplicate(true),"queued_messages":queued})
        target._messages_stack.clear()
        retire(parts)
    target.free()
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    print("ITEM_CONSUME_REFERENCE_COMPLETE "+str(rows.size()))
    get_tree().quit()
