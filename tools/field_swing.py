"""Reviewed Ninten swing tracks; an offline typed action, never a script VM.

Candidate only until a field owner supplies input, sound, drawing and overlap
callbacks. Original AnimationPlayer bindings, not subresource numbering, own
the eight directions (UpRight is source subresource 550).
"""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,SubRef,ExtRef,NodePathValue,Vec2
from tools import podunk_field as f
RECIPE=ROOT/'content/field-swing.json';MAGIC=b'ENCSWG01';STRIDES=(128,24,16,1);LIMIT=64*1024

def read(path=RECIPE):
 def unique(pairs):
  out={}
  for key,value in pairs:require(key not in out,'Duplicate swing recipe field');out[key]=value
  return out
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind commit scene script skill bat_script fetcher texture bat_texture sound clips tracks sources scope'.split()),'Swing recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-swing.reviewed-source'and p['commit']==PIN,'Swing recipe schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Swing scope')
 require(type(p['sources'])is dict and all(type(k)is str and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['sources'].items()),'Swing reviewed identities')
 require(all(type(p[k])is str and p[k]in p['sources']for k in ('scene','script','skill','bat_script','fetcher','texture','bat_texture','sound')),'Swing source bindings')
 require(type(p['clips'])is list and len(p['clips'])==8 and all(type(c)is dict and set(c)=={'id','name'}and type(c['id'])is int and 0<c['id']<=65535 and type(c['name'])is str and c['name']for c in p['clips']),'Swing clip bindings')
 require(len({c['id']for c in p['clips']})==8 and len({c['name']for c in p['clips']})==8,'Duplicate swing clip')
 require(type(p['tracks'])is list and len(p['tracks'])==8 and all(type(t)is dict and set(t)=={'kind','path'}and type(t['kind'])is int and 1<=t['kind']<=8 and type(t['path'])is str for t in p['tracks']),'Swing tracks')
 require({t['kind']for t in p['tracks']}==set(range(1,9))and len({t['path']for t in p['tracks']})==8,'Duplicate swing target')
 ex=Extractor(ROOT)
 for name,digest in p['sources'].items():require(hashlib.sha256(ex.data(name)).hexdigest()==digest,'Reviewed swing source changed: '+name)
 return p,ex

def source(path=RECIPE):
 p,ex=read(path);doc=Document(p['scene'],ex.text(p['scene']));nodes={}
 for n in doc.nodes:
  parent=n.attrs.get('parent');name=n.attrs['name'];key='.'if parent is None else name if parent=='.'else parent+'/'+name
  require(key not in nodes,'Duplicate swing node');nodes[key]=n
 require(doc.ext_path(nodes['.'].props['script'],'Script')==p['script'],'Swing player script differs')
 ap=nodes['AnimationPlayer'];require(ap.attrs.get('type')=='AnimationPlayer'and ap.props.get('method_call_mode',0)==0,'Swing method callback mode')
 special=nodes['SpecialAnimations'];bat=nodes['SpecialAnimations/NintenBat'];pivot=nodes['HitboxPivot'];hit=nodes['HitboxPivot/BatHitbox'];shape=nodes['HitboxPivot/BatHitbox/CollisionShape2D']
 for n,key in ((special,'texture'),(bat,'bat_texture')):
  require(n.attrs.get('type')=='Sprite'and doc.ext_path(n.props['texture'],'Texture')==p[key],'Swing sprite texture')
  require(n.props.get('hframes',1)==10 and n.props.get('vframes',1)==10 and ex.png_size(p[key])==[640,640],'Swing sprite frame geometry')
 require(doc.ext_path(bat.props['script'],'Script')==p['bat_script'],'Swing separate bat script')
 require(doc.ext_path(hit.attrs['instance'],'PackedScene')in p['sources'],'Swing hitbox dependency')
 hitdoc=Document('hitbox',ex.text(doc.ext_path(hit.attrs['instance'],'PackedScene')))
 require(hitdoc.nodes[0].attrs.get('type')=='Area2D'and hitdoc.nodes[0].props=={'collision_layer':2,'collision_mask':2},'Swing hitbox filter')
 kind,geometry=doc.sub[shape.props['shape'].id]
 require(kind=='CapsuleShape2D'and set(geometry)=={'radius','height'}and all(type(v)in(int,float)and math.isfinite(v)and v>0 for v in geometry.values()),'Swing source capsule')
 require(shape.props.get('disabled',False)is True and all(k not in n.props for n in (special,bat,pivot,hit,shape)for k in ('scale','transform','skew')),'Swing initial geometry')
 skill=ex.yaml(p['skill']);require(set(skill)=={'name','description','is_skill_button','flag','usable'}and skill['is_skill_button']is True and skill['usable']=={'ninten':True,'ana':False,'lloyd':False,'teddy':False,'pippi':False},'Swing source skill ownership')
 code=ex.text(p['script']);rates=re.findall(r'_anim_tree.set\("parameters/Bat/TimeScale/scale", ([0-9.]+)\)',code)
 require(rates==['0.7','1']and 'if _party_member.is_incapacitated():'in code and '$AudioStreamPlayer.stream = load("res://'+p['sound']+'")'in code,'Swing source speed/sound')
 vibration=re.search(r'"swing":\n\s+\$AudioStreamPlayer.stream.*?global.start_joy_vibration\(([^)]+)\)',code,re.S)
 require(vibration is not None,'Swing source vibration');rumble=[float(v.strip())for v in vibration[1].split(',')];require(len(rumble)==4,'Swing vibration arity')
 require(ex.text(p['bat_script']).count('frame = special.get_frames()')==1 and 'return _sprite_node.frame'in ex.text(p['fetcher']),'Swing separate bat frame ownership')
 fetch=nodes['SpriteDataFetcher2'];require(fetch.props['_sprite_path']==NodePathValue('../SpecialAnimations'),'Swing special frame fetch binding')
 directions={}
 for kind,props in doc.sub.values():
  if kind!='AnimationNodeBlendSpace2D':continue
  refs=[v for k,v in props.items()if re.fullmatch('blend_point_[0-9]+/node',k)]
  if not refs:continue
  names=[doc.sub[ref.id][1].get('animation')for ref in refs if isinstance(ref,SubRef)]
  if set(names)!={c['name']for c in p['clips']}:continue
  require(not directions and len(refs)==8 and props.get('blend_mode',0)==1,'Swing discrete direction selection')
  for i in range(8):
   ref=props['blend_point_'+str(i)+'/node'];pos=props['blend_point_'+str(i)+'/pos'];require(isinstance(pos,Vec2),'Swing direction point')
   directions[doc.sub[ref.id][1]['animation']]=[pos.x,pos.y]
 require(len(directions)==8,'Swing direction graph missing')
 tracks={t['path']:t['kind']for t in p['tracks']};clips=[]
 for c in p['clips']:
  ref=ap.props.get('anims/'+c['name']);require(isinstance(ref,SubRef),'Swing AnimationPlayer binding missing');kind,props=doc.sub[ref.id]
  require(kind=='Animation'and props.get('resource_name',c['name'])==c['name']and props.get('loop',False)is False,'Swing source animation')
  require(not(set(props)-{'resource_name','length','step','loop'}-{k for k in props if k.startswith('tracks/')}),'Unreviewed swing clip property')
  duration=props.get('length',1);require(type(duration)in(int,float)and math.isfinite(duration)and 0<duration<=10,'Swing source duration');keys=[]
  grouped={}
  for key,value in props.items():
   if key.startswith('tracks/'):
    fields=key.split('/');require(len(fields)==3 and fields[1].isdigit()and str(int(fields[1]))==fields[1],'Swing track field');grouped.setdefault(int(fields[1]),{})[fields[2]]=value
  require(set(grouped)==set(range(8)),'Swing track coverage/order');seen=set()
  for index,t in grouped.items():
   require(set(t)=={'type','path','interp','loop_wrap','imported','enabled','keys'}and type(t['interp'])is int and t['interp']==1 and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'Unreviewed swing track settings')
   require(isinstance(t['path'],NodePathValue)and t['path'].path in tracks,'Swing track target');track=tracks[t['path'].path];seen.add(track);k=t['keys']
   require(type(k)is dict and set(k)==({'times','transitions','values'}if track==8 else{'times','transitions','values','update'}),'Swing key fields')
   require(t['type']==('method'if track==8 else'value')and all(type(v)in(int,float)and v==1 for v in k['transitions']),'Swing track type/curve')
   require(type(k['times'])is list and type(k['transitions'])is list and type(k['values'])is list and 0<len(k['times'])==len(k['transitions'])==len(k['values'])<=16,'Swing key counts')
   if track!=8:
    require(type(k['update'])is int and k['update']in(0,1),'Swing update mode')
    if k['update']==0:require(track==2 and k['times']==[0.0],'Unsupported continuous swing curve')
   previous=-1
   for time,value in zip(k['times'],k['values']):
    require(type(time)in(int,float)and math.isfinite(time)and previous<time<=duration and time>=0,'Swing key time');previous=time
    if track==1:require(type(value)is int and 0<=value<100,'Swing frame')
    elif track==2:require(type(value)in(int,float)and math.isfinite(value)and abs(value)<=360,'Swing pivot angle')
    elif track in(3,4,5,7):require(type(value)is bool,'Swing boolean property');value=int(value)
    elif track==6:require(isinstance(value,ExtRef)and doc.ext_path(value,'Texture')==p['texture'],'Swing animated texture');value=0
    else:require(value=={'args':[],'method':'_attack_animation_finished'},'Unknown swing method callback');value=0
    keys.append(dict(time=time,kind=track,value=value,order=index))
  require(seen==set(range(1,9))and sum(k['kind']==8 for k in keys)==1,'Swing typed track completeness')
  keys.sort(key=lambda k:(k['time'],k['order']));clips.append(dict(id=c['id'],name=c['name'],direction=directions[c['name']],duration=duration,keys=keys))
 def position(n):
  v=n.props.get('position',Vec2(0,0));require(isinstance(v,Vec2),'Swing source position');return[v.x,v.y]
 # Policy is a fixed checked schema. All values originate in reviewed nodes.
 policy=[100,special.props.get('frame',0),int(nodes['Position/main'].props.get('visible',True))|int(special.props.get('visible',True))<<1|int(not shape.props.get('disabled',False))<<2|int(bat.props.get('show_behind_parent',False))<<3,0]
 real=[float(rates[1]),float(rates[0]),*position(special),*position(pivot),pivot.props.get('rotation',0),*position(hit),hit.props.get('rotation',0),*position(shape),shape.props.get('rotation',0),geometry['radius'],geometry['height'],*rumble]
 require(len(real)==19 and all(type(v)in(int,float)and math.isfinite(v)for v in real),'Swing geometry cardinality')
 require(type(special.props.get('frame',0))is int and 0<=special.props.get('frame',0)<100 and all(type(n.props.get(key,default))is bool for n,key,default in ((nodes['Position/main'],'visible',True),(special,'visible',True),(shape,'disabled',False),(bat,'show_behind_parent',False))),'Swing initial property types')
 clips.sort(key=lambda c:list(directions).index(c['name']))
 return dict(policy=policy,real=real,strings=[p['texture'],p['bat_texture'],p['sound'],skill['flag']],clips=clips,sources=ex.sources,scope=p['scope'])

def encode(r):
 strings=bytearray();refs=[]
 for value in r['strings']:refs.extend([len(strings),len(value.encode('utf8'))]);strings.extend(value.encode('utf8'))
 policy=struct.pack('<4I19f8I',*r['policy'],*r['real'],*refs)+bytes(4);require(len(policy)==128,'Swing policy layout')
 clips=bytearray();keys=bytearray()
 for c in r['clips']:
  clips.extend(struct.pack('<I3f2I',c['id'],*c['direction'],c['duration'],len(keys)//16,len(c['keys'])))
  for k in c['keys']:keys.extend(struct.pack('<fIfI',k['time'],k['kind'],k['value'],k['order']))
 return f.pack_container(MAGIC,0,[policy,bytes(clips),bytes(keys),bytes(strings)],STRIDES,LIMIT)

def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned swing candidate directory required')
 r=source();blob=encode(r);f.unpack_container(blob,MAGIC,STRIDES,LIMIT);out.mkdir(parents=True);(out/'source.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');(out/'swing.encswing').write_bytes(blob);print('Swing:',len(r['clips']),'source directions;',len(blob),'candidate bytes; not release-bound')
if __name__=='__main__':main()
