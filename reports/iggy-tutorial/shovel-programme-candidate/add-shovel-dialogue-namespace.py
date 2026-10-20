from pathlib import Path
import json
r=Path(__file__).resolve().parents[2];p=r/'content/graveyard-shovel.json';d=json.loads(p.read_bytes());assert'dialogue_first_id'not in d;d['dialogue_first_id']=3200;p.write_bytes((json.dumps(d,indent=2,ensure_ascii=False)+'\n').encode('utf8'));print('Native binding namespace3200..3204 recorded in independent source review')
