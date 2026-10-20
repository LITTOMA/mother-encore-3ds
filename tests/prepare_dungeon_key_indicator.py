"""Build independent HUD state resource and the two actual native step oracles."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_indicator as d
from tools.run_dungeon_key_indicator_reference import cases
REFERENCE=ROOT/'reports/dungeon-key-indicator'

def build(out,reference=REFERENCE):
 out=Path(out).resolve();d.require(out.is_relative_to(ROOT/'build'),'Key HUD fixture stays under build');out.mkdir(parents=True,exist_ok=True)
 reference=Path(reference);a=json.loads((reference/'native-14/indicator.json').read_text('utf8'));b=json.loads((reference/'native-15/indicator.json').read_text('utf8'));d.require(a==b,'Key HUD native repetitions differ')
 model,proof=d.source_model();config=json.loads((reference/'native-14/input.json').read_text('utf8'));d.require(config==json.loads((reference/'native-15/input.json').read_text('utf8')),'Key HUD native input repetitions differ')
 for k,v in model.items():
  if k!='keys':d.require(config[k]==v,'Key HUD native input/source differs:'+k)
 d.require(config['cases']==cases()and a['append_attempts']==[dict(case=1,phase=4),dict(case=5,phase=4),dict(case=6,phase=4)],'Key HUD reference cases/diagnostics differ')
 blob=d.encode(model);(out/'source.enckui').write_bytes(blob);rows=[];counts={};attempts={(v['case'],v['phase'])for v in a['append_attempts']}
 for row in a['rows']:
  ci,pi=row['case'],row['phase'];op=dict(op='initial')if pi<0 else config['cases'][ci][pi]
  if pi<0:counts[ci]=0
  if op['op']=='update':counts[ci]=op['count']
  tag={'initial':'I','update':'U','close':'C','step':'T'}[op['op']]
  fields=[ci,pi,tag,op.get('delta',0),counts[ci],row['x'],row['y'],int(row['visible']),int(row['open']),int(row['running']),int((ci,pi)in attempts)]
  rows.append(' '.join(map(str,fields))+' '+json.dumps(row['text']))
 (out/'reference.tsv').write_text('\n'.join(rows)+'\n',encoding='utf8');proof.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),cases=len(config['cases']),rows=len(rows));(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Key HUD fixture:',len(blob),'bytes;',len(config['cases']),'cases;',len(rows),'native observations;3 typed original diagnostic pairs')
 return blob
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--reference',type=Path,default=REFERENCE);a=p.parse_args();build(a.out,a.reference)
