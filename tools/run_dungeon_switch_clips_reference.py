"""Original numeric frame/body animations with script/signals quarantined.

Source resources, tracks and initial properties are exact. Numeric copies
exclude script execution and signal dispatch; this is not scene/gameplay,
texture binding, actual contact, audio or hardware verification.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switch_clips as d
from tools.godot_text import Document
from tools.run_scene_reference import invoke
PROBE='''extends SceneTree
var target
var animator
var sprite
var body
var rows=[]
var completed=[]
func _init():call_deferred("run")
func finished(name):completed.append(name)
func capture(profile,index,action):
    rows.append({"file":profile.file,"index":index,"op":action.op,"frame":sprite.frame,"visible":sprite.visible and target.visible,"body":not body.disabled if body!=null else false,"playing":animator.is_playing(),"current":animator.current_animation if animator.is_playing() else "","position":"%.17f" % animator.current_animation_position,"completed":completed.duplicate()})
    completed.clear()
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    for profile in config.profiles:
        target=load("res://"+profile.scene).instance()
        animator=target.get_node("AnimationPlayer")
        animator.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        sprite=target.get_node(profile.frame_owner)
        body=target.get_node(profile.body_owner) if profile.body_owner!=null else null
        get_root().add_child(target)
        animator.connect("animation_finished",self,"finished")
        for i in profile.actions.size():
            var action=profile.actions[i]
            if action.op=="play":animator.play(action.name)
            elif action.op=="advance":animator.advance(action.delta)
            else:assert(false)
            capture(profile,i,action)
        target.queue_free()
        yield(self,"idle_frame")
        yield(self,"idle_frame")
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    quit()
'''
def numeric(raw,profile):
 doc=Document(profile['scene'],raw);scripts=[(i,p)for i,(p,k)in doc.ext.items()if k=='Script'];d.require(len(scripts)==1 and scripts[0][1]==profile['script'],'Dungeon numeric script quarantine scope')
 rid=scripts[0][0];value,n=re.subn(r'^\[ext_resource path="res://'+re.escape(profile['script'])+r'" type="Script" id='+str(rid)+r'\]\n','',raw,flags=re.M);d.require(n==1,'Dungeon numeric script declaration')
 value,n=re.subn(r'^script = ExtResource\( '+str(rid)+r' \)\n','',value,flags=re.M);d.require(n==1,'Dungeon numeric script attachment')
 value,n=re.subn(r'^\[connection [^\n]*\]\n?','',value,flags=re.M);d.require(n==len(doc.connections),'Dungeon numeric signal quarantine')
 value,n=re.subn(r'load_steps='+str(doc.header.attrs['load_steps'])+r'\b','load_steps='+str(doc.header.attrs['load_steps']-1),value,count=1);d.require(n==1,'Dungeon numeric resource count')
 result=Document(profile['scene'],value);d.require(result.sub==doc.sub and len(result.nodes)==len(doc.nodes),'Dungeon numeric animation/shape resources changed')
 for old,new in zip(doc.nodes,result.nodes):d.require(old.attrs==new.attrs and {k:v for k,v in old.props.items()if k!='script'}==new.props,'Dungeon numeric property changed')
 return value,dict(script=profile['script'],connections=doc.connections,scope='All script/signals excluded from numeric property-only fixture; source subresources/node properties exact.')
def actions(model):
 result=[]
 for clip in model['clips']:
  result.append(dict(op='play',name=clip['name'],id=clip['id']));times={0,clip['duration']/2,clip['duration'],clip['duration']+.001}
  for key in clip['keys']:
   if key['time']>0:times.update((max(0,key['time']-.000001),key['time'],key['time']+.000001))
  previous=0
  for time in sorted(times):result.append(dict(op='advance',delta=time-previous));previous=time
  result.extend([dict(op='play',name=clip['name'],id=clip['id']),dict(op='advance',delta=0),dict(op='advance',delta=clip['duration']+.01),dict(op='play',name=clip['name'],id=clip['id'])])
  result.extend(dict(op='advance',delta=1/60)for _ in range(62))
 return result
def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh dungeon numeric reference and actual engine required')
 data,proof=d.extract();review,ex=d.read();project=out/'project';project.mkdir(parents=True);originals=out/'source-scenes';originals.mkdir();(project/'.owned-dungeon-clip-reference').write_text('source-dungeon-property-reference-v1',encoding='utf8');config=[];copied={};quarantine={}
 for row in data:
  p=row['profile'];raw=ex.data(p['scene']);(originals/(p['name']+'.tscn')).write_bytes(raw);numeric_text,boundary=numeric(ex.text(p['scene']),p);name='numeric-'+p['name']+'.tscn';(project/name).write_text(numeric_text,encoding='utf8');quarantine[p['scene']]=boundary
  doc=Document(p['scene'],ex.text(p['scene']))
  for source,kind in doc.ext.values():
   if kind=='Script':continue
   d.require(kind in('Texture','AudioStream'),'Dungeon numeric unknown external resource');target=project/source;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(source));copied[source]=hashlib.sha256(target.read_bytes()).hexdigest()
  config.append(dict(scene=name,file=p['name']+'.encpcl',frame_owner=p['frame_owner'],body_owner=p['body_owner'],actions=actions(row['model']),ids={c['name']:c['id']for c in row['model']['clips']}))
 (project/'input.json').write_text(json.dumps(dict(profiles=config),indent=2)+'\n',encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8');(project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source dungeon property clips"\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'engine.log')
 observed=json.loads((project/'observations.json').read_bytes());v=observed['engine'];d.require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Dungeon numeric engine version changed')
 expected=[(profile,i,action)for profile in config for i,action in enumerate(profile['actions'])];d.require(len(observed['rows'])==len(expected),'Dungeon numeric observations incomplete');lines=['file op value frame visible body playing current completed']
 for row,(profile,i,action)in zip(observed['rows'],expected):
  d.require(row['file']==profile['file']and row['index']==i and row['op']==action['op'],'Dungeon numeric action order changed');ids=profile['ids']
  lines.append(' '.join(map(str,[profile['file'],action['op'],action['id']if action['op']=='play'else repr(action['delta']),row['frame'],int(row['visible']),int(row['body']),int(row['playing']),ids[row['current']]if row['current']else 0,','.join(str(ids[c])for c in row['completed'])or'-'])))
 for source,digest in copied.items():d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==digest,'Dungeon numeric original asset modified')
 (out/'states.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(out/'observations.json').write_bytes((project/'observations.json').read_bytes());(out/'input.json').write_bytes((project/'input.json').read_bytes());proof.update(scope=__doc__,rows=len(expected),quarantine=quarantine,copied_assets=copied,source_scenes={p['scene']:hashlib.sha256(ex.data(p['scene'])).hexdigest()for p in review['profiles']},engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),numeric_input='Exact original input JSON reals used in TSV, not rounded diagnostic echoes.',probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest())
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original dungeon frame/body:',len(expected),'observations,3profiles/11sourceclips; script/signals explicitly quarantined')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
