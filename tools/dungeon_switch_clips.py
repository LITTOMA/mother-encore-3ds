"""Admit source switch frame/body clips into existing ENCPCL01 candidates.

No new runtime format, source script execution or live presentation admission.
Only independent frame/discrete body properties and closed constant keys are
represented. Method/audio/transform tracks and continuous curves fail closed.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,SubRef,NodePathValue,node_path
from tools import prop_clips
from tools.dungeon_keys import fields,path
RECIPE=ROOT/'content/catacombs-switch-clips.json'

def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate dungeon clip JSON field');out[k]=v
  return out
 def invalid(value):raise ValueError('Nonfinite dungeon clip JSON')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope profiles sources','dungeon clip review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-switch-clips.reviewed-source'and p['commit']==PIN,'Dungeon clip schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048 and type(p['profiles'])is list and 0<len(p['profiles'])<=128,'Dungeon clip scope/profiles')
 ids=set();names=set();sources=set()
 for row in p['profiles']:
  fields(row,'id name scene script texture frame_owner body_owner clips','dungeon clip profile')
  require(type(row['id'])is int and 0<row['id']<65536 and row['id']not in ids,'Dungeon clip profile ID');ids.add(row['id'])
  require(type(row['name'])is str and re.fullmatch('[a-z][a-z0-9_]{0,63}',row['name'])and row['name']not in names,'Dungeon clip profile name');names.add(row['name'])
  for k in('scene','script','texture'):path(row[k]);sources.add(row[k])
  path(row['frame_owner']);require(row['body_owner']is None or type(row['body_owner'])is str,'Dungeon clip body selector')
  if row['body_owner']is not None:path(row['body_owner'])
  require(type(row['clips'])is list and 0<len(row['clips'])<=32,'Dungeon clip count');clip_ids=set();clip_names=set()
  for c in row['clips']:
   fields(c,'id name','dungeon clip identity');require(type(c['id'])is int and 0<c['id']<=65535 and c['id']not in clip_ids and type(c['name'])is str and c['name']and c['name']not in clip_names,'Dungeon clip identity');clip_ids.add(c['id']);clip_names.add(c['name'])
 require(type(p['sources'])is dict and set(p['sources'])==sources,'Dungeon clip source coverage');ex=Extractor(ROOT)
 for name,digest in p['sources'].items():require(type(digest)is str and len(digest)==64 and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Dungeon clip reviewed source changed: '+name)
 return p,ex

def extract(file=RECIPE):
 p,ex=read(file);result=[]
 for profile in p['profiles']:
  doc=Document(profile['scene'],ex.text(profile['scene']));nodes={node_path(n.attrs):n for n in doc.nodes}
  require(len(nodes)==len(doc.nodes)and nodes['.'].attrs.get('type')=='Sprite'and doc.ext_path(nodes['.'].props.get('script'),'Script')==profile['script'],'Dungeon clip scene script/owner')
  sprite=nodes.get(profile['frame_owner']);require(sprite is not None and sprite.attrs.get('type')=='Sprite','Dungeon clip frame owner')
  require(set(sprite.props)<=set('texture hframes vframes frame visible position offset'.split())and doc.ext_path(sprite.props.get('texture'),'Texture')==profile['texture'],'Dungeon clip sprite properties/texture')
  columns=sprite.props.get('hframes',1);rows=sprite.props.get('vframes',1);frame=sprite.props.get('frame',0)
  require(type(columns)is int and type(rows)is int and columns>0 and rows>0 and columns*rows<=65535 and type(frame)is int and 0<=frame<columns*rows,'Dungeon clip frame grid')
  size=ex.png_size(profile['texture']);require(size[0]%columns==0 and size[1]%rows==0,'Dungeon clip nonintegral source frame grid')
  visible=sprite.props.get('visible',True);root_visible=nodes['.'].props.get('visible',True);require(type(visible)is bool and type(root_visible)is bool,'Dungeon clip visibility type')
  tracks={profile['frame_owner']+':frame':1};body=False
  if profile['body_owner']is not None:
   shape=nodes.get(profile['body_owner']);require(shape is not None and shape.attrs.get('type')=='CollisionShape2D','Dungeon clip body owner')
   parent=nodes.get(shape.attrs['parent']);require(parent is not None and parent.attrs.get('type')=='KinematicBody2D','Dungeon clip body parent')
   disabled=shape.props.get('disabled',False);require(type(disabled)is bool,'Dungeon clip body disabled type');body=not disabled;tracks[profile['body_owner']+':disabled']=3
  animator=nodes.get('AnimationPlayer');require(animator is not None and animator.attrs.get('type')=='AnimationPlayer'and list(animator.props)==['anims/'+c['name']for c in profile['clips']],'Dungeon clip animation coverage/order/autoplay')
  definitions=[]
  for clip in profile['clips']:
   ref=animator.props['anims/'+clip['name']];require(type(ref)is SubRef,'Dungeon clip resource reference');kind,props=doc.sub[ref.id]
   require(kind=='Animation'and not(set(props)-{'resource_name','length','step','loop'}-{k for k in props if k.startswith('tracks/')}),'Dungeon clip animation fields')
   require(props.get('resource_name',clip['name'])==clip['name']and props.get('loop',False)is False,'Dungeon clip name/loop')
   duration=prop_clips.f32(props.get('length',1));require(0<duration<=60,'Dungeon clip duration');grouped={}
   for key,value in props.items():
    if not key.startswith('tracks/'):continue
    parts=key.split('/');require(len(parts)==3 and parts[1].isdigit()and str(int(parts[1]))==parts[1],'Dungeon clip track field');grouped.setdefault(int(parts[1]),{})[parts[2]]=value
   require(grouped and set(grouped)==set(range(len(grouped))),'Dungeon clip track order');keys=[];constants=[];targets=set()
   for index,t in grouped.items():
    require(set(t)=={'type','path','interp','loop_wrap','imported','enabled','keys'}and t['type']=='value'and type(t['interp'])is int and t['interp']==1 and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'Dungeon clip unsupported track')
    require(type(t['path'])is NodePathValue and t['path'].path in tracks and t['path'].path not in targets,'Dungeon clip unknown/duplicate target');targets.add(t['path'].path);role=tracks[t['path'].path];k=t['keys']
    fields(k,'times transitions update values','dungeon clip keys');require(type(k['update'])is int and k['update']in(0,1),'Dungeon clip key update mode')
    require(all(type(k[n])is list for n in('times','transitions','values'))and 0<len(k['times'])==len(k['transitions'])==len(k['values'])<=256 and all(type(v)in(int,float)and v==1 for v in k['transitions']),'Dungeon clip key cardinality/curve')
    if k['update']==0:require(len(k['times'])==1 and k['times'][0]==0,'Dungeon clip continuous multi-key curve unsupported');constants.append(index)
    previous=-1
    for time,value in zip(k['times'],k['values']):
     time=prop_clips.f32(time);require(0<=time<=duration and time>previous,'Dungeon clip key time order/range');previous=time
     require(type(value)is(int if role==1 else bool)and(0<=value<columns*rows if role==1 else True),'Dungeon clip property value type/range');keys.append(dict(time=time,kind=role,value=int(value)))
   # These independent property targets commute; no method/audio dispatch is
   # admitted by sorting frame/body keys into the existing property format.
   keys.sort(key=lambda k:(k['time'],k['kind']));definitions.append(dict(id=clip['id'],name=clip['name'],duration=duration,keys=keys,constant_tracks=constants))
  model=dict(frames=columns*rows,initial_frame=frame,initial_mask=int(visible and root_visible)|int(body)<<1,clips=definitions)
  blob=prop_clips.encode(model);prop_clips.unpack_container(blob,prop_clips.MAGIC,prop_clips.STRIDES,prop_clips.LIMIT)
  result.append(dict(profile=profile,model=model,bytes=blob))
 return result,dict(commit=PIN,scope=p['scope'],whole_scripts_approved=False,sources=dict(sorted(ex.sources.items())))

def build(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=(ROOT/'build').resolve(),'Dungeon clip output must stay under build')
 data,proof=extract(file);out.mkdir(parents=True,exist_ok=True);details=[]
 for row in data:
  name=row['profile']['name']+'.encpcl';blob=row['bytes'];(out/name).write_bytes(blob);details.append(dict(profile=row['profile'],model=row['model'],file=name,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest()))
 proof.update(profiles=details,converter_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Dungeon property clips:',len(data),'profiles,',sum(len(r['model']['clips'])for r in data),'clips,',sum(len(r['bytes'])for r in data),'bytes; no scene/texture binding')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--recipe',type=Path,default=RECIPE);a=p.parse_args();build(a.out,a.recipe)
