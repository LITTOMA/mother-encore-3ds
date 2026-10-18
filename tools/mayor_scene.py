#!/usr/bin/env python3
"""Admit the reviewed initial Mayor scene into the native resource build.

Assets require the real converters; normal builds rederive metadata and verify
source/pixel/PCM receipts. Unknown later dialogue remains an explicit boundary.
"""
from __future__ import annotations
import argparse, copy, hashlib, json, shlex, shutil, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f, field_scene_npcs as npc, mayor_story as story
from tools import field_scene_music as music, field_footsteps as steps, field_area as area
from tools import audio_asset as audio, native_content as nc
RECIPE=ROOT/'content/mayor-field.json'
RECEIPT=ROOT/'content/asset-receipts/graphics/world/mayor/source.json'
AUDIO_RECIPE=ROOT/'content/mayor-audio.json'
AUDIO_RECEIPT=ROOT/'content/asset-receipts/audio/mayor.json'
BUILD_RECEIPT=ROOT/'content/asset-receipts/graphics/world/mayor/scene.json'

def sha(data):return hashlib.sha256(data).hexdigest()
def dump(path,value):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes((json.dumps(value,indent=2,sort_keys=True)+'\n').encode())
def checked(root,name,row):
 path=Path(name);f.require(name and ':'not in name and '\\'not in name and not path.is_absolute()and all(p not in ('','.','..')for p in name.split('/')),'Unsafe Mayor output path')
 target=(root/path).resolve();f.require(target.is_relative_to(root.resolve())and not (root/path).is_symlink(),'Mayor output escaped owner')
 data=target.read_bytes();f.require(len(data)==row['bytes']and sha(data)==row['sha256'],'Mayor asset fingerprint changed: '+name);return data

def audio_recipe(metadata):
 base=json.loads((ROOT/'content/native-audio.json').read_bytes());base['assets']=copy.deepcopy(metadata['tracks']);return base

def assets(tex3ds,command=None,ffmpeg='ffmpeg',ffprobe='ffprobe'):
 from tools import field_scene_candidate as candidate
 tool=Path(tex3ds).resolve();f.require(tool.is_file(),'Real tex3ds required')
 (ROOT/'build').mkdir(exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='mayor-assets-',dir=ROOT/'build')as owner:
  owner=Path(owner);mapdir=owner/'map';npcdir=owner/'npc';musicdir=owner/'music'
  proof=candidate.compile_map(RECIPE,mapdir,tool,command)
  args=[sys.executable,str(ROOT/'tools/field_scene_npcs.py'),'--out',str(npcdir),'--interaction-policy','--tex3ds',str(tool)]
  if command:args+=['--tex3ds-command',command]
  subprocess.run(args,cwd=ROOT,check=True)
  subprocess.run([sys.executable,str(ROOT/'tools/field_scene_music.py'),'--out',str(musicdir),'--audio','--ffmpeg',ffmpeg,'--ffprobe',ffprobe],cwd=ROOT,check=True)
  np=json.loads((npcdir/'candidate.json').read_bytes());mp=json.loads((musicdir/'source.json').read_bytes())
  files={}
  for row in proof['pages']:files[row['path']]=checked(mapdir/'romfs',row['path'],row)
  for name,row in np['textures'].items():files[name]=checked(npcdir,name,row)
  for name,row in mp['outputs'].items():
   if name.endswith(('.encaudio','.pcm')):files[name]=checked(musicdir,name,row)
  for name,data in files.items():dest=ROOT/'romfs'/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
  _,metadata,_,_=music.source();dump(AUDIO_RECIPE,audio_recipe(metadata));dump(AUDIO_RECEIPT,mp['audio'])
  dump(RECEIPT,dict(schema=1,commit=f.PIN,tex3ds_sha256=sha(tool.read_bytes()),map_sources=proof['sources'],npc_sources=np['sources'],pages=proof['pages'],textures=np['textures'],asset_permission='Upstream LICENSE game-related fork/modification; source notices retained; no third-party tool binary distributed'))
 return compile_scene()

def payloads(source=ROOT/'romfs'):
 source=Path(source);recipe=f.load_recipe(RECIPE);model=f.Model(recipe);atlases=f.Atlases(model)
 receipt=json.loads(RECEIPT.read_bytes());f.require(set(receipt)==set('schema commit tex3ds_sha256 map_sources npc_sources pages textures asset_permission'.split())and type(receipt['schema'])is int and receipt['schema']==1 and receipt['commit']==f.PIN,'Mayor texture receipt schema/pin')
 f.require(receipt['map_sources']==model.sources.digests(),'Mayor map source set changed')
 paths=[recipe['atlas_directory']+'/atlas-%02d.t3x'%i for i in range(len(atlases.pages))]
 f.require([p['path']for p in receipt['pages']]==paths,'Mayor atlas ownership changed');files={}
 for i,p in enumerate(receipt['pages']):
  f.require(p['rgba_sha256']==atlases.digest(i)and(p['width'],p['height'])==atlases.pages[i].size,'Mayor atlas source pixels changed');files[p['path']]=checked(source,p['path'],p)
 np,records,sources=npc.build();f.require(sources==receipt['npc_sources'],'Mayor NPC source set changed')
 textures={}
 for row in records:
  b=row['binding'];image=row['ir']['texture'];name=b['texture']
  f.require(name in receipt['textures'],'Mayor NPC texture missing');t=receipt['textures'][name]
  f.require(t['source']==image['source']and t['source_sha256']==sha((ROOT/'upstream/MOTHER-Encore'/image['source']).read_bytes()),'Mayor NPC texture source changed')
  textures[name]=checked(source,name,t);files[b['motion']]=npc.interaction_blob(row)
 f.require(set(textures)==set(receipt['textures']),'Unexpected Mayor NPC texture');files.update(textures)
 ar,area_sources=area.source();files[recipe['outputs']['map']]=f.encode_map(model,atlases,paths,area=ar)
 room=f.build_room(recipe,model,recipe['house']['room_ir'],npc_adder=lambda *args:npc.add_room(*args,asset_root=source),story_adder=story.add_room,recipe_path=RECIPE)
 parsed=nc.parse_pack(room);story_blob,story_proof=story.build_story(parsed);files[recipe['outputs']['room']]=room
 from tools.world_scene_routes import load as routes
 link=next(s for s in routes()['scenes']if s['id']==recipe['scene']['id']);from tools.resource_catalog import load_catalog
 bindings={b['id']:b for b in load_catalog()['bindings']}
 def output(identity,role):
  f.require(identity in bindings and bindings[identity]['role']==role,'Mayor catalog identity/role mismatch');return bindings[identity]['path']
 f.require(output(link['room'],'FieldRoom')==recipe['outputs']['room']and output(link['map'],'FieldMap')==recipe['outputs']['map'],'Mayor catalog scene paths differ')
 a=link['assets'];files[output(a['story'],'FieldStory')]=story_blob
 f.require([output(i,'FieldNpc')for i in a['npcs']]==[r['binding']['motion']for r in records],'Mayor NPC catalog order differs')
 sp,sm,step_proof=steps.source();f.require(output(a['steps'],'FieldSteps')==sp['output'],'Mayor step path differs');files[sp['output']]=steps.encode(sm)
 mp,mm,polygons,music_sources=music.source();mb,zb=music.encode(mm,polygons)
 for key,role,blob in [('music','FieldMusic',mb),('zones','FieldMusicZones',zb)]:
  name=mp['binding'if key=='music'else'zones'];f.require(output(a[key],role)==name,'Mayor music catalog path differs');files[name]=blob
 f.require(output(a['audio'],'FieldAudio')==mp['audio'],'Mayor audio path differs')
 expected=audio_recipe(mm);f.require(json.loads(AUDIO_RECIPE.read_bytes())==expected,'Mayor audio recipe differs from source mapping')
 manifest=json.loads(AUDIO_RECEIPT.read_bytes());f.require(manifest['schema']==1 and manifest['upstream_commit']==f.PIN and manifest['recipe_sha256']==sha(json.dumps(expected,sort_keys=True,separators=(',',':')).encode()),'Mayor audio receipt recipe changed')
 wanted={mp['audio']}|{t['pcm_path']for t in mm['tracks']};f.require({r['path']for r in manifest['files']}==wanted and len(manifest['files'])==len(wanted),'Mayor audio file ownership changed')
 for row in manifest['files']:files[row['path']]=checked(source,row['path'],dict(bytes=row['size'],sha256=row['sha256']))
 bank=audio.parse_bank(files[mp['audio']]);f.require(files[mp['audio']]==audio.build_bank(manifest['assets'],bank['master_db'],bank['silence_db']),'Mayor bank differs from conversion records')
 f.require({x['stable_id']for x in bank['assets']}=={t['stable_id']for t in mm['tracks']},'Mayor audio/music coverage differs')
 for resource in parsed['sections']['Resource']:
  if resource['kind']!=1:continue
  name=parsed['strings'][resource['path_string']];data=files.get(name)
  if data is None:data=(source/name).read_bytes()
  f.require(sha(data)==resource['sha256'],'Mayor Room texture dependency changed');files[name]=data
 from tools.romfs_layout import check_layout
 check_layout(files)
 proof=dict(schema=1,commit=f.PIN,scene=recipe['scene']['id'],map_sources=model.sources.digests(),npc_sources=sources,area_sources=area_sources,story_sources=story_proof['sources'],music_sources=music_sources,steps=step_proof,producer_sha256=sha(Path(__file__).read_bytes()),outputs={name:dict(bytes=len(data),sha256=sha(data))for name,data in sorted(files.items())})
 return {Path(k):v for k,v in files.items()},proof

def compile_scene():
 files,proof=payloads()
 for name,data in files.items():
  dest=ROOT/'romfs'/name;dest.parent.mkdir(parents=True,exist_ok=True);temp=dest.with_suffix(dest.suffix+'.pending');temp.write_bytes(data);temp.replace(dest)
 dump(BUILD_RECEIPT,proof);print('Mayor initial scene:',len(files),'checked files; later rescue/zoo interactions remain explicit boundaries');return files

def stage_files(source=ROOT/'romfs'):
 files,proof=payloads(source);f.require(json.loads(BUILD_RECEIPT.read_bytes())==proof,'Mayor scene build receipt stale')
 for name,data in files.items():f.require((source/name).read_bytes()==data,'Mayor scene resource stale: '+str(name))
 return files

def main():
 ap=argparse.ArgumentParser();ap.add_argument('command',choices=('assets','compile','verify'));ap.add_argument('--tex3ds',type=Path);ap.add_argument('--tex3ds-command');ap.add_argument('--ffmpeg',default='ffmpeg');ap.add_argument('--ffprobe',default='ffprobe');a=ap.parse_args()
 if a.command=='assets':f.require(a.tex3ds is not None,'Real texture compiler required');assets(a.tex3ds,a.tex3ds_command,a.ffmpeg,a.ffprobe)
 elif a.command=='compile':compile_scene()
 else:print('Mayor scene verified:',len(stage_files()),'files')
if __name__=='__main__':main()
