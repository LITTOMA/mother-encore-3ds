"""Checked polygon metadata and frozen actual-engine stepping observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_steps as s,run_catacombs_steps_reference as calls,run_catacombs_step_contacts_reference as contacts
REPORT=ROOT/'reports/catacombs-stepping'
REVIEW=ROOT/'compatibility/reviews/catacombs-stepping-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-catacombs-steps-fixture';s.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and marker.read_bytes()==b'catacombs-steps-fixture-v1\n'),'Owned stepping fixture under build required');out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'catacombs-steps-fixture-v1\n')
 _,m,proof=s.source();review=json.loads(REVIEW.read_bytes());s.require(review['source']==proof,'Catacombs stepping semantic review changed');binary=s.binary_model(m);blob=s.encode(binary);s.require(s.parse(blob)==binary,'Stepping fixture roundtrip');(out/'catacombs.encsteps').write_bytes(blob)
 native={}
 for kind,tool,file,validator in [('calls',calls,'reference.json',calls.validate_result),('contacts',contacts,'contacts.json',None)]:
  path=REPORT/('native-'+kind+'-4');raw=(path/file).read_bytes();receipt=json.loads((path/'receipt.json').read_bytes());s.require(hashlib.sha256(raw).hexdigest()==review[kind+'_sha256']and receipt['producer_sha256']==hashlib.sha256(Path(tool.__file__).read_bytes()).hexdigest(),'Frozen original stepping result/producer changed');doc=json.loads(raw)
  if validator:validator(doc)
  else:contacts.validate_result(doc,receipt)
  native[kind]=(doc,receipt)
 modes={a['name']:i for i,a in enumerate(binary['modes'])};sounds={a['name']:i for i,a in enumerate(binary['sounds'])};rows=[]
 for row in native['calls'][0]['rows']:
  action=row['action'];values=[action[0],*action[1:]]
  if action[0]==5:values[2]=modes[action[2]]
  values=(values+[0]*5)[:5];events=row['events'];values +=[row['wanted'],row['registered'],row['playing'],row['running'],len(events),events[0][0]if events else 0,events[0][1]if events else 0]
  for kind in['player','party']:
   pose=row[kind];values +=[modes[pose['mode']],pose['frame'],pose['behind'],pose['playing']]
  rows.append(' '.join(str(int(v))for v in values))
 (out/'calls.tsv').write_bytes(('\n'.join(rows)+'\n').encode());rows=[]
 doc,receipt=native['contacts']
 for row in doc['rows']:
  phase=['initial','inside','stay','disabled_exit'].index(row['phase']);test=receipt['cases'][row['case']];values=[row['case'],phase,int(test['enabled']),sounds[row['wanted']]]
  for kind in['player','party']:
   pose=row[kind];values +=[modes[pose['mode']],pose['behind'],pose['playing']]
  for body in row['bodies']:values +=[*body['position'],body['layer'],body['mask']]
  values +=[int(v)for overlap in row['overlaps']for v in overlap];values +=[len(row['events'])]
  for event in row['events']:values +=[event['area'],int(event['entered']),event['body']]
  rows.append(' '.join(str(int(v))if type(v)is bool else str(v)for v in values))
 (out/'contacts.tsv').write_bytes(('\n'.join(rows)+'\n').encode());player,_=contacts.player_source();(out/'player.json').write_bytes((json.dumps(player,indent=2)+'\n').encode());(out/'body.tsv').write_bytes((' '.join(str(v)for v in[*player['position'],len(player['points'])])+ '\n'+'\n'.join(' '.join(str(v)for v in p)for p in player['points'])+'\n').encode());(out/'source.json').write_bytes((json.dumps(dict(source=proof,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),native_calls=len(native['calls'][0]['rows']),native_contacts=len(doc['rows']),scope='Frozen actual observations,not new engine execution;no complete scene/renderer/NDSP pass'),indent=2)+'\n').encode());print('Original stepping fixture:',len(blob),'bytes;',len(native['calls'][0]['rows']),'calls;',len(doc['rows']),'contact states');return blob
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
