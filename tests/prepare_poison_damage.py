"""Build checked existing metadata and combined poison/Character observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import world_damage as d,catacombs_poison as p,catacombs_steps as s,run_poison_damage_reference as n
from tests import prepare_world_damage as common
REVIEW=ROOT/'compatibility/reviews/poison-damage-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-poison-damage-fixture';d.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'poison-damage-fixture-v1'),'Owned poison damage fixture under build required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=d.unique);d.fields(review,'schema source native_producer_sha256 fixture_protocol_sha256 results mapping unverified','poison damage review');files,_,source=n.source();d.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['native_producer_sha256']==hashlib.sha256(Path(n.__file__).read_bytes()).hexdigest()and review['fixture_protocol_sha256']==hashlib.sha256(Path(common.__file__).read_bytes()).hexdigest()and type(review['results'])is list and len(review['results'])==2,'Poison damage source/protocol review changed');docs=[]
 for row in review['results']:
  d.fields(row,'path sha256','poison damage original result');d.text(row['path']);folder=(ROOT/row['path']).resolve();d.require(folder.is_relative_to(ROOT/'reports/poison-damage')and not folder.is_symlink(),'Poison damage frozen result escaped');raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=d.unique)
  d.require(hashlib.sha256(raw).hexdigest()==row['sha256']and receipt['result_sha256']==row['sha256']and receipt['producer_sha256']==review['native_producer_sha256'],'Poison damage result/producer changed')
  for name,h in source['generated_sha256'].items():file=(folder/'observer'/name).resolve();d.require(file.is_relative_to(folder/'observer')and not file.is_symlink()and hashlib.sha256(file.read_bytes()).hexdigest()==h,'Poison damage frozen observer changed')
  docs.append(n.validate_result(json.loads(raw,object_pairs_hook=d.unique),receipt))
 d.require(docs[0]==docs[1],'Poison damage originals differ');_,dm,_=d.source();_,pm,_=p.source();_,sm,_=s.source();config=json.loads(files['input.json']);lines=[' '.join(map(str,[config['hp'],config['maxhp'],config['seed'],config['initial_run'],len(docs[0]['rows'])]))];seen=set()
 for row,action in zip(docs[0]['rows'],config['actions']):
  if action[0]=='area':line=[1,pm['areas'][action[1]]['id'],int(action[2]),1 if action[3]=='player'else 3]
  elif action[0]=='geyser':line=[2,int(action[1]),1 if action[2]=='player'else 3]
  elif action[0]=='pause':line=[3,int(action[1])]
  elif action[0]=='tick':line=[4,repr(action[1])]
  else:raise ValueError('Unknown poison damage action')
  lines.append(' '.join(map(str,line)));lines.append(' '.join(map(str,[int(row['continuous']),row['damage'],row['variance'],int(row['paused']),row['run_sound'],row['shadow']])));base={k:v for k,v in row.items()if k not in n.EXTRA};common.append_observation(lines,base,dm,seen)
 outputs={'damage.encwdm':d.encode(dm),'poison.encpoison':p.encode(pm),'steps.encsteps':s.encode(s.binary_model(sm)),'native.tsv':('\n'.join(lines)+'\n').encode()}
 for name,b in outputs.items():target=out/name;d.require(not target.exists()or target.is_file()and not target.is_symlink()and target.read_bytes()==b,'Poison damage cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'poison-damage-fixture-v1')
 for name,b in outputs.items():(out/name).write_bytes(b)
 print('Checked existing three resource formats and284 combined original observations;no new native execution');return outputs
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
