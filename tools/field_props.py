"""Source-local breakable object data and exact bindings to the compiled map.

The original map is never changed. Candidate output does not stage release
resources; the checked scene consumer must explicitly bind this data.
"""
import argparse,hashlib,json,posixpath,re,struct,sys,shlex,subprocess
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
 require(type(p)is dict and set(p)==set('schema kind commit field_recipe clip_recipe texture instances callbacks map_sha256 scope'.split())|({'output','receipt'}if p.get('schema')in(2,3)else set())|({'contact_recipe'}if p.get('schema')==3 else set()),'Field prop recipe fields')
 require(type(p['schema'])is int and p['schema']in(1,2,3) and p['kind']=='encore.field-props.reviewed-source'and p['commit']==f.PIN,'Field prop recipe schema/pin')
 for key in ('field_recipe','clip_recipe','texture')+(('output','receipt')if p['schema']>=2 else ())+(('contact_recipe',)if p['schema']==3 else ()):require(type(p[key])is str and p[key] and not p[key].startswith('/') and all(v not in ('','.','..')for v in p[key].split('/'))and ':'not in p[key]and '\\'not in p[key],'Field prop recipe path')
 require(p['texture'].startswith('graphics/')and p['texture'].endswith('.t3x'),'Field prop texture destination')
 if p['schema']>=2:require(p['output'].startswith('world/')and p['output'].endswith('.encprops')and p['receipt'].startswith('content/asset-receipts/')and p['receipt'].endswith('.json'),'Field prop output paths')
 if p['schema']==3:require(p['contact_recipe'].startswith('content/')and p['contact_recipe'].endswith('.json'),'Field prop contact recipe identity')
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
  hit_center,hit_extents=shape(path+'/Hitbox/CollisionShape2D');interaction_center,interaction_extents=shape(path+'/interact/CollisionShape2D');callback=f.NONE;callback_path=''
  if 'called_object'in n.props:
   target=n.props['called_object'];require(isinstance(target,NodePathValue),'Field prop callback path type');target=posixpath.normpath(path+'/'+target.path);entry={'path':target,'method':n.props.get('call_object_function','')};require(entry in p['callbacks'],'Field prop unreviewed callback');used_callbacks.add(target)
   callback=exactly([i for i,r in enumerate(m.boundaries)if text(r['path'])==target and r['kind']==2],'callback target')
   callback_path=target
  notice=exactly([i for i,r in enumerate(m.notices)if text(r['path'])==path],'interaction notice')
  row=dict(id=binding['id'],sprite=sprite_index,body=body_index,clone_item=clone_item,appear=flag(n.props.get('flag','')),finish=flag(n.props.get('object_function_disappear_flag','')),callback=callback,callback_path=callback_path,position=position,sprite_center=center,view_center=view_center,view_extents=view_extents,hit_center=hit_center,hit_extents=hit_extents,interact_center=interaction_center,interact_extents=interaction_extents,body_extents=body_extents,notice=notice,path=path)
  rows.append(row)
 require(used_callbacks=={c['path']for c in p['callbacks']},'Field prop unowned callback policy')
 audio=sc.by_path[p['instances'][0]['path']+'/AudioStreamPlayer'];sound=owner_of(audio,'stream').ext_path(audio.props['stream'],'AudioStream');ex.data(sound)
 require(len(clone_parents)==1,'Field prop clone parent domain differs');parent=next(iter(clone_parents));children=sc.children[parent]
 require(children,'Field prop clone parent child order requires review');tail=children[-1]
 require(sc.by_path[tail].type=='TileMap'and not sc.children[tail]and sc.visible(tail),'Field prop clone subtree tail class/children changed')
 clone_after=exactly([i for i,r in enumerate(m.items)if r['kind']==1 and r['first']==m.layer_by_path[tail]],'clone subtree tail')
 require(any(g['kind']==1 and g['first']<=clone_after<g['first']+g['count']for g in m.groups),'Field prop clone tail is sorted')
 policy=dict(scene=recipe['scene']['id'],hidden=clipids['Hidden'],idle=clipids['Idle'],grow=clipids['Grow'],broken=clipids['Break'],clone_frame=int(frame[1]),bat_flag=flags.index('bat'),width=width,height=height,hard=calls[1],easy=calls[0],sound=sound,texture=p['texture'],clone_parent=parent,clone_after=clone_after,rumble=[float(v.strip())for v in rumble[1].split(',')])
 if p['schema']==3:
  contact,more=contact_source(p,m,sound);policy['contact']=contact;ex.sources.update(more)
 return dict(policy=policy,flags=flags,props=rows),prop_clips.encode(clips),dict(commit=f.PIN,sources=dict(sorted({**m.sources.extractor.sources,**ex.sources}.items())),scope=p['scope'])

def contact_source(p,model,sound):
 from tools import field_swing as swing
 from tools.godot_text import Document
 import math,zlib
 source,ex=swing.read(ROOT/p['contact_recipe']);action=swing.source(ROOT/p['contact_recipe']);blob=swing.encode(action);doc=Document(source['scene'],ex.text(source['scene']));hit=next(n for n in doc.nodes if n.attrs.get('parent')=='HitboxPivot'and n.attrs['name']=='BatHitbox');base=Document('emitter',ex.text(doc.ext_path(hit.attrs['instance'],'PackedScene')))
 def area(props):
  require(set(props)<=set('position collision_layer collision_mask monitoring monitorable'.split()),'Field prop unreviewed Area override');layer=props.get('collision_layer',1);mask=props.get('collision_mask',1);monitoring=props.get('monitoring',True);monitorable=props.get('monitorable',True)
  require(type(layer)is type(mask)is int and 0<=layer<2**20 and 0<=mask<2**20 and type(monitoring)is type(monitorable)is bool,'Field prop contact filter domain')
  return [layer,mask,int(monitoring)|int(monitorable)<<1]
 emitter=area(base.nodes[0].props);filters=[];audios=[]
 for binding in p['instances']:
  filters.append(area(model.scene.by_path[binding['path']+'/Hitbox'].props));node=model.scene.by_path[binding['path']+'/AudioStreamPlayer'];props=node.props
  require(node.type=='AudioStreamPlayer'and set(props)<=set('stream bus volume_db pitch_scale pause_mode autoplay stream_paused mix_target'.split()),'Field prop unreviewed sound node/override')
  require(props.get('bus','Master')=='SFX'and type(props.get('pause_mode',0))is int and props.get('pause_mode',0)==0 and props.get('autoplay',False)is False and props.get('stream_paused',False)is False and type(props.get('mix_target',0))is int and props.get('mix_target',0)==0,'Field prop unsupported sound bus/pause/automatic playback')
  require(owner_of(node,'stream').ext_path(props['stream'],'AudioStream')==sound,'Field prop inconsistent per-node sound source');gain=props.get('volume_db',0);pitch=props.get('pitch_scale',1)
  require(type(gain)in(int,float)and math.isfinite(gain)and -256<=gain<=128 and type(pitch)in(int,float)and math.isfinite(pitch)and 0<pitch<=16,'Field prop source gain/pitch domain');audios.append([float(gain),float(pitch)])
 require(all(v==filters[0]for v in filters)and all(v==audios[0]for v in audios)and filters[0][2]&1 and emitter[2]&2 and filters[0][1]&emitter[0],'Field prop common monitored source pair')
 return dict(target=filters[0],emitter=emitter,gain_db=audios[0][0],pitch=audios[0][1],bus=1,pause_mode=0,swing_bytes=len(blob),swing_crc=zlib.crc32(blob),sound_sha256=hashlib.sha256(model.sources.data(sound)).hexdigest()),ex.sources

def strides(capability):return (1,176,1,116,8)if capability==3 else STRIDES

def encode(data,clips,*,capability=2):
 strings=bytearray();spans={}
 def text(v):
  if v not in spans:spans[v]=(len(strings),len(v.encode()));strings.extend(v.encode())
  return spans[v]
 p=data['policy'];policy=struct.pack('<9I',*(p[k]for k in 'scene hidden idle grow broken clone_frame bat_flag width height'.split()))
 for k in ('hard','easy','sound','texture'):policy+=struct.pack('<2I',*text(p[k]))
 require(type(capability)is int and capability in(1,2,3),'Field prop encoding capability')
 policy+=struct.pack('<4f3I',*p['rumble'],*text(p['clone_parent']),p['clone_after']if capability>=2 else 0);props=bytearray()
 if capability==3:
  require(type(p.get('contact'))is dict,'Field prop contact policy is missing')
  c=p['contact'];policy+=struct.pack('<6I2f4I',*c['target'],*c['emitter'],c['gain_db'],c['pitch'],c['bus'],c['pause_mode'],c['swing_bytes'],c['swing_crc'])+bytes.fromhex(c['sound_sha256']);require(len(policy)==176,'Field prop contact policy layout')
 for r in data['props']:
  path=0
  if capability>=2 and r['callback_path']:path=len(strings)+1;strings.extend(r['callback_path'].encode()+b'\0')
  props+=struct.pack('<8I',*(r[k]for k in 'id sprite body clone_item appear finish callback'.split()),path)
  for k in 'position sprite_center view_center view_extents hit_center hit_extents interact_center interact_extents body_extents'.split():props+=struct.pack('<2f',*r[k])
  if capability==3:props+=struct.pack('<3I',r['notice'],*text(r['path']))
 flags=b''.join(struct.pack('<2I',*text(s))for s in data['flags'])
 return f.pack_container(MAGIC,0,[strings,policy,clips,props,flags],strides(capability),LIMIT,capability=capability)

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def producers(p=None):
 p=read()if p is None else p;names=['tools/field_props.py','tools/prop_clips.py','tools/podunk_field.py']
 if p['schema']==3:names+=['tools/field_swing.py','tools/godot_text.py','tools/extract_battle_entry.py',p['contact_recipe']]
 return {n:sha(ROOT/n)for n in names}
def capability(p):return 3 if p['schema']==3 else 2
def texture_bytes(root,p,expected):
 root=Path(root).resolve();target=(root/p['texture']).resolve();require(target.is_relative_to(root)and target.is_file(),'Missing/unsafe field prop texture');blob=target.read_bytes()
 require(type(expected)is dict and set(expected)=={'bytes','sha256'}and type(expected['bytes'])is int and expected['bytes']==len(blob)and hashlib.sha256(blob).hexdigest()==expected['sha256'],'Field prop texture fingerprint differs');return blob
def receipt(p,proof,root=ROOT/'romfs'):
 require(p['schema']in(2,3),'Field props release requires bound output recipe')
 r=json.loads((ROOT/p['receipt']).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(r)is dict and set(r)==set('schema commit policy_sha256 producers sources tex3ds_sha256 texture permission'.split())and type(r['schema'])is int and r['schema']==1 and r['commit']==f.PIN and r['policy_sha256']==sha(RECIPE)and r['producers']==producers(p)and r['sources']==proof['sources'],'Field prop receipt/source changed')
 require(type(r['tex3ds_sha256'])is str and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256'])and r['permission']=='Pinned upstream LICENSE game-related port; original notices retained; no tool binary distributed','Field prop tool/permission record differs')
 return texture_bytes(root,p,r['texture'])
def stage_files(root=ROOT/'romfs'):
 p=read();data,clips,proof=source();blob=encode(data,clips,capability=capability(p));texture=receipt(p,proof,root)
 require((Path(root)/p['output']).read_bytes()==blob,'Field prop resource is stale')
 return {Path(p['output']):blob,Path(p['texture']):texture}
def same_adapter_except(previous,current,names):
 import ast
 def consumed(raw):
  parsed=ast.parse(raw.decode('utf8'));parsed.body=[n for n in parsed.body if not(isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef))and n.name in names)];return ast.dump(parsed,include_attributes=False)
 require(consumed(previous)==consumed(current),'Field prop consumed adapter changed outside reviewed metadata/CLI functions')

def readmit(base):
 """Retain a real conversion only when source, model and pixels are unchanged."""
 require(type(base)is str and re.fullmatch('[0-9a-f]{40}',base),'Field prop readmission needs an exact baseline commit')
 old=lambda path:subprocess.check_output(['git','show',base+':'+path],cwd=ROOT)
 p=read();data,clips,proof=source();require(p['schema']in(2,3),'Field prop release output binding required')
 recipe_path=RECIPE.relative_to(ROOT).as_posix();raw_recipe=RECIPE.read_bytes();require(old(recipe_path)==raw_recipe,'Field prop readmission changed policy')
 baseline=json.loads(old(p['receipt']),object_pairs_hook=unique);current=json.loads((ROOT/p['receipt']).read_bytes(),object_pairs_hook=unique);require(current==baseline,'Field prop readmission baseline receipt differs')
 require(set(baseline)==set('schema commit policy_sha256 producers sources tex3ds_sha256 texture permission'.split())and baseline['schema']==1 and baseline['commit']==f.PIN and baseline['policy_sha256']==hashlib.sha256(raw_recipe).hexdigest()and baseline['sources']==proof['sources'],'Field prop readmission source/policy changed')
 require(set(baseline['producers'])==set(producers()),'Field prop readmission producer coverage')
 for path,digest in baseline['producers'].items():
  before=old(path);after=(ROOT/path).read_bytes();require(hashlib.sha256(before).hexdigest()==digest,'Field prop original producer digest differs')
  if path=='tools/podunk_field.py':same_adapter_except(before,after,{'build_room'})
  elif path=='tools/field_props.py':same_adapter_except(before,after,{'main','readmit','same_adapter_except'})
  else:require(before==after,'Field prop consumed clip producer changed')
 require(encode(data,clips,capability=capability(p))==old('romfs/'+p['output'])==(ROOT/'romfs'/p['output']).read_bytes(),'Field prop readmission changes resource semantics')
 texture_bytes(ROOT/'romfs',p,baseline['texture'])
 require(type(baseline['tex3ds_sha256'])is str and re.fullmatch('[0-9a-f]{64}',baseline['tex3ds_sha256'])and baseline['permission']=='Pinned upstream LICENSE game-related port; original notices retained; no tool binary distributed','Field prop original conversion/permission changed')
 updated=dict(baseline,producers=producers());(ROOT/p['receipt']).write_bytes((json.dumps(updated,indent=2)+'\n').encode());receipt(p,proof)
 print('Field prop original texture retained; canonical resource, consumed adapter AST, source and actual texture unchanged; metadata/CLI provenance re-admitted')

def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',nargs='?',default='candidate',choices=('candidate','assets','compile','verify','readmit'));ap.add_argument('--recipe',type=Path,default=RECIPE);ap.add_argument('--out',type=Path);ap.add_argument('--tex3ds',type=Path);ap.add_argument('--tex3ds-command');ap.add_argument('--base');a=ap.parse_args()
 if a.action=='readmit':require(a.recipe.resolve()==RECIPE.resolve(),'Field prop readmission uses the release policy only');readmit(a.base);return
 require(a.base is None,'Field prop baseline only applies to readmission')
 p=read(a.recipe);data,clips,proof=source(a.recipe);cap=capability(p);blob=encode(data,clips,capability=cap);f.unpack_container(blob,MAGIC,strides(cap),LIMIT,capability=cap)
 if a.action=='candidate':
  require(a.out is not None,'Candidate output required');out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned field prop candidate required');out.mkdir(parents=True);(out/'source.encprops').write_bytes(blob);(out/'source.json').write_bytes((json.dumps(dict(data=data,proof=proof),indent=2)+'\n').encode())
 else:
  require(a.recipe.resolve()==RECIPE.resolve()and p['schema']in(2,3),'Release props require the reviewed output recipe')
  if a.action=='assets':
   require(a.tex3ds and a.tex3ds.is_file(),'Actual tex3ds required');cp,ex=prop_clips.read(ROOT/p['clip_recipe']);require("purpose you're using them is related to the game"in ex.text('LICENSE'),'Field prop asset permission changed')
   target=ROOT/'romfs'/p['texture'];target.parent.mkdir(parents=True,exist_ok=True);image=ROOT/'upstream/MOTHER-Encore'/cp['texture'];command=shlex.split(a.tex3ds_command)if a.tex3ds_command else None;rel=lambda v:v.resolve().relative_to(ROOT).as_posix()
   args=command+['-f','rgba8','-z','none','-o',rel(target),rel(image)]if command else[str(a.tex3ds),'-f','rgba8','-z','none','-o',str(target),str(image)]
   subprocess.run(args,cwd=ROOT,check=True)
   r=dict(schema=1,commit=f.PIN,policy_sha256=sha(RECIPE),producers=producers(),sources=proof['sources'],tex3ds_sha256=sha(a.tex3ds),texture=dict(bytes=target.stat().st_size,sha256=sha(target)),permission='Pinned upstream LICENSE game-related port; original notices retained; no tool binary distributed')
   dest=ROOT/p['receipt'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((json.dumps(r,indent=2)+'\n').encode())
  else:receipt(p,proof)
  target=ROOT/'romfs'/p['output']
  if a.action=='verify':require(target.read_bytes()==blob,'Field prop resource is stale')
  else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
 print('Field props:',len(data['props']),'source objects,',len(blob),'bytes; action',a.action)
if __name__=='__main__':main()
