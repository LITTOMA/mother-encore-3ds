#!/usr/bin/env python3
"""Pinned complete Podunk native audio properties, without source Ready."""
from pathlib import Path
import argparse,hashlib,json,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
IR=ROOT/'content/podunk-scene-audio.json'
REVIEW=ROOT/'reports/field-scene-audio/source-review.json'
PACK=ROOT/'romfs/data/podunk.encnativeaudio'
FAMILY=0x454e0067

def extract(native,engine,upstream):
 d=read(native);tree=read(ROOT/'content/podunk-node-tree.json');context=read(ROOT/'content/podunk-scene.json')
 require(d['source']=='res://'+SCENE and d['godot']['string']=='3.6.2-stable (official)' and len(d['nodes'])==8686,'Native scene scope')
 require(sha(native)==tree['native_sha256']==context['export_sha256'],'Native snapshot identity')
 engine=Path(engine);proof={}
 for filename in ['audio_stream_player.cpp','audio_stream_player_2d.cpp','project_settings.cpp']:
  text=(engine/filename).read_text(encoding='utf8');proof[filename]=dict(sha256=sha(engine/filename),source='https://github.com/godotengine/godot/blob/3.6.2-stable/'+('core/project_settings.cpp'if filename=='project_settings.cpp'else 'scene/2d/'+filename if filename.endswith('_2d.cpp')else 'scene/audio/'+filename))
  if filename=='project_settings.cpp':require('GLOBAL_DEF_RST("audio/2d_panning_strength", 1.0f)'in text,'Native global panning default')
  elif filename.endswith('_2d.cpp'):require('NOTIFICATION_INTERNAL_PHYSICS_PROCESS'in text and 'Math::pow(1.0f - dist / max_distance, attenuation)'in text and 'stream_playback->is_playing()'in text,'Native positional mixing changed')
  else:require('NOTIFICATION_INTERNAL_PROCESS'in text and 'set_process_internal(false)'in text and 'stream_playback->is_playing()'in text,'Native mixing changed')
 project=(upstream/'project.godot').read_text(encoding='utf8');require('2d_panning_strength'not in project,'Project overrides global panning')
 banks=[read(ROOT/'content/native-audio.json'),read(ROOT/'content/podunk-bundle-audio.json')];streams=[];sources={SCENE:sha(upstream/SCENE),'project.godot':sha(upstream/'project.godot')}
 for r in d['resources']:
  if not r['class'].startswith('AudioStream'):continue
  require(r['class']in ('AudioStreamMP3','AudioStreamSample')and set(r)=={'id','class','path','payload'} and r['payload']=='external codec; original bytes tracked in source.json','Unsupported native source stream properties')
  name=r['path'];matches=[(i+1,a)for i,b in enumerate(banks)for a in b['assets']if a['source_path']==name]
  # Prefer the explicitly source-bound scene bank; do not resolve by source
  # path through a runtime list containing duplicate bash metadata.
  chosen=[x for x in matches if x[0]==2]or matches
  require(len(chosen)==1,'Native source stream lacks unambiguous explicit asset '+name)
  bank,a=chosen[0];require(a['source_sha256']==sha(upstream/name[6:]),'Changed native audio source')
  sources[name[6:]]=a['source_sha256'];sources[name[6:]+'.import']=sha(upstream/(name[6:]+'.import'))
  streams.append(dict(id=r['id'],native_class=r['class'],source=name[6:],source_sha256=a['source_sha256'],asset_id=a['stable_id'],bank=bank))
 nodes=[];known={x['id']for x in streams};tree_by={n['id']:n for n in tree['records']}
 common={'_import_path','pause_mode','physics_interpolation_mode','unique_name_in_owner','process_priority','stream','volume_db','pitch_scale','autoplay','stream_paused','bus','script'}
 canvas={'visible','modulate','self_modulate','show_behind_parent','light_mask','material','use_parent_material','position','rotation','scale','z_index','z_as_relative'}
 for n in d['nodes']:
  if n['class']not in ('AudioStreamPlayer','AudioStreamPlayer2D'):continue
  p=decode(n['properties']);kind=1 if n['class']=='AudioStreamPlayer'else 2
  require(set(p)==common|({'mix_target'}if kind==1 else canvas|{'max_distance','attenuation','panning_strength','area_mask'}),'Unknown native audio property '+n['path'])
  require(p['script']is None and not p['unique_name_in_owner']and p['pause_mode']==0 and p['physics_interpolation_mode']==0 and p['process_priority']==0,'Unsupported native audio constructor')
  stream=0 if p['stream']is None else p['stream']['id'];require(not stream or stream in known,'Unknown native stream')
  sid=stable(n['path']);require(sid in tree_by,'Audio source missing actual tree')
  row=dict(id=sid,path=n['path'],kind=kind,stream=stream,bus=p['bus'],volume_db=p['volume_db'],pitch=p['pitch_scale'],autoplay=p['autoplay'],paused=p['stream_paused'],mix_target=p.get('mix_target',0),max_distance=p.get('max_distance',0),attenuation=p.get('attenuation',0),panning=p.get('panning_strength',0),area_mask=p.get('area_mask',0))
  require(not row['autoplay']and row['mix_target']==0 and row['pitch']>0,'Actual Podunk audio capability')
  if kind==2:require(p['material']is None and not p['use_parent_material'],'Unsupported positional Audio material')
  nodes.append(row)
 require(len(nodes)==129 and sum(x['kind']==2 for x in nodes)==4 and len(streams)==7,'Complete native audio closure')
 result=dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=SCENE,scene_id=context['scene_id'],source_sha256=context['source_sha256'],native_sha256=sha(native),tree_ir_sha256=sha(ROOT/'content/podunk-node-tree.json'),global_panning=1.0,scene_bank='sound/banks/podunk-scene-effects.encaudio',bank_sha256=sha(ROOT/'romfs/sound/banks/podunk-scene-effects.encaudio'),nodes=nodes,streams=streams,sources=sources,engine=proof,admission_ready=False)
 write(IR,result);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=sources,engine=proof,native_sha256=sha(native),admission_ready=False))
 return result

def load():
 d=read(IR);require(d['commit']==PIN and d['family']==FAMILY and read(REVIEW)['ir_sha256']==sha(IR),'Native audio review')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'Native audio source changed '+p)
 require(sha(ROOT/'content/podunk-node-tree.json')==d['tree_ir_sha256']and sha(ROOT/'romfs'/d['scene_bank'])==d['bank_sha256'],'Native audio binding changed')
 return d
def encode(d,ir=IR):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene_bank']);b.extend(bytes.fromhex(d['bank_sha256']));f(d['global_panning']);u(len(d['nodes']),len(d['streams']))
 for x in d['streams']:u(x['id'],x['asset_id'],x['bank']);t(x['source']);t(x['native_class']);b.extend(bytes.fromhex(x['source_sha256']))
 for x in d['nodes']:
  u(x['id'],x['kind'],x['stream'],x['mix_target'],x['area_mask'],x['autoplay'],x['paused']);t(x['path']);t(x['bus']);f(x['volume_db'],x['pitch'],x['max_distance'],x['attenuation'],x['panning'])
 struct.pack_into('<8s8I',b,0,b'ENCSAUD1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(ir));return bytes(b)
def stage_files(root):
 d=load();b=encode(d);require((Path(root)/'data/podunk.encnativeaudio').read_bytes()==b,'Native audio pack changed');return {Path('data/podunk.encnativeaudio'):b}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 try:
  d=extract(a.native,a.engine,ROOT/'upstream/MOTHER-Encore')if a.action=='extract'else load();PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(d));print('Podunk native audio:',len(d['nodes']),'nodes;',len(d['streams']),'streams; lifecycle not admitted')
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('SCENE AUDIO ERROR: '+str(e))
