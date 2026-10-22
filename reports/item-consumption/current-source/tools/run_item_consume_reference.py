"""Observe original item consumption and target message/audio selectors.

The original complete Inventory/Item/Character/Status scripts execute. A member
holder supplies stats/inventory and exact PartyMember zero-boost/status methods;
the selected original target method uses explicit UI/audio command collectors.
No rendering, PCM output, target-widget timing or full-game claim.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, require
from tools import catacombs_item_catalog as catalog
from tools.run_scene_reference import invoke


def source_methods(raw):
    text=raw.decode('utf-8')
    starts=list(re.finditer(r'^(?:static )?func ([A-Za-z_][A-Za-z_0-9]*)\(',text,re.M))
    require(starts,'Missing original consumption methods')
    result={}
    for index,start in enumerate(starts):
        name=start[1]
        require(name not in result,'Duplicate original consumption method')
        end=starts[index+1].start() if index+1<len(starts) else len(text)
        block=text[start.start():end].rstrip()+'\n'
        # A source comment can be at column zero within an indented method.
        # Function boundaries, rather than comment indentation, delimit code.
        require(any(line.startswith('\t') and line.strip() and not line.lstrip().startswith('#')
                    for line in block.splitlines()[1:]),'Empty original consumption method')
        result[name]=block
    return result


PROBE = r'''extends Node
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
    var initial_status=[]
    for status in row.status:initial_status.append({"status":status,"passive_healing_turns":0})
    member._status_init_from_dict({"status":initial_status})
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
        if JSON.print(after)!=JSON.print(direct) or JSON.print(stats_events)!=JSON.print(direct_events):
            print("CONSUME_COMPARISON "+JSON.print({"input":row,"direct":direct,"target":after,"direct_signals":direct_events,"target_signals":stats_events}))
        # Godot 3 compares Dictionary identity. Compare observed values;
        # preserve every state and signal field in the two fresh captures.
        assert(JSON.print(after)==JSON.print(direct))
        assert(JSON.print(stats_events)==JSON.print(direct_events))
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
'''


def source():
    data = catalog.load()
    ex = Extractor(ROOT)
    originals = {name: ex.text(path) for name, path in {
        'Character.gd':'Scripts/global/Character.gd', 'Status.gd':'Scripts/Main/Status.gd',
        'Item.gd':'Scripts/global/Item.gd', 'Inventory.gd':'Scripts/global/Inventory.gd'}.items()}
    # Reuse only the two previously audited analyzer adaptations, verified
    # against complete current original files; no gameplay method is omitted.
    originals['Character.gd'] = originals['Character.gd'].replace(
        'func get_base_stat(stat: String, with_boosts := false) -> int:\n',
        'func get_base_stat(stat: String, with_boosts := false) -> int:\n\tvar _observer_boosts=with_boosts\n')
    originals['Status.gd'] = originals['Status.gd'].replace(
        'func get_status_message(ailment: String, message: String)',
        'func get_status_message(_observer_ailment: String, message: String)').replace(
        'get_ailment_data(ailment)\n\tif ailment_info',
        'get_ailment_data(_observer_ailment)\n\tif ailment_info')
    frozen = ROOT / 'reports/world-damage/native1'
    receipt = catalog.read(frozen / 'receipt.json')
    for name in ('Character.gd','Status.gd'):
        raw = originals[name].encode()
        require(hashlib.sha256(raw).hexdigest() == receipt['source']['generated_sha256'][name]
                and raw == (frozen / 'observer' / name).read_bytes(),
                'Original consumption Character/Status adaptation changed')
    members = source_methods(ex.data('Scripts/global/PartyMember.gd'))
    target_methods = source_methods(ex.data('Nodes/Ui/Inventory/TargetCharaSelect.gd'))
    member = 'extends "res://Character.gd"\nclass_name PartyMember\nvar _permanent_boosts={}\nvar _inventory=Inventory.new()\nvar inv setget , get_inventory\nfunc get_character_type() -> int:return Type.PARTY_MEMBER\nfunc get_nickname():return _name\n'
    member_names = ('get_inventory','set_stat','add_status','remove_status','_refresh_hp_from_status','_refresh_status_from_hp','apply_boosts')
    member += '\n'.join(members[name] for name in member_names)
    target = 'extends Node\nsignal show_dialogbox(dialog,character,value,stat,item)\nvar _messages_stack={}\n' + target_methods['consume_item']
    files = dict(originals, **{'Member.gd':member, 'Target.gd':target, 'probe.gd':PROBE})
    items = {row['source']: row['document'] for row in data['entries']}
    items['AsthmaSpray'] = ex.yaml('Data/Items/AsthmaSpray.yaml')
    items['BaseballCap'] = ex.yaml('Data/Items/BaseballCap.yaml')
    statuses = {name:ex.yaml('Data/StatusAilments/' + name + '.yaml') for name in ('asthma','poisoned')}
    require(all(not v.get('reusable',False) and v.get('transform','') == ''
                and not any(v['boost'].values()) for k,v in items.items() if k != 'BaseballCap'),
            'Unreviewed original consume traits')
    cases = []
    for identity in [v['source'] for v in data['entries']] + ['AsthmaSpray']:
        for maxhp in (30,120):
            for hp in (0,1,maxhp//2,maxhp-1,maxhp):
                for status in ([],['asthma'],['poisoned'],['poisoned','asthma']):
                    for doses in range(1,items[identity].get('doses',1)+1):
                        cases.append(dict(index=len(cases),item=identity,maxhp=maxhp,hp=hp,
                                          maxpp=26,pp=5,status=status,doses=doses))
    files['input.json'] = json.dumps(dict(items=items,statuses=statuses,cases=cases))
    files['globaldata.gd'] = '''extends Node
var characters={}
var fixture={}
func _ready():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    fixture=JSON.parse(file.get_as_text()).result
    file.close()
func get_item_data(name):return fixture.items[name]
func get_ailment_data(name):return fixture.statuses.get(name,{})
func does_ailment_exist(name):return fixture.statuses.has(name)
'''
    files['global.gd'] = 'extends Node\nvar events=[]\nvar party=["ninten"]\n'
    files['ui.gd'] = 'extends Node\nfunc is_in_battle():return false\n'
    files['audio.gd'] = 'extends Node\nfunc play_sfx(stream,slot):global.events.append(["audio",stream.resource_path,slot])\n'
    files['TextTools.gd'] = '''extends Reference
class_name TextTools
static func format_text_with_context(key,target,_context,value):
    return JSON.print([key,target.get_name(),value])
'''
    names = [('Character','Object','Character.gd'),('Status','Node','Status.gd'),
             ('Item','Reference','Item.gd'),('Inventory','Reference','Inventory.gd'),
             ('PartyMember','Object','Member.gd'),('TextTools','Reference','TextTools.gd')]
    classes = [dict(base=base,**{'class':name},language='GDScript',path='res://'+path) for name,base,path in names]
    config = 'config_version=4\n_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons='+json.dumps({n:''for n,_,_ in names})+'\n[application]\nconfig/name="Original item consumption"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobaldata="*res://globaldata.gd"\nglobal="*res://global.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://audio.gd"\n'
    project = ex.text('project.godot')
    warnings = [line for line in project.splitlines() if line.startswith('gdscript/warnings/')]
    require(warnings and all(re.fullmatch('gdscript/warnings/[a-z_]+=false',line) for line in warnings),
            'Unreviewed source analyzer settings')
    config += '[debug]\n'+'\n'.join(warnings)+'\n'
    files['project.godot'] = config
    files['probe.tscn'] = '[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource(1)\n'
    sound = 'Audio/Sound effects/EB/eat.wav'
    ex.data(sound+'.import')
    copied = {sound:ex.data(sound)}
    ex.data('LICENSE')
    proof = dict(commit=ex.lock['commit'],sources=ex.sources,
                 target_method_sha256=hashlib.sha256(target_methods['consume_item'].encode()).hexdigest(),
                 generated_sha256={name:hashlib.sha256(value.encode()).hexdigest() for name,value in files.items()},
                 copied_sha256={name:hashlib.sha256(value).hexdigest() for name,value in copied.items()},
                 cases=len(cases),scope=__doc__)
    proof['member_methods']={name:hashlib.sha256(members[name].encode()).hexdigest() for name in member_names}
    return files,copied,proof


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    require(catalog.digest(args.godot)=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd',
            'Pinned official original engine binary changed')
    out=args.out.resolve()
    require(out.is_relative_to(ROOT/'build') and out!=ROOT/'build' and not out.exists(),
            'Fresh original consumption output below build required')
    files,copied,proof=source()
    out.mkdir(parents=True)
    project=out/'project'
    project.mkdir()
    for name,value in copied.items():
        path=project/name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(value)
    catalog.write(out/'inputs.json',proof)
    # --editor --quit exits before the WAV scan completes. Use the existing
    # checked scan-completion plugin with only the original WAV present.
    assets=out/'asset-import'
    assets.mkdir()
    bootstrap='config_version=4\n[application]\nconfig/name="Original consume asset import"\n[logging]\nfile_logging/enable_logging=false\n[editor_plugins]\nenabled=PoolStringArray("res://addons/source_import/plugin.cfg")\n'
    (out/'asset-import-project.godot').write_text(bootstrap,encoding='utf-8',newline='')
    (assets/'project.godot').write_text(bootstrap,encoding='utf-8',newline='')
    plugin=assets/'addons/source_import'
    plugin.mkdir(parents=True)
    plugin_bytes=(ROOT/'tools/godot_exporter/import_plugin.gd').read_bytes()
    (plugin/'plugin.gd').write_bytes(plugin_bytes)
    plugin_config='[plugin]\nname="Checked source import"\ndescription="Wait for source asset scan, then quit"\nauthor="Encore native"\nversion="1.0"\nscript="plugin.gd"\n'
    (plugin/'plugin.cfg').write_text(plugin_config,encoding='utf-8',newline='')
    for name,value in copied.items():
        path=assets/name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(value)
    invoke(args.godot,assets,['--video-driver','GLES2','--debug','--editor'],out/'asset-import.log')
    require('ENCORE_IMPORT_COMPLETE' in (out/'asset-import.log').read_text(encoding='utf-8'),
            'Original consume asset scan did not finish')
    imported={}
    for path in assets.rglob('*'):
        if path.is_file() and (path.name.endswith('.import') or path.relative_to(assets).parts[0]=='.import'):
            name=path.relative_to(assets).as_posix()
            target=project/name
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(path.read_bytes())
            imported[name]=catalog.digest(path)
    require(imported,'Native WAV import produced no cache')
    proof.update(imported_sha256=imported,import_plugin_sha256=hashlib.sha256(plugin_bytes).hexdigest(),import_plugin_config_sha256=hashlib.sha256(plugin_config.encode()).hexdigest())
    catalog.write(out/'inputs.json',proof)
    for name,value in files.items():
        path=project/name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(value,encoding='utf-8',newline='')
    invoke(args.godot,project,['--video-driver','GLES2','--debug','--fixed-fps','60','--disable-render-loop'],out/'godot.log')
    require(all((project/name).read_bytes()==value.encode() for name,value in files.items())
            and all((project/name).read_bytes()==value for name,value in copied.items()),
            'Executed original consumption source changed')
    result=catalog.read(out/'reference.json')
    require(result['schema']==1 and len(result['rows'])==proof['cases']
            and result['engine']['string']=='3.6.2-stable (official)', 'Incomplete original consumption reference')
    receipt=dict(source=proof,engine_sha256=catalog.digest(args.godot),producer_sha256=catalog.digest(__file__),reference_sha256=catalog.digest(out/'reference.json'))
    catalog.write(out/'receipt.json',receipt)
    print(json.dumps(dict(cases=proof['cases'],reference_sha256=receipt['reference_sha256'])))


if __name__=='__main__':
    main()
