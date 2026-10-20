from pathlib import Path
import json,hashlib,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import mick_treats as m
before=json.loads((r/'build/full-port/ready-rng-before.json').read_bytes());after=m.build(r);canonical=lambda x:json.dumps(x,sort_keys=True,separators=(',',':'))
assert canonical(after)==canonical(before['ir']),'Mick IR gameplay/source data changed';pack=m.encode(after);assert hashlib.sha256(pack).hexdigest()==before['pack_sha256'],'Mick binary changed';print('Mick IR EXACT unchanged; binary EXACT',before['pack_sha256'],'viewport rows',len(after['rng']),'poststate',(after['actor']['rng_hi']<<32)|after['actor']['rng_lo'])
(r/'build/full-port/ready-rng-factor-proof.json').write_text(json.dumps(dict(before_producer_sha256=before['producer_sha256'],after_producer_sha256=hashlib.sha256((r/'tools/mick_treats.py').read_bytes()).hexdigest(),binary_sha256=before['pack_sha256'],ir_exact=True,viewport_rows=len(after['rng']),state=(after['actor']['rng_hi']<<32)|after['actor']['rng_lo']),indent=2)+'\n',encoding='utf8')
