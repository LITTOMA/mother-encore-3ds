from pathlib import Path
import json,sys,hashlib
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import mick_treats as m
ir=m.build(r);blob=m.encode(ir);out=r/'build/full-port/ready-rng-before.json';out.write_text(json.dumps(dict(ir=ir,pack_sha256=hashlib.sha256(blob).hexdigest(),producer_sha256=hashlib.sha256((r/'tools/mick_treats.py').read_bytes()).hexdigest()),indent=2,ensure_ascii=False)+'\n',encoding='utf8');print('Captured old ready poststate',(ir['actor']['rng_hi']<<32)|ir['actor']['rng_lo'],'events',len(ir['rng']),'pack',hashlib.sha256(blob).hexdigest())
