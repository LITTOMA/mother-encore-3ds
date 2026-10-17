#!/usr/bin/env python3
"""Run unchanged source BattleItemPool with a controlled Item UID interface.

The source Item.new randomize/clock allocator is tested separately. Here the
one-draw UID stub makes the pool's own candidate/percentage calls observable.
"""
from __future__ import annotations
import argparse,hashlib,json,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools import bridge_entry
STUB_ITEM='''extends Reference
class_name Item
var item_name = ""
var uid = 0
func _init(value):
    item_name = value
    uid = randi()
'''
STUB_PARTICIPANT='''extends Reference
class_name BattleParticipant
class SourceCharacter:
    extends Reference
    var data
    func _init(value):
        data = value
    func get_data():
        return data
var character
var identity
func _init(value):
    identity = value.id
    character = SourceCharacter.new(value.data)
func is_boss():
    return character.data.boss
func get_id():
    return identity
'''
PROBE='''extends SceneTree
func _init():
    call_deferred("_run")
func _run():
    var input = File.new()
    input.open("res://enemy.json", File.READ)
    var data = parse_json(input.get_as_text())
    input.close()
    var enemy = BattleParticipant.new(data)
    var records = []
    for initial_seed in range(1, 257):
        seed(initial_seed)
        var pool = load("res://pool.gd").new()
        pool.roll_item([enemy])
        var chosen = pool.item != null
        var record = {"seed": initial_seed, "selected": chosen, "uid": pool.item.uid if chosen else 0, "item": pool.item.item_name if chosen else "", "tail": [randi(), randi(), randi()]}
        records.append(record)
        pool.free()
    var output = File.new()
    output.open("res://result.json", File.WRITE)
    output.store_string(to_json({"godot": Engine.get_version_info(), "records": records}))
    output.close()
    quit(0)
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    out=a.out.resolve();require(a.godot.is_file() and out.is_relative_to(ROOT/'build') and not out.exists(),'Fresh bounded output and explicit Godot required')
    ex=Extractor(ROOT);recipe=bridge_entry.policy();enemy=ex.yaml('Data/Battlers/'+recipe['battler']+'.yaml');source='Scripts/UI/Battle/BattleItemPool.gd';original=ex.text(source)
    require(enemy['boss']is False and len(enemy['items'])==1 and set(enemy['items'][0])=={'chance','item'},'Only ordinary single-enemy source pool admitted')
    out.mkdir(parents=True);temporary=Path(tempfile.mkdtemp(prefix='owned-loot-',dir=out));(temporary/'.owned-bridge-reference').write_text('bridge-loot-v1',encoding='utf8')
    try:
        classes=[dict(base='Reference',**{'class':'Item'},language='GDScript',path='res://item.gd'),dict(base='Reference',**{'class':'BattleParticipant'},language='GDScript',path='res://participant.gd')]
        project='_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons={}\n[application]\nconfig/name="Original pool probe"\n[autoload]\nglobaldata="*res://data.gd"\n[rendering]\nquality/driver/driver_name="GLES2"\n'
        for name,text in [('project.godot',project),('item.gd',STUB_ITEM),('participant.gd',STUB_PARTICIPANT),('pool.gd',original),('probe.gd',PROBE),('data.gd','extends Node\nvar rare_drops = {}\n')]:
            (temporary/name).write_bytes(text.encode())
        (temporary/'enemy.json').write_bytes(json.dumps(dict(id=recipe['battler'],data=enemy)).encode())
        startup=None
        if sys.platform=='win32':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        for label,args in [('import',['--editor','--quit']),('run',['--script','res://probe.gd'])]:
            with(out/(label+'.log')).open('wb')as log:r=subprocess.run([str(a.godot.resolve()),'--path',str(temporary),*args],stdout=log,stderr=subprocess.STDOUT,startupinfo=startup,timeout=40)
            require(r.returncode==0,'Original pool '+label+' failed; retained actual log')
        result=json.loads((temporary/'result.json').read_text(encoding='utf8'))
        require(result['godot']['major']==3 and result['godot']['minor']==6 and result['godot']['patch']==2,'Wrong reference engine')
        records=result['records'];require(len(records)==256 and any(r['selected']for r in records) and any(not r['selected']for r in records),'Pool oracle branch coverage')
        result.update(scope='Unchanged original BattleItemPool.gd and source straydog pool, single enemy; controlled one-randi Item UID interface. Not original clock allocation, rewards UI, whole battle or hardware.',sources=ex.sources,godot_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),interfaces={name:hashlib.sha256(text.encode()).hexdigest()for name,text in [('item',STUB_ITEM),('participant',STUB_PARTICIPANT),('probe',PROBE)]})
        (out/'result.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
        rows=['seed,selected,uid,tail0,tail1,tail2']
        rows.extend(','.join(str(v)for v in [int(r['seed']),int(r['selected']),int(r['uid']),*[int(n)for n in r['tail']]])for r in records)
        (out/'expected.csv').write_bytes(('\n'.join(rows)+'\n').encode())
        print('Original pool:',len(records),'seeds,',sum(r['selected']for r in records),'selected; owned project removed')
    finally:
        require(temporary.parent==out and (temporary/'.owned-bridge-reference').is_file(),'Temporary reference ownership changed');shutil.rmtree(temporary)

if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError,subprocess.SubprocessError)as e:print('BRIDGE LOOT REFERENCE ERROR:',e,file=sys.stderr);raise SystemExit(1)
