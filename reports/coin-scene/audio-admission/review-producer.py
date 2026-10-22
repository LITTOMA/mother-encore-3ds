from pathlib import Path
import copy,hashlib,json,sys
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
from tools import coin_scene as c,run_coin_scene_reference as native
sha=lambda raw:hashlib.sha256(raw).hexdigest()
out=root/'build/full-port/coin-audio-admission-2';out.mkdir(exist_ok=False)
prior=(root/'content/catacombs-coin-scene.json').read_bytes()
breakproof=json.loads((root/'build/full-port/breakable-audio-admission-2/source-review.json').read_bytes())
oldroot=root/'build/full-port/breakable-audio-admission-2/baseline'
breakpath=root/'content/catacombs-breakables.json';newbreak=breakpath.read_bytes()
assert sha(newbreak)==breakproof['new_recipe_sha256']
oldbreak=newbreak.replace(breakproof['new_audio_sha256'].encode(),breakproof['old_audio_sha256'].encode())
assert sha(oldbreak)==breakproof['old_recipe_sha256']
oldpath=out/'previous-breakable-recipe.json';oldpath.write_bytes(oldbreak)
old_b_root=c.b.ROOT;old_b_source=c.b.source
try:
 c.b.ROOT=oldroot
 c.b.source=lambda:old_b_source(oldpath)
 _,before,oldproof=c.source()
 oldfiles,oldcopies,oldexpected=native.source()
finally:
 c.b.ROOT=old_b_root;c.b.source=old_b_source
review_path=root/'compatibility/reviews/catacombs-coin-scene-v0410.json'
oldreview=review_path.read_bytes();review=json.loads(oldreview)
assert review['source']==oldproof
for run in review['results']:
 receipt=json.loads((root/run['path']/'receipt.json').read_bytes())
 assert all(receipt.get(k)==v for k,v in oldexpected.items())
p=json.loads(prior);assert p['breakable_recipe_sha256']==breakproof['old_recipe_sha256']
updated=prior.replace(breakproof['old_recipe_sha256'].encode(),breakproof['new_recipe_sha256'].encode())
expected=copy.deepcopy(p);expected['breakable_recipe_sha256']=breakproof['new_recipe_sha256']
assert json.loads(updated)==expected
path=root/'content/catacombs-coin-scene.json';path.write_bytes(updated)
try:
 _,after,newproof=c.source();newfiles,newcopies,newexpected=native.source()
 assert before==after and c.encode(before)==c.encode(after)
 proof=copy.deepcopy(oldproof);proof['breakables']['recipe_sha256']=breakproof['new_recipe_sha256'];proof['recipe_sha256']=sha(updated)
 assert proof==newproof
 assert oldfiles==newfiles and oldcopies==newcopies
except BaseException:
 path.write_bytes(prior);raise
(out/'previous-coin-recipe.json').write_bytes(prior)
(out/'previous-review.json').write_bytes(oldreview)
(out/'old-inputs.json').write_text(json.dumps(oldexpected,indent=2)+'\n',encoding='utf-8')
(out/'new-inputs.json').write_text(json.dumps(newexpected,indent=2)+'\n',encoding='utf-8')
record=dict(schema=1,upstream_commit=newproof['commit'],changed_recipe_fields=['breakable_recipe_sha256'],
 old_recipe_sha256=sha(prior),new_recipe_sha256=sha(updated),
 old_breakable_recipe_sha256=breakproof['old_recipe_sha256'],new_breakable_recipe_sha256=breakproof['new_recipe_sha256'],
 model_equal=True,resource_equal=True,resource_sha256=sha(c.encode(after)),factory_count=len(after['bindings']),
 native_generated_files_equal=True,native_copied_original_files_equal=True,
 scope='Re-extracted original 49 concrete factories. Only transitive reviewed audio recipe provenance changed; exact generated observer inputs and resource bytes unchanged. Fresh original runs required before updating review.',
 producer_sha256=sha(Path(__file__).read_bytes()))
(out/'source-review.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record))
