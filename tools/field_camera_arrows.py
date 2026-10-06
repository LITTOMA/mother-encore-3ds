#!/usr/bin/env python3
"""All14 original GameCamera MapArrows source controls and native tracks."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib,subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
SCRIPT='Scripts/UI/MapScreen/MapArrows.gd';PROTOTYPE='Nodes/Ui/MapScreen/MapArrows.tscn';IMAGE='Graphics/UI/Inventory/cursor.png';TEXTURE='graphics/ui/map-arrows/cursor.t3x';IR=ROOT/'content/podunk-camera-arrows.json';REVIEW=ROOT/'compatibility/reviews/podunk-camera-arrows-v0410.json';ASSETS=ROOT/'content/podunk-camera-arrows-assets.json';OUT=ROOT/'romfs/data/podunk.encarrows'
def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');sources=dict(g['sources']);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};require(d['source']=='res://'+SCENE and len(nm)==8686 and sha(native)==g['export_sha256'],'MapArrows full source identity differs');text=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text();methods=re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',text,re.M);require(methods==['_ready','show','hide','_refresh_show_hide','handle_input_events','point_directions','unpoint_directions','point_dir_sum','set_offset','set_bounds','_refresh_visibility','set_arrow_visible','_on_anim_finished'],'MapArrows source methods changed');directions=re.findall(r'Vector2\.(UP|DOWN|LEFT|RIGHT): \$(arrow[A-Z])',text);require(directions==[('UP','arrowU'),('DOWN','arrowD'),('LEFT','arrowL'),('RIGHT','arrowR')],'MapArrows dictionary order changed')
 for f in ['_show_arrows = visible','_refresh_show_hide(true)','_refresh_show_hide(visible)','_refresh_show_hide(!visible)','play(anim_name, -1, 0, true)','get_just_pressed_directions()','get_just_released_directions()','anim_player.assigned_animation != "Point"','anim_player.assigned_animation != "UnPoint"','Vector2(sign(dir_sum.x), 0), Vector2(0, sign(dir_sum.y))','if arrow != other_arrow and other_arrow.visible and other_arrow.playing:','yield(other_arrow, "frame_changed")','arrow.frame = other_arrow.frame']:require(f in text,'MapArrows changed '+f)
 prototype=(ROOT/'upstream/MOTHER-Encore'/PROTOTYPE).read_text();atlas={}
 for aid,body in re.findall(r'^\[sub_resource type="AtlasTexture" id=(\d+)\]\n(.*?)(?=^\[|\Z)',prototype,re.M|re.S):
  require(re.findall(r'^([a-z_]+) =',body,re.M)==['atlas','region']and 'atlas = ExtResource( 3 )'in body,'MapArrows original atlas unknown property');region=re.search(r'region = Rect2\( ([^)]*) \)',body);require(region,'MapArrows original region missing');atlas['res://'+PROTOTYPE+'::'+aid]=[float(x)for x in region[1].split(',')]
 require(len(atlas)==3 and '[ext_resource path="res://'+IMAGE+'" type="Texture" id=3]'in prototype,'MapArrows original atlas source binding differs')
 states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def instances(root,f):
  roots.append((root,f))
  for n in states[f]['nodes']:
   if n['instance']:
    local=n['path'].removeprefix('./');instances(local if root=='.'else root+'/'+local,rs[n['instance']['id']]['path'][6:])
 instances('.',SCENE);overrides={}
 for root,f in sorted(roots,key=lambda v:v[0].count('/')if v[0]!='.'else-1,reverse=True):
  for n in states[f]['nodes']:
   path=n['path'].removeprefix('./');path=root if path=='.'else path if root=='.'else root+'/'+path
   if path in nm:overrides.setdefault(path,{}).update(decode(n['properties']))
 children={p:[]for p in nm}
 for p in nm:
  if p!='.':children[p.rsplit('/',1)[0]if'/'in p else'.'].append(p)
 ready=[]
 def visit(p):
  for c in children[p]:visit(c)
  ready.append(p)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};profiles=[];sprites=[];records=[];frames=None;rootclips=['Come In','Come Out','RESET'];arrowclips=['Point','UnPoint','RESET'];property_role={'frame':1,'offset':2,'playing':3,'position':4,'visible':5};property_kind={'frame':'int','offset':'vector','playing':'bool','position':'vector','visible':'bool'}
 def player(path,target_root,kind,targets):
  a=decode(nm[path]['properties']);require(a['playback_process_mode']==1 and a['playback_speed']>0 and a['autoplay']==''and a['blend_times']==[]and a['playback_default_blend_time']==0,'MapArrows unknown AnimationPlayer state');out=[]
  for role,name in enumerate(rootclips if kind==0 else arrowclips,1):
   v=decode(rs[a['anims/'+name]['id']]['properties']);tracks=[];i=0
   while'tracks/'+str(i)+'/type'in v:
    pre='tracks/'+str(i)+'/';key=dict(v[pre+'keys']['pairs']);ref=v[pre+'path']['value'];node,prop=ref.split(':');require(prop in property_role and node in targets and v[pre+'type']=='value'and v[pre+'enabled']and v[pre+'interp']==1 and v[pre+'loop_wrap']and key['update']in[0,1],'MapArrows unsupported native track '+ref);values=key['values'];typed=[]
    for value in values:
     if property_kind[prop]=='vector':require(type(value)is list and len(value)==2,'MapArrows expected Vector2');typed.append(value)
     elif property_kind[prop]=='bool':require(type(value)is bool,'MapArrows expected bool');typed.append([float(value),0])
     else:require(type(value)is int,'MapArrows expected int');typed.append([float(value),0])
    require(len(typed)==len(key['times'])==len(key['transitions']),'MapArrows native keys differ');tracks.append(dict(target=targets[node],property=property_role[prop],update=key['update'],keys=[dict(time=t,transition=c,value=value)for t,c,value in zip(key['times'],key['transitions'],typed)]));i+=1
   out.append(dict(role=role,name=name,length=v['length'],loop=v['loop'],tracks=tracks))
  if out not in profiles:profiles.append(out)
  return dict(id=stable(path),ready=ordinal[path],target_root=target_root,kind=kind,profile=profiles.index(out),speed=a['playback_speed'],pause=a['pause_mode'],priority=a['process_priority'])
 players=[]
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];v=decode(nm[p]['properties']);require(v['rotation']==0 and v['scale']==[1,1]and v['material']is None and not v['use_parent_material'],'MapArrows root material/transform unsupported');arrow_ids=[]
  for index,(_,part)in enumerate(directions):
   path=p+'/'+part;s=decode(nm[path]['properties']);require(s['animation']=='Idle'and not s['flip_h']and not s['flip_v']and s['scale']==[1,1]and s['material']is None and not s['use_parent_material'],'MapArrows source sprite unsupported');a=dict(decode(rs[s['frames']['id']]['properties'])['animations'][0]['pairs']);require(a['name']=='Idle'and len(a['frames'])==4 and a['loop'],'MapArrows SpriteFrames differs');f=[]
   for ref in a['frames']:
    q=rs[ref['id']];require(q['class']=='AtlasTexture'and q['path']in atlas and decode(q['size'])==atlas[q['path']][2:],'MapArrows native/original atlas binding differs');f.append(atlas[q['path']])
   if frames is None:frames=f
   require(frames==f,'MapArrows source frame roster differs');color=lambda q:[q[k]for k in ['r','g','b','a']];sid=stable(path);arrow_ids.append(sid);sprites.append(dict(id=sid,root_id=b['stable_id'],ready=ordinal[path],direction=index,frame=overrides[path].get('frame',0),flags=sum(int(x)<<i for i,x in enumerate([s['visible'],s['playing'],s['centered']])),position=s['position'],offset=s['offset'],scale=s['scale'],rotation=s['rotation'],speed_scale=s['speed_scale'],animation_speed=a['speed'],pause=s['pause_mode'],priority=s['process_priority'],modulate=color(s['modulate']),self_modulate=color(s['self_modulate'])));players.append(player(path+'/AnimationPlayer',sid,1,{'.':0}))
  players.append(player(p+'/AnimationPlayer',b['stable_id'],0,{'.':0,**{part:i+1 for i,(_,part)in enumerate(directions)}}));color=lambda q:[q[k]for k in ['r','g','b','a']];records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],parent_id=stable(p.rsplit('/',1)[0]),node=p,arrows=arrow_ids,position=v['position'],visible=v['visible'],z=v['z_index'],z_relative=v['z_as_relative'],pause=v['pause_mode'],priority=v['process_priority'],modulate=color(v['modulate']),self_modulate=color(v['self_modulate'])))
 require(len(records)==14 and len(sprites)==56 and len(players)==70 and len(profiles)==2,'Incomplete MapArrows full source scope');image=(ROOT/'upstream/MOTHER-Encore'/IMAGE).read_bytes();width,height=struct.unpack('>II',image[16:24]);require('flags/filter=false'in(ROOT/'upstream/MOTHER-Encore'/(IMAGE+'.import')).read_text(),'MapArrows source filtering changed');require('2d/snapping/use_gpu_pixel_snap=true'in(ROOT/'upstream/MOTHER-Encore/project.godot').read_text(),'MapArrows pixel snap source changed')
 for p in [SCRIPT,PROTOTYPE,IMAGE,IMAGE+'.import','LICENSE','project.godot']:sources[p]=inv[p]['sha256']
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'MapArrows changed source '+p)
 write(IR,dict(schema=1,kind='encore.field-camera-arrows.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,native_sha256=sha(native),sources=sources,program=dict(source='platform/ctr/shaders/field_camera_arrows.v.pica',path='shaders/field-camera-arrows.shbin',source_sha256=sha(ROOT/'platform/ctr/shaders/field_camera_arrows.v.pica')),records=records,sprites=sprites,players=players,profiles=profiles,frames=frames,asset=dict(source=IMAGE,path=TEXTURE,width=width,height=height),pixel_snap=True,directions=[[0,-1],[0,1],[-1,0],[1,0]],pending=['Actual liveGameCamera, source parent/material/Canvas and OS update_pending/process gates','Real synchronous AnimationPlayer/AnimatedSprite signals and ordered one-shot frame_changed resync','No generic input aliases: controlsManager pressed/released direction arrays must preserve source dictionary directions','Full GameCamera14 and other scene classes independently pending']))
def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted']and r['commit']==PIN and r['ir_sha256']==sha(IR),'MapArrows source review differs')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed MapArrows '+p)
 return d
def assets(d,tex3ds,picasso=None):
 a=d['asset'];target=ROOT/'romfs'/a['path']
 if tex3ds:
  require(picasso,'MapArrows real PICA compiler required with tex3ds');program=d['program'];program_target=ROOT/'romfs'/program['path'];program_target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(picasso),'-o',str(program_target),str(ROOT/program['source'])],check=True)
  target.parent.mkdir(parents=True,exist_ok=True)
  with ThreadPoolExecutor(max_workers=4)as pool:list(pool.map(lambda _:subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True),[0]))
 r=dict(source_sha256=d['sources'][a['source']],output_sha256=sha(target),bytes=target.stat().st_size,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),format='rgba8',compression='none',tex3ds_sha256=sha(tex3ds)if tex3ds else read(ASSETS)['tex3ds_sha256'])
 program_target=ROOT/'romfs'/d['program']['path'];r['program']=dict(source_sha256=sha(ROOT/d['program']['source']),output_sha256=sha(program_target),bytes=program_target.stat().st_size,crc=zlib.crc32(program_target.read_bytes()),picasso_sha256=sha(picasso)if picasso else read(ASSETS)['program']['picasso_sha256']);require(r['program']['source_sha256']==d['program']['source_sha256'],'Changed MapArrows shader execution source')
 if not tex3ds:require(r==read(ASSETS)and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']),'MapArrows genuine receipt differs')
 return r
def encode(d,asset):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(SCRIPT);u(int(d['pixel_snap']));[f(*v)for v in d['directions']];a=d['asset'];t(a['source']);t(a['path']);u(a['width'],a['height'],asset['bytes'],zlib.crc32((ROOT/'romfs'/a['path']).read_bytes()));b.extend(bytes.fromhex(asset['source_sha256']+asset['output_sha256']));program=d['program'];proof=asset['program'];t(program['source']);t(program['path']);u(proof['bytes'],proof['crc']);b.extend(bytes.fromhex(proof['source_sha256']+proof['output_sha256']));u(len(d['frames']));[f(*v)for v in d['frames']];u(len(d['profiles']))
 for p in d['profiles']:
  u(len(p))
  for c in p:
   u(c['role']);t(c['name']);f(c['length']);u(int(c['loop']),len(c['tracks']))
   for tr in c['tracks']:
    u(tr['target'],tr['property'],tr['update'],len(tr['keys']))
    for k in tr['keys']:f(k['time'],k['transition'],*k['value'])
 u(len(d['sprites']),len(d['players']))
 for r in d['records']:u(r['id'],r['ready'],r['parent_id'],*r['arrows'],int(r['visible']),int(r['z_relative']),r['pause']);i(r['z'],r['priority']);f(*r['position'],*r['modulate'],*r['self_modulate']);t(r['node'])
 for s in d['sprites']:u(s['id'],s['root_id'],s['ready'],s['direction'],s['frame'],s['flags'],s['pause']);i(s['priority']);f(*s['position'],*s['offset'],*s['scale'],s['rotation'],s['speed_scale'],s['animation_speed'],*s['modulate'],*s['self_modulate'])
 for a in d['players']:u(a['id'],a['ready'],a['target_root'],a['kind'],a['profile'],a['pause']);i(a['priority']);f(a['speed'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFARR1',1,128,len(b),0,0x454e0030,3,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def stage_files(source):
 d=load();a=assets(d,None);raw=encode(d,a);require((Path(source)/'data/podunk.encarrows').read_bytes()==raw,'StagedMapArrows differs');target=Path(source)/d['asset']['path'];require(sha(target)==a['output_sha256'],'StagedMapArrows atlas differs');program=Path(source)/d['program']['path'];require(sha(program)==a['program']['output_sha256'],'StagedMapArrows shader differs');return {Path('data/podunk.encarrows'):raw,Path(d['asset']['path']):target.read_bytes(),Path(d['program']['path']):program.read_bytes()}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--tex3ds',type=Path);p.add_argument('--picasso',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Explicit full source required');extract(a.native);return
 d=load();receipt=assets(d,a.tex3ds,a.picasso);raw=encode(d,receipt)
 if a.action=='verify':require(OUT.read_bytes()==raw and read(ASSETS)==receipt,'MapArrows output differs')
 else:write(ASSETS,receipt);OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('MapArrows complete source:',len(d['records']),'roots;',len(d['sprites']),'native sprites;',len(d['players']),'AnimationPlayers;',len(raw),'bytes; scene_admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.SubprocessError)as e:sys.exit('FIELD CAMERA ARROWS ERROR: '+str(e))
