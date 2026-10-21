"""Compile checked Geyser resource and exact frozen native command TSV."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_geyser as g,run_geyser_reference as native
REVIEW=ROOT/'compatibility/reviews/geyser-timeline-v0410.json'
def build(out):
 out=Path(out).resolve();marker=out/'.owned-geyser-fixture';g.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not out.is_symlink()and marker.read_bytes()==b'geyser-fixture-v1'),'Owned Geyser fixture under build required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=g.unique);g.fields(review,'schema source producer_sha256 native_producer_sha256 results mapping unverified','geyser semantic review');_,m,source=g.source();producer=hashlib.sha256(Path(g.__file__).read_bytes()).hexdigest();native_producer=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest();g.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==producer and review['native_producer_sha256']==native_producer and type(review['results'])is list and len(review['results'])==2,'Geyser semantic review/source/producer changed');docs=[]
 for row in review['results']:
  g.fields(row,'path sha256','geyser original result');g.safe(row['path']);folder=(ROOT/row['path']).resolve();g.require(folder.is_relative_to(ROOT/'reports/geyser-timeline')and not folder.is_symlink(),'Geyser frozen result escaped');raw=(folder/'reference.json').read_bytes();proof=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=g.unique);g.require(hashlib.sha256(raw).hexdigest()==row['sha256']and proof['result_sha256']==row['sha256']and proof['producer_sha256']==native_producer and proof['source']==source,'Geyser frozen source/result receipt differs');docs.append(native.validate_result(json.loads(raw,object_pairs_hook=g.unique),proof))
 g.require(docs[0]==docs[1],'Geyser two original observations differ');doc=docs[0];lines=[str(len(native.CASES))];ids={'res://'+a['source']:a['id']for a in m['sounds']};last_sound=0
 def observed(row):
  nonlocal last_sound
  lines.append(' '.join(map(str,[row['time'],row['frame'],int(row['monitoring']),int(row['disabled']),int(row['active']),int(row['paused']),row['gain'],int(row['queued']),int(bool(row['finished']))])))
  commands=[]
  for c in row['commands']:
   if c[0]=='stream':last_sound=ids[c[1]];commands.append([1,last_sound,0])
   elif c[0]=='play':commands.append([2,last_sound,c[1]])
   elif c[0]=='stop':commands.append([3,0,0])
   else:raise ValueError('Unsupported native Geyser command')
  for c in row['global_calls']:commands.append([4,ids[c[0]],0])
  lines.append(str(len(commands)));lines.extend(' '.join(map(str,c))for c in commands)
 for index,c in enumerate(native.CASES):
  rows=[v for v in doc['rows']if v['case']==index];lines.append(' '.join(map(str,[len(c['deltas']),c.get('voice',1),int(not c.get('offscreen',False))])));last_sound=0;observed(rows[0])
  for j in range(len(c['deltas'])):lines.append(' '.join(map(str,[int(c['pause'][j])if j in c.get('pause',{})else -1,rows[1+2*j]['delta'],c.get('gate',{}).get(j,0)])));observed(rows[1+2*j]);observed(rows[2+2*j])
 outputs={'geyser.encgeyser':g.encode(m),'timeline.tsv':('\n'.join(lines)+'\n').encode()}
 for name,value in outputs.items():target=out/name;g.require(not target.exists()or target.is_file()and not target.is_symlink()and target.read_bytes()==value,'Geyser cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'geyser-fixture-v1')
 for name,value in outputs.items():(out/name).write_bytes(value)
 print('Checked Geyser resource and451 manual/deferred observations;not new native execution');return outputs
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
