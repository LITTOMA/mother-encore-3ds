"""Actual source-loop conversion and frozen real named-SFX observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_step_audio as audio,run_step_audio_reference as native
from tools.append_audio_assets import checked_previous
REVIEW=ROOT/'compatibility/reviews/catacombs-step-audio-v0410.json'
def build(out):
 out=Path(out).resolve();marker=out/'.owned-step-audio-fixture';audio.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and marker.read_bytes()==b'step-audio-fixture-v1'),'Owned step audio fixture under build required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=audio.unique);audio.fields(review,'schema source producer_sha256 native_producer_sha256 results mapping unverified','step audio semantic review');policy,metadata,recipe,_,source=audio.source();audio.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==hashlib.sha256(Path(audio.__file__).read_bytes()).hexdigest()and review['native_producer_sha256']==hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest()and type(review['results'])is list and len(review['results'])==2,'Step audio semantic review/producer changed');out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'step-audio-fixture-v1');candidate=out/'media'
 if not candidate.exists():audio.compile(candidate)
 else:
  audio.require(candidate.resolve()==candidate and not candidate.is_symlink()and(candidate/'.owned-catacombs-step-audio').read_bytes()==b'catacombs-step-audio-v1','Step audio fixture candidate owner changed');receipt=json.loads((candidate/'source.json').read_bytes(),object_pairs_hook=audio.unique);audio.fields(receipt,' '.join(list(source)+['conversion','output_bindings','producer_sha256']),'step audio cached receipt');audio.require(receipt['producer_sha256']==review['producer_sha256']and all(receipt[k]==v for k,v in source.items()if k!='scope'),'Stale step audio fixture producer/source');blob=(candidate/policy['output']).read_bytes();checked_previous(recipe,receipt['conversion'],blob,'sound/banks/opening.encaudio');expected=[dict(conversion_path=r['path'],candidate_path=policy['output']if r['path']=='sound/banks/opening.encaudio'else r['path'],bytes=r['size'],sha256=r['sha256'])for r in receipt['conversion']['files']];audio.require(receipt['output_bindings']==expected,'Step audio cached output coverage changed')
  for record in expected:
   path=candidate/record['candidate_path'];audio.require(path.resolve().is_relative_to(candidate)and not path.is_symlink(),'Step audio cached file escaped');data=path.read_bytes();audio.require(len(data)==record['bytes']and hashlib.sha256(data).hexdigest()==record['sha256'],'Stale step audio fixture payload')
 rows=[]
 for record in review['results']:
  audio.fields(record,'path sha256 rows','step audio native record');audio.safe(record['path']);p=(ROOT/record['path']).resolve();audio.require(p.is_relative_to(ROOT/'reports/catacombs-step-audio'),'Step audio original record escaped');raw=(p/'reference.json').read_bytes();proof=json.loads((p/'receipt.json').read_bytes(),object_pairs_hook=audio.unique);audio.require(hashlib.sha256(raw).hexdigest()==record['sha256']and proof['producer_sha256']==review['native_producer_sha256']and proof['result_sha256']==record['sha256'],'Frozen original run-audio result changed');doc=native.validate_result(json.loads(raw,object_pairs_hook=audio.unique),proof);audio.require(type(record['rows'])is int and record['rows']==len(doc['rows']),'Original run-audio observation count changed')
  if rows:continue
  for row in doc['rows']:rows.append(' '.join(str(v)for v in[row['index'],['initial','play','repeat','stop'].index(row['phase']),int(row['running']),int(row['playing']),row['gain_db'],row['pitch'],row['length']]))
 (out/'native.tsv').write_bytes(('\n'.join(rows)+'\n').encode());print('Prepared source8 loop bank and32 actual native states;not new engine/hardware execution');return candidate
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
