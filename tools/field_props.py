"""Source-local breakable object data and exact bindings to the compiled map.

The original map is never changed. Candidate output does not stage release
resources; the checked scene consumer must explicitly bind this data.
"""
import argparse,hashlib,json,posixpath,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f,prop_clips
from tools.extract_battle_entry import require
from tools.godot_text import Vec2,NodePathValue,owner_of
RECIPE=ROOT/'content/field-props.json';MAGIC=b'ENCPRP01';STRIDES=(1,96,1,104,8);LIMIT=128*1024

def unique(pairs):
 out={}
 for k,v in pairs:require(k not in out,'Duplicate field prop recipe field');out[k]=v
 return out

def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind commit field_recipe clip_recipe texture instances callbacks map_sha256 scope'.split()),'Field prop recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-props.reviewed-source'and p['commit']==f.PIN,'Field prop recipe schema/pin')
 for key in ('field_recipe','clip_recipe','texture'):require(type(p[key])is str and p[key] and not p[key].startswith('/') and all(v not in ('','.','..')for v in p[key].split('/'))and ':'not in p[key]and '\\'not in p[key],'Field prop recipe path')
 require(p['texture'].startswith('graphics/')and p['texture'].endswith('.t3x'),'Field prop texture destination')
 require(type(p['instances'])is list and 0<len(p['instances'])<=128,'Field prop instance capacity')
 ids=set();paths=set()
 for row in p['instances']:
  require(type(row)is dict and set(row)=={'id','path'}and type(row['id'])is int and 0<row['id']<2**32 and type(row['path'])is str and row['path'] and row['id']not in ids and row['path']not in paths,'Field prop instance identity');ids.add(row['id']);paths.add(row['path'])
 require(type(p['callbacks'])is list and all(type(c)is dict and set(c)=={'path','method'}and type(c['path'])is str and c['path']and c['method']=='check_start'for c in p['callbacks'])and len({c['path']for c in p['callbacks']})==len(p['callbacks']),'Field prop callback binding')
 require(type(p['map_sha256'])is str and re.fullmatch('[0-9a-f]{64}',p['map_sha256'])and type(p['scope'])is str and 0<len(p['scope'])<=2048,'Field prop source identity/scope')
 return p

def source(path=RECIPE):
 p=read(path);cp,ex=prop_clips.read(ROOT/p['clip_recipe']);clips=prop_clips.source(ROOT/p['clip_recipe']);recipe=f.load_recipe(ROOT/p['field_recipe']);m=f.Model(recipe);sc=m.scene
 require(hashlib.sha256(m.sources.data(recipe['scene']['source'])).hexdigest()==p['map_sha256'],'Field prop reviewed map changed')
 script=ex.text(cp['script']);frame=re.search(r'Roots.frame = ([0-9]+)',script);rumble=re.search(r'global.start_joy_vibration\(([^)]+)\)',script)
 require(frame and rumble and 'yield(_anim_player, "animation_finished")'in script and 'anim_name == "Break" and globaldata.flags.has(object_function_disappear_flag)'in script,'Field prop lifecycle source changed')
 calls=re.findall(r'uiManager.open_dialogue_box\("([^\"]+)"\)',script);require(len(calls)==2 and 'if globaldata.flags["bat"]:'in script,'Field prop interaction policy changed')
 sprite=sc.by_path[p['instances'][0]['path']+'/Sprite'];texture=owner_of(sprite,'texture').ext_path(sprite.props['texture'],'Texture');require(texture==cp['texture'],'Field prop texture source differs')
 tw,th=ex.png_size(texture);width=tw//sprite.props.get('hframes',1);height=th//sprite.props.get('vframes',1)
 require(width*sprite.props.get('hframes',1)==tw and height*sprite.props.get('vframes',1)==th,'Field prop frame grid')
 clipids={c['name']:c['id']for c in clips['clips']};flags=['bat']
 def flag(name):
  require(type(name)is str,'Field prop flag type')
  if not name:return f.NONE
  if name not in flags:flags.append(name)
  return flags.index(name)
 actual=[n.path for n in sc.nodes if n.scene==cp['scene']and n.type=='Node2D'];require(actual==[r['path']for r in p['instances']],'Field prop source instance order/coverage differs')
 def text(pair):return m.strings[pair[0]:pair[0]+pair[1]].decode()
 def exactly(values,label):require(len(values)==1,'Field prop ambiguous '+label);return values[0]
 def point(path):
  x=sc.global_xf[path];require((x.xx,x.xy,x.yx,x.yy)==(1,0,0,1),'Field prop transformed owner');return [f.f32(x.ox),f.f32(x.oy)]
 def shape(path):
  n=sc.by_path[path];d=owner_of(n,'shape');require(d.sub_type(n.props['shape'])=='RectangleShape2D'and not n.props.get('disabled',False),'Field prop shape source type/state');e=d.sub_resource(n.props['shape'])['extents'];return point(path),[f.f32(e.x),f.f32(e.y)]
 rows=[];used_callbacks=set();clone_parents=set()
 for binding in p['instances']:
  path=binding['path'];n=sc.by_path[path];require(set(n.props)<={'script','position','flag','New_parent','called_object','call_object_function','object_function_disappear_flag'},'Field prop unreviewed instance override')
  parent=n.props['New_parent'];require(isinstance(parent,NodePathValue),'Field prop clone parent path type');parent=posixpath.normpath(path+'/'+parent.path);layer=m.layer_by_path[parent]
  clone_parents.add(parent)
  require(point(parent)==[0,0]and not m.layers[layer]['flags']&1,'Field prop source clone parent transform/order')
  clone_item=exactly([i for i,r in enumerate(m.items)if r['kind']==1 and r['first']==layer],'clone layer')
  center=point(path+'/Sprite');position=point(path);sp=sc.by_path[path+'/Sprite'];require(sp.props==sprite.props,'Field prop source sprite override')
  sprite_index=exactly([i for i,s in enumerate(m.sprites)if s['x']==center[0]-width/2 and s['y']==center[1]-height/2 and s['w']==width and s['h']==height],'sprite join')
  imagekeys=[m.images.keys[i]for i in m.pictures.rows[m.sprites[sprite_index]['picture']]['frames']];require(imagekeys==[(texture,width*sp.props['frame'],0,width,height)],'Field prop map source picture differs')
  body_center,body_extents=shape(path+'/StaticBody2D/CollisionShape2D');require(body_center==position,'Field prop body offset scope')
  body_index=exactly([i for i,b in enumerate(m.bodies)if b['count']==1 and (s:=m.shape_rows[b['first']])['minx']==position[0]-body_extents[0]and s['maxx']==position[0]+body_extents[0]and s['miny']==position[1]-body_extents[1]and s['maxy']==position[1]+body_extents[1]],'body join')
  view=sc.by_path[path+'/VisibilityNotifier2D'];vx=sc.global_xf[view.path];rect=view.props.get('rect',f.Rect2(-10,-10,20,20));require(vx.xy==vx.yx==0 and vx.xx>0 and vx.yy>0,'Field prop viewport transform');view_center=[f.f32(vx.ox+(rect.x+rect.w/2)*vx.xx),f.f32(vx.oy+(rect.y+rect.h/2)*vx.yy)];view_extents=[f.f32(rect.w*vx.xx/2),f.f32(rect.h*vx.yy/2)]
  hit_center,hit_extents=shape(path+'/Hitbox/CollisionShape2D');interaction_center,interaction_extents=shape(path+'/interact/CollisionShape2D');callback=f.NONE
  if 'called_object'in n.props:
   target=n.props['called_object'];require(isinstance(target,NodePathValue),'Field prop callback path type');target=posixpath.normpath(path+'/'+target.path);entry={'path':target,'method':n.props.get('call_object_function','')};require(entry in p['callbacks'],'Field prop unreviewed callback');used_callbacks.add(target)
   callback=exactly([i for i,r in enumerate(m.boundaries)if text(r['path'])==target and r['kind']==2],'callback target')
  row=dict(id=binding['id'],sprite=sprite_index,body=body_index,clone_item=clone_item,appear=flag(n.props.get('flag','')),finish=flag(n.props.get('object_function_disappear_flag','')),callback=callback,position=position,sprite_center=center,view_center=view_center,view_extents=view_extents,hit_center=hit_center,hit_extents=hit_extents,interact_center=interaction_center,interact_extents=interaction_extents,body_extents=body_extents)
  rows.append(row)
 require(used_callbacks=={c['path']for c in p['callbacks']},'Field prop unowned callback policy')
 audio=sc.by_path[p['instances'][0]['path']+'/AudioStreamPlayer'];sound=owner_of(audio,'stream').ext_path(audio.props['stream'],'AudioStream');ex.data(sound)
 require(len(clone_parents)==1,'Field prop clone parent domain differs')
 policy=dict(scene=recipe['scene']['id'],hidden=clipids['Hidden'],idle=clipids['Idle'],grow=clipids['Grow'],broken=clipids['Break'],clone_frame=int(frame[1]),bat_flag=flags.index('bat'),width=width,height=height,hard=calls[1],easy=calls[0],sound=sound,texture=p['texture'],clone_parent=next(iter(clone_parents)),rumble=[float(v.strip())for v in rumble[1].split(',')])
 return dict(policy=policy,flags=flags,props=rows),prop_clips.encode(clips),dict(commit=f.PIN,sources=dict(sorted({**m.sources.extractor.sources,**ex.sources}.items())),scope=p['scope'])

def encode(data,clips):
 strings=bytearray();spans={}
 def text(v):
  if v not in spans:spans[v]=(len(strings),len(v.encode()));strings.extend(v.encode())
  return spans[v]
 p=data['policy'];policy=struct.pack('<9I',*(p[k]for k in 'scene hidden idle grow broken clone_frame bat_flag width height'.split()))
 for k in ('hard','easy','sound','texture'):policy+=struct.pack('<2I',*text(p[k]))
 policy+=struct.pack('<4f3I',*p['rumble'],*text(p['clone_parent']),0);props=bytearray()
 for r in data['props']:
  props+=struct.pack('<8I',*(r[k]for k in 'id sprite body clone_item appear finish callback'.split()),0)
  for k in 'position sprite_center view_center view_extents hit_center hit_extents interact_center interact_extents body_extents'.split():props+=struct.pack('<2f',*r[k])
 flags=b''.join(struct.pack('<2I',*text(s))for s in data['flags'])
 return f.pack_container(MAGIC,0,[strings,policy,clips,props,flags],STRIDES,LIMIT)

def main():
 p=argparse.ArgumentParser();p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned field prop candidate required');data,clips,proof=source(a.recipe);blob=encode(data,clips);f.unpack_container(blob,MAGIC,STRIDES,LIMIT);out.mkdir(parents=True);(out/'source.encprops').write_bytes(blob);(out/'source.json').write_bytes((json.dumps(dict(data=data,proof=proof),indent=2)+'\n').encode());print('Field props:',len(data['props']),'source objects,',len(blob),'bytes; candidate, no release staging')
if __name__=='__main__':main()
