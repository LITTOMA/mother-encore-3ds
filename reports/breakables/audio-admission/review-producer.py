from pathlib import Path
import copy,hashlib,json,subprocess,sys
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
from tools import catacombs_breakables as b,catacombs_item_catalog as cat,item_consumption as consumption
base='0b63d7c0f46d7f620415de718537e5864ef36540'
out=root/'build/full-port/breakable-audio-admission-2';out.mkdir(exist_ok=False)
sha=lambda raw:hashlib.sha256(raw).hexdigest()
def old(path):return subprocess.check_output(['git','show',base+':'+path],cwd=root)
audio_name='content/native-audio.json';recipe_path=root/'content/catacombs-breakables.json'
old_audio=old(audio_name);new_audio=(root/audio_name).read_bytes()
before=json.loads(old_audio);after=json.loads(new_audio)
assert len(before['assets'])==60 and len(after['assets'])==61
assert all(after[k]==v for k,v in before.items() if k!='assets')
assert after['assets'][:60]==before['assets']
assert after['assets'][60]['stable_id']==1801 and after['assets'][60]['pcm_path']=='sound/effects/item-eat.pcm'
evidence=json.loads((root/'build/full-port/item-eat-audio-publication-2/source.json').read_bytes())
assert evidence['old_records_unchanged'] is True and evidence['staged'] is True
assert all(sha((root/'romfs'/path).read_bytes())==digest for path,digest in evidence['unchanged_pcm'].items())
prior=recipe_path.read_bytes();assert prior==old('content/catacombs-breakables.json')
p=json.loads(prior);assert p['audio_recipe_sha256']==sha(old_audio)
oldroot=out/'baseline';(oldroot/'content').mkdir(parents=True)
(oldroot/audio_name).write_bytes(old_audio)
originalroot=b.ROOT
try:
    b.ROOT=oldroot
    _,oldmodel,oldproof=b.source()
finally:b.ROOT=originalroot
oldblob=b.encode(oldmodel)
updated=prior.replace(p['audio_recipe_sha256'].encode(),sha(new_audio).encode())
assert updated!=prior
expected=copy.deepcopy(p);expected['audio_recipe_sha256']=sha(new_audio)
assert json.loads(updated)==expected
recipe_path.write_bytes(updated)
try:
    _,newmodel,newproof=b.source()
    assert newmodel==oldmodel and b.encode(newmodel)==oldblob
    expectedproof=copy.deepcopy(oldproof);expectedproof['recipe_sha256']=sha(updated)
    assert expectedproof==newproof
    # Re-admission must not silently alter downstream content or review records.
    cat.load();consumption.load()
except BaseException:
    recipe_path.write_bytes(prior)
    raise
record=dict(schema=1,baseline_commit=base,old_audio_sha256=sha(old_audio),new_audio_sha256=sha(new_audio),
 old_asset_count=60,new_asset_count=61,retained_assets_equal=True,retained_pcm_equal=True,
 old_recipe_sha256=sha(prior),new_recipe_sha256=sha(updated),changed_recipe_fields=['audio_recipe_sha256'],
 breakable_owners=len(newmodel['owners']),breakable_families=len(newmodel['families']),
 shared_sound=newmodel['sound'],breakable_model_equal=True,breakable_resource_equal=True,
 breakable_resource_sha256=sha(oldblob),downstream_catalog_and_consumption_reviews_current=True,
 original_source_digests=newproof['sources'],mapped_methods=newproof['symbols'],
 verification_scope='Actual pinned source re-extraction and exact model/resource comparison. No new scene, GPU, sound audibility, emulator or hardware result.',
 producer_sha256=sha(Path(__file__).read_bytes()))
(out/'source-review.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in record.items() if k not in ['original_source_digests','mapped_methods']}),flush=True)
