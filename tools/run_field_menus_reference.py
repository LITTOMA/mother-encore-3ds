#!/usr/bin/env python3
"""Bounded original inventory methods and volume curve, in official Godot 3.6.2.

Uses explicit data/character adapters; not a full original game or UI run.
"""
import argparse,hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.run_battle_victory_reference import function,sha

ITEM='''extends Reference
enum ItemActions { HP_UP, PP_UP, HP_MAX, PP_MAX, STAT_UP, HEAL, HEAL_FAIL }
var item_name=""
var equipped=false
var doses=1
var uid=0
var data={}
func get_data():return data
func is_equippable():
	for action in data.get("actions",[]):
		if action.function=="equip":return true
	return false
'''
CHARACTER='''extends Reference
const MAXHP="maxhp"
const MAXPP="maxpp"
const OFFENSE="offense"
const DEFENSE="defense"
const SPEED="speed"
const IQ="iq"
const GUTS="guts"
var hp=25
var pp=26
func get_hp():return hp
func set_hp(value):hp=value
func get_pp():return pp
func set_pp(value):pp=value
func get_stat(_stat):return 30
func has_status(_status):return false
func remove_status(_status):assert(false)
func apply_boosts(boosts,_actions):
	for value in boosts.values():assert(value==0)
'''
PROBE='''extends SceneTree
func _init():call_deferred("run")
func snapshot(inv):
	var rows=[]
	for item in inv._items:rows.append({"item_id":item.item_name,"equipped":item.equipped,"doses":item.doses,"uid":item.uid})
	return rows
func run():
	var output=""
	for arg in OS.get_cmdline_args():
		if arg.begins_with("--out="):output=arg.substr(6)
	var file=File.new()
	assert(file.open("res://fixture.json",File.READ)==OK)
	var fixture=JSON.parse(file.get_as_text()).result
	file.close()
	var script=load("res://inventory.gd")
	if !script.can_instance():
		quit(1)
		return
	var inv=script.new()
	print("INVENTORY_LOADED")
	var result={}
	for row in fixture.items:
		var item=load("res://item.gd").new()
		item.item_name=row.id
		item.data=row.data
		item.equipped=row.equipped
		item.doses=row.data.get("doses",1)
		item.uid=row.uid
		inv._items.append(item)
	inv.sort_auto()
	print("INVENTORY_SORTED")
	result.sorted=[]
	for item in inv._items:result.sorted.append(item.item_name)
	result.sorted_items=snapshot(inv)
	var spray=inv._items[0]
	assert(spray.item_name=="AsthmaSpray")
	var receiver=load("res://character.gd").new()
	var actions=inv.consume_item(spray,receiver)
	print("INVENTORY_CONSUMED")
	assert(actions[6]==["asthma"])
	result.consume_fail={"doses":spray.doses,"hp":receiver.hp,"pp":receiver.pp,"count":inv._items.size(),"uid":spray.uid}
	result.consumed_items=snapshot(inv)
	var first_uid=inv._items[0].uid
	var last_uid=inv._items[-1].uid
	inv.switch_items(0,inv._items.size()-1)
	assert(inv._items[0].uid==last_uid and inv._items[-1].uid==first_uid)
	result.manual_preserves_uid=true
	inv.switch_items(0,inv._items.size()-1)
	inv.consume_item(spray,receiver)
	inv.consume_item(spray,receiver)
	assert(!inv._items.has(spray))
	result.exhausted_count=inv._items.size()
	result.exhausted_items=snapshot(inv)
	var cap=null
	for item in inv._items:
		if item.item_name=="BatPlastic":cap=item
	assert(cap!=null)
	assert(inv.drop_item(cap) and !cap.equipped)
	result.dropped_count=inv._items.size()
	result.dropped_items=snapshot(inv)
	var options=load("res://options.gd").new()
	result.volume_db=[]
	for units in range(11):
		var db=options._volume_units_to_db(units)
		assert(options._volume_db_to_units(db)==units)
		result.volume_db.append(db)
	assert(file.open(output,File.WRITE)==OK)
	file.store_string(JSON.print({"engine":Engine.get_version_info(),"kind":"field-menus","cases":result},"  "))
	file.close()
	print("FIELD_MENUS_REFERENCE_COMPLETE")
	quit()
'''

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    out=a.out.resolve();require(not out.exists() and out.is_relative_to(ROOT/'build/emulator-regression'),'Fresh owned reference output required')
    out.mkdir(parents=True);work=out/'temporary';work.mkdir();ex=Extractor(ROOT);methods={}
    def extract(path,names):
        source=ex.text(path);parts=[]
        for name in names:
            code=re.split(r'^class ',function(source,name),maxsplit=1,flags=re.M)[0].rstrip()+'\n';methods[path+':'+name]=hashlib.sha256(code.encode()).hexdigest();parts.append(code.replace(':=','='))
        return '\n'.join(parts)
    path='Scripts/global/Inventory.gd';source=ex.text(path)
    coeffs=re.findall(r'^const BOOSTABLE_STATS_COEFFS := \{.*?^\}',source,re.M|re.S);require(len(coeffs)==1,'Source boost coefficients missing')
    inventory='extends Reference\nvar _items=[]\nconst SLOTS=["weapon","body","arms","other"]\n'+coeffs[0].replace(':=','=')+'\n'
    inventory+=re.sub(r':\s*(Item|Character)(?=\s*[,)=\n])','',extract(path,['get_items','drop_item','reduce_or_drop_item','_sort_auto','sort_auto','switch_items','consume_item','get_boost_total']))
    character=ex.text('Scripts/global/Character.gd')
    for name in ('maxhp','maxpp','offense','defense','speed','iq','guts'):
        require('const '+name.upper()+' := "'+name+'"'in character,'Unknown Character stat identity')
        inventory=inventory.replace('Character.'+name.upper(),'"'+name+'"')
    actions=re.findall(r'^enum ItemActions \{([^}]+)\}',ex.text('Scripts/global/Item.gd'),re.M)
    require(len(actions)==1,'Unknown item action enum')
    action_names=[name.strip()for name in actions[0].split(',')]
    inventory=re.sub(r'Item\.ItemActions\.([A-Z_]+)',lambda match:str(action_names.index(match[1])),inventory)
    options='extends Reference\nconst VOLUME_GRADES=11\n'+extract('Scripts/UI/Options/OptionsUI.gd',['_volume_units_to_db','_volume_db_to_units'])
    # Exact data comes from the pinned source. Stable UIDs avoid invoking the
    # source's OS/random item-identity service in this bounded oracle.
    session=json.loads((ROOT/'content/native-session.json').read_text(encoding='utf8'))
    initial=session['defaults']['characters'][0]['inventory'];rows=[]
    for index,row in enumerate(initial+session['acquisitions']):
        name=row['item_id'];rows.append(dict(id=name,data=ex.yaml('Data/Items/'+name+'.yaml'),equipped=row.get('equipped',False),uid=row['uid'] if 'uid'in row else 100+index-len(initial)))
    scripts={'item.gd':ITEM,'character.gd':CHARACTER,'inventory.gd':inventory,'options.gd':options,'probe.gd':PROBE}
    for name,code in scripts.items():(work/name).write_text(code,encoding='utf8')
    (work/'fixture.json').write_text(json.dumps(dict(items=rows)),encoding='utf8')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Bounded original inventory oracle"\n[logging]\nfile_logging/enable_logging=false\n')
    command=[str(a.godot.resolve()),'--no-window','--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--out='+str(out/'reference.json')]
    with (out/'godot.log').open('wb')as log:result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=45)
    log=(out/'godot.log').read_text(encoding='utf8',errors='replace')
    require(result.returncode==0 and 'SCRIPT ERROR'not in log and 'ERROR:'not in log and 'FIELD_MENUS_REFERENCE_COMPLETE'in log,'Original inventory oracle failed; retained log')
    data=json.loads((out/'reference.json').read_text());require(data['engine']['string']=='3.6.2-stable (official)','Wrong reference engine')
    receipt=dict(commit=ex.lock['commit'],engine_sha256=sha(a.godot),tool_sha256=sha(__file__),sources=ex.sources,methods_sha256=methods,reference_sha256=sha(out/'reference.json'),generated_scripts_sha256={name:sha(work/name)for name in scripts},fixture_sha256=sha(work/'fixture.json'),
      adapters=['Item dictionaries are pinned YAML; item identity, get_data and equippable lookup are bounded adapters without RNG','Character has no asthma, no boosts and fixed HP/PP; source consume_item executes its real failure and dose removal branches','Only sorting, manual swap, consumption, drop and volume conversion methods execute; no original UI rendering/input/audio or full-game claim'],scope='Bounded source inventory and options methods; START controller integration is tested separately in the native emulator')
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');shutil.rmtree(work);print(json.dumps(data['cases'],indent=2))
if __name__=='__main__':main()
