from pathlib import Path
import hashlib, json, sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import music_library, music_library_bindings
out = ROOT/'build/full-port/root-music-dependency-review'
audit = json.loads((out/'semantic-review.json').read_bytes())
assert audit['metadata_equal'] and len(audit['frozen_metadata_files']) == 27
assert set(audit['changed_proof_fields']) == {'native_audio_sha256','source_bank_sha256'}
def sha(raw): return hashlib.sha256(raw).hexdigest()
def replace(name, proof, expected):
    path = ROOT/'compatibility/reviews'/name
    before = path.read_bytes()
    old = json.loads(before)
    changes = {k:dict(old=old.get(k),new=proof.get(k)) for k in old.keys() | proof.keys() if old.get(k) != proof.get(k)}
    assert set(changes) == expected, changes
    (out/(name+'.before')).write_bytes(before)
    after = (json.dumps(proof,indent=2)+'\n').encode('utf-8')
    path.write_bytes(after)
    return dict(before_sha256=sha(before),after_sha256=sha(after),changes=changes)

_, metadata, records, bank, proof = music_library.source()
changes = {}
changes['music-library-v0410.json'] = replace('music-library-v0410.json',proof,{'native_audio_sha256','source_bank_sha256'})
_, doc, files, proof = music_library_bindings.source()
assert len(files) == 28
for name,data in files.items():
    assert data == (ROOT/'tests/fixtures/global-library'/name).read_bytes(), name
changes['music-library-bindings-v0410.json'] = replace('music-library-bindings-v0410.json',proof,{'source_library_review_sha256'})
report = dict(changes=changes,unchanged_frozen_bundle={name:dict(bytes=len(data),sha256=sha(data)) for name,data in files.items()},
              bundle_count=len(files),generated_using=['tools.music_library.source','tools.music_library_bindings.source'],
              release_staged=False,source_code_unchanged=True,fixtures_unchanged=True)
(out/'regeneration.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Regenerated two review records through their source producers; all 28 frozen files identical; no PCM or release changes.')
