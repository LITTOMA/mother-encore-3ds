#!/usr/bin/env python3
"""All original Podunk OpenableDoor setget/Ready/animation/interaction data."""
from __future__ import annotations
import argparse,csv,hashlib,io,json,re,struct,subprocess,sys,zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
SCRIPT='Scripts/Main/Openable Door.gd';SOURCE='Nodes/Overworld/Objects/Openable Door.tscn';INVENTORY='Scripts/global/Inventory.gd'
IR=ROOT/'content/podunk-openable-door.json';REVIEW=ROOT/'compatibility/reviews/podunk-openable-door-v0410.json';ASSETS=ROOT/'content/podunk-openable-door-assets.json';OUT=ROOT/'romfs/data/podunk.encopenable'
def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(g['sources']);nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};ss={r['source'][6:]:r for r in d['scene_states']};roots=[]
 require(d['source']=='res://'+SCENE and g['export_sha256']==sha(native) and len(nm)==8686,'Incomplete OpenableDoor source export')
 def visit(root,f):
  roots.append((root,f))
  for n in ss[f]['nodes']:
   if n['instance']:
    p=n['path'].removeprefix('./');visit(p if root=='.' else root+'/'+p,rs[n['instance']['id']]['path'][6:])
 visit('.',SCENE);ov={}
 for root,f in sorted(roots,key=lambda x:x[0].count('/') if x[0]!='.' else -1,reverse=True):
  for n in ss[f]['nodes']:
   p=n['path'].removeprefix('./');p=root if p=='.' else p if root=='.' else root+'/'+p
   if p in nm:ov.setdefault(p,{}).update(decode(n['properties']))
 text=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text(encoding='utf-8')
 require(re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',text,re.M)==['_set_texture','_set_offset','_update_positions','_ready','_update_door_state','_on_Area2D_body_entered','_on_Area2D_body_exited','_on_Timer_timeout','close','open','unlock','lock','_use_key','interact','interact_item','_is_valid_body_in_area'],'Unreviewed OpenableDoor method')
 def default(name):
  raw=re.search(r'var '+re.escape(name)+r'\s*:?=\s*([^\n#]+)',text)[1].strip()
  if raw.startswith('"'):return raw.strip('"')
  if raw in ['true','false']:return raw=='true'
  if raw.startswith('Vector2'):return [float(v) for v in re.search(r'\(([^)]+)\)',raw)[1].split(',')]
  return float(raw)
 keys=['door_offset','sound','end_sound','interact_doorblocked','interact_lockopened','interact_locklocked','key','blocked','locked','remove_key','flag','activates_flag','deactivates_flag','one_way'];defaults={k:default(k) for k in keys};textures=[];texturemap={};records=[];clips=[]
 paths=['Sprite','Area2D','interact','StaticBody2D','NonPlayerStaticBody2D','StaticBody2D/CollisionShape2D','NonPlayerStaticBody2D/CollisionShape2D','Area2D/CollisionShape2D','interact/CollisionShape2D','interact/ButtonPrompt','AudioStreamPlayer','Timer','AnimationPlayer']
 properties={'Sprite:visible':1,'StaticBody2D/CollisionShape2D:disabled':2,'NonPlayerStaticBody2D/CollisionShape2D:disabled':3,'AudioStreamPlayer:playing':4}
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];world=decode(nm[p]['world_transform']);require(world[0][1]==0 and world[1][0]==0,'OpenableDoor native parent basis requires a rotated/sheared GPU consumer');v=dict(defaults);v.update({k:x for k,x in ov[p].items() if k in keys});children={q:decode(nm[p+'/'+q]['properties']) for q in paths};sp=children['Sprite'];ap=children['AudioStreamPlayer'];tp=children['Timer'];a=children['AnimationPlayer'];nativeclips=[]
  require(tp['one_shot'] and not tp['autostart'] and tp['process_mode']==1 and not tp.get('paused',False),'Unsupported source Door timer')
  require(sp['rotation']==0 and not sp['flip_h'] and not sp['flip_v'] and not sp['region_enabled'] and sp['hframes']==sp['vframes']==1 and sp['centered'],'Unreviewed source Door Sprite geometry')
  for name in ['Action','Normal','RESET']:
   q=decode(rs[a['anims/'+name]['id']]['properties']);tracks=[];i=0
   while 'tracks/'+str(i)+'/type' in q:
    prefix='tracks/'+str(i)+'/';key=dict(q[prefix+'keys']['pairs']);path=q[prefix+'path']['value'];require(q[prefix+'type']=='value' and path in properties and q[prefix+'enabled'] and q[prefix+'interp']==1 and q[prefix+'loop_wrap'] and key['update']in[0,1] and key['times']==[0] and key['transitions']==[1] and len(key['values'])==1 and type(key['values'][0])is bool,'Unreviewed Door animation key/track')
    tracks.append(dict(property=properties[path],update=key['update'],value=int(key['values'][0])));i+=1
   nativeclips.append(dict(role=['Action','Normal','RESET'].index(name)+1,name=name,length=q['length'],loop=q['loop'],tracks=tracks))
  if nativeclips not in clips:clips.append(nativeclips)
  profile=clips.index(nativeclips)
  tref=ov[p].get('sprite');require(tref,'Source setget sprite property absent');texture=rs[tref['id']];source=texture['path'][6:];region=[0,0,0,0]
  if texture['class']=='AtlasTexture':
   at=decode(texture['properties']);require(at['margin']==[[0,0],[0,0]],'Door Atlas margin needs an admitted layout mechanism');source=rs[at['atlas']['id']]['path'][6:];region=at['region'][0]+at['region'][1]
  else:require(texture['class'] in ['StreamTexture','ImageTexture'],'Unknown Door texture class')
  from PIL import Image
  with Image.open(ROOT/'upstream/MOTHER-Encore'/source) as image:
   if region[2]==0:region=[0,0,*image.size]
   require(all(int(x)==x for x in region) and region[0]>=0 and region[1]>=0 and region[2]>0 and region[3]>0 and region[0]+region[2]<=image.width and region[1]+region[3]<=image.height,'Source Door atlas bounds')
  token=source+':'+','.join(str(int(x)) for x in region);texture_id=int.from_bytes(hashlib.sha256(token.encode()).digest()[:4],'little');require(texture_id,'Zero Door texture ID')
  if token not in texturemap:
   texturemap[token]=len(textures);textures.append(dict(id=texture_id,source=source,region=region,path='graphics/objects/doors/'+str(texture_id)+'.t3x'))
  flags=sum(int(x)<<i for i,x in enumerate([v['blocked'],v['locked'],v['remove_key'],v['one_way'],sp['visible'],children['StaticBody2D/CollisionShape2D']['disabled'],children['NonPlayerStaticBody2D/CollisionShape2D']['disabled']]))
  record=dict(id=b['stable_id'],ready=b['ready_ordinal'],node=p,children=[stable(p+'/'+q) for q in paths],texture=texturemap[token],profile=profile,flags=flags,offset=v['door_offset'],initial_sprite_position=sp['position'],sprite_offset=sp['offset'],sprite_scale=sp['scale'],positions=[children[q]['position'] for q in ['Area2D','interact','StaticBody2D','NonPlayerStaticBody2D']],timer_wait=tp['wait_time'],gain_db=ap['volume_db'],bus=ap['bus'],key=v['key'],flag=v['flag'],activate=v['activates_flag'],deactivate=v['deactivates_flag'],sound='Audio/Sound effects/'+v['sound'] if v['sound']!='None' else v['sound'],end_sound='Audio/Sound effects/'+v['end_sound'] if v['end_sound']!='None' else v['end_sound'],dialogs=[v[k] for k in ['interact_doorblocked','interact_lockopened','interact_locklocked']]);records.append(record)
 require(len(records)==10 and clips,'Complete OpenableDoor coverage differs');records.sort(key=lambda r:r['ready']);dialogs=[]
 csvpath='Translations/TranslatedText/dialogue_Reusable - sheet.csv';table={r['key']:r for r in csv.DictReader(io.StringIO((ROOT/'upstream/MOTHER-Encore'/csvpath).read_text(encoding='utf-8')))}
 for program in dict.fromkeys(p for n in records for p in n['dialogs']):
  file='Data/Dialogue/'+program+'.yaml';raw=(ROOT/'upstream/MOTHER-Encore'/file).read_text(encoding='utf-8');key=re.fullmatch(r"'0':\s*\n\s*text: ([A-Z_0-9]+)\s*",raw)[1];require(key in table,'Door source dialogue translation absent');dialogs.append(dict(program=program,source=file,key=key,en=table[key]['en'],zh_CN=table[key]['zh_CN']));sources[file]=inv[file]['sha256']
 bash=re.search(r'load\("res://(Audio/Sound effects/bash.mp3)"\)',text)[1];sounds=list(dict.fromkeys([bash]+[s for n in records for s in [n['sound'],n['end_sound']] if s!='None']))
 for name in [SCRIPT,SOURCE,INVENTORY,'Scripts/global/Item.gd','Scripts/global/global.gd',csvpath,'LICENSE']+sounds+[n['source'] for n in textures]:sources[name]=inv[name]['sha256']
 for texture in textures:
  name=texture['source']+'.import';text_import=(ROOT/'upstream/MOTHER-Encore'/name).read_text(encoding='utf-8');require(all(v in text_import for v in ['flags/filter=false','flags/mipmaps=false','process/premult_alpha=false']),'Unreviewed Door texture flags');sources[name]=inv[name]['sha256']
 for name,h in sources.items():require(h==inv[name]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/name),'Changed OpenableDoor source '+name)
 height=float(re.search(r'door_offset.y \+ ([0-9]+)',text)[1]);shake=[float(v.strip()) for v in re.search(r'shake_camera\(([^,]+), ([^,]+), Vector2\(([^)]+)\)\)',text).groups()[:2]];shake.extend(float(v.strip()) for v in re.search(r'shake_camera\([^\n]+Vector2\(([^)]+)\)',text)[1].split(','));run_y=float(re.search(r'get_direction\(\).y == (-?[0-9]+)',text)[1])
 write(IR,dict(schema=1,commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],sources=sources,scene_admitted=False,native_sha256=sha(native),none=defaults['sound'] if defaults['sound']=='None' else 'None',initial_unlocked=default('_unlocked'),child_height=height,shake=shake,run_y=run_y,bash=bash,records=records,clips=clips,textures=textures,sounds=[dict(id=stable(s),source=s,pcm='sound/effects/openable-door/'+str(stable(s))+'.pcm') for s in sounds],dialogs=dialogs,pending=['Actual inventory Item identity, source party/key inventories, geometry/Prompt/audio/dialogue hosts must bind before Ready','Source exported sprite setters execute before Ready; quarantined native null child textures are not authoritative']))
def assets(ir,tex3ds=None):
 if tex3ds:
  from PIL import Image
  def convert(t):
   target=ROOT/'romfs'/t['path'];target.parent.mkdir(parents=True,exist_ok=True);temporary=ROOT/'build/openable-door-assets'/str(t['id']);temporary.mkdir(parents=True,exist_ok=True);crop=temporary/'source.png';x,y,w,h=map(int,t['region'])
   with Image.open(ROOT/'upstream/MOTHER-Encore'/t['source']) as image:image.convert('RGBA').crop((x,y,x+w,y+h)).save(crop)
   subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(crop)],check=True)
  with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(convert,ir['textures']))
 result=[dict(id=t['id'],path=t['path'],bytes=(ROOT/'romfs'/t['path']).stat().st_size,source_sha256=ir['sources'][t['source']],output_sha256=sha(ROOT/'romfs'/t['path']),ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),format='rgba8',compression='none',tex3ds_sha256=sha(tex3ds) if tex3ds else None) for t in ir['textures']]
 if not tex3ds:
  previous=read(ASSETS);require(len(previous)==len(result),'OpenableDoor source texture receipt count differs')
  for r,p in zip(result,previous):
   r['tex3ds_sha256']=p.get('tex3ds_sha256');require(re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256'] or '') and r==p,'OpenableDoor genuine source texture receipt differs')
 return result
def pack(d,receipts):
 out=bytearray(128)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 s(d['scene']);s(SCRIPT);s(d['none']);s(d['bash']);u(int(d['initial_unlocked']));f(d['child_height'],*d['shake'],d['run_y']);u(len(d['clips']))
 for profile in d['clips']:
  u(len(profile))
  for c in profile:
   u(c['role']);s(c['name']);f(c['length']);u(int(c['loop']),len(c['tracks']))
   for t in c['tracks']:u(t['property'],t['update'],t['value'])
 u(len(d['textures']))
 for t,r in zip(d['textures'],receipts):u(t['id']);s(t['source']);s(t['path']);f(*t['region']);out.extend(bytes.fromhex(r['source_sha256']+r['output_sha256']))
 u(len(d['sounds']))
 for a in d['sounds']:u(a['id']);s(a['source']);s(a['pcm']);out.extend(bytes.fromhex(d['sources'][a['source']]))
 u(len(d['dialogs']))
 for a in d['dialogs']:
  for k in ['program','source','key','en','zh_CN']:s(a[k])
  out.extend(bytes.fromhex(d['sources'][a['source']]))
 for n in d['records']:
  u(n['id'],n['ready']);s(n['node']);u(*n['children'],n['texture'],n['profile'],n['flags']);f(*n['offset'],*n['initial_sprite_position'],*n['sprite_offset'],*n['sprite_scale'],*[v for p in n['positions'] for v in p],n['timer_wait'],n['gain_db']);s(n['bus'])
  for k in ['key','flag','activate','deactivate','sound','end_sound']:s(n[k])
  for a in n['dialogs']:s(a)
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',out,0,b'ENCFOPN1',1,128,len(out),0,0x454e0020,3,len(d['records']),d['scene_id']);out[40:60]=bytes.fromhex(PIN);out[60:92]=bytes.fromhex(d['source_sha256']);out[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',out,20,zlib.crc32(out[128:]));return bytes(out)
def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted'] and r['ir_sha256']==sha(IR) and r['commit']==PIN,'OpenableDoor semantic review missing/stale')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'OpenableDoor source differs '+p)
 return d
def audio_bindings(root=ROOT):
 d=load();return[dict(source=n['source'],identity=dict(kind='stable',value=n['id']),pcm=n['pcm'],gain_db=0,conversion=None) for n in d['sounds']]
def stage_files(source):
 d=load();r=assets(d);raw=pack(d,r);source=Path(source);require((source/'data/podunk.encopenable').read_bytes()==raw,'Stale OpenableDoor binary');out={Path('data/podunk.encopenable'):raw}
 for t,a in zip(d['textures'],r):p=source/t['path'];require(p.stat().st_size==a['bytes'] and sha(p)==a['output_sha256'],'OpenableDoor staged texture differs');out[Path(t['path'])]=p.read_bytes()
 return out
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Official complete native export required');extract(a.native);return
 d=load();r=assets(d,a.tex3ds);raw=pack(d,r)
 if a.action=='verify':require(read(ASSETS)==r and OUT.read_bytes()==raw,'OpenableDoor assets/data differ')
 else:write(ASSETS,r);OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('OpenableDoor complete source:',len(d['records']),'Ready;',len(d['textures']),'native atlas crops;',len(raw),'bytes; scene admitted=False')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,TypeError,OSError,subprocess.SubprocessError) as e:sys.exit('FIELD OPENABLE DOOR ERROR: '+str(e))
