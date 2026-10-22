from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]/'build/full-port'
before=json.loads((root/'item-admission-bb-official/verified-artifact/build-receipt.json').read_bytes());after=json.loads((root/'item-admission-d41-official/verified-artifact/build-receipt.json').read_bytes())
for key in sorted(set(before)|set(after)):
    if before.get(key)!=after.get(key):print(key,json.dumps(dict(bb=before.get(key),d41=after.get(key)),ensure_ascii=False)[:5500])
