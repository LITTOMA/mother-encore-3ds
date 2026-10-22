from pathlib import Path
import hashlib,json,shutil,sys
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
from tools import coin_scene as c,run_coin_scene_reference as original
from tests import prepare_coin_scene as fixture
sha=lambda raw:hashlib.sha256(raw).hexdigest()
proofdir=root/'build/full-port/coin-audio-admission-2'
proof=json.loads((proofdir/'source-review.json').read_bytes())
path=fixture.REVIEW;prior=path.read_bytes();review=json.loads(prior)
assert prior==(proofdir/'previous-review.json').read_bytes()
_,model,source=c.source();files,copied,expected=original.source()
assert source['recipe_sha256']==proof['new_recipe_sha256'] and sha(c.encode(model))==proof['resource_sha256']
baseline=[(root/row['path']/'reference.json').read_bytes()for row in review['results']]
assert baseline[0]==baseline[1]
records=[]
for n in (1,2):
 src=root/('build/full-port/coin-audio-original-'+str(n));raw=(src/'reference.json').read_bytes();receipt=json.loads((src/'receipt.json').read_bytes())
 assert raw==baseline[0] and receipt['result_sha256']==sha(raw)
 assert receipt['engine_binary_sha256']=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd' and receipt['return_code']==0
 assert all(receipt.get(k)==v for k,v in expected.items())
 fixture.validate(json.loads(raw),json.loads(files['input.json']))
 dest=root/('reports/coin-scene/audio-append-original-'+str(n));dest.mkdir(exist_ok=False)
 for name in ['reference.json','receipt.json','inputs.json','producer.py','editor.log','godot.log']:shutil.copyfile(src/name,dest/name)
 records.append(dict(path=dest.relative_to(root).as_posix(),sha256=sha(raw)))
review['source']=source;review['results']=records
assert review['native_producer_sha256']==sha(Path(original.__file__).read_bytes())
assert review['fixture_protocol_sha256']==sha(Path(fixture.__file__).read_bytes())
dest=root/'reports/coin-scene/audio-admission';dest.mkdir(exist_ok=False)
for name in ['previous-review.json','previous-coin-recipe.json','previous-breakable-recipe.json','source-review.json','old-inputs.json','new-inputs.json']:shutil.copyfile(proofdir/name,dest/name)
shutil.copyfile(Path(__file__),dest/'freeze-producer.py')
shutil.copyfile(root/'build/full-port/review-coin-audio-admission.py',dest/'review-producer.py')
shutil.copyfile(root/'build/full-port/coin-audio-original-runs-1/steps.json',dest/'actual-runs.json')
path.write_text(json.dumps(review,indent=2)+'\n',encoding='utf-8')
fixture.build(root/'build/full-port/coin-audio-fixture-1')
print('Re-reviewed49 factories; two actual runs of3 modes, each42 snapshots, exactly equal to both retained original runs. Unchanged native inputs and resource; no whole-room claim.')
