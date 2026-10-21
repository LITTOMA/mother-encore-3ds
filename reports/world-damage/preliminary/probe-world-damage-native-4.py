from pathlib import Path
import hashlib,json,subprocess,sys,re,argparse
r=Path.cwd();sys.path.insert(0,str(r))
from tools.extract_battle_entry import Extractor
from tools.run_scene_reference import invoke
ex=Extractor(r);a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);out=a.parse_args().out.resolve();assert out.is_relative_to(r/'build') and not out.exists();out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-world-damage').write_bytes(b'world-damage-reference-v1')
def method(text,name):
 m=re.search(r'^func '+re.escape(name)+r'\([^\n]*\n(?:[\t ].*\n|\n)*',text,re.M);assert m,name
 return m[0].rstrip()+'\n\n'
base=ex.text('Scripts/Main/party/party_object.gd');player=ex.text('Scripts/Main/party/Player.gd');scene=ex.text('Nodes/Reusables/Player.tscn');char=ex.text('Scripts/global/Character.gd').replace('func get_base_stat(stat: String, with_boosts := false) -> int:\n','func get_base_stat(stat: String, with_boosts := false) -> int:\n\tvar _observer_boosts=with_boosts\n');status=ex.text('Scripts/Main/Status.gd').replace('func get_status_message(ailment: String, message: String)', 'func get_status_message(_observer_ailment: String, message: String)').replace('get_ailment_data(ailment)\n\tif ailment_info', 'get_ailment_data(_observer_ailment)\n\tif ailment_info')
base_script='extends Node2D\nconst PAUSABLE_FLASH_ANIMS := ["Flash"]\nvar _party_member: Character\nvar _attack_damage := 0\nvar _damage_variance := 0\nvar _is_continuous_damage := false\nonready var _flash_anim=$FlashAnim\n'
for name in ['_is_invulnerable','damage','start_continuous_damage','stop_continuous_damage','play_flash_anim','pause_flash_anim','resume_flash_anim']:base_script+=method(base,name)
# Observer-only reads suppress original unused-argument diagnostics without changing calls.
base_script=base_script.replace('\n\t#if cant_kill','\n\tvar _observer_hit=hit_direct\n\t#if cant_kill').replace('\n\t_is_continuous_damage = true','\n\tvar _observer_hit=hit_direct\n\t_is_continuous_damage = true')
player_script='extends "res://base.gd"\nvar camera\nvar _paused := false\nvar _hit_direct := Vector2.ZERO\nvar _knockback := Vector2.ZERO\n'
for name in ['start_continuous_damage','damage']:player_script+=method(player,name)
start=player.index('\tif _is_continuous_damage and !_paused:');end=player.index('\n\t\n',start)
player_script+='func physics_tail():\n'+player[start:end]+'\n\nfunc game_over():\n\tglobal.events.append(["game_over"])\n\t_paused=true\n'
resources=''.join(re.search(r'^\[sub_resource type="Animation" id='+str(i)+r'\]\n.*?(?=^\[|\Z)',scene,re.M|re.S)[0] for i in [134,135])
actor='[gd_scene load_steps=5 format=2]\n[ext_resource path="res://player.gd" type="Script" id=1]\n[ext_resource path="res://sprite_holder.gd" type="Script" id=2]\n'+resources+'[node name="Actor" type="Node2D"]\nscript=ExtResource(1)\n[node name="Position" type="Node2D" parent="."]\n[node name="main" type="Node2D" parent="Position"]\nscript=ExtResource(2)\n[node name="SpecialAnimations" type="Node2D" parent="."]\nscript=ExtResource(2)\n[node name="FlashAnim" type="AnimationPlayer" parent="."]\nanims/Flash=SubResource(134)\nanims/RESET=SubResource(135)\nplayback_process_mode=2\n'
cases=[dict(name='ordinary',hp=10,maxhp=20,damage=4,variance=2,kill=False),dict(name='floor',hp=1,maxhp=20,damage=4,variance=2,kill=False),dict(name='healing_clamp',hp=19,maxhp=20,damage=-5,variance=0,kill=False),dict(name='incapacitated',hp=0,maxhp=20,damage=4,variance=2,kill=False,status='unconscious'),dict(name='zero_maxhp',hp=0,maxhp=0,damage=4,variance=2,kill=False),dict(name='lethal',hp=2,maxhp=20,damage=10,variance=0,kill=True),dict(name='zero_variance',hp=15,maxhp=20,damage=4,variance=0,kill=False)]
probe=r'''extends Node
var retained=[]
func _ready():call_deferred("run")
func bits(v):
    var b=StreamPeerBuffer.new()
    b.put_float(v)
    b.seek(0)
    return b.get_u32()
func color(v):return [bits(v.r),bits(v.g),bits(v.b),bits(v.a)]
func stat_changed(stat,value,max_value):global.events.append(["stat",stat,value,max_value])
func status_changed():global.events.append(["status"])
func capture(actor,label):
    for s in actor._party_member.get_status_ailments():
        if !retained.has(s):retained.append(s)
    var p=actor._flash_anim
    var row={"label":label,"hp":actor._party_member.get_hp(),"status":actor._party_member.status_to_array(),"current":p.current_animation,"assigned":p.assigned_animation,"playing":p.is_playing(),"time":bits(p.current_animation_position),"invulnerable":actor._is_invulnerable(),"main_alpha":bits(actor.get_node("Position/main").modulate.a),"special_alpha":bits(actor.get_node("SpecialAnimations").modulate.a),"main_modifier":bits(actor.get_node("Position/main").material.get_shader_param("flash_modifier")),"main_color":color(actor.get_node("Position/main").material.get_shader_param("flash_color")),"events":global.events.duplicate(true)}
    global.events.clear()
    return row
func create_actor(hp,maxhp):
    var member=load("res://member.gd").new()
    member.init_from_dict({"name":"ninten","level":1,"exp":0,"hp":hp,"maxhp":maxhp,"pp":0,"maxpp":0,"offense":1,"defense":1,"speed":1,"iq":1,"guts":1})
    var actor=load("res://actor.tscn").instance()
    actor._party_member=member
    actor.camera=load("res://camera.gd").new()
    actor.add_child(actor.camera)
    add_child(actor)
    member.connect("stat_changed",self,"stat_changed")
    member.connect("status_changed",self,"status_changed")
    global.party=[member]
    global.player=actor
    actor._flash_anim.play("RESET")
    actor._flash_anim.advance(0)
    actor._flash_anim.advance(0.01)
    global.events.clear()
    return actor
func retire(actor):
    actor._party_member.free()
    actor.free()
    for s in retained:
        if is_instance_valid(s):s.free()
    retained.clear()
func run():
    global.currentScene=self
    var file=File.new()
    assert(file.open("res://cases.json",File.READ)==OK)
    var cases=JSON.parse(file.get_as_text()).result
    file.close()
    var rows=[]
    for index in cases.size():
        var c=cases[index]
        var actor=create_actor(int(c.hp),int(c.maxhp))
        if c.has("status"):actor._party_member.add_status(c.status)
        capture(actor,"initial")
        seed(1)
        actor.damage(int(c.damage),int(c.variance),Vector2.ZERO,!c.kill)
        var row=capture(actor,c.name)
        row.rng_next=randi()
        rows.append(row)
        retire(actor)
    for scenario in ["complete","pause_zero","pause_positive","restart"]:
        var actor=create_actor(100,100)
        actor.play_flash_anim("Flash")
        rows.append(capture(actor,scenario+":play"))
        if scenario=="pause_zero":
            actor.pause_flash_anim()
            rows.append(capture(actor,scenario+":pause"))
            actor.resume_flash_anim()
            rows.append(capture(actor,scenario+":resume"))
        for index in 55:
            actor._flash_anim.advance(1.0/60)
            rows.append(capture(actor,scenario+":"+str(index)))
            if scenario=="pause_positive" and index==5:
                actor.pause_flash_anim()
                rows.append(capture(actor,scenario+":pause"))
                actor._flash_anim.advance(0.1)
                rows.append(capture(actor,scenario+":manual_paused"))
                actor.resume_flash_anim()
                rows.append(capture(actor,scenario+":resume"))
            if scenario=="restart" and index==20:
                actor.play_flash_anim("Flash")
                rows.append(capture(actor,scenario+":replay"))
        retire(actor)
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    get_tree().quit()
'''
classes=[dict(base='Object',**{'class':'Character'},language='GDScript',path='res://Character.gd'),dict(base='Node',**{'class':'Status'},language='GDScript',path='res://Status.gd'),dict(base='Reference',**{'class':'Item'},language='GDScript',path='res://Item.gd')]
config='config_version=4\n_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons={"Character":"","Status":"","Item":""}\n[application]\nconfig/name="Original world damage"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobaldata="*res://globaldata.gd"\nglobal="*res://global.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://audio.gd"\n'
global_script='extends Node\nvar events=[]\nvar party=[]\nvar player\nvar currentScene\nfunc start_joy_vibration(device,weak,strong,duration):events.append(["vibration",device,weak,strong,duration])\nfunc get_player():return player\n'+method(ex.text('Scripts/global/global.gd'),'get_conscious_party')
ui='extends Node\nfunc is_in_battle():return false\nfunc info_plates_show(a,b):global.events.append(["show",a,b])\nfunc info_plates_update(a):global.events.append(["update",a])\n'+method(ex.text('Scripts/global/uiManager.gd'),'create_flying_num')
# Actual FlyingNumber.run with a Label/AnimationPlayer holder;no original font/GPU admission.
flying=ex.text('Scripts/UI/Battle/FlyingNumber.gd');flying_scene='[gd_scene load_steps=3 format=2]\n[ext_resource path="res://FlyingNumber.gd" type="Script" id=1]\n[sub_resource type="Animation" id=1]\nlength=0.1\n[node name="FlyingNumber" type="Label"]\nscript=ExtResource(1)\n[node name="AnimationPlayer" type="AnimationPlayer" parent="."]\nanims/start=SubResource(1)\n'
files={'project.godot':config,'base.gd':base_script,'player.gd':player_script,'actor.tscn':actor,'Character.gd':char,'Status.gd':status,'Item.gd':'extends Reference\nclass_name Item\nfunc get_data() -> Dictionary:\n\tassert(false)\n\treturn {}\n','member.gd':'extends "res://Character.gd"\nfunc get_character_type() -> int:return Type.PARTY_MEMBER\n','global.gd':global_script,'camera.gd':'extends Node\nfunc shake_camera(magnitude,duration,direction):global.events.append(["camera",magnitude,duration,[direction.x,direction.y]])\n','audio.gd':'extends Node\nfunc play_sfx(stream,slot):global.events.append(["hurt",stream.resource_path,slot])\n','ui.gd':ui,'FlyingNumber.gd':flying,'Nodes/Ui/Battle/FlyingNumber.tscn':flying_scene,'globaldata.gd':'extends Node\nvar rows={}\nfunc _ready():\n\tvar file=File.new()\n\tassert(file.open("res://ailments.json",File.READ)==OK)\n\trows=JSON.parse(file.get_as_text()).result\n\tfile.close()\nfunc get_ailment_data(name):return rows.get(name,{})\nfunc does_ailment_exist(name):return rows.has(name)\n','ailments.json':json.dumps({name:ex.yaml('Data/StatusAilments/'+name+'.yaml')for name in ['poisoned','unconscious']}),'cases.json':json.dumps(cases),'probe.gd':probe,'probe.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource(1)\n'}
files['sprite_holder.gd']='extends Node2D\nvar material=load("res://material_holder.gd").new()\n'
files['material_holder.gd']='extends Reference\nvar values={"flash_modifier":0.0,"flash_color":Color.white,"glow_modifier":0.0,"glow_color":Color.white}\nfunc _get(property):\n\tassert(property.begins_with("shader_param/") and values.has(property.substr(13)))\n\treturn values[property.substr(13)]\nfunc _set(property,value):\n\tassert(property.begins_with("shader_param/") and values.has(property.substr(13)))\n\tvalues[property.substr(13)]=value\n\treturn true\nfunc get_shader_param(name):return values[name]\n'
copy='Audio/Sound effects/Hurt 1.mp3';t=project/copy;t.parent.mkdir(parents=True);t.write_bytes(ex.data(copy))
def write():
 for n,v in files.items():t=project/n;t.parent.mkdir(parents=True,exist_ok=True);t.write_bytes(v.encode())
write();proof={'sources':ex.sources,'generated_sha256':{n:hashlib.sha256(v.encode()).hexdigest()for n,v in files.items()},'scope':'Original damage/flash methods and clips,full Character/Status,actual FlyingNumber.run and create_flying_num.Explicit command-only camera/joy/audio/plates holders and shader-property Reference holders(no shader execution/GPU);synthetic Label holder has no font/GPU;selected player physics tail only,not movement or formal scene.'};(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');engine=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';assert hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd';invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'editor.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],out/'godot.log');assert all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items());raw=(out/'reference.json').read_bytes();(out/'receipt.json').write_text(json.dumps(dict(result_sha256=hashlib.sha256(raw).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),source=proof),indent=2)+'\n');print('Actual original damage/flash observations:',len(json.loads(raw)['rows']))
