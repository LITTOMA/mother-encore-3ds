"""Compile the reviewed single-skill Bat/Idle action owner, not a general tree VM."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing,podunk_field as f
from tools.godot_text import Document,SubRef,Vec2,NodePathValue
from tools.extract_battle_entry import require
RECIPE=ROOT/'content/field-swing-action.json';MAGIC=b'ENCSWA01';STRIDES=(64,24,1);LIMIT=32*1024

def unique(pairs):
 out={}
 for k,v in pairs:require(k not in out,'Duplicate swing action binding');out[k]=v
 return out

def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind source_recipe reference idle_state attack_state input_action skill_id process_mode output sources symbols'.split()),'Swing action binding fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.swing-action.reviewed-bindings'and p['source_recipe']=='content/field-swing.json'and p['reference']=='reports/iggy-tutorial/swing-action-reference-5/source.json','Swing action binding schema/source')
 require(all(type(p[k])is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]*',p[k])for k in ('idle_state','attack_state','input_action','skill_id'))and p['idle_state']!=p['attack_state']and type(p['process_mode'])is int and p['process_mode']==1,'Swing action role/process policy')
 require(type(p['output'])is str and re.fullmatch('world/[a-z0-9_/-]+\.encaction',p['output'])and '..'not in p['output']and '//'not in p['output'],'Swing action output identity')
 base,ex=field_swing.read(ROOT/p['source_recipe']);reference=json.loads((ROOT/p['reference']).read_bytes())
 require(type(p['sources'])is dict and p['sources']==reference['sources']and type(p['symbols'])is dict and p['symbols']==reference['symbols']and reference['engine']['string']=='3.6.2-stable (official)'and reference['observations']==683,'Swing action reviewed reference identity')
 for name,digest in p['sources'].items():require(hashlib.sha256(ex.data(name)).hexdigest()==digest,'Swing action reviewed source changed: '+name)
 code=ex.text(base['script'])
 for name,digest in p['symbols'].items():
  source=ex.text(base['fetcher'])if name=='get_frames'else code;match=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',source,re.M|re.S)
  require(match is not None and hashlib.sha256(match[0].encode()).hexdigest()==digest,'Swing action reviewed method changed: '+name)
 return p,base,ex

def source(path=RECIPE):
 p,base,ex=read(path);action=field_swing.source(ROOT/p['source_recipe']);doc=Document(base['scene'],ex.text(base['scene']));nodes={n.attrs['name']:n for n in doc.nodes};machine=doc.sub[nodes['AnimationTree'].props['tree_root'].id]
 require(machine[0]=='AnimationNodeStateMachine'and machine[1]['start_node']==p['idle_state']and nodes['AnimationTree'].props.get('process_mode',p['process_mode'])==p['process_mode'],'Swing action graph start/process changed')
 states={name:doc.sub[machine[1]['states/'+name+'/node'].id]for name in(p['idle_state'],p['attack_state'])};require(states[p['idle_state']][0]=='AnimationNodeBlendSpace2D'and states[p['attack_state']][0]=='AnimationNodeBlendTree','Swing action graph node types changed')
 edges=[];transitions=machine[1]['transitions']
 for a,b,ref in zip(transitions[::3],transitions[1::3],transitions[2::3]):
  if a in states and b in states:edges.append((a,b,doc.sub[ref.id]))
 require(edges==[(p['idle_state'],p['attack_state'],('AnimationNodeStateMachineTransition',{})),(p['attack_state'],p['idle_state'],('AnimationNodeStateMachineTransition',{'switch_mode':2,'auto_advance':True}))],'Swing action transitions changed')
 code=ex.text(base['script']);require('if _paused or _state != MOVE or global.entering_door:'in code and 'if event.is_action_pressed("'+p['input_action']+'") and !_climbing and !_spinning:'in code and '"'+p['skill_id']+'":'in code,'Swing action input gate mapping changed')
 # Roles are typed execution bits; the source operations supply their masks.
 start=re.search(r'^func _do_attack\(\):\n\tmatch _current_skill_action:\n\t\t"'+re.escape(p['skill_id'])+r'":\n(.*?)(?=\n\t\t"|\Z)',code,re.M|re.S)[1]
 clear=[('_crouch = false',1),('_idle = false',2),('_tap_run = false',4),('_set_running(false)',8),('_walk = false',16)]
 require(all(start.count(text)==1 for text,_ in clear)and start.index('_state = ATTACK')<start.index('_set_running(false)')<start.index('_attack_unleash()'),'Swing action start operation order changed')
 finish=re.search(r'^func _attack_animation_finished\(\):\n(.*?)(?=^func |\Z)',code,re.M|re.S)[1];require(finish.strip()=='$HitboxPivot/BatHitbox/CollisionShape2D.disabled = true\n\t_state = MOVE\n\t_tap_run = false','Swing action deferred finish changed')
 idle=states[p['idle_state']][1];require(set(idle)=={'blend_mode'}|{f'blend_point_{i}/{k}'for i in range(8)for k in ('node','pos')}and idle.get('blend_mode')==1,'Swing idle direction selection changed');ap=nodes['AnimationPlayer'];directions=[]
 for i in range(8):
  ref=idle['blend_point_'+str(i)+'/node'];position=idle['blend_point_'+str(i)+'/pos'];kind,node=doc.sub[ref.id];require(kind=='AnimationNodeAnimation'and isinstance(position,Vec2),'Swing idle source direction node')
  kind,clip=doc.sub[ap.props['anims/'+node['animation']].id];duration=clip['length'];tracks={k.split('/')[1]for k in clip if k.startswith('tracks/')};require(kind=='Animation'and clip.get('loop')is True and type(duration)in(int,float)and math.isfinite(duration)and duration>0 and tracks=={'0','1','2'}and not(set(clip)-{'resource_name','length','loop','step'}-{k for k in clip if k.startswith('tracks/')}),'Swing idle source clip');values={}
  for j in range(3):
   root='tracks/'+str(j)+'/';target=clip[root+'path'];keys=clip[root+'keys'];require({k[len(root):]for k in clip if k.startswith(root)}=={'type','path','interp','loop_wrap','imported','enabled','keys'}and isinstance(target,NodePathValue)and clip[root+'type']=='value'and type(clip[root+'interp'])is int and clip[root+'interp']==1 and clip[root+'loop_wrap']is True and clip[root+'imported']is False and clip[root+'enabled']is True and type(keys)is dict and set(keys)=={'times','transitions','values','update'}and type(keys['update'])is int and keys['update']==1,'Swing idle source track')
   require(all(type(keys[k])is list for k in ('times','transitions','values'))and 0<len(keys['times'])==len(keys['transitions'])==len(keys['values'])and all(type(t)in(int,float)and math.isfinite(t)and t>=0 for t in keys['times'])and all(a<b for a,b in zip(keys['times'],keys['times'][1:]))and all(type(x)in(int,float)and x==1 for x in keys['transitions']),'Swing idle source keys')
   active=[v for t,v in zip(keys['times'],keys['values'])if t<=duration];times=[t for t in keys['times']if t<=duration];require(times==[0]and len(active)==1,'Swing idle reachable keys changed');values[target.path]=active[0]
  require(set(values)=={'Position/main:frame','Position/main:visible','SpecialAnimations:visible'}and type(values['Position/main:frame'])is int and all(type(values[k])is bool for k in ('Position/main:visible','SpecialAnimations:visible')),'Swing idle pose fields')
  matching=[c for c in action['clips']if c['direction']==[position.x,position.y]];require(len(matching)==1,'Swing action/idle direction binding')
  directions.append(dict(clip=matching[0]['id'],direction=[position.x,position.y],frame=values['Position/main:frame'],flags=int(values['Position/main:visible'])|int(values['SpecialAnimations:visible'])<<1))
 directions.sort(key=lambda row:next(i for i,c in enumerate(action['clips'])if c['id']==row['clip']))
 normal=next(n for n in doc.nodes if n.attrs['name']=='main'and n.attrs.get('parent')=='Position');frame_count=normal.props['hframes']*normal.props['vframes']
 model=dict(policy=[p['process_mode'],63,sum(bit for _,bit in clear),4,0,2,1,frame_count],strings=[p[k]for k in ('idle_state','attack_state','input_action','skill_id')],directions=directions)
 return model,dict(commit=field_swing.PIN,scope=__doc__,sources=ex.sources,reviewed_methods=p['symbols'],reference_sha256=hashlib.sha256((ROOT/p['reference']).read_bytes()).hexdigest(),dependencies={Path(path).resolve().relative_to(ROOT).as_posix():hashlib.sha256(Path(path).read_bytes()).hexdigest(),'tools/field_swing_action.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'tools/field_swing.py':hashlib.sha256((ROOT/'tools/field_swing.py').read_bytes()).hexdigest()})

def encode(m):
 require(type(m)is dict and set(m)=={'policy','strings','directions'}and type(m['policy'])is list and len(m['policy'])==8 and all(type(v)is int for v in m['policy']),'Swing action model fields')
 mode,gates,start,finish,enter,leave,automatic,frame_count=m['policy'];require(mode==1 and 0<gates<=63 and 0<=start<=31 and 0<=finish<=31 and enter==0 and leave==2 and automatic==1 and 0<frame_count<=65535,'Swing action model policy')
 require(type(m['strings'])is list and len(m['strings'])==4 and all(type(v)is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]{0,63}',v)for v in m['strings'])and m['strings'][0]!=m['strings'][1],'Swing action model roles')
 require(type(m['directions'])is list and len(m['directions'])==8,'Swing action model direction count');ids=set();positions=set();rows=[]
 for r in m['directions']:
  require(type(r)is dict and set(r)=={'clip','direction','frame','flags'}and all(type(r[k])is int for k in ('clip','frame','flags'))and 0<r['clip']<=65535 and 0<=r['frame']<frame_count and 0<=r['flags']<=3,'Swing action model direction fields');v=r['direction'];require(type(v)is list and len(v)==2 and all(type(x)in(int,float)and math.isfinite(x)and abs(x)<=1 for x in v)and v!=[0,0]and r['clip']not in ids and tuple(v)not in positions,'Swing action model direction identity');ids.add(r['clip']);positions.add(tuple(v));rows.append(struct.pack('<I2f3I',r['clip'],*v,r['frame'],r['flags'],0))
 pool=bytearray();refs=[]
 for value in m['strings']:refs.extend([len(pool),len(value)]);pool.extend(value.encode('ascii'))
 return f.pack_container(MAGIC,0,[struct.pack('<16I',*m['policy'],*refs),b''.join(rows),bytes(pool)],STRIDES,LIMIT)
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned swing action candidate directory required');m,proof=source();blob=encode(m);f.unpack_container(blob,MAGIC,STRIDES,LIMIT);out.mkdir(parents=True);(out/'swing.encaction').write_bytes(blob);proof.update(model=m,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Source swing action:',len(blob),'candidate bytes; no release binding')
