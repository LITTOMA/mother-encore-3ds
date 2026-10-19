"""Checked Sprite/collision animation subset; no scene or script executor.

Outputs are development candidates until a source-specific scene owner binds
them. Continuous curves are not implemented; a single constant key is exact
when the closed property owner cannot receive external writes.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,SubRef,ExtRef,NodePathValue
from tools.podunk_field import pack_container,unpack_container

RECIPE=ROOT/'content/field-prop-clips.json'
MAGIC=b'ENCPCL01';STRIDES=(16,24,12);LIMIT=64*1024

def f32(value):
 require(type(value)in(int,float)and math.isfinite(value)and abs(value)<=1000000,'Prop clip finite real required')
 return struct.unpack('<f',struct.pack('<f',value))[0]

def read(path=RECIPE):
 def unique(pairs):
  result={}
  for k,v in pairs:require(k not in result,'Duplicate prop recipe field');result[k]=v
  return result
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind commit scene script texture clips tracks sources scope'.split()),'Prop clip recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.prop-clips.reviewed-source'and p['commit']==PIN,'Prop clip recipe schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Prop clip scope')
 require(type(p['sources'])is dict and all(type(k)is str and type(v)is str and len(v)==64 for k,v in p['sources'].items()),'Prop clip source identities')
 require(all(type(p[k])is str and p[k]in p['sources']for k in ('scene','script','texture')),'Prop clip source bindings')
 require(type(p['clips'])is list and 0<len(p['clips'])<=32 and all(type(c)is dict and set(c)=={'id','name'}and type(c['id'])is int and 0<c['id']<=65535 and type(c['name'])is str and c['name']for c in p['clips']),'Prop clip identities')
 require(len({c['id']for c in p['clips']})==len(p['clips'])and len({c['name']for c in p['clips']})==len(p['clips']),'Duplicate prop clip identity/name')
 require(type(p['tracks'])is list and len(p['tracks'])==5 and all(type(t)is dict and set(t)=={'kind','path'}and type(t['kind'])is int and 1<=t['kind']<=5 and type(t['path'])is str for t in p['tracks']),'Prop track identities')
 require({t['kind']for t in p['tracks']}==set(range(1,6))and len({t['path']for t in p['tracks']})==5,'Duplicate prop track binding')
 ex=Extractor(ROOT)
 for source,digest in p['sources'].items():require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Reviewed prop source changed: '+source)
 return p,ex

def source(path=RECIPE):
 p,ex=read(path);doc=Document(p['scene'],ex.text(p['scene']));nodes={}
 for n in doc.nodes:
  parent=n.attrs.get('parent');name=n.attrs['name'];key='.'if parent is None else name if parent=='.'else parent+'/'+name
  require(key not in nodes,'Duplicate prop node');nodes[key]=n
 root=nodes['.'];binding=root.props['script'];require(doc.ext_path(binding,'Script')==p['script'],'Prop source script binding differs')
 tracks={t['path']:t['kind']for t in p['tracks']};defaults={};definitions=[]
 for target,kind in tracks.items():
  parts=target.split(':');require(len(parts)==2 and parts[0]in nodes,'Missing prop track target');n=nodes[parts[0]];property=parts[1]
  require(n.attrs.get('type')==('Sprite'if kind<=2 else'CollisionShape2D')and property==('frame'if kind==1 else'visible'if kind==2 else'disabled'),'Prop track property/type differs')
  if kind>=3:
   parent=nodes[n.attrs['parent']];expected='StaticBody2D'if kind==3 else'Area2D';require(parent.attrs.get('type')==expected,'Prop collision owner type differs')
   if kind==4:require(parent.props.get('collision_mask',1)==2 and parent.props.get('monitorable',True)is False,'Prop hitbox filter differs')
   if kind==5:require(parent.props.get('monitoring',True)is False,'Prop interaction filter differs')
  value=n.props.get(property,0 if kind==1 else kind==2);require(type(value)is(int if kind==1 else bool),'Prop initial property type')
  defaults[kind]=value
  if kind==1:
   texture=n.props['texture'];require(doc.ext_path(texture,'Texture')==p['texture'],'Prop source texture differs')
   columns=n.props.get('hframes',1);rows=n.props.get('vframes',1);require(type(columns)is int and type(rows)is int and columns>0 and rows>0,'Prop source frame grid')
   size=ex.png_size(p['texture']);require(size[0]%columns==0 and size[1]%rows==0,'Nonintegral prop frame grid');frames=columns*rows
 animator=nodes['AnimationPlayer'];require(animator.attrs.get('type')=='AnimationPlayer'and set(animator.props)=={'anims/'+c['name']for c in p['clips']},'Unreviewed prop animation/autoplay binding')
 for c in p['clips']:
  reference=animator.props['anims/'+c['name']];require(isinstance(reference,SubRef),'Prop clip is not local');kind,properties=doc.sub[reference.id];require(kind=='Animation','Prop clip source class')
  require(not(set(properties)-{'resource_name','length','step','loop'}-{k for k in properties if k.startswith('tracks/')}),'Unreviewed prop animation property')
  require(properties.get('resource_name',c['name'])==c['name']and properties.get('loop',False)is False,'Unreviewed prop animation name/loop')
  duration=f32(properties.get('length',1));require(0<duration<=60,'Prop duration domain')
  if 'step'in properties:require(0<f32(properties['step'])<=60,'Prop editor step domain')
  grouped={}
  for key,value in properties.items():
   if not key.startswith('tracks/'):continue
   parts=key.split('/');require(len(parts)==3 and parts[1].isdigit()and str(int(parts[1]))==parts[1],'Prop track field');grouped.setdefault(int(parts[1]),{})[parts[2]]=value
  require(grouped and set(grouped)==set(range(len(grouped))),'Prop source track order')
  keys=[];constant=[]
  for index,t in grouped.items():
   require(set(t)=={'type','path','interp','loop_wrap','imported','enabled','keys'}and t['type']=='value'and type(t['interp'])is int and t['interp']==1 and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'Unreviewed prop source track')
   require(isinstance(t['path'],NodePathValue)and t['path'].path in tracks,'Unreviewed prop source target');kind=tracks[t['path'].path];k=t['keys']
   require(type(k)is dict and set(k)=={'times','transitions','update','values'}and type(k['update'])is int and k['update']in(0,1),'Unreviewed prop key mode')
   require(type(k['times'])is list and type(k['values'])is list and type(k['transitions'])is list and 0<len(k['times'])==len(k['values'])==len(k['transitions'])<=256,'Prop key cardinality')
   require(all(type(v)in(int,float)and v==1 for v in k['transitions']),'Unreviewed prop transition curve')
   if k['update']==0:require(len(k['times'])==1 and k['times'][0]==0,'Continuous prop curves are unsupported');constant.append(index)
   previous=-1
   for time,value in zip(k['times'],k['values']):
    time=f32(time);require(0<=time<=duration and time>previous,'Prop key time order/range');previous=time
    require(type(value)is(int if kind==1 else bool)and(0<=value<frames if kind==1 else True),'Prop key value domain');keys.append(dict(time=time,kind=kind,value=int(value)))
  keys.sort(key=lambda k:(k['time'],k['kind']));require(len({(k['time'],k['kind'])for k in keys})==len(keys),'Duplicate prop key target/time')
  definitions.append(dict(id=c['id'],name=c['name'],duration=duration,keys=keys,constant_tracks=constant))
 require(0<frames<=65535 and 0<=defaults[1]<frames,'Prop initial frame domain')
 mask=int(defaults[2])|int(not defaults[3])<<1|int(not defaults[4])<<2|int(not defaults[5])<<3
 return dict(frames=frames,initial_frame=defaults[1],initial_mask=mask,clips=definitions,sources=ex.sources,scope=p['scope'])

def encode(r):
 policy=struct.pack('<4I',r['frames'],r['initial_frame'],r['initial_mask'],0);clips=bytearray();keys=bytearray()
 for c in r['clips']:
  clips.extend(struct.pack('<If4I',c['id'],c['duration'],len(keys)//12,len(c['keys']),0,0))
  for k in c['keys']:keys.extend(struct.pack('<f2I',k['time'],k['kind'],k['value']))
 return pack_container(MAGIC,0,[policy,bytes(clips),bytes(keys)],STRIDES,LIMIT)

def main():
 p=argparse.ArgumentParser();p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
 require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh prop candidate directory required within build')
 r=source(a.recipe);blob=encode(r);unpack_container(blob,MAGIC,STRIDES,LIMIT);out.mkdir(parents=True)
 (out/'source.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');(out/'source.encpcl').write_bytes(blob)
 print('Prop clip candidate:',len(r['clips']),'source animations,',len(blob),'bytes; no scene/release binding')
if __name__=='__main__':main()
