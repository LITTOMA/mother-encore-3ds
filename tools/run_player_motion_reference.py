"""Observe original directional setters and pause/attack/motion handoffs.

The retained eight original animation branches and original logical input,
pause, set_direction, blend_position, _turn_to and set_anim_state methods run
in official Godot. Party/audio/collision boundaries remain fixture services;
this does not execute original movement physics or the whole game.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_swing_pause_reference as pause,field_swing,field_swing_entry
from tools.run_scene_reference import invoke
PROBE=pause.PROBE[:pause.PROBE.index('func run():')]+'''func run():
    yield(self,"idle_frame")
    var f=File.new()
    assert(f.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(f.get_as_text()).result
    f.close()
    for definition in config.cases:
        var id=definition.id
        pauses=0
        unpauses=0
        get_root().get_node("global").rumble.clear()
        var target=load("res://probe.tscn").instance()
        get_root().add_child(target)
        target.connect("paused",self,"on_pause")
        target.connect("unpaused",self,"on_unpause")
        target._party_member.incapacitated=definition.incapacitated
        target._direction=Vector2(0,1)
        var tree=target.get_node("AnimationTree")
        tree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
        tree.active=true
        for param in config.parameters:tree.set(param,target._direction)
        tree.advance(0)
        var playback=tree.get("parameters/playback")
        playback.start(definition.state)
        tree.advance(0)
        tree.advance(.04)
        capture(target,tree,id,0)
        for op in definition.ops:
            if op==1:press(target)
            elif op==2:tree.advance(1.0/60)
            elif op==3:yield(self,"idle_frame")
            elif op==4:target.pause(false,false)
            elif op==6:target.unpause()
            elif op==8:target.set_direction(Vector2(1,0))
            elif op==9:target._turn_to(Vector2(1,0),{"x":true,"y":true})
            elif op==10:target.set_anim_state("Walk")
            elif op==11:target.set_direction(Vector2(0,-1))
            elif op==12:target.set_direction(Vector2(-1,0))
            elif op==13:target._turn_to(Vector2(-1,-1),{"x":true,"y":true})
            else:assert(false)
            capture(target,tree,id,op)
        tree.active=false
        target.free()
        yield(self,"idle_frame")
    assert(f.open("res://native.json",File.WRITE)==OK)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows}))
    f.close()
    quit()
'''
def prepare(work):
 proof,params=pause.prepare(work);p,ex=field_swing.read();player=ex.text(p['script']);party=ex.text('Scripts/Main/party/party_object.gd');methods=[]
 for path,text,names in[(p['script'],player,['set_direction','blend_position','_turn_to']),('Scripts/Main/party/party_object.gd',party,['blend_animation'])]:
  for name in names:
   found=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',text,re.M|re.S)
   if not found:raise ValueError('Missing original directional method: '+name)
   proof.setdefault('direction_methods',{})[path+'::'+name]=hashlib.sha256(found[0].encode()).hexdigest();methods.append(found[0])
 path=work/'player.gd';wrapper=path.read_text(encoding='utf8');pattern=r'^func blend_animation\(.*?(?=^func |^static func |\Z)'
 if len(re.findall(pattern,wrapper,re.M|re.S))!=1:raise ValueError('Directional fixture blend adapter differs')
 wrapper=re.sub(pattern,'',wrapper,flags=re.M|re.S);path.write_text(wrapper+'\nonready var eventRayCaster = $EventRayCaster\n'+''.join(methods),encoding='utf8')
 path=work/'probe.tscn';path.write_text(path.read_text(encoding='utf8')+'\n[node name="EventRayCaster" type="RayCast2D" parent="."]\n',encoding='utf8');proof.update(sources={**proof['sources'],**ex.sources},scope=__doc__);return proof,params
def definitions():
 result=[]
 for state in field_swing_entry.read()['states']:
  if state['name']=='Bat':continue
  for idle_only in(False,True):
   step=lambda n:[2,3]*n
   ops=step(3)+[1]+step(5)+[4,6,9 if idle_only else 8]+step(4)+[10]+step(8)+[11]+step(8)+[12,11]+step(4)+[13]+step(6)+[1]+step(30)
   result.append(dict(id=len(result),state=state['name'],incapacitated=state['name'].startswith('Fainted'),idle_only=idle_only,ops=ops))
 return result
def run(work,reports,engine):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned player motion reference directories required')
 work.mkdir(parents=True);reports.mkdir(parents=True);proof,params=prepare(work);cases=definitions();(work/'probe.gd').write_text(PROBE,encoding='utf8');(reports/'cases.json').write_text(json.dumps(cases,indent=2)+'\n',encoding='utf8');invoke(engine,work,['--editor','--quit'],reports/'import.log');actual=None
 for offset in range(0,len(cases),4):
  batch=cases[offset:offset+4];raw=json.dumps(dict(cases=batch,parameters=params));(work/'input.json').write_text(raw,encoding='utf8');(reports/f'input-{offset}.json').write_text(raw,encoding='utf8');invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd')],reports/f'godot-{offset}.log');part=json.loads((work/'native.json').read_bytes())
  if part['engine']['string']!='3.6.2-stable (official)'or len(part['rows'])!=sum(1+len(c['ops'])for c in batch):raise ValueError('Incomplete original directional observations')
  if actual is None:actual=part
  else:
   if actual['engine']!=part['engine']:raise ValueError('Original directional engine changed')
   actual['rows'].extend(part['rows'])
 stable=[{k:v for k,v in row.items()if k!='stream_playing'}for row in actual['rows']];(reports/'reference.json').write_text(json.dumps(actual,separators=(',',':'))+'\n',encoding='utf8');(reports/'stable.json').write_text(json.dumps(stable,separators=(',',':'))+'\n',encoding='utf8')
 ids={s['name']:s['id']for s in field_swing_entry.read()['states']};lines=[]
 for row in stable:
  c=cases[row['case']];lines.append(' '.join(map(str,[row['case'],ids[c['state']],int(c['incapacitated']),row['op'],ids[row['node']],row['time'],row['main_frame'],row['frame'],int(row['main']),int(row['special']),int(row['hit']),row['pivot'],int(row['behind']),int(row['state']==2),int(row['paused']),ids[row['paused_node']]if row['paused_node']else 0,row['starts'],int(row['selected']=='swing')])))
 (reports/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();proof.update(engine=actual['engine'],engine_sha256=sha(engine),cases=len(cases),observations=len(stable),tools={p:sha(ROOT/p)for p in['tools/run_player_motion_reference.py','tools/run_swing_pause_reference.py','tools/run_swing_entry_reference.py','tools/run_swing_action_reference.py']},outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()});(reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original directional methods:',len(cases),'cases;',len(stable),'observations; no source movement physics')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);p.add_argument('--engine',type=Path,required=True);o=p.parse_args();run(o.work,o.reports,o.engine)
