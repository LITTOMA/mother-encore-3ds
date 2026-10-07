#!/usr/bin/env python3
"""Complete pinned Podunk Door source -> checked binary authoring resource.

Exports are extraction inputs only. Destination Ready/SceneTransition backends
must be admitted by the live scene owner before a transition can start.
"""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
SCRIPT='Scripts/Main/Door.gd';TRANS='Scripts/global/SceneTransition.gd';GLOBAL='Scripts/global/global.gd';FADE='Nodes/Ui/effects/Fade.gd'
IR=ROOT/'content/podunk-door.json';REVIEW=ROOT/'compatibility/reviews/podunk-door-v0410.json';OUT=ROOT/'romfs/data/podunk.encdoor'
FORMATS={1:'2I',2:'B',3:'17I31f',4:'I32s',5:'5If',6:'3I32s',7:'2I32s',8:'10I',9:'I'}
def flat(t):return [v for p in t for v in p]
def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(g['sources'])
 require(d['source']=='res://'+SCENE and g['export_sha256']==sha(native),'Door native identity differs')
 require([d['godot'].get(k) for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'Door source engine differs')
 nodes={n['path']:n for n in d['nodes']};res={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 require(len(nodes)==8686,'Incomplete native Door scene')
 def visit(root,f):
  roots.append((root,f))
  for n in states[f]['nodes']:
   if n['instance']:
    p=n['path'].removeprefix('./');visit(p if root=='.' else root+'/'+p,res[n['instance']['id']]['path'][6:])
 visit('.',SCENE);overrides={}
 for root,f in sorted(roots,key=lambda x:x[0].count('/') if x[0]!='.' else -1,reverse=True):
  for n in states[f]['nodes']:
   p=n['path'].removeprefix('./');p=root if p=='.' else p if root=='.' else root+'/'+p
   if p in nodes:overrides.setdefault(p,{}).update(decode(n['properties']))
 source=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text(encoding='utf-8')
 def default(name):
  value=re.search(r'var '+re.escape(name)+r'\s*:?=\s*([^\n#]+)',source)[1].strip()
  if value.startswith('"'):return value.strip('"')
  if value in ['true','false']:return value=='true'
  if value=='Vector2.ZERO':return [0,0]
  if value=='Color.black':return [0,0,0,1]
  if value=='[]':return []
  return float(value)
 keys=['targetX','targetY','dir','sound','end_sound','transit_in_anim','transit_out_anim','transit_in_color','transit_out_color','fade_in_speed','fade_out_speed','fadeout_music_on_scene_change','fadeout_music_length','targetScene','_target_scene_params','set_respawn','set_crumbs','unpause_player','show_player_after_warp','flag_set','set_flag_state']
 defaults={k:default(k) for k in keys};records=[];audio=[];targets=[]
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];n=nodes[p];v=dict(defaults);v.update({k:x for k,x in overrides[p].items() if k in keys})
  require(v['_target_scene_params']==[],'Door nonempty init_params pending')
  np=decode(n['properties']);marker=nodes[p+'/Position2D'];player=nodes[p+'/AudioStreamPlayer'];ap=decode(player['properties']);shape=nodes[p+'/CollisionShape2D'];sp=decode(shape['properties']);sr=res[sp['shape']['id']]
  require(sr['class']=='RectangleShape2D' and not sp['disabled'] and not sp['one_way_collision'],'Door unknown/disabled/one-way geometry')
  target='Maps/'+v['targetScene']+'.tscn' if v['targetScene'] else ''
  require(not target or target in inv,'Door destination source missing '+target)
  sound=lambda name:'Audio/Sound effects/'+v[name] if v[name] and v[name]!='None' else v[name]
  initial=res[ap['stream']['id']]['path'][6:];flags=sum(int(x)<<i for i,x in enumerate([np['monitoring'],np['monitorable'],np['visible'],v['set_respawn'],v['set_crumbs'],v['unpause_player'],v['show_player_after_warp'],v['set_flag_state'],v['fadeout_music_on_scene_change']]))
  records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],node=p,marker=stable(marker['path']),audio=stable(player['path']),shape=stable(shape['path']),layer=np['collision_layer'],mask=np['collision_mask'],flags=flags,pause_mode=np['pause_mode'],target_name=v['targetScene'],target_path=target,sound=sound('sound'),end_sound=sound('end_sound'),in_anim=v['transit_in_anim'],out_anim=v['transit_out_anim'],flag=v['flag_set'],target=[v['targetX'],v['targetY']],direction=v['dir'],speeds=[v['fade_in_speed'],v['fade_out_speed']],music_fade=v['fadeout_music_length'],in_color=v['transit_in_color'],out_color=v['transit_out_color'],body_transform=decode(n['world_transform']),marker_transform=decode(marker['world_transform']),shape_offset=sp['position'],extents=decode(sr['properties']['extents'])))
  audio.append(dict(id=stable(player['path']),source=initial,bus=ap['bus']))
  if target:targets.append(dict(id=b['stable_id'],path=target))
  for name in [target,initial,sound('sound'),sound('end_sound')]:
   if name and name!='None':sources[name]=inv[name]['sha256']
 records.sort(key=lambda x:x['ready']);require(len(records)==14,'Complete Door coverage differs')
 for name in [SCRIPT,TRANS,GLOBAL,FADE,'Nodes/Overworld/Door.tscn','Nodes/Ui/effects/Fade.tscn']:sources[name]=inv[name]['sha256']
 for name,h in sources.items():require(h==inv[name]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/name),'Door changed source '+name)
 offset=float(re.search(r'targetY - ([0-9]+)',source)[1]);require('Vector2(0, '+str(int(offset))+')' in source,'Door ground offsets differ')
 body=re.search(r'\[connection signal="body_entered" from="\." to="\." method="([^"]+)"\]',(ROOT/'upstream/MOTHER-Encore/Nodes/Overworld/Door.tscn').read_text(encoding='utf-8'));require(body and 'func '+body[1]+'(' in source,'Door body method source missing')
 write(IR,dict(schema=3,commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],sources=sources,scene_admitted=False,capabilities=5,body_method=body[1],native_sha256=sha(native),ground_offset=offset,none=defaults['sound'],records=records,audio=audio,targets=targets,pending=['Destination SceneTree/Ready and native Fade must be provided by admitted live backends','Unused _special_guest AoOni method remains unsupported and rejects if requested','Nonempty destination init_params unsupported for this source scope']))
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=sources,semantics=['Complete original fourteen Door bindings and transition rules retained','Native body_entered connection method extracted from original Door.tscn and verified against Door.gd; actual same-node callback supplied by concrete scene owner'],scene_admitted=False))

def pack(ir):
 strings=[];lookup={};blob=bytearray();rows={k:[] for k in FORMATS}
 def string(v):
  if v not in lookup:
   lookup[v]=len(strings);s=v.encode();strings.append((len(blob),len(s)));blob.extend(s+b'\0')
  return lookup[v]
 scene=string(ir['scene'])
 for n in ir['records']:
  ints=[n[k] for k in ['id','ready']]+[string(n['node'])]+[n[k] for k in ['marker','audio','shape','layer','mask','flags','pause_mode']]+[string(n[k]) for k in ['target_name','target_path','sound','end_sound','in_anim','out_anim','flag']]
  values=n['target']+n['direction']+n['speeds']+[n['music_fade']]+n['in_color']+n['out_color']+flat(n['body_transform'])+flat(n['marker_transform'])+n['shape_offset']+n['extents']
  rows[3].append((*ints,*values))
 rows[4]=[(string(p),bytes.fromhex(h)) for p,h in ir['sources'].items()];rows[5]=[(*[string(p) for p in [SCRIPT,TRANS,GLOBAL,FADE,ir['none']]],ir['ground_offset'])]
 rows[6]=[(n['id'],string(n['source']),string(n['bus']),bytes.fromhex(ir['sources'][n['source']])) for n in ir['audio']];rows[7]=[(n['id'],string(n['path']),bytes.fromhex(ir['sources'][n['path']])) for n in ir['targets']];rows[9]=[(string(ir['body_method']),)];rows[1]=strings;rows[2]=[(v,) for v in blob]
 begin=128+24*len(FORMATS);out=bytearray(begin);dirs=[]
 for k,fmt in FORMATS.items():
  data=b''.join(struct.pack('<'+fmt,*r) for r in rows[k]);dirs.append((k,len(rows[k]),struct.calcsize('<'+fmt),len(out),len(data),0));out.extend(data)
 struct.pack_into('<8s8I',out,0,b'ENCFDOR1',3,128,len(out),len(FORMATS),zlib.crc32(out[begin:]),0x454e001f,5,ir['scene_id']);out[40:60]=bytes.fromhex(PIN);out[60:92]=bytes.fromhex(ir['source_sha256']);out[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',out,124,scene)
 for i,row in enumerate(dirs):struct.pack_into('<6I',out,128+24*i,*row)
 return bytes(out)
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Complete official native export required');extract(a.native);return
 ir=read(IR);rv=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(ir['schema']==3 and ir['commit']==PIN and ir['scene']==SCENE and ir['scene_admitted'] is False and ir['capabilities']==5 and rv['commit']==PIN and rv['ir_sha256']==sha(IR),'Door review identity rejected')
 for name,h in ir['sources'].items():require(h==inv[name]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/name),'Door source differs '+name)
 data=pack(ir)
 if a.action=='verify':require(OUT.read_bytes()==data,'Door binary differs')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(data)
 print('Door source Ready/transition:',len(ir['records']),'nodes;',len(ir['targets']),'source targets;',len(data),'bytes; scene admitted=False')
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,KeyError,TypeError) as e:sys.exit('FIELD DOOR ERROR: '+str(e))
