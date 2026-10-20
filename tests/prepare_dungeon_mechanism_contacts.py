"""Translate frozen native settled contacts without changing their outcomes."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switches as switches
REFERENCE=ROOT/'reports/dungeon-mechanism-contacts/native-2'

def reference(folder=REFERENCE):
 proof=json.loads((folder/'contact-receipt.json').read_bytes())
 if proof['commit']!=switches.PIN:raise ValueError('Contact reference source commit')
 for name,digest in proof['outputs'].items():
  if hashlib.sha256((folder/name).read_bytes()).hexdigest()!=digest:raise ValueError('Contact reference output changed: '+name)
 for name,digest in proof['sources'].items():
  if hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/name).read_bytes()).hexdigest()!=digest:raise ValueError('Contact source changed: '+name)
 doc=json.loads((folder/'contacts.json').read_bytes());config=json.loads((folder/'contact-input.json').read_bytes())
 if doc['engine']['hash']!='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8':raise ValueError('Contact reference engine')
 graph,source=switches.extract();ids={r['path']:r['id']for r in graph['switches']};order=list(ids);rows=iter(doc['rows']);out=[]
 for ci,case in enumerate(config['contact_cases']):
  for pi,phase in enumerate(case['phases']):
   r=next(rows)
   if (r['case'],r['phase'])!=(case['name'],phase['name'])or[s['path']for s in r['states']]!=order:raise ValueError('Contact reference order/coverage')
   mask=sum(int(s['on'])<<i for i,s in enumerate(r['states']))
   hits=sum(e['kind']=='switch_hit'for e in r['events']);denied=sum(e['kind']=='sfx'for e in r['denied'])
   out.append([ci,pi,int((pi==0 and case['wait_ready'])or phase['delay']>0),ids[case['target']],int(case['kind']=='bat'),int(phase['inside']and not phase['other']),int(phase['paused']),int(case['can_switch']),phase['body_layer'],phase['body_mask'],int(phase['enabled']),phase['layer'],phase['mask'],int(phase['monitorable']),mask,hits,denied])
 if next(rows,None)is not None or len(out)!=proof['observations']or len(config['contact_cases'])!=proof['cases']:raise ValueError('Contact reference trailing/missing rows')
 return out

def build(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True);rows=reference();(out/'contacts.tsv').write_text(''.join('\t'.join(map(str,row))+'\n'for row in rows),encoding='utf8');print('Native contact fixture:',len(rows),'unchanged settled outcomes')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
