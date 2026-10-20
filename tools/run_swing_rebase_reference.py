"""Observe repeated motion/attack handoffs on one persistent source player.

The restricted source branches and original methods come from the unchanged
entry fixture. Each sequence retains the same nodes and sparse properties.
This does not execute the full Player movement physics or a playable scene.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_swing_entry_reference as entry,field_swing_entry
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
var rows=[]
func _init():call_deferred("run")
func capture(target,tree,id,sequence,phase):
    target.get_node("SpecialAnimations/NintenBat")._process(0)
    var playback=tree.get("parameters/playback")
    rows.append({"case":id,"sequence":sequence,"phase":phase,"node":playback.get_current_node(),"time":"%.17f" % playback.get_current_play_position(),"main_frame":target.get_node("Position/main").frame,"main":target.get_node("Position/main").visible,"special":target.get_node("SpecialAnimations").visible,"frame":target.get_node("SpecialAnimations").frame,"hit":not target.get_node("HitboxPivot/BatHitbox/CollisionShape2D").disabled,"pivot":"%.17f" % target.get_node("HitboxPivot").rotation_degrees,"attack":target._state==target.ATTACK,"behind":target.get_node("SpecialAnimations/NintenBat").show_behind_parent,"selected":target._current_skill_action,"starts":get_root().get_node("global").rumble.size(),"bat_visible":target.get_node("SpecialAnimations/NintenBat").visible})
func run():
    yield(self,"idle_frame")
    var f=File.new()
    assert(f.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(f.get_as_text()).result
    f.close()
    var target=load("res://probe.tscn").instance()
    get_root().add_child(target)
    var tree=target.get_node("AnimationTree")
    tree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
    tree.active=true
    tree.advance(0)
    var playback=tree.get("parameters/playback")
    var id=config.offset
    for definition in config.cases:
        assert(target._state==target.MOVE)
        target._party_member.incapacitated=definition.incapacitated
        target._direction=Vector2(definition.direction[0],definition.direction[1])
        for param in config.parameters:tree.set(param,target._direction)
        playback.start(definition.state)
        tree.advance(0)
        tree.advance(definition.warmup)
        assert(playback.get_current_node()==definition.state)
        capture(target,tree,id,config.sequence,0)
        var event=InputEventJoypadButton.new()
        event.button_index=1
        event.pressed=true
        target._input(event)
        capture(target,tree,id,config.sequence,1)
        for i in range(40):
            tree.advance(1.0/60.0)
            capture(target,tree,id,config.sequence,2)
            yield(self,"idle_frame")
            capture(target,tree,id,config.sequence,3)
        assert(target._state==target.MOVE)
        id+=1
    tree.active=false
    target.free()
    assert(f.open("res://native.json",File.WRITE)==OK)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows}))
    f.close()
    quit()
'''

def run(work,reports,engine):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned rebase reference directories required')
 work.mkdir(parents=True);reports.mkdir(parents=True);recipe=field_swing_entry.read();proof,params=entry.prepare(work,recipe)
 original=json.loads((ROOT/'reports/iggy-tutorial/swing-entry-reference-4/cases.json').read_bytes())
 if len(original)!=80:raise ValueError('Original entry matrix incomplete')
 cases=[original[(i*37)%len(original)]for i in range(80)]
 (work/'probe.gd').write_text(PROBE,encoding='utf8');(reports/'cases.json').write_text(json.dumps(cases,indent=2)+'\n',encoding='utf8')
 invoke(engine,work,['--editor','--quit'],reports/'import.log');actual=None
 for start in range(0,len(cases),10):
  config=dict(cases=cases[start:start+10],offset=start,sequence=start//10,parameters=params);raw=json.dumps(config);(work/'input.json').write_text(raw,encoding='utf8');(reports/f'input-{start}.json').write_text(raw,encoding='utf8')
  invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd')],reports/f'godot-{start}.log');part=json.loads((work/'native.json').read_bytes())
  if len(part['rows'])!=820 or part['engine']['string']!='3.6.2-stable (official)':raise ValueError('Incomplete persistent source sequence')
  if actual is None:actual=part
  else:
   if actual['engine']!=part['engine']:raise ValueError('Source engine changed between sequences')
   actual['rows'].extend(part['rows'])
 ids={v['name']:v['id']for v in recipe['states']};lines=[]
 for i,c in enumerate(cases):
  rows=actual['rows'][i*82:(i+1)*82]
  if len(rows)!=82 or any(r['case']!=i or r['sequence']!=i//10 for r in rows)or rows[0]['phase']!=0 or rows[0]['node']!=c['state']or rows[0]['attack']or rows[-1]['attack']:raise ValueError('Incomplete rebase method sequence')
  if rows[0]['starts']!=i%10 or rows[1]['starts']!=i%10+1:raise ValueError('Source player was not retained')
  for row in rows:
   if row['selected']not in('','swing'):raise ValueError('Unreviewed source skill selection')
   lines.append(' '.join(map(str,[i,i//10,c['id'],*c['direction'],int(c['incapacitated']),rows[0]['time'],row['phase'],ids[row['node']],row['time'],row['main_frame'],row['frame'],int(row['main']),int(row['special']),int(row['hit']),row['pivot'],int(row['attack']),int(row['behind']),int(row['bat_visible']),row['starts'],int(row['selected']=='swing')])))
 (reports/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(reports/'reference.json').write_text(json.dumps(actual,separators=(',',':'))+'\n',encoding='utf8')
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 proof.update(scope=__doc__,engine=actual['engine'],engine_sha256=sha(engine),cases=80,sequences=8,observations=len(actual['rows']),tools={p:sha(ROOT/p)for p in ['tools/run_swing_rebase_reference.py','tools/run_swing_entry_reference.py','tools/run_swing_action_reference.py']},probe_sha256=sha(work/'probe.gd'),outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()})
 (reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Persistent source player:',len(actual['rows']),'observations in 8 retained sequences')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);p.add_argument('--engine',type=Path,required=True);o=p.parse_args();run(o.work,o.reports,o.engine)
