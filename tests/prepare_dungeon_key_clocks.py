"""Build key clock data and project retained native observations for C++."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_clocks as d
SOURCE=ROOT/'reports/dungeon-key-clocks/native-6'
OPS={'noop':0,'play':1,'advance':2,'flush':3,'interact':4,'pickup':5,'finish_audio':6}
def build(out,reference=SOURCE):
 out=Path(out).resolve();reference=Path(reference).resolve();d.require(out.is_relative_to(ROOT/'build'),'Key clock fixture stays under build')
 data,blob,proof=d.extract();observed=json.loads((reference/'observations.json').read_text('utf8'));config=json.loads((reference/'input.json').read_text('utf8'));receipt=json.loads((reference/'receipt.json').read_text('utf8'))
 d.require(receipt['resource_sha256']==hashlib.sha256(blob).hexdigest()and receipt['commit']==proof['commit']and observed['engine']['string']=='3.6.2-stable (official)','Key clock frozen source/resource provenance')
 names={(p['id'],c['name']):c['id']for p in config['profiles']for c in p['clips']};by={p['id']:p for p in data['profiles']};rows=[]
 def joined(values):return ','.join(map(str,values))or'-'
 def numeric_pose(p,profile):
  return [int(p.get('alive',False)),int(p.get('queued',False)),p.get('frame',0),int(p.get('visible',False)),int(p.get('root_visible',False)),*p.get('position',['0','0']),int(p.get('playing',False)),names.get((profile,p.get('assigned','')),0),p.get('time','0'),int(p.get('audio',False)),int(p.get('disabled',False)),int(p.get('prompt',False)),int(p.get('opened',False))]
 index=0
 for ci,case in enumerate(config['cases']):
  profile=by[case['profile']];node=next((n for n in data['metadata']['nodes']if n['kind']==profile['kind']),None)if profile['kind']!=3 else None
  for ai,action in enumerate(case['actions']):
   row=observed['rows'][index];d.require(row['case']==ci and row['index']==ai and row['op']==action['op'],'Key clock native action order');index+=1;p=row['pose'];child=p.get('child',{})
   completed=[str(c['profile'])+':'+str(names[c['profile'],c['name']])for c in row['completed']]
   diagnostics=[sum(bool(by[c['profile']]['diagnostics']&bit)for c in row['completed'])for bit in[1,2]]
   role=next((c['role']for c in data['clips']if c['id']==action.get('clip')),0)
   values=[ci,ai,OPS[action['op']],case['profile'],node['id']if node else 0,int(case.get('saved',False)),role,repr(action.get('delta',0.)),row['counter'],int(row['flags'].get(node['flag'],False))if node else 0]
   values+=numeric_pose(p,case['profile']);values+=numeric_pose(child,profile['child'])
   values+=[joined(int(v)for v in p.get('audio_writes',[])),joined(int(v)for v in child.get('audio_writes',[])),joined(completed),*diagnostics]
   rows.append(' '.join(map(str,values)))
 d.require(index==len(observed['rows'])==receipt['rows'],'Key clock native row ownership');out.mkdir(parents=True,exist_ok=True);(out/'source.encklc').write_bytes(blob);(out/'reference.tsv').write_text('\n'.join(rows)+'\n',encoding='utf8');print('Key clock fixture:',len(blob),'bytes,',len(rows),'native observations; explicit source services')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--reference',type=Path,default=SOURCE);a=p.parse_args();build(a.out,a.reference)
