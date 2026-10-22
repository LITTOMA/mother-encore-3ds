"""Unchanged independent resources and original Geyser contact observations."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_geyser_contact_reference as native,poison_contacts as body,catacombs_geyser_lifecycle as life,world_damage as damage,catacombs_steps as steps
from tests import prepare_world_damage as protocol
REVIEW=ROOT/'compatibility/reviews/geyser-contacts-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-geyser-contacts-fixture';body.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'geyser-contacts-fixture-v1'),'Owned Geyser contact fixture required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=body.unique);body.fields(review,'schema source native_producer_sha256 fixture_protocol_sha256 results mapping unverified','Geyser contact review');files,_,source=native.source();body.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['native_producer_sha256']==hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest()and review['fixture_protocol_sha256']==hashlib.sha256(Path(protocol.__file__).read_bytes()).hexdigest()and type(review['results'])is list and len(review['results'])==2,'Geyser contact source/protocol changed');docs=[]
 for row in review['results']:
  body.fields(row,'path sha256','Geyser contact result');body.poison.safe(row['path']);folder=(ROOT/row['path']).resolve();body.require(folder.is_relative_to(ROOT/'reports/geyser-contacts')and not folder.is_symlink(),'Geyser contact frozen path escaped');raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=body.unique);body.require(hashlib.sha256(raw).hexdigest()==row['sha256']==receipt['result_sha256']and receipt['producer_sha256']==review['native_producer_sha256']and receipt.get('engine_sha256')=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Geyser contact frozen bytes/producer/engine changed')
  for name,h in source['generated_sha256'].items():p=(folder/'observer'/name).resolve();body.require(p.is_relative_to(folder/'observer')and not p.is_symlink()and hashlib.sha256(p.read_bytes()).hexdigest()==h,'Geyser contact frozen observer changed')
  docs.append(native.validate_result(json.loads(raw,object_pairs_hook=body.unique),receipt))
 body.require(docs[0]==docs[1],'Geyser contact originals differ');config=json.loads(files['input.json']);_,bm,_=body.source();_,lm,_=life.source();_,dm,_=damage.source();_,sm,_=steps.source();bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0];lines=[' '.join(map(str,[len(config['cases']),config['hp'],config['maxhp'],config['seed'],bits(config['arm']),bits(config['disarm']-config['arm']),bits(config['finish']),config['initial_run']])),' '.join(str(v)for p in docs[0]['geometry']for v in p)];seen=set()
 for ci,case in enumerate(config['cases']):
  lines.append(' '.join(str(int(case[k]))for k in ['other','monitoring_stop','animation_finish']))
  for row in docs[0]['rows'][ci*len(native.PHASES):(ci+1)*len(native.PHASES)]:
   g=row['geyser'];values=[native.PHASES.index(row['phase']),*row['position'],row['layer'],row['mask'],int(row['enabled']),int(row['paused']),int(row['continuous']),row['damage'],row['variance'],row['run_sound'],row['shadow'],int(g['alive'])]
   if g['alive']:values.extend([int(g['queued']),int(g['monitoring']),int(g['disabled']),int(g['overlap']),g['time'],g['frame']])
   lines.append(' '.join(map(str,values+[len(row['contacts'])])))
   for event in row['contacts']:lines.append(' '.join(str(int(v))for v in event))
   protocol.append_observation(lines,{k:v for k,v in row.items()if k not in native.EXTRA},dm,seen)
 outputs={'body.encpcn':body.encode(bm),'family.encglf':life.encode(lm),'damage.encwdm':damage.encode(dm),'steps.encsteps':steps.encode(steps.binary_model(sm)),'native.tsv':('\n'.join(lines)+'\n').encode()}
 for name,raw in outputs.items():p=out/name;body.require(not p.exists()or p.is_file()and not p.is_symlink()and p.read_bytes()==raw,'Geyser contact cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'geyser-contacts-fixture-v1')
 for name,raw in outputs.items():(out/name).write_bytes(raw)
 print('Checked Geyser contact originals72 states and unchanged independent source resources');return outputs

if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
