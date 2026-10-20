"""Generate native input and immutable expectations from actual engine reports."""
import argparse,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_player_tree as tree
def prepare(out):
 out.mkdir(parents=True,exist_ok=True);model,proof=tree.source();blob=tree.encode(model);(out/'swing.encentry').write_bytes(blob)
 folder=ROOT/'reports/iggy-tutorial/swing-pause-reference-4';rows=json.loads((folder/'reference.json').read_bytes())['rows'];cases=json.loads((folder/'cases.json').read_bytes());ids={s['name']:s['id']for s in model['entry']['states']};lines=[]
 for r in rows:
  c=cases[r['case']];values=[r['case'],ids[c['state']],*c['direction'],int(c['incapacitated']),int(c['start_idle']),r['op'],ids[r['node']],r['time'],r['main_frame'],r['frame'],int(r['main']),int(r['special']),int(r['hit']),r['pivot'],int(r['behind']),int(r['state']==2),int(r['paused']),ids[r['paused_node']]if r['paused_node']else 0,r['starts'],int(r['selected']=='swing')];lines.append(' '.join(map(str,values)))
 (out/'pause.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Player tree fixture:',len(blob),'bytes;',len(rows),'unaltered original pause expectations')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);prepare(p.parse_args().out)
