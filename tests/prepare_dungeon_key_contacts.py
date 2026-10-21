"""Project retained native contacts without rewriting their expected results."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_media as d
from tools.run_dungeon_key_contact_reference import validate_observed
SOURCE=ROOT/'reports/dungeon-key-contacts/native-3'

def build(out,reference=SOURCE):
 out=Path(out).resolve();d.require(out.is_relative_to(ROOT/'build'),'Key contact fixture stays under build');p=Path(reference);observed=json.loads((p/'contacts.json').read_text('utf8'));config=json.loads((p/'input.json').read_text('utf8'));receipt=json.loads((p/'receipt.json').read_text('utf8'));d.require(receipt['contacts_sha256']==hashlib.sha256((p/'contacts.json').read_bytes()).hexdigest()and receipt['input_sha256']==hashlib.sha256((p/'input.json').read_bytes()).hexdigest(),'Key contact native provenance');validate_observed(observed,config);model,_,proof=d.source_model();d.require(receipt['clock_sha256']==proof['clock_sha256']and receipt['review_sha256']==proof['review_sha256'],'Key contact source binding changed');owners={o['owner']for o in model['owners']};d.require(len(config['keys'])==3 and all(k['owner']in owners for k in config['keys']),'Key contact key ownership');rows=[]
 for row in observed['rows']:
  ci,pi=row['case'],row['phase'];case=config['cases'][ci];phase=case['phases'][pi];target=config['keys'][case['target']]['owner'];values=[ci,pi,target,int(case['initial_inside']),int(case['initial_paused']),int(case['saved']),config['player']['layer'],config['player']['mask'],int(phase['inside']and not phase['other']),int(phase['paused']),int(phase['enabled']),phase['layer'],phase['mask'],int(phase['finish']),row['count'],row['notices'],len(row['ui']),sum(w['playing']for w in row['audio_writes'])]
  for key,state,flag in zip(config['keys'],row['states'],row['flags']):values.extend([key['owner'],int(state['alive']),int(state.get('queued',False)),int(state.get('visible',False)),int(state.get('audio',False)),int(state.get('disabled',False)),int(flag)])
  rows.append(' '.join(map(str,values)))
 d.require(len(rows)==receipt['rows']==27 and receipt['cases']==len(config['cases'])==14,'Key contact source coverage');out.mkdir(parents=True,exist_ok=True);(out/'reference.tsv').write_text('\n'.join(rows)+'\n',encoding='utf8');print('Key contacts fixture:14 native cases,27 settled observations; sampled current body,explicit pause/audio services')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
