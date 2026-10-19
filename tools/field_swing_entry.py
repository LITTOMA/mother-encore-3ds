"""Compile checked source motion clips and requested paths into the Bat action.

This bounded entry graph has frame/visibility tracks only. It neither runs
arbitrary AnimationTree resources nor replaces the original movement audit.
"""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing,field_swing_action,podunk_field as f
from tools.godot_text import Document,SubRef,Vec2,NodePathValue
from tools.extract_battle_entry import require
RECIPE=ROOT/'content/field-swing-entry.json';MAGIC=b'ENCSWE01';STRIDES=(48,32,40,8,1);LIMIT=64*1024

def unique(pairs):
 out={}
 for k,v in pairs:require(k not in out,'Duplicate swing entry binding');out[k]=v
 return out
def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind source_recipe action_recipe idle attack states paths motion_states output'.split()),'Swing entry binding fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.swing-entry.reviewed-bindings'and p['source_recipe']=='content/field-swing.json'and p['action_recipe']=='content/field-swing-action.json','Swing entry binding schema/source')
 require(type(p['states'])is list and len(p['states'])==8,'Swing entry state count');ids=set();names=set()
 for s in p['states']:
  require(type(s)is dict and set(s)=={'id','name'}and type(s['id'])is int and 0<s['id']<=65535 and type(s['name'])is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]{0,63}',s['name'])and s['id']not in ids and s['name']not in names,'Swing entry state identities');ids.add(s['id']);names.add(s['name'])
 require(type(p['idle'])is int and type(p['attack'])is int and p['idle']in ids and p['attack']in ids and p['idle']!=p['attack'],'Swing entry state roles')
 lookup={s['id']:s['name']for s in p['states']};require(type(p['paths'])is dict and set(p['paths'])==names-{lookup[p['attack']]},'Swing entry path domain')
 for name,path in p['paths'].items():require(type(path)is list and 0<len(path)<len(names)and all(type(v)is str and v in names for v in path)and len(set([name,*path]))==len(path)+1 and path[-1]==lookup[p['attack']],'Swing entry path identity/cycle')
 for path in p['paths'].values():
  for i,name in enumerate(path[:-1]):require(p['paths'][name]==path[i+1:],'Swing entry path suffix differs')
 require(type(p['motion_states'])is list and len(p['motion_states'])==4 and all(type(i)is int and i in ids and i!=p['attack']for i in p['motion_states'])and len(set(p['motion_states']))==4,'Swing entry movement bindings')
 require(type(p['output'])is str and re.fullmatch('world/[a-z0-9_/-]+\.encentry',p['output'])and '..'not in p['output']and '//'not in p['output'],'Swing entry output identity')
 return p

def source(path=RECIPE):
 p=read(path);reference_path=ROOT/'reports/iggy-tutorial/swing-entry-reference-4/source.json';reference=json.loads(reference_path.read_bytes());require(reference['engine']['string']=='3.6.2-stable (official)'and reference['paths']==p['paths']and reference['recipe_sha256']==hashlib.sha256(Path(path).read_bytes()).hexdigest(),'Swing entry original topology reference differs');base,ex=field_swing.read(ROOT/p['source_recipe']);model,action_proof=field_swing_action.source(ROOT/p['action_recipe']);d=Document(base['scene'],ex.text(base['scene']));tree=next(n for n in d.nodes if n.attrs['name']=='AnimationTree');machine=d.sub[tree.props['tree_root'].id][1];ap=next(n for n in d.nodes if n.attrs['name']=='AnimationPlayer');ids={s['name']:s['id']for s in p['states']};names={v:k for k,v in ids.items()};require(names[p['idle']]==model['strings'][0]and names[p['attack']]==model['strings'][1],'Swing entry/action role names differ')
 edges={(a,b):d.sub[r.id]for a,b,r in zip(machine['transitions'][::3],machine['transitions'][1::3],machine['transitions'][2::3])};states=[];clips=[];excluded=[]
 for state in sorted(p['states'],key=lambda s:s['id']):
  row=dict(id=state['id'],name=state['name'],next=0,switch=0,first=len(clips),count=0)
  if state['id']==p['attack']:states.append(row);continue
  path_names=p['paths'][state['name']]
  for a,b in zip([state['name'],*path_names],path_names):require((a,b)in edges,'Swing entry path edge missing from original topology')
  kind,policy=edges[(state['name'],path_names[0])];require(kind=='AnimationNodeStateMachineTransition'and (policy=={}or policy=={'switch_mode':2}),'Swing entry unsupported edge settings');row.update(next=ids[path_names[0]],switch=policy.get('switch_mode',0))
  ref=machine['states/'+state['name']+'/node'];kind,space=d.sub[ref.id]
  if kind=='AnimationNodeBlendTree':
   fields={'nodes/'+state['name']+'/node','nodes/'+state['name']+'/position','nodes/TimeScale/node','nodes/TimeScale/position','node_connections'};optional={'graph_offset','nodes/output/position'};require(fields<=set(space)<=fields|optional and all(isinstance(space[k],Vec2)and math.isfinite(space[k].x)and math.isfinite(space[k].y)for k in optional&set(space)),'Swing entry unknown blend-tree fields')
   require(space['node_connections']==['TimeScale',0,state['name'],'output',0,'TimeScale']and d.sub[space['nodes/TimeScale/node'].id]==('AnimationNodeTimeScale',{}),'Swing entry time-scale wiring');kind,space=d.sub[space['nodes/'+state['name']+'/node'].id]
  require(kind=='AnimationNodeBlendSpace2D'and space.get('blend_mode')==1,'Swing entry discrete direction selection')
  indices=sorted({int(k.split('/')[0].removeprefix('blend_point_'))for k in space if k.startswith('blend_point_')});fields={'blend_mode'}|{f'blend_point_{i}/{k}'for i in indices for k in ('node','pos')};require(indices==list(range(len(indices)))and 1<=len(indices)<=8 and (set(space)==fields or set(space)==fields|{'auto_triangles','triangles'}),'Swing entry blend-space coverage')
  if 'triangles'in space:require(space['auto_triangles']is False and type(space['triangles'])is list and len(space['triangles'])%3==0 and all(type(v)is int and 0<=v<len(indices)for v in space['triangles']),'Swing entry discrete-space triangle metadata')
  for i in indices:
   kind,node=d.sub[space[f'blend_point_{i}/node'].id];direction=space[f'blend_point_{i}/pos'];require(kind=='AnimationNodeAnimation'and set(node)=={'animation'}and isinstance(direction,Vec2),'Swing entry source direction node');kind,clip=d.sub[ap.props['anims/'+node['animation']].id];duration=clip.get('length',1);require(kind=='Animation'and type(duration)in(int,float)and math.isfinite(duration)and 0<duration<=60 and type(clip.get('loop',False))is bool,'Swing entry source clip')
   require(not(set(clip)-{'resource_name','length','loop','step'}-{k for k in clip if k.startswith('tracks/')}),'Swing entry unknown clip fields');track_ids=sorted({int(k.split('/')[1])for k in clip if k.startswith('tracks/')});require(track_ids==list(range(len(track_ids)))and 1<=len(track_ids)<=3,'Swing entry track coverage');values={}
   for j in track_ids:
    root=f'tracks/{j}/';require({k[len(root):]for k in clip if k.startswith(root)}=={'type','path','interp','loop_wrap','imported','enabled','keys'}and clip[root+'type']=='value'and type(clip[root+'interp'])is int and clip[root+'interp']==1 and clip[root+'loop_wrap']is True and clip[root+'enabled']is True and clip[root+'imported']is False,'Swing entry unknown track settings');target=clip[root+'path'];keys=clip[root+'keys'];require(isinstance(target,NodePathValue)and target.path in('Position/main:frame','Position/main:visible','SpecialAnimations:visible')and target.path not in values,'Swing entry unsupported track target')
    require(type(keys)is dict and set(keys)=={'times','transitions','values','update'}and type(keys['update'])is int and keys['update']==1 and all(type(keys[k])is list for k in('times','transitions','values'))and 0<len(keys['times'])==len(keys['values'])==len(keys['transitions'])<=16 and all(type(t)in(int,float)and math.isfinite(t)and t>=0 for t in keys['times'])and all(a<b for a,b in zip(keys['times'],keys['times'][1:]))and all(type(v)in(int,float)and v==1 for v in keys['transitions']),'Swing entry source key schema')
    valid=lambda v:type(v)is int and 0<=v<model['policy'][7]if target.path.endswith(':frame')else type(v)is bool
    require(all(valid(v)for v in keys['values']),'Swing entry frame/visibility value');active=[dict(time=t,value=v)for t,v in zip(keys['times'],keys['values'])if t<=duration];require(active and active[0]['time']==0,'Swing entry initial key missing')
    if target.path.endswith(':visible'):require(len(active)==1,'Swing entry changing visibility curve unsupported')
    values[target.path]=active
    for t,v in zip(keys['times'],keys['values']):
     if t>duration:excluded.append(dict(animation=node['animation'],target=target.path,time=t,value=v,reason='Beyond source clip length; loop cannot reach this key'))
   require('Position/main:frame'in values,'Swing entry missing sprite frames');clips.append(dict(state=state['id'],ordinal=i,direction=[direction.x,direction.y],length=duration,loop=clip.get('loop',False),main=int(values['Position/main:visible'][0]['value'])if'Position/main:visible'in values else-1,special=int(values['SpecialAnimations:visible'][0]['value'])if'SpecialAnimations:visible'in values else-1,keys=[dict(time=k['time'],frame=k['value'])for k in values['Position/main:frame']]))
  row['count']=len(clips)-row['first'];states.append(row)
 model=dict(policy=[model['policy'][7],p['idle'],p['attack'],*p['motion_states']],states=states,clips=clips)
 return model,dict(commit=field_swing.PIN,scope=__doc__,sources={**ex.sources,**action_proof['sources']},excluded_unreachable_keys=excluded,paths=p['paths'],reference_sha256=hashlib.sha256(reference_path.read_bytes()).hexdigest(),dependencies={Path(path).resolve().relative_to(ROOT).as_posix():hashlib.sha256(Path(path).read_bytes()).hexdigest(),'tools/field_swing_entry.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'tools/field_swing_action.py':hashlib.sha256((ROOT/'tools/field_swing_action.py').read_bytes()).hexdigest()})

def encode(m):
 require(type(m)is dict and set(m)=={'policy','states','clips'}and type(m['policy'])is list and len(m['policy'])==7 and all(type(x)is int for x in m['policy']),'Swing entry model fields')
 frames,idle,attack,*motion=m['policy'];require(0<frames<=65535 and idle!=attack and type(m['states'])is list and 2<=len(m['states'])<=32 and type(m['clips'])is list and 0<len(m['clips'])<=256,'Swing entry model bounds');pool=bytearray();state_rows=[];state_ids=set();names=set();first=0
 for s in m['states']:
  require(type(s)is dict and set(s)==set('id name next switch first count'.split())and all(type(s[k])is int for k in('id','next','switch','first','count'))and 0<s['id']<=65535 and s['id']not in state_ids and type(s['name'])is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]{0,63}',s['name'])and s['name']not in names and s['switch']in(0,2)and s['first']==first and 0<=s['count']<=8 and s['count']<=len(m['clips'])-first,'Swing entry state model');state_ids.add(s['id']);names.add(s['name']);start=len(pool);pool.extend(s['name'].encode('ascii'));state_rows.append(struct.pack('<8I',s['id'],start,len(s['name']),s['next'],s['switch'],first,s['count'],0));first+=s['count']
 require(first==len(m['clips'])and idle in state_ids and attack in state_ids and all(x in state_ids and x!=attack for x in motion)and len(set(motion))==4,'Swing entry state/clip ownership');lookup={s['id']:s for s in m['states']}
 for s in m['states']:
  require((s['id']==attack and s['count']==0 and s['next']==0 and s['switch']==0)or(s['id']!=attack and s['count']>0 and s['next']in state_ids),'Swing entry target state ownership')
 for s in m['states']:
  visited=set();at=s['id']
  while at!=attack:require(at not in visited,'Swing entry path cycle');visited.add(at);at=lookup[at]['next']
 clips=[];keys=[]
 for s in m['states']:
  seen=set()
  for ordinal,c in enumerate(m['clips'][s['first']:s['first']+s['count']]):
   require(type(c)is dict and set(c)==set('state ordinal direction length loop main special keys'.split())and type(c['state'])is int and c['state']==s['id']and type(c['ordinal'])is int and c['ordinal']==ordinal and type(c['length'])in(int,float)and math.isfinite(c['length'])and 0<c['length']<=60 and type(c['loop'])is bool and all(type(c[k])is int and -1<=c[k]<=1 for k in('main','special')),'Swing entry clip model');v=c['direction'];require(type(v)is list and len(v)==2 and all(type(x)in(int,float)and math.isfinite(x)and abs(x)<=1 for x in v)and v!=[0,0]and tuple(v)not in seen,'Swing entry direction model');seen.add(tuple(v))
   require(type(c['keys'])is list and 0<len(c['keys'])<=16,'Swing entry key capacity');previous=-1;start=len(keys)
   for k in c['keys']:
    require(type(k)is dict and set(k)=={'time','frame'}and type(k['time'])in(int,float)and math.isfinite(k['time'])and previous<k['time']<=c['length']and type(k['frame'])is int and 0<=k['frame']<frames,'Swing entry key model');previous=k['time'];keys.append(struct.pack('<fI',k['time'],k['frame']))
   require(c['keys'][0]['time']==0,'Swing entry first key time');clips.append(struct.pack('<2I3f3I2i',c['state'],ordinal,*v,c['length'],int(c['loop']),start,len(c['keys']),c['main'],c['special']))
 meta=struct.pack('<12I',frames,idle,attack,len(m['states']),len(clips),len(keys),len(m['states']),0,*motion)
 return f.pack_container(MAGIC,0,[meta,b''.join(state_rows),b''.join(clips),b''.join(keys),bytes(pool)],STRIDES,LIMIT)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);o=p.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned entry candidate required');model,proof=source();blob=encode(model);f.unpack_container(blob,MAGIC,STRIDES,LIMIT);out.mkdir(parents=True);(out/'swing.encentry').write_bytes(blob);proof.update(model=model,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Source swing entry:',len(blob),'candidate bytes; no release binding')
