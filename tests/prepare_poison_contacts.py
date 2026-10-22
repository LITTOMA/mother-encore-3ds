"""Checked independent contact resource and original settled-state fixtures."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import poison_contacts as contacts,run_poison_contact_reference as native,world_damage as damage,catacombs_steps as steps
from tests import prepare_world_damage as protocol
REVIEW=ROOT/'compatibility/reviews/poison-contacts-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-poison-contacts-fixture';contacts.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'poison-contacts-fixture-v1'),'Owned poison contact fixture required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=contacts.unique);contacts.fields(review,'schema source producer_sha256 native_producer_sha256 fixture_protocol_sha256 results mapping unverified','poison contact review');_,model,source=contacts.source();files,_,native_source=native.source();contacts.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==hashlib.sha256(Path(contacts.__file__).read_bytes()).hexdigest()and review['native_producer_sha256']==hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest()and review['fixture_protocol_sha256']==hashlib.sha256(Path(protocol.__file__).read_bytes()).hexdigest()and type(review['results'])is list and len(review['results'])==2,'Poison contact source/producer/protocol changed');docs=[]
 for row in review['results']:
  contacts.fields(row,'path sha256','poison contact frozen result');contacts.poison.safe(row['path']);folder=(ROOT/row['path']).resolve();contacts.require(folder.is_relative_to(ROOT/'reports/poison-contacts')and not folder.is_symlink(),'Poison contact frozen path escaped');raw=(folder/'reference.json').read_bytes();proof=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=contacts.unique);contacts.require(hashlib.sha256(raw).hexdigest()==row['sha256']and proof['result_sha256']==row['sha256']and proof['producer_sha256']==review['native_producer_sha256'],'Poison contact frozen bytes/producer changed')
  for name,digest in native_source['generated_sha256'].items():path=(folder/'observer'/name).resolve();contacts.require(path.is_relative_to(folder/'observer')and not path.is_symlink()and hashlib.sha256(path.read_bytes()).hexdigest()==digest,'Poison contact frozen observer changed')
  docs.append(native.validate_result(json.loads(raw,object_pairs_hook=contacts.unique),proof))
 contacts.require(docs[0]==docs[1],'Poison contact originals differ');config=json.loads(files['input.json']);_,dm,_=damage.source();_,sm,_=steps.source();delta=struct.unpack('<I',struct.pack('<f',1/60))[0];lines=[' '.join(map(str,[len(config['cases']),config['hp'],config['maxhp'],config['seed'],delta,config['initial_run']]))];seen=set()
 for ci,case in enumerate(config['cases']):
  lines.append(str(3 if case['other']else 1))
  for row in docs[0]['rows'][ci*len(native.PHASES):(ci+1)*len(native.PHASES)]:
   lines.append(' '.join(map(str,[native.PHASES.index(row['phase']),*row['position'],row['layer'],row['mask'],int(row['enabled']),int(row['paused']),int(row['continuous']),row['damage'],row['variance'],row['run_sound'],row['shadow'],len(row['contacts'])])))
   for event in row['contacts']:lines.append(' '.join(str(int(v))for v in event))
   lines.append(' '.join(str(int(v))for v in row['overlaps']));protocol.append_observation(lines,{k:v for k,v in row.items()if k not in native.EXTRA},dm,seen)
 outputs={'contacts.encpcn':contacts.encode(model),'damage.encwdm':damage.encode(dm),'steps.encsteps':steps.encode(steps.binary_model(sm)),'native.tsv':('\n'.join(lines)+'\n').encode()}
 for name,raw in outputs.items():path=out/name;contacts.require(not path.exists()or path.is_file()and not path.is_symlink()and path.read_bytes()==raw,'Poison contact cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'poison-contacts-fixture-v1')
 for name,raw in outputs.items():(out/name).write_bytes(raw)
 print('Checked poison contact resource and two105-state originals;not new native execution');return outputs

if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
