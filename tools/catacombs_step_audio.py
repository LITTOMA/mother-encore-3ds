"""Original eight run loops in an independent candidate ENCAUD01 bank."""
import argparse,hashlib,json,math,re,shutil,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_steps as steps,audio_asset
from tools.dungeon_keys import fields,path as safe
from tools.dungeon_key_indicator import unique
from tools.extract_battle_entry import Extractor,PIN,require
from tools.reference_movement import extract
RECIPE=ROOT/'content/catacombs-step-audio.json'
def read(file=RECIPE):
 def invalid(value):raise ValueError('Nonfinite stepping-audio JSON')
 p=json.loads(Path(file).read_text('utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit steps_recipe steps_recipe_sha256 bus_source bus_sha256 manager_source manager_sha256 bus gain_db pitch output scope license_review','stepping audio review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-stepping.reviewed-audio'and p['commit']==PIN,'Stepping audio review identity')
 for key in['steps_recipe','bus_source','manager_source','output']:safe(p[key])
 require(p['steps_recipe']=='content/catacombs-stepping.json'and p['output'].startswith('sound/banks/')and p['output'].endswith('.encaudio'),'Stepping audio source/output owner')
 require(type(p['bus'])is str and p['bus']=='SFX'and type(p['gain_db'])in(int,float)and math.isfinite(p['gain_db'])and p['gain_db']==0 and type(p['pitch'])in(int,float)and math.isfinite(p['pitch'])and p['pitch']==1,'Stepping source SFX node defaults')
 require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in['scope','license_review']),'Stepping audio review scope/license')
 for key in['steps_recipe_sha256','bus_sha256','manager_sha256']:require(type(p[key])is str and re.fullmatch('[0-9a-f]{64}',p[key])and int(p[key],16),'Stepping audio source digest')
 require(hashlib.sha256((ROOT/p['steps_recipe']).read_bytes()).hexdigest()==p['steps_recipe_sha256'],'Stepping audio source review changed');ex=Extractor(ROOT)
 for key in['bus','manager']:require(hashlib.sha256(ex.data(p[key+'_source'])).hexdigest()==p[key+'_sha256'],'Stepping audio original service source changed')
 return p
def source(file=RECIPE):
 p=read(file);_,m,original=steps.source();ex=Extractor(ROOT);manager=ex.data(p['manager_source']);methods,hashes=extract(manager,hashlib.sha256(manager).hexdigest(),['add_sfx','play_sfx','get_sfx']);require('sfx_node = AudioStreamPlayer.new()'in methods and 'sfx_node.bus = "'+p['bus']+'"'in methods and 'sfx_node.stream = stream'in methods,'Stepping named-SFX source binding changed')
 recipe=dict(schema=1,upstream_commit=PIN,**{k:p[k]for k in['bus_source','bus_sha256','manager_source','manager_sha256']},assets=[dict(stable_id=s['id'],source_path=s['source'],source_sha256=s['sha256'],import_sha256=s['import_sha256'],pcm_path=s['pcm'],gain_db=p['gain_db'])for s in m['sounds']])
 proof=dict(commit=PIN,source=original,review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),manager_methods=hashes,service_sources={p[k+'_source']:p[k+'_sha256']for k in['bus','manager']},source_defaults=dict(bus=p['bus'],gain_db=p['gain_db'],pitch=p['pitch']),native_defaults_unverified=True,scope=p['scope']);return p,m,recipe,methods,proof
def compile(out,ffmpeg='ffmpeg',ffprobe='ffprobe'):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned stepping audio candidate under build required');p,m,recipe,_,proof=source();out.mkdir(parents=True);(out/'.owned-catacombs-step-audio').write_bytes(b'catacombs-step-audio-v1');bindings=[]
 with tempfile.TemporaryDirectory(prefix='decode-',dir=out)as raw:
  stage=Path(raw).resolve();require(stage.parent==out,'Stepping audio temporary output escaped');receipt=audio_asset.compile_assets(recipe,ROOT/'upstream/MOTHER-Encore',stage,ffmpeg=ffmpeg,ffprobe=ffprobe);expected={'sound/banks/opening.encaudio'}|{s['pcm']for s in m['sounds']};require({v['path']for v in receipt['files']}==expected,'Stepping audio conversion coverage')
  for row in receipt['files']:
   path=p['output']if row['path']=='sound/banks/opening.encaudio'else row['path'];source_file=stage/row['path'];data=source_file.read_bytes();require(len(data)==row['size']and hashlib.sha256(data).hexdigest()==row['sha256'],'Stepping audio conversion bytes changed');target=out/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source_file,target);bindings.append(dict(conversion_path=row['path'],candidate_path=path,bytes=len(data),sha256=row['sha256']))
 bank=audio_asset.parse_bank((out/p['output']).read_bytes());require(len(bank['assets'])==len(m['sounds'])and all(a['stable_id']==s['id']and a['source_path']==s['source']and a['source_sha256']==s['sha256']and a['loop']and a['loop_start']==0 and a['gain_db']==p['gain_db']for a,s in zip(bank['assets'],m['sounds'])),'Stepping original bank/loop binding mismatch')
 proof.update(conversion=receipt,output_bindings=bindings,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),scope='Actual original PCM conversion and independent bank only;not native playback/defaults,scene,NDSP,perceptual audio or hardware verification');(out/'source.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Stepping audio candidate:',len(bank['assets']),'original full loops;',sum(a['pcm_bytes']for a in bank['assets']),'PCM bytes;not staged/no NDSP or audible claim');return bank
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--ffmpeg',default='ffmpeg');a.add_argument('--ffprobe',default='ffprobe');o=a.parse_args();compile(o.out,o.ffmpeg,o.ffprobe)
