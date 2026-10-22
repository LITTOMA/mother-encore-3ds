from pathlib import Path
import hashlib,json,shutil
root=Path(__file__).resolve().parents[2];base=root/'build/full-port'
sha=lambda raw:hashlib.sha256(raw).hexdigest()
phone=base/'item-admission-python-followups-2';phone_result=json.loads((phone/'completion.json').read_bytes());assert phone_result['returncode']==0 and phone_result['inputs_unchanged']
out=root/'reports/item-admission/ci-followups';out.mkdir(exist_ok=False)
for name,label in [('item-admission-d41-failures-2','failed-d41-full'),('item-admission-ci-followups-1','core-after'),('basement-legacy-adaptation-1','basement-after'),('item-admission-python-followups-1','python44-plus-gbk-failure'),('item-admission-python-followups-2','python-utf8-retry'),('item-admission-partition-after-3','partition-after'),('phone-item-audio-report-1','phone-report-review')]:
 src=base/name;dest=out/label;dest.mkdir()
 for p in src.rglob('*'):
  if p.is_file()and(p.suffix in ['.json','.log','.py','.cpp','.sh','.map']):
   target=dest/p.relative_to(src);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
for name in ['review-phone-item-audio-report.py','check-admission-ci-followups.py','check-basement-legacy-adaptation.py','check-admission-python-followups.py','check-admission-partition-release.py']:
 shutil.copyfile(base/name,out/name)
emu=root/'reports/item-admission/bb-emulator'
for label in ['before','after']:
 src=base/('item-admission-facing-policy-'+label);dest=emu/('policy-'+label);dest.mkdir();shutil.copytree(src,dest,dirs_exist_ok=True)
for name in ['check-promoted-opening-policy.py','admit-facing-input-strategy.py']:shutil.copyfile(base/name,emu/name)
sdk=root/'reports/item-admission/d41-sdk-comparison';sdk.mkdir()
for label,dirname in [('manual-full','item-admission-d41-full-official'),('automatic','item-admission-d41-official')]:
 src=base/dirname;dest=sdk/label;dest.mkdir();shutil.copyfile(src/'verified-artifact/build-receipt.json',dest/'build-receipt.json')
 if(src/'byte-equivalence.json').is_file():shutil.copyfile(src/'byte-equivalence.json',dest/'byte-equivalence.json')
for name in ['compare-item-admission-d41-full-driver.py','compare-item-admission-d41-full.py','inspect-admission-sdk-difference.py']:shutil.copyfile(base/name,sdk/name)
record=dict(schema=1,failed_source='d41c04698df543e40b5c19788faa5b0d9e2c684f',failed_full_run=39771295093,
 actual_failures={'gcc_ordinary':dict(selected=387,failed=9,not_run=3),'gcc_sanitizer':dict(selected=368,failed=3,not_run=3),'clang_ordinary':dict(selected=368,failed=3,not_run=3),'clang_sanitizer':dict(selected=368,failed=3,not_run=3)},
 reasons=['Audio consumer still expected60 records','Historical naming test relabeled current12 as8','Coin source recipe/review still pinned old breakable provenance','Dedicated test registration omitted new details module','Phone binding test expected60 records','Phone link report omitted item-eat provenance'],
 after=dict(gcc_normal_leak_sanitizer={'audio_data':31983,'coin_scene_data':11443,'coin_scene':276,'coin_scene_source':282774},coin_python=9,basement_python=10,phone_and_partition_units='44 passed unchanged-input cases plus actual UTF8 retry of1 failed Windows child-process case; both hash seeds run',method_accounting={'discovered':1667,'aggregate':1275,'dedicated':392,'shards':[319,319,319,318]}),
 original_coin_runs='reports/coin-scene/audio-append-original-1 and2:each3 modes x42 snapshots, equal to both historical runs',
 simulator='reports/item-admission/bb-emulator:actual two cold90-checkpoint runs with exact positions; four original menu cases only',
 sdk_scope='Manual full D41 artifact12422724217 matches actually tested BBgame. Automatic D41 artifact12424100129 differs in package bytes and is not claimed as the tested executable.',
 pending=['Fresh complete host matrix for follow-up head','New head SDK/game correspondence','Formal Catacombs ordinary input,party,production GPU/audio','Hardware'])
(out/'source.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print('Retained actual failed matrix, focused fixes, unchanged-expectation simulator evidence and explicit official package distinction.')
