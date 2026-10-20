from pathlib import Path
import hashlib, json, subprocess, sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import audio_asset, music_library

out = ROOT / 'build/full-port/root-music-dependency-review'
out.mkdir(exist_ok=True)
def sha(raw): return hashlib.sha256(raw).hexdigest()
def git_file(ref, name):
    return subprocess.check_output(['git', 'show', ref + ':' + name], cwd=ROOT)

old = json.loads((ROOT/'compatibility/reviews/music-library-v0410.json').read_bytes())
_, metadata, records, bank, new = music_library.source()
changes = {k: dict(old=old.get(k), new=new.get(k)) for k in old.keys() | new.keys() if old.get(k) != new.get(k)}
assert set(changes) == {'native_audio_sha256', 'source_bank_sha256'}, changes
assert new['metadata'] == old['metadata']
current_recipe = json.loads((ROOT/'content/native-audio.json').read_bytes())
history = []
for ref in ['8956037^', '00cb71d', '00cb71d^', 'd33d0b9', '8fc8466']:
    recipe_raw = git_file(ref, 'content/native-audio.json')
    bank_raw = git_file(ref, 'romfs/sound/banks/opening.encaudio')
    historical_bank = audio_asset.parse_bank(bank_raw)
    historical_recipe = json.loads(recipe_raw)
    row = dict(ref=ref, native_audio_sha256=sha(recipe_raw), source_bank_sha256=sha(bank_raw),
               assets=len(historical_bank['assets']), recipe_assets=len(historical_recipe['assets']))
    if row['source_bank_sha256'] == old['source_bank_sha256']:
        assert row['native_audio_sha256'] == old['native_audio_sha256']
        assert {k:v for k,v in bank.items() if k != 'assets'} == {k:v for k,v in historical_bank.items() if k != 'assets'}
        assert {k:v for k,v in current_recipe.items() if k != 'assets'} == {k:v for k,v in historical_recipe.items() if k != 'assets'}
        assert bank['assets'][:len(historical_bank['assets'])] == historical_bank['assets']
        assert current_recipe['assets'][:len(historical_recipe['assets'])] == historical_recipe['assets']
        row.update(all_prior_bank_records_unchanged=True, all_prior_recipe_records_unchanged=True,
                   appended_bank_records=bank['assets'][len(historical_bank['assets']):],
                   appended_recipe_records=current_recipe['assets'][len(historical_recipe['assets']):])
    history.append(row)
assert any(r.get('all_prior_bank_records_unchanged') for r in history), 'No exact reviewed source predecessor found'
fixtures = {}
data = music_library.encode(metadata)
assert data == (ROOT/'tests/fixtures/global-music.encmusic').read_bytes()
fixtures['global-music.encmusic'] = dict(bytes=len(data), sha256=sha(data))
for track, record in zip(metadata['tracks'], records):
    stem = 'sound/banks/global/' + str(track['id'])
    for name, data in [(stem+'.encmusic', music_library.encode({**metadata,'tracks':[track]})),
                       (stem+'.encaudio', audio_asset.build_bank([record],bank['master_db'],bank['silence_db']))]:
        assert data == (ROOT/'tests/fixtures/global-library'/name).read_bytes(), name
        fixtures[name] = dict(bytes=len(data), sha256=sha(data))
assert len(records) == 13 and len(fixtures) == 27
report = dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              changed_proof_fields=changes, metadata_equal=True, source_records_equal=True,
              history=history, frozen_metadata_files=fixtures, no_release_or_pcm_changes=True)
(out/'semantic-review.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(changed_fields=list(changes), tracks=len(records), frozen_files=len(fixtures),
                     history=[{k:v for k,v in r.items() if not k.startswith('appended_')} for r in history]),indent=2))
