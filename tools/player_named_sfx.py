#!/usr/bin/env python3
"""Original audioManager constructor/named-SFX data; no cold Ready permission."""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools import field_ui_manager_recipes as recipes
import tools.audio_asset as audio
IR=ROOT/'content/native-player-named-sfx.json';REVIEW=ROOT/'reports/player-named-sfx/source-review.json';PACK=ROOT/'romfs/data/audio-manager.encnamedsfx';FAMILY=0x454e0073
SCENE='Nodes/Ui/audioManager.tscn';SCRIPT='Scripts/global/audioManager.gd';RECIPE=ROOT/'content/named-sfx-audio.json';RECEIPT=ROOT/'content/asset-receipts/audio/named-sfx.json'

def stable(p):return zlib.crc32(('namedSfx:'+p).encode())or 1

def extract(directory):
 recipe=recipes.extract_recipe(SCENE,directory/'native.json',directory/'tree.json',directory/'source.json');n=read(directory/'native.json');s=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text();match=re.search(r'var _sound_effects := \{(.*?)\n\}',s,re.S);require(match,'Original sound dictionary missing');effects=re.findall(r'"([^"\n]+)": load\("res://([^"\n]+)"\)',match[1]);require(len(effects)==13,'Original full sound constructor changed')
 defaults=decode(n['voice_defaults']);require(defaults['stream']is None and defaults['bus']=='Master'and defaults['volume_db']==0 and defaults['pitch_scale']==1 and defaults['autoplay']is False and defaults['stream_paused']is False and defaults['mix_target']==0 and defaults['script']is None,'Original AudioStreamPlayer.new defaults changed')
 source={**recipe['sources']};old=read(ROOT/'content/native-audio.json');entries=[];extra=[]
 for row in n['streams']:
  p=row['source'];require(row['native']in ['AudioStreamMP3','AudioStreamSample'],'Unknown source AudioStream class');sourcesha=sha(ROOT/'upstream/MOTHER-Encore'/p);importsha=sha(ROOT/'upstream/MOTHER-Encore'/(p+'.import'));source[p]=sourcesha;source[p+'.import']=importsha;blob=(directory/row['payload']).read_bytes();require(len(blob)==row['payload_bytes']and blob,'Original stream payload absent');existing=next((x for x in old['assets']if x['source_path']=='res://'+p),None)
  if existing:asset=existing['stable_id']
  else:
   asset=stable(p);extra.append(dict(stable_id=asset,source_path='res://'+p,source_sha256=sourcesha,import_sha256=importsha,pcm_path='sound/effects/named-'+str(asset)+'.pcm',gain_db=0))
  entries.append(dict(source=p,native=row['native'],source_sha256=sourcesha,import_sha256=importsha,payload_sha256=hashlib.sha256(blob).hexdigest(),payload=list(blob),metadata=row['metadata'],asset_id=asset,slot=row['resource']['id']))
 body={}
 for method in ['add_sfx','play_sfx','get_sfx','_ready','_add_at_zero','_get_audio_player_count','add_audio_player']:
  q=re.search(r'^func '+method+r'\(.*?(?=^func |\Z)',s,re.M|re.S);require(q,'Named SFX method absent');body[method]=q[0]
 require(body['add_sfx'].index('AudioStreamPlayer.new()')<body['add_sfx'].index('sfx_node.bus')<body['add_sfx'].index('sfx_node.name')<body['add_sfx'].index('$Sfx.add_child')<body['add_sfx'].index('sfx_node.stream'),'Named SFX actual source order changed')
 bus=re.search(r'sfx_node.bus = "([^"]+)"',body['add_sfx'])[1];paths=[x['node']for x in recipe['records']];require(paths==['.','AudioPlayers','Sfx','Tween'],'Original full audioManager native source changed')
 out=dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=SCENE,owner=SCRIPT,source_sha256=source[SCENE],sources=source,recipe=recipe,native_snapshot=n,voice_defaults=n['voice_defaults'],effects=[dict(name=k,source=p)for k,p in effects],streams=entries,constructor=dict(sound_effects='_sound_effects',music_changers='musicChangers',overworld_battle_music='overworldBattleMusic',tween='_tween',sfx_parent='Sfx',music_parent='AudioPlayers',tween_node='Tween'),bus=bus,music_bus=re.search(r'const BUSES = \["([^"]+)"',s)[1],methods=body,opening_recipe_sha256=sha(ROOT/'content/native-audio.json'),bank='sound/banks/named-sfx.encaudio',ready_admitted=False)
 write(IR,out);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=source,native_sha256=sha(directory/'native.json'),exporter_sha256=sha(ROOT/'tools/godot_exporter/named_sfx_native.gd'),recipes_helper_sha256=sha(ROOT/'tools/field_ui_manager_recipes.py'),ready_admitted=False))
 r=dict(schema=1,upstream_commit=PIN,bus_source=old['bus_source'],bus_sha256=old['bus_sha256'],manager_source=old['manager_source'],manager_sha256=old['manager_sha256'],assets=extra);write(RECIPE,r);return out

def load():
 d=read(IR);r=read(REVIEW);require(d['commit']==PIN and d['family']==FAMILY and d['ready_admitted']is False and r['ir_sha256']==sha(IR)and d['opening_recipe_sha256']==sha(ROOT/'content/native-audio.json'),'Named SFX source scope/review rejected')
 require(r['exporter_sha256']==sha(ROOT/'tools/godot_exporter/named_sfx_native.gd')and r['recipes_helper_sha256']==sha(ROOT/'tools/field_ui_manager_recipes.py'),'Named SFX exporter/helper changed')
 for p,h in r['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'Named SFX source changed '+p)
 return d

def compile_assets(output):
 import concurrent.futures,tempfile
 d=load();recipe=read(RECIPE);output=Path(output);records=[];receipts=[];files=[];versions=[]
 with tempfile.TemporaryDirectory(prefix='encore-named-assets-',dir=ROOT/'build')as temp:
  temp=Path(temp)
  def one(pair):
   index,entry=pair;job=temp/str(index);r={**recipe,'assets':[entry]};m=audio.compile_assets(r,ROOT/'upstream/MOTHER-Encore',job);return job,m
  with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:jobs=list(pool.map(one,enumerate(recipe['assets'])))
  for job,m in jobs:
   v=m['assets'][0];src=job/v['pcm_path'];target=output/v['pcm_path'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(src.read_bytes());v.pop('command',None);receipts.append(v);records.extend(audio.parse_bank((job/'sound/banks/opening.encaudio').read_bytes())['assets']);versions.append(dict(version=m['ffmpeg_version'],sha256=m['ffmpeg_sha256']));files.append(dict(path=v['pcm_path'],size=target.stat().st_size,sha256=sha(target)))
  metadata=audio.parse_bank((jobs[0][0]/'sound/banks/opening.encaudio').read_bytes());bank=audio.build_bank(records,metadata['master_db'],metadata['silence_db']);target=output/d['bank'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(bank);files.append(dict(path=d['bank'],size=len(bank),sha256=hashlib.sha256(bank).hexdigest()))
 require(all(x==versions[0]for x in versions),'Named SFX decoder identity diverged');manifest=dict(schema=1,upstream_commit=PIN,recipe_sha256=sha(RECIPE),ir_sha256=sha(IR),bank=d['bank'],assets=receipts,files=files,decoder=versions[0],source_only=True,hardware_verified=False);write(RECEIPT,manifest);return manifest

def restore_assets(output):
 """Restore missing PCM only; reviewed source and decoder records are immutable."""
 import concurrent.futures,os,tempfile
 from tools import ci_bootstrap
 from tools.restore_audio import safe_target
 d=load();recipe=read(RECIPE);receipt=read(RECEIPT);output=Path(output).absolute()
 lock,baseline=ci_bootstrap.load_pin(ROOT)
 require(recipe['schema']==receipt['schema']==1 and recipe['upstream_commit']==receipt['upstream_commit']==lock['commit']==PIN,'Named SFX restore source pin mismatch')
 require(receipt['recipe_sha256']==sha(RECIPE)and receipt['ir_sha256']==sha(IR)and receipt['bank']==d['bank'],'Named SFX restore reviewed recipe changed')
 upstream=ROOT/'upstream/MOTHER-Encore'
 audio.verified(upstream,recipe['bus_source'],recipe['bus_sha256']);audio.verified(upstream,recipe['manager_source'],recipe['manager_sha256'])
 files={x['path']:x for x in receipt['files']};entries={x['pcm_path']:x for x in recipe['assets']};assets={x['pcm_path']:x for x in receipt['assets']}
 require(len(files)==len(receipt['files'])and len(entries)==len(recipe['assets'])and len(assets)==len(receipt['assets']),'Named SFX restore duplicate path')
 bankbytes=(ROOT/'romfs'/d['bank']).read_bytes();bankfile=files.get(d['bank'])
 require(bankfile and len(bankbytes)==bankfile['size']and hashlib.sha256(bankbytes).hexdigest()==bankfile['sha256'],'Named SFX restore reviewed bank changed')
 bank=audio.parse_bank(bankbytes);records={x['pcm_path']:x for x in bank['assets']}
 require(len(records)==len(bank['assets'])and set(entries)==set(assets)==set(records)and set(files)==set(entries)|{d['bank']},'Named SFX restore full bank/recipe/receipt closure mismatch')
 decoder=receipt['decoder'];require(set(decoder)=={'version','sha256'}and type(decoder['version'])is str and decoder['version']and re.fullmatch('[0-9a-f]{64}',decoder['sha256']),'Named SFX restore decoder record rejected')
 plan=[]
 for path,entry in entries.items():
  expected=assets[path];record=records[path];fingerprint=files[path]
  for key,value in record.items():require(expected[key]==value,'Named SFX restore receipt/bank mismatch: '+key)
  for key in ['stable_id','source_path','source_sha256','import_sha256','pcm_path','gain_db']:require(entry[key]==expected[key],'Named SFX restore recipe/receipt mismatch: '+key)
  require(fingerprint['size']==expected['pcm_bytes']and fingerprint['sha256']==expected['pcm_sha256'],'Named SFX restore PCM fingerprint disagreement')
  source=audio.source_path(entry['source_path']);audio.verified(upstream,source,entry['source_sha256']);imported=audio.verified(upstream,source+'.import',entry['import_sha256']);loop,offset=audio.import_settings(imported,entry['source_path'])
  rate=entry.get('output_sample_rate',expected['source_sample_rate'])
  require(rate==expected['sample_rate']and loop==expected['loop']and offset==expected['loop_offset_seconds']and int(offset*rate)==expected['loop_start'],'Named SFX restore source importer/output format changed')
  target=safe_target(output,path)
  if target.exists():require(target.is_file()and target.stat().st_size==fingerprint['size']and sha(target)==fingerprint['sha256'],'Existing named PCM differs; refusing overwrite: '+path)
  plan.append((entry,expected,fingerprint,target,not target.exists()))
 missing=[x for x in plan if x[-1]]
 if not missing:return dict(status='verified',assets=len(plan),restored=0,converted_bytes=0,workers=4)
 ci_bootstrap.verify(upstream,lock,baseline)
 output.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='.restore-named-audio-',dir=output)as temporary:
  temporary=Path(temporary)
  def one(pair):
   index,item=pair;entry,expected,fingerprint,target,_=item;job=temporary/str(index)
   manifest=audio.compile_assets({**recipe,'assets':[entry]},upstream,job)
   require(manifest['upstream_commit']==PIN and manifest['ffmpeg_version']==decoder['version']and manifest['ffmpeg_sha256']==decoder['sha256'],'Named SFX restore actual decoder differs from reviewed decoder')
   actual=dict(manifest['assets'][0]);actual.pop('command',None)
   require(actual==expected,'Named SFX restore decoded source metadata differs: '+entry['pcm_path'])
   converted=audio.parse_bank((job/'sound/banks/opening.encaudio').read_bytes())
   require(converted['assets']==[records[entry['pcm_path']]]and converted['master_db']==bank['master_db']and converted['silence_db']==bank['silence_db'],'Named SFX restore converted bank record changed')
   candidate=job/entry['pcm_path'];payload=candidate.read_bytes()
   require(len(payload)==fingerprint['size']and hashlib.sha256(payload).hexdigest()==fingerprint['sha256']and zlib.crc32(payload)&0xffffffff==expected['pcm_crc32'],'Named SFX restore converted PCM differs; no file published')
   return candidate,target,entry['pcm_path'],len(payload)
  with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:converted=list(pool.map(one,enumerate(missing)))
  # Complete every conversion/check before publishing any candidate. A file
  # created concurrently is never overwritten, even if its bytes happen to match.
  for candidate,target,path,size in converted:
   safe_target(output,path);target.parent.mkdir(parents=True,exist_ok=True);os.link(candidate,target)
 return dict(status='restored',assets=len(plan),restored=len(missing),converted_bytes=sum(x[3]for x in converted),workers=4)


def encode(d):
 import math
 receipt=read(RECEIPT);require(receipt['recipe_sha256']==sha(RECIPE)and receipt['ir_sha256']==sha(IR),'Named SFX PCM receipt stale')
 for f in receipt['files']:require(sha(ROOT/'romfs'/f['path'])==f['sha256'],'Named SFX converted bytes stale')
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):x=s.encode();u(len(x));b.extend(x)
 def value(v):
  if v is None:u(0)
  elif type(v)is bool:u(1,int(v))
  elif type(v)is int:u(2);b.extend(struct.pack('<q',v))
  elif type(v)is float:require(math.isfinite(v),'Nonfinite audio source');u(3);b.extend(struct.pack('<d',v))
  elif isinstance(v,str):u(4);t(v)
  elif isinstance(v,list):u(5,len(v));[value(x)for x in v]
  elif isinstance(v,dict):u(6,len(v));[(t(k),value(x))for k,x in v.items()]
  else:raise ValueError('Unsupported source Variant')
 recipe={**d['recipe'],'sources':d['sources']};sub=recipes.encode_recipe(recipe);u(recipe['scene_id']);b.extend(bytes.fromhex(recipe['source_sha256']));u(len(sub));b.extend(sub)
 snapshot={k:v for k,v in d['native_snapshot'].items()if k not in ['streams']};value(snapshot);value(d['constructor']);value(d['effects']);value(d['methods']);value(decode(d['voice_defaults']));t(d['bus']);t(d['music_bus']);t(d['bank']);b.extend(bytes.fromhex(sha(ROOT/'romfs'/d['bank'])));u(stable('AudioStreamPlayer.new'))
 u(len(d['streams']))
 for v in d['streams']:
  u(stable(v['source']),v['asset_id']);t(v['source']);t(v['native']);b.extend(bytes.fromhex(v['source_sha256']));b.extend(bytes.fromhex(v['import_sha256']));b.extend(bytes.fromhex(v['payload_sha256']));value(v['metadata']);u(len(v['payload']));b.extend(bytes(v['payload']))
 struct.pack_into('<8s8I',b,0,b'ENCNSFX1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,recipe['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(root):
 d=load();raw=encode(d);root=Path(root);require((root/'data/audio-manager.encnamedsfx').read_bytes()==raw,'Named SFX pack stale');out={Path('data/audio-manager.encnamedsfx'):raw}
 for f in read(RECEIPT)['files']:
  p=Path(f['path']);raw=(root/p).read_bytes();require(hashlib.sha256(raw).hexdigest()==f['sha256'],'Named SFX staged PCM stale');out[p]=raw
 return out

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','assets','restore','compile']);p.add_argument('--directory',type=Path);p.add_argument('--output',type=Path,default=ROOT/'romfs');a=p.parse_args()
 try:
  if a.action=='restore':print(json.dumps(restore_assets(a.output),indent=2))
  elif a.action=='assets':m=compile_assets(a.output);print('Named SFX actual PCM:',len(m['assets']),'assets; 4 workers')
  elif a.action=='extract':d=extract(a.directory);print('Named SFX source:',len(d['effects']),'constructor streams;',len(d['streams']),'actual payloads;',len(d['recipe']['records']),'native nodes; no cold Ready')
  else:raw=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);print('Named SFX actual native resource:',len(raw),'bytes')
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('NAMED SFX ERROR: '+str(e))
