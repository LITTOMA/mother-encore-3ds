from pathlib import Path
import sys,json,hashlib
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import mick_treats as m,field_ready_rng as helper
from tools.extract_battle_entry import Extractor
from tools.godot_text import Document,instantiate
ex=Extractor(r);docs={}
def load(path):
 if path not in docs:docs[path]=Document(path,ex.text(path))
 return docs[path]
order=instantiate(load,m.SCENE);ops,result=helper.replay(ex,order,m.READY_SCRIPTS);baseline=json.loads((r/'build/full-port/ready-rng-before.json').read_bytes());before=baseline['ir']['actor'];assert result['state']==(before['rng_hi']<<32)|before['rng_lo']
owned=result['samples']['Objects/Items/Item/Sparkles'];assert owned['seeded'],'Owned random ready sample depends on unrepresented incoming state'
out=r/'build/full-port/ready-rng-helper-1';out.mkdir(exist_ok=False);(out/'input.json').write_text(json.dumps(dict(ops=ops,sources=ex.sources),indent=2)+'\n',encoding='utf8');(out/'prediction.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print('Exact existing ready poststate',result['state'],'ops',len(ops),'raw',result['raw_draws'],'sparkle samples',len(result['samples']),'owned',owned)
