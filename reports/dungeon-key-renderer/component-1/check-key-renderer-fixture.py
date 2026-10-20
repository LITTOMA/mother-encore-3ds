import hashlib,json,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests.prepare_dungeon_key_renderer import build
from tools import dungeon_key_media as media
out=r/'build/full-port/key-renderer-fixture-1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={p.relative_to(out).as_posix():sha(p)for p in out.rglob('*')if p.is_file()}
build(out);after={p.relative_to(out).as_posix():sha(p)for p in out.rglob('*')if p.is_file()};assert before==after
rejected=[]
for path in[r/'build/full-port',r/'docs/renderer-fixture-must-not-exist',r.parent/'renderer-fixture-must-not-exist']:
 try:build(path)
 except ValueError:rejected.append(str(path))
 else:raise AssertionError('Fixture accepted foreign destination')
original=media.parse((out/'original/source.enckmp').read_bytes());synthetic=media.parse((out/'source.enckmp').read_bytes())
for a,b in zip(original['profiles'],synthetic['profiles']):
 for key in['texture_bytes','texture_crc','texture_sha256']:a.pop(key);b.pop(key)
assert original==synthetic
proof=dict(unchanged_repeated_fixture=True,synthetic_only_byte_identities=True,rejected_foreign_destinations=rejected,files=after)
(r/'build/full-port/key-renderer-fixture-check-1.json').write_text(json.dumps(proof,indent=2)+'\n');print('Renderer fixture:idempotent owned rebuild;3 foreign roots rejected;only declared texture length/CRC/SHA differ,no source clock/geometry changes')
