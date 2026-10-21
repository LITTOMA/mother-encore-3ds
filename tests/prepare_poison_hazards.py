"""Rebuild small checked poison metadata and exact frozen native TSVs."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_poison as poison,run_poison_hazards_reference as native,catacombs_steps as steps
REVIEW=ROOT/'compatibility/reviews/poison-hazards-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-poison-fixture';poison.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not out.is_symlink()and marker.read_bytes()==b'poison-fixture-v1'),'Owned poison fixture under build required')
 review=json.loads(REVIEW.read_bytes(),object_pairs_hook=poison.unique);poison.fields(review,'schema source producer_sha256 native_producer_sha256 results mapping unverified','poison semantic review');_,m,source=poison.source();producer=hashlib.sha256(Path(poison.__file__).read_bytes()).hexdigest();native_producer=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest();poison.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==producer and review['native_producer_sha256']==native_producer and type(review['results'])is list and len(review['results'])==2,'Poison semantic review/source/producer changed');docs=[]
 for row in review['results']:
  poison.fields(row,'path sha256','poison original result');poison.safe(row['path']);path=(ROOT/row['path']).resolve();poison.require(path.is_relative_to(ROOT/'reports/poison-hazards')and not path.is_symlink(),'Poison frozen result escaped');raw=(path/'reference.json').read_bytes();proof=json.loads((path/'receipt.json').read_bytes(),object_pairs_hook=poison.unique);poison.require(hashlib.sha256(raw).hexdigest()==row['sha256']and proof['result_sha256']==row['sha256']and proof['producer_sha256']==native_producer and proof['source']==source,'Poison frozen source/result receipt differs');docs.append(native.validate_result(json.loads(raw,object_pairs_hook=poison.unique),proof))
 poison.require(docs[0]==docs[1],'Poison two original observations differ');doc=docs[0];clock=[str(len(m['holes']))]
 for row in doc['initial']:clock.append(' '.join(str(int(v))for v in row))
 clock.append(str(len(doc['rows'])))
 for row in doc['rows']:
  clock.append(' '.join(map(str,[row['tick'],row['delta_bits'],int(row['before_tree_pause']),len(row['actions']),len(row['timeouts'])])))
  for op in row['actions']:
   code={'player_pause':1,'hole_pause':2,'tree_pause':3,'visible':4}[op[0]];clock.append(' '.join(map(str,[code,op[1]if code==2 else 0,int(op[2]if code==2 else op[1])])))
  clock+=[' '.join(str(int(v))for v in observed)for observed in row['clocks']];clock+=list(map(str,row['timeouts']))
 clock.append(str(len(doc['spawned'])));clock+=[' '.join(map(str,v))for v in doc['spawned']];_,sm,_=steps.source();sounds=[s['name']for s in sm['sounds']];modes=[s['name']for s in sm['shadows']['animations']];contacts=[str(len(doc['contacts']))]
 for row in doc['contacts']:
  values=[int(v)for v in row['action']]+[sounds.index(row['wanted'])]
  for key in['player','party']:s=row[key];values += [int(s[0]),s[1],s[2],len(s[3]),modes.index(s[4])]
  contacts.append(' '.join(map(str,values)))
 outputs={'catacombs.encpoison':poison.encode(m),'clocks.tsv':('\n'.join(clock)+'\n').encode(),'contacts.tsv':('\n'.join(contacts)+'\n').encode()}
 for name,value in outputs.items():
  target=out/name;poison.require(not target.exists()or target.is_file()and not target.is_symlink()and target.read_bytes()==value,'Poison cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'poison-fixture-v1')
 for name,value in outputs.items():(out/name).write_bytes(value)
 print('Checked original poison metadata and720/99/102 TSVs;not new engine execution');return outputs
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
