"""Compile the exact audited mechanism sounds into an independent PCM bank.

Source-node ownership is retained in the receipt, including independent wall
property writes and audioManager's shared denial channel. No live sound output
or binding-JSON runtime admission is provided by this candidate compiler.
"""
import argparse,hashlib,json,shutil,sys,tempfile,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import audio_asset as audio,dungeon_mechanism_bindings as bindings
from tools.extract_battle_entry import PIN,Extractor,require
from tools.godot_text import node_path
RECIPE=ROOT/'content/catacombs-mechanism-audio.json'

def fields(p,s):require(type(p)is dict and set(p)==set(s.split()),'Mechanism audio fields')
def extract(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate mechanism audio review field');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite mechanism audio review')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid);fields(p,'schema kind commit scope bindings_recipe bindings_review_sha256 permission recipe identities source_profiles')
 require(type(p['schema'])is int and p['schema']==2 and p['kind']=='encore.dungeon-mechanism-audio.reviewed-source'and p['commit']==PIN,'Mechanism audio schema/pin')
 require(all(type(p[k])is str and 0<len(p[k])<=2048 for k in('scope','permission','identities'))and p['bindings_recipe']=='content/catacombs-mechanism-bindings.json'and p['bindings_review_sha256']==hashlib.sha256(bindings.RECIPE.read_bytes()).hexdigest(),'Mechanism audio dependency/scope')
 recipe=p['recipe'];fields(recipe,'schema upstream_commit bus_source bus_sha256 manager_source manager_sha256 assets');require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['upstream_commit']==PIN and type(recipe['assets'])is list and 0<len(recipe['assets'])<=64,'Mechanism audio recipe identity')
 root=json.loads((ROOT/'content/native-audio.json').read_bytes());require(all(recipe[k]==root[k]for k in('bus_source','bus_sha256','manager_source','manager_sha256')),'Mechanism audio reviewed root bus/manager binding differs')
 model,_,_=bindings.extract();review=bindings.read();ex=Extractor(ROOT);source=bindings.graph.Sources(ROOT);greview,_=bindings.graph.read();nodes={n.path:n for n in bindings.graph.instantiate(source.doc,greview['paths']['scene'])};profiles={p['id']:p for p in model['profiles']};owners=[];expected=set();wall_sound=None
 require(type(p['source_profiles'])is list and 0<len(p['source_profiles'])<=64,'Mechanism sound source selectors');selectors={}
 for row in p['source_profiles']:
  fields(row,'profile node');bindings.integer(row['profile'],1,65535);bindings.wall.path(row['node']);require(row['profile']not in selectors and'/'not in row['node'],'Mechanism sound source selector identity/path');selectors[row['profile']]=row['node']
 require(set(selectors)=={r['id']for r in model['profiles']if r['family']!=3},'Mechanism sound source selector coverage')
 for row in review['bindings']:
  if row['profile']not in selectors:require(profiles[row['profile']]['family']==3,'Mechanism source audio owner absent');continue
  node=nodes.get(row['path']+'/'+selectors[row['profile']]);require(node is not None,'Mechanism selected source audio node missing')
  require(node.type=='AudioStreamPlayer'and not node.overrides and set(node.props)<=set('stream bus volume_db pitch_scale'.split()),'Unreviewed mechanism AudioStreamPlayer property/override')
  path=node.resolve.ext_path(node.props.get('stream'),'AudioStream');gain=node.props.get('volume_db',0);pitch=node.props.get('pitch_scale',1)
  require(node.props.get('bus','Master')=='SFX'and type(gain)in(int,float)and gain==0 and type(pitch)in(int,float)and pitch==1,'Unreviewed mechanism audio bus/gain/pitch')
  expected.add('res://'+path);owners.append(dict(owner=row['owner'],path=row['path']+'/'+selectors[row['profile']],source_path='res://'+path,gain_db=gain,pitch_scale=pitch,bus=1,pause_mode=0))
  if profiles[row['profile']]['family']==4:wall_sound='res://'+path
 denied='res://'+model['graph']['denied'];expected.add(denied);assets={};ids=set()
 for r in recipe['assets']:
  fields(r,'stable_id source_path source_sha256 import_sha256 pcm_path gain_db'+(' output_sample_rate'if'output_sample_rate'in r else''));bindings.integer(r['stable_id'],1,2**32-1);require(r['stable_id']not in ids and r['source_path']not in assets and r['source_path']in expected and r['gain_db']==0 and type(r['gain_db'])in(int,float),'Mechanism audio asset identity/gain/coverage');ids.add(r['stable_id']);assets[r['source_path']]=r
  relative=audio.source_path(r['source_path']);audio.safe_path(r['pcm_path']);require(r['pcm_path'].startswith('sound/effects/')and r['pcm_path'].endswith('.pcm'),'Mechanism audio PCM ownership')
  require(hashlib.sha256(ex.data(relative)).hexdigest()==r['source_sha256']and hashlib.sha256(ex.data(relative+'.import')).hexdigest()==r['import_sha256'],'Mechanism audio source fingerprint changed');loop,offset=audio.import_settings(ex.data(relative+'.import'),r['source_path']);require(not loop and offset==0,'Unreviewed mechanism audio loop')
  existing=False
  for old in root['assets']:
   if old['stable_id']==r['stable_id']or old['source_path']==r['source_path']:require(old==r,'Mechanism audio root-bank identity collision');existing=True
  require(existing or'output_sample_rate'not in r,'Unreviewed mechanism sound sample-rate conversion')
 require(set(assets)==expected and wall_sound is not None,'Mechanism audio source coverage');wall=next(x for x in model['profiles']if x['family']==4)['wall'];require(assets[wall_sound]['stable_id']==wall['audio']['id'],'Mechanism wall timeline sound identity differs')
 for row in owners:row['stable_id']=assets[row['source_path']]['stable_id']
 for path,digest in((recipe['bus_source'],recipe['bus_sha256']),(recipe['manager_source'],recipe['manager_sha256'])):require(hashlib.sha256(ex.data(path)).hexdigest()==digest,'Mechanism audio bus/manager source changed')
 require("purpose you're using them is related to the game"in ex.text('LICENSE'),'Mechanism audio permission changed')
 return recipe,dict(commit=PIN,scope=p['scope'],permission=p['permission'],review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),sources={**source.digests(),**ex.sources},owners=owners,denied=dict(source_path=denied,stable_id=assets[denied]['stable_id'],channel=model['graph']['channel']),live_output_admission=False)

def compile_assets(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists(),'Fresh owned mechanism audio candidate required');recipe,proof=extract(file);out.mkdir(parents=True)
 with tempfile.TemporaryDirectory(prefix='decoded-',dir=out)as directory:
  decoded=Path(directory);manifest=audio.compile_assets(recipe,ROOT/'upstream/MOTHER-Encore',decoded);bank=(decoded/'sound/banks/opening.encaudio').read_bytes();audio.parse_bank(bank)
  for row in manifest['assets']:
   path=row['pcm_path'];target=out/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(decoded/path,target)
  target=out/'sound/banks/catacombs-mechanisms.encaudio';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(bank)
 bank_path='sound/banks/catacombs-mechanisms.encaudio';files=[dict(path=r['pcm_path'],bytes=r['pcm_bytes'],sha256=r['pcm_sha256'])for r in manifest['assets']];files.append(dict(path=bank_path,bytes=len(bank),sha256=hashlib.sha256(bank).hexdigest()))
 proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),decoder_manifest=manifest,bank=dict(path=bank_path,bytes=len(bank),sha256=hashlib.sha256(bank).hexdigest()),pcm_bytes=sum(r['pcm_bytes']for r in manifest['assets']),files=files)
 (out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual mechanism audio:',len(manifest['assets']),'PCM sources,',proof['pcm_bytes'],'bytes,',len(proof['owners']),'independent source nodes; no live audio admission');return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();compile_assets(a.out)
