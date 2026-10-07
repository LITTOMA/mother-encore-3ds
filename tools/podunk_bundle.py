#!/usr/bin/env python3
"""Source-backed complete Podunk resource closure. No lifecycle is admitted."""
from pathlib import Path
import argparse, concurrent.futures, hashlib, importlib, json, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT),str(ROOT/'tools')]
from tools.podunk_scene import PIN,read,sha,require,write,stable
RECIPE=ROOT/'content/podunk-bundle-recipe.json'
IR=ROOT/'content/native-podunk-bundle.json'
REVIEW=ROOT/'reports/podunk-bundle/source-review.json'
PACK=ROOT/'romfs/data/podunk.encbundle'
FAMILY=0x454e0066

def safe(p):
 return isinstance(p,str)and p and len(p)<2048 and not p.startswith('/')and ':'not in p and '\\'not in p and all(v not in ('','.','..')for v in p.split('/'))
def converted_paths(value):
 if isinstance(value,str):
  if value.startswith(('graphics/','sound/','fonts/','input/','shaders/')) and safe(value):yield value
 elif isinstance(value,list):
  for v in value:yield from converted_paths(v)
 elif isinstance(value,dict):
  for v in value.values():yield from converted_paths(v)
def check_declared_assets(value):
 if isinstance(value,dict):
  for key in ('path','output','pcm_path'):
   p=value.get(key)
   if isinstance(p,str)and p.startswith(('graphics/','sound/','fonts/','input/','shaders/'))and safe(p):
    fingerprint=value.get('output_sha256',value.get('pcm_sha256',value.get('sha256')))
    if isinstance(fingerprint,str)and re.fullmatch('[0-9a-f]{64}',fingerprint):require(sha(ROOT/'romfs'/p)==fingerprint,'Declared converted fingerprint differs '+p)
  for v in value.values():check_declared_assets(v)
 elif isinstance(value,list):
  for v in value:check_declared_assets(v)

def sources(d):
 result={}
 def visit(v):
  if isinstance(v,dict):
   for key,value in v.items():
    if key=='sources'and isinstance(value,dict):
     for p,h in value.items():
      if isinstance(h,str)and re.fullmatch('[0-9a-f]{64}',h):
       require(p not in result or result[p]==h,'Source identity alias '+p);result[p]=h
    elif key!='engine':visit(value)
  elif isinstance(v,list):
   for a in v:visit(a)
 visit(d);return result

def checked_pack(spec):
 require(safe(spec['path'])and spec['path'].startswith('data/'),'Pack path')
 ip=ROOT/spec['ir'];d=read(ip);check_declared_assets(d);require(d.get('commit')==PIN,'Pack source pin '+spec['ir'])
 m=importlib.import_module('tools.'+spec['producer']);raw=(ROOT/'romfs'/spec['path']).read_bytes()
 staged={}
 if hasattr(m,'stage_files'):staged=m.stage_files(ROOT/'romfs')
 else:
  if spec['producer']=='podunk_scene':expected=m.pack(d,read(ROOT/'content/podunk-scene-assets.json')['resources'])
  elif spec['producer']=='field_dandelion':
   art=m.assets();expected=m.pack(d,art);staged={Path(spec['path']):expected,**{Path(a['path']):(ROOT/'romfs'/a['path']).read_bytes()for a in art}}
  elif spec['producer']=='podunk_map':expected=m.pack(d,read(ROOT/'content/podunk-scene-map-assets.json')['textures'])
  elif hasattr(m,'encode')and hasattr(m,'load'):expected=m.encode(m.load())
  elif hasattr(m,'encode')and hasattr(m,'derive'):
   actual=m.derive();require(actual==d,'Source derivation differs '+spec['ir']);require(read(m.REVIEW)['ir_sha256']==sha(ip),'Source review differs');expected=m.encode(d)
  elif hasattr(m,'pack'):expected=m.pack(d)
  else:raise ValueError('No checked producer '+spec['producer'])
  require(raw==expected,'Producer output differs '+spec['path'])
 require(not staged or staged.get(Path(spec['path']))==raw,'Producer stage binding differs '+spec['path'])
 pin=raw.find(bytes.fromhex(PIN),0,160);require(pin>=0,'Missing actual source pin '+spec['path'])
 header_size=struct.unpack_from('<I',raw,12)[0]
 block=header_size==128
 family_offset=24 if block and 0x454e0000<=struct.unpack_from('<I',raw,24)[0]<=0x454effff else 28 if block else 20 if 0x454e0000<=struct.unpack_from('<I',raw,20)[0]<=0x454effff else 28
 family=struct.unpack_from('<I',raw,family_offset)[0]
 if not 0x454e0000<=family<=0x454effff:family=0 # original magic-only formats have no numeric family
 capability=struct.unpack_from('<I',raw,family_offset+4)[0]if family and (block or family_offset==20)else struct.unpack_from('<I',raw,20)[0]
 if raw[:8]==b'ENCFID01':capability=struct.unpack_from('<I',raw,24)[0]
 if raw[:8]in(b'ENCSIG01',b'ENCPRN01',b'ENCSCL01'):capability=struct.unpack_from('<I',raw,12)[0]
 require(0<capability<65536,'Capability schema '+spec['path'])
 ss=sources(d);scene=d.get('scene',d.get('source_save',d.get('owner',d.get('script',''))))
 scene=scene or read(RECIPE)['scene']
 source=d.get('source_sha256',d.get('scene_sha256',ss.get(scene,d.get('script_sha256'))))
 if not source and not block:
  source=ss.get(scene)
  if not source:
   context=read(ROOT/'content/podunk-scene.json');require(scene==context['scene'],'No checked context '+spec['path']);source=context['source_sha256']
 sid=d.get('scene_id',stable('.')if scene==read(RECIPE)['scene']else int.from_bytes(hashlib.sha256(scene.encode()).digest()[:4],'little'))
 if block:
  sid=struct.unpack_from('<I',raw,36)[0];source=raw[pin+20:pin+52].hex()
 require(sid and re.fullmatch('[0-9a-f]{64}',source),'Actual identity '+spec['path'])
 entry=dict(spec,identity_kind=1 if block else 2,magic=raw[:8].decode(),format=struct.unpack_from('<I',raw,8)[0],family=family,capability=capability,rules=d['rules']if type(d.get('rules'))is int else 1,scene=scene,scene_id=sid,source_sha256=source,ir_sha256=sha(ip),bytes=len(raw),crc32=zlib.crc32(raw),sha256=hashlib.sha256(raw).hexdigest(),original_header=raw[:128].hex(),sources=ss)
 return entry,staged,d

def one_pack(spec):
 try:return checked_pack(spec)
 except (ValueError,TypeError,KeyError,AttributeError,OSError)as e:raise ValueError(spec["path"]+": "+str(e))from e

AUDIO_RECIPE=ROOT/'content/podunk-bundle-audio.json'
AUDIO_RECEIPT=ROOT/'content/asset-receipts/audio/podunk-scene-effects.json'
AUDIO_BANK='sound/banks/podunk-scene-effects.encaudio'
def audio_recipe():
 base=read(ROOT/'content/native-audio.json');assets=[]
 for producer in ['field_dead_bush','field_emotes','field_openable_door']:
  m=importlib.import_module('tools.'+producer)
  for a in m.audio_bindings():
   source=a['source'];assets.append(dict(stable_id=a['identity']['value'],source_path='res://'+source,source_sha256=sha(ROOT/'upstream/MOTHER-Encore'/source),import_sha256=sha(ROOT/'upstream/MOTHER-Encore'/(source+'.import')),pcm_path=a['pcm'],gain_db=a['gain_db']))
 source='Audio/Sound effects/EB/knock.wav'
 inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(inv[source]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/source),'Scene knock source changed')
 # The native Door source sound is distinct from audioManager global voices.
 sid=int.from_bytes(hashlib.sha256(('native-audio:'+source).encode()).digest()[:4],'little')
 assets.append(dict(stable_id=sid,source_path='res://'+source,source_sha256=sha(ROOT/'upstream/MOTHER-Encore'/source),import_sha256=sha(ROOT/'upstream/MOTHER-Encore'/(source+'.import')),pcm_path='sound/effects/scene/knock.pcm',gain_db=0))
 require(len(assets)==7 and len({a['stable_id']for a in assets})==7,'Scene effects source coverage')
 return dict(schema=1,upstream_commit=PIN,bus_source=base['bus_source'],bus_sha256=base['bus_sha256'],manager_source=base['manager_source'],manager_sha256=base['manager_sha256'],assets=assets)
def convert_audio(ffmpeg,ffprobe,logs):
 require(logs,"Explicit private audio log directory required")
 import tempfile
 from tools import audio_asset as audio
 recipe=audio_recipe();reused={};old_recipe=read(AUDIO_RECIPE)if AUDIO_RECIPE.exists()else None;old_rc=read(AUDIO_RECEIPT)if AUDIO_RECEIPT.exists()else None
 if old_recipe is not None and old_rc is not None:
  require(old_rc['commit']==PIN and old_rc['recipe_sha256']==sha(AUDIO_RECIPE),'Old scene audio receipt changed')
  require(len(old_recipe['assets'])<=len(recipe['assets']) and len(old_recipe['assets'])==len(old_rc['assets']),'Old scene audio receipt coverage')
  for spec,proof,target in zip(old_recipe['assets'],old_rc['assets'],recipe['assets']):
   require({k:v for k,v in spec.items()if k!='pcm_path'}=={k:v for k,v in target.items()if k!='pcm_path'},'Unrelated scene audio source change during extension')
   require(safe(target['pcm_path'])and target['pcm_path'].startswith('sound/effects/'),'Scene audio target must remain a checked effect')
   payload=ROOT/'romfs'/spec['pcm_path'];require(sha(payload)==proof['pcm_sha256']and payload.stat().st_size==proof['pcm_bytes'],'Old scene audio PCM changed before extension')
   proof=dict(proof,pcm_path=target['pcm_path'])
   reused[spec['stable_id']]=(dict(assets=[proof],ffmpeg_sha256=old_rc['ffmpeg_sha256'],ffmpeg_version=old_rc['ffmpeg_version']),payload.read_bytes())
 write(AUDIO_RECIPE,recipe)
 def one(a):
  if a['stable_id']in reused:return reused[a['stable_id']]
  with tempfile.TemporaryDirectory(prefix='encore-source-scene-audio-')as tmp:
   r=dict(recipe,assets=[a]);v=audio.compile_assets(r,ROOT/'upstream/MOTHER-Encore',Path(tmp),ffmpeg,ffprobe);return v,(Path(tmp)/a['pcm_path']).read_bytes()
 with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:result=list(pool.map(one,recipe['assets']))
 records=[];proof=[]
 for m,b in result:
  record=m['assets'][0];proof.append(record);records.append({k:record[k]for k in ['stable_id','source_sha256','sample_rate','channels','loop','frames','loop_start','pcm_bytes','pcm_crc32','pcm_path','source_path','gain_db']});p=ROOT/'romfs'/record['pcm_path'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 manager=(ROOT/'upstream/MOTHER-Encore'/recipe['manager_source']).read_text(encoding='utf8');bus=(ROOT/'upstream/MOTHER-Encore'/recipe['bus_source']).read_text(encoding='utf8');floor=float(re.search(r'^const SILENT_SOUND_THRESHOLD = (-?[0-9.]+)$',manager,re.M)[1]);master=float(re.search(r'^bus/0/volume_db = (-?[0-9.]+)$',bus,re.M)[1]);b=audio.build_bank(records,master,floor);target=ROOT/'romfs'/AUDIO_BANK;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
 raw_receipt=dict(schema=1,commit=PIN,recipe_sha256=sha(AUDIO_RECIPE),workers=4,ffmpeg_sha256=result[0][0]['ffmpeg_sha256'],ffmpeg_version=result[0][0]['ffmpeg_version'],assets=proof,bank=dict(path=AUDIO_BANK,bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
 write(Path(logs)/'audio-conversion-raw.json',raw_receipt)
 for item in proof:item.pop('command',None)
 raw_receipt['producer_sha256']=sha(Path(__file__));write(AUDIO_RECEIPT,raw_receipt)
def restore_scene_audio(ffmpeg,ffprobe,logs):
 import os,tempfile
 from tools import audio_asset as audio
 recipe=read(AUDIO_RECIPE);require(recipe==audio_recipe(),'Scene audio source recipe changed');rc=read(AUDIO_RECEIPT);require(rc['commit']==PIN and rc['recipe_sha256']==sha(AUDIO_RECIPE)and len(rc['assets'])==len(recipe['assets']),'Scene audio restoration receipt')
 bank=(ROOT/'romfs'/AUDIO_BANK).read_bytes();require(len(bank)==rc['bank']['bytes']and hashlib.sha256(bank).hexdigest()==rc['bank']['sha256'],'Scene audio bank changed before restoration')
 def one(pair):
  spec,expected=pair;target=ROOT/'romfs'/spec['pcm_path']
  if target.exists():require(target.is_file()and target.stat().st_size==expected['pcm_bytes']and sha(target)==expected['pcm_sha256'],'Existing PCM differs; no overwrite');return dict(path=spec['pcm_path'],restored=False)
  target.parent.mkdir(parents=True,exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='source-audio-',dir=target.parent)as tmp:
   result=audio.compile_assets(dict(recipe,assets=[spec]),ROOT/'upstream/MOTHER-Encore',Path(tmp),ffmpeg,ffprobe);actual=result['assets'][0]
   for key,value in expected.items():require(actual[key]==value,'Reference decoder output differs '+key)
   candidate=Path(tmp)/spec['pcm_path'];require(not target.exists(),'PCM appeared concurrently; not overwritten');os.link(candidate,target)
   return dict(path=spec['pcm_path'],restored=True,raw=result)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:results=list(pool.map(one,zip(recipe['assets'],rc['assets'])))
 write(Path(logs or ROOT/'build/private-resource-generation/podunk-bundle')/'audio-restoration-raw.json',dict(workers=4,results=results))
 print('Scene effects PCM restored:',sum(v['restored']for v in results),'of',len(results),'without receipt changes')

def checked_audio():
 from tools import audio_asset as audio
 recipe=read(AUDIO_RECIPE);require(recipe==audio_recipe(),'Scene audio source recipe changed');rc=read(AUDIO_RECEIPT);require(rc['commit']==PIN and rc['recipe_sha256']==sha(AUDIO_RECIPE)and rc['workers']>=4 and len(rc['assets'])==len(recipe['assets']),'Scene audio conversion receipt')
 bank=(ROOT/'romfs'/AUDIO_BANK).read_bytes();require(len(bank)==rc['bank']['bytes']and hashlib.sha256(bank).hexdigest()==rc['bank']['sha256'],'Scene effects bank fingerprint')
 parsed=audio.parse_bank(bank);require(len(parsed['assets'])==len(recipe['assets']),'Scene effects bank count');out={AUDIO_BANK:bank}
 for spec,proof,record in zip(recipe['assets'],rc['assets'],parsed['assets']):
  for k in ['stable_id','source_path','source_sha256','import_sha256','pcm_path','gain_db']:require(spec[k]==proof[k],'Scene audio source identity')
  for k,v in record.items():require(proof[k]==v,'Scene audio bank record')
  b=(ROOT/'romfs'/spec['pcm_path']).read_bytes();require(len(b)==proof['pcm_bytes']and zlib.crc32(b)==proof['pcm_crc32']and hashlib.sha256(b).hexdigest()==proof['pcm_sha256'],'Scene actual PCM changed');out[spec['pcm_path']]=b
 from tools.restore_audio import validated_plan
 plan,_,_,_=validated_plan(ROOT,ROOT/'romfs')
 for a in plan:require(not a['missing'],'Existing reviewed PCM missing '+a['path']);out[a['path']]=a['target'].read_bytes()
 for _,_,bank_path in __import__('tools.restore_audio',fromlist=['BANKS']).BANKS:out[bank_path]=(ROOT/'romfs'/bank_path).read_bytes()
 from tools.podunk_music import checked
 music=checked(read(ROOT/'content/podunk-music.json'),ROOT/'upstream/MOTHER-Encore');mp='sound/banks/podunk.encmusic';require((ROOT/'romfs'/mp).read_bytes()==music,'Podunk actual music bank changed');out[mp]=music
 return out

def derive():
 recipe=read(RECIPE);require(recipe['schema']==1 and recipe['commit']==PIN and recipe['admission_ready']is False and len(recipe['packs'])==85,'Bundle recipe scope')
 with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:rows=list(pool.map(one_pack,recipe['packs']))
 packs=[];assets=checked_audio();inputs={RECIPE.relative_to(ROOT).as_posix():sha(RECIPE)};all_sources={}
 for entry,staged,d in rows:
  packs.append(entry);inputs[entry['ir']]=entry['ir_sha256'];inputs['tools/'+entry['producer']+'.py']=sha(ROOT/'tools'/ (entry['producer']+'.py'))
  for p,h in entry['sources'].items():require(p not in all_sources or all_sources[p]==h,'Conflicting actual source');all_sources[p]=h
  for p in set(converted_paths(d))|{p.as_posix()for p in staged if not p.as_posix().startswith('data/')}:
   fp=ROOT/'romfs'/p;require(fp.is_file(),'Missing converted dependency '+p);assets[p]=fp.read_bytes()
 for input in recipe['extra_inputs']+[AUDIO_RECIPE.relative_to(ROOT).as_posix(),AUDIO_RECEIPT.relative_to(ROOT).as_posix()]:
  inputs[input]=sha(ROOT/input);check_declared_assets(read(ROOT/input))
  for p in converted_paths(read(ROOT/input)):
   fp=ROOT/'romfs'/p;require(fp.is_file(),'Missing source asset '+p);assets[p]=fp.read_bytes()
 # Receipts are source-generation inputs. Keep only receipts whose actual output paths overlap this closure.
 for rp in (ROOT/'content/asset-receipts').rglob('*.json'):
  d=read(rp)
  if set(converted_paths(d))&assets.keys():check_declared_assets(d);inputs[rp.relative_to(ROOT).as_posix()]=sha(rp)
 inv=read(ROOT/'compatibility/upstream-inventory.json');require(inv['commit']==PIN,'Bundle inventory pin')
 for p,h in all_sources.items():
  require(p in inv['files']and inv['files'][p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Bundle upstream source differs '+p)
 lifecycle=read(ROOT/'content/podunk-scene-lifecycle.json');by={r['ir']:r['role']for r in packs};scripts=[]
 from tools.field_scene_host import ROLES
 mapping={1:'podunk-scene',2:'native-field-npc',3:'native-field-enemy',4:'native-field-tint',5:'native-field-sprite-bridge',6:'native-field-sprite-bridge',7:'podunk-scene-lifecycle',8:'podunk-scene-lifecycle',9:'podunk-scene-lifecycle',10:'podunk-scene-lifecycle',11:'native-field-emotes',12:'podunk-dandelion',13:'podunk-door',14:'native-field-prompts',15:'native-field-dead-bush',16:'podunk-openable-door',17:'podunk-sparkles',18:'native-field-interact-dialog',19:'native-field-payphone',20:'native-field-present',21:'native-field-dropped',22:'native-field-butterfly',23:'native-field-cutscene-area',24:'podunk-birds',25:'podunk-camera-area',26:'native-field-music-changer',27:'podunk-camera-arrows',28:'podunk-game-camera',29:'native-field-scene-actions',30:'native-field-scene-actions',31:'native-field-stepping-sounds',32:'native-field-player-transitions',33:'native-field-player-transitions',34:'native-field-door-npc',35:'native-field-melody-background',36:'native-field-vending-machine'}
 for role in range(1,37):scripts.append(dict(source_role=role,pack_role=by['content/'+mapping[role]+'.json'],actual_instances=sum(v['role']==role for v in lifecycle['roster'])))
 require(len(lifecycle['roster'])==2156 and all(v['role']in mapping for v in lifecycle['roster']),'Uncovered source lifecycle')
 art=[]
 for i,(p,b)in enumerate(sorted(assets.items()),1):
  kind=6 if p.endswith('.encmusic')else 5 if p.endswith('.encaudio')else 1 if p.endswith('.t3x')else 2 if p.endswith('.pcm')else 3 if p.endswith('.shbin')else 4 if p.startswith('fonts/')else 0
  require(kind,'Unsupported asset schema '+p)
  art.append(dict(id=i,kind=kind,path=p,bytes=len(b),crc32=zlib.crc32(b),sha256=hashlib.sha256(b).hexdigest()))
 base=read(ROOT/'content/podunk-scene.json')
 return dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=recipe['scene'],scene_id=base['scene_id'],source_sha256=base['source_sha256'],admission_ready=False,packs=packs,assets=art,script_bindings=scripts,inputs=inputs,sources=all_sources,semantics=['Complete resource closure only; 2156 actual Ready callbacks remain consumer-owned','PackedScene loading never grants unknown native/script Ready','File paths and SHA bindings are authoring data compiled to independent binary; no runtime JSON','Four source-admission workers; original generated packs and assets remain unmodified'])

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 def h(s):b.extend(bytes.fromhex(s))
 t(d['scene']);u(len(d['packs']),len(d['assets']),len(d['script_bindings']))
 for x in d['packs']:
  u(x['role'],x['identity_kind'],x['bytes'],x['crc32'],x['format'],x['family'],x['capability'],x['rules'],x['scene_id']);t(x['path']);h(x['sha256']);h(x['source_sha256']);h(x['ir_sha256']);v=bytes.fromhex(x['original_header']);u(len(v));b.extend(v)
 for x in d['assets']:u(x['id'],x['kind'],x['bytes'],x['crc32']);t(x['path']);h(x['sha256'])
 for x in d['script_bindings']:u(x['source_role'],x['pack_role'])
 struct.pack_into('<8s8I',b,0,b'ENCPBND1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def load():
 d=read(IR);require(d==derive()and read(REVIEW)['ir_sha256']==sha(IR),'Bundle source/closure review stale');return d
def stage_files(source):
 d=load();raw=encode(d);require((Path(source)/'data/podunk.encbundle').read_bytes()==raw,'Bundle binary stale');out={Path('data/podunk.encbundle'):raw}
 for r in d['packs']+d['assets']:
  p=Path(r['path']);b=(Path(source)/p).read_bytes();require(len(b)==r['bytes']and zlib.crc32(b)==r['crc32']and hashlib.sha256(b).hexdigest()==r['sha256'],'Bundle stage resource changed '+r['path']);out[p]=b
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify','audio','restore']);p.add_argument('--logs',type=Path);p.add_argument('--ffmpeg',default='ffmpeg');p.add_argument('--ffprobe',default='ffprobe');a=p.parse_args()
 try:
  if a.action=='restore':restore_scene_audio(a.ffmpeg,a.ffprobe,a.logs);sys.exit(0)
  if a.action=='audio':convert_audio(a.ffmpeg,a.ffprobe,a.logs);sys.exit(0)
  if a.action=='extract':d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],inputs=d['inputs'],semantics=d['semantics'],admission_ready=False))
  else:d=load()
  raw=encode(d)
  if a.action!='verify':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
  else:require(PACK.read_bytes()==raw,'Bundle binary differs')
  print('Podunk bundle:',len(d['packs']),'typed packs,',len(d['assets']),'assets,',len(raw),'bytes; lifecycle not admitted')
 except(ValueError,KeyError,OSError,TypeError,AttributeError,struct.error)as e:sys.exit('PODUNK BUNDLE ERROR: '+str(e))
